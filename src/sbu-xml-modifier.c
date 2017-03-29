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

#include "sbu-xml-modifier.h"

struct _SbuXmlModifier
{
	GObject			 parent_instance;
	GHashTable		*hash_cdata;
	GHashTable		*hash_attrs;
};

G_DEFINE_TYPE (SbuXmlModifier, sbu_xml_modifier, G_TYPE_OBJECT)

typedef struct {
	GString		*out;
	SbuXmlModifier	*self;
	gchar		*id;
} SbuXmlHelper;

static void
sbu_xml_modifier_start_element (GMarkupParseContext *context,
				const gchar *element_name,
				const gchar **attribute_names,
				const gchar **attribute_values,
				gpointer user_data,
				GError **error)
{
	SbuXmlHelper *helper = (SbuXmlHelper *) user_data;
	if (g_strv_length ((gchar **)attribute_names) == 0) {
		g_string_append_printf (helper->out, "<%s>", element_name);
		return;
	}

	/* does have an ID */
	for (guint i = 0; attribute_names[i] != NULL; i++) {
		if (g_strcmp0 (attribute_names[i], "id") == 0) {
			g_free (helper->id);
			helper->id = g_strdup (attribute_values[i]);
			break;
		}
	}

	g_string_append_printf (helper->out, "<%s", element_name);
	for (guint i = 0; attribute_names[i] != NULL; i++) {
		const gchar *text_replace;
		g_autofree gchar *key = NULL;

		/* anything to replace */
		key = g_strdup_printf ("%s-%s", helper->id, attribute_names[i]);
		text_replace = g_hash_table_lookup (helper->self->hash_cdata, key);
		if (text_replace != NULL) {
			g_string_append_printf (helper->out, " %s=\"%s\"",
						attribute_names[i],
						text_replace);
			continue;
		}

		/* as per usual */
		g_string_append_printf (helper->out, " %s=\"%s\"",
					attribute_names[i],
					attribute_values[i]);
	}
	g_string_append (helper->out, ">");
}

static void
sbu_xml_modifier_end_element (GMarkupParseContext *context,
			      const gchar *element_name,
			      gpointer user_data,
			      GError **error)
{
	SbuXmlHelper *helper = (SbuXmlHelper *) user_data;
	g_string_append_printf (helper->out, "</%s>", element_name);
}

static gboolean
sbu_xml_modifier_is_all_whitespace (const gchar *text, gsize text_len)
{
	for (gsize i = 0; i < text_len; i++) {
		if (!g_ascii_isspace (text[i]))
			return FALSE;
	}
	return TRUE;
}

static void
sbu_xml_modifier_text (GMarkupParseContext *context,
		       const gchar *text,
		       gsize text_len,
		       gpointer user_data,
		       GError **error)
{
	SbuXmlHelper *helper = (SbuXmlHelper *) user_data;
	const gchar *text_replace;

	/* all whitespace */
	if (sbu_xml_modifier_is_all_whitespace (text, text_len))
		return;

	/* is replaced */
	text_replace = g_hash_table_lookup (helper->self->hash_cdata, helper->id);
	if (text_replace != NULL) {
		g_string_append (helper->out, text_replace);
		return;
	}

	/* as per usual */
	g_string_append_len (helper->out, text, text_len);
}

static void
sbu_xml_modifier_passthrough (GMarkupParseContext *context,
			      const gchar *passthrough_text,
			      gsize text_len,
			      gpointer  user_data,
			      GError **error)
{
	SbuXmlHelper *helper = (SbuXmlHelper *) user_data;
	g_string_append_len (helper->out, passthrough_text, text_len);
}

static void
sbu_xml_modifier_helper_free (SbuXmlHelper *helper)
{
	if (helper->out != NULL)
		g_string_free (helper->out, TRUE);
	g_free (helper);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(SbuXmlHelper, sbu_xml_modifier_helper_free);

GString *
sbu_xml_modifier_process (SbuXmlModifier *self,
			  const gchar *text,
			  gssize text_len,
			  GError **error)
{
	GMarkupParser parser = {
		sbu_xml_modifier_start_element,
		sbu_xml_modifier_end_element,
		sbu_xml_modifier_text,
		sbu_xml_modifier_passthrough,
		NULL
	};
	g_autoptr(SbuXmlHelper) helper = g_new0 (SbuXmlHelper, 1);
	g_autoptr(GMarkupParseContext) ctx = NULL;

	helper->out = g_string_sized_new (text_len);
	helper->self = self;

	ctx = g_markup_parse_context_new (&parser, 0, helper, NULL);
	if (!g_markup_parse_context_parse (ctx, text, text_len, error))
		return NULL;

	return g_steal_pointer (&helper->out);
}

void
sbu_xml_modifier_replace_cdata (SbuXmlModifier *self,
				const gchar *id,
				const gchar *value)
{
	g_hash_table_insert (self->hash_cdata, g_strdup (id), g_strdup (value));
}

void
sbu_xml_modifier_replace_attr (SbuXmlModifier *self,
			       const gchar *id,
			       const gchar *attr,
			       const gchar *value)
{
	g_hash_table_insert (self->hash_cdata,
			     g_strdup_printf ("%s-%s", id, attr),
			     g_strdup (value));
}

static void
sbu_xml_modifier_finalize (GObject *object)
{
	SbuXmlModifier *self = SBU_XML_MODIFIER (object);

	g_hash_table_unref (self->hash_cdata);
	g_hash_table_unref (self->hash_attrs);

	G_OBJECT_CLASS (sbu_xml_modifier_parent_class)->finalize (object);
}

static void
sbu_xml_modifier_init (SbuXmlModifier *self)
{
	self->hash_cdata = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	self->hash_attrs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
sbu_xml_modifier_class_init (SbuXmlModifierClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = sbu_xml_modifier_finalize;
}

SbuXmlModifier *
sbu_xml_modifier_new (void)
{
	SbuXmlModifier *self;
	self = g_object_new (SBU_TYPE_XML_MODIFIER, NULL);
	return SBU_XML_MODIFIER (self);
}
