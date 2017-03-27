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


#ifndef __MSX_CONTEXT_H
#define __MSX_CONTEXT_H

#include <gusb.h>

#include "msx-database.h"

G_BEGIN_DECLS

#define MSX_CONTEXT_ID_DEFAULT		0

#define MSX_TYPE_CONTEXT (msx_context_get_type ())

G_DECLARE_FINAL_TYPE (MsxContext, msx_context, MSX, CONTEXT, GObject)

MsxContext	*msx_context_new		(void);
MsxContext	*msx_context_new_with_ctx	(GUsbContext	*usb_context);
gboolean	 msx_context_coldplug		(MsxContext	*self,
						 GError		**error);
GPtrArray	*msx_context_get_devices	(MsxContext	*self);

G_END_DECLS

#endif /* __MSX_CONTEXT_H */
