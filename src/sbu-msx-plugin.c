/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "sbu-msx-plugin.h"

#include <config.h>

#include "sbu-msx-device.h"

struct _SbuMsxPlugin {
	SbuPlugin parent_instance;
	GUsbContext *usb_context;
	GHashTable *devices;
};

G_DEFINE_TYPE(SbuMsxPlugin, sbu_msx_plugin, SBU_TYPE_PLUGIN)

static gdouble
sbu_msx_val_to_double(gint value)
{
	return ((gdouble)value) / 1000.f;
}

static gdouble
sbu_msx_vals_to_double(gint value1, gint value2)
{
	return sbu_msx_val_to_double(value1) * sbu_msx_val_to_double(value2);
}

static void
sbu_msx_device_update_node_battery_power(SbuMsxDevice *device)
{
	gint v = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE);
	gint a = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT);
	if (a == 0)
		a = -sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_BATTERY_CURRENT);
	sbu_device_set_node_value(SBU_DEVICE(device),
				  SBU_NODE_KIND_BATTERY,
				  SBU_DEVICE_PROPERTY_POWER,
				  sbu_msx_vals_to_double(v, a));
}

static void
sbu_msx_device_update_link_solar_load(SbuMsxDevice *device)
{
	gint src_prio = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_OUTPUT_SOURCE_PRIORITY);
	if (src_prio / 1000 == SBU_MSX_DEVICE_OUTPUT_SOURCE_PRIORITY_SOLAR) {
		/* link is active when:
		 * 1. solar power is greater than load power
		 * 2. solar voltage is present
		 * 3. load power is present
		 */
		gdouble v_solr = sbu_device_get_node_value(SBU_DEVICE(device),
							   SBU_NODE_KIND_SOLAR,
							   SBU_DEVICE_PROPERTY_VOLTAGE);
		gdouble p_solr = sbu_device_get_node_value(SBU_DEVICE(device),
							   SBU_NODE_KIND_SOLAR,
							   SBU_DEVICE_PROPERTY_POWER);
		gdouble p_load = sbu_device_get_node_value(SBU_DEVICE(device),
							   SBU_NODE_KIND_LOAD,
							   SBU_DEVICE_PROPERTY_POWER);
		sbu_device_set_link_active(SBU_DEVICE(device),
					   SBU_NODE_KIND_SOLAR,
					   SBU_NODE_KIND_LOAD,
					   v_solr > 0 && p_solr > p_load);
	} else {
		/* link is active when:
		 * 1. battery is NOT charging
		 * 2. solar voltage is present
		 * 3. load power is present
		 */
		gint b_chg = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_CHARGING_ON);
		gint v_sol = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_PV_INPUT_VOLTAGE);
		gint p_out = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_AC_OUTPUT_POWER);
		sbu_device_set_link_active(SBU_DEVICE(device),
					   SBU_NODE_KIND_SOLAR,
					   SBU_NODE_KIND_LOAD,
					   b_chg == 0 && v_sol > 0 && p_out > 0);
	}
}

static void
sbu_msx_device_update_node_solar_voltage(SbuMsxDevice *device)
{
	/* if the PWM voltage is nonzero, get the voltage as
	 * applied to the battery */
	gint v = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_PV_INPUT_VOLTAGE);
	if (v > 0)
		v = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC);
	sbu_device_set_node_value(SBU_DEVICE(device),
				  SBU_NODE_KIND_SOLAR,
				  SBU_DEVICE_PROPERTY_VOLTAGE,
				  sbu_msx_val_to_double(v));
}

static void
sbu_msx_device_update_node_utility_power(SbuMsxDevice *device)
{
	gboolean active;

	/* we don't have any reading for input power or current so if
	 * the link is active, copy the load value + 5W */
	active = sbu_device_get_link_active(SBU_DEVICE(device),
					    SBU_NODE_KIND_UTILITY,
					    SBU_NODE_KIND_LOAD);
	if (active) {
		gdouble tmp;
		tmp = sbu_device_get_node_value(SBU_DEVICE(device),
						SBU_NODE_KIND_LOAD,
						SBU_DEVICE_PROPERTY_POWER);
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_UTILITY,
					  SBU_DEVICE_PROPERTY_POWER,
					  tmp + 5.f);
	} else {
		/* not using utility */
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_UTILITY,
					  SBU_DEVICE_PROPERTY_POWER,
					  0.f);
	}
}

static void
sbu_msx_device_update_link_utility_load(SbuMsxDevice *device)
{
	gboolean active;
	active =
	    sbu_device_get_link_active(SBU_DEVICE(device), SBU_NODE_KIND_SOLAR, SBU_NODE_KIND_LOAD);
	if (active) {
		sbu_device_set_link_active(SBU_DEVICE(device),
					   SBU_NODE_KIND_UTILITY,
					   SBU_NODE_KIND_LOAD,
					   FALSE);
	} else {
		gint a_bat =
		    sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT);
		gint v_uti = sbu_msx_device_get_value(device, SBU_MSX_DEVICE_KEY_GRID_VOLTAGE);
		sbu_device_set_link_active(SBU_DEVICE(device),
					   SBU_NODE_KIND_UTILITY,
					   SBU_NODE_KIND_LOAD,
					   a_bat == 0 && v_uti > 0);
	}
	sbu_msx_device_update_node_utility_power(device);
}

static void
sbu_msx_device_changed_cb(SbuMsxDevice *device, SbuMsxDeviceKey key, gint value, SbuMsxPlugin *self)
{
	switch (key) {
	case SBU_MSX_DEVICE_KEY_GRID_RATING_VOLTAGE:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_UTILITY,
					  SBU_DEVICE_PROPERTY_VOLTAGE_MAX,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_GRID_RATING_CURRENT:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_UTILITY,
					  SBU_DEVICE_PROPERTY_CURRENT_MAX,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_RATING_VOLTAGE:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_LOAD,
					  SBU_DEVICE_PROPERTY_VOLTAGE_MAX,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_RATING_FREQUENCY:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_LOAD,
					  SBU_DEVICE_PROPERTY_FREQUENCY,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_RATING_CURRENT:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_LOAD,
					  SBU_DEVICE_PROPERTY_CURRENT_MAX,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_RATING_ACTIVE_POWER:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_LOAD,
					  SBU_DEVICE_PROPERTY_POWER_MAX,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_BATTERY_FLOAT_VOLTAGE:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_BATTERY,
					  SBU_DEVICE_PROPERTY_VOLTAGE_MAX,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_PRESENT_MAX_CHARGING_CURRENT:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_BATTERY,
					  SBU_DEVICE_PROPERTY_CURRENT_MAX,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_GRID_VOLTAGE:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_UTILITY,
					  SBU_DEVICE_PROPERTY_VOLTAGE,
					  sbu_msx_val_to_double(value));
		sbu_msx_device_update_link_utility_load(device);
		break;
	case SBU_MSX_DEVICE_KEY_GRID_FREQUENCY:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_UTILITY,
					  SBU_DEVICE_PROPERTY_FREQUENCY,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_VOLTAGE:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_LOAD,
					  SBU_DEVICE_PROPERTY_VOLTAGE,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_FREQUENCY:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_LOAD,
					  SBU_DEVICE_PROPERTY_FREQUENCY,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_POWER:
		/* MSX can't measure any power load less than 50W */
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_LOAD,
					  SBU_DEVICE_PROPERTY_POWER,
					  MAX(sbu_msx_val_to_double(value), 20.f));
		sbu_msx_device_update_link_solar_load(device);
		sbu_msx_device_update_link_utility_load(device);
		break;
	case SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_BATTERY,
					  SBU_DEVICE_PROPERTY_VOLTAGE,
					  sbu_msx_val_to_double(value));
		sbu_msx_device_update_node_battery_power(device);
		break;
	case SBU_MSX_DEVICE_KEY_BATTERY_CURRENT:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_BATTERY,
					  SBU_DEVICE_PROPERTY_CURRENT,
					  -sbu_msx_val_to_double(value));
		sbu_msx_device_update_node_battery_power(device);
		break;
	case SBU_MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_SOLAR,
					  SBU_DEVICE_PROPERTY_CURRENT,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC:
		sbu_msx_device_update_node_solar_voltage(device);
		sbu_msx_device_update_link_solar_load(device);
		break;
	case SBU_MSX_DEVICE_KEY_PV_CHARGING_POWER:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_SOLAR,
					  SBU_DEVICE_PROPERTY_POWER,
					  sbu_msx_val_to_double(value));
		break;
	case SBU_MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT:
		sbu_device_set_node_value(SBU_DEVICE(device),
					  SBU_NODE_KIND_BATTERY,
					  SBU_DEVICE_PROPERTY_CURRENT,
					  sbu_msx_val_to_double(value));
		sbu_device_set_link_active(SBU_DEVICE(device),
					   SBU_NODE_KIND_BATTERY,
					   SBU_NODE_KIND_LOAD,
					   value >= 1);
		sbu_msx_device_update_node_battery_power(device);
		sbu_msx_device_update_link_utility_load(device);
		break;
	case SBU_MSX_DEVICE_KEY_CHARGING_ON:
		sbu_msx_device_update_link_solar_load(device);
		break;
	case SBU_MSX_DEVICE_KEY_CHARGING_ON_SOLAR:
		sbu_device_set_link_active(SBU_DEVICE(device),
					   SBU_NODE_KIND_SOLAR,
					   SBU_NODE_KIND_BATTERY,
					   value >= 1);
		break;
	case SBU_MSX_DEVICE_KEY_CHARGING_ON_AC:
		sbu_device_set_link_active(SBU_DEVICE(device),
					   SBU_NODE_KIND_UTILITY,
					   SBU_NODE_KIND_BATTERY,
					   value >= 1);
		break;
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_ACTIVE_POWER:
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_RATING_APPARENT_POWER:
	case SBU_MSX_DEVICE_KEY_BATTERY_RATING_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_BATTERY_RECHARGE_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_BATTERY_UNDER_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_BATTERY_BULK_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_BATTERY_TYPE:
	case SBU_MSX_DEVICE_KEY_PRESENT_MAX_AC_CHARGING_CURRENT:
	case SBU_MSX_DEVICE_KEY_INPUT_VOLTAGE_RANGE:
	case SBU_MSX_DEVICE_KEY_OUTPUT_SOURCE_PRIORITY:
	case SBU_MSX_DEVICE_KEY_CHARGER_SOURCE_PRIORITY:
	case SBU_MSX_DEVICE_KEY_PARALLEL_MAX_NUM:
	case SBU_MSX_DEVICE_KEY_MACHINE_TYPE:
	case SBU_MSX_DEVICE_KEY_TOPOLOGY:
	case SBU_MSX_DEVICE_KEY_OUTPUT_MODE:
	case SBU_MSX_DEVICE_KEY_BATTERY_REDISCHARGE_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_PV_OK_CONDITION_FOR_PARALLEL:
	case SBU_MSX_DEVICE_KEY_PV_POWER_BALANCE:
	case SBU_MSX_DEVICE_KEY_MAXIMUM_POWER_PERCENTAGE:
	case SBU_MSX_DEVICE_KEY_BUS_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_BATTERY_CAPACITY:
	case SBU_MSX_DEVICE_KEY_INVERTER_HEATSINK_TEMPERATURE:
	case SBU_MSX_DEVICE_KEY_PV_INPUT_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_ADD_SBU_PRIORITY_VERSION:
	case SBU_MSX_DEVICE_KEY_CONFIGURATION_STATUS_CHANGE:
	case SBU_MSX_DEVICE_KEY_SCC_FIRMWARE_VERSION_UPDATED:
	case SBU_MSX_DEVICE_KEY_LOAD_STATUS_ON:
	case SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING:
	case SBU_MSX_DEVICE_KEY_ENABLE_BUZZER:
	case SBU_MSX_DEVICE_KEY_OVERLOAD_BYPASS_FUNCTION:
	case SBU_MSX_DEVICE_KEY_POWER_SAVE:
	case SBU_MSX_DEVICE_KEY_LCD_DISPLAY_ESCAPE:
	case SBU_MSX_DEVICE_KEY_OVERLOAD_RESTART:
	case SBU_MSX_DEVICE_KEY_OVER_TEMPERATURE_RESTART:
	case SBU_MSX_DEVICE_KEY_LCD_BACKLIGHT:
	case SBU_MSX_DEVICE_KEY_ALARM_PRIMARY_SOURCE_INTERRUPT:
	case SBU_MSX_DEVICE_KEY_FAULT_CODE_RECORD:
	case SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE_OFFSET_FOR_FANS:
	case SBU_MSX_DEVICE_KEY_EEPROM_VERSION:
	case SBU_MSX_DEVICE_KEY_CHARGING_TO_FLOATING_MODE:
	case SBU_MSX_DEVICE_KEY_SWITCH_ON:
		g_debug("key %s=%i not handled", sbu_device_key_to_string(key), value);
		break;
	default:
		g_warning("key %s=%i not handled", sbu_device_key_to_string(key), value);
		break;
	}

	/* save nearly every changed key until we have a stable API */
	switch (key) {
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_FREQUENCY:
	case SBU_MSX_DEVICE_KEY_AC_OUTPUT_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_BATTERY_CURRENT:
	case SBU_MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT:
	case SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC:
	case SBU_MSX_DEVICE_KEY_GRID_FREQUENCY:
	case SBU_MSX_DEVICE_KEY_GRID_RATING_VOLTAGE:
	case SBU_MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY:
		break;
	default:
		if (sbu_msx_device_get_value(device, key) != value) {
			sbu_plugin_update_metadata(SBU_PLUGIN(self),
						   SBU_DEVICE(device),
						   sbu_device_key_to_string(key),
						   value);
		}
		break;
	}
}

static void
sbu_msx_plugin_device_added_cb(GUsbContext *context, GUsbDevice *usb_device, SbuMsxPlugin *self)
{
	SbuMsxDevice *device;
	g_autoptr(GError) error = NULL;
	SbuNodeKind kinds[] = {SBU_NODE_KIND_SOLAR,
			       SBU_NODE_KIND_BATTERY,
			       SBU_NODE_KIND_UTILITY,
			       SBU_NODE_KIND_LOAD,
			       SBU_NODE_KIND_UNKNOWN};
	SbuNodeKind links[] = {SBU_NODE_KIND_SOLAR,
			       SBU_NODE_KIND_BATTERY,
			       SBU_NODE_KIND_SOLAR,
			       SBU_NODE_KIND_LOAD,
			       SBU_NODE_KIND_BATTERY,
			       SBU_NODE_KIND_LOAD,
			       SBU_NODE_KIND_UTILITY,
			       SBU_NODE_KIND_LOAD,
			       SBU_NODE_KIND_UTILITY,
			       SBU_NODE_KIND_BATTERY,
			       SBU_NODE_KIND_UNKNOWN,
			       SBU_NODE_KIND_UNKNOWN};

	if (g_usb_device_get_vid(usb_device) != 0x0665)
		return;
	if (g_usb_device_get_pid(usb_device) != 0x5161)
		return;

	/* create device */
	device = sbu_msx_device_new(usb_device);
	sbu_device_set_id(SBU_DEVICE(device), "msx");
	for (guint i = 0; kinds[i] != SBU_NODE_KIND_UNKNOWN; i++) {
		g_autoptr(SbuNode) n = sbu_node_new(kinds[i]);
		sbu_device_add_node(SBU_DEVICE(device), n);
	}
	for (guint i = 0; links[i] != SBU_NODE_KIND_UNKNOWN; i += 2) {
		g_autoptr(SbuLink) l = sbu_link_new(links[i], links[i + 1]);
		sbu_device_add_link(SBU_DEVICE(device), l);
	}

	/* open */
	g_signal_connect(device, "changed", G_CALLBACK(sbu_msx_device_changed_cb), self);
	if (!sbu_msx_device_open(device, &error)) {
		g_warning("failed to open: %s", error->message);
		return;
	}

	g_hash_table_insert(self->devices,
			    g_strdup(g_usb_device_get_platform_id(usb_device)),
			    g_object_ref(device));
	sbu_plugin_add_device(SBU_PLUGIN(self), SBU_DEVICE(device));
}

static void
sbu_msx_plugin_device_removed_cb(GUsbContext *context, GUsbDevice *usb_device, SbuMsxPlugin *self)
{
	SbuDevice *device;

	/* remove device */
	device = g_hash_table_lookup(self->devices, g_usb_device_get_platform_id(usb_device));
	if (device == NULL)
		return;
	g_debug("device removed: %s", sbu_device_get_serial_number(device));
	g_hash_table_remove(self->devices, device);
	sbu_plugin_remove_device(SBU_PLUGIN(self), device);
}

static gboolean
sbu_msx_plugin_setup(SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuMsxPlugin *self = SBU_MSX_PLUGIN(plugin);
	g_autoptr(GPtrArray) devices = NULL;

	/* get all the SBU devices */
	self->usb_context = g_usb_context_new(error);
	if (self->usb_context == NULL) {
		g_prefix_error(error, "failed to get USB context: ");
		return FALSE;
	}

	/* add all devices */
	devices = g_usb_context_get_devices(self->usb_context);
	for (guint i = 0; i < devices->len; i++) {
		GUsbDevice *usb_device = g_ptr_array_index(devices, i);
		sbu_msx_plugin_device_added_cb(self->usb_context, usb_device, self);
	}

	/* watch */
	g_signal_connect(self->usb_context,
			 "device-added",
			 G_CALLBACK(sbu_msx_plugin_device_added_cb),
			 self);
	g_signal_connect(self->usb_context,
			 "device-added",
			 G_CALLBACK(sbu_msx_plugin_device_removed_cb),
			 self);

	return TRUE;
}

static void
sbu_msx_plugin_finalize(GObject *object)
{
	SbuMsxPlugin *self = SBU_MSX_PLUGIN(object);

	if (self->usb_context != NULL)
		g_object_unref(self->usb_context);
	g_hash_table_unref(self->devices);

	G_OBJECT_CLASS(sbu_msx_plugin_parent_class)->finalize(object);
}

static void
sbu_msx_plugin_init(SbuMsxPlugin *self)
{
	self->devices =
	    g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_object_unref);
}

static void
sbu_msx_plugin_class_init(SbuMsxPluginClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	SbuPluginClass *plugin_class = SBU_PLUGIN_CLASS(object_class);
	object_class->finalize = sbu_msx_plugin_finalize;
	plugin_class->setup = sbu_msx_plugin_setup;
}
