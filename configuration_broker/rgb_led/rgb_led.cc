// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cheri.hh>
#include <debug.hh>
#include <thread.h>

#include "rgb_led.h"

// Expose debugging features unconditionally for this library.
using Debug = ConditionalDebug<true, "RGB LED">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;

//
// Function which nominally configures the logger
// In this demo it just prints the config value
//
void __cheri_libcall rgb_led_config(void *c)
{
	auto *config = (RGB_LED_Config *)c;
	Debug::log("LED 0 red: {} green: {} blue: {}",
	           config->led0.red,
	           config->led0.green,
	           config->led0.blue);
	Debug::log("LED 1 red: {} green: {} blue: {}",
	           config->led1.red,
	           config->led1.green,
	           config->led1.blue);
}
