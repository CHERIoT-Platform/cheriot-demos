// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#   include <__macro_map.h>
#	include "lcd.hh"
#	include <platform-pwm.hh>
#	include <platform-spi.hh>
#	include <utility>

template<typename T>
using Cap = CHERI::Capability<T>;

static OpenTitanSPIDriver spi;

/**
 * Helper. Returns a pointer to the LCD's backlight PWM device.
 * Unfortunately the board wired the this to the wrong pin so we can only choose
 * on or off.
 */
// [[nodiscard, gnu::always_inline]] static Cap<
//   volatile SonataPulseWidthModulation::LcdBacklight>
// pwm_bl()
// {
// 	return MMIO_CAPABILITY(SonataPulseWidthModulation::LcdBacklight, pwm_lcd);
// }

static constexpr uint8_t LcdCsPin  = 0x13; // PB03 SPI chip select
static constexpr uint8_t LcdClock  = 0x14; // PB04 SPI clock
static constexpr uint8_t LcdMosi   = 0x15; // PB05 SPI host output
static constexpr uint8_t LcdMiso   = 0x16; // PB06 SPI host input
static constexpr uint8_t LcdSDLCD  = 0x17; // PB07 GPIO for selecting SD or LCD
static constexpr uint8_t LcdLED    = 0x18; // PB08 GPIO for controlling backlight? Seems to be on anyway.
static constexpr uint8_t LcdDcPin  = 0x1b; // PB11 GPIO selects data / command on LCD
static constexpr uint8_t LcdRstPin = 0x1c; // PB12 LCD reset

constexpr IceniGpioPin::Control GpioControl = {};
constexpr IceniGpioPin::Data GpioData = {};
// value to set in gpio data register to set output high.
// Other fields (Input, Interrupt control) are zero.
constexpr auto GpioDataOutHigh = GpioData.with<IceniGpioPin::Data::Output>(1);
// As above but set output low.
constexpr auto GpioDataOutLow = GpioData.with<IceniGpioPin::Data::Output>(0);

using Debug = ConditionalDebug<true, "LCD">;

void lcd_init(LCD_Interface *lcdIntf, St7735Context *ctx)
{
	auto gpio = MMIO_CAPABILITY(IceniGpioPin, gpio);
	// Configure the SPI pins.
	gpio[LcdCsPin].control = BITPACK_QWITHS(IceniGpioPin::Control{}, 
		PinDrive::PushPull,
		Function{1} // SPI1
	);
	gpio[LcdClock].control = BITPACK_QWITHS(IceniGpioPin::Control{}, 
		PinDrive::PushPull,
		Function{1} // SPI1
	);
	gpio[LcdMosi].control = BITPACK_QWITHS(IceniGpioPin::Control{}, 
		PinDrive::PushPull,
		Function{1}
	);
	gpio[LcdMiso].control = BITPACK_QWITHS(IceniGpioPin::Control{}, 
		PinDrive::No, // should pull up / down?
		Function{1}
	);
	// this wire selects the muxed chip selects between the Lcd and SD card
	auto gpio_sd_lcd = &gpio[LcdSDLCD];
	gpio_sd_lcd->control = BITPACK_QWITHS(IceniGpioPin::Control{}, 
		PinDrive::PushPull,
		Function{0}
	);
	// Drive low to select LCD
	gpio_sd_lcd->data = BITPACK_QWITHS(IceniGpioPin::Data{}, Output{0});

	// pin for selecting LCD data / command
	static auto gpio_lcd_dc = &gpio[LcdDcPin];
	gpio_sd_lcd->control = BITPACK_QWITHS(IceniGpioPin::Control{}, 
		PinDrive::PushPull,
		Function{1} // GPIO is function 1 on this pin!
	);
	gpio_sd_lcd->data = GpioDataOutHigh;

	// LCD reset pin
	auto gpio_lcd_reset = &gpio[LcdRstPin];
	gpio_sd_lcd->control = BITPACK_QWITHS(IceniGpioPin::Control{}, 
		PinDrive::PushPull,
		Function{1} // GPIO is function 1 on this pin!
	);

	auto spi1 = MMIO_CAPABILITY(OpenTitanSPI_Bitpack, spi1);
	int error = spi.init(spi1, 3, 2,2,2); // run at 4MHz
	if (error < 0)
	{
		// Debug::log("SPI init failed: {}", error);
		return;
	}

	// Set the initial state of the LCD control pins.
	gpio_lcd_dc->data = GpioDataOutLow;
	
	// pwm_bl()->output_set(/*period=*/1, /*duty_cycle=*/255);
	// spi()->chip_select_assert(true);

	Debug::log("LCD Reset");
	// Reset LCD (active low)
	gpio_lcd_reset->data = GpioDataOutLow;
	thread_millisecond_wait(150);
	gpio_lcd_reset->data = GpioDataOutHigh;

	// Initialise LCD driver.
	lcdIntf->handle = nullptr;
	lcdIntf->spi_write =
	  [](void *handle, uint8_t *data, size_t len) -> uint32_t {
		Debug::log("LCD write {}", len);
		for (size_t i=0;i<len;i++)
		{
			Debug::log("{}", data[i]);
		}
		spi.blocking_write(data, len);
		return len;
	};
	lcdIntf->gpio_write =
	  [](void *handle, bool csHigh, bool dcHigh) -> uint32_t {
		// spi()->chip_select_assert(!csHigh);
		Debug::log("LCD DC {}", dcHigh ? "high (data)" : "low (command)");
		gpio_lcd_dc->data = dcHigh ? GpioDataOutHigh : GpioDataOutLow;
		return 0;
	};
	lcdIntf->timer_delay = [](uint32_t ms) { 
		Debug::log("delay {}ms", ms);
		thread_millisecond_wait(ms); 
	};
	lcd_st7735_init(ctx, lcdIntf);

	// Set the LCD orentiation.
	lcd_st7735_set_orientation(ctx, LCD_Rotate180);

	lcd_st7735_clean(ctx);
}

void lcd_destroy(LCD_Interface *lcdIntf, St7735Context *ctx)
{
	lcd_st7735_clean(ctx);
	// Hold LCD in reset.
	// spi()->reset_assert(true);
	// Turn off backlight.
	// pwm_bl()->output_set(/*period=*/0, /*duty_cycle=*/0);
}

void SonataLcd::set_brightness(uint8_t brightness)
{
	// pwm_bl()->output_set(255, brightness);
}

void SonataLcd::clean()
{
	// Clean the display with a white rectangle.
	lcd_st7735_clean(&ctx);
}

void SonataLcd::clean(Color color)
{
	// Clean the display with a rectangle of the given colour
	size_t w, h;
	lcd_st7735_get_resolution(&ctx, &h, &w);
	lcd_st7735_fill_rectangle(
	  &ctx,
	  {.origin = {.x = 0, .y = 0}, .width = w, .height = h},
	  static_cast<uint32_t>(color));
}

void SonataLcd::draw_image_rgb565(Rect rect, const uint8_t *data)
{
	::lcd_st7735_draw_rgb565(
	  &ctx,
	  {{rect.left, rect.top}, rect.right - rect.left, rect.bottom - rect.top},
	  data);
}

void SonataLcd::draw_str(Point       point,
                         const char *str,
                         Color       background,
                         Color       foreground)
{
	lcd_st7735_set_font(&ctx, &lucidaConsole_12ptFont);
	lcd_st7735_set_font_colors(&ctx,
	                           static_cast<uint32_t>(background),
	                           static_cast<uint32_t>(foreground));
	lcd_st7735_puts(&ctx, {point.x, point.y}, str);
}

void SonataLcd::draw_pixel(Point point, Color color)
{
	lcd_st7735_draw_pixel(
	  &ctx, {point.x, point.y}, static_cast<uint32_t>(color));
}

void SonataLcd::draw_line(Point a, Point b, Color color)
{
	if (a.y == b.y)
	{
		uint32_t x1 = std::min(a.x, b.x);
		uint32_t x2 = std::max(a.x, b.x);
		lcd_st7735_draw_horizontal_line(
		  &ctx, {{x1, a.y}, x2 - x1}, static_cast<uint32_t>(color));
	}
	else if (a.x == b.x)
	{
		uint32_t y1 = std::min(a.y, b.y);
		uint32_t y2 = std::max(a.y, b.y);
		lcd_st7735_draw_vertical_line(
		  &ctx, {{a.x, y1}, y2 - y1}, static_cast<uint32_t>(color));
	}
	else
	{
		// We currently only support horizontal and vertical lines.
		panic();
	}
}

void SonataLcd::draw_image_bgr(Rect rect, const uint8_t *data)
{
	lcd_st7735_draw_bgr(
	  &ctx,
	  {{rect.left, rect.top}, rect.right - rect.left, rect.bottom - rect.top},
	  data);
}

void SonataLcd::fill_rect(Rect rect, Color color)
{
	lcd_st7735_fill_rectangle(
	  &ctx,
	  {{rect.left, rect.top}, rect.right - rect.left, rect.bottom - rect.top},
	  static_cast<uint32_t>(color));
}
