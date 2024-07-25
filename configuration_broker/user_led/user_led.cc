// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <debug.hh>

#include "user_led.h"

// Expose debugging features unconditionally for this library.
using Debug = ConditionalDebug<true, "USER LED">;

/**
 * Function which nominally configures the user LEDs
 * In this demo it just prints the config value
 */
void __cheri_libcall user_led_config(void *c)
{
	auto *config = (userLed::Config *)c;
	Debug::log("User LEDs: {} {} {} {} {} {} {} {}",
	           config->led0,
	           config->led1,
	           config->led2,
	           config->led3,
	           config->led4,
	           config->led5,
	           config->led6,
	           config->led7);
}
