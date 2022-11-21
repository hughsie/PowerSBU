/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include <gusb.h>

#include "sbu-device.h"
#include "sbu-msx-common.h"

#define SBU_TYPE_MSX_DEVICE sbu_msx_device_get_type()
G_DECLARE_FINAL_TYPE(SbuMsxDevice, sbu_msx_device, SBU, MSX_DEVICE, SbuDevice)

SbuMsxDevice *
sbu_msx_device_new(GUsbDevice *usb_device);

gboolean
sbu_msx_device_close(SbuMsxDevice *self, GError **error);
gboolean
sbu_msx_device_open(SbuMsxDevice *self, GError **error);
gint
sbu_msx_device_get_value(SbuMsxDevice *self, SbuMsxDeviceKey key);
