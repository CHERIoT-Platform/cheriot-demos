// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdlib>

// Mocked example of configuration data for a remote
// logging service

enum class User_LED
{
	Off = 0,
	On  = 1,
};

struct User_LED_Config
{
	User_LED led0;
	User_LED led1;
	User_LED led2;
	User_LED led3;
	User_LED led4;
	User_LED led5;
	User_LED led6;
	User_LED led7;
};

// Function which nominally configures the user LEDs
// In this demo it just prints the config value
void __cheri_libcall user_led_config(void *config);
