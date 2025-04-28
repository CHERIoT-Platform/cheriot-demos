import * as host from "./ffi.js"

export const HOUR_SECONDS = 3600;

export function schedule_index_for_time(time, base)
{
  var delta = time - base;

  if ((delta < 0) || (delta >= 2*24*HOUR_SECONDS /* two days' seconds */))
  {
    // Out of bounds
    return null;
  }

  return (delta / HOUR_SECONDS) | 0 /* truncated division */;
}

export function rate_variance(time, base, index)
{
  var variance_duration = host.read_from_snapshot(host.DATA_PROVIDER_VARIANCE_DURATION, index);

  if (variance_duration === 0)
  {
    return null;
  }

  var variance_start = base + host.read_from_snapshot(host.DATA_PROVIDER_VARIANCE_START, index);

  if ((time < variance_start) || (time > (variance_start + variance_duration)))
  {
    return null;
  }

  return host.read_from_snapshot(host.DATA_PROVIDER_VARIANCE_RATE, index);
}

export function rate_for_time(time, schedule_base)
{
  var variance_base = host.read_from_snapshot(host.DATA_PROVIDER_VARIANCE_TIMESTAMP_BASE, 0);

  if (variance_base !== 0)
  {
    // Do either of the variance slots cover this time?
    let v0 = rate_variance(time, variance_base, 0);
    if (v0 !== null)
    {
      // Yes, the 0th
      return v0;
    }

    let v1 = rate_variance(time, variance_base, 1);
    if (v1 !== null)
    {
      // Yes, the 1st
      return v1;
    }
  }

  // No; what's the scheduled rate?
  let schedule_index = schedule_index_for_time(time, schedule_base)
  if (schedule_index === null)
  {
    // Don't know; out of schedule bounds
    return null;
  }

  return host.read_from_snapshot(host.DATA_PROVIDER_SCHEDULE_RATE, schedule_index);
}

export function grid_request(time)
{
  var req_duration = host.read_from_snapshot(host.DATA_GRID_REQUEST_DURATION, 0);
  if (req_duration === 0)
  {
    return null;
  }

  var req_start = host.read_from_snapshot(host.DATA_GRID_REQUEST_START_TIME, 0);

  if ((time < req_start) || (time > (req_start + req_duration)))
  {
    return null;
  }

  return host.read_from_snapshot(host.DATA_GRID_REQUEST_SEVERITY, 0);
}

// XXX: This is a WIP as opposed to anything useful.
//
// It should: return null if there's no scheduled outage within the 48 hour
// provider-specified window or an outage in progress, or a timestamp at which
// to begin charging the battery, between now and the start of the outage.  If
// that's now, we should be charging the battery; if it's in the future, we
// should wait.
export function grid_outage(schedule_day_change, now)
{
  // Is there a grid planned outage in the near future?
  var grid_outage_duration = host.read_from_snapshot(host.DATA_GRID_OUTAGE_DURATION, 0);
  if (grid_outage_duration !== 0)
  {
    var grid_outage_start = host.read_from_snapshot(host.DATA_GRID_OUTAGE_START, 0);
    let grid_outage_end = grid_outage_start + grid_outage_end;

    if (grid_outage_end < now)
    {
      // Past outage
      return null;
    }
    else if (grid_outage_start < now)
    {
      // Ongoing outage; not much we can do about it now
      return null;
    }
    else if (grid_outage_start > (schedule_day_change + 2*24*HOUR_SECONDS))
    {
      // Far future outage, beyond the schedule's end
      return null;
    }
    else
    {
      /*
       * Outage within planning horizon.  We want to ensure that we have
       * power reserves when the time comes, so sweep through the schedule
       * looking for a good time to charge.
       *
       * TODO
       */
    }
  }
}
