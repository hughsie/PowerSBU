/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "config.h"

#include <gio/gio.h>
#include <glib-unix.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <stdlib.h>

#include "sbu-common.h"
#include "sbu-device.h"
#include "sbu-manager.h"

typedef struct {
	GCancellable *cancellable;
	GMainLoop *loop;
	guint name_owner_id;
	guint timed_exit_id;
	SbuManager *manager;
	GDBusConnection *connection;
	GDBusNodeInfo *introspection;
} SbuMain;

static gboolean
sbu_main_sigint_cb(gpointer user_data)
{
	SbuMain *self = (SbuMain *)user_data;
	g_debug("handling SIGINT");
	g_cancellable_cancel(self->cancellable);
	g_main_loop_quit(self->loop);
	return FALSE;
}

static GVariant *
sbu_main_device_array_to_variant(GPtrArray *devices)
{
	GVariantBuilder builder;

	g_variant_builder_init(&builder, G_VARIANT_TYPE("aa{sv}"));
	for (guint i = 0; i < devices->len; i++) {
		SbuDevice *device = g_ptr_array_index(devices, i);
		GVariant *tmp = sbu_device_to_variant(SBU_DEVICE(device));
		g_variant_builder_add_value(&builder, tmp);
	}
	return g_variant_new("(aa{sv})", &builder);
}

static void
sbu_main_manager_changed_cb(SbuManager *manager, SbuMain *self)
{
	/* not yet connected */
	if (self->connection == NULL)
		return;
	g_dbus_connection_emit_signal(self->connection,
				      NULL,
				      SBU_DBUS_PATH,
				      SBU_DBUS_INTERFACE,
				      "Changed",
				      NULL,
				      NULL);
}

static GDBusNodeInfo *
sbu_main_load_introspection(const gchar *filename, GError **error)
{
	const gchar *xml =
	    "<node name='/' xmlns:doc='http://www.freedesktop.org/dbus/1.0/doc.dtd'>\n"
	    "  <interface name='com.hughski.PowerSBU'>\n"
	    "    <property name='Version' type='s' access='read'/>\n"
	    "    <method name='GetDevices'>\n"
	    "      <arg name='devices' type='aa{sv}' direction='out' />\n"
	    "    </method>\n"
	    "    <method name='GetHistory'>\n"
	    "      <arg name='device_id' direction='in' type='s'/>\n"
	    "      <arg name='key' direction='in' type='s'/>\n"
	    "      <arg name='start' direction='in' type='t'/>\n"
	    "      <arg name='end' direction='in' type='t'/>\n"
	    "      <arg name='limit' direction='in' type='u'/>\n"
	    "      <arg name='data' direction='out' type='a(td)'/>\n"
	    "    </method>\n"
	    "    <signal name='Changed' />\n"
	    "  </interface>\n"
	    "</node>\n";
	/* build introspection from XML */
	return g_dbus_node_info_new_for_xml(xml, error);
}

static void
sbu_main_daemon_method_call(GDBusConnection *connection,
			    const gchar *sender,
			    const gchar *object_path,
			    const gchar *interface_name,
			    const gchar *method_name,
			    GVariant *parameters,
			    GDBusMethodInvocation *invocation,
			    gpointer user_data)
{
	SbuMain *self = (SbuMain *)user_data;
	GVariant *val = NULL;
	g_autoptr(GError) error = NULL;

	g_debug("Called %s()", method_name);
	if (g_strcmp0(method_name, "GetDevices") == 0) {
		val = sbu_main_device_array_to_variant(sbu_manager_get_devices(self->manager));
		g_dbus_method_invocation_return_value(invocation, val);
		return;
	}
	if (g_strcmp0(method_name, "GetHistory") == 0) {
		const gchar *device_id = NULL;
		const gchar *key = NULL;
		guint64 start = 0;
		guint64 end = 0;
		guint limit = 0;
		g_autoptr(SbuDevice) device = NULL;

		g_variant_get(parameters, "(&s&sttu)", &device_id, &key, &start, &end, &limit);
		device = sbu_manager_get_device_by_id(self->manager, device_id, &error);
		if (device == NULL) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		val =
		    sbu_manager_get_history(self->manager, device, key, start, end, limit, &error);
		if (val == NULL) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, val);
		return;
	}
	g_set_error(&error,
		    G_DBUS_ERROR,
		    G_DBUS_ERROR_UNKNOWN_METHOD,
		    "no such method %s",
		    method_name);
	g_dbus_method_invocation_return_gerror(invocation, error);
}

static GVariant *
sbu_main_daemon_get_property(GDBusConnection *connection_,
			     const gchar *sender,
			     const gchar *object_path,
			     const gchar *interface_name,
			     const gchar *property_name,
			     GError **error,
			     gpointer user_data)
{
	if (g_strcmp0(property_name, "Version") == 0)
		return g_variant_new_string("1.0.0");

	/* return an error */
	g_set_error(error,
		    G_DBUS_ERROR,
		    G_DBUS_ERROR_UNKNOWN_PROPERTY,
		    "failed to get daemon property %s",
		    property_name);
	return NULL;
}

static void
sbu_main_bus_acquired_cb(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	SbuMain *self = (SbuMain *)user_data;
	guint registration_id;
	static const GDBusInterfaceVTable interface_vtable = {sbu_main_daemon_method_call,
							      sbu_main_daemon_get_property,
							      NULL};

	self->connection = g_object_ref(connection);
	registration_id = g_dbus_connection_register_object(self->connection,
							    SBU_DBUS_PATH,
							    self->introspection->interfaces[0],
							    &interface_vtable,
							    self,  /* user_data */
							    NULL,  /* user_data_free_func */
							    NULL); /* GError** */
	g_assert(registration_id > 0);
}

static void
sbu_main_name_lost_cb(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	SbuMain *self = (SbuMain *)user_data;
	g_debug("lost (or failed to acquire) the name %s on the bus", name);
	g_main_loop_quit(self->loop);
}

static void
sbu_main_name_acquired_cb(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	//	SbuMain *self = (SbuMain *)user_data;
	g_debug("acquired the name %s on the bus", name);
}

static void
sbu_main_self_free(SbuMain *self)
{
	if (self->name_owner_id != 0)
		g_bus_unown_name(self->name_owner_id);
	if (self->connection != NULL)
		g_object_unref(self->connection);
	if (self->introspection != NULL)
		g_dbus_node_info_unref(self->introspection);
	g_main_loop_unref(self->loop);
	g_object_unref(self->cancellable);
	g_object_unref(self->manager);
	g_free(self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuMain, sbu_main_self_free)

static SbuMain *
sbu_main_self_new(void)
{
	SbuMain *self = g_new0(SbuMain, 1);
	self->loop = g_main_loop_new(NULL, FALSE);
	self->cancellable = g_cancellable_new();
	self->manager = sbu_manager_new();
	return self;
}

static gboolean
sbu_main_timed_exit_cb(gpointer user_data)
{
	SbuMain *self = (SbuMain *)user_data;
	g_main_loop_quit(self->loop);
	return FALSE;
}

int
main(int argc, char *argv[])
{
	g_autoptr(SbuMain) self = sbu_main_self_new();
	gboolean timed_exit = FALSE;
	gboolean verbose = FALSE;
	g_autoptr(GError) error = NULL;
	g_autoptr(GOptionContext) context = g_option_context_new(NULL);

	const GOptionEntry options[] = {{"verbose",
					 'v',
					 0,
					 G_OPTION_ARG_NONE,
					 &verbose,
					 /* TRANSLATORS: command line option */
					 _("Show extra debugging information"),
					 NULL},
					{"timed-exit",
					 '\0',
					 0,
					 G_OPTION_ARG_NONE,
					 &timed_exit,
					 /* TRANSLATORS: command line option */
					 _("Exit after a small delay"),
					 NULL},
					{NULL}};

	setlocale(LC_ALL, "");

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	/* avoid GVFS */
	if (!g_setenv("GIO_USE_VFS", "local", TRUE)) {
		g_printerr("Error setting GIO_USE_GVFS\n");
		return EXIT_FAILURE;
	}

	/* do stuff on ctrl+c */
	g_unix_signal_add_full(G_PRIORITY_DEFAULT, SIGINT, sbu_main_sigint_cb, self, NULL);

	/* TRANSLATORS: program name */
	g_set_application_name(_("SBU Daemon"));
	g_option_context_add_main_entries(context, options, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		/* TRANSLATORS: the user didn't read the man page */
		g_printerr("%s: %s\n", _("Failed to parse arguments"), error->message);
		return EXIT_FAILURE;
	}

	/* set verbose? */
	if (verbose)
		g_setenv("G_MESSAGES_DEBUG", "all", TRUE);

	/* perform failable manager setup */
	if (!sbu_manager_setup(self->manager, &error)) {
		g_printerr("%s: %s\n", _("Failed to start manager"), error->message);
		return EXIT_FAILURE;
	}
	g_signal_connect(SBU_MANAGER(self->manager),
			 "changed",
			 G_CALLBACK(sbu_main_manager_changed_cb),
			 self);

	/* valgrinding */
	if (timed_exit)
		self->timed_exit_id = g_timeout_add_seconds(15, sbu_main_timed_exit_cb, self);

	/* load introspection from file */
	self->introspection = sbu_main_load_introspection(SBU_DBUS_INTERFACE ".xml", &error);
	if (self->introspection == NULL) {
		g_printerr("%s: %s\n", _("Failed to load introspection"), error->message);
		return EXIT_FAILURE;
	}

	/* try to own name */
	self->name_owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
					     SBU_DBUS_NAME,
					     G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
						 G_BUS_NAME_OWNER_FLAGS_REPLACE,
					     sbu_main_bus_acquired_cb,
					     sbu_main_name_acquired_cb,
					     sbu_main_name_lost_cb,
					     self,
					     NULL);
	g_main_loop_run(self->loop);

	/* success */
	return EXIT_SUCCESS;
}
