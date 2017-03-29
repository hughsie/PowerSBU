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

#include <math.h>
#include <string.h>
#include <gio/gio.h>

#include "msx-common.h"

gint
msx_common_parse_int (const gchar *buf, gsize off, gssize buflen, GError **error)
{
	gboolean allowed_decimal = TRUE;
	gboolean do_decimal = FALSE;
	gint val1 = 0;
	gint val2 = 0;
	guint i;
	guint j = 0;

	/* invalid */
	if (buf == NULL || buf[0] == '\0') {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "no data");
		return G_MAXINT;
	}

	/* auto detect */
	if (buflen < 0)
		buflen = strlen (buf);

	for (i = off; i < buflen; i++) {

		/* one decimal allowed */
		if (allowed_decimal && buf[i] == '.') {
			do_decimal = TRUE;
			break;
		}

		/* end of field, and special case */
		if (buf[i] == ' ' || buf[i] == '-')
			break;

		/* too many chars */
		if (i - off > 2)
			allowed_decimal = FALSE;
		if (i - off > 4) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "major number too many chars: %i", val1);
			return G_MAXINT;
		}

		/* number */
		val1 *= 10;
		if (buf[i] >= '0' && buf[i] <= '9') {
			val1 += buf[i] - '0';
			continue;
		}

		/* invalid */
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "invalid data in major [%02x]", (guint) buf[i]);
		return G_MAXINT;
	}

	if (do_decimal) {
		for (j = i + 1; j < buflen; j++) {

			/* too many chars */
			if (j - i > 3) {
				g_set_error (error,
					     G_IO_ERROR,
					     G_IO_ERROR_INVALID_DATA,
					     "major number too many chars: %i", val2);
				return G_MAXINT;
			}

			/* end of field */
			if (buf[j] == ' ')
				break;

			/* number */
			val2 *= 10;
			if (buf[j] >= '0' && buf[j] <= '9') {
				val2 += buf[j] - '0';
				continue;
			}

			/* invalid */
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "invalid data in minor [%02x]", (guint) buf[i]);
			return G_MAXINT;
		}
	}

	/* raise units to the right power */
	for (i = j - i; i < 4; i++)
		val2 *= 10;

	/* success */
	return (val1 * 1000) + val2;
}

