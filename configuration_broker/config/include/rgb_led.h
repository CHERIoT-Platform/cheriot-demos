// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <stdlib.h>

// Mocked example of configuration data for a controller
// with two RGB LEDs (such as on the Sonata Board)

namespace rgbLed
{

	struct Colour
	{
		uint8_t red;
		uint8_t green;
		uint8_t blue;
	};

	struct Config
	{
		Colour led0; // Settings for LED 0
		Colour led1; // Settings for LED 1
	};

} // namespace rgbLed

/**
 * Configure the LEDs
 * In this demo it just prints the config value
 */
void __cheri_libcall rgb_led_config(void *config);
