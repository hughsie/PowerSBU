/*
 * Copyright (C) 2012-2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>
#include <gmodule.h>

#include "sbu-common.h"
#include "sbu-device.h"

#define SBU_TYPE_PLUGIN sbu_plugin_get_type()
G_DECLARE_DERIVABLE_TYPE(SbuPlugin, sbu_plugin, SBU, PLUGIN, GObject)

struct _SbuPluginClass {
	GObjectClass parent_class;
	void (*update_metadata)(SbuPlugin *self, SbuDevice *device, const gchar *key, gint value);
	void (*add_device)(SbuPlugin *self, SbuDevice *device);
	void (*remove_device)(SbuPlugin *self, SbuDevice *device);
	gboolean (*setup)(SbuPlugin *self, GCancellable *cancellable, GError **error);
	gboolean (*refresh)(SbuPlugin *self, GCancellable *cancellable, GError **error);
};

const gchar *
sbu_plugin_get_name(SbuPlugin *self);
gboolean
sbu_plugin_get_enabled(SbuPlugin *self);
void
sbu_plugin_set_enabled(SbuPlugin *self, gboolean enabled);

void
sbu_plugin_update_metadata(SbuPlugin *self, SbuDevice *device, const gchar *key, gint value);
void
sbu_plugin_add_device(SbuPlugin *self, SbuDevice *device);
void
sbu_plugin_remove_device(SbuPlugin *self, SbuDevice *device);

gboolean
sbu_plugin_setup(SbuPlugin *self, GCancellable *cancellable, GError **error);
gboolean
sbu_plugin_refresh(SbuPlugin *self, GCancellable *cancellable, GError **error);
