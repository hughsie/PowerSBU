/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#pragma once

#include "sbu-common.h"
#include "sbu-database.h"
#include "sbu-link.h"
#include "sbu-node.h"

#define SBU_TYPE_DEVICE sbu_device_get_type()
G_DECLARE_DERIVABLE_TYPE(SbuDevice, sbu_device, SBU, DEVICE, GObject)

struct _SbuDeviceClass {
	GObjectClass parent_class;
	gboolean (*refresh)(SbuDevice *device, GError **error);
};

SbuDevice *
sbu_device_new(void);
const gchar *
sbu_device_get_id(SbuDevice *self);
void
sbu_device_set_id(SbuDevice *self, const gchar *id);
const gchar *
sbu_device_get_firmware_version(SbuDevice *self);
void
sbu_device_set_firmware_version(SbuDevice *self, const gchar *firmware_version);
const gchar *
sbu_device_get_serial_number(SbuDevice *self);
void
sbu_device_set_serial_number(SbuDevice *self, const gchar *serial_number);
gboolean
sbu_device_refresh(SbuDevice *device, GError **error);

GPtrArray *
sbu_device_get_nodes(SbuDevice *self);
GPtrArray *
sbu_device_get_links(SbuDevice *self);

void
sbu_device_add_node(SbuDevice *self, SbuNode *node);
SbuNode *
sbu_device_get_node(SbuDevice *self, SbuNodeKind kind);
void
sbu_device_add_link(SbuDevice *self, SbuLink *link);
SbuLink *
sbu_device_get_link(SbuDevice *self, SbuNodeKind src, SbuNodeKind dst);

gdouble
sbu_device_get_node_value(SbuDevice *self, SbuNodeKind kind, SbuDeviceProperty key);
void
sbu_device_set_node_value(SbuDevice *self, SbuNodeKind kind, SbuDeviceProperty key, gdouble value);
gboolean
sbu_device_get_link_active(SbuDevice *self, SbuNodeKind src, SbuNodeKind dst);
void
sbu_device_set_link_active(SbuDevice *self, SbuNodeKind src, SbuNodeKind dst, gboolean value);

SbuDevice *
sbu_device_from_variant(GVariant *value);
GVariant *
sbu_device_to_variant(SbuDevice *self);
