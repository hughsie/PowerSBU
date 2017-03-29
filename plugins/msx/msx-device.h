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


#ifndef __MSX_DEVICE_H
#define __MSX_DEVICE_H

#include <gusb.h>

G_BEGIN_DECLS

#define MSX_TYPE_DEVICE (msx_device_get_type ())

G_DECLARE_FINAL_TYPE (MsxDevice, msx_device, MSX, DEVICE, GObject)

MsxDevice	*msx_device_new				(GUsbDevice	*usb_device);

gboolean	 msx_device_close			(MsxDevice	*self,
							 GError		**error);
gboolean	 msx_device_open			(MsxDevice	*self,
							 GError		**error);
gboolean	 msx_device_refresh			(MsxDevice	*self,
							 GError		**error);
GBytes		*msx_device_send_command		(MsxDevice	*self,
							 const gchar	*cmd,
							 GError		**error);
const gchar	*msx_device_get_serial_number		(MsxDevice	*self);
const gchar	*msx_device_get_firmware_version1	(MsxDevice	*self);
const gchar	*msx_device_get_firmware_version2	(MsxDevice	*self);
gint		 msx_device_get_value			(MsxDevice	*self,
							 const gchar	*key);

G_END_DECLS

#endif /* __MSX_DEVICE_H */
