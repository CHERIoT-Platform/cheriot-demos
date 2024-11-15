// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <NetAPI.h>
#include <cstdlib>
#include <debug.hh>
#include <errno.h>
#include <locks.hh>
#include <mqtt.h>
#include <platform-entropy.hh>
#include <sntp.h>
#include <tick_macros.h>
#include <unwind.h>

#include "mosquitto.org.h"

#include "config/include/system_config.h"

#include "config.h"
#include "status.h"

// Define sealed capability that gives this compartment
// read access to configuration data
#include "common/config_broker/config_broker.h"

#define SYSTEM_CONFIG "system"
DEFINE_READ_CONFIG_CAPABILITY(SYSTEM_CONFIG)

using CHERI::Capability;

using Debug            = ConditionalDebug<true, "Provider">;
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

std::string config_topic;
std::string status_topic;

/**
 * Generate config and status topics from an assigned sysetm ID
 */
void generate_topics(char *id)
{
	constexpr std::string_view TopicPrefix{"sonata-config/"};

	config_topic.clear();
	config_topic.reserve(TopicPrefix.size() + 8 + 7);
	config_topic.append(TopicPrefix.data(), TopicPrefix.size());
	config_topic.append("Config/", 7);
	config_topic.append(id, strlen(id));
	config_topic.append("/#", 2);

	status_topic.clear();
	status_topic.reserve(TopicPrefix.size() + 8 + 7);
	status_topic.append(TopicPrefix.data(), TopicPrefix.size());
	status_topic.append("Status/", 7);
	status_topic.append(id, strlen(id));
}

/**
 * Handle any incomming configuration messages.  The configurtaion
 * item being updated is the last element of the topic
 */
void __cheri_callback publishCallback(const char *topic,
                                      size_t      topicLength,
                                      const void *payload,
                                      size_t      payloadLength)
{
	Debug::log("Received a message on topic '{}'",
	           std::string_view{topic, topicLength});

	// Extract the config ID from the topic
	size_t idOffset = config_topic.size() - 1;

	if (idOffset < topicLength)
	{
		const char *id       = topic + idOffset;
		size_t      idLength = topicLength - idOffset;

		Debug::log("extracted ID '{}'", std::string_view{id, idLength});

		updateConfig(id, idLength, payload, payloadLength);
	}
	else
	{
		Debug::log("Missing config id in topic: {} {}", idOffset, topicLength);
	}
}

/**
 * Check for changes in system status.
 */
int update_status(SObj configHandle, SObj mqttHandle)
{
	static bool                  subscribed    = false;
	static uint32_t              configVersion = 0;
	static systemConfig::Config *sysConfig;

	// read the current system config
	auto config = get_config(configHandle);
	if (config.data == nullptr)
	{
		Debug::log("No System Config data yet");
		return 0;
	}

	if (config.version == configVersion)
	{
		return 0;
	}

	configVersion = config.version;
	Debug::log("Config updated to version {}", configVersion);
	Timeout t{5000};
	if (heap_claim(MALLOC_CAPABILITY, config.data) <= 0)
	{
		Debug::log("Claim failed for config data: {} error: {}");
		return -1;
	}

	systemConfig::Config *newSysConfig =
	  static_cast<systemConfig::Config *>(config.data);

	// Check if the ID has changed.  We can release our claim on the oldConfig
	// after this
	bool idChanged = true;
	if (sysConfig)
	{
		idChanged =
		  (strncmp(newSysConfig->id, sysConfig->id, systemConfig::IdLength) !=
		   0);
		free(sysConfig);
	}

	sysConfig = newSysConfig;

	// if the ID has changed we need to change the topics we're using
	if (idChanged)
	{
		if (subscribed)
		{
			clear_status(mqttHandle, status_topic);
			auto ret = mqtt_unsubscribe(&t,
			                            mqttHandle,
			                            1, // QoS 1 = delivered at least once
			                            config_topic.data(),
			                            config_topic.size());
			if (ret < 0)
			{
				Debug::log("Unsubscribe failed with {}", ret);
				return -1;
			}
		}

		// Update the topics to contain the system id
		generate_topics(sysConfig->id);

		Debug::log("Subscribing to topic '{}' ({} bytes)",
		           config_topic.c_str(),
		           config_topic.size());
		auto ret = mqtt_subscribe(&t,
		                          mqttHandle,
		                          1, // QoS 1 = delivered at least once
		                          config_topic.data(),
		                          config_topic.size());
		if (ret < 0)
		{
			Debug::log("Failed to subscribe, error {}.", ret);
			return -1;
		}

		subscribed = true;
	}

	send_status(mqttHandle, status_topic, sysConfig);

	return 0;
}

void __cheri_compartment("provider") provider_run()
{
	int     ret;
	Timeout t{MS_TO_TICKS(5000)};

	// Handle granting us access to read the system config
	SObj configHandle = READ_CONFIG_CAPABILITY(SYSTEM_CONFIG);

	// Prefix with something recognizable, for convenience.
	memcpy(clientID.data(), clientIDPrefix.data(), clientIDPrefix.size());
	// Suffix with random character chain.
	mqtt_generate_client_id(clientID.data() + clientIDPrefix.size(),
	                        clientID.size() - clientIDPrefix.size());

	while (true)
	{
		Debug::log("Connecting to MQTT broker...");

		t               = UnlimitedTimeout;
		SObj mqttHandle = mqtt_connect(&t,
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

		if (!Capability{mqttHandle}.is_valid())
		{
			Debug::log("Failed to connect.");
			continue;
		}

		Debug::log("Connected to MQTT broker!");

		// Use the unwind error handler around our
		// main loop
		on_error([&]() {
			// start the main loop, only exit this if
			// something goes wrong
			while (true)
			{
				auto ret = update_status(configHandle, mqttHandle);
				if (ret < 0)
				{
					Debug::log("update status failed - error {}", ret);
					break;
				}

				// process any received messages
				ret = mqtt_run(&t, mqttHandle);
				if (ret < 0)
				{
					Debug::log("mqtt_run failed - error {}", ret);
					break;
				}

				// Give other threads a chance to run
				Timeout t{MS_TO_TICKS(5)};
				thread_sleep(&t);
			}
		});

		// If we got here something went wrong, so disconnect
		// and try again
		Debug::log("Disconecting from MQTT server");
		mqtt_disconnect(&t, STATIC_SEALED_VALUE(mqttTestMalloc), mqttHandle);
	}
}
