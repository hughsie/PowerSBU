/*
 * Copyright (C) 2013-2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "config.h"

#include "sbu-plugin.h"

typedef struct {
	GModule *module;
	guint64 flags;
	gboolean enabled;
	gchar *name;
} SbuPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(SbuPlugin, sbu_plugin, G_TYPE_OBJECT)

enum { PROP_0, PROP_FLAGS, PROP_LAST };

enum { SIGNAL_UPDATE_METADATA, SIGNAL_ADD_DEVICE, SIGNAL_REMOVE_DEVICE, SIGNAL_LAST };

static guint signals[SIGNAL_LAST] = {0};

gboolean
sbu_plugin_setup(SbuPlugin *self, GCancellable *cancellable, GError **error)
{
	SbuPluginClass *plugin_klass = SBU_PLUGIN_GET_CLASS(self);
	g_return_val_if_fail(SBU_IS_PLUGIN(self), FALSE);
	if (plugin_klass->setup == NULL)
		return TRUE;
	return plugin_klass->setup(self, cancellable, error);
}

gboolean
sbu_plugin_refresh(SbuPlugin *self, GCancellable *cancellable, GError **error)
{
	SbuPluginClass *plugin_klass = SBU_PLUGIN_GET_CLASS(self);
	g_return_val_if_fail(SBU_IS_PLUGIN(self), FALSE);
	if (plugin_klass->refresh == NULL)
		return TRUE;
	return plugin_klass->refresh(self, cancellable, error);
}

gboolean
sbu_plugin_get_enabled(SbuPlugin *self)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private(self);
	g_return_val_if_fail(SBU_IS_PLUGIN(self), FALSE);
	return priv->enabled;
}

void
sbu_plugin_set_enabled(SbuPlugin *self, gboolean enabled)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private(self);
	g_return_if_fail(SBU_IS_PLUGIN(self));
	priv->enabled = enabled;
}

static void
sbu_plugin_finalize(GObject *object)
{
	SbuPlugin *self = SBU_PLUGIN(object);
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private(self);
	g_free(priv->name);
}

const gchar *
sbu_plugin_get_name(SbuPlugin *self)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private(self);
	g_return_val_if_fail(SBU_IS_PLUGIN(self), FALSE);
	return priv->name;
}

void
sbu_plugin_update_metadata(SbuPlugin *self, SbuDevice *device, const gchar *key, gint value)
{
	g_return_if_fail(SBU_IS_PLUGIN(self));
	g_return_if_fail(SBU_IS_DEVICE(device));
	g_debug("saving metadata %s=%i", key, value);
	g_signal_emit(self, signals[SIGNAL_UPDATE_METADATA], 0, device, key, value);
}

void
sbu_plugin_add_device(SbuPlugin *self, SbuDevice *device)
{
	g_return_if_fail(SBU_IS_PLUGIN(self));
	g_return_if_fail(SBU_IS_DEVICE(device));
	g_signal_emit(self, signals[SIGNAL_ADD_DEVICE], 0, device);
}

void
sbu_plugin_remove_device(SbuPlugin *self, SbuDevice *device)
{
	g_return_if_fail(SBU_IS_PLUGIN(self));
	g_return_if_fail(SBU_IS_DEVICE(device));
	g_signal_emit(self, signals[SIGNAL_REMOVE_DEVICE], 0, device);
}

static void
sbu_plugin_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SbuPlugin *self = SBU_PLUGIN(object);
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private(self);
	g_return_if_fail(SBU_IS_PLUGIN(self));
	switch (prop_id) {
	case PROP_FLAGS:
		priv->flags = g_value_get_uint64(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
sbu_plugin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SbuPlugin *self = SBU_PLUGIN(object);
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private(self);
	g_return_if_fail(SBU_IS_PLUGIN(self));
	switch (prop_id) {
	case PROP_FLAGS:
		g_value_set_uint64(value, priv->flags);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
sbu_plugin_class_init(SbuPluginClass *class)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS(class);

	object_class->set_property = sbu_plugin_set_property;
	object_class->get_property = sbu_plugin_get_property;
	object_class->finalize = sbu_plugin_finalize;

	pspec = g_param_spec_uint64("flags", NULL, NULL, 0, G_MAXUINT64, 0, G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_FLAGS, pspec);

	signals[SIGNAL_UPDATE_METADATA] =
	    g_signal_new("update-metadata",
			 G_TYPE_FROM_CLASS(object_class),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(SbuPluginClass, update_metadata),
			 NULL,
			 NULL,
			 g_cclosure_marshal_generic,
			 G_TYPE_NONE,
			 3,
			 SBU_TYPE_DEVICE,
			 G_TYPE_STRING,
			 G_TYPE_INT);
	signals[SIGNAL_ADD_DEVICE] = g_signal_new("add-device",
						  G_TYPE_FROM_CLASS(object_class),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET(SbuPluginClass, add_device),
						  NULL,
						  NULL,
						  g_cclosure_marshal_generic,
						  G_TYPE_NONE,
						  1,
						  SBU_TYPE_DEVICE);
	signals[SIGNAL_REMOVE_DEVICE] = g_signal_new("remove-device",
						     G_TYPE_FROM_CLASS(object_class),
						     G_SIGNAL_RUN_LAST,
						     G_STRUCT_OFFSET(SbuPluginClass, add_device),
						     NULL,
						     NULL,
						     g_cclosure_marshal_generic,
						     G_TYPE_NONE,
						     1,
						     SBU_TYPE_DEVICE);
}

static void
sbu_plugin_init(SbuPlugin *self)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private(self);
	priv->enabled = TRUE;
}
