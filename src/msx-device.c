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

#include "msx-common.h"
#include "msx-device.h"

#define MSX_DEVICE_TIMEOUT	5000

struct _MsxDevice
{
	GObject			 parent_instance;
	GUsbDevice		*usb_device;
	MsxDatabase		*database;
	gchar			*serial_number;
	gchar			*firmware_version1;
	gchar			*firmware_version2;
	guint			 poll_id;
	guint			 poll_timeout;
};

G_DEFINE_TYPE (MsxDevice, msx_device, G_TYPE_OBJECT)

static guint16
msx_crc_half (const guint8 *pin, guint8 len)
{
	const guint8 *ptr;
	guint16 crc;
	guint8 da;
	guint8 crc_hi;
	guint8 crc_lo;
	guint16 crc_ta[16] = {
		0x0000, 0x1021, 0x2042, 0x3063,
		0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b,
		0xc18c, 0xd1ad, 0xe1ce, 0xf1ef
	};
	ptr = pin;
	crc = 0;

	while (len-- != 0)  {
		da = ((guint8)(crc>>8))>>4;
		crc <<= 4;
		crc ^= crc_ta[da^(*ptr>>4)];
		da = ((guint8) (crc>>8))>>4;
		crc <<= 4;
		crc ^= crc_ta[da^(*ptr&0x0f)];
		ptr++;
	}
	crc_lo = crc;
	crc_hi = (guint8) (crc >> 8);

	if (crc_lo == 0x28 || crc_lo == 0x0d || crc_lo == 0x0a)
		crc_lo++;
	if (crc_hi == 0x28 || crc_hi == 0x0d || crc_hi == 0x0a)
		crc_hi++;
	crc = ((guint16) crc_hi) << 8;
	crc += crc_lo;
	return crc;
}

static void
msx_dump_raw (const gchar *title, const guint8 *data, gsize len)
{
	g_autoptr(GString) str = g_string_new (NULL);
	if (len == 0)
		return;
	g_string_append_printf (str, "%s:", title);
	for (gsize i = strlen (title); i < 16; i++)
		g_string_append (str, " ");
	for (gsize i = 0; i < len; i++) {
		g_string_append_printf (str, "%02x ", data[i]);
		if (i > 0 && i % 32 == 0)
			g_string_append (str, "\n");
	}
	g_debug ("%s", str->str);
}

static guint
msx_device_packet_count_data (const guint8 *buf, gsize len)
{
	for (guint j = 0; j < len; j++) {
		if (buf[j] == '\r')
			return j;
	}
	return 8;
}

GBytes *
msx_device_send_command (MsxDevice *self, const gchar *cmd, GError **error)
{
	gsize actual_len = 0;
	gsize idx = 0;
	gsize len;
	guint16 crc;
	guint8 buf2[256];
	guint8 buf[8];

	/* copy in the command */
	memset (buf, 0x00, sizeof (buf));
	len = strlen (cmd);
	memcpy (buf, cmd, len);

	/* copy in the footer: CRC then newline */
	crc = GUINT16_TO_BE (msx_crc_half (buf, len));
	g_debug ("crc = %04x", crc);
	memcpy (buf + len, &crc, 2);
	buf[len + 2] = '\r';

	/* send */
	msx_dump_raw ("host->self", buf, 8);
	if (!g_usb_device_control_transfer (self->usb_device,
					    G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
					    G_USB_DEVICE_REQUEST_TYPE_CLASS,
					    G_USB_DEVICE_RECIPIENT_INTERFACE,
					    0x9, 0x200, 0,
					    buf, 8, &actual_len,
					    MSX_DEVICE_TIMEOUT,
					    NULL,
					    error)) {
		g_prefix_error (error, "failed to send data: ");
		return FALSE;
	}
	if (actual_len != 8) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "only sent %" G_GSIZE_FORMAT " bytes",
			     actual_len);
		return FALSE;
	}

	/* recieve */
	for (guint i = 0; i < 20; i++) {
		gsize data_valid;

		memset (buf, 0x00, sizeof (buf));
		if (!g_usb_device_interrupt_transfer (self->usb_device,
						      0x81,
						      buf,
						      sizeof (buf),
						      &actual_len,
						      MSX_DEVICE_TIMEOUT,
						      NULL,
						      error)) {
			g_prefix_error (error, "failed to get data: ");
			return FALSE;
		}

		/* check message was long enough to parse */
		if (actual_len != 8) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "only recieved %" G_GSIZE_FORMAT " bytes",
				     actual_len);
			return FALSE;
		}

		msx_dump_raw ("self->host", buf, actual_len);
		data_valid = msx_device_packet_count_data (buf, actual_len);
		g_debug ("data_valid = %" G_GSIZE_FORMAT, data_valid);
		memcpy (buf2 + idx, buf, data_valid);
		idx += data_valid;
		if (data_valid <= 7)
			break;
	}

	/* check checksum of recieved message */
	crc = GUINT16_TO_BE (msx_crc_half (buf2, idx - 2));
	if (memcmp (&crc, buf2 + idx - 2, 2) != 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "failed checksum, expected %04x",
			     crc);
		return FALSE;
	}

	/* check first char */
	if (buf2[0] != '(') {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "invalid response start, expected '('");
		return FALSE;
	}

	return g_bytes_new (buf2 + 1, idx - 3);
}

static gboolean
msx_device_rescan_protocol (MsxDevice *self, GError **error)
{
	gconstpointer data;
	gsize len = 0;
	g_autoptr(GBytes) response = NULL;

	response = msx_device_send_command (self, "QPI", error);
	if (response == NULL) {
		g_prefix_error (error, "failed to get protocol version: ");
		return FALSE;
	}
	data = g_bytes_get_data (response, &len);
	if (len != 4) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "QPI data invalid, got %" G_GSIZE_FORMAT " bytes",
			     len);
		return FALSE;
	}
	if (memcmp ("PI30", data, len) != 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "QPI data invalid, got '%s'",
			     (const gchar *) data);
		return FALSE;
	}
	return TRUE;
}

static gboolean
msx_device_rescan_serial_number (MsxDevice *self, GError **error)
{
	gconstpointer data;
	gsize len = 0;
	g_autoptr(GBytes) response = NULL;

	response = msx_device_send_command (self, "QID", error);
	if (response == NULL) {
		g_prefix_error (error, "failed to get serial number: ");
		return FALSE;
	}
	data = g_bytes_get_data (response, &len);
	if (len != 14) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "QID data invalid, got %" G_GSIZE_FORMAT " bytes",
			     len);
		return FALSE;
	}
	self->serial_number = g_strdup (data);
	return TRUE;
}

typedef struct {
	gsize		 off;
	const gchar	*key;
} MsxDeviceBufferOffsets;

static gboolean
msx_device_buffer_parse (MsxDevice *self, GBytes *response,
			 MsxDeviceBufferOffsets *offsets,
			 GError **error)
{
	const gchar *data;
	gsize len = 0;
	guint i;

	/* check the size */
	for (i = 0; offsets[i].key != NULL; i++);
	data = g_bytes_get_data (response, &len);
	if (len != offsets[i].off) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "got %" G_GSIZE_FORMAT " bytes, expected %" G_GSIZE_FORMAT,
			     len, offsets[i].off);
		return FALSE;
	}

	/* parse each value */
	for (i = 0; offsets[i].key != NULL; i++) {
		gint val = msx_common_parse_int (data, offsets[i].off, len, error);
		if (val == G_MAXINT) {
			g_prefix_error (error,
					"failed to parse %s @%02x: ",
					offsets[i].key, (guint) offsets[i].off);
			return FALSE;
		}
		/* add to the database */
		if (self->database != NULL) {
			if (!msx_database_save_value (self->database,
						      offsets[i].key,
						      val,
						      error))
				return FALSE;
		}
	}
	return TRUE;
}

static gboolean
msx_device_rescan_device_rating (MsxDevice *self, GError **error)
{
	g_autoptr(GBytes) response = NULL;
	MsxDeviceBufferOffsets buffer_offsets[] = {
		{ 0x00,		"GridRatingVoltage" },
		{ 0x06,		"GridRatingCurrent" },
		{ 0x0b,		"AcOutputRatingVoltage" },
		{ 0x11,		"AcOutputRatingFrequency" },
		{ 0x16,		"AcOutputRatingCurrent" },
		{ 0x1b,		"AcOutputRatingApparentPower" },
		{ 0x20,		"AcOutputRatingActivePower" },
		{ 0x25,		"BatteryRatingVoltage" },
		{ 0x2a,		"BatteryRechargeVoltage" },
		{ 0x2f,		"BatteryUnderVoltage" },
		{ 0x34,		"BatteryBulkVoltage" },
		{ 0x39,		"BatteryFloatVoltage" },
		{ 0x3e,		"BatteryType" },
		{ 0x40,		"PresentMaxAcChargingCurrent" },
		{ 0x43,		"PresentMaxChargingCurrent" },
		{ 0x46,		"InputVoltageRange" },
		{ 0x48,		"OutputSourcePriority" },
		{ 0x4a,		"ChargerSourcePriority" },
		{ 0x4c,		"ParallelMaxNum" },
		{ 0x4e,		"MachineType" },
		{ 0x51,		"Topology" },
		{ 0x53,		"OutputMode" },
		{ 0x55,		"BatteryRedischargeVoltage" },
		{ 0x5a,		"PvOkConditionForParallel" },
		{ 0x5c,		"PvPowerBalance" },
		{ 0x5d,		NULL }
	};

	/* parse the data buffer */
	response = msx_device_send_command (self, "QPIRI", error);
	if (response == NULL) {
		g_prefix_error (error, "failed to get device rating: ");
		return FALSE;
	}
	if (!msx_device_buffer_parse (self, response, buffer_offsets, error)) {
		g_prefix_error (error, "QPIRI data invalid: ");
		return FALSE;
	}
	return TRUE;
}

static gboolean
msx_device_rescan_firmware_versions (MsxDevice *self, GError **error)
{
	gconstpointer data;
	gsize len = 0;
	g_autoptr(GBytes) response1 = NULL;
	g_autoptr(GBytes) response2 = NULL;

	/* main CPU firmware version inquiry */
	response1 = msx_device_send_command (self, "QVFW", error);
	if (response1 == NULL) {
		g_prefix_error (error, "failed to get CPU version: ");
		return FALSE;
	}
	data = g_bytes_get_data (response1, &len);
	if (len != 14) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "QVFW data invalid, got %" G_GSIZE_FORMAT " bytes",
			     len);
		return FALSE;
	}
	self->firmware_version1 = g_strdup ((const gchar *) data + 6);

	/* secondary CPU firmware version inquiry */
	response2 = msx_device_send_command (self, "QVFW2", error);
	if (response2 == NULL) {
		g_prefix_error (error, "failed to get CPU version: ");
		return FALSE;
	}
	data = g_bytes_get_data (response2, &len);
	if (len != 15) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "QVFW2 data invalid, got %" G_GSIZE_FORMAT " bytes",
			     len);
		return FALSE;
	}
	self->firmware_version2 = g_strdup ((const gchar *) data + 7);

	return TRUE;
}

static gboolean
msx_device_rescan_runtime (MsxDevice *self, GError **error)
{
	if (!msx_device_rescan_device_rating (self, error))
		return FALSE;
	return TRUE;
}

static gboolean
msx_device_poll_cb (gpointer user_data)
{
	MsxDevice *self = MSX_DEVICE (user_data);
	g_autoptr(GError) error = NULL;

	/* rescan stuff that can change at runtime */
	g_debug ("poll %s", self->serial_number);
	if (!msx_device_rescan_runtime (self, &error))
		g_warning ("failed to rescan: %s", error->message);

	return TRUE;
}

static void
msx_device_poll_start (MsxDevice *self)
{
	if (self->poll_id != 0)
		g_source_remove (self->poll_id);
	self->poll_id = g_timeout_add_seconds (self->poll_timeout, msx_device_poll_cb, self);
}

static void
msx_device_poll_stop (MsxDevice *self)
{
	if (self->poll_id == 0)
		return;
	g_source_remove (self->poll_id);
	self->poll_id = 0;
}

gboolean
msx_device_open (MsxDevice *self, GError **error)
{
	g_debug ("opening device");
	if (!g_usb_device_open (self->usb_device, error)) {
		g_prefix_error (error, "failed to open self: ");
		return FALSE;
	}

	g_debug ("claiming interface");
	if (!g_usb_device_claim_interface (self->usb_device, 0x00,
					   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					   error)) {
		g_prefix_error (error, "failed to claim interface: ");
		return FALSE;
	}

	/* rescan static things */
	if (!msx_device_rescan_protocol (self, error))
		return FALSE;
	if (!msx_device_rescan_serial_number (self, error))
		return FALSE;
	if (!msx_device_rescan_firmware_versions (self, error))
		return FALSE;

	/* initial try */
	if (!msx_device_rescan_runtime (self, error))
		return FALSE;

	/* set up initial poll */
	msx_device_poll_start (self);
	return TRUE;
}

gboolean
msx_device_close (MsxDevice *self, GError **error)
{
	g_debug ("releasing interface");
	if (!g_usb_device_release_interface (self->usb_device, 0x00,
					     G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					     error)) {
		g_prefix_error (error, "failed to claim interface: ");
		return FALSE;
	}

	g_debug ("closing device");
	if (!g_usb_device_close (self->usb_device, error)) {
		g_prefix_error (error, "failed to close self: ");
		return FALSE;
	}
	return TRUE;
}

const gchar *
msx_device_get_serial_number (MsxDevice *self)
{
	return self->serial_number;
}

const gchar *
msx_device_get_firmware_version1 (MsxDevice *self)
{
	return self->firmware_version1;
}

const gchar *
msx_device_get_firmware_version2 (MsxDevice *self)
{
	return self->firmware_version2;
}

void
msx_device_set_database	(MsxDevice *self, MsxDatabase *database)
{
	g_set_object (&self->database, database);
}

static void
msx_device_finalize (GObject *object)
{
	MsxDevice *self = MSX_DEVICE (object);

	msx_device_poll_stop (self);

	g_free (self->serial_number);
	g_free (self->firmware_version1);
	g_free (self->firmware_version2);
	if (self->database != NULL)
		g_object_unref (self->database);
	g_object_unref (self->usb_device);

	G_OBJECT_CLASS (msx_device_parent_class)->finalize (object);
}

static void
msx_device_init (MsxDevice *self)
{
	self->poll_timeout = 5;
}

static void
msx_device_class_init (MsxDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = msx_device_finalize;
}

/**
 * msx_device_new:
 *
 * Return value: a new MsxDevice object.
 **/
MsxDevice *
msx_device_new (GUsbDevice *usb_device)
{
	MsxDevice *self;
	self = g_object_new (MSX_TYPE_DEVICE, NULL);
	self->usb_device = g_object_ref (usb_device);
	return MSX_DEVICE (self);
}
