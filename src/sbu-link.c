/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#include "config.h"

#include "sbu-link.h"

struct _SbuLink {
	GObject parent_instance;
	gchar *id;
	gboolean active;
	SbuNodeKind src;
	SbuNodeKind dst;
};

G_DEFINE_TYPE(SbuLink, sbu_link, G_TYPE_OBJECT)

enum { PROP_0, PROP_ACTIVE, PROP_LAST };

const gchar *
sbu_link_get_id(SbuLink *self)
{
	g_return_val_if_fail(SBU_IS_LINK(self), NULL);
	return self->id;
}

static void
sbu_link_set_id(SbuLink *self, const gchar *id)
{
	g_return_if_fail(SBU_IS_LINK(self));
	if (g_strcmp0(self->id, id) == 0)
		return;
	g_free(self->id);
	self->id = g_strdup(id);
}

SbuNodeKind
sbu_link_get_src(SbuLink *self)
{
	g_return_val_if_fail(SBU_IS_LINK(self), SBU_NODE_KIND_UNKNOWN);
	return self->src;
}

SbuNodeKind
sbu_link_get_dst(SbuLink *self)
{
	g_return_val_if_fail(SBU_IS_LINK(self), SBU_NODE_KIND_UNKNOWN);
	return self->dst;
}

gboolean
sbu_link_get_active(SbuLink *self)
{
	g_return_val_if_fail(SBU_IS_LINK(self), FALSE);
	return self->active;
}

void
sbu_link_set_active(SbuLink *self, gboolean active)
{
	g_return_if_fail(SBU_IS_LINK(self));
	if (self->active == active)
		return;
	self->active = active;
	g_object_notify(G_OBJECT(self), "active");
}

GVariant *
sbu_link_to_variant(SbuLink *self)
{
	GVariantBuilder builder;

	g_return_val_if_fail(SBU_IS_LINK(self), NULL);

	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
	if (self->id != NULL) {
		g_variant_builder_add(&builder, "{sv}", "id", g_variant_new_string(self->id));
	}
	if (self->src != SBU_NODE_KIND_UNKNOWN) {
		g_variant_builder_add(&builder, "{sv}", "src", g_variant_new_uint32(self->src));
	}
	if (self->dst != SBU_NODE_KIND_UNKNOWN) {
		g_variant_builder_add(&builder, "{sv}", "dst", g_variant_new_uint32(self->dst));
	}
	g_variant_builder_add(&builder, "{sv}", "active", g_variant_new_boolean(self->active));
	return g_variant_new("a{sv}", &builder);
}

static void
sbu_link_from_key_value(SbuLink *self, const gchar *key, GVariant *value)
{
	if (g_strcmp0(key, "id") == 0) {
		sbu_link_set_id(self, g_variant_get_string(value, NULL));
		return;
	}
	if (g_strcmp0(key, "src") == 0) {
		self->src = g_variant_get_uint32(value);
		return;
	}
	if (g_strcmp0(key, "dst") == 0) {
		self->dst = g_variant_get_uint32(value);
		return;
	}
	if (g_strcmp0(key, "active") == 0) {
		self->active = g_variant_get_boolean(value);
		return;
	}
}

static void
sbu_link_set_from_variant_iter(SbuLink *self, GVariantIter *iter)
{
	GVariant *value;
	const gchar *key;
	while (g_variant_iter_next(iter, "{&sv}", &key, &value)) {
		sbu_link_from_key_value(self, key, value);
		g_variant_unref(value);
	}
}

SbuLink *
sbu_link_from_variant(GVariant *value)
{
	SbuLink *self = NULL;
	const gchar *type_string;
	g_autoptr(GVariantIter) iter = NULL;

	type_string = g_variant_get_type_string(value);
	if (g_strcmp0(type_string, "(a{sv})") == 0) {
		self = sbu_link_new(SBU_NODE_KIND_UNKNOWN, SBU_NODE_KIND_UNKNOWN);
		g_variant_get(value, "(a{sv})", &iter);
		sbu_link_set_from_variant_iter(self, iter);
	} else if (g_strcmp0(type_string, "a{sv}") == 0) {
		self = sbu_link_new(SBU_NODE_KIND_UNKNOWN, SBU_NODE_KIND_UNKNOWN);
		g_variant_get(value, "a{sv}", &iter);
		sbu_link_set_from_variant_iter(self, iter);
	} else {
		g_warning("type %s not known", type_string);
	}
	return self;
}

static void
sbu_link_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SbuLink *self = SBU_LINK(object);
	g_return_if_fail(SBU_IS_LINK(self));
	switch (prop_id) {
	case PROP_ACTIVE:
		self->active = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
sbu_link_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SbuLink *self = SBU_LINK(object);
	g_return_if_fail(SBU_IS_LINK(self));
	switch (prop_id) {
	case PROP_ACTIVE:
		g_value_set_boolean(value, self->active);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
sbu_link_finalize(GObject *object)
{
	SbuLink *self = SBU_LINK(object);
	g_free(self->id);
	G_OBJECT_CLASS(sbu_link_parent_class)->finalize(object);
}

static void
sbu_link_init(SbuLink *self)
{
}

static void
sbu_link_class_init(SbuLinkClass *klass_link)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS(klass_link);

	object_class->set_property = sbu_link_set_property;
	object_class->get_property = sbu_link_get_property;
	object_class->finalize = sbu_link_finalize;

	pspec = g_param_spec_boolean("active", NULL, NULL, FALSE, G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_ACTIVE, pspec);
}

SbuLink *
sbu_link_new(SbuNodeKind src, SbuNodeKind dst)
{
	SbuLink *self;
	self = g_object_new(SBU_TYPE_LINK, NULL);
	self->src = src;
	self->dst = dst;
	if (src != SBU_NODE_KIND_UNKNOWN && dst != SBU_NODE_KIND_UNKNOWN) {
		self->id = g_strdup_printf("link_%s_%s",
					   sbu_node_kind_to_string(src),
					   sbu_node_kind_to_string(dst));
	}
	return SBU_LINK(self);
}
