// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment-macros.h"
#include "platform-pinmux.hh"
#include "platform-spi.hh"
#include "platform-gpio.hh"
#include <compartment.h>
#include <debug.hh>
#include <thread.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "neopixel">;

constexpr uint8_t NUM_LEDS = 20;

struct RGB {
	RGB() : r(0), g(0), b(0) {}
	RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

// frame buffer
std::array<RGB, NUM_LEDS> leds;

static int abs(int x) {
	return (x < 0) ? -x : x;
}

static uint64_t measure_cycles (auto && function) {
	uint64_t start = rdcycle64();
	function();
	uint64_t end = rdcycle64();
	return end - start;
}

// Use 1 SPI byte per pixel bit, with 3 color bytes per pixel
// We could easily get two pixel bits per byte, but this is simpler for now
uint8_t spi_buf[NUM_LEDS * 24];
int wave_offset = 0;
int wave_direction = 1;
static void write_leds(volatile SonataSpi::Generic<1> *spi1) {
	for (int led = 0; led < NUM_LEDS; led++) {
		int brightness = 255;
		uint8_t r = static_cast<uint32_t>(leds[led].r) * brightness / 255;
		uint8_t g = static_cast<uint32_t>(leds[led].g) * brightness / 255;
		uint8_t b = static_cast<uint32_t>(leds[led].b) * brightness / 255;
		for (int bit = 0; bit < 8; bit++) {
			spi_buf[led * 24 + bit + 0] = (b & (1 << (7 - bit))) ? 0b11110000 : 0b11000000;
			spi_buf[led * 24 + bit + 8] = (g & (1 << (7 - bit))) ? 0b11110000 : 0b11000000;
			spi_buf[led * 24 + bit + 16] = (r & (1 << (7 - bit))) ? 0b11110000 : 0b11000000;
		}
	};
	spi1->blocking_write(spi_buf, sizeof(spi_buf) / sizeof(*spi_buf));
}

static void knightrider_effect() {
	// decay previous colors
	for (auto &led : leds) {
		led.r >>= 1;
		led.g >>= 1;
		led.b >>= 1;
	}
	leds[wave_offset].r = 0xFF;
	// leds[(wave_offset+7) % 20].g = 0xFF;
	// leds[(wave_offset+14) % 20].b = 0xFF;
	wave_offset = wave_offset + wave_direction;
	if (wave_offset == NUM_LEDS - 1) {
		wave_direction = -1;
	} else if (wave_offset == 0) {
		wave_direction = 1;
	}
}

/**
 * Convert a hue value (0-191) to an RGB color (full saturation, full value).
 */
static RGB hue(uint8_t h) {
	int octant = h >> 5;
	int v = (h & 0x1F) << 3;
	switch (octant) {
		case 0: return {0xff, static_cast<uint8_t>(v), 0x00}; // red to yellow
		case 1: return {static_cast<uint8_t>(0xff - v), 0xff, 0x00}; // yellow to green
		case 2: return {0x00, 0xff, static_cast<uint8_t>(v)}; // green to cyan
		case 3: return {0x00, static_cast<uint8_t>(0xff - v), 0xff}; // cyan to blue
		case 4: return {static_cast<uint8_t>(v), 0x00, 0xff}; // blue to magenta
		case 5: return {0xff, 0x00, static_cast<uint8_t>(0xff - v)}; // magenta to red
		default: return {0x00, 0x00, 0x00};
	}
}

static void rainbow_effect() {
	for (int i = 0; i < NUM_LEDS; i++) {
		leds[i] = hue(((1 + i + wave_offset) % NUM_LEDS) * 192 / NUM_LEDS);
	}
	wave_offset = (wave_offset + 1) % NUM_LEDS;
}

std::array<void (*)(), 2> effects = {rainbow_effect, knightrider_effect};

/// Thread entry point.
void __cheri_compartment("neopixel") neopixel_entry()
{
	uint32_t delay = 100;
	size_t effect = 0;

	Debug::log("Configuring pinmux");
	auto pinSinks = MMIO_CAPABILITY(SonataPinmux::PinSinks, pinmux_pins_sinks);
	pinSinks->get(SonataPinmux::PinSink::rph_g10).select(1); // spi1_copi -> rph_g10 (labelled 19 on board!)

	auto spi1 = MMIO_CAPABILITY(SonataSpi::Generic<1>, spi1);
	auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio_board);

	// 40 / 2 / (2+1) = 6.66 Mhz SPI clock.
	spi1->init(false, false, true, 2);
	while(true) {
		uint64_t leds_time = 0;
		uint64_t effect_time = 0;

		leds_time += measure_cycles([spi1](){ write_leds(spi1); });
		effect_time += measure_cycles(effects[effect]);

		while(auto joystick = gpio->read_joystick()) {
			if (joystick.is_up()) {
				delay = std::min(10000u, delay + 20);
			} else if (joystick.is_down()) {
				delay = std::max(delay - 20, 20u);
			}
			if (joystick.is_left()) {
				effect = (effect + effects.size() - 1) % effects.size();
			} else if (joystick.is_right()) {
				effect = (effect + 1) % effects.size();
			}
			Debug::log("Effect: {}, Delay: {} ms", effect, delay);
			thread_millisecond_wait(200);
		};

		thread_millisecond_wait(delay);
		// Debug::log("Average LED write time: {} cycles", leds_time / 10);
		// Debug::log("Average effect time: {} cycles", effect_time / 10);
	}
}
