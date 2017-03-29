/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012-2017 Richard Hughes <richard@hughsie.com>
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

#ifndef __SBU_MANAGER_IMPL_H__
#define __SBU_MANAGER_IMPL_H__

G_BEGIN_DECLS

#include <glib-object.h>

#include "generated-gdbus.h"

#include "sbu-database.h"

typedef struct _SbuManagerImpl SbuManagerImpl;

#define SBU_TYPE_MANAGER_IMPL	(sbu_manager_impl_get_type ())
#define SBU_MANAGER_IMPL(o)	(G_TYPE_CHECK_INSTANCE_CAST ((o), SBU_TYPE_MANAGER_IMPL, SbuManagerImpl))
#define SBU_IS_MANAGER_IMPL(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), SBU_TYPE_MANAGER_IMPL))

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuManager, g_object_unref)

GType		 sbu_manager_impl_get_type		(void);
SbuManagerImpl	*sbu_manager_impl_new			(GDBusObjectManagerServer *object_manager);
void		 sbu_manager_impl_start			(SbuManagerImpl	*self);
gboolean	 sbu_manager_impl_setup			(SbuManagerImpl	*self,
							 GError		**error);

G_END_DECLS

#endif /* __SBU_MANAGER_IMPL_H__ */
