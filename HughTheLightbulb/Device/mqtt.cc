// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <NetAPI.h>
#include <cstdlib>
#include <debug.hh>
#include <errno.h>
#include <fail-simulator-on-error.h>
#include <locks.hh>
#include <mqtt.h>
#include <platform-entropy.hh>
#include <platform-rgbctrl.hh>
#include <sntp.h>
#include <tick_macros.h>

#include "third_party/display_drivers/lcd.hh"

#include "mosquitto.org.h"

using CHERI::Capability;

using Debug            = ConditionalDebug<true, "Hugh the lightbulb">;
constexpr bool UseIPv6 = CHERIOT_RTOS_OPTION_IPv6;

/// Maximum permitted MQTT client identifier length (from the MQTT
/// specification)
constexpr size_t MQTTMaximumClientLength = 23;
/// Prefix for MQTT client identifier
constexpr std::string_view clientIDPrefix{"cheriotMQTT"};
/// Space for the random client ID.
std::array<char, MQTTMaximumClientLength> clientID;
static_assert(clientIDPrefix.size() < clientID.size());

// MQTT network buffer sizes
constexpr const size_t networkBufferSize    = 1024;
constexpr const size_t incomingPublishCount = 20;
constexpr const size_t outgoingPublishCount = 20;

// MQTT test broker: https://test.mosquitto.org/
// Note: port 8883 is encrypted and unautenticated
DECLARE_AND_DEFINE_CONNECTION_CAPABILITY(MosquittoOrgMQTT,
                                         "test.mosquitto.org",
                                         8883,
                                         ConnectionTypeTCP);

DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(mqttTestMalloc, 32 * 1024);

std::string           topic;
std::atomic<uint32_t> barrier;

void __cheri_callback publishCallback(const char *topic,
                                      size_t      topicLength,
                                      const void *payload,
                                      size_t      payloadLength)
{
	// FIXME: Work out why != doesn't work (non-mangled memcmp being inserted)
	if (memcmp(topic, ::topic.data(), std::min(topicLength, ::topic.size())) !=
	    0)
	{
		Debug::log(
		  "Received a publish message on topic '{}', but expected '{}'",
		  std::string_view{topic, topicLength},
		  ::topic);
		return;
	}
	if (payloadLength < 3)
	{
		Debug::log("Payload is too short to be a colour");
		return;
	}
	auto *colours = static_cast<const uint8_t *>(payload);
	auto  leds    = MMIO_CAPABILITY(SonataRgbLedController, rgbled);
	leds->rgb(SonataRgbLed::Led0, colours[0], colours[1], colours[2]);
	leds->update();
}

auto status_leds()
{
	return MMIO_CAPABILITY(SonataGPIO, gpio);
}

void __cheri_compartment("hugh") hugh()
{
	int     ret;
	Timeout t{MS_TO_TICKS(5000)};

	status_leds()->led_on(0);
	network_start();
	status_leds()->led_on(1);

	// SNTP must be run for the TLS stack to be able to check certificate dates.
	while (sntp_update(&t) != 0)
	{
		Debug::log("Failed to update NTP time");
		t = Timeout{MS_TO_TICKS(5000)};
	}
	status_leds()->led_on(2);

	{
		timeval tv;
		int     ret = gettimeofday(&tv, nullptr);
		if (ret != 0)
		{
			Debug::log("Failed to get time of day: {}", ret);
		}
		else
		{
			// Truncate the epoch time to 32 bits for printing.
			Debug::log("Current UNIX epoch time: {}", int32_t(tv.tv_sec));
		}
	}

	while (barrier.load() == 0)
	{
		barrier.wait(0);
	}

	while (true)
	{
		status_leds()->led_off(3);
		status_leds()->led_off(4);

		// Prefix with something recognizable, for convenience.
		memcpy(clientID.data(), clientIDPrefix.data(), clientIDPrefix.size());
		// Suffix with random character chain.
		mqtt_generate_client_id(clientID.data() + clientIDPrefix.size(),
		                        clientID.size() - clientIDPrefix.size());

		Debug::log("Connecting to MQTT broker...");

		t           = UnlimitedTimeout;
		SObj handle = mqtt_connect(&t,
		                           STATIC_SEALED_VALUE(mqttTestMalloc),
		                           STATIC_SEALED_VALUE(MosquittoOrgMQTT),
		                           publishCallback,
		                           nullptr,
		                           TAs,
		                           TAs_NUM,
		                           networkBufferSize,
		                           incomingPublishCount,
		                           outgoingPublishCount,
		                           clientID.data(),
		                           clientID.size());
		status_leds()->led_on(3);

		if (!Capability{handle}.is_valid())
		{
			Debug::log("Failed to connect.");
			continue;
		}

		Debug::log("Connected to MQTT broker!");

		Debug::log(
		  "Subscribing to topic '{}' ({} bytes)", topic.c_str(), topic.size());
		ret = mqtt_subscribe(&t,
		                     handle,
		                     1, // QoS 1 = delivered at least once
		                     topic.data(),
		                     topic.size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe, error {}.", ret);
			mqtt_disconnect(&t, STATIC_SEALED_VALUE(mqttTestMalloc), handle);
			continue;
		}

		status_leds()->led_on(4);

		while (true)
		{
			size_t  heapFree = heap_available();
			ret = mqtt_run(&t, handle);

			if (ret < 0)
			{
				Debug::log("Failed to wait for the SUBACK, error {}.", ret);
				status_leds()->led_off(3);
				status_leds()->led_off(4);
				mqtt_disconnect(
				  &t, STATIC_SEALED_VALUE(mqttTestMalloc), handle);
				break;
			}
		}
	}
}

void __cheri_compartment("hugh") graphs()
{
	Debug::log("Starting graphs thread");
	static constexpr uint32_t  ScreenWidth = 160;
	SonataLcd                  lcd;
	constexpr std::string_view TopicPrefix{"hugh-the-lightbulb/"};
	const size_t               HeapSize =
	  LA_ABS(__export_mem_heap_end) - LA_ABS(__export_mem_heap);

	Rect     memoryGraph{0, 118, ScreenWidth, 128};
	Rect     cpuGraph{0, 0, ScreenWidth, 10};
	uint64_t lastCycles     = 0;
	uint64_t lastIdleCycles = 0;
	topic.reserve(TopicPrefix.size() + 8);
	topic.append(TopicPrefix.data(), TopicPrefix.size());
#if 0
	topic.append("testing");
#else
	{
		EntropySource entropySource;
		for (int i = 0; i < 8; i++)
		{
			topic.push_back('a' + entropySource() % 26);
		}
	}
#endif
	barrier.store(1);
	barrier.notify_all();

	// We do all of the drawing with interrupts disabled because the Sonata SPI
	// has problems if we try to talk to both the Ethernet and LCD devices over
	// SPI.
	CHERI::with_interrupts_disabled([&]() {
		lcd.draw_str({55, 16}, "Hugh", Color::White, Color::Black);
		lcd.draw_str({10, 32}, "The Lightbulb", Color::White, Color::Black);
		lcd.draw_str({10, 64}, "ID:", Color::White, Color::Black);
		lcd.draw_str({50, 64},
		             topic.c_str() + TopicPrefix.size(),
		             Color::White,
		             Color::Black);
	});

	bool lastSwitchValue = false;
	int  faults          = 0;
	while (true)
	{
		CHERI::with_interrupts_disabled([&]() {
			if (faults > 0)
			{
				char buffer[14];
				Debug::log("Faults: {}", faults);
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
		});
		bool switchValue = status_leds()->read_switch(7);
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
			}
		}
		Timeout oneSecond{MS_TO_TICKS(500)};
		thread_sleep(&oneSecond, ThreadSleepFlags::ThreadSleepNoEarlyWake);
	}
}
