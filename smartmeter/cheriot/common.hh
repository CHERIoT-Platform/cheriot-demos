#pragma once
#include <atomic>
#include <compartment.h>
#include <errno.h>
#include <stdint.h>

/**
 * @defgroup entryvectors Compartment entry vectors
 * @{
 */

#ifdef MONOLITH_BUILD_WITHOUT_SECURITY
#	define SMARTMETER_COMPARTMENT(x) __cheri_compartment("monolith")
#else
#	define SMARTMETER_COMPARTMENT(x) __cheri_compartment(x)
#endif

int SMARTMETER_COMPARTMENT("grid") grid_entry();
int SMARTMETER_COMPARTMENT("housekeeping") housekeeping_entry();
int SMARTMETER_COMPARTMENT("provider") provider_entry();
int SMARTMETER_COMPARTMENT("sensor") sensor_entry();
int SMARTMETER_COMPARTMENT("user") user_data_entry();
int SMARTMETER_COMPARTMENT("user") user_net_entry();

/* @} */

/**
 * @defgroup crosscalls Compartment cross-calls
 * @{
 */

/**
 * Wait for housekeeping to perform initialization tasks
 */
void SMARTMETER_COMPARTMENT("housekeeping") housekeeping_initial_barrier();

static constexpr size_t housekeeping_mqtt_unique_size = 8;

/**
 * The housekeeping compartment also builds a per-boot 8-character unique value
 * for us for use with client IDs and subscription topics.  We can ask it to
 * copy it out to a local buffer.
 */
const char *SMARTMETER_COMPARTMENT("housekeeping")
  housekeeping_mqtt_unique_get();

/**
 * This pattern shows up in all our MQTT threads, so give it a short name.
 */
#define HOUSEKEEPING_MQTT_CONCAT(out, prefix, mqttUnique)                      \
	memcpy(out.data(), prefix.data(), prefix.size());                          \
	memcpy(                                                                    \
	  out.data() + prefix.size(), mqttUnique, housekeeping_mqtt_unique_size);

/**
 * Replace the user policy code in the JS compartment
 *
 * Called by user
 */
int SMARTMETER_COMPARTMENT("userJS")
  user_javascript_load(const uint8_t *bytecode, size_t size);

/**
 * Run user policy code
 *
 * Called by user
 */
int SMARTMETER_COMPARTMENT("userJS") user_javascript_run(/* XXX */);

/* @} */

/**
 * @defgroup sharedstate Shared object types
 *
 * Version fields will be treated as futexes and will be odd during updates.
 *
 * @{
 */

template<typename Payload>
struct FutexVersioned
{
	/*
	 * If the version is congruent (mod 4) to ..., then payloads are ... :
	 *   - 0, uninitialized;
	 *   - 1, initialized and stable; or
	 *   - 3, being updated.
	 */

	uint32_t version;
	Payload  payload;

	void write(Payload &update)
	{
		// XXX: it's a pity we don't have C++20's atomic_ref

		__c11_atomic_thread_fence(__ATOMIC_RELEASE);
		this->version |= 0x3;
		this->payload = update;
		__c11_atomic_thread_fence(__ATOMIC_RELEASE);
		this->version += 2;
		futex_wake(&this->version, UINT32_MAX);
	}

	int read(Timeout *t, uint32_t &version, Payload &out)
	{
		uint32_t version_post;

		uint32_t version_pre = this->version;

		if (version_pre == version)
			return -ENOMSG;

		do
		{
			while ((version_pre & 0x2) != 0)
			{
				int res = futex_timed_wait(t, &this->version, version_pre);
				if (res < 0)
				{
					return res;
				}
				version_pre = this->version;
			}

			out = this->payload;
			__c11_atomic_thread_fence(__ATOMIC_ACQUIRE);
			version_post = this->version;

		} while (version_pre != version_post);

		version = version_pre;

		return 0;
	}
};

struct sensor_data_fine_payload
{
	uint32_t timestamp;  // of samples[0], each next a minute back in the past
	int32_t  samples[8]; // The past few minutes of sensor data
};

using sensor_data_fine = FutexVersioned<sensor_data_fine_payload>;
static_assert(sizeof(sensor_data_fine) == 40,
              "sensor_data object bad size; update xmake.lua");

static constexpr size_t SENSOR_COARSENING = 8;
static_assert(SENSOR_COARSENING <=
                sizeof(((sensor_data_fine_payload){}).samples) /
                  sizeof(((sensor_data_fine_payload){}).samples[0]),
              "Can't coarsen past fine log's end");

struct sensor_data_coarse_payload
{
	uint32_t timestamp; // of samples[0], each SENSOR_COARSENING samples back
	int32_t  samples[6];
};

using sensor_data_coarse = FutexVersioned<sensor_data_coarse_payload>;
static_assert(sizeof(sensor_data_coarse) == 32,
              "sensor_data object bad size; update xmake.lua");

/**
 * The next planned outage, if any.
 *
 * 0-duration outages do not exist.
 */
struct grid_planned_outage_payload
{
	uint32_t start_time; // Outage start time
	uint32_t duration;   // seconds after start
};

using grid_planned_outage = FutexVersioned<grid_planned_outage_payload>;
static_assert(sizeof(grid_planned_outage) == 12,
              "grid_planned_outage object bad size; update xmake.lua");

/**
 * A grid request for load modulation.
 *
 * Sign of `severity` indicates direction of request:
 *   - push (negative; grid running low) or
 *   - pull (positive; grid has excess power)
 * Magnitude is arbitrary but intended to reflect the risk of grid failure.
 */
struct grid_request_payload
{
	uint32_t start_time;
	uint16_t duration;
	int16_t  severity;
};

using grid_request = FutexVersioned<grid_request_payload>;
static_assert(sizeof(grid_request) == 12,
              "grid_request object bad size; update xmake.lua");

/**
 * Provider specified rate schedule
 */
struct provider_schedule_payload
{
	uint32_t timestamp_day; // Start of rate array; "today's midnight"
	int16_t  rate[48];      // Hourly rates, today then tomorrow, centipence/kWh
};

using provider_schedule = FutexVersioned<provider_schedule_payload>;
static_assert(sizeof(provider_schedule) == 104,
              "provider_schedule object bad size; update xmake.lua");

/**
 * Provider-signaled variance in pricing.
 *
 * Up to two variances may be reported at once, so that we can report both
 * the current one and an impending one.  A zero duration variance does not
 * exist.
 */
struct provider_variance_payload
{
	uint32_t timestamp_base;

	int16_t  start[2];    // seconds relative to timestamp_update, negative past
	uint16_t duration[2]; // seconds from start that variance applies
	int16_t  rate[2];     // Metering rate during this interval, p/kWh
};

using provider_variance = FutexVersioned<provider_variance_payload>;
static_assert(sizeof(provider_variance) == 20,
              "provider_variance object bad size; update xmake.lua");

/**
 * User signaled crash reporting
 */
struct user_crash_count_payload
{
	uint32_t crashes_since_boot;
};

using user_crash_count = FutexVersioned<user_crash_count_payload>;
static_assert(sizeof(user_crash_count) == 8,
              "user_crash_count object bad size; update xmake.lua");

/*
 * The most recent stable snapshot of all the data sources we're
 * monitoring.
 *
 * Updated by user compartment before entering userJS to respond.
 */
struct userjs_snapshot
{
	struct sensor_data_fine_payload sensor_data;

	struct grid_planned_outage_payload grid_outage;
	struct grid_request_payload        grid_request;

	struct provider_schedule_payload provider_schedule;
	struct provider_variance_payload provider_variance;
};
static_assert(sizeof(struct userjs_snapshot) == 168,
              "userjs_snapshot object bad size; update xmake.lua");

/* @} */

/*
 * In a compartmentalized build, we use static shared objects for communication.
 * In a monolithic build, just use globals, as we would in a non-CHERI system.
 */
#ifdef MONOLITH_BUILD_WITHOUT_SECURITY
extern struct mergedData
{
	sensor_data_fine   sensor_data_fine;
	sensor_data_coarse sensor_data_coarse;
	userjs_snapshot    userjs_snapshot;
} theData;

static_assert(
  (offsetof(struct mergedData, userjs_snapshot.provider_schedule.rate) -
   offsetof(struct mergedData, sensor_data_fine.payload.samples[0])) == 120,
  "Offsets shifted; update attack demo SENSOR_OFFSET_COARSE");

static_assert(
  (offsetof(struct mergedData, userjs_snapshot.provider_schedule.rate) -
   offsetof(struct mergedData, sensor_data_coarse.payload.samples[0])) == 80,
  "Offsets shifted; update attack demo SENSOR_OFFSET_FINE.");
#endif
