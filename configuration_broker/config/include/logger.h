// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <stdlib.h>

/**
 * Contrived example of configuration data for a remote
 * logging service.
 */

namespace logger
{

	enum class logLevel
	{
		Debug = 0,
		Info  = 1,
		Warn  = 2,
		Error = 3
	};

	struct Host
	{
		char     address[16]; // ipv4 address of host
		uint16_t port;        // port on host
	};

	struct Config
	{
		Host     host;  // Details of the host
		logLevel level; // required logging level
	};

}; // namespace logger

/**
 * Function which nominally configures the logger
 * In this demo it just prints the config value.
 */
void __cheri_libcall logger_config(void *config);
