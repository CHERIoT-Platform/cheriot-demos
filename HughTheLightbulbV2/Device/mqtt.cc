// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "interface.hh"

#include <NetAPI.h>
#include <cstdlib>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <locks.hh>
#include <mqtt.h>
#include <platform-entropy.hh>
#include <platform-gpio.hh>
#include <platform-rgbctrl.hh>
#include <sntp.h>
#include <tick_macros.h>

// Uncomment to use the demo.cheriot.org server instead of mosquitto.
// This is faster (not rate limited) but might not be running.
// #define CHERIOT_DEMO_SERVER 1

#ifdef CHERIOT_DEMO_SERVER
#	include "letsencrypt.h"
#else
#	include "mosquitto.org.h"
#endif

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
// Note: port 8883 is encrypted and unauthenticated
DECLARE_AND_DEFINE_CONNECTION_CAPABILITY(MQTTServerCapability,
#ifdef CHERIOT_DEMO_SERVER
                                         "demo.cheriot.org",
#else
                                         "test.mosquitto.org",
#endif
                                         8883,
                                         ConnectionTypeTCP);

/// Separate allocation quota for the network, for accounting.
DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(mqttMalloc, 32 * 1024);

namespace
{
	/**
	 * Generic prefix in MQTT.  Must be kept in sync with the mobile app.
	 */
	constexpr std::string_view TopicPrefix{"hugh-the-lightbulb/"};

	/**
	 * Generate and return a topic for this boot.
	 */
	const std::string &topic()
	{
		// C++ thread-safe initialisation
		static const std::string Topic = []() {
			std::string t;
			t.reserve(TopicPrefix.size() + TopicSuffixLength);
			t.append(TopicPrefix.data(), TopicPrefix.size());
			{
				EntropySource e;
				for (int i = 0; i < TopicSuffixLength; i++)
				{
					t.push_back('a' + e() % 26);
				}
			}
			return t;
		}();
		return Topic;
	}

	/**
	 * Returns a capability to the status LEDs.
	 */
	auto status_leds()
	{
		return MMIO_CAPABILITY(SonataGPIO,
#if DEVICE_EXISTS(gpio_board)
		                       gpio_board
#else
		                       gpio
#endif
		);
	}

} // namespace

const char *topic_suffix()
{
	CHERI::Capability ptr = topic().c_str() + TopicPrefix.size();
	ptr.permissions() &= {CHERI::Permission::Load};
	return ptr;
}

void __cheri_callback publish_callback(const char *topic,
                                       size_t      topicLength,
                                       const void *payload,
                                       size_t      payloadLength)
{
	// Ignore invalid topics
	if (::topic() != std::string_view{topic, topicLength})
	{
		Debug::log(
		  "Received a publish message on topic '{}', but expected '{}'",
		  std::string_view{topic, topicLength},
		  ::topic());
		return;
	}

	// Decrypt the message.
	uint8_t colours[3];
	if (int err = decrypt(
	      colours, static_cast<const uint8_t *>(payload), payloadLength);
	    err != 0)
	{
		Debug::log("Decryption failed: {}", err);
		status_leds()->led_on(7);
		return;
	}

	// Update the LEDs.
	auto leds = MMIO_CAPABILITY(SonataRgbLedController, rgbled);
	leds->rgb(SonataRgbLed::Led0, colours[0], colours[1], colours[2]);
	leds->update();
	status_leds()->led_off(7);
}

void __cheri_compartment("hugh_network") run()
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
		auto handle = mqtt_connect(&t,
		                           STATIC_SEALED_VALUE(mqttMalloc),
		                           CONNECTION_CAPABILITY(MQTTServerCapability),
		                           publish_callback,
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

		Debug::log("Subscribing to topic '{}' ({} bytes)",
		           topic().c_str(),
		           topic().size());
		ret = mqtt_subscribe(&t,
		                     handle,
		                     1, // QoS 1 = delivered at least once
		                     topic().data(),
		                     topic().size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe, error {}.", ret);
			mqtt_disconnect(&t, STATIC_SEALED_VALUE(mqttMalloc), handle);
			continue;
		}

		status_leds()->led_on(4);

		while (true)
		{
			size_t heapFree = heap_available();
			ret             = mqtt_run(&t, handle);

			if (ret < 0)
			{
				Debug::log("Failed to wait for the SUBACK, error {}.", ret);
				status_leds()->led_off(3);
				status_leds()->led_off(4);
				mqtt_disconnect(&t, STATIC_SEALED_VALUE(mqttMalloc), handle);
				break;
			}
		}
	}
}
