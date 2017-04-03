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

const gchar *
sbu_device_key_to_string (MsxDeviceKey key)
{
	if (key == MSX_DEVICE_KEY_GRID_RATING_VOLTAGE)
		return "GridRatingVoltage";
	if (key == MSX_DEVICE_KEY_GRID_RATING_CURRENT)
		return "GridRatingCurrent";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_RATING_VOLTAGE)
		return "AcOutputRatingVoltage";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_RATING_FREQUENCY)
		return "AcOutputRatingFrequency";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_RATING_CURRENT)
		return "AcOutputRatingCurrent";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_RATING_APPARENT_POWER)
		return "AcOutputRatingApparentPower";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_RATING_ACTIVE_POWER)
		return "AcOutputRatingActivePower";
	if (key == MSX_DEVICE_KEY_BATTERY_RATING_VOLTAGE)
		return "BatteryRatingVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_RECHARGE_VOLTAGE)
		return "BatteryRechargeVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_UNDER_VOLTAGE)
		return "BatteryUnderVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_BULK_VOLTAGE)
		return "BatteryBulkVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_FLOAT_VOLTAGE)
		return "BatteryFloatVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_TYPE)
		return "BatteryType";
	if (key == MSX_DEVICE_KEY_PRESENT_MAX_AC_CHARGING_CURRENT)
		return "PresentMaxAcChargingCurrent";
	if (key == MSX_DEVICE_KEY_PRESENT_MAX_CHARGING_CURRENT)
		return "PresentMaxChargingCurrent";
	if (key == MSX_DEVICE_KEY_INPUT_VOLTAGE_RANGE)
		return "InputVoltageRange";
	if (key == MSX_DEVICE_KEY_OUTPUT_SOURCE_PRIORITY)
		return "OutputSourcePriority";
	if (key == MSX_DEVICE_KEY_CHARGER_SOURCE_PRIORITY)
		return "ChargerSourcePriority";
	if (key == MSX_DEVICE_KEY_PARALLEL_MAX_NUM)
		return "ParallelMaxNum";
	if (key == MSX_DEVICE_KEY_MACHINE_TYPE)
		return "MachineType";
	if (key == MSX_DEVICE_KEY_TOPOLOGY)
		return "Topology";
	if (key == MSX_DEVICE_KEY_OUTPUT_MODE)
		return "OutputMode";
	if (key == MSX_DEVICE_KEY_BATTERY_REDISCHARGE_VOLTAGE)
		return "BatteryRedischargeVoltage";
	if (key == MSX_DEVICE_KEY_PV_OK_CONDITION_FOR_PARALLEL)
		return "PvOkConditionForParallel";
	if (key == MSX_DEVICE_KEY_PV_POWER_BALANCE)
		return "PvPowerBalance";
	if (key == MSX_DEVICE_KEY_GRID_VOLTAGE)
		return "GridVoltage";
	if (key == MSX_DEVICE_KEY_GRID_FREQUENCY)
		return "GridFrequency";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_VOLTAGE)
		return "AcOutputVoltage";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_FREQUENCY)
		return "AcOutputFrequency";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_POWER)
		return "AcOutputPower";
	if (key == MSX_DEVICE_KEY_AC_OUTPUT_ACTIVE_POWER)
		return "AcOutputActivePower";
	if (key == MSX_DEVICE_KEY_MAXIMUM_POWER_PERCENTAGE)
		return "MaximumPowerPercentage";
	if (key == MSX_DEVICE_KEY_BUS_VOLTAGE)
		return "BusVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_VOLTAGE)
		return "BatteryVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_CURRENT)
		return "BatteryCurrent";
	if (key == MSX_DEVICE_KEY_BATTERY_CAPACITY)
		return "BatteryCapacity";
	if (key == MSX_DEVICE_KEY_INVERTER_HEATSINK_TEMPERATURE)
		return "InverterHeatSinkTemperature";
	if (key == MSX_DEVICE_KEY_PV_INPUT_CURRENT_FOR_BATTERY)
		return "PvInputCurrentForBattery";
	if (key == MSX_DEVICE_KEY_PV_INPUT_VOLTAGE)
		return "PvInputVoltage";
	if (key == MSX_DEVICE_KEY_BATTERY_VOLTAGE_FROM_SCC)
		return "BatteryVoltageFromScc";
	if (key == MSX_DEVICE_KEY_BATTERY_DISCHARGE_CURRENT)
		return "BatteryDischargeCurrent";
	if (key == MSX_DEVICE_KEY_ADD_SBU_PRIORITY_VERSION)
		return "AddSbuPriorityVersion";
	if (key == MSX_DEVICE_KEY_CONFIGURATION_STATUS_CHANGE)
		return "ConfigurationStatusChange";
	if (key == MSX_DEVICE_KEY_SCC_FIRMWARE_VERSION_UPDATED)
		return "SccFirmwareVersionUpdated";
	if (key == MSX_DEVICE_KEY_LOAD_STATUS_ON)
		return "LoadStatusOn";
	if (key == MSX_DEVICE_KEY_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING)
		return "BatteryVoltageToSteadyWhileCharging";
	if (key == MSX_DEVICE_KEY_CHARGING_ON)
		return "ChargingOn";
	if (key == MSX_DEVICE_KEY_CHARGING_ON_SOLAR)
		return "ChargingOnSolar";
	if (key == MSX_DEVICE_KEY_CHARGING_ON_AC)
		return "ChargingOnAC";
	if (key == MSX_DEVICE_KEY_ENABLE_BUZZER)
		return "EnableBuzzer";
	if (key == MSX_DEVICE_KEY_OVERLOAD_BYPASS_FUNCTION)
		return "OverloadBypassFunction";
	if (key == MSX_DEVICE_KEY_POWER_SAVE)
		return "PowerSave";
	if (key == MSX_DEVICE_KEY_LCD_DISPLAY_ESCAPE)
		return "LcdDisplayEscape";
	if (key == MSX_DEVICE_KEY_OVERLOAD_RESTART)
		return "OverloadRestart";
	if (key == MSX_DEVICE_KEY_OVER_TEMPERATURE_RESTART)
		return "OverTemperatureRestart";
	if (key == MSX_DEVICE_KEY_LCD_BACKLIGHT)
		return "LcdBacklight";
	if (key == MSX_DEVICE_KEY_ALARM_PRIMARY_SOURCE_INTERRUPT)
		return "AlarmPrimarySourceInterrupt";
	if (key == MSX_DEVICE_KEY_FAULT_CODE_RECORD)
		return "FaultCodeRecord";
	return NULL;
}
