/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2017 Richard Hughes <richard@hughsie.com>
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

#ifndef __SBU_PLUGIN_PRIVATE_H
#define __SBU_PLUGIN_PRIVATE_H

#include <glib-object.h>

#include "sbu-plugin.h"

G_BEGIN_DECLS

SbuPlugin	*sbu_plugin_new				(void);
SbuPlugin	*sbu_plugin_create			(const gchar	*filename,
							 GError		**error);
gpointer	 sbu_plugin_get_symbol			(SbuPlugin	*plugin,
							 const gchar	*function_name);

G_END_DECLS

#endif /* __SBU_PLUGIN_PRIVATE_H */

/* vim: set noexpandtab: */
