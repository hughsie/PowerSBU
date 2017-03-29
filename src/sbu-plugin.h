/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2017 Richard Hughes <richard@hughsie.com>
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

#ifndef __SBU_PLUGIN_H
#define __SBU_PLUGIN_H

#include <glib-object.h>
#include <gmodule.h>
#include <gio/gio.h>

#include "sbu-common.h"
#include "sbu-device-impl.h"

G_BEGIN_DECLS

#define SBU_TYPE_PLUGIN (sbu_plugin_get_type ())

G_DECLARE_DERIVABLE_TYPE (SbuPlugin, sbu_plugin, SBU, PLUGIN, GObject)

struct _SbuPluginClass
{
	GObjectClass		 parent_class;
	void			(*update_metadata)	(SbuPlugin	*plugin,
							 SbuDeviceImpl	*device,
							 const gchar	*key,
							 gint		 value);
	void			(*add_device)		(SbuPlugin	*plugin,
							 SbuDeviceImpl	*device);
	void			(*remove_device)	(SbuPlugin	*plugin,
							 SbuDeviceImpl	*device);
};

typedef struct	SbuPluginData	SbuPluginData;

SbuPluginData	*sbu_plugin_alloc_data			(SbuPlugin	*plugin,
							 gsize		 sz);
SbuPluginData	*sbu_plugin_get_data			(SbuPlugin	*plugin);
const gchar	*sbu_plugin_get_name			(SbuPlugin	*plugin);
gboolean	 sbu_plugin_get_enabled			(SbuPlugin	*plugin);
void		 sbu_plugin_set_enabled			(SbuPlugin	*plugin,
							 gboolean	 enabled);

void		 sbu_plugin_update_metadata		(SbuPlugin	*plugin,
							 SbuDeviceImpl	*device,
							 const gchar	*key,
							 gint		 value);
void		 sbu_plugin_add_device			(SbuPlugin	*plugin,
							 SbuDeviceImpl	*device);
void		 sbu_plugin_remove_device		(SbuPlugin	*plugin,
							 SbuDeviceImpl	*device);

G_END_DECLS

#endif /* __SBU_PLUGIN_H */

/* vim: set noexpandtab: */
