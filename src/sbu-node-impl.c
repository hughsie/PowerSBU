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

#include "config.h"

#include <glib.h>

#include "sbu-node-impl.h"

typedef struct _SbuNodeImplClass	SbuNodeImplClass;

struct _SbuNodeImpl
{
	SbuNodeSkeleton			 parent_instance;
	GDBusObjectManagerServer	*object_manager; /* no ref */
	const gchar			*object_path;
};

struct _SbuNodeImplClass
{
	SbuNodeSkeletonClass		 parent_class;
};

enum
{
	PROP_0,
	PROP_OBJECT_MANAGER,
	PROP_OBJECT_PATH,
	PROP_LAST
};

static void sbu_node_iface_init (SbuNodeIface *iface);

G_DEFINE_TYPE_WITH_CODE (SbuNodeImpl, sbu_node_impl, SBU_TYPE_NODE_SKELETON,
			 G_IMPLEMENT_INTERFACE(SBU_TYPE_NODE, sbu_node_iface_init));

const gchar *
sbu_node_impl_get_object_path (SbuNodeImpl *self)
{
	return self->object_path;
}

static void
sbu_node_iface_init (SbuNodeIface *iface)
{
}

static void
sbu_node_impl_get_property (GObject *object,
			 guint prop_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	SbuNodeImpl *self = SBU_NODE_IMPL (object);

	switch (prop_id) {
	case PROP_OBJECT_MANAGER:
		g_value_set_object (value, self->object_manager);
		break;
	case PROP_OBJECT_PATH:
		g_value_set_string (value, self->object_path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sbu_node_impl_set_property (GObject *object,
			 guint prop_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	SbuNodeImpl *self = SBU_NODE_IMPL (object);

	switch (prop_id) {
	case PROP_OBJECT_MANAGER:
		g_assert (self->object_manager == NULL);
		self->object_manager = g_value_get_object (value);
		break;
	case PROP_OBJECT_PATH:
		g_assert (self->object_path == NULL);
		self->object_path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sbu_node_impl_finalize (GObject *object)
{
	SbuNodeImpl *self = SBU_NODE_IMPL (object);
	g_free (self->object_path);
	G_OBJECT_CLASS (sbu_node_impl_parent_class)->finalize (object);
}

static void
sbu_node_impl_init (SbuNodeImpl *self)
{
	g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (self),
					     G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
}

static void
sbu_node_impl_class_init (SbuNodeImplClass *klass_node)
{
	GObjectClass *klass;
	klass = G_OBJECT_CLASS (klass_node);
	klass->finalize = sbu_node_impl_finalize;
	klass->set_property = sbu_node_impl_set_property;
	klass->get_property = sbu_node_impl_get_property;

	g_object_class_install_property (klass,
					 PROP_OBJECT_MANAGER,
					 g_param_spec_object ("object-manager",
							      "GDBusObjectManager",
							      NULL,
							      G_TYPE_DBUS_OBJECT_MANAGER,
							      G_PARAM_READWRITE));
	g_object_class_install_property (klass,
					 PROP_OBJECT_PATH,
					 g_param_spec_string ("object-path",
							      "D-Bus Object Path",
							      NULL, NULL,
							      G_PARAM_READWRITE));
}

SbuNodeImpl *
sbu_node_impl_new (void)
{
	SbuNodeImpl *node;
	node = g_object_new (SBU_TYPE_NODE_IMPL, NULL);
	return SBU_NODE_IMPL (node);
}
