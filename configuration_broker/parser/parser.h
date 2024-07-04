// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>

void __cheri_compartment("parser") parserInit();

//
// Parser callbacks for different config data types
//
int __cheri_callback parse_logger_config(const char *json, void *config);

int __cheri_callback parse_RGB_LED_config(const char *json, void *config);

int __cheri_callback parse_User_LED_config(const char *json, void *config);
