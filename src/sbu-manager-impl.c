/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2017 Richard Hughes <richard@hughsie.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <glib.h>
#include <string.h>

#include "sbu-common.h"
#include "sbu-config.h"
#include "sbu-database.h"
#include "sbu-device-impl.h"
#include "sbu-manager-impl.h"
#include "sbu-plugin-private.h"

typedef struct _SbuManagerImplClass	SbuManagerImplClass;

struct _SbuManagerImpl
{
	SbuManagerSkeleton		 parent_instance;
	GDBusObjectManagerServer	*object_manager; /* no ref */
	guint				 poll_id;
	guint				 poll_interval;
	GPtrArray			*plugins;
	GPtrArray			*devices;
	SbuDatabase			*database;
};

typedef void		 (*SbuPluginFunc)		(SbuPlugin	*plugin);
typedef gboolean	 (*SbuPluginSetupFunc)		(SbuPlugin	*plugin,
							 GCancellable	*cancellable,
							 GError		**error);

struct _SbuManagerImplClass
{
	SbuManagerSkeletonClass	 parent_class;
};

enum
{
	PROP_0,
	PROP_OBJECT_MANAGER,
	PROP_LAST
};

static void sbu_manager_iface_init (SbuManagerIface *iface);

G_DEFINE_TYPE_WITH_CODE (SbuManagerImpl, sbu_manager_impl, SBU_TYPE_MANAGER_SKELETON,
			 G_IMPLEMENT_INTERFACE(SBU_TYPE_MANAGER, sbu_manager_iface_init));

static gboolean
sbu_manager_impl_plugins_call_vfunc (SbuPlugin *plugin,
				     const gchar *function_name,
				     GCancellable *cancellable,
				     GError **error)
{
	gpointer func = NULL;

	/* load the possible symbol */
	func = sbu_plugin_get_symbol (plugin, function_name);
	if (func == NULL) {
		g_debug ("no %s for %s, skipping",
			 function_name,
			 sbu_plugin_get_name (plugin));
		return TRUE;
	}

	/* run the correct vfunc */
	if (g_strcmp0 (function_name, "sbu_plugin_initialize") == 0 ||
	    g_strcmp0 (function_name, "sbu_plugin_destroy") == 0) {
		SbuPluginFunc plugin_func = func;
		plugin_func (plugin);
		return TRUE;
	}
	if (g_strcmp0 (function_name, "sbu_plugin_setup") == 0 ||
	    g_strcmp0 (function_name, "sbu_plugin_refresh") == 0) {
		SbuPluginSetupFunc plugin_func = func;
		return plugin_func (plugin, cancellable, error);
	}
	
	/* failed */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_FAILED,
		     "function_name %s invalid",
		     function_name);
	return FALSE;
}

static gboolean
sbu_manager_impl_poll_cb (gpointer user_data)
{
	SbuManagerImpl *self = SBU_MANAGER_IMPL (user_data);

	/* rescan stuff that can change at runtime */
	for (guint i = 0; i < self->plugins->len; i++) {
		SbuPlugin *plugin = g_ptr_array_index (self->plugins, i);
		g_autoptr(GError) error = NULL;
		if (!sbu_manager_impl_plugins_call_vfunc (plugin,
							  "sbu_plugin_refresh",
						          NULL,
						          &error)) {
			g_warning ("failed to refresh %s: %s",
				   sbu_plugin_get_name (plugin),
				   error->message);
		}
	}

	return TRUE;
}

static void
sbu_manager_impl_poll_start (SbuManagerImpl *self)
{
	if (self->poll_id != 0)
		g_source_remove (self->poll_id);
	self->poll_id = g_timeout_add_seconds (self->poll_interval,
					       sbu_manager_impl_poll_cb,
					       self);
}

static void
sbu_manager_impl_poll_stop (SbuManagerImpl *self)
{
	if (self->poll_id == 0)
		return;
	g_source_remove (self->poll_id);
	self->poll_id = 0;
}

static void
sbu_manager_impl_plugins_update_metadata_cb (SbuPlugin *plugin,
					     SbuDeviceImpl *device,
					     const gchar *key,
					     gint value,
					     SbuManagerImpl *self)
{
	g_autoptr(GError) error = NULL;
	if (!sbu_database_save_value (self->database, key, value, &error))
		g_warning ("%s", error->message);
}

static void
sbu_manager_impl_plugins_remove_device_cb (SbuPlugin *plugin,
					   SbuDeviceImpl *device,
					   SbuManagerImpl *self)
{
	g_debug ("removing device %s", sbu_device_impl_get_object_path (device));
	sbu_device_impl_unexport (device);
	g_ptr_array_remove (self->devices, device);
	if (self->devices->len == 0)
		sbu_manager_impl_poll_stop (self);
}

static void
sbu_manager_impl_save_history (SbuManagerImpl *self, const gchar *object_path, const gchar *propname, GObject *obj)
{
	gint value = -1;

	/* boolean */
	if (g_strcmp0 (propname, "active") == 0) {
		gboolean tmp;
		g_object_get (obj, propname, &tmp, NULL);
		value = (guint) tmp;

	/* double */
	} else if (g_strcmp0 (propname, "power") == 0 ||
		   g_strcmp0 (propname, "current") == 0 ||
		   g_strcmp0 (propname, "voltage") == 0 ||
		   g_strcmp0 (propname, "frequency") == 0) {
		gdouble tmp;
		g_object_get (obj, propname, &tmp, NULL);
		value = tmp * 1000.f;
	}

	/* save to database */
	if (value != -1) {
		g_autofree gchar *key = NULL;
		g_autoptr(GError) error = NULL;
		key = g_strdup_printf ("%s:%s",
				       object_path + strlen (SBU_DBUS_PATH_DEVICE),
				       propname);
		if (!sbu_database_save_value (self->database, key, value, &error))
			g_warning ("%s", error->message);
	}
}

static void
sbu_manager_impl_node_notify_cb (SbuNodeImpl *n, GParamSpec *pspec, gpointer user_data)
{
	SbuManagerImpl *self = SBU_MANAGER_IMPL (user_data);
	g_debug ("changed %s:%s",
		 sbu_node_impl_get_object_path (n),
		 g_param_spec_get_name (pspec));
	sbu_manager_impl_save_history (self,
				       sbu_node_impl_get_object_path (n),
				       g_param_spec_get_name (pspec),
				       G_OBJECT (n));
}

static void
sbu_manager_impl_link_notify_cb (SbuLinkImpl *l, GParamSpec *pspec, gpointer user_data)
{
	SbuManagerImpl *self = SBU_MANAGER_IMPL (user_data);
	g_debug ("changed %s:%s",
		 sbu_link_impl_get_object_path (l),
		 g_param_spec_get_name (pspec));
	sbu_manager_impl_save_history (self,
				       sbu_link_impl_get_object_path (l),
				       g_param_spec_get_name (pspec),
				       G_OBJECT (l));
}

static void
sbu_manager_impl_plugins_add_device_cb (SbuPlugin *plugin,
					SbuDeviceImpl *device,
					SbuManagerImpl *self)
{
	GPtrArray *array;
	g_autofree gchar *object_path = NULL;

	/* just use the array position as the ID */
	object_path = g_strdup_printf ("%s/%u",
				       SBU_DBUS_PATH_DEVICE,
				       self->devices->len);
	g_debug ("adding device %s", object_path);
	g_object_set (device,
		      "object-manager", self->object_manager,
		      "object-path", object_path,
		      NULL);

	/* watch all links and nodes */
	array = sbu_device_impl_get_link_array (device);
	for (guint i = 0; i < array->len; i++) {
		SbuLink *link = g_ptr_array_index (array, i);
		g_signal_connect (link, "notify",
				  G_CALLBACK (sbu_manager_impl_link_notify_cb),
				  self);
	}
	array = sbu_device_impl_get_node_array (device);
	for (guint i = 0; i < array->len; i++) {
		SbuNode *node = g_ptr_array_index (array, i);
		g_signal_connect (node, "notify",
				  G_CALLBACK (sbu_manager_impl_node_notify_cb),
				  self);
	}

	/* export and save device */
	sbu_device_set_database (device, self->database);
	sbu_device_impl_export (device);
	g_ptr_array_add (self->devices, g_object_ref (device));

	/* set up initial poll */
	sbu_manager_impl_poll_start (self);
}

static void
sbu_manager_impl_plugins_open_plugin (SbuManagerImpl *self,
				      const gchar *filename)
{
	SbuPlugin *plugin;
	g_autoptr(GError) error = NULL;

	/* create plugin from file */
	plugin = sbu_plugin_create (filename, &error);
	if (plugin == NULL) {
		g_warning ("Failed to load %s: %s", filename, error->message);
		return;
	}
	g_signal_connect (plugin, "update-metadata",
			  G_CALLBACK (sbu_manager_impl_plugins_update_metadata_cb),
			  self);
	g_signal_connect (plugin, "add-device",
			  G_CALLBACK (sbu_manager_impl_plugins_add_device_cb),
			  self);
	g_signal_connect (plugin, "remove-device",
			  G_CALLBACK (sbu_manager_impl_plugins_remove_device_cb),
			  self);
	g_debug ("opened plugin %s: %s", filename, sbu_plugin_get_name (plugin));

	/* add to array */
	g_ptr_array_add (self->plugins, plugin);
}

static gboolean
sbu_manager_impl_plugins_setup (SbuManagerImpl *self,
				GCancellable *cancellable,
				GError **error)
{
	const gchar *location = PLUGINDIR;
	g_autoptr(GDir) dir = NULL;

	/* search in the plugin directory for plugins */
	dir = g_dir_open (location, 0, error);
	if (dir == NULL)
		return FALSE;

	/* try to open each plugin */
	g_debug ("searching for plugins in %s", location);
	do {
		g_autofree gchar *filename_plugin = NULL;
		const gchar *filename_tmp = g_dir_read_name (dir);
		if (filename_tmp == NULL)
			break;
		if (!g_str_has_suffix (filename_tmp, ".so"))
			continue;
		filename_plugin = g_build_filename (location,
						    filename_tmp,
						    NULL);
		sbu_manager_impl_plugins_open_plugin (self, filename_plugin);
	} while (TRUE);

	/* run the initialize */
	for (guint i = 0; i < self->plugins->len; i++) {
		SbuPlugin *plugin = g_ptr_array_index (self->plugins, i);
		sbu_manager_impl_plugins_call_vfunc (plugin, "sbu_plugin_initialize",
						     cancellable, NULL);
	}

	return TRUE;
}

/* runs in thread dedicated to handling @invocation */
static gboolean
sbu_manager_impl_get_devices (SbuManager *manager,
			      GDBusMethodInvocation *invocation)
{
	SbuManagerImpl *self = SBU_MANAGER_IMPL (manager);
	GVariantBuilder builder;

	g_debug ("handling GetDevices");
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ao)"));
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("ao"));
	for (guint i = 0; i < self->devices->len; i++) {
		SbuDeviceImpl *device = g_ptr_array_index (self->devices, i);
		g_variant_builder_add (&builder, "o",
				       sbu_device_impl_get_object_path (device));
	}
	g_variant_builder_close (&builder);
	g_dbus_method_invocation_return_value (invocation,
					       g_variant_builder_end (&builder));
	return TRUE;
}

static void
sbu_manager_iface_init (SbuManagerIface *iface)
{
	iface->handle_get_devices = sbu_manager_impl_get_devices;
}

static void
sbu_manager_impl_get_property (GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	SbuManagerImpl *self = SBU_MANAGER_IMPL (object);

	switch (prop_id) {
	case PROP_OBJECT_MANAGER:
		g_value_set_object (value, self->object_manager);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sbu_manager_impl_set_property (GObject *object,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	SbuManagerImpl *self = SBU_MANAGER_IMPL (object);

	switch (prop_id) {
	case PROP_OBJECT_MANAGER:
		g_assert (self->object_manager == NULL);
		self->object_manager = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sbu_manager_impl_finalize (GObject *object)
{
	SbuManagerImpl *self = SBU_MANAGER_IMPL (object);

	sbu_manager_impl_poll_stop (self);
	for (guint i = 0; i < self->plugins->len; i++) {
		SbuPlugin *plugin = g_ptr_array_index (self->plugins, i);
		sbu_manager_impl_plugins_call_vfunc (plugin, "sbu_plugin_destroy", NULL, NULL);
	}

	g_object_unref (self->database);
	g_ptr_array_unref (self->plugins);
	g_ptr_array_unref (self->devices);
	G_OBJECT_CLASS (sbu_manager_impl_parent_class)->finalize (object);
}

static gboolean
sbu_manager_impl_plugins_setup_cb (gpointer user_data)
{
	SbuManagerImpl *self = (SbuManagerImpl *) user_data;
	g_autoptr(GError) error = NULL;
	if (!sbu_manager_impl_plugins_setup (self, NULL, &error))
		g_warning ("Failed to set up plugins: %s", error->message);
	return FALSE;
}

void
sbu_manager_impl_start (SbuManagerImpl *self)
{
	/* run setup */
	for (guint i = 0; i < self->plugins->len; i++) {
		g_autoptr(GError) error_local = NULL;
		SbuPlugin *plugin = g_ptr_array_index (self->plugins, i);
		if (!sbu_manager_impl_plugins_call_vfunc (plugin, "sbu_plugin_setup",
							  NULL, &error_local)) {
			g_debug ("disabling %s as setup failed: %s",
				 sbu_plugin_get_name (plugin),
				 error_local->message);
			sbu_plugin_set_enabled (plugin, FALSE);
		}
	}
}


static void
sbu_manager_impl_init (SbuManagerImpl *self)
{
	self->database = sbu_database_new ();
	self->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	self->plugins = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* start up the plugin loader in idle */
	g_idle_add (sbu_manager_impl_plugins_setup_cb, self);

	g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (self),
					     G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
}

static void
sbu_manager_impl_class_init (SbuManagerImplClass *klass_manager)
{
	GObjectClass *klass;
	klass = G_OBJECT_CLASS (klass_manager);
	klass->finalize = sbu_manager_impl_finalize;
	klass->set_property = sbu_manager_impl_set_property;
	klass->get_property = sbu_manager_impl_get_property;

	g_object_class_install_property (klass,
					 PROP_OBJECT_MANAGER,
					 g_param_spec_object ("object-manager",
							      "GDBusObjectManager",
							      "The object manager",
							      G_TYPE_DBUS_OBJECT_MANAGER,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
}

SbuManagerImpl *
sbu_manager_impl_new (GDBusObjectManagerServer *object_manager)
{
	SbuManagerImpl *manager;
	manager = g_object_new (SBU_TYPE_MANAGER_IMPL,
				"version", PACKAGE_VERSION,
				"object-manager", object_manager,
				NULL);
	return SBU_MANAGER_IMPL (manager);
}

gboolean
sbu_manager_impl_setup (SbuManagerImpl *self, GError **error)
{
	g_autofree gchar *location = NULL;
	g_autoptr(SbuConfig) config = sbu_config_new ();

	/* use the system-wide database */
	location = sbu_config_get_string (config, "DatabaseLocation", error);
	if (location == NULL)
		return FALSE;
	sbu_database_set_location (self->database, location);
	if (!sbu_database_open (self->database, error)) {
		g_prefix_error (error, "failed to open database %s: ", location);
		return FALSE;
	}

	/* set the poll interval */
	self->poll_interval = sbu_config_get_integer (config, "DevicePollInterval", error);
	if (self->poll_interval == 0)
		return FALSE;

	/* enable test device */
	if (sbu_config_get_boolean (config, "EnableDummyDevice", NULL))
		g_setenv ("SBU_DUMMY_ENABLE", "", TRUE);

	/* success */
	return TRUE;
}
