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

#include "sbu-device-impl.h"
#include "sbu-node-impl.h"

typedef struct _SbuDeviceImplClass	SbuDeviceImplClass;

struct _SbuDeviceImpl
{
	SbuDeviceSkeleton		 parent_instance;
	GDBusObjectManagerServer	*object_manager; /* no ref */
	gchar				*object_path;
	GPtrArray			*nodes;
	GPtrArray			*links;
};

struct _SbuDeviceImplClass
{
	SbuDeviceSkeletonClass	 parent_class;
};

enum
{
	PROP_0,
	PROP_OBJECT_MANAGER,
	PROP_OBJECT_PATH,
	PROP_LAST
};

static void sbu_device_iface_init (SbuDeviceIface *iface);

G_DEFINE_TYPE_WITH_CODE (SbuDeviceImpl, sbu_device_impl, SBU_TYPE_DEVICE_SKELETON,
			 G_IMPLEMENT_INTERFACE(SBU_TYPE_DEVICE, sbu_device_iface_init));

SbuNodeImpl *
sbu_device_impl_get_node (SbuDeviceImpl *self, SbuNodeKind kind)
{
	for (guint i = 0; i < self->nodes->len; i++) {
		SbuNodeKind kind_tmp;
		SbuNodeImpl *node = g_ptr_array_index (self->nodes, i);
		g_object_get (node, "kind", &kind_tmp, NULL);
		if (kind == kind_tmp)
			return node;
	}
	return NULL;
}

SbuLinkImpl *
sbu_device_impl_get_link (SbuDeviceImpl *self, SbuNodeKind src, SbuNodeKind dst)
{
	for (guint i = 0; i < self->links->len; i++) {
		SbuNodeKind src_tmp;
		SbuNodeKind dst_tmp;
		SbuLinkImpl *link = g_ptr_array_index (self->links, i);
		g_object_get (link,
			      "src", &src_tmp,
			      "dst", &dst_tmp,
			      NULL);
		if (src == src_tmp && dst == dst_tmp)
			return link;
	}
	return NULL;
}

/* runs in thread dedicated to handling @invocation */
static gboolean
sbu_device_impl_get_nodes (SbuDevice *_device,
			      GDBusMethodInvocation *invocation)
{
	SbuDeviceImpl *self = SBU_DEVICE_IMPL (_device);
	GVariantBuilder builder;

	g_debug ("handling GetNodes");
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ao)"));
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("ao"));
	for (guint i = 0; i < self->nodes->len; i++) {
		SbuNodeImpl *node = g_ptr_array_index (self->nodes, i);
		g_variant_builder_add (&builder, "o",
				       sbu_node_impl_get_object_path (node));
	}
	g_variant_builder_close (&builder);
	g_dbus_method_invocation_return_value (invocation,
					       g_variant_builder_end (&builder));
	return TRUE;
}

/* runs in thread dedicated to handling @invocation */
static gboolean
sbu_device_impl_get_links (SbuDevice *_device,
			      GDBusMethodInvocation *invocation)
{
	SbuDeviceImpl *self = SBU_DEVICE_IMPL (_device);
	GVariantBuilder builder;

	g_debug ("handling GetLinks");
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ao)"));
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("ao"));
	for (guint i = 0; i < self->links->len; i++) {
		SbuLinkImpl *link = g_ptr_array_index (self->links, i);
		g_variant_builder_add (&builder, "o",
				       sbu_link_impl_get_object_path (link));
	}
	g_variant_builder_close (&builder);
	g_dbus_method_invocation_return_value (invocation,
					       g_variant_builder_end (&builder));
	return TRUE;
}

void
sbu_device_impl_add_node (SbuDeviceImpl *self, SbuNodeImpl *node)
{
	g_ptr_array_add (self->nodes, g_object_ref (node));
}

void
sbu_device_impl_add_link (SbuDeviceImpl *self, SbuLinkImpl *link)
{
	g_ptr_array_add (self->links, g_object_ref (link));
}

const gchar *
sbu_device_impl_get_object_path (SbuDeviceImpl *self)
{
	return self->object_path;
}

void
sbu_device_impl_export (SbuDeviceImpl *self)
{
	g_autoptr(SbuObjectSkeleton) device_object = NULL;

	/* export the device */
	g_debug ("exporting device %s", self->object_path);
	device_object = sbu_object_skeleton_new (self->object_path);
	sbu_object_skeleton_set_device (device_object, SBU_DEVICE (self));
	g_dbus_object_manager_server_export (self->object_manager,
					     G_DBUS_OBJECT_SKELETON (device_object));

	/* and now each node */
	for (guint i = 0; i < self->nodes->len; i++) {
		SbuNodeImpl *node = g_ptr_array_index (self->nodes, i);
		g_autoptr(SbuObjectSkeleton) node_object = NULL;
		node_object = sbu_object_skeleton_new (sbu_node_impl_get_object_path (node));
		g_debug ("exporting node %s", sbu_node_impl_get_object_path (node));
		sbu_object_skeleton_set_node (node_object, SBU_NODE (node));
		g_dbus_object_manager_server_export (self->object_manager,
						     G_DBUS_OBJECT_SKELETON (node_object));
	}

	/* and now each link */
	for (guint i = 0; i < self->links->len; i++) {
		SbuLinkImpl *link = g_ptr_array_index (self->links, i);
		g_autoptr(SbuObjectSkeleton) link_object = NULL;
		link_object = sbu_object_skeleton_new (sbu_link_impl_get_object_path (link));
		g_debug ("exporting link %s", sbu_link_impl_get_object_path (link));
		sbu_object_skeleton_set_link (link_object, SBU_LINK (link));
		g_dbus_object_manager_server_export (self->object_manager,
						     G_DBUS_OBJECT_SKELETON (link_object));
	}
}

static void
sbu_device_iface_init (SbuDeviceIface *iface)
{
	iface->handle_get_nodes = sbu_device_impl_get_nodes;
	iface->handle_get_links = sbu_device_impl_get_links;
}

static void
sbu_device_impl_get_property (GObject *object,
			      guint prop_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	SbuDeviceImpl *self = SBU_DEVICE_IMPL (object);

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
sbu_device_impl_set_property (GObject *object,
			      guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	SbuDeviceImpl *self = SBU_DEVICE_IMPL (object);

	switch (prop_id) {
	case PROP_OBJECT_MANAGER:
		g_assert (self->object_manager == NULL);
		self->object_manager = g_value_get_object (value);

		/* set for all links and nodes */
		for (guint i = 0; i < self->nodes->len; i++) {
			SbuNode *node = g_ptr_array_index (self->nodes, i);
			g_object_set (node,
				      "object-manager", self->object_manager,
				      NULL);
		}
		for (guint i = 0; i < self->links->len; i++) {
			SbuLink *link = g_ptr_array_index (self->links, i);
			g_object_set (link,
				      "object-manager", self->object_manager,
				      NULL);
		}
		break;
	case PROP_OBJECT_PATH:
		g_assert (self->object_path == NULL);
		self->object_path = g_value_dup_string (value);

		/* set for all nodes and links */
		for (guint i = 0; i < self->nodes->len; i++) {
			SbuNode *node = g_ptr_array_index (self->nodes, i);
			g_autofree gchar *object_path = NULL;
			object_path = g_strdup_printf ("%s/node_%u", self->object_path, i);
			g_object_set (node, "object-path", object_path, NULL);
		}
		for (guint i = 0; i < self->links->len; i++) {
			SbuLink *link = g_ptr_array_index (self->links, i);
			g_autofree gchar *object_path = NULL;
			object_path = g_strdup_printf ("%s/link_%u", self->object_path, i);
			g_object_set (link, "object-path", object_path, NULL);
		}

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sbu_device_impl_finalize (GObject *object)
{
	SbuDeviceImpl *self = SBU_DEVICE_IMPL (object);
	g_free (self->object_path);
	g_ptr_array_unref (self->nodes);
	g_ptr_array_unref (self->links);
	G_OBJECT_CLASS (sbu_device_impl_parent_class)->finalize (object);
}

static void
sbu_device_impl_init (SbuDeviceImpl *self)
{
	self->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	self->links = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (self),
					     G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
}

static void
sbu_device_impl_class_init (SbuDeviceImplClass *klass_device)
{
	GObjectClass *klass;
	klass = G_OBJECT_CLASS (klass_device);
	klass->finalize = sbu_device_impl_finalize;
	klass->set_property = sbu_device_impl_set_property;
	klass->get_property = sbu_device_impl_get_property;

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

SbuDeviceImpl *
sbu_device_impl_new (void)
{
	SbuDeviceImpl *device;
	device = g_object_new (SBU_TYPE_DEVICE_IMPL, NULL);
	return SBU_DEVICE_IMPL (device);
}
