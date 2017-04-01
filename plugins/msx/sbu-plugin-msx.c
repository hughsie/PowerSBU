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

#include <config.h>

#include "sbu-plugin.h"
#include "sbu-plugin-vfuncs.h"

#include "msx-context.h"
#include "msx-device.h"

struct SbuPluginData {
	MsxContext		*context;
	GHashTable		*devices; /* MsxDevice : SbuDeviceImpl */
};

static gdouble
msx_val_to_double (gint value)
{
	return ((gdouble) value) / 1000.f;
}

static gdouble
msx_vals_to_double (gint value1, gint value2)
{
	return msx_val_to_double (value1) * msx_val_to_double (value2);
}

static void
msx_device_update_node_battery_power (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	SbuNodeImpl *n = sbu_device_impl_get_node (device, SBU_NODE_KIND_BATTERY);
	gint v = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_VOLTAGE);
	gint a = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_CURRENT);
	if (a == 0)
		a = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT);
	g_object_set (n, "power", msx_vals_to_double (v, a), NULL);
}

static void
msx_device_update_node_solar_power (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	SbuNodeImpl *n = sbu_device_impl_get_node (device, SBU_NODE_KIND_SOLAR);
	gint v = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC);
	gint a = msx_device_get_value (msx_device, MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY);
	g_object_set (n, "power", msx_vals_to_double (v, a), NULL);
}

static void
msx_device_update_link_solar_load (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	gint b_chg = msx_device_get_value (msx_device, MSX_DEVICE_KEY_CHARGING_ON);
	gint v_sol = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC);
	gint p_out = msx_device_get_value (msx_device, MSX_DEVICE_KEY_AC_OUTPUT_POWER);
	SbuLinkImpl *l = sbu_device_impl_get_link (device, SBU_NODE_KIND_SOLAR, SBU_NODE_KIND_LOAD);
	g_object_set (l, "active", b_chg == 0 && v_sol > 0 && p_out > 0, NULL);
}

static void
msx_device_update_link_utility_load (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	gint a_bat = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT);
	gint v_uti = msx_device_get_value (msx_device, MSX_DEVICE_KEY_GRID_VOLTAGE);
	gint p_out = msx_device_get_value (msx_device, MSX_DEVICE_KEY_AC_OUTPUT_POWER);
	SbuLinkImpl *l = sbu_device_impl_get_link (device, SBU_NODE_KIND_UTILITY, SBU_NODE_KIND_LOAD);
	g_object_set (l, "active", a_bat == 0 && v_uti > 0 && p_out > 0, NULL);
}

static void
msx_device_changed_cb (MsxDevice *msx_device,
		       MsxDeviceKey key,
		       gint value,
		       SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	SbuNodeImpl *n = NULL;
	SbuDeviceImpl *device = NULL;
	SbuLinkImpl *l = NULL;

	device = g_hash_table_lookup (self->devices, msx_device);
	g_assert (device != NULL);

	switch (key) {
	case MSX_DEVICE_KEY_GRID_RATING_VOLTAGE:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_UTILITY);
		g_object_set (n, "voltage-max", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_GRID_RATING_CURRENT:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_UTILITY);
		g_object_set (n, "current-max", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_VOLTAGE:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_LOAD);
		g_object_set (n, "voltage-max", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_FREQUENCY:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_LOAD);
		g_object_set (n, "frequency", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_CURRENT:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_LOAD);
		g_object_set (n, "current-max", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_ACTIVE_POWER:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_LOAD);
		g_object_set (n, "power-max", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_BATTERY_FLOAT_VOLTAGE:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_BATTERY);
		g_object_set (n, "voltage-max", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_PRESENT_MAX_CHARGING_CURRENT:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_BATTERY);
		g_object_set (n, "current-max", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_GRID_VOLTAGE:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_UTILITY);
		g_object_set (n, "voltage", msx_val_to_double (value), NULL);
		msx_device_update_link_utility_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_GRID_FREQUENCY:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_UTILITY);
		g_object_set (n, "frequency", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_VOLTAGE:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_LOAD);
		g_object_set (n, "voltage", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_FREQUENCY:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_LOAD);
		g_object_set (n, "frequency", msx_val_to_double (value), NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_POWER:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_LOAD);
		g_object_set (n, "power", msx_val_to_double (value), NULL);
		msx_device_update_link_solar_load (device, msx_device);
		msx_device_update_link_utility_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_BATTERY);
		g_object_set (n, "voltage", msx_val_to_double (value), NULL);
		msx_device_update_node_battery_power (device, msx_device);
		break;
	case MSX_DEVICE_KEY_BATTERY_CURRENT:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_BATTERY);
		g_object_set (n, "current", msx_val_to_double (value), NULL);
		msx_device_update_node_battery_power (device, msx_device);
		break;
	case MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_SOLAR);
		g_object_set (n, "current", msx_val_to_double (value), NULL);
		msx_device_update_node_solar_power (device, msx_device);
		break;
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_SOLAR);
		g_object_set (n, "voltage", msx_val_to_double (value), NULL);
		msx_device_update_node_solar_power (device, msx_device);
		msx_device_update_link_solar_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT:
		n = sbu_device_impl_get_node (device, SBU_NODE_KIND_BATTERY);
		g_object_set (n, "current", msx_val_to_double (value), NULL);
		msx_device_update_node_battery_power (device, msx_device);
		l = sbu_device_impl_get_link (device,
					      SBU_NODE_KIND_BATTERY,
					      SBU_NODE_KIND_LOAD);
		g_object_set (l, "active", value >= 1 ? TRUE : FALSE, NULL);
		msx_device_update_link_utility_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_CHARGING_ON:
		msx_device_update_link_solar_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_CHARGING_ON_SOLAR:
		l = sbu_device_impl_get_link (device,
					      SBU_NODE_KIND_SOLAR,
					      SBU_NODE_KIND_BATTERY);
		g_object_set (l, "active", value >= 1 ? TRUE : FALSE, NULL);
		break;
	case MSX_DEVICE_KEY_CHARGING_ON_AC:
		l = sbu_device_impl_get_link (device,
					      SBU_NODE_KIND_UTILITY,
					      SBU_NODE_KIND_BATTERY);
		g_object_set (l, "active", value >= 1 ? TRUE : FALSE, NULL);
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_ACTIVE_POWER:
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_APPARENT_POWER:
	case MSX_DEVICE_KEY_BATTERY_RATING_VOLTAGE:
	case MSX_DEVICE_KEY_BATTERY_RECHARGE_VOLTAGE:
	case MSX_DEVICE_KEY_BATTERY_UNDER_VOLTAGE:
	case MSX_DEVICE_KEY_BATTERY_BULK_VOLTAGE:
	case MSX_DEVICE_KEY_BATTERY_TYPE:
	case MSX_DEVICE_KEY_PRESENT_MAX_AC_CHARGING_CURRENT:
	case MSX_DEVICE_KEY_INPUT_VOLTAGE_RANGE:
	case MSX_DEVICE_KEY_OUTPUT_SOURCE_PRIORITY:
	case MSX_DEVICE_KEY_CHARGER_SOURCE_PRIORITY:
	case MSX_DEVICE_KEY_PARALLEL_MAX_NUM:
	case MSX_DEVICE_KEY_MACHINE_TYPE:
	case MSX_DEVICE_KEY_TOPOLOGY:
	case MSX_DEVICE_KEY_OUTPUT_MODE:
	case MSX_DEVICE_KEY_BATTERY_REDISCHARGE_VOLTAGE:
	case MSX_DEVICE_KEY_PV_OK_CONDITION_FOR_PARALLEL:
	case MSX_DEVICE_KEY_PV_POWER_BALANCE:
	case MSX_DEVICE_KEY_MAXIMUM_POWER_PERCENTAGE:
	case MSX_DEVICE_KEY_BUS_VOLTAGE:
	case MSX_DEVICE_KEY_BATTERY_CAPACITY:
	case MSX_DEVICE_KEY_INVERTER_HEATSINK_TEMPERATURE:
	case MSX_DEVICE_KEY_PV_INPUT_VOLTAGE:
	case MSX_DEVICE_KEY_ADD_SBU_PRIORITY_VERSION:
	case MSX_DEVICE_KEY_CONFIGURATION_STATUS_CHANGE:
	case MSX_DEVICE_KEY_SCC_FIRMWARE_VERSION_UPDATED:
	case MSX_DEVICE_KEY_LOAD_STATUS_ON:
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING:
		g_debug ("key %s not handled", sbu_device_key_to_string (key));
		break;
	default:
		g_warning ("key %s not handled", sbu_device_key_to_string (key));
		break;
	}

	/* save every key until we have a stable database API */
	sbu_plugin_update_metadata (plugin, device,
				    sbu_device_key_to_string (key), value);
}

static const gchar *
sbu_plugin_msx_remove_leading_zeros (const gchar *val)
{
	for (guint i = 0; val[i] != '\0'; i++) {
		if (val[i] != '0')
			return val + i;
	}
	return val;
}

static gchar *
msx_device_get_firmware_version (MsxDevice *msx_device)
{
	const gchar *ver1 = msx_device_get_firmware_version1 (msx_device);
	const gchar *ver2 = msx_device_get_firmware_version2 (msx_device);
	return g_strdup_printf ("%s, %s",
				sbu_plugin_msx_remove_leading_zeros (ver1),
				sbu_plugin_msx_remove_leading_zeros (ver2));
}

static void
msx_device_added_cb (MsxContext *context,
		     MsxDevice *msx_device,
		     SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	g_autoptr(GError) error = NULL;
	g_autoptr(SbuDeviceImpl) device = NULL;
	g_autofree gchar *firmware_version = NULL;
	SbuNodeKind kinds[] = { SBU_NODE_KIND_SOLAR,
				SBU_NODE_KIND_BATTERY,
				SBU_NODE_KIND_UTILITY,
				SBU_NODE_KIND_LOAD,
				SBU_NODE_KIND_UNKNOWN };
	SbuNodeKind links[] = { SBU_NODE_KIND_SOLAR,	SBU_NODE_KIND_BATTERY,
				SBU_NODE_KIND_SOLAR,	SBU_NODE_KIND_LOAD,
				SBU_NODE_KIND_BATTERY,	SBU_NODE_KIND_LOAD,
				SBU_NODE_KIND_UTILITY,	SBU_NODE_KIND_LOAD,
				SBU_NODE_KIND_UTILITY,	SBU_NODE_KIND_BATTERY,
				SBU_NODE_KIND_UNKNOWN,	SBU_NODE_KIND_UNKNOWN };

	/* create device */
	device = sbu_device_impl_new ();
	for (guint i = 0; kinds[i] != SBU_NODE_KIND_UNKNOWN; i++) {
		g_autoptr(SbuNodeImpl) node = sbu_node_impl_new ();
		g_object_set (node, "kind", kinds[i], NULL);
		sbu_device_impl_add_node (device, node);
	}
	for (guint i = 0; links[i] != SBU_NODE_KIND_UNKNOWN; i += 2) {
		g_autoptr(SbuLinkImpl) link = sbu_link_impl_new ();
		g_object_set (link,
			      "src", links[i+0],
			      "dst", links[i+1],
			      NULL);
		sbu_device_impl_add_link (device, link);
	}

	/* add the device */
	g_debug ("device added: %s", msx_device_get_serial_number (msx_device));
	g_hash_table_insert (self->devices,
			     g_object_ref (msx_device),
			     g_object_ref (device));

	/* open */
	g_signal_connect (msx_device, "changed",
			  G_CALLBACK (msx_device_changed_cb), plugin);
	if (!msx_device_open (msx_device, &error)) {
		g_warning ("failed to open: %s", error->message);
		return;
	}
	firmware_version = msx_device_get_firmware_version (msx_device);
	g_object_set (device,
		      "description", "PIP-MSX",
		      "serial-number", msx_device_get_serial_number (msx_device),
		      "firmware-version", firmware_version,
		      NULL);

	sbu_plugin_add_device (plugin, device);
}

static void
msx_device_removed_cb (MsxContext *context, MsxDevice *msx_device, SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	SbuDeviceImpl *device = g_hash_table_lookup (self->devices, msx_device);
	g_autoptr(GError) error = NULL;

	/* remove device */
	g_debug ("device removed: %s", msx_device_get_serial_number (msx_device));
	g_hash_table_remove (self->devices, msx_device);
	sbu_plugin_remove_device (plugin, device);

	/* close device */
	if (!msx_device_close (msx_device, &error)) {
		g_warning ("failed to close: %s", error->message);
		return;
	}
}

void
sbu_plugin_initialize (SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_alloc_data (plugin, sizeof(SbuPluginData));
	self->context = msx_context_new ();
	g_signal_connect (self->context, "added",
			  G_CALLBACK (msx_device_added_cb), plugin);
	g_signal_connect (self->context, "removed",
			  G_CALLBACK (msx_device_removed_cb), plugin);
	self->devices = g_hash_table_new_full (g_direct_hash, g_direct_equal,
						(GDestroyNotify) g_object_unref,
						(GDestroyNotify) g_object_unref);
}

gboolean
sbu_plugin_refresh (SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	g_autoptr(GList) devices = g_hash_table_get_keys (self->devices);
	for (GList *l = devices; l != NULL; l = l->next) {
		MsxDevice *device = MSX_DEVICE (l->data);
		if (!msx_device_refresh (device, error))
			return FALSE;
	}
	return TRUE;
}

gboolean
sbu_plugin_setup (SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);

	/* get all the SBU devices */
	if (!msx_context_coldplug (self->context, error)) {
		g_prefix_error (error, "failed to coldplug: ");
		return FALSE;
	}

	return TRUE;
}

void
sbu_plugin_destroy (SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	g_object_unref (self->context);
	g_hash_table_unref (self->devices);
}
