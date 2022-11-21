/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "config.h"

#include <string.h>

#include "sbu-config.h"

struct _SbuConfig {
	GObject parent_instance;
	GKeyFile *config;
	gboolean loaded;
};

G_DEFINE_TYPE(SbuConfig, sbu_config, G_TYPE_OBJECT)

#define SBU_CONFIG_GROUP "sbud Settings"

static gboolean
sbu_config_open(SbuConfig *self, GError **error)
{
	g_autofree gchar *config_fn = NULL;
	if (self->loaded)
		return TRUE;
	config_fn = g_build_filename(SYSCONFDIR, "sbud", "sbud.conf", NULL);
	if (!g_key_file_load_from_file(self->config, config_fn, G_KEY_FILE_NONE, error)) {
		g_prefix_error(error, "coulf not open %s: ", config_fn);
		return FALSE;
	}
	self->loaded = TRUE;
	return TRUE;
}

gchar *
sbu_config_get_string(SbuConfig *self, const gchar *key, GError **error)
{
	if (!sbu_config_open(self, error))
		return NULL;
	return g_key_file_get_string(self->config, SBU_CONFIG_GROUP, key, error);
}

gint
sbu_config_get_integer(SbuConfig *self, const gchar *key, GError **error)
{
	if (!sbu_config_open(self, error))
		return 0;
	return g_key_file_get_integer(self->config, SBU_CONFIG_GROUP, key, error);
}

gboolean
sbu_config_get_boolean(SbuConfig *self, const gchar *key, GError **error)
{
	if (!sbu_config_open(self, error))
		return FALSE;
	return g_key_file_get_boolean(self->config, SBU_CONFIG_GROUP, key, error);
}

static void
sbu_config_finalize(GObject *object)
{
	SbuConfig *self = SBU_CONFIG(object);

	g_key_file_unref(self->config);

	G_OBJECT_CLASS(sbu_config_parent_class)->finalize(object);
}

static void
sbu_config_init(SbuConfig *self)
{
	self->config = g_key_file_new();
}

static void
sbu_config_class_init(SbuConfigClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	object_class->finalize = sbu_config_finalize;
}

SbuConfig *
sbu_config_new(void)
{
	SbuConfig *self;
	self = g_object_new(SBU_TYPE_CONFIG, NULL);
	return SBU_CONFIG(self);
}
