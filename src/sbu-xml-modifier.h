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


#ifndef __SBU_XML_MODIFIER_H
#define __SBU_XML_MODIFIER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define SBU_TYPE_XML_MODIFIER (sbu_xml_modifier_get_type ())

G_DECLARE_FINAL_TYPE (SbuXmlModifier, sbu_xml_modifier, SBU, XML_MODIFIER, GObject)

SbuXmlModifier	*sbu_xml_modifier_new		(void);
void		 sbu_xml_modifier_replace_cdata	(SbuXmlModifier	*self,
						 const gchar	*id,
						 const gchar	*value);
void		 sbu_xml_modifier_replace_attr	(SbuXmlModifier	*self,
						 const gchar	*id,
						 const gchar	*attr,
						 const gchar	*value);
GString		*sbu_xml_modifier_process	(SbuXmlModifier	*self,
						 const gchar	*text,
						 gssize		 text_len,
						 GError		**error);

G_END_DECLS

#endif /* __SBU_XML_MODIFIER_H */
