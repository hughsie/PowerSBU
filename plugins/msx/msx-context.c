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

#include "msx-context.h"
#include "msx-device.h"

#define MSX_CONTEXT_TIMEOUT	5000

struct _MsxContext
{
	GObject			 parent_instance;
	GUsbContext		*usb_context;
	GPtrArray		*devices;
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_LAST
};

static guint signals [SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (MsxContext, msx_context, G_TYPE_OBJECT)

GPtrArray *
msx_context_get_devices (MsxContext *self)
{
	return self->devices;
}

static void
msx_context_device_added_cb (GUsbContext *context, GUsbDevice *usb_device, MsxContext *self)
{
	MsxDevice *device;

	if (g_usb_device_get_vid (usb_device) != 0x0665)
		return;
	if (g_usb_device_get_pid (usb_device) != 0x5161)
		return;

	device = msx_device_new (usb_device);
	g_ptr_array_add (self->devices, device);
	g_signal_emit (self, signals[SIGNAL_ADDED], 0, device);
}

static void
msx_context_device_removed_cb (GUsbContext *context, GUsbDevice *usb_device, MsxContext *self)
{
	MsxDevice *device;

	if (g_usb_device_get_vid (usb_device) != 0x0665)
		return;
	if (g_usb_device_get_pid (usb_device) != 0x5161)
		return;

	device = g_ptr_array_index (self->devices, 0); // FIXME, use platform_id
	g_ptr_array_remove (self->devices, device);
	g_signal_emit (self, signals[SIGNAL_REMOVED], 0, device);
}

gboolean
msx_context_coldplug (MsxContext *self, GError **error)
{
	g_autoptr(GPtrArray) devices = NULL;

	if (self->usb_context != NULL)
		return TRUE;

	self->usb_context = g_usb_context_new (error);
	if (self->usb_context == NULL)
		return FALSE;

	/* add all devices */
	devices = g_usb_context_get_devices (self->usb_context);
	for (guint i = 0; i < devices->len; i++) {
		GUsbDevice *usb_device = g_ptr_array_index (devices, i);
		msx_context_device_added_cb (self->usb_context, usb_device, self);
	}

	/* watch */
	g_signal_connect (self->usb_context, "device-added",
			  G_CALLBACK (msx_context_device_added_cb), self);
	g_signal_connect (self->usb_context, "device-added",
			  G_CALLBACK (msx_context_device_removed_cb), self);

	return TRUE;
}

static void
msx_context_finalize (GObject *object)
{
	MsxContext *self = MSX_CONTEXT (object);

	if (self->usb_context != NULL)
		g_object_unref (self->usb_context);
	g_ptr_array_unref (self->devices);

	G_OBJECT_CLASS (msx_context_parent_class)->finalize (object);
}

static void
msx_context_init (MsxContext *self)
{
	self->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static void
msx_context_class_init (MsxContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = msx_context_finalize;

	signals [SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 1, MSX_TYPE_DEVICE);
	signals [SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, g_cclosure_marshal_generic,
			      G_TYPE_NONE, 1, MSX_TYPE_DEVICE);
}

/**
 * msx_context_new:
 *
 * Return value: a new MsxContext object.
 **/
MsxContext *
msx_context_new (void)
{
	MsxContext *self;
	self = g_object_new (MSX_TYPE_CONTEXT, NULL);
	return MSX_CONTEXT (self);
}

/**
 * msx_context_new:
 *
 * Return value: a new MsxContext object.
 **/
MsxContext *
msx_context_new_with_ctx (GUsbContext *usb_context)
{
	MsxContext *self;
	self = g_object_new (MSX_TYPE_CONTEXT, NULL);
	self->usb_context = g_object_ref (usb_context);
	return MSX_CONTEXT (self);
}
