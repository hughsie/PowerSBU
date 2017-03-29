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
msx_device_changed_cb (MsxDevice *msx_device,
		       const gchar *key,
		       gint value,
		       SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	SbuElementImpl *el = NULL;
	SbuDeviceImpl *device = NULL;

	device = g_hash_table_lookup (self->devices, msx_device);
	g_assert (device != NULL);

	/* battery */
	if (g_strcmp0 (key, "BatteryVoltage") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_BATTERY);
		g_object_set (el, "voltage", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "BatteryFloatVoltage") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_BATTERY);
		g_object_set (el, "voltage-max", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "PresentMaxChangingCurrent") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_BATTERY);
		g_object_set (el, "current-max", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "BatteryCurrent") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_BATTERY);
		g_object_set (el,
			      "current", -msx_val_to_double (value),
			      "power", -msx_vals_to_double (value, msx_device_get_value (msx_device, "BatteryVoltage")),
			      NULL);
	} else if (g_strcmp0 (key, "BatteryDischargeCurrent") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_BATTERY);
		g_object_set (el,
			      "current", msx_val_to_double (value),
			      "power", msx_vals_to_double (value, msx_device_get_value (msx_device, "BatteryVoltage")),
			      NULL);

	/* utility */
	} else if (g_strcmp0 (key, "GridVoltage") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_UTILITY);
		g_object_set (el, "voltage", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "GridRatingCurrent") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_UTILITY);
		g_object_set (el, "current-max", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "GridRatingVoltage") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_UTILITY);
		g_object_set (el, "voltage-max", msx_val_to_double (value), NULL);

	/* solar */
	} else if (g_strcmp0 (key, "BatteryVoltageFromScc") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_SOLAR);
		g_object_set (el,
			      "voltage", msx_val_to_double (value),
			      "power", msx_vals_to_double (value, msx_device_get_value (msx_device, "PvInputCurrentForBattery")),
			      NULL);
	} else if (g_strcmp0 (key, "PvInputCurrentForBattery") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_SOLAR);
		g_object_set (el,
			      "current", msx_val_to_double (value),
			      "power", msx_vals_to_double (value, msx_device_get_value (msx_device, "BatteryVoltageFromScc")),
			      NULL);

	/* load */
	} else if (g_strcmp0 (key, "AcOutputActivePower") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_LOAD);
		g_object_set (el, "power", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "AcOutputColtage") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_LOAD);
		g_object_set (el, "voltage", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "AcOutputRatingActivePower") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_LOAD);
		g_object_set (el, "power-max", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "AcOutputRatingVoltage") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_LOAD);
		g_object_set (el, "voltage-max", msx_val_to_double (value), NULL);
	} else if (g_strcmp0 (key, "AcOutputFrequency") == 0) {
		el = sbu_device_impl_get_element_by_kind (device, SBU_ELEMENT_KIND_LOAD);
		g_object_set (el, "frequency", msx_val_to_double (value), NULL);

	/* unknown */
	} else {
		g_debug ("not handled: %s=%i", key, value);
	}

	/* save every key until we have a stable database API */
	sbu_plugin_update_metadata (plugin, device, key, value);
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
	SbuElementKind kinds[] = { SBU_ELEMENT_KIND_SOLAR,
				   SBU_ELEMENT_KIND_BATTERY,
				   SBU_ELEMENT_KIND_UTILITY,
				   SBU_ELEMENT_KIND_LOAD,
				   SBU_ELEMENT_KIND_UNKNOWN };

	/* create device */
	device = sbu_device_impl_new ();
	for (guint i = 0; kinds[i] != SBU_ELEMENT_KIND_UNKNOWN; i++) {
		g_autoptr(SbuElementImpl) element = sbu_element_impl_new ();
		g_object_set (element, "kind", kinds[i], NULL);
		sbu_device_impl_add_element (device, element);
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
	firmware_version = g_strdup_printf ("%s, %s",
					    msx_device_get_firmware_version1 (msx_device),
					    msx_device_get_firmware_version2 (msx_device));
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
