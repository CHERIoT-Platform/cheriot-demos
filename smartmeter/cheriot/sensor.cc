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
struct mergedData theData;
#endif

/**
 * Read from uart in recieve_buffer until \n is recieved or buffer is full
 * then the return number of characters recieved (excluding the \n).
 */
std::string read_line() {
	std::string ret;
	auto uart = MMIO_CAPABILITY(Uart, uart1);
	while(true) {
		char c = uart->blocking_read();
		if (c == '\n')
			break;
		ret.push_back(c);
	}
	return ret;
}

int sensor_entry()
{
	int i = 0;

	housekeeping_initial_barrier();
	Debug::log("initialization barrier down");

#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto sensorData = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data, sensor_data, true, true, false, false);
#else
	auto *sensorData = &theData.sensor_data;
#endif
	while (1)
	{
		auto line = read_line();
		int sample = 0;
		Debug::log("Got line {}", line);
		if (line.starts_with("powerSample")) {
			sample = strtol(line.substr(sizeof("powerSample")).c_str(), nullptr, 0);
			Debug::log("Sample {}", sample);
		}

		timeval tv;
		int     ret = gettimeofday(&tv, nullptr);
		if (ret == 0)
		{
			// TODO: update array with meaningful numbers
			struct sensor_data_payload nextPayload = {0};
			nextPayload.timestamp                  = tv.tv_sec;
			nextPayload.samples[0] = sample;
			
			sensorData->write(nextPayload);
		}

		Debug::log("Tick {}...", tv.tv_sec);

		Timeout t{MS_TO_TICKS(500)};
		thread_sleep(&t, ThreadSleepNoEarlyWake);
		i++;
	}
}
