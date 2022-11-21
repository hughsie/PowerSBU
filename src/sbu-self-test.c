/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gstdio.h>
#include <math.h>

#include "sbu-common.h"
#include "sbu-database.h"
#include "sbu-msx-common.h"
#include "sbu-msx-device.h"

static void
sbu_msx_test_common_func(void)
{
	const gchar *buf = "12.34 - 45.67 89.12 0";
	const gchar *raw = "230.0 13.0 230.0 50.0 13.0 3000 2400";
	gchar buf2[5];

	g_assert_cmpint(sbu_msx_common_parse_int("123.456", 0, -1, NULL), ==, 123456);
	g_assert_cmpint(sbu_msx_common_parse_int("123.4", 0, -1, NULL), ==, 123400);
	g_assert_cmpint(sbu_msx_common_parse_int("123", 0, -1, NULL), ==, 123000);
	g_assert_cmpint(sbu_msx_common_parse_int("30000", 0, -1, NULL), ==, 30000000);
	g_assert_cmpint(sbu_msx_common_parse_int("000.000", 0, -1, NULL), ==, 0);
	g_assert_cmpint(sbu_msx_common_parse_int("0", 0, -1, NULL), ==, 0);
	g_assert_cmpint(sbu_msx_common_parse_int(buf + 8, 0, -1, NULL), ==, 45670);
	g_assert_cmpint(sbu_msx_common_parse_int(NULL, 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("deadbeef", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("123zzz", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("123.456zzz", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("123.456.789", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("100000", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("123.10000", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int("3000.1", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint(sbu_msx_common_parse_int(buf, 6, -1, NULL), ==, 0);
	g_assert_cmpint(sbu_msx_common_parse_int(buf, 20, -1, NULL), ==, 0);

	/* not NUL terminated */
	buf2[0] = '1';
	buf2[1] = '2';
	buf2[2] = '.';
	buf2[3] = '3';
	buf2[4] = '4';
	g_assert_cmpint(sbu_msx_common_parse_int(buf2, 0, 2, NULL), ==, 12000);
	g_assert_cmpint(sbu_msx_common_parse_int(buf2, 0, 5, NULL), ==, 12340);

	/* real world example */
	g_assert_cmpint(sbu_msx_common_parse_int(raw, 0x1b, -1, NULL), ==, 3000000);

	/* test enum to string */
	for (guint i = 1; i < SBU_MSX_DEVICE_KEY_LAST; i++)
		g_assert_cmpstr(sbu_device_key_to_string(i), !=, NULL);
}

static void
sbu_test_database_func(void)
{
	SbuDatabaseItem *item;
	gboolean ret;
	gint ts;
	g_autofree gchar *location = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) latest = NULL;
	g_autoptr(GPtrArray) array1 = NULL;
	g_autoptr(GPtrArray) array2 = NULL;
	g_autoptr(SbuDatabase) db = NULL;

	location = g_build_filename("/tmp", "sbu-self-test", "raw.db", NULL);
	g_unlink(location);

	db = sbu_database_new();
	sbu_database_set_location(db, location);
	ret = sbu_database_open(db, &error);
	g_assert_no_error(error);
	g_assert(ret);

	sbu_database_save_value(db, "device-id", "GridFrequency", 50000, NULL);
	g_usleep(G_USEC_PER_SEC);
	sbu_database_save_value(db, "device-id", "GridFrequency", 52000, NULL);
	g_usleep(G_USEC_PER_SEC);
	sbu_database_save_value(db, "device-id", "AcOutputVoltage", 230000, NULL);

	/* query what we just put in */
	ts = g_get_real_time() / G_USEC_PER_SEC;
	array1 = sbu_database_query(db, "device-id", "GridFrequency", 0, ts, &error);
	g_assert_no_error(error);
	g_assert(array1 != NULL);
	g_assert_cmpint(array1->len, ==, 2);
	g_assert_cmpint(((SbuDatabaseItem *)g_ptr_array_index(array1, 0))->val, ==, 50000);
	g_assert_cmpint(((SbuDatabaseItem *)g_ptr_array_index(array1, 1))->val, ==, 52000);

	/* query unknown key */
	array2 = sbu_database_query(db, "device-id", "SomeThingElse", 0, ts, &error);
	g_assert_no_error(error);
	g_assert(array2 != NULL);
	g_assert_cmpint(array2->len, ==, 0);

	/* close, and reload */
	g_debug("loading again...");
	g_object_unref(db);
	db = sbu_database_new();
	sbu_database_set_location(db, location);
	ret = sbu_database_open(db, &error);
	g_assert_no_error(error);
	g_assert(ret);

	/* get a dictionary of all the keys and last set values */
	latest = sbu_database_get_latest(db, "device-id", 5, &error);
	g_assert_no_error(error);
	g_assert(latest != NULL);
	item = g_ptr_array_index(latest, 0);
	g_assert_cmpstr(item->key, ==, "AcOutputVoltage");
	g_assert_cmpint(item->val, ==, 230000);
	item = g_ptr_array_index(latest, 1);
	g_assert_cmpstr(item->key, ==, "GridFrequency");
	g_assert_cmpint(item->val, ==, 52000);

	/* cleanup */
	g_unlink(location);
}

static void
sbu_test_common_func(void)
{
	struct {
		gdouble val;
		const gchar *str;
	} data[] = {{0.f, "0W"},
		    {0.12f, "0.1W"},
		    {9.f, "9W"},
		    {99.f, "99W"},
		    {99.9f, "99.9W"},
		    {999.f, "999W"},
		    {1200.f, "1.2kW"},
		    {1234.f, "1.2kW"},
		    {-1234.f, "-1.2kW"},
		    {0.0f, NULL}};

	for (guint i = 0; i < SBU_NODE_KIND_LAST; i++)
		g_assert_cmpstr(sbu_node_kind_to_string(i), !=, NULL);
	for (guint i = 0; i < SBU_DEVICE_PROPERTY_LAST; i++)
		g_assert_cmpstr(sbu_device_property_to_string(i), !=, NULL);
	for (guint i = 0; data[i].str != NULL; i++) {
		g_autofree gchar *tmp = sbu_format_for_display(data[i].val, "W");
		g_assert_cmpstr(tmp, ==, data[i].str);
	}
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);

	g_setenv("G_MESSAGES_DEBUG", "all", TRUE);

	/* tests go here */
	g_test_add_func("/database", sbu_test_database_func);
	g_test_add_func("/common", sbu_test_common_func);
	g_test_add_func("/msx", sbu_msx_test_common_func);

	return g_test_run();
}
