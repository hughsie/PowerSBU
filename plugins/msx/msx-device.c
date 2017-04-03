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
	gchar			*serial_number;
	gchar			*firmware_version1;
	gchar			*firmware_version2;
	GHashTable		*hash;	/* MsxDeviceKey : int */
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

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
	MsxDeviceKey	 key;
} MsxDeviceBufferOffsets;

static void
msx_device_emit_changed (MsxDevice *self, MsxDeviceKey key, gint val)
{
	gpointer hash_key = GUINT_TO_POINTER (key);
	gint *val_ptr = g_hash_table_lookup (self->hash, hash_key);
	if (val_ptr == NULL) {
		val_ptr = g_new0 (gint, 1);
		g_hash_table_insert (self->hash, hash_key, val_ptr);
		g_debug ("cache add new %s=%i",
			 sbu_device_key_to_string (key), val);
	} else if (*val_ptr == val) {
		g_debug ("cache ignore duplicate %s=%i",
			 sbu_device_key_to_string (key), val);
		return;
	}

	/* emit and THEN save new value so we can get the old value */
	g_signal_emit (self, signals[SIGNAL_CHANGED], 0, key, val);
	*val_ptr = val;
}

gint
msx_device_get_value (MsxDevice *self, MsxDeviceKey key)
{
	gpointer hash_key = GUINT_TO_POINTER (key);
	gint *val_ptr = g_hash_table_lookup (self->hash, hash_key);
	if (val_ptr == NULL)
		return 0;
	return *val_ptr;
}

static gboolean
msx_device_buffer_parse (MsxDevice *self, GBytes *response,
			 MsxDeviceBufferOffsets *offsets,
			 GError **error)
{
	const gchar *data;
	gsize len = 0;
	guint i;

	/* check the size */
	for (i = 0; offsets[i].key != MSX_DEVICE_KEY_UNKNOWN; i++);
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
	for (i = 0; offsets[i].key != MSX_DEVICE_KEY_UNKNOWN; i++) {
		gint val = msx_common_parse_int (data, offsets[i].off, len, error);
		if (val == G_MAXINT) {
			g_prefix_error (error,
					"failed to parse %s @%02x: ",
					sbu_device_key_to_string (offsets[i].key),
					(guint) offsets[i].off);
			return FALSE;
		}
		msx_device_emit_changed (self, offsets[i].key, val);
	}
	return TRUE;
}

static gboolean
msx_device_buffer_parse_bits (MsxDevice *self, GBytes *response,
			      MsxDeviceBufferOffsets *offsets,
			      GError **error)
{
	/* parse each value */
	const gchar *data = g_bytes_get_data (response, NULL);
	for (guint i = 0; offsets[i].key != MSX_DEVICE_KEY_UNKNOWN; i++) {
		if (data[offsets[i].off] == '0') {
			msx_device_emit_changed (self, offsets[i].key, 0);
		} else if (data[offsets[i].off] == '1') {
			msx_device_emit_changed (self, offsets[i].key, 1);
		} else {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "failed to parse %s @%02x: %c",
				     sbu_device_key_to_string (offsets[i].key),
				     (guint) offsets[i].off,
				     data[offsets[i].off]);
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
		{ 0x00,		MSX_DEVICE_KEY_GRID_RATING_VOLTAGE },
		{ 0x06,		MSX_DEVICE_KEY_GRID_RATING_CURRENT },
		{ 0x0b,		MSX_DEVICE_KEY_AC_OUTPUT_RATING_VOLTAGE },
		{ 0x11,		MSX_DEVICE_KEY_AC_OUTPUT_RATING_FREQUENCY },
		{ 0x16,		MSX_DEVICE_KEY_AC_OUTPUT_RATING_CURRENT },
		{ 0x1b,		MSX_DEVICE_KEY_AC_OUTPUT_RATING_APPARENT_POWER },
		{ 0x20,		MSX_DEVICE_KEY_AC_OUTPUT_RATING_ACTIVE_POWER },
		{ 0x25,		MSX_DEVICE_KEY_BATTERY_RATING_VOLTAGE },
		{ 0x2a,		MSX_DEVICE_KEY_BATTERY_RECHARGE_VOLTAGE },
		{ 0x2f,		MSX_DEVICE_KEY_BATTERY_UNDER_VOLTAGE },
		{ 0x34,		MSX_DEVICE_KEY_BATTERY_BULK_VOLTAGE },
		{ 0x39,		MSX_DEVICE_KEY_BATTERY_FLOAT_VOLTAGE },
		{ 0x3e,		MSX_DEVICE_KEY_BATTERY_TYPE },
		{ 0x40,		MSX_DEVICE_KEY_PRESENT_MAX_AC_CHARGING_CURRENT },
		{ 0x43,		MSX_DEVICE_KEY_PRESENT_MAX_CHARGING_CURRENT },
		{ 0x46,		MSX_DEVICE_KEY_INPUT_VOLTAGE_RANGE },
		{ 0x48,		MSX_DEVICE_KEY_OUTPUT_SOURCE_PRIORITY },
		{ 0x4a,		MSX_DEVICE_KEY_CHARGER_SOURCE_PRIORITY },
		{ 0x4c,		MSX_DEVICE_KEY_PARALLEL_MAX_NUM },
		{ 0x4e,		MSX_DEVICE_KEY_MACHINE_TYPE },
		{ 0x51,		MSX_DEVICE_KEY_TOPOLOGY },
		{ 0x53,		MSX_DEVICE_KEY_OUTPUT_MODE },
		{ 0x55,		MSX_DEVICE_KEY_BATTERY_REDISCHARGE_VOLTAGE },
		{ 0x5a,		MSX_DEVICE_KEY_PV_OK_CONDITION_FOR_PARALLEL },
		{ 0x5c,		MSX_DEVICE_KEY_PV_POWER_BALANCE },
		{ 0x5d,		MSX_DEVICE_KEY_UNKNOWN }
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
msx_device_rescan_device_flags (MsxDevice *self, GError **error)
{
	const gchar *data;
	gint val = 1;
	gsize len = 0;
	g_autoptr(GBytes) response = NULL;

	/* send request */
	response = msx_device_send_command (self, "QFLAG", error);
	if (response == NULL) {
		g_prefix_error (error, "failed to get device rating: ");
		return FALSE;
	}

	/* check the size */
	data = g_bytes_get_data (response, &len);
	if (len != 11) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "got %" G_GSIZE_FORMAT " bytes, expected 11",
			     len);
		return FALSE;
	}

	/* parse */
	for (gsize i = 0; i < len; i++) {
		switch (data[i]) {
		case 'D':
			val = 0;
			break;
		case 'E':
			val = 1;
			break;
		case 'a':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_ENABLE_BUZZER, val);
			break;
		case 'b':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_OVERLOAD_BYPASS_FUNCTION, val);
			break;
		case 'j':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_POWER_SAVE, val);
			break;
		case 'k':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_LCD_DISPLAY_ESCAPE, val);
			break;
		case 'u':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_OVERLOAD_RESTART, val);
			break;
		case 'v':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_OVER_TEMPERATURE_RESTART, val);
			break;
		case 'x':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_LCD_BACKLIGHT, val);
			break;
		case 'y':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_ALARM_PRIMARY_SOURCE_INTERRUPT, val);
			break;
		case 'z':
			msx_device_emit_changed (self, MSX_DEVICE_KEY_FAULT_CODE_RECORD, val);
			break;
		default:
			g_warning ("failed to parse flag '%c'", data[i]);
			break;
		}
	}
	return TRUE;
}

static gboolean
msx_device_rescan_device_warning_status (MsxDevice *self, GError **error)
{
	const gchar *data;
	gsize len = 0;
	g_autoptr(GBytes) response = NULL;
	struct {
		gboolean	 is_fault;
		const gchar	*msg;
	} error_codes[] = {
		{ TRUE,		"Reserved" },
		{ TRUE,		"Inverter fault" },
		{ TRUE,		"Bus Over" },
		{ TRUE,		"Bus Under" },
		{ TRUE,		"Bus Soft Fail" },
		{ FALSE,	"LINE_FAIL" },
		{ FALSE,	"OPVShort" },
		{ TRUE,		"Inverter voltage too low" },
		{ TRUE,		"Inverter voltage too high" },
		{ FALSE,	"Over temperature" },
		{ FALSE,	"Fan locked" },
		{ FALSE,	"Battery voltage high" },
		{ FALSE,	"Battery low alarm" },
		{ TRUE,		"Reserved" },
		{ FALSE,	"Battery under shutdown" },
		{ FALSE,	"Reserved" },
		{ FALSE,	"Overload" },
		{ FALSE,	"Eeprom" },
		{ TRUE,		"Inverter Over Current" },
		{ TRUE,		"Inverter Soft Fail" },
		{ TRUE,		"Self Test Fail" },
		{ TRUE,		"OP DC Voltage Over" },
		{ TRUE,		"Bat Open" },
		{ TRUE,		"Current Sensor Fail" },
		{ TRUE,		"Battery Short" },
		{ FALSE,	"Power limit" },
		{ FALSE,	"PV voltage high" },
		{ FALSE,	"MPPT overload fault" },
		{ FALSE,	"MPPT overload warning" },
		{ FALSE,	"Battery too low to charge" },
		{ TRUE,		"Reserved" },
		{ TRUE,		"Reserved" },
		{ FALSE,	NULL }
	};

	/* send request */
	response = msx_device_send_command (self, "QPIWS", error);
	if (response == NULL) {
		g_prefix_error (error, "failed to get device rating: ");
		return FALSE;
	}

	/* check the size */
	data = g_bytes_get_data (response, &len);
	if (len != 32) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "got %" G_GSIZE_FORMAT " bytes, expected 11",
			     len);
		return FALSE;
	}

	/* look for fatal errors */
	for (gsize i = len - 1; i > 0; i--) {
		if (data[i] != '1')
			continue;
		if (error_codes[i].is_fault || data[1] == '1') {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "inverter error: %s",
				     error_codes[i].msg);
			return FALSE;
		}
		g_warning ("%s", error_codes[i].msg);
	}
	return TRUE;
}

static gboolean
msx_device_rescan_device_general_status (MsxDevice *self, GError **error)
{
	g_autoptr(GBytes) response = NULL;
	MsxDeviceBufferOffsets buffer_offsets[] = {
		{ 0x00,		MSX_DEVICE_KEY_GRID_VOLTAGE },
		{ 0x06,		MSX_DEVICE_KEY_GRID_FREQUENCY },
		{ 0x0b,		MSX_DEVICE_KEY_AC_OUTPUT_VOLTAGE },
		{ 0x11,		MSX_DEVICE_KEY_AC_OUTPUT_FREQUENCY },
		{ 0x16,		MSX_DEVICE_KEY_AC_OUTPUT_POWER },
		{ 0x1b,		MSX_DEVICE_KEY_AC_OUTPUT_ACTIVE_POWER },
		{ 0x20,		MSX_DEVICE_KEY_MAXIMUM_POWER_PERCENTAGE },
		{ 0x24,		MSX_DEVICE_KEY_BUS_VOLTAGE },
		{ 0x28,		MSX_DEVICE_KEY_BATTERY_VOLTAGE },
		{ 0x2e,		MSX_DEVICE_KEY_BATTERY_CURRENT },
		{ 0x32,		MSX_DEVICE_KEY_BATTERY_CAPACITY },
		{ 0x36,		MSX_DEVICE_KEY_INVERTER_HEATSINK_TEMPERATURE },
		{ 0x3b,		MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY },
		{ 0x40,		MSX_DEVICE_KEY_PV_INPUT_VOLTAGE },
		{ 0x46,		MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC },
		{ 0x4c,		MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT },
		{ 0x6a,		MSX_DEVICE_KEY_UNKNOWN }
	};
	MsxDeviceBufferOffsets buffer_bits[] = {
		{ 0x52,		MSX_DEVICE_KEY_ADD_SBU_PRIORITY_VERSION },
		{ 0x53,		MSX_DEVICE_KEY_CONFIGURATION_STATUS_CHANGE },
		{ 0x54,		MSX_DEVICE_KEY_SCC_FIRMWARE_VERSION_UPDATED },
		{ 0x55,		MSX_DEVICE_KEY_LOAD_STATUS_ON },
		{ 0x56,		MSX_DEVICE_KEY_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING },
		{ 0x57,		MSX_DEVICE_KEY_CHARGING_ON },
		{ 0x58,		MSX_DEVICE_KEY_CHARGING_ON_SOLAR },
		{ 0x59,		MSX_DEVICE_KEY_CHARGING_ON_AC },
		{ 0x6a,		MSX_DEVICE_KEY_UNKNOWN }
	};

	/* parse the data buffer */
	response = msx_device_send_command (self, "QPIGS", error);
	if (response == NULL) {
		g_prefix_error (error, "failed to get device rating: ");
		return FALSE;
	}
	if (!msx_device_buffer_parse (self, response, buffer_offsets, error)) {
		g_prefix_error (error, "QPIGS data invalid: ");
		return FALSE;
	}
	if (!msx_device_buffer_parse_bits (self, response, buffer_bits, error)) {
		g_prefix_error (error, "QPIGS data invalid: ");
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

gboolean
msx_device_refresh (MsxDevice *self, GError **error)
{
	if (!msx_device_rescan_device_rating (self, error))
		return FALSE;
	if (!msx_device_rescan_device_general_status (self, error))
		return FALSE;
	if (!msx_device_rescan_device_flags (self, error))
		return FALSE;
	if (!msx_device_rescan_device_warning_status (self, error))
		return FALSE;
	return TRUE;
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
	if (!msx_device_refresh (self, error))
		return FALSE;

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

static void
msx_device_finalize (GObject *object)
{
	MsxDevice *self = MSX_DEVICE (object);

	g_free (self->serial_number);
	g_free (self->firmware_version1);
	g_free (self->firmware_version2);
	g_object_unref (self->usb_device);
	g_hash_table_unref (self->hash);

	G_OBJECT_CLASS (msx_device_parent_class)->finalize (object);
}

static void
msx_device_init (MsxDevice *self)
{
	self->hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

static void
msx_device_class_init (MsxDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = msx_device_finalize;

	signals [SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_INT);
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
