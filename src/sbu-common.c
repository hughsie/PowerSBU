/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#include "config.h"

#include <glib.h>
#include <math.h>

#include "sbu-common.h"

const gchar *
sbu_node_kind_to_string(SbuNodeKind kind)
{
	if (kind == SBU_NODE_KIND_UNKNOWN)
		return "unknown";
	if (kind == SBU_NODE_KIND_SOLAR)
		return "solar";
	if (kind == SBU_NODE_KIND_BATTERY)
		return "battery";
	if (kind == SBU_NODE_KIND_UTILITY)
		return "utility";
	if (kind == SBU_NODE_KIND_LOAD)
		return "load";
	return NULL;
}

const gchar *
sbu_device_property_to_string(SbuDeviceProperty key)
{
	if (key == SBU_DEVICE_PROPERTY_UNKNOWN)
		return "unknown";
	if (key == SBU_DEVICE_PROPERTY_VOLTAGE)
		return "voltage";
	if (key == SBU_DEVICE_PROPERTY_POWER)
		return "power";
	if (key == SBU_DEVICE_PROPERTY_CURRENT)
		return "current";
	if (key == SBU_DEVICE_PROPERTY_VOLTAGE_MAX)
		return "voltage-max";
	if (key == SBU_DEVICE_PROPERTY_POWER_MAX)
		return "power-max";
	if (key == SBU_DEVICE_PROPERTY_CURRENT_MAX)
		return "current-max";
	if (key == SBU_DEVICE_PROPERTY_FREQUENCY)
		return "frequency";
	return NULL;
}

const gchar *
sbu_device_property_to_unit(SbuDeviceProperty key)
{
	if (key == SBU_DEVICE_PROPERTY_VOLTAGE || key == SBU_DEVICE_PROPERTY_VOLTAGE_MAX)
		return "V";
	if (key == SBU_DEVICE_PROPERTY_POWER || key == SBU_DEVICE_PROPERTY_POWER_MAX)
		return "W";
	if (key == SBU_DEVICE_PROPERTY_CURRENT || key == SBU_DEVICE_PROPERTY_CURRENT_MAX)
		return "A";
	if (key == SBU_DEVICE_PROPERTY_FREQUENCY)
		return "Hz";
	return NULL;
}

gchar *
sbu_format_for_display(gdouble val, const gchar *suffix)
{
	GString *str = g_string_new(NULL);
	gboolean kilo = FALSE;
	guint numdigits = 4;

	/* leave room for negative */
	if (val < 0)
		numdigits++;

	/* big number */
	if (fabs(val) > 1000) {
		kilo = TRUE;
		numdigits--;
		val /= 1000;
	}
	g_string_printf(str, "%.1f", val);

	/* don't show trailing zeros */
	if (g_str_has_suffix(str->str, ".0"))
		g_string_truncate(str, str->len - 2);

	/* truncate this down */
	if (str->len > numdigits)
		g_string_truncate(str, numdigits);
	if (g_str_has_suffix(str->str, "."))
		g_string_truncate(str, str->len - 1);
	if (suffix != NULL) {
		if (kilo)
			g_string_append(str, "k");
		g_string_append(str, suffix);
	}
	return g_string_free(str, FALSE);
}
