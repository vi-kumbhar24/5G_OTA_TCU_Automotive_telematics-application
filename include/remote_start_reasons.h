#ifndef REMOTE_START_REASONS_H
#define REMOTE_START_REASONS_H

string RemoteAbortReasons[] = {
	"none",					// 0x00
	"normal_timeout",			// 0x01
	"normal_timeout",			// 0x02
	"ignition_in_run_start",		// 0x03
	"driver_door_ajar",			// 0x04
	"passenger_door_ajar",			// 0x05
	"left_rear_door_ajar",			// 0x06
	"right_rear_door_ajar",			// 0x07
	"too_many_remote_starts",		// 0x08
	"failed_counter_reached",		// 0x09
	"low_rpm_shutdown",			// 0x0A
	"key_in_ignition",			// 0x0B
	"brake_pressed",			// 0x0C
	"hazard_lamps",				// 0x0D
	"not_in_park_neutral",			// 0x0E
	"vehicle_speed_high",			// 0x0F
	"hood_ajar",				// 0x10
	"trunk_liftgate_ajar",			// 0x11
	"vehicle_theft_alarm",			// 0x12
	"panic_mode",				// 0x13
	"battery_voltage_high",			// 0x14
	"battery_voltage_low",			// 0x15
	"power_loss",				// 0x16
	"check_engine_indicator_on",		// 0x17
	"oil_pressure_low",			// 0x18
	"coolant_temp_high",			// 0x19
	"rpm_too_high",				// 0x1A
	"crank_no_start",			// 0x1B
	"rke_off_message",			// 0x1C
	"rs_disabled_prev",			// 0x1D
	"not_configured",			// 0x1E
	"no_hood_switch",			// 0x1F
	"no_automatic_transmision",		// 0x20
	"not_enabled",				// 0x21
	"invalid_key",				// 0x22
	"ignition_signal_not_avaliable",	// 0x23
	"ignition_not_in_lock",			// 0x24
	"glow_plug_timeout",			// 0x25
	"low_fuel",				// 0x26
	"electronic_throttle_control_lamp_on",	// 0x27
	"logistic_mode",			// 0x28
	"cold_start_lamp_flashing"		// 0x29
//      "do not use: SNA"			// 0x3F
};

string AutonetRemoteAbortReasons[] = {
	"remote_engine_off"			// 0x100;
};

#define RemoteAbortSNA			0x3F
#define RemoteAbortNotProgrammed	0xFF

#endif
