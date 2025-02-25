// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/*
 * Housekeeping compartment.
 *
 * Performs device initialization and periodic SNTP resync.
 */

#include "common.hh"

#include <NetAPI.h>
#include <cstdlib>
#include <cstring>
#include <debug.hh>
#include <errno.h>
#include <string_view>
#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
#	include <fail-simulator-on-error.h>
#endif
#include <futex.h>
#include <mqtt.h>
#include <sntp.h>
#include <thread.h>
#include <tick_macros.h>

using Debug = ConditionalDebug<true, "housekeeping">;

std::atomic<uint32_t> initialized;

char                  mqttUnique[8];

constexpr bool random_id = std::string_view(MQTT_UNIQUE_ID) == "random";
// MQTT_UNIQUE_ID must either be "random" or axactly 8 A-Za-z0-9 characters
// (no null byte)
constexpr bool is_valid_id(std::string_view id)
{
	if (id == "random")
		return true;

	if (id.size() != sizeof(mqttUnique))
	{
		return false;
	}

	for (char c : id)
	{
		switch (c)
		{
			default:
				return false;
			case '0' ... '9':
			case 'a' ... 'z':
			case 'A' ... 'Z':
				continue;
		}
	}
	return true;
}
static_assert(is_valid_id(MQTT_UNIQUE_ID));

static void do_sntp()
{
	Timeout t{MS_TO_TICKS(5000)};

	// SNTP must be run for the TLS stack to be able to check certificate dates.
	while (sntp_update(&t) != 0)
	{
		Debug::log("Failed to update NTP time");

		t = Timeout{MS_TO_TICKS(5000)};
		thread_sleep(&t, ThreadSleepNoEarlyWake);

		t = Timeout{MS_TO_TICKS(5000)};
	}
	Debug::log("Updating NTP took {} ticks", t.elapsed);

	{
		timeval tv;
		int     ret = gettimeofday(&tv, nullptr);
		if (ret != 0)
		{
			Debug::log("Failed to get time of day: {}", ret);
		}
		else
		{
			// Truncate the epoch time to 32 bits for printing.
			Debug::log("Current UNIX epoch time: {}", (int32_t)tv.tv_sec);
		}
	}
}

void housekeeping_initial_barrier()
{
	while (initialized.load() == 0)
	{
		initialized.wait(0);
	}
}

const char *housekeeping_mqtt_unique_get()
{
	CHERI::Capability ret{mqttUnique};
	ret.bounds() = sizeof(mqttUnique);
	ret.permissions() &= CHERI::Permission::Load;
	return ret.get();
}

int housekeeping_entry()
{
	Debug::log("entry");

	if constexpr(random_id) {
		mqtt_generate_client_id(mqttUnique, sizeof(mqttUnique));
	} else {
		// copy the configured MQTT_UNIQUE_ID excluding the null byte
		memcpy(mqttUnique, MQTT_UNIQUE_ID, sizeof(mqttUnique));
	}

	{
		std::string_view mqttUniqueView{mqttUnique, sizeof(mqttUnique)};
		Debug::log("MQTT Unique: {}", mqttUniqueView);
	}

	network_start();

	Debug::log("network started");

	do_sntp();

	initialized.store(1);
	initialized.notify_all();

	Debug::log("initialization barrier released");

	while (1)
	{
		int tick = 0;

		Timeout t{MS_TO_TICKS(60000)};
		thread_sleep(&t, ThreadSleepNoEarlyWake);
		tick++;

		/* Every so often (16 minutes, presently), resync our clock */
		if ((tick & 0xF) == 0)
		{
			do_sntp();
		}

		/*
		 * For more useful reporting, flush the quarantine.
		 *
		 * XXX Should we make heap_available report the sum?  Separately expose
		 * the quarantine size?
		 */
		heap_quarantine_empty();
		Debug::log("Heap available: {}", heap_available());
	}
}
