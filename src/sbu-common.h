/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __SBU_COMMON_H__
#define __SBU_COMMON_H__

G_BEGIN_DECLS

#include <glib.h>

#define SBU_DBUS_NAME		"com.hughski.PowerSBU"
#define SBU_DBUS_PATH		"com.hughski.PowerSBU"
#define SBU_DBUS_PATH_MANAGER	"/com/hughski/PowerSBU/Manager"

typedef enum {
	SBU_NODE_KIND_UNKNOWN,
	SBU_NODE_KIND_SOLAR,
	SBU_NODE_KIND_BATTERY,
	SBU_NODE_KIND_UTILITY,
	SBU_NODE_KIND_LOAD,
	SBU_NODE_KIND_LAST
} SbuNodeKind;

const gchar	*sbu_node_kind_to_string	(SbuNodeKind	 kind);

gchar		*sbu_format_for_display		(gdouble	 val,
						 const gchar	*suffix);

G_END_DECLS

#endif /* __SBU_COMMON_H__ */
