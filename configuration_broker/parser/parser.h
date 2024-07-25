// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>

//
// Each parser needed to be initialised to register with the
// broker, but there is no init mechanism in CHERIoT so we
// have to trigger the init from the Broker. To keep the
// Broker independent from the set of parsers we wrap them
// into a single inline method for it to call.
//
int __cheri_compartment("parser_rgb_led") parse_rgb_led_init();
int __cheri_compartment("parser_user_led") parse_user_led_init();
int __cheri_compartment("parser_logger") parse_logger_init();

inline int parser_init()
{
	auto res = parse_rgb_led_init();
	res      = std::min(res, parse_user_led_init());
	res      = std::min(res, parse_logger_init());

	return res;
}
