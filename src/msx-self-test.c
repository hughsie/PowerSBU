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

#include "msx-common.h"
#include "msx-device.h"
#include "msx-database.h"

static void
msx_test_common_func (void)
{
	const gchar *buf = "12.34 - 45.67 89.12 0";
	const gchar *raw = "230.0 13.0 230.0 50.0 13.0 3000 2400";
	gchar buf2[5];

	g_assert_cmpint (msx_common_parse_int ("123.456", 0, -1, NULL), ==, 123456);
	g_assert_cmpint (msx_common_parse_int ("123.4", 0, -1, NULL), ==, 123400);
	g_assert_cmpint (msx_common_parse_int ("123", 0, -1, NULL), ==, 123000);
	g_assert_cmpint (msx_common_parse_int ("30000", 0, -1, NULL), ==, 30000000);
	g_assert_cmpint (msx_common_parse_int ("000.000", 0, -1, NULL), ==, 0);
	g_assert_cmpint (msx_common_parse_int ("0", 0, -1, NULL), ==, 0);
	g_assert_cmpint (msx_common_parse_int (buf + 8, 0, -1, NULL), ==, 45670);
	g_assert_cmpint (msx_common_parse_int (NULL, 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("deadbeef", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("123zzz", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("123.456zzz", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("123.456.789", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("100000", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("123.10000", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int ("3000.1", 0, -1, NULL), ==, G_MAXINT);
	g_assert_cmpint (msx_common_parse_int (buf, 6, -1, NULL), ==, 0);
	g_assert_cmpint (msx_common_parse_int (buf, 20, -1, NULL), ==, 0);

	/* not NUL terminated */
	buf2[0] = '1';
	buf2[1] = '2';
	buf2[2] = '.';
	buf2[3] = '3';
	buf2[4] = '4';
	g_assert_cmpint (msx_common_parse_int (buf2, 0, 2, NULL), ==, 12000);
	g_assert_cmpint (msx_common_parse_int (buf2, 0, 5, NULL), ==, 12340);

	/* real world example */
	g_assert_cmpint (msx_common_parse_int (raw, 0x1b, -1, NULL), ==, 3000000);
}

static void
msx_test_database_func (void)
{
	gboolean ret;
	gint ts;
	g_autofree gchar *location = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) array1 = NULL;
	g_autoptr(GPtrArray) array2 = NULL;
	g_autoptr(MsxDatabase) db = NULL;

	location = g_build_filename ("/tmp", "msx-self-test", "raw.db", NULL);
	g_unlink (location);

	db = msx_database_new ();
	msx_database_set_location (db, location);
	ret = msx_database_open (db, &error);
	g_assert_no_error (error);
	g_assert (ret);

	msx_database_save_value (db, "GridFrequency", 50000, NULL);
	msx_database_save_value (db, "GridFrequency", 50000, NULL);
	msx_database_save_value (db, "GridFrequency", 50001, NULL);
	msx_database_save_value (db, "GridFrequency", 51000, NULL);
	msx_database_save_value (db, "GridFrequency", 52000, NULL);
	msx_database_save_value (db, "AcOutputVoltage", 230000, NULL);

	/* query what we just put in */
	ts = g_get_real_time () / G_USEC_PER_SEC;
	array1 = msx_database_query (db, "GridFrequency", MSX_DEVICE_ID_DEFAULT, 0, ts, &error);
	g_assert_no_error (error);
	g_assert (array1 != NULL);
	g_assert_cmpint (array1->len, ==, 3);
	g_assert_cmpint (((MsxDatabaseItem *) g_ptr_array_index (array1, 0))->val, ==, 50000);
	g_assert_cmpint (((MsxDatabaseItem *) g_ptr_array_index (array1, 1))->val, ==, 51000);
	g_assert_cmpint (((MsxDatabaseItem *) g_ptr_array_index (array1, 2))->val, ==, 52000);

	/* query unknown key */
	array2 = msx_database_query (db, "SomeThingElse", MSX_DEVICE_ID_DEFAULT, 0, ts, &error);
	g_assert_no_error (error);
	g_assert (array2 != NULL);
	g_assert_cmpint (array2->len, ==, 0);

	/* close, and reload */
	g_debug ("loading again...");
	g_object_unref (db);
	db = msx_database_new ();
	msx_database_set_location (db, location);
	ret = msx_database_open (db, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* cleanup */
	g_unlink (location);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* tests go here */
	g_test_add_func ("/common", msx_test_common_func);
	g_test_add_func ("/database", msx_test_database_func);

	return g_test_run ();
}

