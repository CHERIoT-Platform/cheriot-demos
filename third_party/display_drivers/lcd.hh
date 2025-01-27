// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cheri.hh>
#include <platform-gpio.hh>
#include <platform-spi.hh>
#include <thread.h>
#include <utility>

extern "C"
{
#include "core/lucida_console_12pt.h"
#include "st7735/lcd_st7735.h"
}

void lcd_init(LCD_Interface *, St7735Context *);
void lcd_destroy(LCD_Interface *, St7735Context *);

struct Point
{
	uint32_t x;
	uint32_t y;

	static const Point ORIGIN;
};

inline constexpr const Point Point::ORIGIN{0, 0};

struct Size
{
	uint32_t width;
	uint32_t height;
};

struct Rect
{
	uint32_t left;
	uint32_t top;
	uint32_t right;
	uint32_t bottom;

	static Rect from_points(Point a, Point b)
	{
		return {std::min(a.x, b.x),
		        std::min(a.y, b.y),
		        std::max(a.x, b.x),
		        std::max(a.y, b.y)};
	}

	static Rect from_point_and_size(Point point, Size size)
	{
		return {point.x, point.y, point.x + size.width, point.y + size.height};
	}

	Rect centered_subrect(Size size)
	{
		return {(right + left - size.width) / 2,
		        (bottom + top - size.height) / 2,
		        (right + left + size.width) / 2,
		        (bottom + top + size.height) / 2};
	}
};

enum class Color : uint32_t
{
	Black = 0x000000,
	White = 0xFFFFFF,
	Red   = 0x0000FF,
	Green = 0x00FF00
};

class SonataLcd
{
	private:
	LCD_Interface lcdIntf;
	St7735Context ctx;

	public:
	SonataLcd()
	{
		lcd_init(&lcdIntf, &ctx);
	}

	Size resolution()
	{
		return {ctx.parent.width, ctx.parent.height};
	}

	~SonataLcd()
	{
		lcd_destroy(&lcdIntf, &ctx);
	}
	void clean();
	void clean(Color color);
	void draw_pixel(Point point, Color color);
	void draw_line(Point a, Point b, Color color);
	void draw_image_bgr(Rect rect, const uint8_t *data);
	void draw_image_rgb565(Rect rect, const uint8_t *data);
	void fill_rect(Rect rect, Color color);
	void
	draw_str(Point point, const char *str, Color background, Color foreground);
};
