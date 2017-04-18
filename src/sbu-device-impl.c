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
#include <math.h>
#include <string.h>

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
	SbuDatabase			*database;
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

GPtrArray *
sbu_device_impl_get_node_array (SbuDeviceImpl *self)
{
	return self->nodes;
}

GPtrArray *
sbu_device_impl_get_link_array (SbuDeviceImpl *self)
{
	return self->links;
}

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

gdouble
sbu_device_impl_get_node_value (SbuDeviceImpl *self,
				SbuNodeKind kind,
				SbuDeviceProperty key)
{
	SbuNodeImpl *n;
	gdouble val;
	n = sbu_device_impl_get_node (self, kind);
	if (n == NULL)
		return 0.f;
	g_object_get (n, sbu_device_property_to_string (key), &val, NULL);
	return val;
}

void
sbu_device_impl_set_node_value (SbuDeviceImpl *self,
				SbuNodeKind kind,
				SbuDeviceProperty key,
				gdouble value)
{
	SbuNodeImpl *n = sbu_device_impl_get_node (self, kind);
	if (n == NULL)
		return;
	g_object_set (n, sbu_device_property_to_string (key), value, NULL);
}

gboolean
sbu_device_impl_get_link_active (SbuDeviceImpl *self,
				 SbuNodeKind src,
				 SbuNodeKind dst)
{
	gboolean value;
	SbuLinkImpl *l = sbu_device_impl_get_link (self, src, dst);
	if (l == NULL)
		return FALSE;
	g_object_get (l, "active", &value, NULL);
	return value;
}

void
sbu_device_impl_set_link_active (SbuDeviceImpl *self,
				 SbuNodeKind src,
				 SbuNodeKind dst,
				 gboolean value)
{
	SbuLinkImpl *l = sbu_device_impl_get_link (self, src, dst);
	if (l == NULL)
		return;
	g_object_set (l, "active", value, NULL);

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

/* runs in thread dedicated to handling @invocation */
static gboolean
sbu_device_impl_get_history (SbuDevice *_device,
			     GDBusMethodInvocation *invocation,
			     const gchar *arg_key,
			     guint64 arg_start,
			     guint64 arg_end,
			     guint limit)
{
	SbuDeviceImpl *self = SBU_DEVICE_IMPL (_device);
	GVariantBuilder builder;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GPtrArray) results2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) key = g_string_new (NULL);
	const gchar *device_id_suffix = self->object_path;

	/* sanity check */
	if (self->database == NULL) {
		g_dbus_method_invocation_return_error (invocation,
						       G_IO_ERROR,
						       G_IO_ERROR_FAILED,
						       "no database to use");
		return FALSE;
	}

	/* clients can query raw keys or those with a prefix */
	if (g_strstr_len (arg_key, -1, ":") != NULL) {
		if (g_str_has_prefix (device_id_suffix, SBU_DBUS_PATH_DEVICE))
			device_id_suffix += strlen (SBU_DBUS_PATH_DEVICE);
		g_string_printf (key, "%s/%s", device_id_suffix, arg_key);
	} else {
		g_string_assign (key, arg_key);
	}

	/* get all results between the two times */
	g_debug ("handling GetHistory %s for %" G_GUINT64_FORMAT
		 "->%" G_GUINT64_FORMAT, key->str, arg_start, arg_end);
	results = sbu_database_query (self->database,
				      key->str,
				      SBU_DEVICE_ID_DEFAULT,
				      arg_start,
				      arg_end,
				      &error);
	if (results == NULL) {
		g_dbus_method_invocation_return_gerror (invocation, error);
		return FALSE;
	}

	/* no filter */
	if (limit == 0) {
		results2 = g_ptr_array_ref (results);

	/* just one value */
	} else if (limit == 1) {
		SbuDatabaseItem *item;
		gdouble ave_acc = 0;
		gint ts = 0;
		results2 = g_ptr_array_new_with_free_func (g_free);
		for (guint i = 0; i < results->len; i++) {
			item = g_ptr_array_index (results, i);
			ave_acc += item->val;
			ts = item->ts;
		}
		item = g_new0 (SbuDatabaseItem, 1);
		item->ts = ts;
		item->val = ave_acc / results->len;
		g_ptr_array_add (results2, item);

	/* bin into averaged groups */
	} else {
		gint ts_last_added = 0;
		guint ave_cnt = 0;
		gdouble ave_acc = 0;
		gint64 interval = (arg_end - arg_start) / (limit - 1);

		results2 = g_ptr_array_new_with_free_func (g_free);
		for (guint i = 0; i < results->len; i++) {
			SbuDatabaseItem *item = g_ptr_array_index (results, i);

			if (ts_last_added == 0)
				ts_last_added = item->ts;

			/* first and last points */
			if (i == 0 || i == results->len - 1) {
				SbuDatabaseItem *item2 = g_new0 (SbuDatabaseItem, 1);
				item2->ts = item->ts;
				item2->val = item->val;
				g_ptr_array_add (results2, item2);
				continue;
			}

			/* add to moving average */
			ave_acc += item->val;
			ave_cnt += 1;

			/* more than the interval */
			if (item->ts - ts_last_added > interval) {
				SbuDatabaseItem *item2 = g_new0 (SbuDatabaseItem, 1);
				item2->ts = item->ts;
				item2->val = ave_acc / (gdouble) ave_cnt;
				g_ptr_array_add (results2, item2);
				ts_last_added = item->ts;

				/* reset moving average */
				ave_cnt = 0;
				ave_acc = 0.f;
			}
		}
	}


	/* return as a GVariant */
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(td))"));
	g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(td)"));
	for (guint i = 0; i < results2->len; i++) {
		SbuDatabaseItem *item = g_ptr_array_index (results2, i);
		gdouble val = item->val;
		if (fabs (val) > 1.1f)
			val /= 1000.f;
		g_variant_builder_add (&builder, "(td)", item->ts, val);
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
sbu_device_impl_unexport (SbuDeviceImpl *self)
{
	for (guint i = 0; i < self->nodes->len; i++) {
		SbuNodeImpl *node = g_ptr_array_index (self->nodes, i);
		const gchar *path = sbu_node_impl_get_object_path (node);
		g_dbus_object_manager_server_unexport (self->object_manager, path);
	}
	for (guint i = 0; i < self->links->len; i++) {
		SbuLinkImpl *link = g_ptr_array_index (self->links, i);
		const gchar *path = sbu_link_impl_get_object_path (link);
		g_dbus_object_manager_server_unexport (self->object_manager, path);
	}
	g_dbus_object_manager_server_unexport (self->object_manager,
					       self->object_path);
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
	iface->handle_get_history = sbu_device_impl_get_history;
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
sbu_device_impl_set_object_manager (SbuDeviceImpl *self,
				    GDBusObjectManagerServer *object_manager)
{
	/* no ref */
	self->object_manager = object_manager;

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
}

static void
sbu_device_impl_set_object_path (SbuDeviceImpl *self,
				 const gchar *object_path)
{
	self->object_path = g_strdup (object_path);

	/* set for all nodes and links */
	for (guint i = 0; i < self->nodes->len; i++) {
		SbuNodeKind kind;
		SbuNode *node = g_ptr_array_index (self->nodes, i);
		g_autofree gchar *node_path = NULL;
		g_object_get (node, "kind", &kind, NULL);
		node_path = g_strdup_printf ("%s/node_%s",
					     object_path,
					     sbu_node_kind_to_string (kind));
		g_object_set (node, "object-path", node_path, NULL);
	}
	for (guint i = 0; i < self->links->len; i++) {
		SbuNodeKind src, dst;
		SbuLink *link = g_ptr_array_index (self->links, i);
		g_autofree gchar *link_path = NULL;
		g_object_get (link, "src", &src, "dst", &dst, NULL);
		link_path = g_strdup_printf ("%s/link_%s_%s",
					     object_path,
					     sbu_node_kind_to_string (src),
					     sbu_node_kind_to_string (dst));
		g_object_set (link, "object-path", link_path, NULL);
	}
}

void
sbu_device_set_database (SbuDeviceImpl *self, SbuDatabase *database)
{
	g_set_object (&self->database, database);
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
		sbu_device_impl_set_object_manager (self, g_value_get_object (value));
		break;
	case PROP_OBJECT_PATH:
		g_assert (self->object_path == NULL);
		sbu_device_impl_set_object_path (self, g_value_get_string (value));
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
	if (self->database != NULL)
		g_object_unref (self->database);
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
