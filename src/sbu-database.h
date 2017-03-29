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

#ifndef __SBU_DATABASE_H
#define __SBU_DATABASE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define SBU_TYPE_DATABASE (sbu_database_get_type ())

G_DECLARE_FINAL_TYPE (SbuDatabase, sbu_database, SBU, DATABASE, GObject)

#define SBU_DEVICE_ID_DEFAULT		0

typedef struct {
	gint64		 ts;
	gint		 val;
} SbuDatabaseItem;

SbuDatabase	*sbu_database_new			(void);
gboolean	 sbu_database_open			(SbuDatabase	*self,
							 GError		**error);
gboolean	 sbu_database_repair			(SbuDatabase	*self,
							 GError		**error);
void		 sbu_database_set_location		(SbuDatabase	*self,
							 const gchar	*location);
gboolean	 sbu_database_save_value		(SbuDatabase	*self,
							 const gchar	*key,
							 gint		 val,
							 GError		**error);
GPtrArray	*sbu_database_query			(SbuDatabase	*self,
							 const gchar	*key,
							 guint		 dev,
							 gint64		 ts_start,
							 gint64		 ts_end,
							 GError		**error);
GHashTable	*sbu_database_get_latest		(SbuDatabase	*self,
							 guint		 dev,
							 GError		**error);

G_END_DECLS

#endif /* __SBU_DATABASE_H */
