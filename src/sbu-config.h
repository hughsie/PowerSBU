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


#ifndef __SBU_CONFIG_H
#define __SBU_CONFIG_H

#include <glib-object.h>

G_BEGIN_DECLS

#define SBU_TYPE_CONFIG (sbu_config_get_type ())

G_DECLARE_FINAL_TYPE (SbuConfig, sbu_config, SBU, CONFIG, GObject)

SbuConfig	*sbu_config_new			(void);
gchar		*sbu_config_get_string		(SbuConfig	*self,
						 const gchar	*key,
						 GError		**error);
gint		 sbu_config_get_integer		(SbuConfig	*self,
						 const gchar	*key,
						 GError		**error);
gboolean	 sbu_config_get_boolean		(SbuConfig	*self,
						 const gchar	*key,
						 GError		**error);

G_END_DECLS

#endif /* __SBU_CONFIG_H */
