// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <thread.h>
#include <token.h>
#include <unwind.h>

//
// As the network stack requires a lot of memory run all
// three configuration consumers in the same compartment
//

// Define sealed capability that gives this compartment
// read access to configuration data
#include "common/config_broker/config_broker.h"

#define SYSTEM_CONFIG "system"
DEFINE_READ_CONFIG_CAPABILITY(SYSTEM_CONFIG)

#define RGB_LED_CONFIG "rgb_led"
DEFINE_READ_CONFIG_CAPABILITY(RGB_LED_CONFIG)

#define USER_LED_CONFIG "user_led"
DEFINE_READ_CONFIG_CAPABILITY(USER_LED_CONFIG)

#include "../third_party/display_drivers/lcd.hh"
#include <platform-gpio.hh>
#include <platform-rgbctrl.hh>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<false, "Consumer">;

#include "config/include/rgb_led.h"
#include "config/include/system_config.h"
#include "config/include/user_led.h"

#include "common/config_consumer/config_consumer.h"

//#include "../logos/CT_logo.h"

namespace
{
	/**
	 * Handle updates to the System configuration
	 */
	int system_config_handler(void *newConfig)
	{
		static auto init = false;

		// Process the configuration
		auto config = static_cast<systemConfig::Config *>(newConfig);

		Debug::log("System Config: {}", (const char *)config->id);

		CHERI::with_interrupts_disabled([&]() {
			static auto lcd = SonataLcd();

			auto screen =
			  Rect::from_point_and_size(Point::ORIGIN, lcd.resolution());

			if (!init)
			{
				// auto logoRect = screen.centered_subrect({105, 80});
				// lcd.draw_image_rgb565(logoRect, CTLogo105x80);
				init = true;
			}

			auto idRect = Rect::from_point_and_size({0, 0}, {screen.right, 17});
			lcd.fill_rect(idRect, Color::White);
			lcd.draw_str({10, 2}, config->id, Color::White, Color::Black);

			uint32_t x = 5;
			uint32_t y = screen.bottom - 17;
			char     switchNr[2];
			for (auto i = 0; i < 8; i++)
			{
				Debug::log("Switch {}: {} {} {}", i, config->switches[i], x, y);
				snprintf(switchNr, 2, "%d", i);
				if (config->switches[i])
				{
					lcd.draw_str({x, y}, switchNr, Color::Red, Color::White);
				}
				else
				{
					lcd.draw_str({x, y}, switchNr, Color::Black, Color::White);
				}
				x += 12;
			}
		});

		return 0;
	}

	/**
	 * Handle updates to the RGB LED configuration
	 */
	int rgb_led_handler(void *newConfig)
	{
		// Note the consumer helper will have already made a
		// fast claim on the new config value, and we only
		// need it for the duration of this call

		// Process the configuration
		auto config = static_cast<rgbLed::Config *>(newConfig);
		auto driver = MMIO_CAPABILITY(SonataRgbLedController, rgbled);

		Debug::log("LED 0 red: {} green: {} blue: {}",
		           config->led0.red,
		           config->led0.green,
		           config->led0.blue);
		Debug::log("LED 1 red: {} green: {} blue: {}",
		           config->led1.red,
		           config->led1.green,
		           config->led1.blue);

		driver->rgb(SonataRgbLed::Led0,
		            config->led0.red,
		            config->led0.green,
		            config->led0.blue);
		driver->rgb(SonataRgbLed::Led1,
		            config->led1.red,
		            config->led1.green,
		            config->led1.blue);
		driver->update();

		return 0;
	}

	void setLED(int id, userLed::State state)
	{
		auto driver = MMIO_CAPABILITY(SonataGPIO, gpio);
		if (state == userLed::State::On)
		{
			Debug::log("Set LED {} on", id);
			driver->led_on(id);
		}
		else
		{
			Debug::log("Set LED {} off", id);
			driver->led_off(id);
		}
	}

	/**
	 * Handle updates to the User LED configuration
	 */
	int user_led_handler(void *newConfig)
	{
		// Note the consumer helper will have already made a
		// fast claim on the new config value, and we only
		// need it for the duration of this call

		// Configure the controller
		auto config = static_cast<userLed::Config *>(newConfig);
		Debug::log("User LEDs: {} {} {} {} {} {} {} {}",
		           config->led0,
		           config->led1,
		           config->led2,
		           config->led3,
		           config->led4,
		           config->led5,
		           config->led6,
		           config->led7);

		setLED(0, config->led0);
		setLED(1, config->led1);
		setLED(2, config->led2);
		setLED(3, config->led3);
		setLED(4, config->led4);
		setLED(5, config->led5);
		setLED(6, config->led6);
		setLED(7, config->led7);
		return 0;
	}

} // namespace

/**
 * Thread entry point.  The waits for changes to one
 * or more configuration values and then calls the
 * appropriate handler.
 */
void __cheri_compartment("consumers") init()
{
	/// List of configuration items we are tracking
	ConfigConsumer::ConfigItem configItems[] = {
	  {READ_CONFIG_CAPABILITY(SYSTEM_CONFIG),
	   system_config_handler,
	   0,
	   nullptr},
	  {READ_CONFIG_CAPABILITY(RGB_LED_CONFIG), rgb_led_handler, 0, nullptr},
	  {READ_CONFIG_CAPABILITY(USER_LED_CONFIG), user_led_handler, 0, nullptr},
	};

	size_t numOfItems = sizeof(configItems) / sizeof(configItems[0]);

	while (true)
	{
		on_error([&]() { ConfigConsumer::run(configItems, numOfItems); },
		         [&]() { Debug::log("Unexpected error in Consumer"); });
	}
}
