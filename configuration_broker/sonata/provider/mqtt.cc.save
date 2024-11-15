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
#include <sntp.h>
#include <tick_macros.h>

#include "mosquitto.org.h"
#include "provider.h"

using CHERI::Capability;

using Debug            = ConditionalDebug<true, "MQTT">;
constexpr bool UseIPv6 = CHERIOT_RTOS_OPTION_IPv6;

/// Maximum permitted MQTT client identifier length (from the MQTT
/// specification)
constexpr size_t MQTTMaximumClientLength = 23;
/// Prefix for MQTT client identifier
constexpr std::string_view clientIDPrefix{"CT-Sonata"};
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
std::string status;

uint32_t get_current_time()
{
	// The return value is only a 32-bit integer, so we will
	// overflow after 4,294,967,295 ms (which should be about 7
	// weeks). The overflow is not a problem though, as wraparound
	// is defined and coreMQTT only uses this for additions and
	// substractions, not ordered comparisons. See:
	// https://github.com/FreeRTOS/coreMQTT/issues/277
	uint64_t currentCycle = rdcycle64();

	// Convert to milliseconds
	constexpr uint64_t MilliSecondsPerSecond = 1000;
	constexpr uint64_t CyclesPerMilliSecond =
		CPU_TIMER_HZ / MilliSecondsPerSecond;
	static_assert(CyclesPerMilliSecond > 0,
					"The CPU frequency is too low for the coreMQTT time "
					"function, which provides time in milliseconds.");
	uint64_t currentTime = currentCycle / CyclesPerMilliSecond;

	// Truncate into 32 bit
	return currentTime & 0xFFFFFFFF;
}


// Create a JSON view of the sonata status
auto create_status(bool switches[]) {
	constexpr std::string_view StatusPrefix{"{\"Status\":\"On\",\"switches\":["};
	status.clear();
	status.reserve(StatusPrefix.size() + 15 + 3);
	status.append(StatusPrefix.data(), StatusPrefix.size());
	for (auto i=0; i<8; i++) {
		status.append(switches[i]?"1":"0");
		if (i < 7)
			status.append(",");
	}
	status.append("]}");
	Debug::log("Status: {}", status.c_str());
}

// Create a JSON view of the sonata status
auto clear_status() {
	constexpr std::string_view StatusPrefix{"{\"Status\":\"Off\"}"};
	status.clear();
	status.reserve(StatusPrefix.size() + 15 + 3);
	status.append(StatusPrefix.data(), StatusPrefix.size());
	Debug::log("Status: {}", status.c_str());
}


//
// Build the config an status topics
//
auto create_topics() {
	
	char ids[11];
	snprintf(ids, sizeof(ids), "Sonata-%d", 1);

	constexpr std::string_view ConfigTopicPrefix{"CT-Sonata/Config/"};
	config_topic.clear();
	config_topic.reserve(ConfigTopicPrefix.size() + 8 + 2);
	config_topic.append(ConfigTopicPrefix.data(), ConfigTopicPrefix.size());
	config_topic.append(ids);
	config_topic.append("/#");

	constexpr std::string_view StatusTopicPrefix{"CT-Sonata/Status/"};
	status_topic.clear();
	status_topic.reserve(StatusTopicPrefix.size() + 8);
	status_topic.append(StatusTopicPrefix.data(), StatusTopicPrefix.size());
	status_topic.append(ids);
}

//
// Method called when we get an MQTT Message
//
void __cheri_callback publishCallback(const char *topic,
                                      size_t      topicLength,
                                      const void *payload,
                                      size_t      payloadLength)
{
	// Create a null terminated srting from the trailing
	// part of the topic
	auto conf_id_length = topicLength-(::config_topic.size()-1);
	auto conf_id = (char *)malloc(conf_id_length+1);
	std::memcpy((void *)conf_id, (void *)(topic + ::config_topic.size()-1), conf_id_length);
	conf_id[conf_id_length] = 0;
	//const char *conf_id = topic + ::config_topic.size()-1;
	Debug::log("topic: {}", topic);
	Debug::log("topic_Len {}, conf_len: {}, conf_id_length: {} ", (uint32_t)topicLength, (uint32_t)(config_topic.size()), (uint32_t)conf_id_length);
	Debug::log("conf_id: {}", (const char *)conf_id);

	// we need a null terminated string to parse
	//auto json = (char*)malloc(payloadLength+1);
	//std::memcpy((void *)json, payload, payloadLength);
	//json[payloadLength] = 0;
	
	//Debug::log("json {}", (const char*)json);
	updateConfig(conf_id, conf_id_length, payload, payloadLength);

	free(conf_id);
	//free(json);
}

void __cheri_compartment("mqtt") mqtt_init()
{
	int     ret;
	Timeout t{MS_TO_TICKS(5000)};

	Debug::log("Calling network start");
	network_start();

	// SNTP must be run for the TLS stack to be able to check certificate dates.
	while (sntp_update(&t) != 0)
	{
		Debug::log("Failed to update NTP time");
		t = Timeout{MS_TO_TICKS(5000)};
	}

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

	// Prefix with something recognizable, for convenience.
	memcpy(clientID.data(), clientIDPrefix.data(), clientIDPrefix.size());
	// Suffix with random character chain.
	mqtt_generate_client_id(clientID.data() + clientIDPrefix.size(),
	                        clientID.size() - clientIDPrefix.size());

		
	// Outer loop
	while (true)
	{
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

		if (!Capability{handle}.is_valid())
		{
			Debug::log("Failed to connect.");
			continue;
		}
	

		Debug::log("Connected to MQTT broker!");
		
		// Inner loop - runs all the time we are connected
		// Subscribe to our ID and then consume messages
		bool connected = true;
		while (connected) {
			create_topics();

			Debug::log(
			"Subscribing to topic '{}' ({} bytes)", config_topic.c_str(), config_topic.size());
			ret = mqtt_subscribe(&t,
								handle,
								1, // QoS 1 = delivered at least once
								config_topic.data(),
								config_topic.size());

			if (ret < 0)
			{
				Debug::log("Failed to subscribe, error {}.", ret);
				mqtt_disconnect(&t, STATIC_SEALED_VALUE(mqttTestMalloc), handle);
				break;
			}

			// Inner loop to process MQTT messages and report
			// switch changes
			while (true)
			{
				//size_t  heapFree = heap_available();
				//t = Timeout{MS_TO_TICKS(100000)};
				ret = mqtt_run(&t, handle);
				
				if (ret < 0)
				{
					Debug::log("Mqtt run failed, error {}.", ret);
					connected = false;
					break;  // trying to reconnect doesn't seem to work
				} 
				else 
				{	
					// If the ID has changed disconnect and reconnect so we
					// subsrcibe to the new set of topics 
					if (false) {
						Debug::log("ID changed");
						ret = mqtt_unsubscribe(&t,
								handle,
								1, // QoS 1 = delivered at least once
								config_topic.data(),
								config_topic.size());

						// Publish an empty status
						clear_status();
						ret = mqtt_publish(&t,
									handle,
									1, // QoS 1 = delivered at least once
									status_topic.data(),
									status_topic.size(),
									(void *)(status.data()),
									status.size());
						break;
					}

					// If the switch setting have changed update the status
					auto newSwitches = 0;
					if (false) {
						auto switches = newSwitches;
						
						// Send the status
						bool sw[]= {true, false};
						create_status(sw);
						
						Debug::log(
							"Publishing to topic '{}' ({} bytes)", status_topic.c_str(), status.c_str());
							ret = mqtt_publish(&t,
									handle,
									1, // QoS 1 = delivered at least once
									status_topic.data(),
									status_topic.size(),
									(void *)(status.data()),
									status.size());
					}
				}
			}
		}
		Debug::log("Disconnecting");
		mqtt_disconnect(&t, STATIC_SEALED_VALUE(mqttTestMalloc), handle);
		Debug::log("Continuing loop to reconnect");
		
	}
}

