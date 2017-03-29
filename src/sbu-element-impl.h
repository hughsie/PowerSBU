/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __SBU_ELEMENT_IMPL_H__
#define __SBU_ELEMENT_IMPL_H__

G_BEGIN_DECLS

#include <glib-object.h>

#include "generated-gdbus.h"

typedef struct _SbuElementImpl SbuElementImpl;

#define SBU_TYPE_ELEMENT_IMPL	(sbu_element_impl_get_type ())
#define SBU_ELEMENT_IMPL(o)	(G_TYPE_CHECK_INSTANCE_CAST ((o), SBU_TYPE_ELEMENT_IMPL, SbuElementImpl))
#define SBU_IS_ELEMENT_IMPL(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), SBU_TYPE_ELEMENT_IMPL))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuElement, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuElementImpl, g_object_unref)

GType		 sbu_element_impl_get_type		(void);
SbuElementImpl	*sbu_element_impl_new			(void);
const gchar	*sbu_element_impl_get_object_path	(SbuElementImpl	*self);

G_END_DECLS

#endif /* __SBU_ELEMENT_IMPL_H__ */
