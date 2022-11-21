/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#include "config.h"

#include <gio/gio.h>

#include "sbu-device.h"
#include "sbu-link.h"
#include "sbu-node.h"

typedef struct {
	GPtrArray *nodes;
	GPtrArray *links;
	gchar *id;
	gchar *firmware_version;
	gchar *serial_number;
} SbuDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(SbuDevice, sbu_device, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (sbu_device_get_instance_private(o))

const gchar *
sbu_device_get_id(SbuDevice *self)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	return priv->id;
}

void
sbu_device_set_id(SbuDevice *self, const gchar *id)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	priv->id = g_strdup(id);
}

const gchar *
sbu_device_get_firmware_version(SbuDevice *self)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	return priv->firmware_version;
}

void
sbu_device_set_firmware_version(SbuDevice *self, const gchar *firmware_version)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	priv->firmware_version = g_strdup(firmware_version);
}

const gchar *
sbu_device_get_serial_number(SbuDevice *self)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	return priv->serial_number;
}

void
sbu_device_set_serial_number(SbuDevice *self, const gchar *serial_number)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	priv->serial_number = g_strdup(serial_number);
}

GPtrArray *
sbu_device_get_nodes(SbuDevice *self)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	return priv->nodes;
}

GPtrArray *
sbu_device_get_links(SbuDevice *self)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	return priv->links;
}

SbuNode *
sbu_device_get_node(SbuDevice *self, SbuNodeKind kind)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	for (guint i = 0; i < priv->nodes->len; i++) {
		SbuNode *n = g_ptr_array_index(priv->nodes, i);
		if (kind == sbu_node_get_kind(n))
			return n;
	}
	return NULL;
}

SbuLink *
sbu_device_get_link(SbuDevice *self, SbuNodeKind src, SbuNodeKind dst)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	for (guint i = 0; i < priv->links->len; i++) {
		SbuLink *link = g_ptr_array_index(priv->links, i);
		if (src == sbu_link_get_src(link) && dst == sbu_link_get_dst(link))
			return link;
	}
	return NULL;
}

gdouble
sbu_device_get_node_value(SbuDevice *self, SbuNodeKind kind, SbuDeviceProperty key)
{
	SbuNode *n;
	gdouble val;
	n = sbu_device_get_node(self, kind);
	if (n == NULL)
		return 0.f;
	if (!sbu_node_get_value(n, key, &val))
		return 0.f;
	return val;
}

void
sbu_device_set_node_value(SbuDevice *self, SbuNodeKind kind, SbuDeviceProperty key, gdouble value)
{
	SbuNode *n = sbu_device_get_node(self, kind);
	if (n == NULL)
		return;
	sbu_node_set_value(n, key, value);
}

gboolean
sbu_device_get_link_active(SbuDevice *self, SbuNodeKind src, SbuNodeKind dst)
{
	SbuLink *l = sbu_device_get_link(self, src, dst);
	if (l == NULL)
		return FALSE;
	return sbu_link_get_active(l);
}

void
sbu_device_set_link_active(SbuDevice *self, SbuNodeKind src, SbuNodeKind dst, gboolean value)
{
	SbuLink *l = sbu_device_get_link(self, src, dst);
	if (l == NULL)
		return;
	sbu_link_set_active(l, value);
}

void
sbu_device_add_node(SbuDevice *self, SbuNode *node)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	g_ptr_array_add(priv->nodes, g_object_ref(node));
}

void
sbu_device_add_link(SbuDevice *self, SbuLink *link)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	g_ptr_array_add(priv->links, g_object_ref(link));
}

GVariant *
sbu_device_to_variant(SbuDevice *self)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	GVariantBuilder builder;

	g_return_val_if_fail(SBU_IS_DEVICE(self), NULL);

	/* create an array with all the metadata in */
	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
	if (priv->id != NULL) {
		g_variant_builder_add(&builder, "{sv}", "id", g_variant_new_string(priv->id));
	}
	if (priv->firmware_version != NULL) {
		g_variant_builder_add(&builder,
				      "{sv}",
				      "firmware-version",
				      g_variant_new_string(priv->firmware_version));
	}
	if (priv->serial_number != NULL) {
		g_variant_builder_add(&builder,
				      "{sv}",
				      "serial-number",
				      g_variant_new_string(priv->serial_number));
	}
	if (priv->nodes->len > 0) {
		g_autofree GVariant **children = g_new0(GVariant *, priv->nodes->len);
		for (guint i = 0; i < priv->nodes->len; i++) {
			SbuNode *n = g_ptr_array_index(priv->nodes, i);
			children[i] = sbu_node_to_variant(n);
		}
		g_variant_builder_add(
		    &builder,
		    "{sv}",
		    "nodes",
		    g_variant_new_array(G_VARIANT_TYPE("a{sv}"), children, priv->nodes->len));
	}
	if (priv->links->len > 0) {
		g_autofree GVariant **children = g_new0(GVariant *, priv->links->len);
		for (guint i = 0; i < priv->links->len; i++) {
			SbuLink *l = g_ptr_array_index(priv->links, i);
			children[i] = sbu_link_to_variant(l);
		}
		g_variant_builder_add(
		    &builder,
		    "{sv}",
		    "links",
		    g_variant_new_array(G_VARIANT_TYPE("a{sv}"), children, priv->links->len));
	}
	return g_variant_new("a{sv}", &builder);
}
static void
sbu_device_from_key_value(SbuDevice *self, const gchar *key, GVariant *value)
{
	if (g_strcmp0(key, "id") == 0) {
		sbu_device_set_id(self, g_variant_get_string(value, NULL));
		return;
	}
	if (g_strcmp0(key, "firmware-version") == 0) {
		sbu_device_set_firmware_version(self, g_variant_get_string(value, NULL));
		return;
	}
	if (g_strcmp0(key, "serial-number") == 0) {
		sbu_device_set_serial_number(self, g_variant_get_string(value, NULL));
		return;
	}
	if (g_strcmp0(key, "nodes") == 0) {
		GVariantIter iter;
		GVariant *child;
		g_variant_iter_init(&iter, value);
		while ((child = g_variant_iter_next_value(&iter))) {
			g_autoptr(SbuNode) n = sbu_node_from_variant(child);
			sbu_device_add_node(self, n);
			g_variant_unref(child);
		}
		return;
	}
	if (g_strcmp0(key, "links") == 0) {
		GVariantIter iter;
		GVariant *child;
		g_variant_iter_init(&iter, value);
		while ((child = g_variant_iter_next_value(&iter))) {
			g_autoptr(SbuLink) l = sbu_link_from_variant(child);
			sbu_device_add_link(self, l);
			g_variant_unref(child);
		}
		return;
	}
}

static void
sbu_device_set_from_variant_iter(SbuDevice *self, GVariantIter *iter)
{
	GVariant *value;
	const gchar *key;
	while (g_variant_iter_next(iter, "{&sv}", &key, &value)) {
		sbu_device_from_key_value(self, key, value);
		g_variant_unref(value);
	}
}

SbuDevice *
sbu_device_from_variant(GVariant *value)
{
	SbuDevice *self = NULL;
	const gchar *type_string;
	g_autoptr(GVariantIter) iter = NULL;

	type_string = g_variant_get_type_string(value);
	if (g_strcmp0(type_string, "(a{sv})") == 0) {
		self = sbu_device_new();
		g_variant_get(value, "(a{sv})", &iter);
		sbu_device_set_from_variant_iter(self, iter);
	} else if (g_strcmp0(type_string, "a{sv}") == 0) {
		self = sbu_device_new();
		g_variant_get(value, "a{sv}", &iter);
		sbu_device_set_from_variant_iter(self, iter);
	} else {
		g_warning("type %s not known", type_string);
	}
	return self;
}

gboolean
sbu_device_refresh(SbuDevice *device, GError **error)
{
	SbuDeviceClass *device_class = SBU_DEVICE_GET_CLASS(device);
	g_return_val_if_fail(SBU_IS_DEVICE(device), FALSE);
	if (device_class->refresh == NULL)
		return TRUE;
	return device_class->refresh(device, error);
}

static void
sbu_device_finalize(GObject *object)
{
	SbuDevice *self = SBU_DEVICE(object);
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	g_free(priv->id);
	g_free(priv->firmware_version);
	g_free(priv->serial_number);
	g_ptr_array_unref(priv->nodes);
	g_ptr_array_unref(priv->links);
	G_OBJECT_CLASS(sbu_device_parent_class)->finalize(object);
}

static void
sbu_device_init(SbuDevice *self)
{
	SbuDevicePrivate *priv = GET_PRIVATE(self);
	priv->nodes = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	priv->links = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
}

static void
sbu_device_class_init(SbuDeviceClass *klass_device)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass_device);
	object_class->finalize = sbu_device_finalize;
}

SbuDevice *
sbu_device_new(void)
{
	SbuDevice *device;
	device = g_object_new(SBU_TYPE_DEVICE, NULL);
	return SBU_DEVICE(device);
}
