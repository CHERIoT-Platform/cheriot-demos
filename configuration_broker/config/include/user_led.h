// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <stdlib.h>

/**
 * Mocked example of configuration data for a controller
 * with a set of eight LEDs that can be turned on and off
 * (such as the user LEDs on a Sonata Board)
 */
namespace userLed
{

	enum class State
	{
		Off = 0,
		On  = 1,
	};

	struct Config
	{
		State led0;
		State led1;
		State led2;
		State led3;
		State led4;
		State led5;
		State led6;
		State led7;
	};

} // namespace userLed

/**
 * Function which nominally configures the user LEDs
 * In this demo it just prints the config value
 */
void __cheri_libcall user_led_config(void *config);
