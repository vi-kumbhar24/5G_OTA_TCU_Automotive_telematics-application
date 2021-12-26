#ifndef VTA_CAUSES_H
#define VTA_CAUSES_H

string VTA_causes[] = {
	"unknown",			// 0x00
	"front_driver_door",		// 0x01
	"front_passenger_door",		// 0x02
	"left_rear_door",		// 0x03
	"right_rear_door",		// 0x04
	"trunk_ajar",			// 0x05
	"hood",				// 0x06
	"ignition_state_run_start",	// 0x07
	"motion_sensor",		// 0x08
	"motion_sensor_fault",		// 0x09
	"tilt",				// 0x0A
	"spare"				// 0x0B
};

#define VTA_CAUSE_NOT_PROGRAMMED 0xFF

#endif
