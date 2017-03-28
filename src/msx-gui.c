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

#include <locale.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "egg-graph-widget.h"

#include "msx-database.h"

typedef struct {
	GtkBuilder	*builder;
	MsxDatabase	*database;
	GtkSizeGroup	*details_sizegroup_title;
	GtkSizeGroup	*details_sizegroup_value;
	GtkWidget	*graph_widget;
} MsxGui;

static void
msx_gui_self_free (MsxGui *self)
{
	g_object_unref (self->builder);
	g_object_unref (self->database);
	g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(MsxGui, msx_gui_self_free)

static int
msx_gui_commandline_cb (GApplication *application,
			GApplicationCommandLine *cmdline,
			MsxGui *self)
{
	gboolean verbose = FALSE;
	gint argc;
	GtkWindow *window;
	g_auto(GStrv) argv;
	g_autoptr(GOptionContext) context = NULL;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		  /* TRANSLATORS: show verbose debugging */
		  N_("Show extra debugging information"), NULL },
		{ NULL}
	};

	/* get arguments */
	argv = g_application_command_line_get_arguments (cmdline, &argc);

	context = g_option_context_new (NULL);
	/* TRANSLATORS: the program name */
	g_option_context_set_summary (context, _("MSX GUI"));
	g_option_context_add_main_entries (context, options, NULL);
	if (!g_option_context_parse (context, &argc, &argv, NULL))
		return FALSE;

	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* make sure the window is raised */
	window = GTK_WINDOW (gtk_builder_get_object (self->builder, "dialog_main"));
	gtk_window_present (window);
	return TRUE;
}

static void
msx_gui_add_details_item (MsxGui *self, const gchar *key, gint value)
{
	GtkWidget *widget_title;
	GtkWidget *widget_value;
	GtkWidget *b;
	GtkWidget *widget;
	const gchar *suffix = NULL;
	GString *str = g_string_new (NULL);

	/* format value */
	g_string_append_printf (str, "%.2f", (gdouble) value / 1000.f);

	/* remove unwanted precision */
	if (g_str_has_suffix (str->str, ".00"))
		g_string_truncate (str, str->len - 3);

	/* add suffix */
	if (g_str_has_suffix (key, "Voltage"))
		suffix = "V";
	else if (g_str_has_suffix (key, "Current"))
		suffix = "A";
	else if (g_str_has_suffix (key, "Frequency"))
		suffix = "Hz";
	else if (g_str_has_suffix (key, "ApparentPower"))
		suffix = "VA";
	else if (g_str_has_suffix (key, "Power"))
		suffix = "W";
	else if (g_str_has_suffix (key, "Percentage") ||
		 g_str_has_suffix (key, "Capacity"))
		suffix = "%";
	else if (g_str_has_suffix (key, "Temperature"))
		suffix = "°F";
	if (suffix != NULL)
		g_string_append (str, suffix);

	b = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_visible (b, TRUE);

	widget_title = gtk_label_new (key);
	gtk_widget_set_visible (widget_title, TRUE);
	gtk_widget_set_hexpand (widget_title, TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget_title), 1.0);
	gtk_size_group_add_widget (self->details_sizegroup_title, widget_title);
	gtk_container_add (GTK_CONTAINER (b), widget_title);

	widget_value = gtk_label_new (str->str);
	gtk_widget_set_visible (widget_value, TRUE);
	gtk_widget_set_hexpand (widget_value, TRUE);
	gtk_label_set_xalign (GTK_LABEL (widget_value), 0.0);
	gtk_size_group_add_widget (self->details_sizegroup_value, widget_value);
	gtk_container_add (GTK_CONTAINER (b), widget_value);

	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "listbox_details"));
	gtk_list_box_prepend (GTK_LIST_BOX (widget), b);
}

static void
msx_gui_refresh_details (MsxGui *self)
{
	g_autoptr(GHashTable) results = NULL;
	g_autoptr(GList) keys = NULL;
	g_autoptr(GError) error = NULL;

	/* get all latest entries */
	results = msx_database_get_latest (self->database, MSX_DEVICE_ID_DEFAULT, &error);
	if (results == NULL) {
		g_warning ("%s", error->message);
		return;
	}
	keys = g_hash_table_get_keys (results);
	for (GList *l = keys; l != NULL; l = l->next) {
		const gchar *key = l->data;
		MsxDatabaseItem *item = g_hash_table_lookup (results, key);
		msx_gui_add_details_item (self, key, item->val);
	}
}

static GPtrArray *
mxs_gui_get_graph_data (MsxGui *self, const gchar *key, guint32 color, GError **error)
{
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	g_autoptr(GPtrArray) data = NULL;
	g_autoptr(GPtrArray) results = NULL;

	results = msx_database_query (self->database, key, MSX_DEVICE_ID_DEFAULT, 0, now, error);
	if (results == NULL)
		return NULL;
	data = g_ptr_array_new_with_free_func ((GDestroyNotify) egg_graph_point_free);
	for (guint i = 0; i < results->len; i++) {
		MsxDatabaseItem *item = g_ptr_array_index (results, i);
		EggGraphPoint *point = egg_graph_point_new ();
		point->x = item->ts - now;
		point->y = (gdouble) item->val / 1000.f;
		point->color = color;
		g_ptr_array_add (data, point);
	}
	return g_steal_pointer (&data);
}

typedef struct {
	const gchar	*key;
	guint32		 color;
	const gchar	*text;
} MsxGuiGraphLine;

static void
msx_gui_history_setup_lines (MsxGui *self, MsxGuiGraphLine *lines)
{
	for (guint i = 0; lines[i].key != NULL; i++) {
		g_autoptr(GError) error = NULL;
		g_autoptr(GPtrArray) data = NULL;

		data = mxs_gui_get_graph_data (self, lines[i].key, lines[i].color, &error);
		if (data == NULL) {
			g_warning ("%s", error->message);
			return;
		}
		egg_graph_widget_data_add (EGG_GRAPH_WIDGET (self->graph_widget),
					   EGG_GRAPH_WIDGET_PLOT_BOTH, data);
		egg_graph_widget_key_legend_add	(EGG_GRAPH_WIDGET (self->graph_widget),
						 lines[i].color, lines[i].text);
	}
}

static void
msx_gui_history_setup_battery_voltage (MsxGui *self)
{
	MsxGuiGraphLine lines[] = {
		{ "BatteryVoltage",		0xff0000,	"Grid" },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", FALSE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_VOLTAGE,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 23.f,
		      "stop-y", (gdouble) 27.f,
		      NULL);
	msx_gui_history_setup_lines (self, lines);
}

static void
msx_gui_history_setup_mains_voltage (MsxGui *self)
{
	MsxGuiGraphLine lines[] = {
		{ "GridVoltage",		0xff0000,	"Unility Grid" },
		{ "AcOutputVoltage",		0x00ff00,	"Interter Output" },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_VOLTAGE,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 220.f,
		      "stop-y", (gdouble) 270.f,
		      NULL);
	msx_gui_history_setup_lines (self, lines);
}

static void
msx_gui_history_setup_power_usage (MsxGui *self)
{
	MsxGuiGraphLine lines[] = {
		{ "AcOutputPower",		0xff0000,	"AcOutputPower" },
		{ "AcOutputActivePower",	0x00ff00,	"AcOutputActivePower" },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_POWER,
		      "autorange-y", TRUE,
		      NULL);
	msx_gui_history_setup_lines (self, lines);
}

static void
msx_gui_history_setup_current (MsxGui *self)
{
	MsxGuiGraphLine lines[] = {
		{ "PvInputCurrentForBattery",	0xff0000,	"PvInputCurrentForBattery" },
		{ "BatteryCurrent",		0x00ff00,	"BatteryCurrent" },
		{ "BatteryDischargeCurrent",	0x0000ff,	"BatteryDischargeCurrent" },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_CURRENT,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 0.f,
		      "stop-y", (gdouble) 15.f,
		      NULL);
	msx_gui_history_setup_lines (self, lines);
}

static void
msx_gui_history_row_selected_cb (GtkListBox *box, GtkListBoxRow *row, MsxGui *self)
{
	guint idx = gtk_list_box_row_get_index (row);
	g_debug ("row now %u", idx);
	switch (idx) {
	case 0:
		msx_gui_history_setup_battery_voltage (self);
		break;
	case 1:
		msx_gui_history_setup_mains_voltage (self);
		break;
	case 2:
		msx_gui_history_setup_power_usage (self);
		break;
	case 3:
		msx_gui_history_setup_current (self);
		break;
	default:
		break;
	}
}

static void
msx_gui_add_history_item (MsxGui *self, const gchar *title)
{
	GtkWidget *widget_title;
	GtkWidget *widget;

	widget_title = gtk_label_new (title);
	gtk_widget_set_visible (widget_title, TRUE);
	gtk_widget_set_margin_start (widget_title, 12);
	gtk_widget_set_margin_end (widget_title, 12);
	gtk_widget_set_margin_top (widget_title, 6);
	gtk_widget_set_margin_bottom (widget_title, 6);
	gtk_label_set_xalign (GTK_LABEL (widget_title), 0.0);

	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "listbox_history"));
	gtk_list_box_prepend (GTK_LIST_BOX (widget), widget_title);
}

static void
msx_gui_refresh_history (MsxGui *self)
{
	msx_gui_add_history_item (self, "Current Flow");
	msx_gui_add_history_item (self, "Power Usage");
	msx_gui_add_history_item (self, "Utility Voltage");
	msx_gui_add_history_item (self, "Battery Voltage");
}

static void
msx_gui_startup_cb (GApplication *application, MsxGui *self)
{
	GtkWidget *widget;
	GtkWindow *window;
	guint retval;
	GError *error = NULL;
	g_autofree gchar *location = NULL;

	/* get UI */
	self->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_resource (self->builder,
						"/com/hughski/MsxGui/msx-gui.ui",
						&error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
	}

	window = GTK_WINDOW (gtk_builder_get_object (self->builder, "dialog_main"));
	gtk_window_set_application (window, GTK_APPLICATION (application));
	gtk_window_set_default_size (window, 1200, 500);
	gtk_window_set_application (window, GTK_APPLICATION (application));
	gtk_window_set_default_icon_name ("battery-good-charging");

	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "dialog_main"));
	gtk_widget_show (widget);

	/* load database */
	self->database = msx_database_new ();
	location = g_build_filename ("/var", "lib", "msxd", "sqlite.db", NULL);
	msx_database_set_location (self->database, location);
	if (!msx_database_open (self->database, &error)) {
		g_warning ("failed to load database: %s", error->message);
		g_error_free (error);
	}

	/* set up details page */
	self->details_sizegroup_title = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	self->details_sizegroup_value = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* set up history page */
	self->graph_widget = egg_graph_widget_new ();
	gtk_widget_set_visible (self->graph_widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "box_history"));
	gtk_box_pack_start (GTK_BOX (widget), self->graph_widget, TRUE, TRUE, 0);
	gtk_widget_set_size_request (self->graph_widget, 500, 250);
	g_object_set (self->graph_widget,
		      "type-x", EGG_GRAPH_WIDGET_KIND_TIME,
		      "autorange-x", TRUE,
		      NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "listbox_history"));
	g_signal_connect (widget, "row-selected",
			  G_CALLBACK (msx_gui_history_row_selected_cb), self);

	/* populate pages */
	msx_gui_refresh_details (self);
	msx_gui_refresh_history (self);
}

int
main (int argc, char *argv[])
{
	g_autoptr(GtkApplication) application = NULL;
	g_autoptr(MsxGui) self = g_new0 (MsxGui, 1);

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* are we already activated? */
	application = gtk_application_new ("com.hughski.MsxGui",
					   G_APPLICATION_HANDLES_COMMAND_LINE);
	g_signal_connect (application, "startup",
			  G_CALLBACK (msx_gui_startup_cb), self);
	g_signal_connect (application, "command-line",
			  G_CALLBACK (msx_gui_commandline_cb), self);

	/* run */
	return g_application_run (G_APPLICATION (application), argc, argv);
}
