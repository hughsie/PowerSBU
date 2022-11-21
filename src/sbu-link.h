/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#pragma once

#include <glib-object.h>

#include "sbu-common.h"

#define SBU_TYPE_LINK sbu_link_get_type()
G_DECLARE_FINAL_TYPE(SbuLink, sbu_link, SBU, LINK, GObject)

SbuLink *
sbu_link_new(SbuNodeKind src, SbuNodeKind dst);
const gchar *
sbu_link_get_id(SbuLink *self);
SbuNodeKind
sbu_link_get_src(SbuLink *self);
SbuNodeKind
sbu_link_get_dst(SbuLink *self);
gboolean
sbu_link_get_active(SbuLink *self);
void
sbu_link_set_active(SbuLink *self, gboolean active);

SbuLink *
sbu_link_from_variant(GVariant *value);
GVariant *
sbu_link_to_variant(SbuLink *self);
