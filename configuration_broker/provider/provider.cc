// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "cdefs.h"
#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <tick_macros.h>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Provider">;

//
// This compartment can set config values "logger" and "rgb_led"
//
#include "../config_broker/config_broker.h"
#define RGB_LED_CONFIG "rgb_led"
DEFINE_WRITE_CONFIG_CAPABILITY(RGB_LED_CONFIG)

#define USER_LED_CONFIG "user_led"
DEFINE_WRITE_CONFIG_CAPABILITY(USER_LED_CONFIG)

#define LOGGER_CONFIG "logger"
DEFINE_WRITE_CONFIG_CAPABILITY(LOGGER_CONFIG)

// Map of Config values to topics
struct Config
{
	const char *topic;
	SObj        cap; // Sealed Write Capability
};

// Can't use the macros at the file level to statically
// initialise topicMap, so do via a function
Config topicMap[3];
void   set_up_topic_map()
{
	static bool init = false;
	if (!init)
	{
		topicMap[0].topic = "logger";
		topicMap[0].cap   = WRITE_CONFIG_CAPABILITY(LOGGER_CONFIG);

		topicMap[1].topic = "rgbled";
		topicMap[1].cap   = WRITE_CONFIG_CAPABILITY(RGB_LED_CONFIG);

		topicMap[2].topic = "userled";
		topicMap[2].cap   = WRITE_CONFIG_CAPABILITY(USER_LED_CONFIG);

		init = true;
	}
}

//
// Compartment Entry point for the publisher
//
int __cheri_compartment("provider")
  updateConfig(const char *topic, const char *message)
{
	Debug::log("thread {} got {} on {}", thread_id_get(), message, topic);

	set_up_topic_map();

	bool found = false;
	int res = -1;

	for (auto t : topicMap)
	{
		if (strcmp(t.topic, topic) == 0)
		{
			found = true;
			res = set_config(t.cap, message);
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
			  "thread {} Unexpected Message topic {}", thread_id_get(), topic);
	}
	
	return res;

};
