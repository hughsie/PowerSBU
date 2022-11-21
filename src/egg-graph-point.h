/*
 * Copyright (C) 2008-2012 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	gdouble x;
	gdouble y;
	guint32 color;
} EggGraphPoint;

EggGraphPoint *
egg_graph_point_new(void);
EggGraphPoint *
egg_graph_point_copy(const EggGraphPoint *cobj);
void
egg_graph_point_free(EggGraphPoint *obj);
