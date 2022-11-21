/*
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#pragma once

#include <glib-object.h>

#define SBU_TYPE_MANAGER sbu_manager_get_type()
G_DECLARE_FINAL_TYPE(SbuManager, sbu_manager, SBU, MANAGER, GObject)

SbuManager *
sbu_manager_new(void);
gboolean
sbu_manager_setup(SbuManager *self, GError **error);
GPtrArray *
sbu_manager_get_devices(SbuManager *self);
SbuDevice *
sbu_manager_get_device_by_id(SbuManager *self, const gchar *device_id, GError **error);
GVariant *
sbu_manager_get_history(SbuManager *self,
			SbuDevice *device,
			const gchar *arg_key,
			guint64 arg_start,
			guint64 arg_end,
			guint limit,
			GError **error);
