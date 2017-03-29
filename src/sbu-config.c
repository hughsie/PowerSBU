/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

#include <string.h>

#include "sbu-config.h"

struct _SbuConfig
{
	GObject			 parent_instance;
	GKeyFile		*config;
	gboolean		 loaded;
};

G_DEFINE_TYPE (SbuConfig, sbu_config, G_TYPE_OBJECT)

#define SBU_CONFIG_GROUP	"sbud Settings"

static gboolean
sbu_config_open (SbuConfig *self, GError **error)
{
	g_autofree gchar *config_fn = NULL;
	if (self->loaded)
		return TRUE;
	config_fn = g_build_filename (SYSCONFDIR, "sbud", "sbud.conf", NULL);
	if (!g_key_file_load_from_file (self->config, config_fn,
					G_KEY_FILE_NONE, error)) {
		g_prefix_error (error, "coulf not open %s: ", config_fn);
		return FALSE;
	}
	self->loaded = TRUE;
	return TRUE;
}

gchar *
sbu_config_get_string (SbuConfig *self, const gchar *key, GError **error)
{
	if (!sbu_config_open (self, error))
		return NULL;
	return g_key_file_get_string (self->config, SBU_CONFIG_GROUP, key, error);
}

gint
sbu_config_get_integer (SbuConfig *self, const gchar *key, GError **error)
{
	if (!sbu_config_open (self, error))
		return 0;
	return g_key_file_get_integer (self->config, SBU_CONFIG_GROUP, key, error);
}

static void
sbu_config_finalize (GObject *object)
{
	SbuConfig *self = SBU_CONFIG (object);

	g_key_file_unref (self->config);

	G_OBJECT_CLASS (sbu_config_parent_class)->finalize (object);
}

static void
sbu_config_init (SbuConfig *self)
{
	self->config = g_key_file_new ();
}

static void
sbu_config_class_init (SbuConfigClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = sbu_config_finalize;
}

/**
 * sbu_config_new:
 *
 * Return value: a new SbuConfig object.
 **/
SbuConfig *
sbu_config_new (void)
{
	SbuConfig *self;
	self = g_object_new (SBU_TYPE_CONFIG, NULL);
	return SBU_CONFIG (self);
}
