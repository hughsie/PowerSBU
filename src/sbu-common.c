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

#include "config.h"

#include <glib.h>
#include <math.h>

#include "sbu-common.h"

const gchar *
sbu_node_kind_to_string (SbuNodeKind kind)
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
sbu_device_property_to_string (SbuDeviceProperty key)
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

gchar *
sbu_format_for_display (gdouble val, const gchar *suffix)
{
	GString *str = g_string_new (NULL);
	gboolean kilo = FALSE;
	guint numdigits = 4;

	/* leave room for negative */
	if (val < 0)
		numdigits++;

	/* big number */
	if (fabs (val) > 1000) {
		kilo = TRUE;
		numdigits--;
		val /= 1000;
	}
	g_string_printf (str, "%.1f", val);

	/* don't show trailing zeros */
	if (g_str_has_suffix (str->str, ".0"))
		g_string_truncate (str, str->len - 2);

	/* truncate this down */
	if (str->len > numdigits)
		g_string_truncate (str, numdigits);
	if (g_str_has_suffix (str->str, "."))
		g_string_truncate (str, str->len - 1);
	if (suffix != NULL) {
		if (kilo)
			g_string_append (str, "k");
		g_string_append (str, suffix);
	}
	return g_string_free (str, FALSE);
}
