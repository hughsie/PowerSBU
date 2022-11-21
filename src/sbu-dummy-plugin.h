/*
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include <sbu-plugin.h>

#define SBU_TYPE_DUMMY_PLUGIN sbu_dummy_plugin_get_type()
G_DECLARE_FINAL_TYPE(SbuDummyPlugin, sbu_dummy_plugin, SBU, DUMMY_PLUGIN, SbuPlugin)
