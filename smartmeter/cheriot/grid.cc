// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/*
 * The grid compartment.
 *
 * Maintains a MQTT connection and reports sensor data
 */

#define CHERIOT_NO_AMBIENT_MALLOC

#include "common.hh"

#include <NetAPI.h>
#include <cstdlib>
#include <debug.hh>
#include <errno.h>
#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
#	include <fail-simulator-on-error.h>
#endif
#include <futex.h>
#include <mqtt.h>
#include <sntp.h>
#include <thread.h>
#include <tick_macros.h>

#include MQTT_BROKER_ANCHOR

DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(GridMallocCapability, 24 * 1024);
#define MALLOC_CAPABILITY STATIC_SEALED_VALUE(GridMallocCapability)

using CHERI::Capability;

using Debug = ConditionalDebug<true, "grid">;

/// Maximum permitted MQTT client identifier length (from the MQTT
/// specification)
constexpr size_t MQTTMaximumClientLength = 23;
/// Prefix for MQTT client identifier
constexpr std::string_view clientIDPrefix{"cheriotsmq"};
/// Space for the random client ID.
static std::array<char, clientIDPrefix.size() + housekeeping_mqtt_unique_size>
  clientID;
static_assert(clientID.size() <= MQTTMaximumClientLength);

// MQTT network buffer sizes
constexpr const size_t networkBufferSize    = 1024;
constexpr const size_t incomingPublishCount = 2;
constexpr const size_t outgoingPublishCount = 2;

DECLARE_AND_DEFINE_CONNECTION_CAPABILITY(MQTTConnectionRightsGrid,
                                         MQTT_BROKER_HOST,
                                         8883,
                                         ConnectionTypeTCP);

constexpr std::string_view outageTopicPrefix{"cheriot-smartmeter/g/outage/"};
static std::array<char,
                  outageTopicPrefix.size() + housekeeping_mqtt_unique_size>
                           outageTopic;
constexpr std::string_view requestTopicPrefix{"cheriot-smartmeter/g/request/"};
static std::array<char,
                  requestTopicPrefix.size() + housekeeping_mqtt_unique_size>
                           requestTopic;
constexpr std::string_view publishTopicPrefix{"cheriot-smartmeter/g/update/"};
static std::array<char,
                  publishTopicPrefix.size() + housekeeping_mqtt_unique_size>
                           publishTopic;
constexpr std::string_view crashTopicPrefix{"cheriot-smartmeter/g/crash/"};
static std::array<char, crashTopicPrefix.size() + housekeeping_mqtt_unique_size>
  crashTopic;

static void __cheri_callback publishCallback(const char *topicName,
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

	if (topicView == std::string_view{outageTopic.data(), outageTopic.size()})
	{
		char buf[22]; // uint32_t is up to 10 decimal chars; add SP and NUL

		Debug::log("Got outage PUBLISH: {}", payloadView);

		if (payloadLength >= sizeof(buf))
		{
			Debug::log("Overlong outage PUBLISH, discarding");
			return;
		}
		memcpy(buf, payload, payloadLength);
		buf[payloadLength] = '\0';

		struct grid_planned_outage_payload payload;
		char                              *ptr;
		payload.start_time = strtoul(buf, &ptr, 10);
		payload.duration   = strtoul(ptr, nullptr, 10);

		auto gridOutage = SHARED_OBJECT_WITH_PERMISSIONS(
		  grid_planned_outage, grid_planned_outage, true, true, false, false);
		gridOutage->write(payload);
	}
	if (topicView == std::string_view{requestTopic.data(), requestTopic.size()})
	{
		/*
		 * 10 digits for 32-bit timestamp_day, space, unsigned 16-bit number (5
		 * digits), space, signed 16-bit number (5 digits + 1 sign), and a NUL
		 * byte.
		 */
		char buf[10 + 1 + 5 + 1 + 6 + 1];

		Debug::log("Got request PUBLISH: {}", payloadView);

		if (payloadLength >= sizeof(buf))
		{
			Debug::log("Overlong request PUBLISH, discarding");
			return;
		}
		memcpy(buf, payload, payloadLength);
		buf[payloadLength] = '\0';

		struct grid_request_payload payload;
		char                       *ptr;
		payload.start_time = strtoul(buf, &ptr, 10);
		payload.duration   = strtoul(ptr, &ptr, 10);
		payload.severity   = strtol(ptr, nullptr, 10);

		auto gridRequest = SHARED_OBJECT_WITH_PERMISSIONS(
		  grid_request, grid_request, true, true, false, false);
		gridRequest->write(payload);
	}
	else
	{
		Debug::log("Unknown topic in PUBLISH callback: {}", topicView);
	}
}

int grid_entry()
{
	int     ret;
	Timeout noTimeout{UnlimitedTimeout};

	Debug::log("entry");
	housekeeping_initial_barrier();
	Debug::log("initialization barrier down");

	const char *mqttName = housekeeping_mqtt_unique_get();
	HOUSEKEEPING_MQTT_CONCAT(clientID, clientIDPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(crashTopic, crashTopicPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(outageTopic, outageTopicPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(publishTopic, publishTopicPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(requestTopic, requestTopicPrefix, mqttName);

#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto sensorDataFine = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data_fine, sensor_data_fine, true, false, false, false);
#else
	auto sensorDataFine = &SHARED_OBJECT_WITH_PERMISSIONS(
	                         merged_data, merged_data, true, true, false, false)
	                         ->sensor_data_fine;
#endif

	auto userCrashCount = SHARED_OBJECT_WITH_PERMISSIONS(
	  user_crash_count, user_crash_count, true, false, false, false);

	while (true)
	{
		int tick = 0;

		Debug::log("Connecting to MQTT broker...");

		Timeout connectTimeout{MS_TO_TICKS(30000)};

		MQTTConnection handle =
		  mqtt_connect(&connectTimeout,
		               MALLOC_CAPABILITY,
		               CONNECTION_CAPABILITY(MQTTConnectionRightsGrid),
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

		ret = mqtt_subscribe(&connectTimeout,
		                     handle,
		                     1, // QoS 1 = delivered at least once
		                     outageTopic.data(),
		                     outageTopic.size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe for outages: {}", ret);
			goto retry;
		}

		// XXX clobbers packet ID; we should be watching our ACK stream
		ret = mqtt_subscribe(&connectTimeout,
		                     handle,
		                     1, // QoS 1 = delivered at least once
		                     requestTopic.data(),
		                     requestTopic.size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe for requests: {}", ret);
			goto retry;
		}

		{
			sensor_data_fine localSensorData = {0};
			user_crash_count localCrashData  = {0};

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
					 * XXX We'd love to be multi-waiting on the network / MQTT
					 * and the sensorData->timestamp, but the APIs we have make
					 * that harder than it should be.
					 */

					// sensor data
					{
						Timeout readTimeout{MS_TO_TICKS(1000)};
						ret = sensorDataFine->read(&readTimeout,
						                           localSensorData.version,
						                           localSensorData.payload);
						if (ret == 0)
						{
							Debug::log("Awake and publishing {}",
							           localSensorData.version);

							/*
							 * 10 digits for 3 32-bit values, space separated,
							 * with NUL terminator: the timestamp and two most
							 * recent values.
							 */
							char    msg[(10 + 1) * 3];
							ssize_t msglen =
							  snprintf(msg,
							           sizeof(msg),
							           "%d %d %d",
							           localSensorData.payload.timestamp,
							           localSensorData.payload.samples[0],
							           localSensorData.payload.samples[1]);

							Timeout t{MS_TO_TICKS(5000)};
							ret =
							  mqtt_publish(&t,
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
					}

					// crash count
					{
						Timeout readTimeout{MS_TO_TICKS(1000)};
						ret = userCrashCount->read(&readTimeout,
						                           localCrashData.version,
						                           localCrashData.payload);
						if (ret == 0)
						{
							Debug::log(
							  "Awake and publishing crash notification {}",
							  localCrashData.version);

							/*
							 * 10 digits for a 32-bit value and NUL terminator
							 */
							char    msg[10 + 1];
							ssize_t msglen = snprintf(
							  msg,
							  sizeof(msg),
							  "%d",
							  localCrashData.payload.crashes_since_boot);

							Timeout t{MS_TO_TICKS(5000)};
							ret =
							  mqtt_publish(&t,
							               handle,
							               1, // QoS 1 = delivered at least once
							               crashTopic.data(),
							               crashTopic.size(),
							               msg,
							               msglen);

							if (ret < 0)
							{
								Debug::log("Failed to publish, error {}.", ret);
								goto retry;
							}
						}
						else if (ret != -ENOMSG)
						{
							Debug::log("Unexpected crash count read result: {}",
							           ret);
						}
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
