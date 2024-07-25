// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <multiwaiter.h>
#include <thread.h>
#include <token.h>

// Define a sealed capability that gives this compartment
// read access to configuration data "logger" and "user_led"
#include "../config_broker/config_broker.h"

#define USER_LED_CONFIG "user_led"
DEFINE_READ_CONFIG_CAPABILITY(USER_LED_CONFIG)

#define LOGGER_CONFIG "logger"
DEFINE_READ_CONFIG_CAPABILITY(LOGGER_CONFIG)

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Consumer #2">;

#include "../logger/logger.h"
#include "../user_led/user_led.h"

namespace
{
	/**
	 * Keep track of the items, which version we have, and the
	 * handler for updates
	 */
	struct Config
	{
		SObj                   capability; // Sealed Read Capability
		uint32_t               version;
		std::atomic<uint32_t> *versionFutex;
		int (*handler)(void *); // Handler to call
	};

	/**
	 * Handle updates to the logger configuration
	 */
	int logger_handler(void *newConfig)
	{
		static LoggerConfig *config;

		// Claim the config against our heap quota to ensure
		// it remains available, as the broker will free it
		// when it gets a new value.
		if (heap_claim(MALLOC_CAPABILITY, newConfig) == 0)
		{
			Debug::log("Failed to claim {}", newConfig);
			return -1;
		}

		auto oldConfig = config;
		config         = static_cast<LoggerConfig *>(newConfig);
		logger_config(config);
		if (oldConfig)
		{
			free(oldConfig);
		}
		return 0;
	}

	/**
	 * Handle updates to the User LED configuration
	 */
	int user_led_handler(void *newConfig)
	{
		// Claim the config against our heap quota to ensure
		// it remains available, as the broker will free it
		// when it gets a new value.
		//
		// The call to configure the led might be into another
		// compartment so we can't rely on the fast claim
		if (heap_claim(MALLOC_CAPABILITY, newConfig) == 0)
		{
			Debug::log("Failed to claim {}", newConfig);
			return -1;
		}

		// Configure the controller
		auto config = static_cast<userLed::Config *>(newConfig);
		user_led_config(config);

		// We can assume the controller has used these values
		// now so just release our claim on the config
		free(newConfig);

		return 0;
	}

} // namespace

/**
 * Thread entry point.  The waits for changes to one
 * or more configuration values and then calls the
 * appropriate handler.
 */
void __cheri_compartment("consumer2") init()
{
	// List of configuration items we are tracking
	Config configItems[] = {
	  {READ_CONFIG_CAPABILITY(LOGGER_CONFIG), 0, nullptr, logger_handler},
	  {READ_CONFIG_CAPABILITY(USER_LED_CONFIG), 0, nullptr, user_led_handler},
	};

	auto numOfItems = sizeof(configItems) / sizeof(configItems[0]);

	// Just for the demo keep track of the number to timeouts to give
	// a clean exit
	uint16_t num_timeouts = 0;

	// Create the multi waiter
	struct MultiWaiter *mw = nullptr;
	Timeout             t1{MS_TO_TICKS(1000)};
	multiwaiter_create(&t1, MALLOC_CAPABILITY, &mw, 2);
	if (mw == nullptr)
	{
		Debug::log("thread {} failed to create multiwaiter", thread_id_get());
		return;
	}

	// Create a set of wait events
	struct EventWaiterSource events[numOfItems];

	// We need to do an initial read of each item to at
	// least get the version futex to wait on (there may not
	// be a value yet. So set up the initial value of the
	// events as if each item has been updated.
	for (auto i = 0; i < numOfItems; i++)
	{
		events[i] = {nullptr, EventWaiterFutex, 1};
	}

	// Loop waiting for config changes.  The flow in here
	// is read and then wait for change to account for the
	// need for an initial read.
	while (true)
	{
		// find out which value changed
		for (auto i = 0; i < numOfItems; i++)
		{
			if (events[i].value == 1)
			{
				auto c    = &configItems[i];
				auto item = get_config(c->capability);

				if (item.versionFutex == nullptr)
				{
					Debug::log("thread {} failed to get {}",
					           thread_id_get(),
					           c->capability);
					continue;
				}

				c->version      = item.version;
				c->versionFutex = item.versionFutex;

				Debug::log("thread {} got version:{} of {}",
				           thread_id_get(),
				           c->version,
				           item.name);

				if (item.data == nullptr)
				{
					Debug::log("No data yet for {}", item.name);
					continue;
				}

				// Make a fast claim on the data now, the handler
				// can decide if it wants to make a full claim
				Timeout t{5000};
				int     claimed = heap_claim_fast(&t, item.data, nullptr);
				if (claimed != 0)
				{
					Debug::log("thread {} failed fast claim for {} {} with {}",
					           thread_id_get(),
					           item.name,
					           item.data,
					           claimed);
					continue;
				}

				// Call the handler for this item
				if (c->handler(item.data) != 0)
				{
					Debug::log("thread {} handler failed for {} {}",
					           thread_id_get(),
					           item.name,
					           item.data);
				}
			}
		}

		// Reset the event waiter
		for (auto i = 0; i < numOfItems; i++)
		{
			events[i] = {configItems[i].versionFutex,
			             EventWaiterFutex,
			             configItems[i].version};
		}

		// Wait for a version to change
		Timeout t{MS_TO_TICKS(10000)};
		if (multiwaiter_wait(&t, mw, events, 2) != 0)
		{
			num_timeouts++;
			Debug::log(
			  "thread {} wait timeout {}", thread_id_get(), num_timeouts);
			// For the demo exit the thread when we stop getting updates
			if (num_timeouts >= 2)
				break;
		}
		else
		{
			num_timeouts = 0;
		}
	}
}
