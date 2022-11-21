/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SBU_TYPE_XML_MODIFIER (sbu_xml_modifier_get_type())

G_DECLARE_FINAL_TYPE(SbuXmlModifier, sbu_xml_modifier, SBU, XML_MODIFIER, GObject)

SbuXmlModifier *
sbu_xml_modifier_new(void);
void
sbu_xml_modifier_replace_cdata(SbuXmlModifier *self, const gchar *id, const gchar *value);
void
sbu_xml_modifier_replace_attr(SbuXmlModifier *self,
			      const gchar *id,
			      const gchar *attr,
			      const gchar *value);
GString *
sbu_xml_modifier_process(SbuXmlModifier *self, const gchar *text, gssize text_len, GError **error);
