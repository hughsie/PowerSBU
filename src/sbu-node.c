/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#include "config.h"

#include "sbu-node.h"

struct _SbuNode {
	GObject parent_instance;
	gchar *id;
	SbuNodeKind kind;
	gdouble values[SBU_DEVICE_PROPERTY_LAST];
};

G_DEFINE_TYPE(SbuNode, sbu_node, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_POWER,
	PROP_POWER_MAX,
	PROP_VOLTAGE,
	PROP_VOLTAGE_MAX,
	PROP_CURRENT,
	PROP_CURRENT_MAX,
	PROP_FREQUENCY,
	PROP_LAST
};

const gchar *
sbu_node_get_id(SbuNode *self)
{
	g_return_val_if_fail(SBU_IS_NODE(self), NULL);
	return self->id;
}

void
sbu_node_set_id(SbuNode *self, const gchar *id)
{
	g_return_if_fail(SBU_IS_NODE(self));
	if (g_strcmp0(self->id, id) == 0)
		return;
	g_free(self->id);
	self->id = g_strdup(id);
}

SbuNodeKind
sbu_node_get_kind(SbuNode *self)
{
	g_return_val_if_fail(SBU_IS_NODE(self), SBU_NODE_KIND_UNKNOWN);
	return self->kind;
}

static void
sbu_node_set_kind(SbuNode *self, SbuNodeKind kind)
{
	g_return_if_fail(SBU_IS_NODE(self));
	if (self->kind == kind)
		return;
	self->kind = kind;
}

gboolean
sbu_node_get_value(SbuNode *self, SbuDeviceProperty kind, gdouble *value)
{
	g_return_val_if_fail(SBU_IS_NODE(self), FALSE);
	if (value != NULL)
		*value = self->values[kind];
	return TRUE;
}

void
sbu_node_set_value(SbuNode *self, SbuDeviceProperty kind, gdouble value)
{
	g_return_if_fail(SBU_IS_NODE(self));
	g_return_if_fail(value > -100000);
	g_return_if_fail(value < 100000);
	if (value == self->values[kind])
		return;
	self->values[kind] = value;
	g_debug("setting node:%s %s=%.2f", self->id, sbu_device_property_to_string(kind), value);
	g_object_notify(G_OBJECT(self), sbu_device_property_to_string(kind));
}

gdouble
sbu_node_get_voltage(SbuNode *self)
{
	g_return_val_if_fail(SBU_IS_NODE(self), -1);
	return self->values[SBU_DEVICE_PROPERTY_VOLTAGE];
}

gdouble
sbu_node_get_voltage_max(SbuNode *self)
{
	g_return_val_if_fail(SBU_IS_NODE(self), -1);
	return self->values[SBU_DEVICE_PROPERTY_VOLTAGE_MAX];
}

gdouble
sbu_node_get_power(SbuNode *self)
{
	g_return_val_if_fail(SBU_IS_NODE(self), -1);
	return self->values[SBU_DEVICE_PROPERTY_POWER];
}

gdouble
sbu_node_get_power_max(SbuNode *self)
{
	g_return_val_if_fail(SBU_IS_NODE(self), -1);
	return self->values[SBU_DEVICE_PROPERTY_POWER_MAX];
}

GVariant *
sbu_node_to_variant(SbuNode *self)
{
	GVariantBuilder builder;

	g_return_val_if_fail(SBU_IS_NODE(self), NULL);

	g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
	if (self->id != NULL)
		g_variant_builder_add(&builder, "{sv}", "id", g_variant_new_string(self->id));
	if (self->kind != SBU_NODE_KIND_UNKNOWN)
		g_variant_builder_add(&builder, "{sv}", "kind", g_variant_new_uint32(self->kind));
	for (guint i = 0; i < SBU_DEVICE_PROPERTY_LAST; i++) {
		if (self->values[i] != -1.f) {
			g_variant_builder_add(&builder,
					      "{sv}",
					      sbu_device_property_to_string(i),
					      g_variant_new_double(self->values[i]));
		}
	}
	return g_variant_new("a{sv}", &builder);
}

static void
sbu_node_from_key_value(SbuNode *self, const gchar *key, GVariant *value)
{
	if (g_strcmp0(key, "id") == 0) {
		sbu_node_set_id(self, g_variant_get_string(value, NULL));
		return;
	}
	if (g_strcmp0(key, "kind") == 0) {
		sbu_node_set_kind(self, g_variant_get_uint32(value));
		return;
	}
	for (guint i = 0; i < SBU_DEVICE_PROPERTY_LAST; i++) {
		if (g_strcmp0(key, sbu_device_property_to_string(i)) == 0) {
			self->values[i] = g_variant_get_double(value);
			return;
		}
	}
}

static void
sbu_node_set_from_variant_iter(SbuNode *self, GVariantIter *iter)
{
	GVariant *value;
	const gchar *key;
	while (g_variant_iter_next(iter, "{&sv}", &key, &value)) {
		sbu_node_from_key_value(self, key, value);
		g_variant_unref(value);
	}
}

SbuNode *
sbu_node_from_variant(GVariant *value)
{
	SbuNode *self = NULL;
	const gchar *type_string;
	g_autoptr(GVariantIter) iter = NULL;

	type_string = g_variant_get_type_string(value);
	if (g_strcmp0(type_string, "(a{sv})") == 0) {
		self = sbu_node_new(SBU_NODE_KIND_UNKNOWN);
		g_variant_get(value, "(a{sv})", &iter);
		sbu_node_set_from_variant_iter(self, iter);
	} else if (g_strcmp0(type_string, "a{sv}") == 0) {
		self = sbu_node_new(SBU_NODE_KIND_UNKNOWN);
		g_variant_get(value, "a{sv}", &iter);
		sbu_node_set_from_variant_iter(self, iter);
	} else {
		g_warning("type %s not known", type_string);
	}
	return self;
}

static void
sbu_node_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SbuNode *self = SBU_NODE(object);
	g_return_if_fail(SBU_IS_NODE(self));
	switch (prop_id) {
	case PROP_POWER:
		self->values[SBU_DEVICE_PROPERTY_POWER] = g_value_get_double(value);
		break;
	case PROP_VOLTAGE:
		self->values[SBU_DEVICE_PROPERTY_VOLTAGE] = g_value_get_double(value);
		break;
	case PROP_CURRENT:
		self->values[SBU_DEVICE_PROPERTY_CURRENT] = g_value_get_double(value);
		break;
	case PROP_FREQUENCY:
		self->values[SBU_DEVICE_PROPERTY_FREQUENCY] = g_value_get_double(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
sbu_node_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SbuNode *self = SBU_NODE(object);
	g_return_if_fail(SBU_IS_NODE(self));
	switch (prop_id) {
	case PROP_POWER:
		g_value_set_double(value, self->values[SBU_DEVICE_PROPERTY_POWER]);
		break;
	case PROP_VOLTAGE:
		g_value_set_double(value, self->values[SBU_DEVICE_PROPERTY_VOLTAGE]);
		break;
	case PROP_CURRENT:
		g_value_set_double(value, self->values[SBU_DEVICE_PROPERTY_CURRENT]);
		break;
	case PROP_FREQUENCY:
		g_value_set_double(value, self->values[SBU_DEVICE_PROPERTY_FREQUENCY]);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
sbu_node_finalize(GObject *object)
{
	SbuNode *self = SBU_NODE(object);
	g_free(self->id);
	G_OBJECT_CLASS(sbu_node_parent_class)->finalize(object);
}

static void
sbu_node_init(SbuNode *self)
{
}

static void
sbu_node_class_init(SbuNodeClass *klass_node)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass_node);

	object_class->set_property = sbu_node_set_property;
	object_class->get_property = sbu_node_get_property;
	object_class->finalize = sbu_node_finalize;

	g_object_class_install_property(object_class,
					PROP_POWER,
					g_param_spec_double("power",
							    NULL,
							    NULL,
							    -G_MAXDOUBLE,
							    G_MAXDOUBLE,
							    0.f,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class,
					PROP_POWER_MAX,
					g_param_spec_double("power-max",
							    NULL,
							    NULL,
							    -G_MAXDOUBLE,
							    G_MAXDOUBLE,
							    0.f,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class,
					PROP_VOLTAGE,
					g_param_spec_double("voltage",
							    NULL,
							    NULL,
							    -G_MAXDOUBLE,
							    G_MAXDOUBLE,
							    0.f,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class,
					PROP_VOLTAGE_MAX,
					g_param_spec_double("voltage-max",
							    NULL,
							    NULL,
							    -G_MAXDOUBLE,
							    G_MAXDOUBLE,
							    0.f,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class,
					PROP_CURRENT,
					g_param_spec_double("current",
							    NULL,
							    NULL,
							    -G_MAXDOUBLE,
							    G_MAXDOUBLE,
							    0.f,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class,
					PROP_CURRENT_MAX,
					g_param_spec_double("current-max",
							    NULL,
							    NULL,
							    -G_MAXDOUBLE,
							    G_MAXDOUBLE,
							    0.f,
							    G_PARAM_READWRITE));
	g_object_class_install_property(
	    object_class,
	    PROP_FREQUENCY,
	    g_param_spec_double("frequency", NULL, NULL, 0.f, G_MAXDOUBLE, 0.f, G_PARAM_READWRITE));
}

SbuNode *
sbu_node_new(SbuNodeKind kind)
{
	SbuNode *self;
	self = g_object_new(SBU_TYPE_NODE, NULL);
	self->kind = kind;
	if (kind != SBU_NODE_KIND_UNKNOWN)
		self->id = g_strdup_printf("node_%s", sbu_node_kind_to_string(kind));
	return SBU_NODE(self);
}
