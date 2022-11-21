/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "config.h"

#include <gio/gio.h>
#include <sqlite3.h>

#include "sbu-database.h"

struct _SbuDatabase {
	GObject parent_instance;
	gchar *location;
	sqlite3 *db;
};

G_DEFINE_TYPE(SbuDatabase, sbu_database, G_TYPE_OBJECT)

void
sbu_database_set_location(SbuDatabase *self, const gchar *location)
{
	g_free(self->location);
	self->location = g_strdup(location);
}

static gboolean
sbu_database_ensure_file_directory(const gchar *path, GError **error)
{
	g_autofree gchar *parent = NULL;
	parent = g_path_get_dirname(path);
	if (g_mkdir_with_parents(parent, 0755) == -1) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, g_strerror(errno));
		return FALSE;
	}
	return TRUE;
}

static gboolean
sbu_database_execute(SbuDatabase *self, const gchar *statement, GError **error)
{
	gint rc = sqlite3_exec(self->db, statement, NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_FAILED,
			    "Failed to execute statement '%s': %s",
			    statement,
			    sqlite3_errmsg(self->db));
		return FALSE;
	}
	return TRUE;
}

static gint
sbu_database_item_cb(void *data, gint argc, gchar **argv, gchar **col_name)
{
	GPtrArray *items = (GPtrArray *)data;
	SbuDatabaseItem *item = g_new0(SbuDatabaseItem, 1);
	item->ts = g_ascii_strtoll(argv[0], NULL, 10);
	item->val = g_ascii_strtoll(argv[1], NULL, 10);
	if (argc > 2)
		item->key = g_strdup(argv[2]);
	g_ptr_array_add(items, item);
	return 0;
}

gboolean
sbu_database_repair(SbuDatabase *self, GError **error)
{
	const gchar *statement;
	const gchar *keys_delete[] = {"MaximumPowerPercentage",
				      "AcOutputActivePower",
				      "BusVoltage",
				      "PvChargingPower",
				      NULL};

	/* delete ignored keys */
	for (guint i = 0; keys_delete[i] != NULL; i++) {
		g_autofree gchar *stmt = NULL;
		stmt = g_strdup_printf("DELETE FROM log "
				       "WHERE key == '%s';",
				       keys_delete[i]);
		if (!sbu_database_execute(self, stmt, error))
			return FALSE;
	}

	/* rename ported keys */
	statement = "UPDATE log SET key = 'node_utility:voltage' WHERE key == 'GridVoltage';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;
	statement = "UPDATE log SET key = 'node_load:voltage' WHERE key == 'AcOutputVoltage';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;
	statement = "UPDATE log SET key = 'node_battery:voltage' WHERE key == 'BatteryVoltage';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;
	statement = "UPDATE log SET key = 'node_battery:current' WHERE key == "
		    "'BatteryDischargeCurrent';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;
	statement =
	    "UPDATE log SET key = 'node_solar:voltage' WHERE key == 'BatteryVoltageFromScc';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;
	statement =
	    "UPDATE log SET key = 'node_solar:current' WHERE key == 'PvInputCurrentForBattery';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;
	statement = "UPDATE log SET key = 'node_utility:frequency' WHERE key == 'GridFrequency';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;
	statement = "UPDATE log SET key = 'node_load:frequency' WHERE key == 'AcOutputFrequency';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;

	/* flip around the new key value */
	statement = "UPDATE log SET key = 'node_battery:current', val = -val "
		    "WHERE key == 'BatteryCurrent';";
	if (!sbu_database_execute(self, statement, error))
		return FALSE;

	return TRUE;
}

gboolean
sbu_database_open(SbuDatabase *self, GError **error)
{
	const gchar *statement;
	gint rc;
	g_autoptr(GError) error_local = NULL;

	/* sanity check */
	if (self->db != NULL) {
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "database already open");
		return FALSE;
	}

	/* ensure location is set */
	if (self->location == NULL) {
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "no location specified");
		return FALSE;
	}

	/* ensure parent dirs exist */
	if (!sbu_database_ensure_file_directory(self->location, error)) {
		g_prefix_error(error, "failed to create directory for %s: ", self->location);
		return FALSE;
	}

	/* open database */
	g_debug("loading %s", self->location);
	rc = sqlite3_open(self->location, &self->db);
	if (rc != SQLITE_OK) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_FAILED,
			    "can't open transaction database: %s",
			    sqlite3_errmsg(self->db));
		sqlite3_close(self->db);
		self->db = NULL;
		return FALSE;
	}

	/* check transactions */
	if (!sbu_database_execute(self, "SELECT * FROM log LIMIT 1", &error_local)) {
		g_debug("creating table to repair: %s", error_local->message);
		g_clear_error(&error_local);
		statement = "CREATE TABLE log ("
			    "id INTEGER PRIMARY KEY,"
			    "device_id STRING DEFAULT NULL,"
			    "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
			    "key STRING DEFAULT NULL,"
			    "val INTEGER);";
		if (!sbu_database_execute(self, statement, error))
			return FALSE;
	}

	/* success */
	g_debug("database open and ready for action!");
	return TRUE;
}

static void
sbu_database_item_free(SbuDatabaseItem *item)
{
	g_free(item->key);
	g_free(item);
}

GPtrArray *
sbu_database_get_latest(SbuDatabase *self, const gchar *device_id, guint limit, GError **error)
{
	g_autoptr(GPtrArray) results =
	    g_ptr_array_new_with_free_func((GDestroyNotify)sbu_database_item_free);
	gchar *error_msg = NULL;
	gint rc;
	g_autofree gchar *statement = NULL;

	statement = g_strdup_printf("SELECT ts, val, key FROM log "
				    "WHERE device_id = '%s' "
				    "ORDER BY ts DESC LIMIT %u;",
				    device_id,
				    limit);
	rc = sqlite3_exec(self->db, statement, sbu_database_item_cb, results, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "SQL error: %s", error_msg);
		sqlite3_free(error_msg);
		return NULL;
	}

	/* success */
	return g_steal_pointer(&results);
}

gboolean
sbu_database_save_value(SbuDatabase *self,
			const gchar *device_id,
			const gchar *key,
			gint val,
			GError **error)
{
	g_autofree gchar *statement = NULL;

	/* sanity check */
	if (self->db == NULL) {
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "database is not open");
		return FALSE;
	}

	/* save to database */
	statement = g_strdup_printf("INSERT INTO log (ts, device_id, key, val) "
				    "VALUES ('%" G_GINT64_FORMAT "', '%s', '%s', '%i')",
				    g_get_real_time() / G_USEC_PER_SEC,
				    device_id,
				    key,
				    val);
	return sbu_database_execute(self, statement, error);
}

GPtrArray *
sbu_database_query(SbuDatabase *self,
		   const gchar *device_id,
		   const gchar *key,
		   gint64 ts_start,
		   gint64 ts_end,
		   GError **error)
{
	g_autoptr(GPtrArray) results =
	    g_ptr_array_new_with_free_func((GDestroyNotify)sbu_database_item_free);
	gchar *error_msg = NULL;
	gint rc;
	g_autofree gchar *statement = NULL;

	statement = g_strdup_printf("SELECT ts, val FROM log "
				    "WHERE device_id = '%s' "
				    "AND key = '%s' "
				    "AND ts >= %" G_GINT64_FORMAT " "
				    "AND ts <= %" G_GINT64_FORMAT " "
				    "ORDER BY ts ASC;",
				    device_id,
				    key,
				    ts_start,
				    ts_end);
	rc = sqlite3_exec(self->db, statement, sbu_database_item_cb, results, &error_msg);
	if (rc != SQLITE_OK) {
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "SQL error: %s", error_msg);
		sqlite3_free(error_msg);
		return NULL;
	}

	/* success */
	return g_steal_pointer(&results);
}

static void
sbu_database_finalize(GObject *object)
{
	SbuDatabase *self = SBU_DATABASE(object);

	if (self->db != NULL)
		sqlite3_close(self->db);
	g_free(self->location);

	G_OBJECT_CLASS(sbu_database_parent_class)->finalize(object);
}

static void
sbu_database_init(SbuDatabase *self)
{
}

static void
sbu_database_class_init(SbuDatabaseClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	object_class->finalize = sbu_database_finalize;
}

SbuDatabase *
sbu_database_new(void)
{
	SbuDatabase *self;
	self = g_object_new(SBU_TYPE_DATABASE, NULL);
	return SBU_DATABASE(self);
}
