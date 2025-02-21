// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * The sensor compartment.
 *
 * Responsible for reading power consumption (or a simulation thereof) and
 * calling callbacks in the other compartments.
 */

#include "common.hh"

#include <debug.hh>
#include <futex.h>
#include <sntp.h>
#include <thread.h>

using Debug = ConditionalDebug<true, "sensor">;

#ifdef MONOLITH_BUILD_WITHOUT_SECURITY
sensor_data theSensorData;
#endif

int sensor_entry()
{
	int i = 0;

	housekeeping_initial_barrier();
	Debug::log("initialization barrier down");

#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto sensorData = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data, sensor_data, true, true, false, false);
#else
	auto *sensorData = &theSensorData;
#endif

	while (1)
	{
		timeval tv;
		int     ret = gettimeofday(&tv, nullptr);
		if (ret == 0)
		{
			// TODO: update array with meaningful numbers
			struct sensor_data_payload nextPayload = {0};
			nextPayload.timestamp                  = tv.tv_sec;

			sensorData->write(nextPayload);
		}

		Debug::log("Tick {}...", tv.tv_sec);

		Timeout t{MS_TO_TICKS(30000)};
		thread_sleep(&t, ThreadSleepNoEarlyWake);
		i++;
	}
}
