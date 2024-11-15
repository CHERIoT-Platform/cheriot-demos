// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <thread.h>
#include <token.h>

// Define a sealed capability that gives this compartment
// read access to configuration data "logger" and "rgb_led"
#include "common/config_broker/config_broker.h"

#define RGB_LED_CONFIG "rgb_led"
DEFINE_READ_CONFIG_CAPABILITY(RGB_LED_CONFIG)

#define LOGGER_CONFIG "logger"
DEFINE_READ_CONFIG_CAPABILITY(LOGGER_CONFIG)

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Consumer #1">;

#include "config/include/logger.h"
#include "config/include/rgb_led.h"

#include "common/config_consumer/config_consumer.h"

// For this version we want the consumer threads to exit itr

namespace
{
	static logger::Config *logger;

	/**
	 * Handle updates to the logger configuration
	 */
	int logger_handler(void *newConfig)
	{
		// Claim the config against our heap quota to ensure
		// it remains available, as we will use it when other
		// confog values change.
		if (heap_claim(MALLOC_CAPABILITY, newConfig) == 0)
		{
			Debug::log("Failed to claim {}", newConfig);
			return -1;
		}

		auto oldConfig = logger;
		logger         = static_cast<logger::Config *>(newConfig);

		// Process the configuration change
		Debug::log("Configured with host: {} port: {} level: {}",
		           (const char *)logger->host.address,
		           (int16_t)logger->host.port,
		           logger->level);

		// Release our claim on the old config.  Note this is a safer
		// pattern that relasing it before we process the new value in
		// case the processing keeps some reference to the value.
		if (oldConfig)
		{
			free(oldConfig);
		}

		return 0;
	}

	/**
	 * Handle updates to the RGB LED configuration
	 */
	int led_handler(void *newConfig)
	{
		// Note the consumer helper will have already made a
		// fast claim on the new config value, and we only
		// need it for the duration of this call

		// Process the configuration
		auto config = static_cast<rgbLed::Config *>(newConfig);
		if (logger)
		{
			if (logger->level == logger::logLevel::Debug)
			{
				Debug::log("LED 0 red: {} green: {} blue: {}",
				           config->led0.red,
				           config->led0.green,
				           config->led0.blue);
				Debug::log("LED 1 red: {} green: {} blue: {}",
				           config->led1.red,
				           config->led1.green,
				           config->led1.blue);
			}
		}

		return 0;
	}

} // namespace

/**
 * Thread entry point.  The waits for changes to one
 * or more configuration values and then calls the
 * appropriate handler.
 */
void __cheri_compartment("consumer1") init()
{
	/// List of configuration items we are tracking
	ConfigConsumer::ConfigItem configItems[] = {
	  {READ_CONFIG_CAPABILITY(LOGGER_CONFIG), logger_handler, 0, nullptr},
	  {READ_CONFIG_CAPABILITY(RGB_LED_CONFIG), led_handler, 0, nullptr},
	};

	size_t numOfItems = sizeof(configItems) / sizeof(configItems[0]);

	ConfigConsumer::run(configItems, numOfItems, MAX_CONFIG_TIMEOUTS);
}
