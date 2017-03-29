/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib-unix.h>
#include <locale.h>
#include <stdlib.h>

#include "generated-gdbus.h"

#include "sbu-common.h"
#include "sbu-manager-impl.h"

typedef struct {
	GCancellable			*cancellable;
	GMainLoop			*loop;
	GOptionContext			*context;
	guint				 name_owner_id;
	SbuManagerImpl			*manager;
	GDBusObjectManagerServer	*object_manager;
} SbuUtil;

static gboolean
sbu_main_sigint_cb (gpointer user_data)
{
	SbuUtil *self = (SbuUtil *) user_data;
	g_debug ("handling SIGINT");
	g_cancellable_cancel (self->cancellable);
	g_main_loop_quit (self->loop);
	return FALSE;
}

static void
sbu_main_bus_acquired_cb (GDBusConnection *connection,
			  const gchar *name,
			  gpointer user_data)
{
	SbuUtil *self = (SbuUtil *) user_data;
	g_autoptr(SbuObjectSkeleton) manager_object = NULL;

	g_debug ("connected to the system bus");

	/* create manager object */
	manager_object = sbu_object_skeleton_new (SBU_DBUS_PATH_MANAGER);
	sbu_object_skeleton_set_manager (manager_object, SBU_MANAGER (self->manager));
	g_dbus_object_manager_server_export (self->object_manager,
					     G_DBUS_OBJECT_SKELETON (manager_object));
	g_dbus_object_manager_server_set_connection (self->object_manager, connection);
}

static void
sbu_main_name_lost_cb (GDBusConnection *connection,
		       const gchar *name,
		       gpointer user_data)
{
	SbuUtil *self = (SbuUtil *) user_data;
	g_debug ("lost (or failed to acquire) the name %s on the bus", name);
	g_main_loop_quit (self->loop);
}

static void
sbu_main_name_acquired_cb (GDBusConnection *connection,
			   const gchar *name,
			   gpointer user_data)
{
	SbuUtil *self = (SbuUtil *) user_data;
	g_debug ("acquired the name %s on the bus", name);
	sbu_manager_impl_start (self->manager);
}

static void
sbu_main_self_free (SbuUtil *self)
{
	if (self->name_owner_id != 0)
		g_bus_unown_name (self->name_owner_id);
	g_main_loop_unref (self->loop);
	g_object_unref (self->cancellable);
	g_option_context_free (self->context);
	g_object_unref (self->manager);
	g_object_unref (self->object_manager);
	g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuUtil, sbu_main_self_free)

static SbuUtil *
sbu_main_self_new (void)
{
	SbuUtil *self = g_new0 (SbuUtil, 1);
	self->loop = g_main_loop_new (NULL, FALSE);
	self->cancellable = g_cancellable_new ();
	self->context = g_option_context_new (NULL);
	self->object_manager = g_dbus_object_manager_server_new ("/com/hughski/PowerSBU");
	self->manager = sbu_manager_impl_new (self->object_manager);
	return self;
}

int
main (int argc, char *argv[])
{
	g_autoptr(SbuUtil) self = sbu_main_self_new ();
	gboolean ret;
	gboolean verbose = FALSE;
	g_autoptr(GError) error = NULL;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* avoid GVFS */
	if (!g_setenv ("GIO_USE_VFS", "local", TRUE)) {
		g_printerr ("Error setting GIO_USE_GVFS\n");
		return EXIT_FAILURE;
	}

	/* do stuff on ctrl+c */
	g_unix_signal_add_full (G_PRIORITY_DEFAULT,
				SIGINT, sbu_main_sigint_cb,
				self, NULL);

	/* perform failable manager setup */
	if (!sbu_manager_impl_setup (self->manager, &error)) {
		g_printerr ("%s: %s\n", _("Failed to start object manager"),
			    error->message);
		return EXIT_FAILURE;
	}

	/* TRANSLATORS: program name */
	g_set_application_name (_("SBU Daemon"));
	g_option_context_add_main_entries (self->context, options, NULL);
	ret = g_option_context_parse (self->context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_printerr ("%s: %s\n", _("Failed to parse arguments"),
			    error->message);
		return EXIT_FAILURE;
	}

	/* set verbose? */
	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* try to own name */
	self->name_owner_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
					      SBU_DBUS_NAME,
					      G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
					      G_BUS_NAME_OWNER_FLAGS_REPLACE,
					      sbu_main_bus_acquired_cb,
					      sbu_main_name_acquired_cb,
					      sbu_main_name_lost_cb,
					      self,
					      NULL);
	g_main_loop_run (self->loop);

	/* success */
	return EXIT_SUCCESS;
}
