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

#include "msx-context.h"
#include "msx-database.h"
#include "msx-device.h"

typedef struct {
	GCancellable		*cancellable;
	GMainLoop		*loop;
	GOptionContext		*context;
	GPtrArray		*cmd_array;
	MsxContext		*msx_context;
	MsxDatabase		*msx_database;
} MsxUtil;

typedef gboolean (*MsxUtilPrivateCb)	(MsxUtil	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar			*name;
	gchar			*arguments;
	gchar			*description;
	MsxUtilPrivateCb	 callback;
} MsxUtilItem;

static void
msx_util_item_free (MsxUtilItem *item)
{
	g_free (item->name);
	g_free (item->arguments);
	g_free (item->description);
	g_free (item);
}

static gint
fu_sort_command_name_cb (MsxUtilItem **item1, MsxUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

static void
msx_util_add (GPtrArray *array,
	      const gchar *name,
	      const gchar *arguments,
	      const gchar *description,
	      MsxUtilPrivateCb callback)
{
	g_auto(GStrv) names = NULL;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (callback != NULL);

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (guint i = 0; names[i] != NULL; i++) {
		MsxUtilItem *item = g_new0 (MsxUtilItem, 1);
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
msx_util_get_descriptions (GPtrArray *array)
{
	gsize len;
	const gsize max_len = 35;
	GString *string;

	/* print each command */
	string = g_string_new ("");
	for (guint i = 0; i < array->len; i++) {
		MsxUtilItem *item = g_ptr_array_index (array, i);
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
msx_util_run (MsxUtil *self, const gchar *command, gchar **values, GError **error)
{
	/* find command */
	for (guint i = 0; i < self->cmd_array->len; i++) {
		MsxUtilItem *item = g_ptr_array_index (self->cmd_array, i);
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

static void
msx_dump_raw (const guint8 *data, gsize len)
{
	g_autoptr(GString) str = g_string_new (NULL);
	if (len == 0)
		return;
	for (gsize i = 0; i < len; i++) {
		g_string_append_printf (str, "%02x ", data[i]);
	}
	g_print ("%s\n", str->str);
}

static void
msx_dump_string (const guint8 *data, gsize len)
{
	g_autoptr(GString) str = g_string_new (NULL);
	for (gsize i = 0; i < len; i++) {
		if (g_ascii_isalnum (data[i]) ||
		    g_ascii_ispunct (data[i]) ||
		    data[i] == ' ') {
			g_string_append_printf (str, "%c", data[i]);
		} else {
			g_string_append_printf (str, "?");
		}
	}
	if (str->len == 0)
		return;
	g_print ("%s\n", str->str);
}

static gboolean
msx_util_repair (MsxUtil *self, gchar **values, GError **error)
{
	/* use the system-wide database */
	if (!msx_database_open (self->msx_database, error))
		return FALSE;
	return msx_database_repair (self->msx_database, error);
}

static gboolean
msx_util_query (MsxUtil *self, gchar **values, GError **error)
{
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	g_autoptr(GPtrArray) results = NULL;

	/* use the system-wide database */
	if (!msx_database_open (self->msx_database, error))
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
	results = msx_database_query (self->msx_database, values[0],
				      MSX_DEVICE_ID_DEFAULT, 0, now, error);
	if (results == NULL)
		return FALSE;
	for (guint i = 0; i < results->len; i++) {
		MsxDatabaseItem *item = g_ptr_array_index (results, i);
		g_print ("%" G_GINT64_FORMAT "\t%.2f\n",
			 item->ts, (gdouble) item->val / 1000.f);
	}

	return TRUE;
}

static gboolean
msx_util_probe (MsxUtil *self, gchar **values, GError **error)
{
	GPtrArray *devices;
	MsxDevice *device;
	g_autoptr(GBytes) data = NULL;

	/* check args */
	if (g_strv_length (values) != 1) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     "Invalid arguments: expected 'cmd'");
		return FALSE;
	}

	/* get device */
	if (!msx_context_coldplug (self->msx_context, error))
		return FALSE;
	devices = msx_context_get_devices (self->msx_context);
	if (devices->len == 0) {
		/* TRANSLATORS: nothing attached that can be upgraded */
		g_print ("%s\n", _("No MSX hardware detected"));
		return TRUE;
	}

	/* open it */
	device = g_ptr_array_index (devices, 0);
	if (!msx_device_open (device, error))
		return FALSE;
	data = msx_device_send_command (device, values[0], error);
	if (data == NULL)
		return FALSE;
	if (!msx_device_close (device, error))
		return FALSE;

	/* dump to screen */
	msx_dump_raw (g_bytes_get_data (data, NULL), g_bytes_get_size (data));
	msx_dump_string (g_bytes_get_data (data, NULL), g_bytes_get_size (data));

	return TRUE;
}

static gboolean
msx_util_devices (MsxUtil *self, gchar **values, GError **error)
{
	GPtrArray *devices;

	/* get all devices */
	if (!msx_context_coldplug (self->msx_context, error))
		return FALSE;
	devices = msx_context_get_devices (self->msx_context);
	if (devices->len == 0) {
		/* TRANSLATORS: nothing attached that can be upgraded */
		g_print ("%s\n", _("No MSX hardware detected"));
		return TRUE;
	}
	for (guint i = 0; i < devices->len; i++) {
		MsxDevice *device = g_ptr_array_index (devices, i);
		if (!msx_device_open (device, error))
			return FALSE;
		g_print ("Device ID:          %u\n", i);
		g_print ("Serial Number:      %s\n",
			 msx_device_get_serial_number (device));
		g_print ("Firmware Version 1: %s\n",
			 msx_device_get_firmware_version1 (device));
		g_print ("Firmware Version 2: %s\n",
			 msx_device_get_firmware_version2 (device));
		if (!msx_device_close (device, error))
			return FALSE;
	}

	return TRUE;
}

static void
msx_util_cancelled_cb (GCancellable *cancellable, gpointer user_data)
{
	MsxUtil *self = (MsxUtil *) user_data;
	/* TRANSLATORS: this is when a device ctrl+c's a watch */
	g_print ("%s\n", _("Cancelled"));
	g_main_loop_quit (self->loop);
}

static void
msx_util_device_added_cb (MsxContext *context,
			  MsxDevice *device,
			  MsxUtil *self)
{
	g_autoptr(GError) error = NULL;

	/* log these */
	msx_device_set_database (device, self->msx_database);

	/* open */
	if (!msx_device_open (device, &error)) {
		g_warning ("failed to open: %s", error->message);
		return;
	}

	/* TRANSLATORS: this is when a device is hotplugged */
	g_print ("%s: %s\n", _("Device added"),
		 msx_device_get_serial_number (device));
}

static void
msx_util_device_removed_cb (MsxContext *context,
			    MsxDevice *device,
			    MsxUtil *self)
{
	g_autoptr(GError) error = NULL;

	/* TRANSLATORS: this is when a device is hotplugged */
	g_print ("%s: %s\n", _("Device removed"),
		 msx_device_get_serial_number (device));

	/* open */
	if (!msx_device_close (device, &error)) {
		g_warning ("failed to close: %s", error->message);
		return;
	}
}

static gboolean
msx_util_daemon (MsxUtil *self, gchar **values, GError **error)
{
	g_autoptr(MsxContext) context = NULL;

	/* use the system-wide database */
	if (!msx_database_open (self->msx_database, error))
		return FALSE;

	/* get all the DFU devices */
	context = msx_context_new ();
	g_signal_connect (context, "added",
			  G_CALLBACK (msx_util_device_added_cb), self);
	g_signal_connect (context, "removed",
			  G_CALLBACK (msx_util_device_removed_cb), self);
	g_signal_connect (self->cancellable, "cancelled",
			  G_CALLBACK (msx_util_cancelled_cb), self);
	if (!msx_context_coldplug (context, error))
		return FALSE;
	g_main_loop_run (self->loop);
	return TRUE;
}

static void
msx_util_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		   const gchar *message, gpointer user_data)
{
}

static gboolean
msx_util_sigint_cb (gpointer user_data)
{
	MsxUtil *self = (MsxUtil *) user_data;
	g_debug ("Handling SIGINT");
	g_cancellable_cancel (self->cancellable);
	return FALSE;
}

static void
msx_util_self_free (MsxUtil *self)
{
	g_main_loop_unref (self->loop);
	g_object_unref (self->cancellable);
	g_ptr_array_unref (self->cmd_array);
	g_option_context_free (self->context);
	g_object_unref (self->msx_context);
	g_object_unref (self->msx_database);
	g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(MsxUtil, msx_util_self_free)

static MsxUtil *
msx_util_self_new (void)
{
	MsxUtil *self = g_new0 (MsxUtil, 1);
	self->loop = g_main_loop_new (NULL, FALSE);
	self->cancellable = g_cancellable_new ();
	self->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) msx_util_item_free);
	self->context = g_option_context_new (NULL);
	self->msx_context = msx_context_new ();
	self->msx_database = msx_database_new ();
	return self;
}

int
main (int argc, char *argv[])
{
	g_autoptr(MsxUtil) self = msx_util_self_new ();
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
	msx_util_add (self->cmd_array,
		      "devices",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Get all devices"),
		      msx_util_devices);
	msx_util_add (self->cmd_array,
		      "probe",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Probe one device"),
		      msx_util_probe);
	msx_util_add (self->cmd_array,
		      "query",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Query one device property"),
		      msx_util_query);
	msx_util_add (self->cmd_array,
		      "repair",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Repair the database"),
		      msx_util_repair);
	msx_util_add (self->cmd_array,
		      "daemon",
		      NULL,
		      /* TRANSLATORS: command description */
		      _("Monitor the daemon for events"),
		      msx_util_daemon);

	/* do stuff on ctrl+c */
	g_unix_signal_add_full (G_PRIORITY_DEFAULT,
				SIGINT, msx_util_sigint_cb,
				self, NULL);

	/* sort by command name */
	g_ptr_array_sort (self->cmd_array,
			  (GCompareFunc) fu_sort_command_name_cb);

	/* get a list of the commands */
	cmd_descriptions = msx_util_get_descriptions (self->cmd_array);
	g_option_context_set_summary (self->context, cmd_descriptions);

	/* use the system database */
	location = g_build_filename ("/var", "lib", "msxd", "sqlite.db", NULL);
	msx_database_set_location (self->msx_database, location);

	/* TRANSLATORS: program name */
	g_set_application_name (_("MSX Utility"));
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
				   msx_util_ignore_cb, NULL);
	}

	/* run the specified command */
	ret = msx_util_run (self, argv[1], (gchar**) &argv[2], &error);
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
