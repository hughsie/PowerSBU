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
	gint v = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_VOLTAGE);
	gint a = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT);
	if (a == 0)
		a = -msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_CURRENT);
	sbu_device_impl_set_node_value (device,
					SBU_NODE_KIND_BATTERY,
					SBU_DEVICE_PROPERTY_POWER,
					msx_vals_to_double (v, a));
}

static void
msx_device_update_link_solar_load (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	gint src_prio = msx_device_get_value (msx_device, MSX_DEVICE_KEY_OUTPUT_SOURCE_PRIORITY);
	if (src_prio / 1000 == MSX_DEVICE_OUTPUT_SOURCE_PRIORITY_SOLAR) {
		/* link is active when:
		 * 1. solar power is greater than load power
		 * 2. solar voltage is present
		 * 3. load power is present
		 */
		gdouble v_solr = sbu_device_impl_get_node_value (device,
								 SBU_NODE_KIND_SOLAR,
								 SBU_DEVICE_PROPERTY_VOLTAGE);
		gdouble p_solr = sbu_device_impl_get_node_value (device,
								 SBU_NODE_KIND_SOLAR,
								 SBU_DEVICE_PROPERTY_POWER);
		gdouble p_load = sbu_device_impl_get_node_value (device,
								 SBU_NODE_KIND_LOAD,
								 SBU_DEVICE_PROPERTY_POWER);
		sbu_device_impl_set_link_active (device,
						 SBU_NODE_KIND_SOLAR,
						 SBU_NODE_KIND_LOAD,
						 v_solr > 0 && p_solr > p_load);
	} else {
		/* link is active when:
		 * 1. battery is NOT charging
		 * 2. solar voltage is present
		 * 3. load power is present
		 */
		gint b_chg = msx_device_get_value (msx_device, MSX_DEVICE_KEY_CHARGING_ON);
		gint v_sol = msx_device_get_value (msx_device, MSX_DEVICE_KEY_PV_INPUT_VOLTAGE);
		gint p_out = msx_device_get_value (msx_device, MSX_DEVICE_KEY_AC_OUTPUT_POWER);
		sbu_device_impl_set_link_active (device,
						 SBU_NODE_KIND_SOLAR,
						 SBU_NODE_KIND_LOAD,
						 b_chg == 0 && v_sol > 0 && p_out > 0);
	}
}

static void
msx_device_update_node_solar_voltage (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	/* if the PWM voltage is nonzero, get the voltage as
	 * applied to the battery */
	gint v = msx_device_get_value (msx_device, MSX_DEVICE_KEY_PV_INPUT_VOLTAGE);
	if (v > 0)
		v = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC);
	sbu_device_impl_set_node_value (device,
					SBU_NODE_KIND_SOLAR,
					SBU_DEVICE_PROPERTY_VOLTAGE,
					msx_val_to_double (v));
}

static void
msx_device_update_node_utility_power (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	gboolean active;

	/* we don't have any reading for input power or current so if
	 * the link is active, copy the load value + 5W */
	active = sbu_device_impl_get_link_active (device,
						  SBU_NODE_KIND_UTILITY,
						  SBU_NODE_KIND_LOAD);
	if (active) {
		gdouble tmp;
		tmp = sbu_device_impl_get_node_value (device,
						      SBU_NODE_KIND_LOAD,
						      SBU_DEVICE_PROPERTY_POWER);
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_UTILITY,
						SBU_DEVICE_PROPERTY_POWER,
						tmp + 5.f);
	} else {
		/* not using utility */
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_UTILITY,
						SBU_DEVICE_PROPERTY_POWER,
						0.f);
	}
}

static void
msx_device_update_link_utility_load (SbuDeviceImpl *device, MsxDevice *msx_device)
{
	gboolean active;
	active = sbu_device_impl_get_link_active (device,
						  SBU_NODE_KIND_SOLAR,
						  SBU_NODE_KIND_LOAD);
	if (active) {
		sbu_device_impl_set_link_active (device,
						 SBU_NODE_KIND_UTILITY,
						 SBU_NODE_KIND_LOAD,
						 FALSE);
	} else {
		gint a_bat = msx_device_get_value (msx_device, MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT);
		gint v_uti = msx_device_get_value (msx_device, MSX_DEVICE_KEY_GRID_VOLTAGE);
		sbu_device_impl_set_link_active (device,
						 SBU_NODE_KIND_UTILITY,
						 SBU_NODE_KIND_LOAD,
						 a_bat == 0 && v_uti > 0);
	}
	msx_device_update_node_utility_power (device, msx_device);
}

static void
msx_device_changed_cb (MsxDevice *msx_device,
		       MsxDeviceKey key,
		       gint value,
		       SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	SbuDeviceImpl *device = NULL;

	device = g_hash_table_lookup (self->devices, msx_device);
	g_assert (device != NULL);

	switch (key) {
	case MSX_DEVICE_KEY_GRID_RATING_VOLTAGE:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_UTILITY,
						SBU_DEVICE_PROPERTY_VOLTAGE_MAX,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_GRID_RATING_CURRENT:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_UTILITY,
						SBU_DEVICE_PROPERTY_CURRENT_MAX,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_VOLTAGE:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_VOLTAGE_MAX,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_FREQUENCY:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_FREQUENCY,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_CURRENT:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_CURRENT_MAX,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_RATING_ACTIVE_POWER:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_POWER_MAX,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_BATTERY_FLOAT_VOLTAGE:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_BATTERY,
						SBU_DEVICE_PROPERTY_VOLTAGE_MAX,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_PRESENT_MAX_CHARGING_CURRENT:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_BATTERY,
						SBU_DEVICE_PROPERTY_CURRENT_MAX,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_GRID_VOLTAGE:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_UTILITY,
						SBU_DEVICE_PROPERTY_VOLTAGE,
						msx_val_to_double (value));
		msx_device_update_link_utility_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_GRID_FREQUENCY:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_UTILITY,
						SBU_DEVICE_PROPERTY_FREQUENCY,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_VOLTAGE:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_VOLTAGE,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_FREQUENCY:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_FREQUENCY,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_AC_OUTPUT_POWER:
		/* MSX can't measure any power load less than 50W */
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_POWER,
						MAX (msx_val_to_double (value), 20.f));
		msx_device_update_link_solar_load (device, msx_device);
		msx_device_update_link_utility_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_BATTERY,
						SBU_DEVICE_PROPERTY_VOLTAGE,
						msx_val_to_double (value));
		msx_device_update_node_battery_power (device, msx_device);
		break;
	case MSX_DEVICE_KEY_BATTERY_CURRENT:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_BATTERY,
						SBU_DEVICE_PROPERTY_CURRENT,
						-msx_val_to_double (value));
		msx_device_update_node_battery_power (device, msx_device);
		break;
	case MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_SOLAR,
						SBU_DEVICE_PROPERTY_CURRENT,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC:
		msx_device_update_node_solar_voltage (device, msx_device);
		msx_device_update_link_solar_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_PV_CHARGING_POWER:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_SOLAR,
						SBU_DEVICE_PROPERTY_POWER,
						msx_val_to_double (value));
		break;
	case MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT:
		sbu_device_impl_set_node_value (device,
						SBU_NODE_KIND_BATTERY,
						SBU_DEVICE_PROPERTY_CURRENT,
						msx_val_to_double (value));
		sbu_device_impl_set_link_active (device,
						 SBU_NODE_KIND_BATTERY,
						 SBU_NODE_KIND_LOAD,
						 value >= 1);
		msx_device_update_node_battery_power (device, msx_device);
		msx_device_update_link_utility_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_CHARGING_ON:
		msx_device_update_link_solar_load (device, msx_device);
		break;
	case MSX_DEVICE_KEY_CHARGING_ON_SOLAR:
		sbu_device_impl_set_link_active (device,
						 SBU_NODE_KIND_SOLAR,
						 SBU_NODE_KIND_BATTERY,
						 value >= 1);
		break;
	case MSX_DEVICE_KEY_CHARGING_ON_AC:
		sbu_device_impl_set_link_active (device,
						 SBU_NODE_KIND_UTILITY,
						 SBU_NODE_KIND_BATTERY,
						 value >= 1);
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
	case MSX_DEVICE_KEY_ENABLE_BUZZER:
	case MSX_DEVICE_KEY_OVERLOAD_BYPASS_FUNCTION:
	case MSX_DEVICE_KEY_POWER_SAVE:
	case MSX_DEVICE_KEY_LCD_DISPLAY_ESCAPE:
	case MSX_DEVICE_KEY_OVERLOAD_RESTART:
	case MSX_DEVICE_KEY_OVER_TEMPERATURE_RESTART:
	case MSX_DEVICE_KEY_LCD_BACKLIGHT:
	case MSX_DEVICE_KEY_ALARM_PRIMARY_SOURCE_INTERRUPT:
	case MSX_DEVICE_KEY_FAULT_CODE_RECORD:
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE_OFFSET_FOR_FANS:
	case MSX_DEVICE_KEY_EEPROM_VERSION:
	case MSX_DEVICE_KEY_CHARGING_TO_FLOATING_MODE:
	case MSX_DEVICE_KEY_SWITCH_ON:
		g_debug ("key %s=%i not handled",
			 sbu_device_key_to_string (key), value);
		break;
	default:
		g_warning ("key %s=%i not handled",
			   sbu_device_key_to_string (key), value);
		break;
	}

	/* save nearly every changed key until we have a stable API */
	switch (key) {
	case MSX_DEVICE_KEY_AC_OUTPUT_FREQUENCY:
	case MSX_DEVICE_KEY_AC_OUTPUT_VOLTAGE:
	case MSX_DEVICE_KEY_BATTERY_CURRENT:
	case MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT:
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE:
	case MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC:
	case MSX_DEVICE_KEY_GRID_FREQUENCY:
	case MSX_DEVICE_KEY_GRID_RATING_VOLTAGE:
	case MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY:
		break;
	default:
		if (msx_device_get_value (msx_device, key) != value) {
			sbu_plugin_update_metadata (plugin, device,
						    sbu_device_key_to_string (key), value);
		}
		break;
	}
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
		g_autoptr(SbuNodeImpl) node = sbu_node_impl_new (kinds[i]);
		sbu_device_impl_add_node (device, node);
	}
	for (guint i = 0; links[i] != SBU_NODE_KIND_UNKNOWN; i += 2) {
		g_autoptr(SbuLinkImpl) link = sbu_link_impl_new (links[i], links[i+1]);
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
