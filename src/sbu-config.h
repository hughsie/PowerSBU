/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include <glib-object.h>

#define SBU_TYPE_CONFIG sbu_config_get_type()
G_DECLARE_FINAL_TYPE(SbuConfig, sbu_config, SBU, CONFIG, GObject)

SbuConfig *
sbu_config_new(void);
gchar *
sbu_config_get_string(SbuConfig *self, const gchar *key, GError **error);
gint
sbu_config_get_integer(SbuConfig *self, const gchar *key, GError **error);
gboolean
sbu_config_get_boolean(SbuConfig *self, const gchar *key, GError **error);
