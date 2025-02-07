// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment-macros.h"
#include "platform/sunburst/platform-pinmux.hh"
#include <compartment.h>
#include <debug.hh>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "uart">;

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
	while(true) {
		char c = uart1->blocking_read();
		Debug::log("read {}", c);
		uart1->blocking_write(c);
	}
}
