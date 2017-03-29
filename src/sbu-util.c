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
#include <gio/gio.h>
#include <locale.h>
#include <stdlib.h>

#include "generated-gdbus.h"

#include "sbu-common.h"
#include "sbu-config.h"
#include "sbu-database.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuDevice, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuElement, g_object_unref)

typedef struct {
	GCancellable		*cancellable;
	GMainLoop		*loop;
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	SbuDatabase		*sbu_database;
	SbuConfig		*sbu_config;
} SbuUtil;

typedef gboolean (*SbuUtilPrivateCb)	(SbuUtil	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar			*name;
	gchar			*arguments;
	gchar			*description;
	SbuUtilPrivateCb	 callback;
} SbuUtilItem;

static void
sbu_util_item_free (SbuUtilItem *item)
{
	g_free (item->name);
	g_free (item->arguments);
	g_free (item->description);
	g_free (item);
}

static gint
fu_sort_command_name_cb (SbuUtilItem **item1, SbuUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

static void
sbu_util_add (GPtrArray *array,
	      const gchar *name,
	      const gchar *arguments,
	      const gchar *description,
	      SbuUtilPrivateCb callback)
{
	g_auto(GStrv) names = NULL;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (callback != NULL);

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (guint i = 0; names[i] != NULL; i++) {
		SbuUtilItem *item = g_new0 (SbuUtilItem, 1);
		item->name = g_strdup (names[i]);
		if (i == 0) {
			item->description = g_strdup (description);
		} else {
			/* TRANSLATORS: this is a command alias, e.g. 'get-devices' */
			item->description = g_strdup_printf (_("Alias to %s"),
							     names[0]);
		}
		item->arguments = g_strdup (arguments);
		item->callback = callback;
		g_ptr_array_add (array, item);
	}
}

static gchar *
sbu_util_get_descriptions (GPtrArray *array)
{
	gsize len;
	const gsize max_len = 35;
	GString *string;

	/* print each command */
	string = g_string_new ("");
	for (guint i = 0; i < array->len; i++) {
		SbuUtilItem *item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name) + 2;
		if (item->arguments != NULL) {
			g_string_append (string, " ");
			g_string_append (string, item->arguments);
			len += strlen (item->arguments) + 1;
		}
		if (len < max_len) {
			for (gsize j = len; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		} else {
			g_string_append_c (string, '\n');
			for (gsize j = 0; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		}
	}

	/* remove trailing newline */
	if (string->len > 0)
		g_string_set_size (string, string->len - 1);

	return g_string_free (string, FALSE);
}

static gboolean
sbu_util_run (SbuUtil *self, const gchar *command, gchar **values, GError **error)
{
	/* find command */
	for (guint i = 0; i < self->cmd_array->len; i++) {
		SbuUtilItem *item = g_ptr_array_index (self->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0)
			return item->callback (self, values, error);
	}

	/* not found */
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_ARGUMENT,
			     /* TRANSLATORS: error message */
			     _("Command not found"));
	return FALSE;
}

static gboolean
sbu_util_repair (SbuUtil *self, gchar **values, GError **error)
{
	/* use the system-wide database */
	if (!sbu_database_open (self->sbu_database, error))
		return FALSE;
	return sbu_database_repair (self->sbu_database, error);
}

static gboolean
sbu_util_query (SbuUtil *self, gchar **values, GError **error)
{
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	g_autoptr(GPtrArray) results = NULL;

	/* use the system-wide database */
	if (!sbu_database_open (self->sbu_database, error))
		return FALSE;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     "Invalid arguments: expected device key");
		return FALSE;
	}

	/* query database */
	results = sbu_database_query (self->sbu_database, values[0],
				      SBU_DEVICE_ID_DEFAULT, 0, now, error);
	if (results == NULL)
		return FALSE;
	for (guint i = 0; i < results->len; i++) {
		SbuDatabaseItem *item = g_ptr_array_index (results, i);
		g_print ("%" G_GINT64_FORMAT "\t%.2f\n",
			 item->ts, (gdouble) item->val / 1000.f);
	}

	return TRUE;
}

static gboolean
sbu_util_dump_element (SbuUtil *self, const gchar *object_path, GError **error)
{
	g_auto(GStrv) elements = NULL;
	g_autoptr(SbuElement) element = NULL;

	g_print ("Querying element: %s\n", object_path);
	element = sbu_element_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						      G_DBUS_PROXY_FLAGS_NONE,
						      SBU_DBUS_NAME,
						      object_path,
						      self->cancellable,
						      error);
	if (element == NULL)
		return FALSE;

	/* show element properties */
	g_print ("Kind:        %s\n", sbu_element_kind_to_string (sbu_element_get_kind (element)));
	g_print ("Voltage:     %.2f\n", sbu_element_get_voltage (element));
	g_print ("Current:     %.2f\n", sbu_element_get_current (element));
	g_print ("Power:       %.2f\n", sbu_element_get_power (element));
	g_print ("Frequency:   %.2f\n", sbu_element_get_frequency (element));
	return TRUE;
}

static gboolean
sbu_util_dump_device (SbuUtil *self, const gchar *object_path, GError **error)
{
	g_auto(GStrv) elements = NULL;
	g_autoptr(SbuDevice) device = NULL;

	g_print ("Querying device:  %s\n", object_path);
	device = sbu_device_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						    G_DBUS_PROXY_FLAGS_NONE,
						    SBU_DBUS_NAME,
						    object_path,
						    self->cancellable,
						    error);
	if (device == NULL)
		return FALSE;

	/* show device properties */
	g_print ("Firmware Version: %s\n", sbu_device_get_firmware_version (device));
	g_print ("Description:      %s\n", sbu_device_get_description (device));
	g_print ("Serial Number:    %s\n", sbu_device_get_serial_number (device));

	/* show each element */
	if (!sbu_device_call_get_elements_sync (device,
						&elements,
						self->cancellable,
						error))
		return FALSE;
	for (guint i = 0; elements[i] != NULL; i++) {
		if (!sbu_util_dump_element (self, elements[i], error))
			return FALSE;
	}

	return TRUE;
}

static gboolean
sbu_util_dump (SbuUtil *self, gchar **values, GError **error)
{
	g_auto(GStrv) devices = NULL;
	g_autoptr(SbuManager) manager = NULL;

	manager = sbu_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
						      G_DBUS_PROXY_FLAGS_NONE,
						      SBU_DBUS_NAME,
						      SBU_DBUS_PATH_MANAGER,
						      self->cancellable,
						      error);
	if (manager == NULL)
		return FALSE;

	/* show daemon properties */
	g_print ("Client version:   %s\n", PACKAGE_VERSION);
	g_print ("Daemon version:   %s\n", sbu_manager_get_version (manager));

	/* show each device */
	if (!sbu_manager_call_get_devices_sync (manager,
						&devices,
						self->cancellable,
						error))
		return FALSE;
	for (guint i = 0; devices[i] != NULL; i++) {
		if (!sbu_util_dump_device (self, devices[i], error))
			return FALSE;
	}

	return TRUE;
}

static void
sbu_util_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		   const gchar *message, gpointer user_data)
{
}

static gboolean
sbu_util_sigint_cb (gpointer user_data)
{
	SbuUtil *self = (SbuUtil *) user_data;
	g_debug ("Handling SIGINT");
	g_cancellable_cancel (self->cancellable);
	return FALSE;
}

static void
sbu_util_self_free (SbuUtil *self)
{
	g_main_loop_unref (self->loop);
	g_object_unref (self->cancellable);
	g_ptr_array_unref (self->cmd_array);
	g_option_context_free (self->context);
	g_object_unref (self->sbu_database);
	g_object_unref (self->sbu_config);
	g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuUtil, sbu_util_self_free)

static SbuUtil *
sbu_util_self_new (void)
{
	SbuUtil *self = g_new0 (SbuUtil, 1);
	self->loop = g_main_loop_new (NULL, FALSE);
	self->cancellable = g_cancellable_new ();
	self->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) sbu_util_item_free);
	self->context = g_option_context_new (NULL);
	self->sbu_database = sbu_database_new ();
	self->sbu_config = sbu_config_new ();
	return self;
}

int
main (int argc, char *argv[])
{
	g_autoptr(SbuUtil) self = sbu_util_self_new ();
	gboolean ret;
	gboolean verbose = FALSE;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *cmd_descriptions = NULL;
	g_autofree gchar *location = NULL;
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

	/* add commands */
	sbu_util_add (self->cmd_array,
		      "dump",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Dump all properties on all devices"),
		      sbu_util_dump);
	sbu_util_add (self->cmd_array,
		      "query",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Query one device property"),
		      sbu_util_query);
	sbu_util_add (self->cmd_array,
		      "repair",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Repair the database"),
		      sbu_util_repair);

	/* do stuff on ctrl+c */
	g_unix_signal_add_full (G_PRIORITY_DEFAULT,
				SIGINT, sbu_util_sigint_cb,
				self, NULL);

	/* sort by command name */
	g_ptr_array_sort (self->cmd_array,
			  (GCompareFunc) fu_sort_command_name_cb);

	/* get a list of the commands */
	cmd_descriptions = sbu_util_get_descriptions (self->cmd_array);
	g_option_context_set_summary (self->context, cmd_descriptions);

	/* get the system database from the config file */
	location = sbu_config_get_string (self->sbu_config, "DatabaseLocation", NULL);
	sbu_database_set_location (self->sbu_database, location);

	/* TRANSLATORS: program name */
	g_set_application_name (_("SBU Utility"));
	g_option_context_add_main_entries (self->context, options, NULL);
	ret = g_option_context_parse (self->context, &argc, &argv, &error);
	if (!ret) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n", _("Failed to parse arguments"),
			 error->message);
		return EXIT_FAILURE;
	}

	/* set verbose? */
	if (verbose) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	} else {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   sbu_util_ignore_cb, NULL);
	}

	/* run the specified command */
	ret = sbu_util_run (self, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT)) {
			g_autofree gchar *tmp = NULL;
			tmp = g_option_context_get_help (self->context, TRUE, NULL);
			g_print ("%s\n\n%s", error->message, tmp);
		} else {
			g_print ("%s\n", error->message);
		}
		return EXIT_FAILURE;
	}

	/* success */
	return EXIT_SUCCESS;
}
