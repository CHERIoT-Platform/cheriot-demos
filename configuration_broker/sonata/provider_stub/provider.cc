// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "cdefs.h"
#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <tick_macros.h>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Provider">;

/**
 * Define the sealed capabilites for each of the configuration
 * items this compartment is allowed to update
 */
#include "common/config_broker/config_broker.h"
#define SYSTEM_CONFIG "system"
DEFINE_WRITE_CONFIG_CAPABILITY(SYSTEM_CONFIG)

#define RGB_LED_CONFIG "rgb_led"
DEFINE_WRITE_CONFIG_CAPABILITY(RGB_LED_CONFIG)

#define USER_LED_CONFIG "user_led"
DEFINE_WRITE_CONFIG_CAPABILITY(USER_LED_CONFIG)

namespace
{

	/**
	 * Map of Config names to capabilites
	 */
	struct Config
	{
		const char *name;
		SObj        cap; // Sealed Write Capability
	};

	// We can't use the macros at the file level to statically
	// initialise configItemMap, so do it via a function
	Config configItemMap[3];
	void   set_up_name_map()
	{
		static bool init = false;
		if (!init)
		{
			configItemMap[0].name = "rgbled";
			configItemMap[0].cap  = WRITE_CONFIG_CAPABILITY(RGB_LED_CONFIG);

			configItemMap[1].name = "userled";
			configItemMap[1].cap  = WRITE_CONFIG_CAPABILITY(USER_LED_CONFIG);

			configItemMap[2].name = "system";
			configItemMap[2].cap  = WRITE_CONFIG_CAPABILITY(SYSTEM_CONFIG);

			init = true;
		}
	}

} // namespace

/**
 * Update a configuration item using the JSON string
 * received via a services such as MQTT.
 */
int updateConfig(const char *name,
                 size_t      nameLength,
                 const void *json,
                 size_t      jsonLength)
{
	std::string_view svName(name, nameLength);
	std::string_view svJson((char *)json, jsonLength);
	Debug::log("thread {} got {} on {}", thread_id_get(), svName, svJson);

	// Initalise the name map
	set_up_name_map();

	bool found = false;
	int  res   = -1;

	// Use the configItemMap to work out which value the
	// message is for.
	for (auto t : configItemMap)
	{
		if (strncmp(t.name, name, nameLength) == 0)
		{
			found = true;
			res   = set_config(t.cap, (const char *)json, jsonLength);
			if (res < 0)
			{
				Debug::log("thread {} Failed to set value for {}",
				           thread_id_get(),
				           t.cap);
			}
			break;
		}
	}

	if (!found)
	{
		Debug::log(
		  "thread {} Unknown config item name {}", thread_id_get(), svName);
	}

	return res;
};
