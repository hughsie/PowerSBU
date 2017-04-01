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

#include <glib/gstdio.h>
#include <glib-object.h>
#include <math.h>

#include "sbu-common.h"
#include "sbu-database.h"
#include "sbu-xml-modifier.h"

static void
sbu_test_database_func (void)
{
	gboolean ret;
	gint ts;
	g_autofree gchar *location = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GHashTable) latest = NULL;
	g_autoptr(GPtrArray) array1 = NULL;
	g_autoptr(GPtrArray) array2 = NULL;
	g_autoptr(SbuDatabase) db = NULL;

	location = g_build_filename ("/tmp", "sbu-self-test", "raw.db", NULL);
	g_unlink (location);

	db = sbu_database_new ();
	sbu_database_set_location (db, location);
	ret = sbu_database_open (db, &error);
	g_assert_no_error (error);
	g_assert (ret);

	sbu_database_save_value (db, "GridFrequency", 50000, NULL);
	sbu_database_save_value (db, "GridFrequency", 50000, NULL);
	sbu_database_save_value (db, "GridFrequency", 50001, NULL);
	sbu_database_save_value (db, "GridFrequency", 51000, NULL);
	sbu_database_save_value (db, "GridFrequency", 52000, NULL);
	sbu_database_save_value (db, "AcOutputVoltage", 230000, NULL);

	/* query what we just put in */
	ts = g_get_real_time () / G_USEC_PER_SEC;
	array1 = sbu_database_query (db, "GridFrequency", SBU_DEVICE_ID_DEFAULT, 0, ts, &error);
	g_assert_no_error (error);
	g_assert (array1 != NULL);
	g_assert_cmpint (array1->len, ==, 3);
	g_assert_cmpint (((SbuDatabaseItem *) g_ptr_array_index (array1, 0))->val, ==, 50000);
	g_assert_cmpint (((SbuDatabaseItem *) g_ptr_array_index (array1, 1))->val, ==, 51000);
	g_assert_cmpint (((SbuDatabaseItem *) g_ptr_array_index (array1, 2))->val, ==, 52000);

	/* query unknown key */
	array2 = sbu_database_query (db, "SomeThingElse", SBU_DEVICE_ID_DEFAULT, 0, ts, &error);
	g_assert_no_error (error);
	g_assert (array2 != NULL);
	g_assert_cmpint (array2->len, ==, 0);

	/* close, and reload */
	g_debug ("loading again...");
	g_object_unref (db);
	db = sbu_database_new ();
	sbu_database_set_location (db, location);
	ret = sbu_database_open (db, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get a dictionary of all the keys and last set values */
	latest = sbu_database_get_latest (db, SBU_DEVICE_ID_DEFAULT, &error);
	g_assert_no_error (error);
	g_assert (latest != NULL);
	g_assert_cmpint (((SbuDatabaseItem *) g_hash_table_lookup (latest, "GridFrequency"))->val, ==, 52000);
	g_assert_cmpint (((SbuDatabaseItem *) g_hash_table_lookup (latest, "AcOutputVoltage"))->val, ==, 230000);
	g_assert (g_hash_table_lookup (latest, "SomeThingElse") == NULL);

	/* cleanup */
	g_unlink (location);
}

static void
sbu_test_xml_modifier_func (void)
{
	g_autoptr(SbuXmlModifier) xml_mod = sbu_xml_modifier_new ();
	g_autoptr(GString) str = NULL;
	g_autoptr(GError) error = NULL;
	const gchar *xml_in =
		"<body>"
		"<person id=\"name\" age=\"18\">Unknown</person>"
		"</body>";
	const gchar *xml_out =
		"<body>"
		"<person id=\"name\" age=\"33\">Richard</person>"
		"</body>";
	sbu_xml_modifier_replace_cdata (xml_mod, "name", "Richard");
	sbu_xml_modifier_replace_attr (xml_mod, "name", "age", "33");
	str = sbu_xml_modifier_process (xml_mod, xml_in, -1, &error);
	g_assert_no_error (error);
	g_assert (str != NULL);
	g_assert_cmpstr (str->str, ==, xml_out);
}


static void
sbu_test_common_func (void)
{
	struct {
		gdouble		 val;
		const gchar	*str;
	} data[] = {
		{ 0.f,		"0W" },
		{ 0.12f,	"0.1W" },
		{ 9.f,		"9W" },
		{ 99.f,		"99W" },
		{ 99.9f,	"99.9W" },
		{ 999.f,	"999W" },
		{ 1200.f,	"1.2kW" },
		{ 1234.f,	"1.2kW" },
		{ -1234.f,	"-1.2kW" },
		{ 0.0f,		NULL }
	};

	for (guint i = 0; i < SBU_NODE_KIND_LAST; i++)
		g_assert_cmpstr (sbu_node_kind_to_string (i), !=, NULL);
	for (guint i= 0; data[i].str != NULL; i++) {
		g_autofree gchar *tmp = sbu_format_for_display (data[i].val, "W");
		g_assert_cmpstr (tmp, ==, data[i].str);
	}
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* tests go here */
	g_test_add_func ("/database", sbu_test_database_func);
	g_test_add_func ("/common", sbu_test_common_func);
	g_test_add_func ("/xml-modifier", sbu_test_xml_modifier_func);

	return g_test_run ();
}

