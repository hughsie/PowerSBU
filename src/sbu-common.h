/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#pragma once

#include <glib.h>

#define SBU_DBUS_NAME	   "com.hughski.PowerSBU"
#define SBU_DBUS_INTERFACE "com.hughski.PowerSBU"
#define SBU_DBUS_PATH	   "/com/hughski/PowerSBU"

typedef enum {
	SBU_NODE_KIND_UNKNOWN,
	SBU_NODE_KIND_SOLAR,
	SBU_NODE_KIND_BATTERY,
	SBU_NODE_KIND_UTILITY,
	SBU_NODE_KIND_LOAD,
	SBU_NODE_KIND_LAST
} SbuNodeKind;

typedef enum {
	SBU_DEVICE_PROPERTY_UNKNOWN,
	SBU_DEVICE_PROPERTY_POWER,
	SBU_DEVICE_PROPERTY_POWER_MAX,
	SBU_DEVICE_PROPERTY_VOLTAGE,
	SBU_DEVICE_PROPERTY_VOLTAGE_MAX,
	SBU_DEVICE_PROPERTY_CURRENT,
	SBU_DEVICE_PROPERTY_CURRENT_MAX,
	SBU_DEVICE_PROPERTY_FREQUENCY,
	SBU_DEVICE_PROPERTY_LAST
} SbuDeviceProperty;

const gchar *
sbu_node_kind_to_string(SbuNodeKind kind);
const gchar *
sbu_device_property_to_string(SbuDeviceProperty value);
const gchar *
sbu_device_property_to_unit(SbuDeviceProperty value);

gchar *
sbu_format_for_display(gdouble val, const gchar *suffix);
