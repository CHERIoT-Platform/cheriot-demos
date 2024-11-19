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
 * Block heap operations
 */
#define CHERIOT_NO_AMBIENT_MALLOC
#define CHERIOT_NO_NEW_DELETE

#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <string.h>
#include <thread.h>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Logger Parser">;

// Set for Items we are allowed to register a parser for
#include "common/config_broker/config_broker.h"

#include "config/include/logger.h"
#define LOGGER_CONFIG "logger"
DEFINE_PARSER_CONFIG_CAPABILITY(LOGGER_CONFIG, sizeof(logger::Config), 500);

namespace
{

	bool isaddr(char c)
	{
		return ((c == '.') || (c >= '0' && c <= '9'));
	}

} // namespace
/**
 * Parse a LoggerConfig struct.
 */
int __cheri_callback parse_logger_config(const void *src, void *dst)
{
	auto *config    = static_cast<logger::Config *>(dst);
	auto *srcConfig = static_cast<const logger::Config *>(src);
	bool  parsed    = true;

	// Check the kind is in range
	switch (srcConfig->level)
	{
		case logger::logLevel::Debug:
		case logger::logLevel::Info:
		case logger::logLevel::Warn:
		case logger::logLevel::Error:
			config->level = srcConfig->level;
			break;

		default:
			Debug::log("thread {} Invalid logLevel {}",
			           thread_id_get(),
			           srcConfig->level);
			parsed = false;
			break;
	}

	// Check the host address
	for (auto i = 0; i < sizeof(config->host.address); i++)
	{
		auto c = srcConfig->host.address[i];

		if (c == 0)
		{
			config->host.address[i] = c;
			break;
		}
		if (!isaddr(c))
		{
			Debug::log("Invalid character {} in host.address", c);
			parsed = false;
			break;
		}
		else
		{
			config->host.address[i] = c;
		}
	}

	// check the port.  As its defined to be a uint_16 any
	// value i svalide, apart form 0 which is reserved
	if (srcConfig->host.port > 0)
	{
		config->host.port = srcConfig->host.port;
	}
	else
	{
		Debug::log("Invalid host.port {}", config->host.port);
		parsed = false;
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
int __cheri_compartment("parser_logger") parse_logger_init()
{
	auto res =
	  set_parser(PARSER_CONFIG_CAPABILITY(LOGGER_CONFIG), parse_logger_config);

	if (res < 0)
	{
		Debug::log("Failed to register parser for logger");
	}

	return res;
}
