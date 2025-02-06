// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define MALLOC_QUOTA 25000
#include "user.hh"
#include <atomic>
#include <debug.hh>
#include <errno.h>
#include <mqtt.h>
#include <multiwaiter.h>
#include <thread.h>
#include <tick_macros.h>
#include <timeout.hh>

#include "mosquitto.org.h"

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "user">;
using CHERI::Capability;

uint32_t code_load_count;

/// Thread entry point.
int user_data_entry()
{
	Debug::log("data entry");

	static constexpr size_t nEvents = 6;

	auto sensorData = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data, sensor_data, true, false, false, false);
	auto gridOutage = SHARED_OBJECT_WITH_PERMISSIONS(
	  grid_planned_outage, grid_planned_outage, true, false, false, false);
	auto gridRequest = SHARED_OBJECT_WITH_PERMISSIONS(
	  grid_request, grid_request, true, false, false, false);
	auto providerSchedule = SHARED_OBJECT_WITH_PERMISSIONS(
	  provider_schedule, provider_schedule, true, false, false, false);
	auto providerVariance = SHARED_OBJECT_WITH_PERMISSIONS(
	  provider_variance, provider_variance, true, false, false, false);

	MultiWaiter              mw;
	struct EventWaiterSource events[nEvents] = {{&sensorData->version, 0},
	                                            {&gridOutage->version, 0},
	                                            {&gridRequest->version, 0},
	                                            {&providerSchedule->version, 0},
	                                            {&providerVariance->version, 0},
	                                            {&code_load_count, 0}};

	{
		int ret =
		  blocking_forever<multiwaiter_create>(MALLOC_CAPABILITY, &mw, nEvents);
		Debug::Invariant(ret == 0, "Could not create multiwaiter object");
	}

	auto snapshots = SHARED_OBJECT_WITH_PERMISSIONS(
	  userjs_snapshot, userJS_snapshot, true, true, false, false);

	while (true)
	{
		int ret = blocking_forever<multiwaiter_wait>(mw, events, nEvents);
		Debug::Invariant(ret == 0, "Failed to wait for events: {}", ret);

		userjs_snapshot localSnapshots;

		// Update snapshots
		// TODO: limited timeouts
		// TODO: collect which ones were updated to pass to JS
		Timeout t{UnlimitedTimeout};
		sensorData->read(&t, events[0].value, localSnapshots.sensor_data);
		gridOutage->read(&t, events[1].value, localSnapshots.grid_outage);
		gridRequest->read(&t, events[2].value, localSnapshots.grid_request);
		providerSchedule->read(
		  &t, events[3].value, localSnapshots.provider_schedule);
		providerVariance->read(
		  &t, events[4].value, localSnapshots.provider_variance);

		events[5].value = code_load_count;

		*snapshots = localSnapshots;

		ret = user_javascript_run();
		if (ret == -ECOMPARTMENTFAIL)
		{
			Debug::log("JavaScript compartment crashed during tick");
		}
	}

	return 0;
}

/// Maximum permitted MQTT client identifier length (from the MQTT
/// specification)
constexpr size_t MQTTMaximumClientLength = 23;
/// Prefix for MQTT client identifier
constexpr std::string_view clientIDPrefix{"cheriot-smu-"};
/// Space for the random client ID.
std::array<char, clientIDPrefix.size() + housekeeping_mqtt_unique_size>
  clientID;
static_assert(clientID.size() <= MQTTMaximumClientLength);

// MQTT network buffer sizes
constexpr const size_t networkBufferSize    = 4096;
constexpr const size_t incomingPublishCount = 2;
constexpr const size_t outgoingPublishCount = 2;

// MQTT test broker: https://test.mosquitto.org/
// Note: port 8883 is encrypted and unautenticated
DECLARE_AND_DEFINE_CONNECTION_CAPABILITY(MosquittoOrgMQTT,
                                         "test.mosquitto.org",
                                         8883,
                                         ConnectionTypeTCP);

constexpr std::string_view jsTopicPrefix{"cheriot-smartmeter/u/js/"};
std::array<char, jsTopicPrefix.size() + housekeeping_mqtt_unique_size> jsTopic;

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

	if (topicView == std::string_view{jsTopic.data(), jsTopic.size()})
	{
		void *newPayload = malloc(payloadLength);

		if (!Capability{newPayload}.is_valid())
		{
			Debug::log("No memory for new JS; that's sad. {}", payloadLength);
			return;
		}

		memcpy(newPayload, payload, payloadLength);
		user_javascript_load(static_cast<const uint8_t *>(newPayload),
		                     payloadLength);

		code_load_count++;
		futex_wake(&code_load_count, UINT32_MAX);

		// The other compartment has claimed it; drop our claim.
		free(newPayload);
	}
	else
	{
		Debug::log("Unknown topic in PUBLISH callback: {}", topicView);
	}
}

/// Thread entry point.
int user_net_entry()
{
	int     ret;
	Timeout noTimeout{UnlimitedTimeout};

	Debug::log("net entry");
	housekeeping_initial_barrier();
	Debug::log("initialization barrier down");

	const char *mqttName = housekeeping_mqtt_unique_get();
	HOUSEKEEPING_MQTT_CONCAT(clientID, clientIDPrefix, mqttName);
	HOUSEKEEPING_MQTT_CONCAT(jsTopic, jsTopicPrefix, mqttName);

	while (true)
	{
		int tick = 0;

		Debug::log("Connecting to MQTT broker...");

		MQTTConnection handle =
		  mqtt_connect(&noTimeout,
		               MALLOC_CAPABILITY,
		               CONNECTION_CAPABILITY(MosquittoOrgMQTT),
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
		                     jsTopic.data(),
		                     jsTopic.size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe for outages: {}", ret);
			goto retry;
		}

		while (true)
		{
			ret = mqtt_run(&noTimeout, handle);

			if (ret < 0)
			{
				Debug::log("Failed to run MQTT, error {}; hanging up", ret);
				goto retry;
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
