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

#include "msx-device.h"
#include "msx-database.h"

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
	g_test_add_func ("/database", msx_test_database_func);

	return g_test_run ();
}

