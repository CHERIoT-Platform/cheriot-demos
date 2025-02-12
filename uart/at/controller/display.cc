// Copyright 3bian Limited and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "../../../third_party/display_drivers/lcd.hh"
#include "interface.hh"
#include <cstdint>
#include <string>
#include <vector>

constexpr uint32_t LineHeight = 15;
constexpr uint32_t ScreenMargin = 4;
constexpr uint32_t ScreenWidth = 160;
constexpr uint32_t ScreenHeight = 128;

class RotatableHistory
{
  public:
    explicit RotatableHistory(uint8_t maxLines) : maxLines(maxLines), dirty(true)
    {
    }

    bool is_dirty() const
    {
        return dirty;
    }

    const std::vector<std::string> &lines() const
    {
        return innerLines;
    }

    void mark_clean()
    {
        dirty = false;
    }

    void push(const std::string &line)
    {
        if (innerLines.size() >= maxLines)
        {
            innerLines.erase(innerLines.begin()); // Remove the oldest line
        }
        innerLines.push_back(line);
        mark_dirty();
    }

  private:
    bool dirty;
    std::vector<std::string> innerLines;
    uint8_t maxLines;

    void mark_dirty()
    {
        dirty = true;
    }
};

RotatableHistory history(8);

void render_rotatable_history(SonataLcd &lcd, RotatableHistory &history);

[[noreturn]] void __cheri_compartment("display") entry_point()
{
    SonataLcd lcd;

    while (true)
    {
        Timeout loopWait{MS_TO_TICKS(500)};

        render_rotatable_history(lcd, history);
        thread_sleep(&loopWait, ThreadSleepFlags::ThreadSleepNoEarlyWake);
    }
}

void __cheri_compartment("display") log(const std::string &line)
{
    history.push(line);
}

void render_rotatable_history(SonataLcd &lcd, RotatableHistory &history)
{
    if (!history.is_dirty())
    {
        return;
    }

    lcd.fill_rect(Rect::from_point_and_size({0, 0}, {ScreenWidth, ScreenHeight}), Color::Black);

    uint32_t y = ScreenMargin;
    for (const auto &line : history.lines())
    {
        lcd.draw_str({ScreenMargin, y}, line.data(), Color::Black, Color::White);
        y += LineHeight;
    }

    history.mark_clean();
}