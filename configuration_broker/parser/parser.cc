// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

//
// Parsing emums with magic_enum needs heap access, so
// we can't use the following macros to configure the
// compartment with no heap access. Instead we call
// heap_free_all() at the end of each parser callback.
//
// #define CHERIOT_NO_AMBIENT_MALLOC
// #define CHERIOT_NO_NEW_DELETE

#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <string.h>
#include <thread.h>

#include <magic_enum/magic_enum.hpp>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Parser">;

#include "parser.h"
#include "parser_helper.h"

// Set for Items we are allowed to register a parser for
#include "../config_broker/config_broker.h"

#include "../rgb_led/rgb_led.h"
#define RGB_LED_CONFIG "rgb_led"
DEFINE_PARSER_CONFIG_CAPABILITY(RGB_LED_CONFIG, sizeof(RGB_LED_Config), 1800);

#include "../user_led/user_led.h"
#define USER_LED_CONFIG "user_led"
DEFINE_PARSER_CONFIG_CAPABILITY(USER_LED_CONFIG, sizeof(User_LED_Config), 1800);

#include "../logger/logger.h"
#define LOGGER_CONFIG "logger"
DEFINE_PARSER_CONFIG_CAPABILITY(LOGGER_CONFIG, sizeof(LoggerConfig), 500);

//
// Register all of our parsers. This needs to be run before
// any values can be accepted.
//
// There is no init mechanism in cheriot and threads are not
// expected to terminate, so rather than have a separate thread
// just to run this which then blocks we expose it as method for
// the Broker to call when the first item is published.
//
void __cheri_compartment("parser") parserInit()
{
	// RGB LED Config Parser
	if (set_parser(PARSER_CONFIG_CAPABILITY(RGB_LED_CONFIG),
	               parse_RGB_LED_config) < 0)
	{
		Debug::log("Failed to register parser for rgb led");
	}

	// USER LED Config Parser
	if (set_parser(PARSER_CONFIG_CAPABILITY(USER_LED_CONFIG),
	           parse_User_LED_config) < 0)
	{
		Debug::log("Failed to register parser for user led");
	}

	// Logger Config Parser
	if (set_parser(PARSER_CONFIG_CAPABILITY(LOGGER_CONFIG),
	               parse_logger_config) < 0)
	{
		Debug::log("Failed to register parser for logger");
	}
}

// /////////////////////////////////////////////
//
// Parsers for specific configuration items
//
// Note that we run in a sandbox compartment to
// contain the blast radius of any parsing errors.
//
// /////////////////////////////////////////////

//
// Parse a json string into a LoggerConfig object.
//
int __cheri_callback parse_logger_config(const char *json, void *dst)
{
	auto        *config = static_cast<LoggerConfig *>(dst);
	JSONStatus_t result;

	// Check we have valid JSON
	result = JSON_Validate(json, strlen(json));
	if (result != JSONSuccess)
	{
		Debug::log("thread {} Invalid JSON {}", thread_id_get(), json);
		return -1;
	}

	// query the individual values and populate the logger config object
	bool parsed = true;
	parsed = parsed && get_string(json, "host.address", config->host.address);
	parsed =
	  parsed && get_number<uint16_t>(json, "host.port", &config->host.port);
	parsed = parsed && get_enum<logLevel>(json, "level", &config->level);

	// Free any heap the parser might have left allocated
	auto heap_freed = heap_free_all(MALLOC_CAPABILITY);
	if (heap_freed > 0)
	{
		Debug::log("Freed {} from heap", heap_freed);
	}

	return (parsed) ? 0 : -1;
}

//
// Parse a json string into an RGB LED Config object.
//
int __cheri_callback parse_RGB_LED_config(const char *json, void *dst)
{
	auto        *config = static_cast<RGB_LED_Config *>(dst);
	JSONStatus_t result;

	// Check we have valid JSON
	result = JSON_Validate(json, strlen(json));
	if (result != JSONSuccess)
	{
		Debug::log("thread {} Invalid JSON {}", thread_id_get(), json);
		return -1;
	}

	// query the individual values and populate the logger config object
	bool parsed = true;
	parsed = parsed && get_number<uint8_t>(json, "led0.red", &config->led0.red);
	parsed =
	  parsed && get_number<uint8_t>(json, "led0.green", &config->led0.green);
	parsed =
	  parsed && get_number<uint8_t>(json, "led0.blue", &config->led0.blue);

	parsed = parsed && get_number<uint8_t>(json, "led1.red", &config->led1.red);
	parsed =
	  parsed && get_number<uint8_t>(json, "led1.green", &config->led1.green);
	parsed =
	  parsed && get_number<uint8_t>(json, "led1.blue", &config->led1.blue);

	// Free any heap the parser might have left allocated
	auto heap_freed = heap_free_all(MALLOC_CAPABILITY);
	if (heap_freed > 0)
	{
		Debug::log("Freed {} from heap", heap_freed);
	}

	return (parsed) ? 0 : -1;
}

//
// Parse a json string into an User LED Config object.
//
int __cheri_callback parse_User_LED_config(const char *json, void *dst)
{
	auto        *config = static_cast<User_LED_Config *>(dst);
	JSONStatus_t result;

	// Check we have valid JSON
	result = JSON_Validate(json, strlen(json));
	if (result != JSONSuccess)
	{
		Debug::log("thread {} Invalid JSON {}", thread_id_get(), json);
		return -1;
	}

	// query the individual values and populate the logger config object
	bool parsed = true;
	parsed      = parsed && get_enum<User_LED>(json, "led0", &config->led0);
	parsed      = parsed && get_enum<User_LED>(json, "led1", &config->led1);
	parsed      = parsed && get_enum<User_LED>(json, "led2", &config->led2);
	parsed      = parsed && get_enum<User_LED>(json, "led3", &config->led3);
	parsed      = parsed && get_enum<User_LED>(json, "led4", &config->led4);
	parsed      = parsed && get_enum<User_LED>(json, "led5", &config->led5);
	parsed      = parsed && get_enum<User_LED>(json, "led6", &config->led6);
	parsed      = parsed && get_enum<User_LED>(json, "led7", &config->led7);

	// Free any heap the parser might have left allocated
	auto heap_freed = heap_free_all(MALLOC_CAPABILITY);
	if (heap_freed > 0)
	{
		Debug::log("Freed {} from heap", heap_freed);
	}

	return (parsed) ? 0 : -1;
}
