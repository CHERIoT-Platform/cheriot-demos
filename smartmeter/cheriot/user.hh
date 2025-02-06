#pragma once

#include "common.hh"

/*
 * The most recent stable snapshot of all the data sources we're
 * monitoring.
 *
 * Updated by user compartment before entering userJS to respond.
 */
struct userjs_snapshot
{
	struct sensor_data_payload sensor_data;

	struct grid_planned_outage_payload grid_outage;
	struct grid_request_payload        grid_request;

	struct provider_schedule_payload provider_schedule;
	struct provider_variance_payload provider_variance;
};
static_assert(sizeof(struct userjs_snapshot) == 168,
              "userjs_snapshot object bad size; update xmake.lua");
