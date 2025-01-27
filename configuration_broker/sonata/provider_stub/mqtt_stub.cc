// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * Fake MQTT server that periodically "publishes"
 * messages where the data is a json string
 */

/*
 * Examples of input in JSON
 *
 * topic: logger
 * -------------
 * {
 *   host: {
 *     address: "x.x.x.x",
 *     port: 400
 *   },
 *   level: "info"
 * }
 *
 * topic: rgbled
 * -------------
 * {
 *   led0: {red: 100, green: 100, blue: 100},
 *   led1: {red: 200, green: 200, blue: 200},
 * }
 *
 * topic: userled
 * --------------
 * {
 *   led0: 'on',
 *   led1: 'off',
 *   led2: 'ON',
 *   led3: 'OFF',
 *   led4: 'Off',
 *   led5: 'On',
 *   led6: 'off',
 *   led7: 'On'
 * }
 *
 */

#include "cdefs.h"
#include <compartment.h>
#include <debug.hh>
#include <errno.h>
#include <fail-simulator-on-error.h>
#include <thread.h>

#include "provider.h"

#include "../../config/include/system_config.h"

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "MQTT">;

namespace
{

	/**
	 * Define a test message
	 */
	struct Message
	{
		const char *description;
		int         expected;
		const char *topic;
		const char *json;
	};

	// Set of messages to publish.
	const Message Messages[] = {

	  // Valid RGB LED config
	  {"Valid RGB LED config",
	   0,
	   "rgbled",
	   "{\"led0\":{\"red\":0,   \"green\":86,  \"blue\":164},"
	   " \"led1\":{\"red\":255, \"green\":200, \"blue\":200}}"},

	  // Valid User LED config
	  {"Valid User LED config",
	   0,
	   "userled",
	   "{\"led0\":\"on\",\"led1\":\"off\",\"led2\":\"ON\",\"led3\":\"OFF\","
	   " \"led4\":\"On\",\"led5\":\"Off\",\"led6\":\"on\",\"led7\":\"off\"}"},

	  // Valid RGB LED config
	  {"Valid RGB LED config",
	   0,
	   "rgbled",
	   "{\"led1\":{\"red\":0,  \"green\":86, \"blue\":164},"
	   " \"led0\":{\"red\":255,\"green\":200,\"blue\":200}}"},

	  // Valid User LED config
	  {"Valid User LED config",
	   0,
	   "userled",
	   "{\"led0\":\"OFF\",\"led1\":\"ON\",\"led2\":\"off\",\"led3\":\"on\","
	   " \"led4\":\"Off\",\"led5\":\"On\",\"led6\":\"off\",\"led7\":\"on\"}"},

	  // Invalid RGB LED config
	  {"Invalid RGB LED config",
	   -EINVAL,
	   "rgbled",
	   "{\"led0\":{\"red\":0,  \"green\":286,\"blue\":400},"
	   " \"led1\":{\"red\":255,\"green\":200,\"blue\":200}}"},

	};

} // namespace

/**
 * Thread Entry point for the MQTT stub.  The thread "publishes" each message by
 * calling the Provider's UpdateConfig() method as if the Provider has
 * subscribed to the topics.  It then waits a short time before publishing the
 * next message. After all the messages has been sent it sends two further
 * messages in quick succession to show the rate limiting in operation.
 */
void __cheri_compartment("provider") provider_init()
{
	for (auto &m : Messages)
	{
		Debug::log("-------- {} --------", m.description);
		Debug::log("thread {} Send {} {}", thread_id_get(), m.topic, m.json);
		auto res =
		  updateConfig(m.topic, strlen(m.topic), m.json, strlen(m.json));
		Debug::Assert(res == m.expected, "Unexpected result {}", res);

		// Give the consumers a chance to run
		Timeout t1{MS_TO_TICKS(3000)};
		thread_sleep(&t1, ThreadSleepNoEarlyWake);
	}

	// try to update the RGB LED too quickly
	auto m = Messages[1];
	Debug::log("------- Update RGB LED --------");
	auto res = updateConfig(m.topic, strlen(m.topic), m.json, strlen(m.json));
	Debug::Assert(res == 0, "Unexpected result {}", res);

	Debug::log("------- Update RGB LED too quickly --------");
	res = updateConfig(m.topic, strlen(m.topic), m.json, strlen(m.json));
	Debug::Assert(res == -EBUSY, "Unexpected result {}", res);

	Debug::log("\n---- Finished ----");
};
