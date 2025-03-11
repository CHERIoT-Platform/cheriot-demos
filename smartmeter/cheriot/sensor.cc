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
#include <interrupt.h>
#include <sntp.h>
#include <thread.h>
#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
#	include <fail-simulator-on-error.h>
#endif
#ifdef SMARTMETER_FAKE_UARTLESS_SENSOR
#	include <ds/xoroshiro.h>
#endif

#ifdef MONOLITH_BUILD_WITHOUT_SECURITY
struct merged_data theData;

struct merged_data *monolith_merged_data_get()
{
	return &theData;
}
#endif

using Debug = ConditionalDebug<true, "sensor">;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(uart1InterruptCap,
                                        Uart1Interrupt,
                                        true,
                                        true);
const uint32_t *uart1InterruptFutex;

/**
 * Read line from uart, discarding any over-long line.
 */
std::string read_line()
{
	static constexpr size_t MaximumLineLength = 64;

	std::string ret;
	ret.reserve(MaximumLineLength);
	Debug::Assert(ret.capacity() >= MaximumLineLength,
	              "read_line alloc failed: {}",
	              ret.capacity());

	/*
	 * If we've overrun the maximum length, then we're dropping bytes until we
	 * find a newline, at which point we'll pick up again.
	 */
	bool discarding = false;

	auto uart = MMIO_CAPABILITY(Uart, uart1);

	while (true)
	{
		Timeout t{MS_TO_TICKS(60000)};
		auto    irqCount = *uart1InterruptFutex;

		while ((uart->status & OpenTitanUart::StatusReceiveEmpty) == 0)
		{
			char c = uart->readData;
			if (c == '\n')
			{
				if (!discarding)
				{
					return ret;
				}
				else
				{
					Debug::log("reset");
					discarding = false;
				}
			}
			else if (ret.size() == MaximumLineLength)
			{
				Debug::log("overlong");
				discarding = true;
				ret.clear();
			}
			else if (!discarding)
			{
				ret.push_back(c);
			}
		}

		interrupt_complete(STATIC_SEALED_VALUE(uart1InterruptCap));

		auto waitRes = futex_timed_wait(&t, uart1InterruptFutex, irqCount);
		if (waitRes != 0)
		{
			Debug::log("Unexpected wait return {}", waitRes);
		}
	}
}

int sensor_entry()
{
	int i = 0;

	uart1InterruptFutex =
	  interrupt_futex_get(STATIC_SEALED_VALUE(uart1InterruptCap));

	housekeeping_initial_barrier();
	Debug::log("initialization barrier down");

#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto sensorDataFine = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data_fine, sensor_data_fine, true, true, false, false);

	auto sensorDataCoarse = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data_coarse, sensor_data_coarse, true, true, false, false);
#else
	auto sensorDataFine   = &theData.sensor_data_fine;
	auto sensorDataCoarse = &theData.sensor_data_coarse;
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
