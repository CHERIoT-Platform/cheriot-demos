// SPDX-FileCopyrightText: CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment-macros.h>
#include <compartment.h>
#include <debug.h>
#include <futex.h>
#include <interrupt.h>
#include <platform-gpio.hh>
#include <tick_macros.h>
#include <timeout.h>

#define RPI_PIN 27

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "External Interrupt Compartment">;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(GPIOInterruptCap,
                                        GpioInterrupt,
                                        true,
                                        true);

/// Thread entry point.
int __cheri_compartment("entry") entry()
{
	auto futex = interrupt_futex_get(STATIC_SEALED_VALUE(GPIOInterruptCap));
	auto gpio  = MMIO_CAPABILITY(SonataGpioRaspberryPiHat, gpio_rpi);

	// Configure GPIO pin as input
	gpio->set_output_enable(RPI_PIN, false);

	gpio->change_interrupt_mode(
	  SonataGpioRaspberryPiHat::PcIntMode::FallingEdge);
	gpio->debounce_interrupt(false);
	// Add RPI_PIN to the set of pins that trigger a pin-change interrupt
	gpio->set_interrupt(RPI_PIN, true);
	gpio->enable_interrupts(true);

	Timeout t{UnlimitedTimeout};
	// This can be used for debugging
	// Timeout t{MS_TO_TICKS(2000)};

	auto irqCount = *futex;
	Debug::log("current irq count {}", irqCount);
	while (true)
	{
		// Wait on the futex until either time t has elapsed, or an interrupt
		// occured.
		auto waitRes = futex_timed_wait(&t, futex, irqCount);
		if (waitRes == 0)
		{
			auto newIrqCount = *futex;
			if (newIrqCount > irqCount)
			{
				Debug::log("irqCount {} new count {}", irqCount, newIrqCount);
				gpio->clear_interrupt_status();
				interrupt_complete(STATIC_SEALED_VALUE(GPIOInterruptCap));
				irqCount = newIrqCount;
			}
		}
		else
		{
			Debug::log("futex_timed_wait error {}", waitRes);
		}
	}
	return 0;
}
