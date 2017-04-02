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

#include "sbu-link-impl.h"

typedef struct _SbuLinkImplClass	SbuLinkImplClass;

struct _SbuLinkImpl
{
	SbuLinkSkeleton			 parent_instance;
	GDBusObjectManagerServer	*object_manager; /* no ref */
	const gchar			*object_path;
};

struct _SbuLinkImplClass
{
	SbuLinkSkeletonClass		 parent_class;
};

enum
{
	PROP_0,
	PROP_OBJECT_MANAGER,
	PROP_OBJECT_PATH,
	PROP_LAST
};

static void sbu_link_iface_init (SbuLinkIface *iface);

G_DEFINE_TYPE_WITH_CODE (SbuLinkImpl, sbu_link_impl, SBU_TYPE_LINK_SKELETON,
			 G_IMPLEMENT_INTERFACE(SBU_TYPE_LINK, sbu_link_iface_init));

const gchar *
sbu_link_impl_get_object_path (SbuLinkImpl *self)
{
	return self->object_path;
}

static void
sbu_link_iface_init (SbuLinkIface *iface)
{
}

static void
sbu_link_impl_get_property (GObject *object,
			 guint prop_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	SbuLinkImpl *self = SBU_LINK_IMPL (object);

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
sbu_link_impl_set_property (GObject *object,
			 guint prop_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	SbuLinkImpl *self = SBU_LINK_IMPL (object);

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
sbu_link_impl_finalize (GObject *object)
{
	SbuLinkImpl *self = SBU_LINK_IMPL (object);
	g_free (self->object_path);
	G_OBJECT_CLASS (sbu_link_impl_parent_class)->finalize (object);
}

static void
sbu_link_impl_init (SbuLinkImpl *self)
{
	g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (self),
					     G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
}

static void
sbu_link_impl_class_init (SbuLinkImplClass *klass_link)
{
	GObjectClass *klass;
	klass = G_OBJECT_CLASS (klass_link);
	klass->finalize = sbu_link_impl_finalize;
	klass->set_property = sbu_link_impl_set_property;
	klass->get_property = sbu_link_impl_get_property;

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

SbuLinkImpl *
sbu_link_impl_new (void)
{
	SbuLinkImpl *link;
	link = g_object_new (SBU_TYPE_LINK_IMPL, NULL);
	return SBU_LINK_IMPL (link);
}
