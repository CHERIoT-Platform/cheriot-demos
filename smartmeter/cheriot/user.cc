// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define CHERIOT_NO_AMBIENT_MALLOC

#include "user.hh"
#include <atomic>
#include <debug.hh>
#include <errno.h>
#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
#	include <fail-simulator-on-error.h>
#endif
#include <mqtt.h>
#include <multiwaiter.h>
#include <thread.h>
#include <tick_macros.h>
#include <timeout.hh>

#include MQTT_BROKER_ANCHOR

DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(UserMallocCapability, 24 * 1024);
#define MALLOC_CAPABILITY STATIC_SEALED_VALUE(UserMallocCapability)

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "user">;
using CHERI::Capability;

static uint32_t net_wake_count;
static uint32_t timebase_zero;
static uint16_t timebase_rate = 1;

#ifdef MONOLITH_BUILD_WITHOUT_SECURITY
userjs_snapshot theUserjsSnapshot;
#endif

/// Thread entry point.
int user_data_entry()
{
	Debug::log("data entry");

	static constexpr size_t nEvents = 6;

#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto sensorData = SHARED_OBJECT_WITH_PERMISSIONS(
	  sensor_data, sensor_data, true, false, false, false);
#else
	auto *sensorData = &theSensorData;
#endif

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
	                                            {&net_wake_count, 0}};

	{
		int ret =
		  blocking_forever<multiwaiter_create>(MALLOC_CAPABILITY, &mw, nEvents);
		Debug::Invariant(ret == 0, "Could not create multiwaiter object");
	}

#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto snapshots = SHARED_OBJECT_WITH_PERMISSIONS(
	  userjs_snapshot, userJS_snapshot, true, true, false, false);
#else
	auto *snapshots = &theUserjsSnapshot;
#endif

	while (true)
	{
		int ret = blocking_forever<multiwaiter_wait>(mw, events, nEvents);
		Debug::Invariant(ret == 0, "Failed to wait for events: {}", ret);

		userjs_snapshot localSnapshots;

		// Update snapshots
		// TODO: limited timeouts
		// TODO: collect which ones were updated to pass to JS
		Timeout t{UnlimitedTimeout};
		if (sensorData->read(&t, events[0].value, localSnapshots.sensor_data) ==
		    0)
		{
			/* Recompute our copy according to our timebase zero and rate */
			uint32_t &sts = localSnapshots.sensor_data.timestamp;
			if (sts >= timebase_zero)
			{
				uint32_t elapsed = sts - timebase_zero;
				elapsed *= timebase_rate;
				sts = timebase_zero + elapsed;
			}
		}
		gridOutage->read(&t, events[1].value, localSnapshots.grid_outage);
		gridRequest->read(&t, events[2].value, localSnapshots.grid_request);
		providerSchedule->read(
		  &t, events[3].value, localSnapshots.provider_schedule);
		providerVariance->read(
		  &t, events[4].value, localSnapshots.provider_variance);

		events[5].value = net_wake_count;

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
constexpr std::string_view clientIDPrefix{"cheriotsmu"};
/// Space for the random client ID.
static std::array<char, clientIDPrefix.size() + housekeeping_mqtt_unique_size>
  clientID;
static_assert(clientID.size() <= MQTTMaximumClientLength);

// MQTT network buffer sizes
constexpr const size_t networkBufferSize    = 4096;
constexpr const size_t incomingPublishCount = 2;
constexpr const size_t outgoingPublishCount = 2;

DECLARE_AND_DEFINE_CONNECTION_CAPABILITY(MQTTConnectionRightsUser,
                                         MQTT_BROKER_HOST,
                                         8883,
                                         ConnectionTypeTCP);

constexpr std::string_view jsTopicPrefix{"cheriot-smartmeter/u/js/"};
std::array<char, jsTopicPrefix.size() + housekeeping_mqtt_unique_size> jsTopic;

constexpr std::string_view timebaseTopicPrefix{
  "cheriot-smartmeter/u/timebase/"};
std::array<char, timebaseTopicPrefix.size() + housekeeping_mqtt_unique_size>
  timebaseTopic;

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

	if (topicView == std::string_view{jsTopic.data(), jsTopic.size()})
	{
		void *newPayload = blocking_forever<heap_allocate>(
		  MALLOC_CAPABILITY, payloadLength, AllocateWaitRevocationNeeded);

		if (!Capability{newPayload}.is_valid())
		{
			Debug::log("No memory for new JS; that's sad. {}", payloadLength);
			return;
		}

		memcpy(newPayload, payload, payloadLength);
		user_javascript_load(static_cast<const uint8_t *>(newPayload),
		                     payloadLength);

		// The other compartment has claimed it; drop our claim.
		heap_free(MALLOC_CAPABILITY, newPayload);

		net_wake_count++;
		futex_wake(&net_wake_count, UINT32_MAX);
	}
	else if (topicView ==
	         std::string_view{timebaseTopic.data(), timebaseTopic.size()})
	{
		/*
		 * 10 digits for 32-bit timebase_zero, space, 5 digits for unsigned
		 * 16-bit number, NUL
		 */
		char buf[10 + 1 + 5 + 1];

		if (payloadLength >= sizeof(buf))
		{
			Debug::log("Overlong timebase PUBLISH, discarding");
			return;
		}
		memcpy(buf, payload, payloadLength);
		buf[payloadLength] = '\0';

		char *ptr;
		timebase_zero = strtoul(buf, &ptr, 10);
		timebase_rate = strtoul(ptr, &ptr, 10);

		Debug::log("New timebase {} {}; re-evaulating policy",
		           timebase_zero,
		           timebase_rate);

		net_wake_count++;
		futex_wake(&net_wake_count, UINT32_MAX);
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
	HOUSEKEEPING_MQTT_CONCAT(timebaseTopic, timebaseTopicPrefix, mqttName);

	while (true)
	{
		int tick = 0;

		Debug::log("Connecting to MQTT broker...");

		MQTTConnection handle =
		  mqtt_connect(&noTimeout,
		               MALLOC_CAPABILITY,
		               CONNECTION_CAPABILITY(MQTTConnectionRightsUser),
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
			Debug::log("Failed to subscribe for new code: {}", ret);
			goto retry;
		}

		ret = mqtt_subscribe(&noTimeout,
		                     handle,
		                     1, // QoS 1 = delivered at least once
		                     timebaseTopic.data(),
		                     timebaseTopic.size());

		if (ret < 0)
		{
			Debug::log("Failed to subscribe for timebase: {}", ret);
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
