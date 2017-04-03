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

#ifndef __SBU_DEVICE_IMPL_H__
#define __SBU_DEVICE_IMPL_H__

G_BEGIN_DECLS

#include <glib-object.h>

#include "generated-gdbus.h"

#include "sbu-common.h"
#include "sbu-node-impl.h"
#include "sbu-link-impl.h"

typedef struct _SbuDeviceImpl SbuDeviceImpl;

#define SBU_TYPE_DEVICE_IMPL	(sbu_device_impl_get_type ())
#define SBU_DEVICE_IMPL(o)	(G_TYPE_CHECK_INSTANCE_CAST ((o), SBU_TYPE_DEVICE_IMPL, SbuDeviceImpl))
#define SBU_IS_DEVICE_IMPL(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), SBU_TYPE_DEVICE_IMPL))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuDeviceImpl, g_object_unref)

GType		 sbu_device_impl_get_type		(void);
SbuDeviceImpl	*sbu_device_impl_new			(void);
const gchar	*sbu_device_impl_get_object_path	(SbuDeviceImpl	*self);
void		 sbu_device_impl_export			(SbuDeviceImpl	*self);
void		 sbu_device_impl_unexport		(SbuDeviceImpl	*self);

void		 sbu_device_impl_add_node		(SbuDeviceImpl	*self,
							 SbuNodeImpl	*node);
SbuNodeImpl	*sbu_device_impl_get_node		(SbuDeviceImpl	*self,
							 SbuNodeKind	 kind);
void		 sbu_device_impl_add_link		(SbuDeviceImpl	*self,
							 SbuLinkImpl	*link);
SbuLinkImpl	*sbu_device_impl_get_link		(SbuDeviceImpl	*self,
							 SbuNodeKind	 src,
							 SbuNodeKind	 dst);

gdouble		 sbu_device_impl_get_node_value		(SbuDeviceImpl	*self,
							 SbuNodeKind	 kind,
							 SbuDeviceProperty key);
void		 sbu_device_impl_set_node_value		(SbuDeviceImpl	*self,
							 SbuNodeKind	 kind,
							 SbuDeviceProperty key,
							 gdouble	 value);
gboolean	 sbu_device_impl_get_link_active	(SbuDeviceImpl	*self,
							 SbuNodeKind	 src,
							 SbuNodeKind	 dst);
void		 sbu_device_impl_set_link_active	(SbuDeviceImpl	*self,
							 SbuNodeKind	 src,
							 SbuNodeKind	 dst,
							 gboolean	 value);
G_END_DECLS

#endif /* __SBU_DEVICE_IMPL_H__ */
