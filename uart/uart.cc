// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment-macros.h"
#include "platform/sunburst/platform-pinmux.hh"
#include <compartment.h>
#include <debug.hh>
#include <futex.h>
#include <interrupt.h>
#include <tick_macros.h>
#include <timeout.h>

/*
 * Await some (hardware-specified) number of bytes in the UART's RX FIFO before
 * waking the core.  Set to Level1 to echo byte by byte.
 */
constexpr auto RXFifoLevel = OpenTitanUart::ReceiveWatermark::Level8;

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "uart">;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(uart1InterruptCap, Uart1Interrupt, true, true);

/// Thread entry point.
void __cheri_compartment("uart") uart_entry()
{
	Debug::log("Configuring pinmux");
	auto pinSinks = MMIO_CAPABILITY(SonataPinmux::PinSinks, pinmux_pins_sinks);
	pinSinks->get(SonataPinmux::PinSink::pmod0_2).select(4); // uart1 tx -> pmod0_2
	auto blockSinks = MMIO_CAPABILITY(SonataPinmux::BlockSinks, pinmux_block_sinks);
	blockSinks->get(SonataPinmux::BlockSink::uart_1_rx).select(5); // pmod0_3 -> uart1 rx

	auto uart1 = MMIO_CAPABILITY(Uart, uart1);
	uart1->init(9600);

	uart1->receive_watermark(RXFifoLevel);
    uart1->interrupt_enable(OpenTitanUart::InterruptReceiveWatermark);

	auto uart1InterruptFutex = interrupt_futex_get(
			  STATIC_SEALED_VALUE(uart1InterruptCap));

	while(true)
    {
        Timeout t {MS_TO_TICKS(60000)};
        auto irqCount = *uart1InterruptFutex;

        Debug::log("UART status {}, IRQ count {}", uart1->status, irqCount);

        while((uart1->status & OpenTitanUart::StatusReceiveEmpty) == 0)
        {
	        char c = uart1->readData;
            Debug::log("read '{}' {}", c, static_cast<unsigned int>(c));
		    uart1->blocking_write(c);
        }

        interrupt_complete(STATIC_SEALED_VALUE(uart1InterruptCap));

        auto waitRes = futex_timed_wait(&t, uart1InterruptFutex, irqCount);
        Debug::log("futex_timed_wait return {}", waitRes);
	}
}
