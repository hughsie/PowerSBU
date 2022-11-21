/*
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include "sbu-plugin.h"

#define SBU_TYPE_MSX_PLUGIN sbu_msx_plugin_get_type()
G_DECLARE_FINAL_TYPE(SbuMsxPlugin, sbu_msx_plugin, SBU, MSX_PLUGIN, SbuPlugin)
