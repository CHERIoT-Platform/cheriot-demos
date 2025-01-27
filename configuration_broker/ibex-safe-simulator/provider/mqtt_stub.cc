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

#include "config.h"

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

	/**
	 * Define a data struct for the Logger conf, send
	 * as a byte format
	 */
	struct Logger
	{
		char     address[16];
		uint16_t port;
		int      level;
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

	  // Invalid RGB LED config - invalid Json
	  {"InvalidRBG LED config (bad JSON)", -EINVAL, "rgbled", "{\"x\":"},

	  // Valid User LED config
	  {"Valid User LED config",
	   0,
	   "userled",
	   "{\"led0\":\"OFF\",\"led1\":\"ON\",\"led2\":\"off\",\"led3\":\"on\","
	   " \"led4\":\"Off\",\"led5\":\"On\",\"led6\":\"off\",\"led7\":\"on\"}"},

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
void __cheri_compartment("provider") provider_run()
{
	Logger loggerConfig = {"100.101.102.103", 666, 2};

	Debug::log("-------- Logger (Warn) --------");
	auto res = updateConfig("logger", 6, &loggerConfig, sizeof(loggerConfig));
	Debug::Assert(res == 0, "Unexpected result {}", res);

	// Give the consumers a chance to run
	Timeout t1{MS_TO_TICKS(500)};
	thread_sleep(&t1, ThreadSleepNoEarlyWake);

	Debug::log("-------- Logger (Debug) --------");
	loggerConfig.level = 0;
	res = updateConfig("logger", 6, &loggerConfig, sizeof(loggerConfig));
	Debug::Assert(res == 0, "Unexpected result {}", res);

	// Give the consumers a chance to run
	Timeout t2{MS_TO_TICKS(500)};
	thread_sleep(&t2, ThreadSleepNoEarlyWake);

	Debug::log("-------- Logger (Invalid address and port) --------");
	strcpy(loggerConfig.address, "invalidAddress");
	loggerConfig.port = 0;
	res = updateConfig("logger", 6, &loggerConfig, sizeof(loggerConfig));
	Debug::Assert(res == -EINVAL, "Unexpected result {}", res);

	Timeout t3{MS_TO_TICKS(500)};
	thread_sleep(&t3, ThreadSleepNoEarlyWake);
	Debug::log("-------- Logger (Bad data) --------");
	char buffer[2];
	res = updateConfig("logger", 6, &buffer, sizeof(loggerConfig));
	Debug::Assert(res == -EINVAL, "Unexpected result {}", res);

	for (auto &m : Messages)
	{
		Debug::log("-------- {} --------", m.description);
		Debug::log("thread {} Send {} {}", thread_id_get(), m.topic, m.json);
		// Match the capabilities set by the MQTT shim
		CHERI::Capability topic{m.topic};
		CHERI::Capability json{m.json};
		topic.permissions() &= CHERI::Permission::Load;
		json.permissions() &= CHERI::Permission::Load;
		res = updateConfig(topic, strlen(m.topic), json, strlen(m.json));
		Debug::Assert(res == m.expected, "Unexpected result {}", res);

		// Give the consumers a chance to run
		Timeout t4{MS_TO_TICKS(500)};
		thread_sleep(&t4, ThreadSleepNoEarlyWake);
	}

	// try to update the User LED too quickly
	auto m = Messages[1];
	Debug::log("------- Update USer LED --------");
	res = updateConfig(m.topic, strlen(m.topic), m.json, strlen(m.json));
	Debug::Assert(res == 0, "Unexpected result {}", res);

	Debug::log("------- Update User LED too quickly --------");
	res = updateConfig(m.topic, strlen(m.topic), m.json, strlen(m.json));
	Debug::Assert(res == -EBUSY, "Unexpected result {}", res);

	Debug::log("\n---- Finished ----");
};
