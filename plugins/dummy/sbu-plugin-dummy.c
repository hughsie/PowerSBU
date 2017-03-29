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

struct SbuPluginData {
	guint		 timeout_id;
	SbuDeviceImpl	*device;
};

void
sbu_plugin_initialize (SbuPlugin *plugin)
{
	sbu_plugin_alloc_data (plugin, sizeof(SbuPluginData));

	if (g_getenv ("SBU_DUMMY_ENABLE") == NULL) {
		g_debug ("disabling '%s' as not testing",
			 sbu_plugin_get_name (plugin));
		sbu_plugin_set_enabled (plugin, FALSE);
		return;
	}
}

static gboolean
dummy_device_add_cb (gpointer user_data)
{
	SbuPlugin *plugin = SBU_PLUGIN (user_data);
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	SbuElementKind kinds[] = { SBU_ELEMENT_KIND_SOLAR,
				   SBU_ELEMENT_KIND_BATTERY,
				   SBU_ELEMENT_KIND_UTILITY,
				   SBU_ELEMENT_KIND_LOAD,
				   SBU_ELEMENT_KIND_UNKNOWN };

	/* create fake device */
	self->device = sbu_device_impl_new ();
	g_object_set (self->device,
		      "firmware-version", "123.456",
		      "description", "PIP-MSX",
		      "serial-number", "007",
		      NULL);

	/* add all the elements */
	for (guint i = 0; kinds[i] != SBU_ELEMENT_KIND_UNKNOWN; i++) {
		g_autoptr(SbuElementImpl) element = sbu_element_impl_new ();
		g_object_set (element,
			      "kind", kinds[i],
			      "voltage", 12.34f * kinds[i],
			      "voltage-max", 12.34f * kinds[i] * 10,
			      "current", 56.78f,
			      "current-max", 56.78f * 10,
			      "power", 87.65f,
			      "power-max", 87.65f * 10,
			      "frequency", 43.21f,
			      NULL);
		sbu_device_impl_add_element (self->device, element);
	}

	/* add the device */
	sbu_plugin_add_device (plugin, self->device);

	/* never again... */
	self->timeout_id = 0;
	return FALSE;
}

gboolean
sbu_plugin_refresh (SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	SbuElementImpl *element = NULL;
	gdouble val;

	/* make the panel more volt-y */
	element = sbu_device_impl_get_element_by_kind (self->device, SBU_ELEMENT_KIND_BATTERY);
	g_assert (element != NULL);
	g_object_get (element, "voltage", &val, NULL);
	g_object_set (element, "voltage", val + 10, NULL);

	/* make the utlity more powerful */
	element = sbu_device_impl_get_element_by_kind (self->device, SBU_ELEMENT_KIND_UTILITY);
	g_assert (element != NULL);
	g_object_get (element, "power", &val, NULL);
	g_object_set (element, "power", val + 100, NULL);

	/* save raw value */
	sbu_plugin_update_metadata (plugin, self->device, "TestKey", 123456);
	return TRUE;
}

gboolean
sbu_plugin_setup (SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	self->timeout_id = g_timeout_add_seconds (2, dummy_device_add_cb, plugin);
	return TRUE;
}

void
sbu_plugin_destroy (SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	g_object_unref (self->device);
	if (self->timeout_id != 0)
		g_source_remove (self->timeout_id);
}
