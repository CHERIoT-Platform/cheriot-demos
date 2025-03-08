// Import everything from the environment
import * as host from "./ffi.js"
import * as sm   from "./smartmeter.js"

function run()
{
  var sts = host.read_from_snapshot(host.DATA_SENSOR_TIMESTAMP, 0);

  var schedule_day_change = host.read_from_snapshot(host.DATA_PROVIDER_SCHEDULE_TIMESTAMP_DAY, 0);
  var rate = sm.rate_for_time(sts, schedule_day_change);

  var req = sm.grid_request(sts);

  host.print("STS ", sts, " RATE ", rate, " REQ ", req);

  if ((rate < 0) && (req === null || req >= 0))
  {
    // Electricity is very cheap and the grid isn't asking us to not draw
    // TODO: charge the battery, turn on expensive things, &c.
    host.print("Do expensive stuff")
  }

  // Is there a grid planned outage in the near future?
  var grid_outage_duration = host.read_from_snapshot(host.DATA_GRID_OUTAGE_DURATION, 0);
  if (grid_outage_duration !== 0)
  {
    var grid_outage_start = host.read_from_snapshot(host.DATA_GRID_OUTAGE_START, 0);
    let grid_outage_end = grid_outage_start + grid_outage_end;

    if (grid_outage_end < sts)
    {
      // Past outage
    }
    else if (grid_outage_start > (schedule_day_change + 2*24*sm.HOUR_SECONDS))
    {
      // Far future outage, beyond the schedule's end
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

vmExport(1234, run);
