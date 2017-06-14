/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2017 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#ifdef USE_VALGRIND
#include <valgrind.h>
#endif

#include "sbu-plugin-private.h"
#include "sbu-plugin.h"

typedef struct
{
	GModule			*module;
	SbuPluginData		*data;			/* for sbu-plugin-{name}.c */
	guint64			 flags;
	gboolean		 enabled;
	GHashTable		*vfuncs;		/* string:pointer */
	gchar			*name;
} SbuPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (SbuPlugin, sbu_plugin, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_FLAGS,
	PROP_LAST
};

enum {
	SIGNAL_UPDATE_METADATA,
	SIGNAL_ADD_DEVICE,
	SIGNAL_REMOVE_DEVICE,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

gboolean
sbu_plugin_get_enabled (SbuPlugin *plugin)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	return priv->enabled;
}

void
sbu_plugin_set_enabled (SbuPlugin *plugin, gboolean enabled)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	priv->enabled = enabled;
}

SbuPlugin *
sbu_plugin_create (const gchar *filename, GError **error)
{
	SbuPlugin *plugin = NULL;
	SbuPluginPrivate *priv;
	g_autofree gchar *basename = NULL;

	/* get the plugin name from the basename */
	basename = g_path_get_basename (filename);
	if (!g_str_has_prefix (basename, "libsbu_plugin_")) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "plugin filename has wrong prefix: %s",
			     filename);
		return NULL;
	}
	g_strdelimit (basename, ".", '\0');

	/* create new plugin */
	plugin = sbu_plugin_new ();
	priv = sbu_plugin_get_instance_private (plugin);
	priv->name = g_strdup (basename + 14);
	priv->module = g_module_open (filename, 0);
	if (priv->module == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "failed to open plugin %s: %s",
			     filename, g_module_error ());
		return NULL;
	}
	return plugin;
}

static void
sbu_plugin_finalize (GObject *object)
{
	SbuPlugin *plugin = SBU_PLUGIN (object);
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	g_free (priv->name);
	g_free (priv->data);
	g_hash_table_unref (priv->vfuncs);
#ifndef RUNNING_ON_VALGRIND
	if (priv->module != NULL)
		g_module_close (priv->module);
#endif
}

SbuPluginData *
sbu_plugin_get_data (SbuPlugin *plugin)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	g_assert (priv->data != NULL);
	return priv->data;
}

SbuPluginData *
sbu_plugin_alloc_data (SbuPlugin *plugin, gsize sz)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	g_assert (priv->data == NULL);
	priv->data = g_malloc0 (sz);
	return priv->data;
}

gpointer
sbu_plugin_get_symbol (SbuPlugin *plugin, const gchar *function_name)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	gpointer func = NULL;

	g_return_val_if_fail (function_name != NULL, NULL);

	/* disabled plugins shouldn't be checked */
	if (!priv->enabled)
		return NULL;

	/* look up the symbol from the cache */
	if (g_hash_table_lookup_extended (priv->vfuncs, function_name, NULL, &func))
		return func;

	/* look up the symbol using the elf headers */
	g_module_symbol (priv->module, function_name, &func);
	g_hash_table_insert (priv->vfuncs, g_strdup (function_name), func);

	return func;
}

const gchar *
sbu_plugin_get_name (SbuPlugin *plugin)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	return priv->name;
}

void
sbu_plugin_update_metadata (SbuPlugin *plugin,
			    SbuDeviceImpl *device,
			    const gchar *key,
			    gint value)
{
	g_debug ("saving metadata %s=%i", key, value);
	g_signal_emit (plugin, signals[SIGNAL_UPDATE_METADATA], 0,
			device, key, value);
}

void
sbu_plugin_add_device (SbuPlugin *plugin, SbuDeviceImpl *device)
{
	g_signal_emit (plugin, signals[SIGNAL_ADD_DEVICE], 0, device);
}

void
sbu_plugin_remove_device (SbuPlugin *plugin, SbuDeviceImpl *device)
{
	g_signal_emit (plugin, signals[SIGNAL_REMOVE_DEVICE], 0, device);
}

static void
sbu_plugin_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SbuPlugin *plugin = SBU_PLUGIN (object);
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	switch (prop_id) {
	case PROP_FLAGS:
		priv->flags = g_value_get_uint64 (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sbu_plugin_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SbuPlugin *plugin = SBU_PLUGIN (object);
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	switch (prop_id) {
	case PROP_FLAGS:
		g_value_set_uint64 (value, priv->flags);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
sbu_plugin_class_init (SbuPluginClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = sbu_plugin_set_property;
	object_class->get_property = sbu_plugin_get_property;
	object_class->finalize = sbu_plugin_finalize;

	pspec = g_param_spec_uint64 ("flags", NULL, NULL,
				     0, G_MAXUINT64, 0, G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FLAGS, pspec);

	signals [SIGNAL_UPDATE_METADATA] =
		g_signal_new ("update-metadata",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (SbuPluginClass, update_metadata),
			      NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 3, SBU_TYPE_DEVICE, G_TYPE_STRING, G_TYPE_INT);
	signals [SIGNAL_ADD_DEVICE] =
		g_signal_new ("add-device",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (SbuPluginClass, add_device),
			      NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 1, SBU_TYPE_DEVICE);
	signals [SIGNAL_REMOVE_DEVICE] =
		g_signal_new ("remove-device",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (SbuPluginClass, add_device),
			      NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 1, SBU_TYPE_DEVICE);
}

static void
sbu_plugin_init (SbuPlugin *plugin)
{
	SbuPluginPrivate *priv = sbu_plugin_get_instance_private (plugin);
	priv->enabled = TRUE;
	priv->vfuncs = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, NULL);
}

SbuPlugin *
sbu_plugin_new (void)
{
	SbuPlugin *plugin;
	plugin = g_object_new (SBU_TYPE_PLUGIN, NULL);
	return plugin;
}
