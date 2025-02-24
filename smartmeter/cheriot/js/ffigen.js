/*
 * Generate ../userffi.h
 */

import * as fs from 'fs';
import * as ffi from './ffi.js';

let header = `
// This file is generated by js/userffi.js; do not edit.
// Regenerate with "microvium --no-snapshot userffi.js"
// and "clang-format -i userffi.h"
#pragma once
`;

let userjs_snapshot_enum = `
enum userjs_snapshot_index
{
`;

let userjs_snapshot_fetch = `
int32_t read_from_snapshot(int32_t type, int32_t index)
{
#ifndef MONOLITH_BUILD_WITHOUT_SECURITY
	auto snapshots = SHARED_OBJECT_WITH_PERMISSIONS(
	  userjs_snapshot, userJS_snapshot, true, false, false, false);
#else
    auto *snapshots = &theData.userjs_snapshot;
#endif

	switch(type)
	{
`;

function add_userjs_snapshot_index(name, c, arrayish)
{
	userjs_snapshot_enum += `\t${name} = ${ffi[name]},\n`;

	let fullc = `snapshots->${c}`;

	userjs_snapshot_fetch += `\tcase ${name}:\n`;
	if (arrayish === "index")
	{
		userjs_snapshot_fetch += `\t\tif ((index < 0) || (index >= (sizeof(${fullc})/sizeof(${fullc}[0]))))\n`;
		userjs_snapshot_fetch += `\t\t{\n`;
		userjs_snapshot_fetch += `\t\t\treturn 0;\n`;
		userjs_snapshot_fetch += `\t\t}\n`;
		userjs_snapshot_fetch += `\t\treturn ${fullc}[index];\n`;
	}
	else if (arrayish === "expose")
    {
        userjs_snapshot_fetch += `\t\tregister_write(index, &${fullc});\n`;
		userjs_snapshot_fetch += `\t\treturn 0;\n`;
    }
	else
	{
		userjs_snapshot_fetch += `\t\treturn ${fullc};\n`;
	}
}

add_userjs_snapshot_index("DATA_SENSOR_TIMESTAMP", "sensor_data.timestamp", "no");
add_userjs_snapshot_index("DATA_SENSOR_SAMPLE", "sensor_data.samples", "index");
add_userjs_snapshot_index("DATA_GRID_OUTAGE_START", "grid_outage.start_time", "no");
add_userjs_snapshot_index("DATA_GRID_OUTAGE_DURATION", "grid_outage.duration", "no");
add_userjs_snapshot_index("DATA_GRID_REQUEST_START_TIME", "grid_request.start_time", "no");
add_userjs_snapshot_index("DATA_GRID_REQUEST_DURATION", "grid_request.duration", "no");
add_userjs_snapshot_index("DATA_GRID_REQUEST_SEVERITY", "grid_request.severity", "no");
add_userjs_snapshot_index("DATA_PROVIDER_SCHEDULE_TIMESTAMP_DAY", "provider_schedule.timestamp_day", "no");
add_userjs_snapshot_index("DATA_PROVIDER_SCHEDULE_RATE", "provider_schedule.rate", "index");
add_userjs_snapshot_index("DATA_PROVIDER_SCHEDULE_RATE_ARRAY", "provider_schedule.rate", "expose");
add_userjs_snapshot_index("DATA_PROVIDER_VARIANCE_TIMESTAMP_BASE", "provider_variance.timestamp_base", "no");
add_userjs_snapshot_index("DATA_PROVIDER_VARIANCE_START", "provider_variance.start", "index");
add_userjs_snapshot_index("DATA_PROVIDER_VARIANCE_DURATION", "provider_variance.duration", "index");
add_userjs_snapshot_index("DATA_PROVIDER_VARIANCE_RATE", "provider_variance.rate", "index");

userjs_snapshot_enum += '};\n'
userjs_snapshot_fetch += '\t}\n\treturn 0;\n}\n'

fs.writeFileSync('../userffi.h',
	header
	+ userjs_snapshot_enum
	+ userjs_snapshot_fetch);
