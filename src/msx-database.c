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

#include <errno.h>
#include <gio/gio.h>
#include <sqlite3.h>
#include <math.h>

#include "msx-database.h"

struct _MsxDatabase
{
	GObject			 parent_instance;

	GHashTable		*hash;
	gchar			*location;
	sqlite3			*db;
};

#define MSX_DATABASE_VALUE_DELTA	0.5f

G_DEFINE_TYPE (MsxDatabase, msx_database, G_TYPE_OBJECT)

void
msx_database_set_location (MsxDatabase *self, const gchar *location)
{
	g_free (self->location);
	self->location = g_strdup (location);
}

static gboolean
msx_database_ensure_file_directory (const gchar *path, GError **error)
{
	g_autofree gchar *parent = NULL;
	parent = g_path_get_dirname (path);
	if (g_mkdir_with_parents (parent, 0755) == -1) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     g_strerror (errno));
		return FALSE;
	}
	return TRUE;
}

static gboolean
msx_database_execute (MsxDatabase *self,
			   const gchar *statement,
			   GError **error)
{
	gint rc = sqlite3_exec (self->db, statement, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Failed to execute statement '%s': %s",
			     statement,
			     sqlite3_errmsg (self->db));
		return FALSE;
	}
	return TRUE;
}

typedef struct {
	gint64		 ts;
	guint		 dev;
	gchar		*key;
	gint		 val;
} MsxDatabaseResult;

static gint
msx_database_result_cb (void *data, gint argc, gchar **argv, gchar **col_name)
{
	GPtrArray *results = (GPtrArray *) data;
	MsxDatabaseResult *res = g_new0 (MsxDatabaseResult, 1);
	res->ts = g_ascii_strtoll (argv[0], NULL, 10);
	res->dev = g_ascii_strtoll (argv[1], NULL, 10);
	res->key = g_strdup (argv[2]);
	res->val = g_ascii_strtoll (argv[3], NULL, 10);
	g_ptr_array_add (results, res);
	return 0;
}

static gint
msx_database_item_cb (void *data, gint argc, gchar **argv, gchar **col_name)
{
	GPtrArray *items = (GPtrArray *) data;
	MsxDatabaseItem *item = g_new0 (MsxDatabaseItem, 1);
	item->ts = g_ascii_strtoll (argv[0], NULL, 10);
	item->val = g_ascii_strtoll (argv[1], NULL, 10);
	g_ptr_array_add (items, item);
	return 0;
}

static void
msx_database_result_free (MsxDatabaseResult *result)
{
	g_free (result->key);
	g_free (result);
}

static MsxDatabaseItem *
msx_database_add_item_to_cache (MsxDatabase *self, const gchar *key, gint val)
{
	MsxDatabaseItem *item = g_new0 (MsxDatabaseItem, 1);
	item->ts = g_get_real_time () / G_USEC_PER_SEC;
	item->val = val;
	g_debug ("adding %s=%i to the cache", key, val);
	g_hash_table_insert (self->hash, g_strdup (key), item);
	return item;
}

gboolean
msx_database_open (MsxDatabase *self, GError **error)
{
	const gchar *statement;
	gchar *error_msg = NULL;
	gint rc;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GPtrArray) results = NULL;

	/* sanity check */
	if (self->db != NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "database already open");
		return FALSE;
	}

	/* ensure location is set */
	if (self->location == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "no location specified");
		return FALSE;
	}

	/* ensure parent dirs exist */
	if (!msx_database_ensure_file_directory (self->location, error)) {
		g_prefix_error (error,
				"failed to create directory for %s: ",
				self->location);
		return FALSE;
	}

	/* open database */
	rc = sqlite3_open (self->location, &self->db);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "can't open transaction database: %s",
			     sqlite3_errmsg (self->db));
		sqlite3_close (self->db);
		self->db = NULL;
		return FALSE;
	}

	/* we don't need to keep doing fsync */
	if (!msx_database_execute (self, "PRAGMA synchronous=OFF", error))
		return FALSE;

	/* check transactions */
	if (!msx_database_execute (self, "SELECT * FROM log LIMIT 1", &error_local)) {
		g_debug ("creating table to repair: %s", error_local->message);
		g_clear_error (&error_local);
		statement = "CREATE TABLE log ("
			    "id INTEGER PRIMARY KEY,"
			    "dev INTEGER DEFAULT 0,"
			    "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
			    "key STRING DEFAULT NULL,"
			    "val INTEGER);";
		if (!msx_database_execute (self, statement, error))
			return FALSE;
	}

	/* load existing values */
	results = g_ptr_array_new_with_free_func ((GDestroyNotify) msx_database_result_free);
	statement = "SELECT ts, dev, key, val FROM log GROUP BY key ORDER BY ts ASC;";
	rc = sqlite3_exec (self->db, statement, msx_database_result_cb, results, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "SQL error: %s", error_msg);
		sqlite3_free (error_msg);
		return FALSE;
	}
	for (guint i = 0; i < results->len; i++) {
		MsxDatabaseResult *res = g_ptr_array_index (results, i);
		msx_database_add_item_to_cache (self, res->key, res->val);
	}

	/* success */
	g_debug ("database open and ready for action!");
	return TRUE;
}

static gdouble
msx_database_compare_values (gint val_new, gint val_old)
{
	gdouble pc = 100.f - (((gdouble) val_new * 100.f) / (gdouble) val_old);
	return fabs (pc);
}

gboolean
msx_database_save_value (MsxDatabase *self, const gchar *key, gint val, GError **error)
{
	MsxDatabaseItem *item;
	g_autofree gchar *statement = NULL;

	/* sanity check */
	if (self->db == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "database is not open");
		return FALSE;
	}

	/* get existing cached value */
	item = g_hash_table_lookup (self->hash, key);
	if (item != NULL) {
		gdouble tmp;
		if (item->val == val) {
			g_debug ("same value for %s=%i, ignoring", key, val);
			return TRUE;
		}
		tmp = msx_database_compare_values (val, item->val);
		if (tmp < MSX_DATABASE_VALUE_DELTA) {
			g_debug ("within %.2f%% of value for %s=%i->%i, ignoring",
				 tmp, key, item->val, val);
			return TRUE;
		}
		g_debug ("replacing existing %s=%i", key, val);
	} else {
		g_debug ("no stored value, saving %s=%i", key, val);
	}

	/* save to cache and database */
	item = msx_database_add_item_to_cache (self, key, val);
	statement = g_strdup_printf ("INSERT INTO log (ts, key, val) "
				     "VALUES ('%" G_GINT64_FORMAT "', '%s', '%i')",
				     item->ts, key, item->val);
	if (!msx_database_execute (self, statement, error))
		return FALSE;

	/* success */
	return TRUE;
}

GPtrArray *
msx_database_query (MsxDatabase *self, const gchar *key, guint dev,
		    gint64 ts_start, gint64 ts_end, GError **error)
{
	g_autoptr(GPtrArray) results = g_ptr_array_new_with_free_func (g_free);
	gchar *error_msg = NULL;
	gint rc;
	g_autofree gchar *statement = NULL;

	statement = g_strdup_printf ("SELECT ts, val FROM log "
				     "WHERE key = '%s' "
				     "AND dev = %u "
				     "AND ts >= %" G_GINT64_FORMAT " "
				     "AND ts <= %" G_GINT64_FORMAT " "
				     "ORDER BY id ASC;",
				     key, dev, ts_start, ts_end);
	rc = sqlite3_exec (self->db, statement, msx_database_item_cb, results, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "SQL error: %s", error_msg);
		sqlite3_free (error_msg);
		return NULL;
	}

	/* success */
	return g_steal_pointer (&results);
}

static void
msx_database_finalize (GObject *object)
{
	MsxDatabase *self = MSX_DATABASE (object);

	if (self->db != NULL)
		sqlite3_close (self->db);
	g_free (self->location);
	g_hash_table_unref (self->hash);

	G_OBJECT_CLASS (msx_database_parent_class)->finalize (object);
}

static void
msx_database_init (MsxDatabase *self)
{
	self->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
msx_database_class_init (MsxDatabaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = msx_database_finalize;
}

/**
 * msx_database_new:
 *
 * Return value: a new MsxDatabase object.
 **/
MsxDatabase *
msx_database_new (void)
{
	MsxDatabase *self;
	self = g_object_new (MSX_TYPE_DATABASE, NULL);
	return MSX_DATABASE (self);
}
