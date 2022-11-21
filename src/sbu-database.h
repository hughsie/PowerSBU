/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include <glib-object.h>

#define SBU_TYPE_DATABASE (sbu_database_get_type())

G_DECLARE_FINAL_TYPE(SbuDatabase, sbu_database, SBU, DATABASE, GObject)

typedef struct {
	gchar *key;
	gint64 ts;
	gint val;
} SbuDatabaseItem;

SbuDatabase *
sbu_database_new(void);
gboolean
sbu_database_open(SbuDatabase *self, GError **error);
gboolean
sbu_database_repair(SbuDatabase *self, GError **error);
void
sbu_database_set_location(SbuDatabase *self, const gchar *location);
gboolean
sbu_database_save_value(SbuDatabase *self,
			const gchar *device_id,
			const gchar *key,
			gint val,
			GError **error);
GPtrArray *
sbu_database_query(SbuDatabase *self,
		   const gchar *device_id,
		   const gchar *key,
		   gint64 ts_start,
		   gint64 ts_end,
		   GError **error);
GPtrArray *
sbu_database_get_latest(SbuDatabase *self, const gchar *device_id, guint limit, GError **error);
