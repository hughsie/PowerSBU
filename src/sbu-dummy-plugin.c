/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "sbu-dummy-plugin.h"

#include <config.h>

struct _SbuDummyPlugin {
	SbuPlugin parent_instance;
	guint timeout_id;
	guint active_id;
	SbuDevice *device;
};

G_DEFINE_TYPE(SbuDummyPlugin, sbu_dummy_plugin, SBU_TYPE_PLUGIN)

static gboolean
dummy_device_add_cb(gpointer user_data)
{
	SbuPlugin *plugin = SBU_PLUGIN(user_data);
	SbuDummyPlugin *self = SBU_DUMMY_PLUGIN(plugin);
	SbuNodeKind nodes[] = {SBU_NODE_KIND_SOLAR,
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

	/* create fake device */
	self->device = sbu_device_new();
	sbu_device_set_id(self->device, "dummy");
	sbu_device_set_firmware_version(self->device, "123.456");
	sbu_device_set_serial_number(self->device, "007");

	/* add all the nodes */
	for (guint i = 0; nodes[i] != SBU_NODE_KIND_UNKNOWN; i++) {
		g_autoptr(SbuNode) n = sbu_node_new(nodes[i]);
		sbu_node_set_value(n, SBU_DEVICE_PROPERTY_VOLTAGE, 12.34f * nodes[i]);
		sbu_node_set_value(n, SBU_DEVICE_PROPERTY_VOLTAGE_MAX, 12.34f * nodes[i] * 10);
		sbu_node_set_value(n, SBU_DEVICE_PROPERTY_CURRENT, 56.78f);
		sbu_node_set_value(n, SBU_DEVICE_PROPERTY_CURRENT_MAX, 56.78f * 10);
		sbu_node_set_value(n, SBU_DEVICE_PROPERTY_POWER, 87.65f);
		sbu_node_set_value(n, SBU_DEVICE_PROPERTY_POWER_MAX, 87.65f * 10);
		sbu_node_set_value(n, SBU_DEVICE_PROPERTY_FREQUENCY, 43.21f);
		sbu_device_add_node(self->device, n);
	}

	/* add all the links */
	for (guint i = 0; links[i] != SBU_NODE_KIND_UNKNOWN; i += 2) {
		g_autoptr(SbuLink) link = sbu_link_new(links[i], links[i + 1]);
		sbu_link_set_active(link, TRUE);
		sbu_device_add_link(self->device, link);
	}

	/* add the device */
	sbu_plugin_add_device(plugin, self->device);

	/* never again... */
	self->timeout_id = 0;
	return FALSE;
}

static gboolean
dummy_device_active_cb(gpointer user_data)
{
	SbuPlugin *plugin = SBU_PLUGIN(user_data);
	SbuDummyPlugin *self = SBU_DUMMY_PLUGIN(plugin);
	gboolean active;

	active =
	    sbu_device_get_link_active(self->device, SBU_NODE_KIND_SOLAR, SBU_NODE_KIND_BATTERY);
	sbu_device_set_link_active(self->device,
				   SBU_NODE_KIND_SOLAR,
				   SBU_NODE_KIND_BATTERY,
				   !active);
	return TRUE;
}

static gboolean
sbu_dummy_plugin_refresh(SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuDummyPlugin *self = SBU_DUMMY_PLUGIN(plugin);
	gdouble tmp;

	/* make the panel more volt-y */
	tmp = sbu_device_get_node_value(self->device,
					SBU_NODE_KIND_BATTERY,
					SBU_DEVICE_PROPERTY_VOLTAGE);
	sbu_device_set_node_value(self->device,
				  SBU_NODE_KIND_BATTERY,
				  SBU_DEVICE_PROPERTY_VOLTAGE,
				  tmp + g_random_double_range(-.2f, .2f));

	/* make the utlity more powerful */
	tmp = sbu_device_get_node_value(self->device,
					SBU_NODE_KIND_UTILITY,
					SBU_DEVICE_PROPERTY_POWER);
	sbu_device_set_node_value(self->device,
				  SBU_NODE_KIND_UTILITY,
				  SBU_DEVICE_PROPERTY_POWER,
				  tmp + g_random_double_range(-10.f, 10.f));

	/* save raw value */
	sbu_plugin_update_metadata(plugin, self->device, "TestKey", 123456);
	return TRUE;
}

static gboolean
sbu_dummy_plugin_setup(SbuPlugin *plugin, GCancellable *cancellable, GError **error)
{
	SbuDummyPlugin *self = SBU_DUMMY_PLUGIN(plugin);
	self->timeout_id = g_timeout_add_seconds(2, dummy_device_add_cb, plugin);
	self->active_id = g_timeout_add_seconds(5, dummy_device_active_cb, plugin);
	return TRUE;
}

static void
sbu_dummy_plugin_finalize(GObject *object)
{
	SbuDummyPlugin *self = SBU_DUMMY_PLUGIN(object);

	if (self->device != NULL)
		g_object_unref(self->device);
	if (self->timeout_id != 0)
		g_source_remove(self->timeout_id);
	if (self->active_id != 0)
		g_source_remove(self->active_id);

	G_OBJECT_CLASS(sbu_dummy_plugin_parent_class)->finalize(object);
}

static void
sbu_dummy_plugin_init(SbuDummyPlugin *self)
{
	SbuPlugin *plugin = SBU_PLUGIN(self);
	if (g_getenv("SBU_DUMMY_ENABLE") == NULL) {
		g_debug("disabling '%s' as not testing", sbu_plugin_get_name(plugin));
		sbu_plugin_set_enabled(plugin, FALSE);
		return;
	}
}

static void
sbu_dummy_plugin_class_init(SbuDummyPluginClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	SbuPluginClass *plugin_class = SBU_PLUGIN_CLASS(class);
	plugin_class->setup = sbu_dummy_plugin_setup;
	plugin_class->refresh = sbu_dummy_plugin_refresh;
	object_class->finalize = sbu_dummy_plugin_finalize;
}
