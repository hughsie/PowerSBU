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

#ifndef __MSX_DATABASE_H
#define __MSX_DATABASE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MSX_TYPE_DATABASE (msx_database_get_type ())

G_DECLARE_FINAL_TYPE (MsxDatabase, msx_database, MSX, DATABASE, GObject)

#define MSX_DEVICE_ID_DEFAULT		0

typedef struct {
	gint64		 ts;
	gint		 val;
} MsxDatabaseItem;

MsxDatabase	*msx_database_new			(void);
gboolean	 msx_database_open			(MsxDatabase	*self,
							 GError		**error);
gboolean	 msx_database_repair			(MsxDatabase	*self,
							 GError		**error);
void		 msx_database_set_location		(MsxDatabase	*self,
							 const gchar	*location);
gboolean	 msx_database_save_value		(MsxDatabase	*self,
							 const gchar	*key,
							 gint		 val,
							 GError		**error);
GPtrArray	*msx_database_query			(MsxDatabase	*self,
							 const gchar	*key,
							 guint		 dev,
							 gint64		 ts_start,
							 gint64		 ts_end,
							 GError		**error);
GHashTable	*msx_database_get_latest		(MsxDatabase	*self,
							 guint		 dev,
							 GError		**error);

G_END_DECLS

#endif /* __MSX_DATABASE_H */
