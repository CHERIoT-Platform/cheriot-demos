// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * Code to run inside a sandbox compartment to parse
 * various configuration items from serialised JSON into
 * the corresponding struct.
 *
 * For each configuration item type there must be
 *   * A sealed capability granting permission to register
 *     the parser, and defining some key characteristics.
 *   * A callback which will perform the parse, typically using
 *     the collection of helper functions in parser_helper.h
 */

/**
 * Normally for code designed to run in a sandbox compartment
 * we would block heap operations by including the following
 * definitions:
 *
 * #define CHERIOT_NO_AMBIENT_MALLOC
 * #define CHERIOT_NO_NEW_DELETE
 *
 * However our JSON parser (and specifically magic_emum) relies on new()
 * so instead we give it a small heap quota and call heap_free_all()
 * at the end of each parser callback to clean up if there is any leakage,
 * That still leaves a small residual risk that a poorly written parser
 * could affect subsequent calls, but the parser for each item type runs
 * in it's own compartment to avoid any cross-contamination.
 *
 */
#define MALLOC_QUOTA 200

#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <string.h>
#include <thread.h>

#include <magic_enum/magic_enum.hpp>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "RGB LED Parser">;

#include "config/parser_helper.h"

// Set for Items we are allowed to register a parser for
#include "common/config_broker/config_broker.h"

#include "config/include/rgb_led.h"
#define RGB_LED_CONFIG "rgb_led"
DEFINE_PARSER_CONFIG_CAPABILITY(RGB_LED_CONFIG, sizeof(rgbLed::Config), 1800);

/**
 * Parse a json string into an RGB LED Config struct.
 */
int __cheri_callback parse_RGB_LED_config(const void *src, void *dst)
{
	auto             *config     = static_cast<rgbLed::Config *>(dst);
	auto              json       = static_cast<const char *>(src);
	CHERI::Capability jsonCap    = {src};
	size_t            jsonLength = jsonCap.bounds();
	JSONStatus_t      result;

	auto initial_quota = heap_quota_remaining(MALLOC_CAPABILITY);

	// Check we have valid JSON
	result = jsonParser::validate(json, jsonLength);
	if (result != JSONSuccess)
	{
		Debug::log("thread {} Invalid JSON {}", thread_id_get(), json);
		return -1;
	}

	// query the individual values and populate the config struct
	bool parsed = true;
	parsed      = parsed && get_number<uint8_t>(
                         json, jsonLength, "led0.red", &config->led0.red);
	parsed = parsed && get_number<uint8_t>(
	                     json, jsonLength, "led0.green", &config->led0.green);
	parsed = parsed && get_number<uint8_t>(
	                     json, jsonLength, "led0.blue", &config->led0.blue);

	parsed = parsed && get_number<uint8_t>(
	                     json, jsonLength, "led1.red", &config->led1.red);
	parsed = parsed && get_number<uint8_t>(
	                     json, jsonLength, "led1.green", &config->led1.green);
	parsed = parsed && get_number<uint8_t>(
	                     json, jsonLength, "led1.blue", &config->led1.blue);

	// Free any heap the parser might have left allocated.
	// Calling heap_free_all() is quite expensive as it has to walk all
	// the objects in the heap, so only call it if the heap usage has
	// increased, which only requires a call to the allocator to read
	// the remaining quota from the capability.
	auto heap_used = initial_quota - heap_quota_remaining(MALLOC_CAPABILITY);
	if (heap_used > 0)
	{
		auto heap_freed = heap_free_all(MALLOC_CAPABILITY);
		Debug::log("Freed {} from heap", heap_freed);
	}

	return (parsed) ? 0 : -1;
}

/**
 * Register the parser with the Broker. This needs to be
 * run before any values can be accepted.
 *
 * There is no init mechanism in CHERIoT and threads are not
 * expected to terminate, so rather than have a separate thread
 * just to run this which then blocks we expose it as method for
 * the Broker to call when the first item is published.
 */
int __cheri_compartment("parser_rgb_led") parse_rgb_led_init()
{
	// RGB LED Config Parser
	auto res = set_parser(PARSER_CONFIG_CAPABILITY(RGB_LED_CONFIG),
	                      parse_RGB_LED_config);

	if (res < 0)
	{
		Debug::log("Failed to register parser for rgb led");
	}

	return res;
}
