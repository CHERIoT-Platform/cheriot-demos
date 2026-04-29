#include <__macro_map.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <stdint.h>
#include <vector>

#include "../../third_party/display_drivers/lcd.hh"
#include "platform-gpio.hh"

using Debug = ConditionalDebug<true, "MPW1-Demo">;

void __cheri_compartment("mpw1-demo") mpw1_demo_entry()
{
	Debug::log("Starting demo thread");
	static constexpr uint32_t ScreenWidth  = 180;
	static constexpr uint32_t ScreenHeight = 128;
	SonataLcd                 lcd;
	Debug::log("Initialised lcd, drawing rect");
	lcd.fill_rect(Rect::from_point_and_size({0, 0}, {ScreenWidth, ScreenHeight / 2}),
				  Color::Red);

	while(true)
	{
		Debug::log(".");
		thread_millisecond_wait(1000);
	}
}
