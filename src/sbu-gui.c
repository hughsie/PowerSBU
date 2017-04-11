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
#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <appstream-glib.h>

#include "generated-gdbus.h"

#include "egg-graph-widget.h"

#include "sbu-common.h"
#include "sbu-config.h"
#include "sbu-database.h"
#include "sbu-gui-resources.h"
#include "sbu-xml-modifier.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuDevice, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuNode, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuLink, g_object_unref)

typedef struct {
	GtkBuilder	*builder;
	GCancellable	*cancellable;
	SbuConfig	*config;
	SbuDatabase	*database;
	SbuManager	*manager;
	SbuDevice	*device;
	GtkSizeGroup	*details_sizegroup_title;
	GtkSizeGroup	*details_sizegroup_value;
	GtkWidget	*graph_widget;
	GPtrArray	*nodes;
	GPtrArray	*links;
	guint		 refresh_id;
	gint64		 history_interval;
} SbuGui;

static void
sbu_gui_self_free (SbuGui *self)
{
	if (self->refresh_id != 0)
		g_source_remove (self->refresh_id);
	if (self->device != NULL)
		g_object_unref (self->device);
	if (self->manager != NULL)
		g_object_unref (self->manager);
	g_ptr_array_unref (self->nodes);
	g_ptr_array_unref (self->links);
	g_object_unref (self->builder);
	g_object_unref (self->database);
	g_object_unref (self->config);
	g_object_unref (self->cancellable);
	g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuGui, sbu_gui_self_free)

static int
sbu_gui_commandline_cb (GApplication *application,
			GApplicationCommandLine *cmdline,
			SbuGui *self)
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

	context = g_option_context_new (NULL);
	/* TRANSLATORS: the program name */
	g_option_context_set_summary (context, _("PowerSBU GUI"));
	g_option_context_add_main_entries (context, options, NULL);
	argv = g_application_command_line_get_arguments (cmdline, &argc);
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
sbu_gui_add_details_item (SbuGui *self, const gchar *key, gint value)
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
sbu_gui_refresh_details (SbuGui *self)
{
	GtkWidget *widget;
	g_autoptr(GHashTable) results = NULL;
	g_autoptr(GList) keys = NULL;
	g_autoptr(GError) error = NULL;

	/* get all latest entries */
	results = sbu_database_get_latest (self->database, SBU_DEVICE_ID_DEFAULT, &error);
	if (results == NULL) {
		g_warning ("%s", error->message);
		return;
	}
	keys = g_hash_table_get_keys (results);
	for (GList *l = keys; l != NULL; l = l->next) {
		const gchar *key = l->data;
		SbuDatabaseItem *item = g_hash_table_lookup (results, key);
		sbu_gui_add_details_item (self, key, item->val);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "listbox_details"));
	gtk_list_box_invalidate_sort (GTK_LIST_BOX (widget));
}

static GPtrArray *
mxs_gui_get_graph_data (SbuGui *self, const gchar *key, guint32 color, GError **error)
{
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	g_autoptr(GPtrArray) data = NULL;
	g_autoptr(GPtrArray) results = NULL;

	results = sbu_database_query (self->database,
				      key,
				      SBU_DEVICE_ID_DEFAULT,
				      now - self->history_interval,
				      now,
				      error);
	if (results == NULL)
		return NULL;
	data = g_ptr_array_new_with_free_func ((GDestroyNotify) egg_graph_point_free);
	for (guint i = 0; i < results->len; i++) {
		SbuDatabaseItem *item = g_ptr_array_index (results, i);
		EggGraphPoint *point = egg_graph_point_new ();
		point->x = item->ts - now;
		point->y = (gdouble) item->val;
		if (fabs (point->y) > 1.1f)
			point->y /= 1000.f;
		point->color = color;
		g_ptr_array_add (data, point);
	}
	return g_steal_pointer (&data);
}

typedef struct {
	const gchar	*key;
	guint32		 color;
	const gchar	*text;
} PowerSBUGraphLine;

static void
sbu_gui_history_setup_lines (SbuGui *self, PowerSBUGraphLine *lines)
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
					   EGG_GRAPH_WIDGET_PLOT_POINTS, data);
		egg_graph_widget_key_legend_add	(EGG_GRAPH_WIDGET (self->graph_widget),
						 lines[i].color, lines[i].text);
	}
}

#define CC_BATTERY	0xcc0000
#define CC_SOLAR	0xcccc00
#define CC_UTILITY	0x00cc00
#define CC_LOAD		0x4444cc

static void
sbu_gui_history_setup_battery_voltage (SbuGui *self)
{
	PowerSBUGraphLine lines[] = {
		{ "/0/node_battery:voltage",	CC_BATTERY,	_("Battery Voltage") },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", FALSE,
		      "mirror-y", FALSE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_VOLTAGE,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 23.f,
		      "stop-y", (gdouble) 28.f,
		      NULL);
	sbu_gui_history_setup_lines (self, lines);
}

static void
sbu_gui_history_setup_mains_voltage (SbuGui *self)
{
	PowerSBUGraphLine lines[] = {
		{ "/0/node_utility:voltage",	CC_UTILITY,	_("Utility Voltage") },
		{ "/0/node_load:voltage",	CC_LOAD,	_("Load Voltage") },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "mirror-y", FALSE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_VOLTAGE,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 220.f,
		      "stop-y", (gdouble) 270.f,
		      NULL);
	sbu_gui_history_setup_lines (self, lines);
}

static void
sbu_gui_history_setup_power_usage (SbuGui *self)
{
	PowerSBUGraphLine lines[] = {
		{ "/0/node_solar:power",	CC_SOLAR,	_("Solar Power") },
		{ "/0/node_battery:power",	CC_BATTERY,	_("Battery Power") },
		{ "/0/node_utility:power",	CC_UTILITY,	_("Utility Power") },
		{ "/0/node_load:power",		CC_LOAD,	_("Load Power") },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "mirror-y", TRUE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_POWER,
		      "autorange-y", TRUE,
		      NULL);
	sbu_gui_history_setup_lines (self, lines);
}

static void
sbu_gui_history_setup_current (SbuGui *self)
{
	PowerSBUGraphLine lines[] = {
		{ "/0/node_solar:current",	CC_SOLAR,	_("Solar Current") },
		{ "/0/node_battery:current",	CC_BATTERY,	_("Battery Current") },
		{ "/0/node_utility:current",	CC_UTILITY,	_("Utility Current") },
		{ "/0/node_load:current",	CC_LOAD,	_("Load Current") },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "mirror-y", TRUE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_CURRENT,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 0.f,
		      "stop-y", (gdouble) 15.f,
		      NULL);
	sbu_gui_history_setup_lines (self, lines);
}

static void
sbu_gui_history_setup_panel_voltage (SbuGui *self)
{
	PowerSBUGraphLine lines[] = {
		{ "PvInputVoltage",		0xff0000,	"PvInputVoltage" },
		{ "/0/node_solar:voltage",	CC_SOLAR,	_("Solar Voltage") },
		{ NULL,				0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "mirror-y", FALSE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_VOLTAGE,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 0.f,
		      "stop-y", (gdouble) 40.f,
		      NULL);
	sbu_gui_history_setup_lines (self, lines);
}

static void
sbu_gui_history_setup_status (SbuGui *self)
{
	PowerSBUGraphLine lines[] = {
		{ "ChargingOn",				0xff0000,	"ChargingOn" },
		{ "ChargingOnSolar",			0x00ff00,	"ChargingOnSolar" },
		{ "ChargingOnAC",			0x0000ff,	"ChargingOnAC" },
		{ "/0/link_solar_battery:active",	CC_SOLAR,	_("Solar ⇢ Battery") },
		{ "/0/link_battery_load:active",	CC_BATTERY,	_("Battery ⇢ Load") },
		{ "/0/link_utility_battery:active",	0xcc00cc,	_("Utility ⇢ Battery") },
		{ "/0/link_utility_load:active",	CC_UTILITY,	_("Utility ⇢ Load") },
		{ "/0/link_solar_load:active",		CC_LOAD,	_("Solar ⇢ Load") },
		{ NULL,					0x000000,	NULL },
	};
	egg_graph_widget_key_legend_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	egg_graph_widget_data_clear (EGG_GRAPH_WIDGET (self->graph_widget));
	g_object_set (self->graph_widget,
		      "use-legend", TRUE,
		      "mirror-y", FALSE,
		      "type-y", EGG_GRAPH_WIDGET_KIND_FACTOR,
		      "autorange-y", FALSE,
		      "start-y", (gdouble) 0.f,
		      "stop-y", (gdouble) 1.f,
		      NULL);
	sbu_gui_history_setup_lines (self, lines);
}

static void
sbu_gui_history_refresh_graph (SbuGui *self)
{
	GtkListBox *list_box;
	GtkListBoxRow *row;

	list_box = GTK_LIST_BOX (gtk_builder_get_object (self->builder, "listbox_history"));
	row = gtk_list_box_get_selected_row (list_box);
	if (row == NULL)
		return;
	switch (gtk_list_box_row_get_index (row)) {
	case 0:
		sbu_gui_history_setup_battery_voltage (self);
		break;
	case 1:
		sbu_gui_history_setup_mains_voltage (self);
		break;
	case 2:
		sbu_gui_history_setup_power_usage (self);
		break;
	case 3:
		sbu_gui_history_setup_current (self);
		break;
	case 4:
		sbu_gui_history_setup_panel_voltage (self);
		break;
	case 5:
		sbu_gui_history_setup_status (self);
		break;
	default:
		break;
	}
}

static void
sbu_gui_history_row_selected_cb (GtkListBox *box, GtkListBoxRow *row, SbuGui *self)
{
	sbu_gui_history_refresh_graph (self);
}

static void
sbu_gui_add_history_item (SbuGui *self, const gchar *title)
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
sbu_gui_refresh_history (SbuGui *self)
{
	sbu_gui_add_history_item (self, "Status");
	sbu_gui_add_history_item (self, "Panel Voltage");
	sbu_gui_add_history_item (self, "Current Flow");
	sbu_gui_add_history_item (self, "Power Usage");
	sbu_gui_add_history_item (self, "Utility Voltage");
	sbu_gui_add_history_item (self, "Battery Voltage");
}

static SbuNode *
sbu_gui_get_node (SbuGui *self, SbuNodeKind kind)
{
	for (guint i = 0; i < self->nodes->len; i++) {
		SbuNode *node = g_ptr_array_index (self->nodes, i);
		SbuNodeKind kind_tmp;
		g_object_get (node, "kind", &kind_tmp, NULL);
		if (kind == kind_tmp)
			return node;
	}
	return NULL;
}

static SbuLink *
sbu_gui_get_link (SbuGui *self, SbuNodeKind src, SbuNodeKind dst)
{
	for (guint i = 0; i < self->links->len; i++) {
		SbuLink *link = g_ptr_array_index (self->links, i);
		SbuNodeKind src_tmp;
		SbuNodeKind dst_tmp;
		g_object_get (link,
			      "src", &src_tmp,
			      "dst", &dst_tmp,
			      NULL);
		if (src == src_tmp && dst == dst_tmp)
			return link;
	}
	return NULL;
}

static gchar *
sbu_gui_format_voltage (SbuNode *node)
{
	if (sbu_node_get_voltage_max (node) > 0) {
		g_autofree gchar *tmp1 = sbu_format_for_display (sbu_node_get_voltage (node), "V");
		g_autofree gchar *tmp2 = sbu_format_for_display (sbu_node_get_voltage_max (node), "V");
		return g_strdup_printf ("%s/%s", tmp1, tmp2);
	}
	return sbu_format_for_display (sbu_node_get_voltage (node), "V");
}

static gchar *
sbu_gui_format_watts (SbuNode *node)
{
	if (sbu_node_get_power_max (node) > 0) {
		g_autofree gchar *tmp1 = sbu_format_for_display (sbu_node_get_power (node), "W");
		g_autofree gchar *tmp2 = sbu_format_for_display (sbu_node_get_power_max (node), "W");
		return g_strdup_printf ("%s/%s", tmp1, tmp2);
	}
	return sbu_format_for_display (sbu_node_get_power (node), "W");
}

#define SBU_SVG_ID_TEXT_SERIAL_NUMBER		"tspan8822"
#define SBU_SVG_ID_TEXT_FIRMWARE_VERSION	"tspan8818"
#define SBU_SVG_ID_TEXT_DEVICE_MODEL		"tspan8814"

#define SBU_SVG_ID_TEXT_SOLAR_TO_UTILITY	"tspan8408"
#define SBU_SVG_ID_TEXT_BATTERY_VOLTAGE		"tspan8424"
#define SBU_SVG_ID_TEXT_BATTERY_TO_LOAD		"tspan8438"

#define SBU_SVG_ID_TEXT_LOAD			"tspan8442"
#define SBU_SVG_ID_TEXT_SOLAR			"tspan8410"
#define SBU_SVG_ID_TEXT_UTILITY			"tspan8725"

#define SBU_SVG_ID_PATH_SOLAR_TO_UTILITY	"path5032"
#define SBU_SVG_ID_PATH_SOLAR_TO_LOAD		"path6089"
#define SBU_SVG_ID_PATH_SOLAR_TO_BATTERY	"path6089-0"
#define SBU_SVG_ID_PATH_UTILITY_TO_BATTERY	"path6089-9"
#define SBU_SVG_ID_PATH_UTILITY_TO_LOAD		"path6089-5"
#define SBU_SVG_ID_PATH_BATTERY_TO_LOAD		"path6089-06"

#define SBU_SVG_OFFSCREEN			"-999"
#define SBU_SVG_STYLE_INACTIVE			"fill:none;stroke:#cccccc;stroke-width:5;stroke-dasharray:5, 5;stroke-dashoffset:0"

static void
sbu_gui_refresh_overview_link (SbuLink *l, SbuXmlModifier *xml_mod, const gchar *id)
{
	gboolean active;
	if (l == NULL) {
		sbu_xml_modifier_replace_attr (xml_mod, id, "style", "");
		return;
	}
	g_object_get (l, "active", &active, NULL);
	if (!active) {
		sbu_xml_modifier_replace_attr (xml_mod, id, "style", SBU_SVG_STYLE_INACTIVE);
		return;
	}
}

static void
sbu_gui_refresh_overview (SbuGui *self)
{
	GtkWidget *widget;
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GInputStream) stream = NULL;
	g_autoptr(GString) svg_data = NULL;
	SbuNode *n;
	SbuLink *l;
	g_autoptr(SbuXmlModifier) xml_mod = sbu_xml_modifier_new ();

	/* load GResource */
	bytes = g_resource_lookup_data (sbu_get_resource (),
					"/com/hughski/PowerSBU/sbu-overview.svg",
					G_RESOURCE_LOOKUP_FLAGS_NONE,
					&error);
	if (bytes == NULL) {
		g_warning ("failed to load image: %s", error->message);
		return;
	}

	/* load XML */
	if (self->device != NULL) {
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_SERIAL_NUMBER,
						sbu_device_get_serial_number (self->device));
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_FIRMWARE_VERSION,
						sbu_device_get_firmware_version (self->device));
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_DEVICE_MODEL,
						sbu_device_get_description (self->device));
	}

	/* not supported yet */
	sbu_xml_modifier_replace_attr (xml_mod, SBU_SVG_ID_TEXT_SOLAR_TO_UTILITY,
				       "x", SBU_SVG_OFFSCREEN);

	/* set up links to have the correct style */
	l = sbu_gui_get_link (self, SBU_NODE_KIND_SOLAR, SBU_NODE_KIND_UTILITY);
	sbu_gui_refresh_overview_link (l, xml_mod, SBU_SVG_ID_PATH_SOLAR_TO_UTILITY);
	l = sbu_gui_get_link (self, SBU_NODE_KIND_SOLAR, SBU_NODE_KIND_LOAD);
	sbu_gui_refresh_overview_link (l, xml_mod, SBU_SVG_ID_PATH_SOLAR_TO_LOAD);
	l = sbu_gui_get_link (self, SBU_NODE_KIND_SOLAR, SBU_NODE_KIND_BATTERY);
	sbu_gui_refresh_overview_link (l, xml_mod, SBU_SVG_ID_PATH_SOLAR_TO_BATTERY);
	l = sbu_gui_get_link (self, SBU_NODE_KIND_UTILITY, SBU_NODE_KIND_BATTERY);
	sbu_gui_refresh_overview_link (l, xml_mod, SBU_SVG_ID_PATH_UTILITY_TO_BATTERY);
	l = sbu_gui_get_link (self, SBU_NODE_KIND_UTILITY, SBU_NODE_KIND_LOAD);
	sbu_gui_refresh_overview_link (l, xml_mod, SBU_SVG_ID_PATH_UTILITY_TO_LOAD);
	l = sbu_gui_get_link (self, SBU_NODE_KIND_BATTERY, SBU_NODE_KIND_LOAD);
	sbu_gui_refresh_overview_link (l, xml_mod, SBU_SVG_ID_PATH_BATTERY_TO_LOAD);

	/* battery */
	n = sbu_gui_get_node (self, SBU_NODE_KIND_BATTERY);
	if (n != NULL) {
		g_autofree gchar *tmp = sbu_gui_format_voltage (n);
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_BATTERY_VOLTAGE, tmp);
	} else {
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_BATTERY_VOLTAGE, "?");
	}
	if (n != NULL) {
		if (sbu_node_get_power (n) < 0) {
			sbu_xml_modifier_replace_attr (xml_mod, SBU_SVG_ID_TEXT_BATTERY_TO_LOAD,
						       "x", SBU_SVG_OFFSCREEN);
		} else {
			g_autofree gchar *tmp = sbu_gui_format_watts (n);
			sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_BATTERY_TO_LOAD, tmp);
		}
	} else {
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_BATTERY_TO_LOAD, "?");
	}

	/* load */
	n = sbu_gui_get_node (self, SBU_NODE_KIND_LOAD);
	if (n != NULL) {
		g_autofree gchar *tmp = sbu_gui_format_watts (n);
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_LOAD, tmp);
	} else {
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_LOAD, "?");
	}

	/* solar */
	n = sbu_gui_get_node (self, SBU_NODE_KIND_SOLAR);
	if (n != NULL) {
		g_autofree gchar *tmp = sbu_gui_format_watts (n);
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_SOLAR, tmp);
	} else {
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_SOLAR, "?");
	}

	/* utility */
	n = sbu_gui_get_node (self, SBU_NODE_KIND_UTILITY);
	if (n != NULL) {
		g_autofree gchar *tmp = sbu_gui_format_watts (n);
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_UTILITY, tmp);
	} else {
		sbu_xml_modifier_replace_cdata (xml_mod, SBU_SVG_ID_TEXT_UTILITY, "?");
	}

	/* process replacements */
	svg_data = sbu_xml_modifier_process (xml_mod,
					     g_bytes_get_data (bytes, NULL),
					     g_bytes_get_size (bytes),
					     &error);
	if (svg_data == NULL) {
		g_warning ("failed to modify the SVG image: %s", error->message);
		return;
	}

	/* load as image */
	stream = g_memory_input_stream_new_from_data (svg_data->str,
						      (gssize) svg_data->len,
						      NULL);
	pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, -1, 600, TRUE, NULL, &error);
	if (pixbuf == NULL) {
		g_warning ("failed to load image: %s", error->message);
		return;
	}
	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "image_overview"));
	gtk_image_set_from_pixbuf (GTK_IMAGE (widget), pixbuf);
}

static gboolean
sbu_gui_node_notify_delay_cb (gpointer user_data)
{
	SbuGui *self = (SbuGui *) user_data;
	sbu_gui_refresh_overview (self);
	self->refresh_id = 0;
	return FALSE;
}

static void
sbu_gui_refresh_overview_delay (SbuGui *self)
{
	if (self->refresh_id != 0)
		g_source_remove (self->refresh_id);
	self->refresh_id = g_timeout_add (500, sbu_gui_node_notify_delay_cb, self);
}

static void
sbu_gui_node_notify_cb (SbuNode *n, GParamSpec *pspec, gpointer user_data)
{
	SbuGui *self = (SbuGui *) user_data;
	g_debug ("changed %s:%s",
		 sbu_node_kind_to_string (sbu_node_get_kind (n)),
		 g_param_spec_get_name (pspec));
	sbu_gui_refresh_overview_delay (self);
}

static void
sbu_gui_link_notify_cb (SbuLink *l, GParamSpec *pspec, gpointer user_data)
{
	SbuGui *self = (SbuGui *) user_data;
	g_debug ("changed %s->%s:%s",
		 sbu_node_kind_to_string (sbu_link_get_src (l)),
		 sbu_node_kind_to_string (sbu_link_get_dst (l)),
		 g_param_spec_get_name (pspec));
	sbu_gui_refresh_overview_delay (self);
}

static gboolean
sbu_gui_update_default_device (SbuGui *self, GError **error)
{
	g_auto(GStrv) devices = NULL;
	g_auto(GStrv) nodes = NULL;
	g_auto(GStrv) links = NULL;

	/* get default device */
	if (!sbu_manager_call_get_devices_sync (self->manager,
						&devices,
						self->cancellable,
						error))
		return FALSE;
	if (g_strv_length (devices) == 0) {
		g_set_error_literal (error, 1, 0, "no devices!");
		return FALSE;
	}

	/* just use the first device we find */
	g_debug ("using device: %s", devices[0]);
	self->device = sbu_device_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							  G_DBUS_PROXY_FLAGS_NONE,
							  SBU_DBUS_NAME,
							  devices[0],
							  self->cancellable,
							  error);
	if (self->device == NULL)
		return FALSE;

	/* show each node */
	if (!sbu_device_call_get_nodes_sync (self->device,
						&nodes,
						self->cancellable,
						error))
		return FALSE;
	for (guint i = 0; nodes[i] != NULL; i++) {
		g_autoptr(SbuNode) node = NULL;
		g_debug ("using node: %s", nodes[i]);
		node = sbu_node_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							G_DBUS_PROXY_FLAGS_NONE,
							SBU_DBUS_NAME,
							nodes[i],
							self->cancellable,
							error);
		if (node == NULL)
			return FALSE;
		g_signal_connect (node, "notify",
				  G_CALLBACK (sbu_gui_node_notify_cb),
				  self);
		g_ptr_array_add (self->nodes, g_object_ref (node));
	}

	/* show each link */
	if (!sbu_device_call_get_links_sync (self->device,
						&links,
						self->cancellable,
						error))
		return FALSE;
	for (guint i = 0; links[i] != NULL; i++) {
		g_autoptr(SbuLink) link = NULL;
		g_debug ("using link: %s", links[i]);
		link = sbu_link_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							G_DBUS_PROXY_FLAGS_NONE,
							SBU_DBUS_NAME,
							links[i],
							self->cancellable,
							error);
		if (link == NULL)
			return FALSE;
		g_signal_connect (link, "notify",
				  G_CALLBACK (sbu_gui_link_notify_cb),
				  self);
		g_ptr_array_add (self->links, g_object_ref (link));
	}

	/* initial load */
	return TRUE;
}

static void
sbu_gui_history_interval_changed_cb (GtkComboBox *combo_box, SbuGui *self)
{
	switch (gtk_combo_box_get_active (combo_box)) {
	case 0:
		self->history_interval = 60 * 60;
		break;
	case 1:
		self->history_interval = 24 * 60 * 60;
		break;
	case 2:
		self->history_interval = 7 * 24 * 60 * 60;
		break;
	case 3:
		self->history_interval = 30 * 24 * 60 * 60;
		break;
	default:
		g_assert_not_reached ();
	}
	sbu_gui_history_refresh_graph (self);
}

static gint
sbu_gui_listbox_details_sort_cb (GtkListBoxRow *row1,
				 GtkListBoxRow *row2,
				 gpointer user_data)
{
	g_autoptr(GList) box1 = gtk_container_get_children (GTK_CONTAINER (row1));
	g_autoptr(GList) box2 = gtk_container_get_children (GTK_CONTAINER (row2));
	g_autoptr(GList) labels1 = gtk_container_get_children (GTK_CONTAINER (box1->data));
	g_autoptr(GList) labels2 = gtk_container_get_children (GTK_CONTAINER (box2->data));
	return g_strcmp0 (gtk_label_get_text (GTK_LABEL (labels1->data)),
			  gtk_label_get_text (GTK_LABEL (labels2->data)));
}

static gboolean
sbu_gui_overview_button_press_cb (GtkWidget *widget, GdkEventButton *event, SbuGui *self)
{
	GdkRectangle rect;
	GtkAllocation widget_sz;
	GtkPositionType position = GTK_POS_LEFT;
	GtkWidget *grid;
	GtkWidget *popover;
	SbuNode *n = NULL;
	gdouble fract_x;
	gdouble fract_y;
	struct {
		SbuNodeKind	 node_kind;
		GtkPositionType	 position;
		gdouble		 center_x;
		gdouble		 center_y;
		gdouble		 size;
	} map[] = {
		{ SBU_NODE_KIND_BATTERY,	GTK_POS_TOP,	0.283f,	0.504f,	0.166 },
		{ SBU_NODE_KIND_SOLAR,		GTK_POS_BOTTOM,	0.129f,	0.197f,	0.166 },
		{ SBU_NODE_KIND_UTILITY,	GTK_POS_TOP,	0.130f,	0.810f,	0.166 },
		{ SBU_NODE_KIND_LOAD,		GTK_POS_BOTTOM,	0.795f,	0.413f,	0.328 },
		{ SBU_NODE_KIND_UNKNOWN,	GTK_POS_LEFT,	0.f,	0.f,	0.f }
	};
	struct {
		SbuDeviceProperty	 prop;
		const gchar		*title;
	} node_props[] = {
		{ SBU_DEVICE_PROPERTY_POWER,		_("Power") },
		{ SBU_DEVICE_PROPERTY_POWER_MAX,	_("Max Power") },
		{ SBU_DEVICE_PROPERTY_VOLTAGE,		_("Voltage") },
		{ SBU_DEVICE_PROPERTY_VOLTAGE_MAX,	_("Max Voltage") },
		{ SBU_DEVICE_PROPERTY_CURRENT,		_("Current") },
		{ SBU_DEVICE_PROPERTY_CURRENT_MAX,	_("Max Current") },
		{ SBU_DEVICE_PROPERTY_FREQUENCY,	_("Frequency") },
		{ SBU_DEVICE_PROPERTY_UNKNOWN,		NULL }
	};

	/* not a left button click */
	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;
	if (event->button != 1)
		return FALSE;

	/* work out in terms of fractional widget position */
	gtk_widget_get_allocation (widget, &widget_sz);
	fract_x = event->x / (gdouble) widget_sz.width;
	fract_y = event->y / (gdouble) widget_sz.height;
	g_debug ("got click @%f x %f", fract_x, fract_y);

	for (guint i = 0; map[i].node_kind != SBU_NODE_KIND_UNKNOWN; i++) {
		if (fract_x > map[i].center_x - (map[i].size / 2) &&
		    fract_x < map[i].center_x + (map[i].size / 2) &&
		    fract_y > map[i].center_y - (map[i].size / 2) &&
		    fract_y < map[i].center_y + (map[i].size / 2)) {
			g_debug ("got click for %s",
				 sbu_node_kind_to_string (map[i].node_kind));
			n = sbu_gui_get_node (self, map[i].node_kind);
			if (n == NULL)
				break;
			rect.x = map[i].center_x * widget_sz.width;
			rect.y = map[i].center_y * widget_sz.height;
			position = map[i].position;
			if (position == GTK_POS_TOP)
				rect.y -= (map[i].size / 2) * widget_sz.height;
			else if (position == GTK_POS_BOTTOM)
				rect.y += (map[i].size / 2) * widget_sz.height;
		}
	}

	/* nothing to show */
	if (n == NULL) {
		g_debug ("no node to show");
		return FALSE;
	}

	/* show popover */
	popover = gtk_popover_new (widget);
	gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
	gtk_popover_set_position (GTK_POPOVER (popover), position);
	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
	gtk_widget_set_margin_start (grid, 16);
	gtk_widget_set_margin_end (grid, 16);
	gtk_widget_set_margin_top (grid, 16);
	gtk_widget_set_margin_bottom (grid, 16);
	for (guint i = 0; node_props[i].prop != SBU_DEVICE_PROPERTY_UNKNOWN; i++) {
		GtkWidget *title;
		GtkWidget *value;
		GtkStyleContext *style_context;
		const gchar *key;
		gdouble val;
		g_autofree gchar *str = NULL;

		/* get value */
		key = sbu_device_property_to_string (node_props[i].prop);
		g_object_get (n, key, &val, NULL);
		if (fabs (val) < 0.01f)
			continue;

		/* add widgets to grid */
		str = sbu_format_for_display (val, sbu_device_property_to_unit (node_props[i].prop));
		title = gtk_label_new (node_props[i].title);
		style_context = gtk_widget_get_style_context (title);
		gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_DIM_LABEL);

		gtk_label_set_xalign (GTK_LABEL (title), 1.0);
		gtk_grid_attach (GTK_GRID (grid), title,
				 0, i, 1, 1);
		value = gtk_label_new (str);
		gtk_label_set_xalign (GTK_LABEL (value), 0.0);
		gtk_grid_attach (GTK_GRID (grid), value,
				 1, i, 1, 1);
	}
	gtk_widget_show_all (grid);
	gtk_container_add (GTK_CONTAINER (popover), grid);
	gtk_popover_popup (GTK_POPOVER (popover));
	return FALSE;
}

static void
sbu_gui_startup_cb (GApplication *application, SbuGui *self)
{
	GtkWidget *widget;
	GtkWindow *window;
	guint retval;
	g_autofree gchar *location = NULL;
	g_autoptr(GError) error = NULL;

	/* get UI */
	retval = gtk_builder_add_from_resource (self->builder,
						"/com/hughski/PowerSBU/sbu-gui.ui",
						&error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		return;
	}

	window = GTK_WINDOW (gtk_builder_get_object (self->builder, "dialog_main"));
	gtk_window_set_application (window, GTK_APPLICATION (application));
	gtk_window_set_default_size (window, 1200, 500);
	gtk_window_set_application (window, GTK_APPLICATION (application));
	gtk_window_set_default_icon_name ("battery-good-charging");

	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "dialog_main"));
	gtk_widget_show (widget);

	/* load database */
	location = sbu_config_get_string (self->config, "DatabaseLocation", &error);
	if (location == NULL) {
		g_warning ("failed to load config: %s", error->message);
		return;
	}
	sbu_database_set_location (self->database, location);
	if (!sbu_database_open (self->database, &error)) {
		g_warning ("failed to load database: %s", error->message);
		return;
	}

	/* find the device to use */
	self->manager = sbu_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
							    G_DBUS_PROXY_FLAGS_NONE,
							    SBU_DBUS_NAME,
							    SBU_DBUS_PATH_MANAGER,
							    self->cancellable,
							    &error);
	if (self->manager == NULL) {
		g_warning ("failed to connect to daemon: %s", error->message);
		return;
	}
	g_debug ("daemon version: %s", sbu_manager_get_version (self->manager));

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
			  G_CALLBACK (sbu_gui_history_row_selected_cb), self);

	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "listbox_details"));
	gtk_list_box_set_sort_func (GTK_LIST_BOX (widget),
				    sbu_gui_listbox_details_sort_cb,
				    self, NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "combobox_timespan"));
	g_signal_connect (widget, "changed",
			  G_CALLBACK (sbu_gui_history_interval_changed_cb), self);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);


	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "eventbox_overview"));
	gtk_widget_add_events (widget, GDK_BUTTON_PRESS_MASK);
	g_signal_connect (widget, "button-press-event",
			  G_CALLBACK (sbu_gui_overview_button_press_cb), self);

	/* get default device */
	if (!sbu_gui_update_default_device (self, &error)) {
		g_warning ("failed to get device: %s", error->message);
		/* FIXME: show custom page */
	}

	/* set up overview page */
	sbu_gui_refresh_overview (self);

	/* populate pages */
	sbu_gui_refresh_details (self);
	sbu_gui_refresh_history (self);
}

static SbuGui *
sbu_gui_self_new (void)
{
	SbuGui *self = g_new0 (SbuGui, 1);
	self->cancellable = g_cancellable_new ();
	self->config = sbu_config_new ();
	self->database = sbu_database_new ();
	self->builder = gtk_builder_new ();
	self->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	self->links = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	self->details_sizegroup_title = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	self->details_sizegroup_value = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	return self;
}

int
main (int argc, char *argv[])
{
	g_autoptr(GtkApplication) application = NULL;
	g_autoptr(SbuGui) self = sbu_gui_self_new ();

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* are we already activated? */
	application = gtk_application_new ("com.hughski.PowerSBU",
					   G_APPLICATION_HANDLES_COMMAND_LINE);
	g_signal_connect (application, "startup",
			  G_CALLBACK (sbu_gui_startup_cb), self);
	g_signal_connect (application, "command-line",
			  G_CALLBACK (sbu_gui_commandline_cb), self);

	/* run */
	return g_application_run (G_APPLICATION (application), argc, argv);
}
