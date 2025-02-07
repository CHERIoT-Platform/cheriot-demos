#include "interface.hh"
#include <NetAPI.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <stdint.h>
#include <vector>

#include "../../third_party/QRCode/qrcode.h"
#include "../../third_party/display_drivers/lcd.hh"
#include "platform-gpio.hh"

using Debug = ConditionalDebug<true, "Hugh the lightbulb (lcd)">;

namespace
{
	/**
	 * QR code metadata.
	 */
	struct QRCode qrcode;
	/// Version for QR code
	constexpr int QRCodeVersion = 3;
	/**
	 * Space for the QR code data.
	 */
	uint8_t qrcodeData[0x6a];

	auto switches()
	{
		return MMIO_CAPABILITY_WITH_PERMISSIONS(
		  SonataGPIO,
#if DEVICE_EXISTS(gpio_board)
		  gpio_board,
#else
		  gpio,
#endif
		  // Load access to the switches, no store.  This compartment can read
		  // the switches but cannot update the LEDs.
		  true,
		  false,
		  false,
		  false);
	}

} // namespace

void __cheri_compartment("display") graphs()
{
	Debug::log("Starting graphs thread");
	static constexpr uint32_t ScreenWidth  = 160;
	static constexpr uint32_t ScreenHeight = 120;
	SonataLcd                 lcd;
	const size_t              HeapSize =
	  LA_ABS(__export_mem_heap_end) - LA_ABS(__export_mem_heap);

	Debug::Assert(sizeof(qrcodeData) == qrcode_getBufferSize(3),
	              "QR code buffer size is {}, needs to be {}",
	              sizeof(qrcodeData),
	              qrcode_getBufferSize(3));

	Rect     memoryGraph{0, 118, ScreenWidth, 128};
	Rect     cpuGraph{0, 0, ScreenWidth, 10};
	uint64_t lastCycles     = 0;
	uint64_t lastIdleCycles = 0;

	{
		// Generate a QR code that contains the connection topic and the key.
		std::vector<uint8_t> qrRawdata;
		qrRawdata.reserve(TopicSuffixLength + KeyLength);
		auto topicSuffix = topic_suffix();
		auto key         = key_bytes();
		qrRawdata.insert(
		  qrRawdata.begin(), topicSuffix, topicSuffix + TopicSuffixLength);
		qrRawdata.insert(qrRawdata.end(), key, key + KeyLength);

		qrcode_initBytes(&qrcode,
		                 qrcodeData,
		                 QRCodeVersion,
		                 ECC_LOW,
		                 qrRawdata.data(),
		                 qrRawdata.size());
	}

	bool lastSwitchValue = false;
	int  faults          = 0;
	bool showQRCode      = false;

	while (true)
	{
		bool newShowQRCode = switches()->read_switch(0);
		if (newShowQRCode != showQRCode)
		{
			lcd.fill_rect(Rect::from_point_and_size({0, 0}, {160, 128}),
			              Color::White);
			showQRCode = newShowQRCode;
		}
		if (!showQRCode)
		{
			lcd.draw_str({55, 16}, "Hugh", Color::White, Color::Black);
			lcd.draw_str({10, 32}, "The Lightbulb", Color::White, Color::Black);
			lcd.draw_str({10, 64}, "ID:", Color::White, Color::Black);
			lcd.draw_str({50, 64}, topic_suffix(), Color::White, Color::Black);
			if (faults > 0)
			{
				char buffer[14];
				snprintf(buffer, sizeof(buffer), "Faults: %d", faults);
				buffer[13] = '\0';
				lcd.draw_str({10, 96}, buffer, Color::White, Color::Red);
			}
			// Draw the memory-usage graph
			size_t heapFree   = heap_available();
			memoryGraph.right = ScreenWidth;
			lcd.fill_rect(memoryGraph, Color::White);
			memoryGraph.right = ScreenWidth * (HeapSize - heapFree) / HeapSize;
			lcd.fill_rect(memoryGraph,
			              (heapFree < 10 * 1024 ? Color::Red : Color::Green));

			// Draw the CPU-usage graph
			uint64_t idleCycles         = thread_elapsed_cycles_idle();
			uint64_t cycles             = rdcycle64();
			uint32_t elapsedCycles      = cycles - lastCycles;
			uint32_t idleElsapsedCycles = idleCycles - lastIdleCycles;
			lastCycles                  = cycles;
			lastIdleCycles              = idleCycles;
			cpuGraph.right              = ScreenWidth;
			lcd.fill_rect(cpuGraph, Color::White);
			cpuGraph.right = ScreenWidth *
			                 (elapsedCycles - idleElsapsedCycles) /
			                 elapsedCycles;
			if (cpuGraph.right > 0)
			{
				lcd.fill_rect(cpuGraph,
				              (cpuGraph.right > (ScreenWidth * 0.8)
				                 ? Color::Red
				                 : Color::Green));
			}
			bool switchValue = switches()->read_switch(7);
			switchValue |= switches()->read_joystick().is_pressed();
			if (switchValue != lastSwitchValue)
			{
				Debug::log("Switch value changed from {} to {}",
				           lastSwitchValue,
				           switchValue);
				lastSwitchValue = switchValue;
				if (switchValue)
				{
					Debug::log("Injecting fault");
					network_inject_fault();
					faults++;
					Debug::log("Faults: {}", faults);
				}
			}
		}
		else
		{
			const Size BlockSize = {ScreenWidth / qrcode.size,
			                        ScreenHeight / qrcode.size};
			for (uint8_t y = 0; y < qrcode.size; y++)
			{
				for (uint8_t x = 0; x < qrcode.size; x++)
				{
					Rect block = Rect::from_point_and_size(
					  {(x + 1) * BlockSize.width, (y + 1) * BlockSize.height},
					  BlockSize);
					auto colour = qrcode_getModule(&qrcode, x, y)
					                ? Color::Black
					                : Color::White;
					// Debug::log("({}, {}, {}, {}) = {}", block.left,
					// block.top, block.right, block.bottom
					lcd.fill_rect(block, colour);
				}
			}
		}
		Timeout oneSecond{MS_TO_TICKS(500)};
		thread_sleep(&oneSecond, ThreadSleepFlags::ThreadSleepNoEarlyWake);
	}
}
