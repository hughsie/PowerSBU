/*
 * Copyright (C) 2008-2012 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "egg-graph-point.h"

#include <glib.h>

EggGraphPoint *
egg_graph_point_copy(const EggGraphPoint *cobj)
{
	EggGraphPoint *obj;
	obj = g_new0(EggGraphPoint, 1);
	obj->x = cobj->x;
	obj->y = cobj->y;
	obj->color = cobj->color;
	return obj;
}

EggGraphPoint *
egg_graph_point_new(void)
{
	EggGraphPoint *obj;
	obj = g_new0(EggGraphPoint, 1);
	obj->x = 0.0f;
	obj->y = 0.0f;
	obj->color = 0x0;
	return obj;
}

void
egg_graph_point_free(EggGraphPoint *obj)
{
	if (obj == NULL)
		return;
	g_free(obj);
}
