// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/*
 * The provider compartment.
 *
 * Maintains a MQTT connection and reports sensor data
 */

#define MALLOC_QUOTA 24000

#include "common.hh"

#include <NetAPI.h>
#include <cstdlib>
#include <debug.hh>
#include <errno.h>
#include <fail-simulator-on-error.h>
#include <futex.h>
#include <mqtt.h>
#include <sntp.h>
#include <thread.h>
#include <tick_macros.h>

#include MQTT_BROKER_ANCHOR

using CHERI::Capability;

using Debug = ConditionalDebug<true, "provider">;

/// Maximum permitted MQTT client identifier length (from the MQTT
/// specification)
constexpr size_t MQTTMaximumClientLength = 23;
/// Prefix for MQTT client identifier
constexpr std::string_view clientIDPrefix{"cheriotsmp"};
/// Space for the random client ID.
std::array<char, clientIDPrefix.size() + housekeeping_mqtt_unique_size>
  clientID;
static_assert(clientID.size() <= MQTTMaximumClientLength);

// MQTT network buffer sizes
constexpr const size_t networkBufferSize    = 1024;
constexpr const size_t incomingPublishCount = 2;
constexpr const size_t outgoingPublishCount = 2;

DECLARE_AND_DEFINE_CONNECTION_CAPABILITY(MQTTConnectionRights,
                                         MQTT_BROKER_HOST,
                                         8883,
                                         ConnectionTypeTCP);

constexpr std::string_view scheduleTopicPrefix{
  "cheriot-smartmeter/p/schedule/"};
std::array<char, scheduleTopicPrefix.size() + housekeeping_mqtt_unique_size>
                           scheduleTopic;
constexpr std::string_view varianceTopicPrefix{
  "cheriot-smartmeter/p/variance/"};
std::array<char, varianceTopicPrefix.size() + housekeeping_mqtt_unique_size>
                           varianceTopic;
constexpr std::string_view publishTopicPrefix{"cheriot-smartmeter/p/update/"};
std::array<char, publishTopicPrefix.size() + housekeeping_mqtt_unique_size>
  publishTopic;

void __cheri_callback publishCallback(const char *topicName,
                                      size_t      topicNameLength,
                                      const void *payload,
                                      size_t      payloadLength)
{
	// Check input pointers (can be skipped if the MQTT library is trusted)
	Timeout t{MS_TO_TICKS(5000)};
	if (heap_claim_ephemeral(&t, topicName) != 0 ||
	    !CHERI::check_pointer(topicName, topicNameLength))
	{
		Debug::log(
		  "Cannot claim or verify PUBLISH callback topic name pointer.");
		return;
	}

	if (heap_claim_ephemeral(&t, payload) != 0 ||
	    !CHERI::check_pointer(payload, payloadLength))
	{
		Debug::log("Cannot claim or verify PUBLISH callback payload pointer.");
		return;
	}

	auto topicView = std::string_view{topicName, topicNameLength};
	auto payloadView =
	  std::string_view{static_cast<const char *>(payload), payloadLength};

	if (topicView ==
	    std::string_view{scheduleTopic.data(), scheduleTopic.size()})
	{
		/*
		 * 10 digits for 32-bit timestamp_day, space, 48 space-separated signed
		 * 16-bit numbers (5 digits, 1 sign), and a NUL byte
		 */
		char buf[10 + 1 + 48 * (6 + 1) + 1];

		Debug::log("Got schedule PUBLISH: {}", payloadView);

		if (payloadLength >= sizeof(buf))
		{
			Debug::log("Overlong outage PUBLISH, discarding");
		}
		memcpy(buf, payload, payloadLength);
		buf[payloadLength] = '\0';

		struct provider_schedule_payload payload;
		char                            *ptr;
		payload.timestamp_day = strtoul(buf, &ptr, 10);
		for (int i = 0; i < 48; i++)
		{
			payload.rate[i] = strtol(ptr, &ptr, 10);
		}

		auto providerSchedule = SHARED_OBJECT_WITH_PERMISSIONS(
		  provider_schedule, provider_schedule, true, true, false, false);
		providerSchedule->write(payload);
	}
	else if (topicView ==
	         std::string_view{varianceTopic.data(), varianceTopic.size()})
	{
		/*
		 * 10 digits for 32-bit timestamp_base, space, 4 space-separated signed
		 * 16-bit numbers (5 digits, 1 sign), 2 space-separated 16-bit unsigned
		 * numbers (5 digits, no sign), and a NUL byte.
		 */
		char buf[10 + 1 + 4 * (6 + 1) + 2 * (5 + 1) + 1];

		Debug::log("Got variance PUBLISH: {}", payloadView);

		if (payloadLength >= sizeof(buf))
		{
			Debug::log("Overlong outage PUBLISH, discarding");
		}
		memcpy(buf, payload, payloadLength);
		buf[payloadLength] = '\0';

		struct provider_variance_payload payload;
		char                            *ptr;
		payload.timestamp_base = strtoul(buf, &ptr, 10);
		for (int i = 0; i < 2; i++)
		{
			payload.start[i]    = strtol(ptr, &ptr, 10);
			payload.duration[i] = strtoul(ptr, &ptr, 10);
			payload.rate[i]     = strtol(ptr, &ptr, 10);
		}

		auto providerVariance = SHARED_OBJECT_WITH_PERMISSIONS(
		  provider_variance, provider_variance, true, true, false, false);
		providerVariance->write(payload);
	}
	else
	{
		Debug::log("Unknown topic in PUBLISH callback: {}", topicView);
	}
}

int provider_entry()
{
	int     ret;
	Timeout noTimeout{UnlimitedTimeout};

	Debug::log("entry");
	housekeeping_initial_barrier();
	Debug::log("initialization barrier down");

	const char *mqttName = housekeeping_mqtt_unique_get();
	HOUSEKEEPING_MQTT_CONCAT(clientID, clientIDPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(publishTopic, publishTopicPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(scheduleTopic, scheduleTopicPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(varianceTopic, varianceTopicPrefix, mqttName);

	while (true)
	{
		int tick = 0;

		Debug::log("Connecting to MQTT broker...");

		MQTTConnection handle =
		  mqtt_connect(&noTimeout,
		               MALLOC_CAPABILITY,
		               CONNECTION_CAPABILITY(MQTTConnectionRights),
		               publishCallback,
		               nullptr /* XXX should watch our ACK stream */,
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
			goto retry;
		}

		Debug::log("Connected to MQTT broker!");

		ret = mqtt_subscribe(&noTimeout,
		                     handle,
		                     1, // QoS 1 = delivered at least once
		                     scheduleTopic.data(),
		                     scheduleTopic.size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe for outages: {}", ret);
			goto retry;
		}

		// XXX clobbers packet ID; we should be watching our ACK stream
		ret = mqtt_subscribe(&noTimeout,
		                     handle,
		                     1, // QoS 1 = delivered at least once
		                     varianceTopic.data(),
		                     varianceTopic.size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe for requests: {}", ret);
			goto retry;
		}

		{
			auto sensorData = SHARED_OBJECT_WITH_PERMISSIONS(
			  sensor_data, sensor_data, true, false, false, false);

			sensor_data localSensorData = {0};

			Timeout loopTimeout{MS_TO_TICKS(5000)};
			while (true)
			{
				ret = mqtt_run(&loopTimeout, handle);

				if (ret < 0)
				{
					Debug::log("Failed to run MQTT, error {}; hanging up", ret);
					goto retry;
				}
				else if (loopTimeout.remaining == 0)
				{
					/*
					 * XXX We'd love to be multi-waiting on the network stack
					 * and the sensorData->timestamp, but the APIs we have make
					 * that harder than it should be.
					 */

					Timeout readTimeout{MS_TO_TICKS(1000)};
					ret = sensorData->read(&readTimeout,
					                       localSensorData.version,
					                       localSensorData.payload);
					if (ret == 0)
					{
						Debug::log("Awake and publishing {}",
						           localSensorData.version);

						char    msg[32];
						ssize_t msglen = snprintf(
						  msg, sizeof(msg), "Tick %d", localSensorData.version);

						Timeout t{MS_TO_TICKS(5000)};
						ret = mqtt_publish(&t,
						                   handle,
						                   1, // QoS 1 = delivered at least once
						                   publishTopic.data(),
						                   publishTopic.size(),
						                   msg,
						                   msglen);

						if (ret < 0)
						{
							Debug::log("Failed to publish, error {}.", ret);
							goto retry;
						}
					}
					else
					{
						Debug::log("Awake but skipping publish: {}", ret);
					}

					loopTimeout = Timeout{MS_TO_TICKS(5000)};
				}
			}
		}

	retry:
		if (Capability{handle}.is_valid())
		{
			mqtt_disconnect(&noTimeout, MALLOC_CAPABILITY, handle);
		}

		Timeout t{MS_TO_TICKS(5000)};
		thread_sleep(&t, ThreadSleepNoEarlyWake);
	}

	return 0;
}
