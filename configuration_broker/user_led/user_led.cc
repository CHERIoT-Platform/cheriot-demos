// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cheri.hh>
#include <debug.hh>
#include <thread.h>

#include "user_led.h"

// Expose debugging features unconditionally for this library.
using Debug = ConditionalDebug<true, "USER LED">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;

//
// Function which nominally configures the logger
// In this demo it just prints the config value
//
void __cheri_libcall user_led_config(void *c)
{
	auto *config = (User_LED_Config *)c;
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
