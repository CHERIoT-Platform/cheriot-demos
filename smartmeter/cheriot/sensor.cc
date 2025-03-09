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
#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
#	include <fail-simulator-on-error.h>
#endif
#ifdef SMARTMETER_FAKE_UARTLESS_SENSOR
#	include <ds/xoroshiro.h>
#endif

using Debug = ConditionalDebug<true, "sensor">;

#ifdef MONOLITH_BUILD_WITHOUT_SECURITY
struct mergedData theData;
#endif

/**
 * Read from uart in recieve_buffer until \n is recieved or buffer is full
 * then the return number of characters recieved (excluding the \n).
 */
std::string read_line()
{
	std::string ret;
	auto        uart = MMIO_CAPABILITY(Uart, uart1);
	while (true)
	{
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
	auto sensorDataFine = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data_fine, sensor_data_fine, true, true, false, false);

	auto sensorDataCoarse = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data_coarse, sensor_data_coarse, true, true, false, false);
#else
	auto *sensorDataFine   = &theData.sensor_data_fine;
	auto *sensorDataCoarse = &theData.sensor_data_coarse;
#endif

#ifdef SMARTMETER_FAKE_UARTLESS_SENSOR
	ds::xoroshiro::P32R16 rand       = {};
	int                   sampleBase = 0;
#endif

	while (1)
	{
		int sample = 0;

#ifdef SMARTMETER_FAKE_UARTLESS_SENSOR
		int sampleDelta = rand.next() % 16;
		if (sampleDelta != 0)
		{
			sampleBase += sampleDelta - 8;
		}
		sample = sampleBase;
#else
		auto line = read_line();
		Debug::log("Got line {}", line);
		if (line.starts_with("powerSample"))
		{
			sample =
			  strtol(line.substr(sizeof("powerSample")).c_str(), nullptr, 0);
			Debug::log("Sample {}", sample);
		}
#endif

		timeval tv;
		int     ret = gettimeofday(&tv, nullptr);
		if (ret == 0)
		{
			struct sensor_data_fine_payload nextFinePayload = {0};
			nextFinePayload.timestamp                       = tv.tv_sec;
			nextFinePayload.samples[0]                      = sample;
			memcpy(&nextFinePayload.samples[1],
			       &sensorDataFine->payload.samples[0],
			       sizeof(nextFinePayload.samples) -
			         sizeof(nextFinePayload.samples[0]));

			sensorDataFine->write(nextFinePayload);

			if (i == SENSOR_COARSENING)
			{
				struct sensor_data_coarse_payload nextCoarsePayload = {0};
				nextCoarsePayload.timestamp                         = tv.tv_sec;

				for (int j = 0; j < SENSOR_COARSENING; j++)
				{
					nextCoarsePayload.samples[0] += nextFinePayload.samples[j];
				}

				memcpy(&nextCoarsePayload.samples[1],
				       &sensorDataCoarse->payload.samples[0],
				       sizeof(nextCoarsePayload.samples) -
				         sizeof(nextCoarsePayload.samples[0]));

				sensorDataCoarse->write(nextCoarsePayload);

				i = 0;
			}
		}

		Debug::log("Tick {}...", tv.tv_sec);

#ifdef SMARTMETER_FAKE_UARTLESS_SENSOR
		Timeout t{MS_TO_TICKS(1000)};
		thread_sleep(&t, ThreadSleepNoEarlyWake);
#endif

		i++;
	}
}
