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
	guint		 active_id;
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
	SbuNodeKind nodes[] = { SBU_NODE_KIND_SOLAR,
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

	/* create fake device */
	self->device = sbu_device_impl_new ();
	g_object_set (self->device,
		      "firmware-version", "123.456",
		      "description", "PIP-MSX",
		      "serial-number", "007",
		      NULL);

	/* add all the nodes */
	for (guint i = 0; nodes[i] != SBU_NODE_KIND_UNKNOWN; i++) {
		g_autoptr(SbuNodeImpl) node = sbu_node_impl_new (nodes[i]);
		g_object_set (node,
			      "voltage", 12.34f * nodes[i],
			      "voltage-max", 12.34f * nodes[i] * 10,
			      "current", 56.78f,
			      "current-max", 56.78f * 10,
			      "power", 87.65f,
			      "power-max", 87.65f * 10,
			      "frequency", 43.21f,
			      NULL);
		sbu_device_impl_add_node (self->device, node);
	}

	/* add all the links */
	for (guint i = 0; links[i] != SBU_NODE_KIND_UNKNOWN; i += 2) {
		g_autoptr(SbuLinkImpl) link = sbu_link_impl_new (links[i], links[i+1]);
		g_object_set (link, "active", TRUE, NULL);
		sbu_device_impl_add_link (self->device, link);
	}

	/* add the device */
	sbu_plugin_add_device (plugin, self->device);

	/* never again... */
	self->timeout_id = 0;
	return FALSE;
}

static gboolean
dummy_device_active_cb (gpointer user_data)
{
	SbuPlugin *plugin = SBU_PLUGIN (user_data);
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	gboolean active;

	active = sbu_device_impl_get_link_active (self->device,
						  SBU_NODE_KIND_SOLAR,
						  SBU_NODE_KIND_BATTERY);
	sbu_device_impl_set_link_active (self->device,
					 SBU_NODE_KIND_SOLAR,
					 SBU_NODE_KIND_BATTERY,
					 !active);
	return TRUE;
}

gboolean
sbu_plugin_refresh (SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	gdouble tmp;

	/* make the panel more volt-y */
	tmp = sbu_device_impl_get_node_value (self->device,
					      SBU_NODE_KIND_BATTERY,
					      SBU_DEVICE_PROPERTY_VOLTAGE);
	sbu_device_impl_set_node_value (self->device,
					SBU_NODE_KIND_BATTERY,
					SBU_DEVICE_PROPERTY_VOLTAGE,
					tmp + g_random_double_range (-.2f, .2f));

	/* make the utlity more powerful */
	tmp = sbu_device_impl_get_node_value (self->device,
					      SBU_NODE_KIND_UTILITY,
					      SBU_DEVICE_PROPERTY_POWER);
	sbu_device_impl_set_node_value (self->device,
					SBU_NODE_KIND_UTILITY,
					SBU_DEVICE_PROPERTY_POWER,
					tmp + g_random_double_range (-10.f, 10.f));

	/* save raw value */
	sbu_plugin_update_metadata (plugin, self->device, "TestKey", 123456);
	return TRUE;
}

gboolean
sbu_plugin_setup (SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	self->timeout_id = g_timeout_add_seconds (2, dummy_device_add_cb, plugin);
	self->active_id = g_timeout_add_seconds (5, dummy_device_active_cb, plugin);
	return TRUE;
}

void
sbu_plugin_destroy (SbuPlugin *plugin)
{
	SbuPluginData *self = sbu_plugin_get_data (plugin);
	g_object_unref (self->device);
	if (self->timeout_id != 0)
		g_source_remove (self->timeout_id);
	if (self->active_id != 0)
		g_source_remove (self->active_id);
}
