// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <platform-entropy.hh>
#include <platform-gpio.hh>
#include <thread.h>
#include <token.h>
#include <unwind.h>

// Define a sealed capability that gives this compartment
// read access to configuration data "logger" and "rgb_led"
#include "common/config_broker/config_broker.h"

#define SYSTEM_CONFIG "system"
DEFINE_WRITE_CONFIG_CAPABILITY(SYSTEM_CONFIG)

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "System Config">;

#include "config/include/system_config.h"

namespace
{
	static systemConfig::Config *currentConfig;

	// Helpers to read the swicth values
	auto switches()
	{
		return MMIO_CAPABILITY(SonataGPIO, gpio);
	}

	// ID comes from the first two switches
	auto read_id()
	{
		uint8_t res = 0;
		for (auto i = 0; i < 2; i++)
		{
			auto set = switches()->read_switch(i);
			if (set)
			{
				res += 1 << i;
			}
		}
		return res;
	}

	auto read_switches()
	{
		uint8_t res = 0;
		for (auto i = 0; i < 8; i++)
		{
			auto set = switches()->read_switch(i);
			if (set)
			{
				res += 1 << i;
			}
		}
		return res;
	}

} // namespace

/**
 * Thread entry point.  The waits for changes to one
 * or more configuration values and then calls the
 * appropriate handler.
 */
void __cheri_compartment("system_config") system_config_run()
{
	static systemConfig::Config config;

	SObj setCap = WRITE_CONFIG_CAPABILITY(SYSTEM_CONFIG);

	char name[systemConfig::IdLength - 3];
	if (strcmp(SYSTEM_ID, "random") == 0)
	{
		// Generate a random 8 char name
		{
			EntropySource entropySource;
			for (int i = 0; i < 8; i++)
			{
				name[i] = ('a' + entropySource() % 26);
			}
			name[8] = '\0';
		}
	}
	else
	{
		size_t max_id = (sizeof(name) / sizeof(name[0])) - 1;
		auto   len    = std::min(max_id, strlen(SYSTEM_ID));
		strncpy(name, SYSTEM_ID, len);
		name[len] = '\0';
	}

	// Loop reading the switches and updating
	// config if they change
	int switchValue = -1;
	int id          = -1;
	while (true)
	{
		on_error(
		  [&]() {
			  int newSwitchValue = 0;
			  for (auto i = 0; i < 8; i++)
			  {
				  config.switches[i] = switches()->read_switch(i);
				  if (config.switches[i])
				  {
					  newSwitchValue += 1 << i;
				  }
			  }

			  auto newId = read_id();
			  if (id != newId)
			  {
				  snprintf(config.id, sizeof(config.id), "%s-%d", name, newId);
			  }

			  if ((switchValue != newSwitchValue) || (id != newId))
			  {
				  Debug::log("Updating system config");
				  CHERI::Capability confCap{&config};
				  confCap.permissions() &= CHERI::Permission::Load;
				  auto res = set_config(setCap, confCap, sizeof(config));
				  if (res == 0)
				  {
					  id          = newId;
					  switchValue = newSwitchValue;
				  }
				  else
				  {
					  Debug::log("thread {} Failed to set value for {}",
					             thread_id_get(),
					             setCap);
				  }
			  }

			  // Sleep a bit
			  Timeout t1{MS_TO_TICKS(10000)};
			  thread_sleep(&t1, ThreadSleepNoEarlyWake);
		  },
		  [&]() { Debug::log("Unexpected error in System Config generator"); });
	}
}
