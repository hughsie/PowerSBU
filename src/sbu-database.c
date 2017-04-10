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

#include "sbu-database.h"

struct _SbuDatabase
{
	GObject			 parent_instance;

	GHashTable		*hash;
	gchar			*location;
	sqlite3			*db;
};

#define SBU_DATABASE_VALUE_DELTA	0.5f
#define SBU_DATABASE_SAVE_INTERVAL	600

G_DEFINE_TYPE (SbuDatabase, sbu_database, G_TYPE_OBJECT)

void
sbu_database_set_location (SbuDatabase *self, const gchar *location)
{
	g_free (self->location);
	self->location = g_strdup (location);
}

static gboolean
sbu_database_ensure_file_directory (const gchar *path, GError **error)
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
sbu_database_execute (SbuDatabase *self,
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

static gint
sbu_database_result_cb (void *data, gint argc, gchar **argv, gchar **col_name)
{
	GHashTable *results = (GHashTable *) data;
	SbuDatabaseItem *item = g_new0 (SbuDatabaseItem, 1);
	item->ts = g_ascii_strtoll (argv[0], NULL, 10);
	item->val = g_ascii_strtoll (argv[2], NULL, 10);
	g_hash_table_insert (results, g_strdup (argv[1]), item);
	return 0;
}

static gint
sbu_database_item_cb (void *data, gint argc, gchar **argv, gchar **col_name)
{
	GPtrArray *items = (GPtrArray *) data;
	SbuDatabaseItem *item = g_new0 (SbuDatabaseItem, 1);
	item->ts = g_ascii_strtoll (argv[0], NULL, 10);
	item->val = g_ascii_strtoll (argv[1], NULL, 10);
	g_ptr_array_add (items, item);
	return 0;
}

static SbuDatabaseItem *
sbu_database_add_item_to_cache (SbuDatabase *self, const gchar *key, gint val)
{
	SbuDatabaseItem *item = g_new0 (SbuDatabaseItem, 1);
	item->ts = g_get_real_time () / G_USEC_PER_SEC;
	item->val = val;
	g_debug ("adding %s=%i to the cache", key, val);
	g_hash_table_insert (self->hash, g_strdup (key), item);
	return item;
}

gboolean
sbu_database_repair (SbuDatabase *self, GError **error)
{
	const gchar *statement;

	/* delete any negative values */
	statement = "DELETE FROM log WHERE val < 0;";
	if (!sbu_database_execute (self, statement, error))
		return FALSE;
	statement = "DELETE FROM log WHERE key == 'TestKey';";
	if (!sbu_database_execute (self, statement, error))
		return FALSE;

	return TRUE;
}

gboolean
sbu_database_open (SbuDatabase *self, GError **error)
{
	const gchar *statement;
	gint rc;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GHashTable) results = NULL;
	g_autoptr(GList) keys = NULL;

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
	if (!sbu_database_ensure_file_directory (self->location, error)) {
		g_prefix_error (error,
				"failed to create directory for %s: ",
				self->location);
		return FALSE;
	}

	/* open database */
	g_debug ("loading %s", self->location);
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

	/* check transactions */
	if (!sbu_database_execute (self, "SELECT * FROM log LIMIT 1", &error_local)) {
		g_debug ("creating table to repair: %s", error_local->message);
		g_clear_error (&error_local);
		statement = "CREATE TABLE log ("
			    "id INTEGER PRIMARY KEY,"
			    "dev INTEGER DEFAULT 0,"
			    "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
			    "key STRING DEFAULT NULL,"
			    "val INTEGER);";
		if (!sbu_database_execute (self, statement, error))
			return FALSE;
	}

	/* load existing values */
	results = sbu_database_get_latest (self, SBU_DEVICE_ID_DEFAULT, error);
	if (results == NULL)
		return FALSE;
	keys = g_hash_table_get_keys (results);
	for (GList *l = keys; l != NULL; l = l->next) {
		const gchar *key = l->data;
		SbuDatabaseItem *item = g_hash_table_lookup (results, key);
		sbu_database_add_item_to_cache (self, key, item->val);
	}

	/* success */
	g_debug ("database open and ready for action!");
	return TRUE;
}

GHashTable *
sbu_database_get_latest (SbuDatabase *self, guint dev, GError **error)
{
	gchar *error_msg = NULL;
	gint rc;
	g_autofree gchar *statement = NULL;
	g_autoptr(GHashTable) results = NULL;

	/* sanity check */
	if (self->db == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "database is not open");
		return NULL;
	}

	/* query */
	results = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	statement = g_strdup_printf ("SELECT ts, key, val FROM log "
				     "WHERE dev = %u GROUP BY key "
				     "ORDER BY ts ASC;", dev);
	rc = sqlite3_exec (self->db, statement, sbu_database_result_cb, results, &error_msg);
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

static gdouble
sbu_database_compare_values (gint val_new, gint val_old)
{
	gdouble pc = 100.f - (((gdouble) val_new * 100.f) / (gdouble) val_old);
	return fabs (pc);
}

gboolean
sbu_database_save_value (SbuDatabase *self, const gchar *key, gint val, GError **error)
{
	SbuDatabaseItem *item;
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
		guint64 ts_now = g_get_real_time () / G_USEC_PER_SEC;
		if (ts_now - item->ts > SBU_DATABASE_SAVE_INTERVAL) {
			g_debug ("replacing existing old value %s=%i", key, val);
		} else {
			gdouble tmp;
			if (item->val == val) {
				g_debug ("same value for %s=%i, ignoring", key, val);
				return TRUE;
			}
			tmp = sbu_database_compare_values (val, item->val);
			if (tmp < SBU_DATABASE_VALUE_DELTA) {
				g_debug ("within %.2f%% of value for %s=%i->%i, ignoring",
					 tmp, key, item->val, val);
				return TRUE;
			}
			g_debug ("replacing existing %s=%i", key, val);
		}
	} else {
		g_debug ("no stored value, saving %s=%i", key, val);
	}

	/* save to cache and database */
	item = sbu_database_add_item_to_cache (self, key, val);
	statement = g_strdup_printf ("INSERT INTO log (ts, key, val) "
				     "VALUES ('%" G_GINT64_FORMAT "', '%s', '%i')",
				     item->ts, key, item->val);
	if (!sbu_database_execute (self, statement, error))
		return FALSE;

	/* success */
	return TRUE;
}

GPtrArray *
sbu_database_query (SbuDatabase *self, const gchar *key, guint dev,
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
				     "ORDER BY ts ASC;",
				     key, dev, ts_start, ts_end);
	rc = sqlite3_exec (self->db, statement, sbu_database_item_cb, results, &error_msg);
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
sbu_database_finalize (GObject *object)
{
	SbuDatabase *self = SBU_DATABASE (object);

	if (self->db != NULL)
		sqlite3_close (self->db);
	g_free (self->location);
	g_hash_table_unref (self->hash);

	G_OBJECT_CLASS (sbu_database_parent_class)->finalize (object);
}

static void
sbu_database_init (SbuDatabase *self)
{
	self->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
sbu_database_class_init (SbuDatabaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = sbu_database_finalize;
}

/**
 * sbu_database_new:
 *
 * Return value: a new SbuDatabase object.
 **/
SbuDatabase *
sbu_database_new (void)
{
	SbuDatabase *self;
	self = g_object_new (SBU_TYPE_DATABASE, NULL);
	return SBU_DATABASE (self);
}
