
// This file is generated by js/ffigen.js; do not edit.
// Regenerate with "microvium --no-snapshot ffigen.js"
// and "clang-format -i userffi.h"
#pragma once

enum userjs_snapshot_index
{
	DATA_SENSOR_TIMESTAMP                 = 1,
	DATA_SENSOR_SAMPLE                    = 2,
	DATA_GRID_OUTAGE_START                = 3,
	DATA_GRID_OUTAGE_DURATION             = 4,
	DATA_GRID_REQUEST_START_TIME          = 5,
	DATA_GRID_REQUEST_DURATION            = 6,
	DATA_GRID_REQUEST_SEVERITY            = 7,
	DATA_PROVIDER_SCHEDULE_TIMESTAMP_DAY  = 8,
	DATA_PROVIDER_SCHEDULE_RATE           = 9,
	DATA_PROVIDER_SCHEDULE_RATE_ARRAY     = 10,
	DATA_PROVIDER_VARIANCE_TIMESTAMP_BASE = 11,
	DATA_PROVIDER_VARIANCE_START          = 12,
	DATA_PROVIDER_VARIANCE_DURATION       = 13,
	DATA_PROVIDER_VARIANCE_RATE           = 14,
};

int32_t read_from_snapshot(int32_t type, int32_t index)
{
#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto snapshots = SHARED_OBJECT_WITH_PERMISSIONS(
	  userjs_snapshot, userJS_snapshot, true, false, false, false);
#else
	auto *snapshots = &SHARED_OBJECT_WITH_PERMISSIONS(
	                     merged_data, merged_data, true, true, false, false)
	                     ->userjs_snapshot;
#endif

	switch (type)
	{
		case DATA_SENSOR_TIMESTAMP:
			return snapshots->sensor_data.timestamp;
		case DATA_SENSOR_SAMPLE:
			if ((index < 0) ||
			    (index >= (sizeof(snapshots->sensor_data.samples) /
			               sizeof(snapshots->sensor_data.samples[0]))))
			{
				return 0;
			}
			return snapshots->sensor_data.samples[index];
		case DATA_GRID_OUTAGE_START:
			return snapshots->grid_outage.start_time;
		case DATA_GRID_OUTAGE_DURATION:
			return snapshots->grid_outage.duration;
		case DATA_GRID_REQUEST_START_TIME:
			return snapshots->grid_request.start_time;
		case DATA_GRID_REQUEST_DURATION:
			return snapshots->grid_request.duration;
		case DATA_GRID_REQUEST_SEVERITY:
			return snapshots->grid_request.severity;
		case DATA_PROVIDER_SCHEDULE_TIMESTAMP_DAY:
			return snapshots->provider_schedule.timestamp_day;
		case DATA_PROVIDER_SCHEDULE_RATE:
			if ((index < 0) ||
			    (index >= (sizeof(snapshots->provider_schedule.rate) /
			               sizeof(snapshots->provider_schedule.rate[0]))))
			{
				return 0;
			}
			return snapshots->provider_schedule.rate[index];
		case DATA_PROVIDER_SCHEDULE_RATE_ARRAY:
			register_write(index, &snapshots->provider_schedule.rate);
			return 0;
		case DATA_PROVIDER_VARIANCE_TIMESTAMP_BASE:
			return snapshots->provider_variance.timestamp_base;
		case DATA_PROVIDER_VARIANCE_START:
			if ((index < 0) ||
			    (index >= (sizeof(snapshots->provider_variance.start) /
			               sizeof(snapshots->provider_variance.start[0]))))
			{
				return 0;
			}
			return snapshots->provider_variance.start[index];
		case DATA_PROVIDER_VARIANCE_DURATION:
			if ((index < 0) ||
			    (index >= (sizeof(snapshots->provider_variance.duration) /
			               sizeof(snapshots->provider_variance.duration[0]))))
			{
				return 0;
			}
			return snapshots->provider_variance.duration[index];
		case DATA_PROVIDER_VARIANCE_RATE:
			if ((index < 0) ||
			    (index >= (sizeof(snapshots->provider_variance.rate) /
			               sizeof(snapshots->provider_variance.rate[0]))))
			{
				return 0;
			}
			return snapshots->provider_variance.rate[index];
	}
	return 0;
}
