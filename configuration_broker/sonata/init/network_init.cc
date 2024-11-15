// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <NetAPI.h>
#include <cstdlib>
#include <debug.hh>
#include <errno.h>
#include <sntp.h>
#include <tick_macros.h>

using Debug = ConditionalDebug<true, "Network Init">;

// Next step in initalisation
void __cheri_compartment("provider") provider_run();

/**
 * Initialise the network stack
 */
void __cheri_compartment("network_init") network_init()
{
	Debug::log("Starting Network");

	network_start();

	// SNTP must be run for the TLS stack to be able to check certificate dates.
	Debug::log("Connecting to SNTP");
	Timeout t{MS_TO_TICKS(5000)};
	while (sntp_update(&t) != 0)
	{
		Debug::log("Failed to update NTP time");
		t = Timeout{MS_TO_TICKS(5000)};
	}

	{
		timeval tv;
		int     ret = gettimeofday(&tv, nullptr);
		if (ret == 0)
		{
			// Truncate the epoch time to 32 bits for printing.
			Debug::log("Current UNIX epoch time: {}", int32_t(tv.tv_sec));
			provider_run();
		}
		else
		{
			Debug::log("Failed to get time of day: {}", ret);
		}
	}
}
