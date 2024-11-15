#include <compartment.h>
#include <debug.hh>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Parser Init">;

//
// Each parser needed to be initialised to register with the
// broker, but there is no init mechanism in CHERIoT so we
// have to trigger the init from one of the threads.
//
// To keep the Broker independent from the set of parsers, and to
// limit which comparments are allowed to call into the parsers
// we wrap the initialisation into a single method which is put
// into its own compartment
//
// The thread that calls this should do so before it calls into
// any compartment that handles untrusted data
//
int __cheri_compartment("parser_rgb_led") parse_rgb_led_init();
int __cheri_compartment("parser_user_led") parse_user_led_init();
int __cheri_compartment("parser_system_config") parse_system_config_init();

// Next step in initialisation
int __cheri_compartment("system_config") system_config_run();

void __cheri_compartment("parser_init") parser_init()
{
	Debug::log("initialising parsers");

	auto res = parse_user_led_init();
	res      = std::min(res, parse_rgb_led_init());
	res      = std::min(res, parse_system_config_init());

	if (res == 0)
	{
		// All good - jump into the next initaliser
		Debug::log("Parsers initialised");
		system_config_run();
	}
	else
	{
		Debug::log("One of more parsers failed to initialise");
	}
}
