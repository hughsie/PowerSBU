/*
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 *
 */

#include "config.h"

#include <math.h>

#include "sbu-common.h"
#include "sbu-config.h"
#include "sbu-database.h"
#include "sbu-device.h"
#include "sbu-dummy-plugin.h"
#include "sbu-manager.h"
#include "sbu-msx-plugin.h"

struct _SbuManager {
	GObject parent_instance;
	guint poll_id;
	guint poll_interval;
	GPtrArray *plugins;
	GPtrArray *devices;
	SbuDatabase *database;
};

G_DEFINE_TYPE(SbuManager, sbu_manager, G_TYPE_OBJECT)

enum { SIGNAL_CHANGED, SIGNAL_LAST };

static guint signals[SIGNAL_LAST] = {0};

GPtrArray *
sbu_manager_get_devices(SbuManager *self)
{
	return self->devices;
}

SbuDevice *
sbu_manager_get_device_by_id(SbuManager *self, const gchar *device_id, GError **error)
{
	for (guint i = 0; i < self->devices->len; i++) {
		SbuDevice *device = g_ptr_array_index(self->devices, i);
		if (g_strcmp0(device_id, sbu_device_get_id(device)) == 0)
			return g_object_ref(device);
	}
	g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "no device ID %s", device_id);
	return NULL;
}

static gboolean
sbu_manager_poll_cb(gpointer user_data)
{
	SbuManager *self = SBU_MANAGER(user_data);

	/* rescan stuff that can change at runtime */
	for (guint i = 0; i < self->devices->len; i++) {
		SbuDevice *device = g_ptr_array_index(self->devices, i);
		g_autoptr(GError) error = NULL;
		if (!sbu_device_refresh(device, &error)) {
			g_warning("failed to refresh %s: %s",
				  sbu_device_get_id(device),
				  error->message);
		}
	}
	for (guint i = 0; i < self->plugins->len; i++) {
		SbuPlugin *plugin = g_ptr_array_index(self->plugins, i);
		g_autoptr(GError) error = NULL;
		if (!sbu_plugin_get_enabled(plugin))
			continue;
		if (!sbu_plugin_refresh(plugin, NULL, &error)) {
			g_warning("failed to refresh %s: %s",
				  sbu_plugin_get_name(plugin),
				  error->message);
		}
	}

	g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
	return TRUE;
}

static void
sbu_manager_poll_start(SbuManager *self)
{
	if (self->poll_id != 0)
		g_source_remove(self->poll_id);
	self->poll_id = g_timeout_add_seconds(self->poll_interval, sbu_manager_poll_cb, self);
}

static void
sbu_manager_poll_stop(SbuManager *self)
{
	if (self->poll_id == 0)
		return;
	g_source_remove(self->poll_id);
	self->poll_id = 0;
}

static void
sbu_manager_plugins_update_metadata_cb(SbuPlugin *plugin,
				       SbuDevice *device,
				       const gchar *key,
				       gint value,
				       SbuManager *self)
{
	g_autoptr(GError) error = NULL;
	if (!sbu_database_save_value(self->database, sbu_device_get_id(device), key, value, &error))
		g_warning("%s", error->message);
}

static void
sbu_manager_plugins_remove_device_cb(SbuPlugin *plugin, SbuDevice *device, SbuManager *self)
{
	g_debug("removing device %s", sbu_device_get_id(device));
	g_ptr_array_remove(self->devices, device);
	if (self->devices->len == 0)
		sbu_manager_poll_stop(self);
}

static void
sbu_manager_save_history(SbuManager *self,
			 SbuDevice *device,
			 const gchar *id,
			 const gchar *propname,
			 GObject *obj)
{
	gint value = -1;

	/* boolean */
	if (g_strcmp0(propname, "active") == 0) {
		gboolean tmp;
		g_object_get(obj, propname, &tmp, NULL);
		value = (guint)tmp;

		/* double */
	} else if (g_strcmp0(propname, "power") == 0 || g_strcmp0(propname, "current") == 0 ||
		   g_strcmp0(propname, "voltage") == 0 || g_strcmp0(propname, "frequency") == 0) {
		gdouble tmp;
		g_object_get(obj, propname, &tmp, NULL);
		value = tmp * 1000.f;
	}

	/* save to database */
	if (value != -1) {
		g_autofree gchar *key = NULL;
		g_autoptr(GError) error = NULL;
		key = g_strdup_printf("%s:%s", id, propname);
		if (!sbu_database_save_value(self->database,
					     sbu_device_get_id(device),
					     key,
					     value,
					     &error))
			g_warning("%s", error->message);
	}
}

GVariant *
sbu_manager_get_history(SbuManager *self,
			SbuDevice *device,
			const gchar *arg_key,
			guint64 arg_start,
			guint64 arg_end,
			guint limit,
			GError **error)
{
	GVariantBuilder builder;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GPtrArray) results2 = NULL;

	/* get all results between the two times */
	g_debug("handling GetHistory %s for %" G_GUINT64_FORMAT "->%" G_GUINT64_FORMAT,
		arg_key,
		arg_start,
		arg_end);
	results = sbu_database_query(self->database,
				     sbu_device_get_id(device),
				     arg_key,
				     arg_start,
				     arg_end,
				     error);
	if (results == NULL)
		return NULL;

	/* no filter */
	if (limit == 0) {
		results2 = g_ptr_array_ref(results);

		/* just one value */
	} else if (limit == 1) {
		SbuDatabaseItem *item;
		gdouble ave_acc = 0;
		gint ts = 0;
		results2 = g_ptr_array_new_with_free_func(g_free);
		for (guint i = 0; i < results->len; i++) {
			item = g_ptr_array_index(results, i);
			ave_acc += item->val;
			ts = item->ts;
		}
		item = g_new0(SbuDatabaseItem, 1);
		item->ts = ts;
		item->val = ave_acc / results->len;
		g_ptr_array_add(results2, item);

		/* bin into averaged groups */
	} else {
		gint ts_last_added = 0;
		guint ave_cnt = 0;
		gdouble ave_acc = 0;
		gint64 interval = (arg_end - arg_start) / (limit - 1);

		results2 = g_ptr_array_new_with_free_func(g_free);
		for (guint i = 0; i < results->len; i++) {
			SbuDatabaseItem *item = g_ptr_array_index(results, i);

			if (ts_last_added == 0)
				ts_last_added = item->ts;

			/* first and last points */
			if (i == 0 || i == results->len - 1) {
				SbuDatabaseItem *item2 = g_new0(SbuDatabaseItem, 1);
				item2->ts = item->ts;
				item2->val = item->val;
				g_ptr_array_add(results2, item2);
				continue;
			}

			/* add to moving average */
			ave_acc += item->val;
			ave_cnt += 1;

			/* more than the interval */
			if (item->ts - ts_last_added > interval) {
				SbuDatabaseItem *item2 = g_new0(SbuDatabaseItem, 1);
				item2->ts = item->ts;
				item2->val = ave_acc / (gdouble)ave_cnt;
				g_ptr_array_add(results2, item2);
				ts_last_added = item->ts;

				/* reset moving average */
				ave_cnt = 0;
				ave_acc = 0.f;
			}
		}
	}

	/* return as a GVariant */
	g_variant_builder_init(&builder, G_VARIANT_TYPE("(a(td))"));
	g_variant_builder_open(&builder, G_VARIANT_TYPE("a(td)"));
	for (guint i = 0; i < results2->len; i++) {
		SbuDatabaseItem *item = g_ptr_array_index(results2, i);
		gdouble val = item->val;
		if (fabs(val) > 1.1f)
			val /= 1000.f;
		g_variant_builder_add(&builder, "(td)", item->ts, val);
	}
	g_variant_builder_close(&builder);
	return g_variant_builder_end(&builder);
}

static void
sbu_manager_node_notify_cb(SbuNode *n, GParamSpec *pspec, gpointer user_data)
{
	SbuManager *self = SBU_MANAGER(user_data);
	g_debug("changed %s:%s", sbu_node_get_id(n), g_param_spec_get_name(pspec));
	sbu_manager_save_history(self,
				 g_ptr_array_index(self->devices, 0),
				 sbu_node_get_id(n),
				 g_param_spec_get_name(pspec),
				 G_OBJECT(n));
	g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
}

static void
sbu_manager_link_notify_cb(SbuLink *l, GParamSpec *pspec, gpointer user_data)
{
	SbuManager *self = SBU_MANAGER(user_data);
	g_debug("changed %s:%s", sbu_link_get_id(l), g_param_spec_get_name(pspec));
	sbu_manager_save_history(self,
				 g_ptr_array_index(self->devices, 0),
				 sbu_link_get_id(l),
				 g_param_spec_get_name(pspec),
				 G_OBJECT(l));
	g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
}

static void
sbu_manager_plugins_add_device_cb(SbuPlugin *plugin, SbuDevice *device, SbuManager *self)
{
	GPtrArray *array;

	/* just use the array position as the ID */
	if (sbu_device_get_id(device) == NULL) {
		g_autofree gchar *id = g_strdup_printf("%u", self->devices->len);
		sbu_device_set_id(device, id);
	}
	g_debug("adding device %s", sbu_device_get_id(device));
	g_ptr_array_add(self->devices, g_object_ref(device));

	/* watch all links and nodes */
	array = sbu_device_get_links(device);
	for (guint i = 0; i < array->len; i++) {
		SbuLink *link = g_ptr_array_index(array, i);
		g_signal_connect(link, "notify", G_CALLBACK(sbu_manager_link_notify_cb), self);
	}
	array = sbu_device_get_nodes(device);
	for (guint i = 0; i < array->len; i++) {
		SbuNode *node = g_ptr_array_index(array, i);
		g_signal_connect(node, "notify", G_CALLBACK(sbu_manager_node_notify_cb), self);
	}

	/* set up initial poll */
	sbu_manager_poll_start(self);
}

gboolean
sbu_manager_setup(SbuManager *self, GError **error)
{
	g_autofree gchar *location = NULL;
	g_autoptr(SbuConfig) config = sbu_config_new();

	/* use the system-wide database */
	location = sbu_config_get_string(config, "DatabaseLocation", error);
	if (location == NULL)
		return FALSE;
	sbu_database_set_location(self->database, location);
	if (!sbu_database_open(self->database, error)) {
		g_prefix_error(error, "failed to open database %s: ", location);
		return FALSE;
	}

	/* set the poll interval */
	self->poll_interval = sbu_config_get_integer(config, "DevicePollInterval", error);
	if (self->poll_interval == 0)
		return FALSE;

	/* enable test device */
	if (sbu_config_get_boolean(config, "EnableDummyDevice", NULL))
		g_setenv("SBU_DUMMY_ENABLE", "", TRUE);
	for (guint i = 0; i < self->plugins->len; i++) {
		SbuPlugin *plugin = g_ptr_array_index(self->plugins, i);
		if (!sbu_plugin_get_enabled(plugin))
			continue;
		g_signal_connect(plugin,
				 "update-metadata",
				 G_CALLBACK(sbu_manager_plugins_update_metadata_cb),
				 self);
		g_signal_connect(plugin,
				 "add-device",
				 G_CALLBACK(sbu_manager_plugins_add_device_cb),
				 self);
		g_signal_connect(plugin,
				 "remove-device",
				 G_CALLBACK(sbu_manager_plugins_remove_device_cb),
				 self);
		if (!sbu_plugin_setup(plugin, NULL, error))
			return FALSE;
	}

	/* success */
	return TRUE;
}

static void
sbu_manager_finalize(GObject *object)
{
	SbuManager *self = SBU_MANAGER(object);

	sbu_manager_poll_stop(self);

	g_object_unref(self->database);
	g_ptr_array_unref(self->plugins);
	g_ptr_array_unref(self->devices);
	G_OBJECT_CLASS(sbu_manager_parent_class)->finalize(object);
}

static void
sbu_manager_init(SbuManager *self)
{
	self->database = sbu_database_new();
	self->devices = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	self->plugins = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);

	g_ptr_array_add(self->plugins, g_object_new(SBU_TYPE_DUMMY_PLUGIN, NULL));
	g_ptr_array_add(self->plugins, g_object_new(SBU_TYPE_MSX_PLUGIN, NULL));
}

static void
sbu_manager_class_init(SbuManagerClass *klass_manager)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass_manager);
	object_class->finalize = sbu_manager_finalize;

	signals[SIGNAL_CHANGED] = g_signal_new("changed",
					       G_TYPE_FROM_CLASS(object_class),
					       G_SIGNAL_RUN_LAST,
					       0,
					       NULL,
					       NULL,
					       g_cclosure_marshal_VOID__VOID,
					       G_TYPE_NONE,
					       0);
}

SbuManager *
sbu_manager_new(void)
{
	SbuManager *manager;
	manager = g_object_new(SBU_TYPE_MANAGER, NULL);
	return SBU_MANAGER(manager);
}
