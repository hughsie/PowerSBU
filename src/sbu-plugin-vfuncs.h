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

#ifndef __SBU_PLUGIN_VFUNCS_H
#define __SBU_PLUGIN_VFUNCS_H

#include <gio/gio.h>

G_BEGIN_DECLS

void		 sbu_plugin_initialize			(SbuPlugin	*plugin);
void		 sbu_plugin_destroy			(SbuPlugin	*plugin);
gboolean	 sbu_plugin_setup			(SbuPlugin	*plugin,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 sbu_plugin_refresh			(SbuPlugin	*plugin,
							 GCancellable	*cancellable,
							 GError		**error);

G_END_DECLS

#endif /* __SBU_PLUGIN_VFUNCS_H */

/* vim: set noexpandtab: */
