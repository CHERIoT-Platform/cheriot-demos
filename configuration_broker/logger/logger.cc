// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cheri.hh>
#include <debug.hh>

#include "logger.h"

// Expose debugging features unconditionally for this library.
using Debug = ConditionalDebug<true, "Logger">;

//
// Function which nominally configures the logger
// In this demo it just prints the config value
//
void __cheri_libcall logger_config(void *c)
{
	LoggerConfig *config = (LoggerConfig *)c;
	Debug::log("Configured with host: {} port: {} level: {}",
	           (const char *)config->host.address,
	           (int16_t)config->host.port,
	           config->level);
}
