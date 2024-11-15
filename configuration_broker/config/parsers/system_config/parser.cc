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

#define CHERIOT_NO_AMBIENT_MALLOC
#define CHERIOT_NO_NEW_DELETE

#include <compartment.h>
#include <cstdlib>
#include <ctype.h>
#include <debug.hh>
#include <string.h>
#include <thread.h>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "System Config Parser">;

// Set for Items we are allowed to register a parser for
#include "common/config_broker/config_broker.h"

#include "config/include/system_config.h"
#define SYSTEM_CONFIG "system"
DEFINE_PARSER_CONFIG_CAPABILITY(SYSTEM_CONFIG,
                                sizeof(systemConfig::Config),
                                500);

namespace
{

	bool is_valid_char(char c)
	{
		return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		        (c >= '0' && c <= '9') || (c == '-') || (c == '_'));
	}

} // namespace

/**
 * Parse a json string into a LoggerConfig struct.
 */
int __cheri_callback parse_system_config(const void *src,
                                         size_t      srcSize,
                                         void       *dst)
{
	auto *config    = static_cast<systemConfig::Config *>(dst);
	auto *srcConfig = static_cast<const systemConfig::Config *>(src);
	bool  parsed    = true;

	// Check we have a valid id name
	for (auto i = 0; i < sizeof(config->id); i++)
	{
		auto c = srcConfig->id[i];

		if (c == 0)
		{
			config->id[i] = c;
			break;
		}
		if (!is_valid_char(c))
		{
			Debug::log("Invalid character {} in group", c);
			parsed = false;
			break;
		}
		else
		{
			config->id[i] = c;
		}
	}

	// Copy the switch settings
	for (auto i = 0; i < sizeof(config->switches); i++)
	{
		config->switches[i] = srcConfig->switches[i];
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
int __cheri_compartment("parser_system_config") parse_system_config_init()
{
	auto res =
	  set_parser(PARSER_CONFIG_CAPABILITY(SYSTEM_CONFIG), parse_system_config);

	if (res < 0)
	{
		Debug::log("Failed to register parser for system config");
	}

	return res;
}
