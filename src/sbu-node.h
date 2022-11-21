/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#pragma once

#include <glib-object.h>

#include "sbu-common.h"

#define SBU_TYPE_NODE sbu_node_get_type()
G_DECLARE_FINAL_TYPE(SbuNode, sbu_node, SBU, NODE, GObject)

SbuNode *
sbu_node_new(SbuNodeKind kind);
SbuNodeKind
sbu_node_get_kind(SbuNode *self);
const gchar *
sbu_node_get_id(SbuNode *self);
void
sbu_node_set_id(SbuNode *self, const gchar *id);

gboolean
sbu_node_get_value(SbuNode *self, SbuDeviceProperty kind, gdouble *value);
void
sbu_node_set_value(SbuNode *self, SbuDeviceProperty kind, gdouble value);

gdouble
sbu_node_get_voltage(SbuNode *self);
gdouble
sbu_node_get_voltage_max(SbuNode *self);
gdouble
sbu_node_get_power(SbuNode *self);
gdouble
sbu_node_get_power_max(SbuNode *self);

SbuNode *
sbu_node_from_variant(GVariant *value);
GVariant *
sbu_node_to_variant(SbuNode *self);
