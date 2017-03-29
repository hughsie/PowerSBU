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
#include "sbu-element-impl.h"

typedef struct _SbuDeviceImplClass	SbuDeviceImplClass;

struct _SbuDeviceImpl
{
	SbuDeviceSkeleton		 parent_instance;
	GDBusObjectManagerServer	*object_manager; /* no ref */
	const gchar			*object_path;
	GPtrArray			*elements;
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

SbuElementImpl *
sbu_device_impl_get_element_by_kind (SbuDeviceImpl *self, SbuElementKind kind)
{
	for (guint i = 0; i < self->elements->len; i++) {
		SbuElementKind kind_tmp;
		SbuElementImpl *element = g_ptr_array_index (self->elements, i);
		g_object_get (element, "kind", &kind_tmp, NULL);
		if (kind == kind_tmp)
			return element;
	}
	return NULL;
}

/* runs in thread dedicated to handling @invocation */
static gboolean
sbu_device_impl_get_elements (SbuDevice *_device,
			      GDBusMethodInvocation *invocation)
{
	SbuDeviceImpl *self = SBU_DEVICE_IMPL (_device);
	GVariantBuilder builder;

	g_debug ("handling GetElements");
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ao)"));
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("ao"));
	for (guint i = 0; i < self->elements->len; i++) {
		SbuElementImpl *element = g_ptr_array_index (self->elements, i);
		g_variant_builder_add (&builder, "o",
				       sbu_element_impl_get_object_path (element));
	}
	g_variant_builder_close (&builder);
	g_dbus_method_invocation_return_value (invocation,
					       g_variant_builder_end (&builder));
	return TRUE;
}

void
sbu_device_impl_add_element (SbuDeviceImpl *self, SbuElementImpl *element)
{
	g_ptr_array_add (self->elements, g_object_ref (element));
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

	/* and now each element */
	for (guint i = 0; i < self->elements->len; i++) {
		SbuElementImpl *element = g_ptr_array_index (self->elements, i);
		g_autoptr(SbuObjectSkeleton) element_object = NULL;
		element_object = sbu_object_skeleton_new (sbu_element_impl_get_object_path (element));
		g_debug ("exporting element %s", sbu_element_impl_get_object_path (element));
		sbu_object_skeleton_set_element (element_object, SBU_ELEMENT (element));
		g_dbus_object_manager_server_export (self->object_manager,
						     G_DBUS_OBJECT_SKELETON (element_object));
	}
}

static void
sbu_device_iface_init (SbuDeviceIface *iface)
{
	iface->handle_get_elements = sbu_device_impl_get_elements;
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

		/* set for all elements */
		for (guint i = 0; i < self->elements->len; i++) {
			SbuElement *element = g_ptr_array_index (self->elements, i);
			g_object_set (element,
				      "object-manager", self->object_manager,
				      NULL);
		}
		break;
	case PROP_OBJECT_PATH:
		g_assert (self->object_path == NULL);
		self->object_path = g_value_dup_string (value);

		/* set for all elements */
		for (guint i = 0; i < self->elements->len; i++) {
			SbuElement *element = g_ptr_array_index (self->elements, i);
			g_autofree gchar *object_path = NULL;
			object_path = g_strdup_printf ("%s/%u", self->object_path, i);
			g_object_set (element, "object-path", object_path, NULL);
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
	g_ptr_array_unref (self->elements);
	G_OBJECT_CLASS (sbu_device_impl_parent_class)->finalize (object);
}

static void
sbu_device_impl_init (SbuDeviceImpl *self)
{
	self->elements = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
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
