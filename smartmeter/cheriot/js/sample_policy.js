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

  if (req !== null)
  {
    if (req < 0)
    {
      // The grid is asking us to draw less; respond by discharging the battery
      // Could also do things like turn off the heat pump
      host.print("Grid request less consumption");
      host.uart_write("battery -1");
    }
    else
    {
      // The grid is asking us to draw more; respond by charging the battery
      // Could also do things like turn on the heat pump
      host.print("Grid request more consumption");
      host.uart_write("battery 1");
    }

    // Grid requests take precedence over the rest of the policy
    return;
  }

  if (rate < 0)
  {
    // Electricity is very cheap and the grid isn't asking us to not draw
    host.print("Power is free");
    host.uart_write("battery 1");
  }
}

vmExport(1234, run);
