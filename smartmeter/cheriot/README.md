# Disclaimer

This is a work-in-progress and is only ever intended as a technology demonstrator.
(That is, while hopefully interesting, this code should not find its way to production environments!)

# Multi-Tenant Smart Metering

This demo showcases how one might run multiple tenants with different operational concerns on a sensing platform.
Specifically, we consider the task of "smart metering" power consumption.
Our tenants are...

1. The grid controller, who wants to

   1. receive real-time information about the local grid,
   2. communicate scheduled outages,
   3. indicate the need for corrective actions (load shedding, load increase)

2. The provider, who wants to

   1. receive usage data (even retrospectively, in the case of, say, network outages),
   2. set the price schedule ahead of time, and
   3. announce spot price variances relative to the schedule

3. The service integrator (or, perhaps, end user), who wants to

   1. receive the price schedule, variances, and grid notifications, and
   2. make policy decisions about local resources (batteries, generation, loads) based on those.

We presume that the grid controller and provider logic is minimal and can be fixed at firmware build time,
while the integrator/user policy may be subject to faster change and should not require rebuilding firmware.
To that end, that policy is run on a lightweight JS interpreter.

## Compartmentalization and Information Flow

Internally, these tenants all exist within their own compartments ("grid", "provider", and two for the "user").
These make use of the (also compartmentalized!) MQTT/TLS/TCP/IP network stack to talk to the network.

Sensing and measurement itself is done by another compartment, "sensor".
This compartment makes its measurements available via a static shared object.
This object contains a small ring buffer of recent measurements and a time-stamp of the most recent.
(We presume the tenant compartments are sufficiently responsive that they will not miss updates.)

Similarly, information from the grid controller and providers are also exposed via shared objects.

These shared objects all contain futex words used as version identifiers and update-in-progress flags,
so it is possible to get stable reads of their contents and to wait for updates.

## Running

Most MQTT messages are composed of space-separated integers,
and so are generally straightforward to synthesize by hand on the command line or with other tooling.

At startup, the device will say something like

    housekeeping: MQTT Unique: S8MhTd2T

This 8-character sequence is used to distinguish multiple devices connected to the broker.
In practice, some of these topics (provider and grid) would be broadcast, with many devices subscribing.
For the purpose of this demo, all topics are "uniquified".
The examples below assume that this string is in the environment variable `METER_ID`.

### Subscribing to MQTT updates

The device publishes to `cheriot-smartmeter/p/update/${METER_ID}` (provider compartment)
and `cheriot-smartmeter/g/update/${METER_ID}` (grid compartment).

### Setting the provider rate schedule

The provider rate schedule contains a timestamp and 48 values thereafter.
Run something like the following to denote a time-of-use schedule for today
(that is, starting at the most recent past midnight)
and a flat rate schedule for tomorrow.

    mosquitto_pub -h test.mosquitto.org -p 1883 -q 1 -t cheriot-smartmeter/p/schedule/${METER_ID} -s << HERE
    $(date +%s --date="")
    7 7 7 7 7 7 7 7 12 12 12 10 10 10 10 12 12 12 7 7 7 7 7 7
    8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8 8
    HERE

### Setting a provider variance

To inform the device of a variance in pricing (say, a surplus of solar power is driving prices negative),
use something like the following, which indicates two variances:
- in the first, starting now and lasting for 5 minutes, power has negative cost
- in the second, starting in 5 minutes and lasting a further 10, power has zero cost

Run:

    mosquitto_pub -h test.mosquitto.org -p 1883 -q 1 -t cheriot-smartmeter/p/variance/${METER_ID} -s << HERE
    $(date +%s)
    0 300 -2
    300 600 0
    HERE

### Scheduling a grid outage

To inform the device of a pending loss of grid power, run something like the following,
which indicates an outage starting in 4 hours and lasting for 6 thereafter.

    mosquitto_pub -h test.mosquitto.org -p 1883 -q 1 -t cheriot-smartmeter/g/outage/${METER_ID} -s << HERE
    $(($(date +%s) + 4*3600)) $((6*3600))
    HERE

### Transmitting a grid request

To inform the device of a requset for more or less production, run something like the following,
which indicates an immediate request, lasting for 5 minutes, to reduce consumption (or increase generation).

    mosquitto_pub -h test.mosquitto.org -p 1883 -q 1 -t cheriot-smartmeter/g/request/${METER_ID} -s << HERE
    $(date +%s) 300 -1024
    HERE

### Pushing new JS Policy Code

The `js/compile.sh` script wraps fetching `microvium` (and its Node.js backend, in particular)
and using that to build bytecode files for use with the device.
Running, for example, `compile.sh ./sample_policy.js`, will result in a `sample_policy.js.mvm-bc` file that can be shipped over MQTT to the device with a command like

    mosquitto_pub -h test.mosquitto.org -p 1883 -q 1 -t cheriot-smartmeter/u/js/${METER_ID} -s < sample_policy.mvm-bc

### Speeding Up Simulated Time

Because this is a _demonstration_ and not a _real device_, it's helpful to be able to run faster than real time.
Towards that end, the policy evaluator understands a "timebase zero",
after which (simulated) time elapses at a positive multiple rate relative to real time.
While the policy code will still be evaluated in real time
(and, in particular, in response to sensor data that will continue to be published every 30 _wall-clock_ seconds),
the timestamp associated sensor reports, which drives most of the policy's idea of time,
will be artificially advanced.
Specifically, given a _wall-clock_ time T, a timebase zero Z, and a timebase rate R,
if T >= Z, then the time will instead be indicated as

    Z + R * (T - Z)

Thus, setting R to 10, for example, will cause the policy code's 30 _wall-clock_ second evaulation interval to
correspond to a 5 _simulated minute_ interval.

As with most everything, setting the timebase is done over MQTT.  Run something like

    mosquitto_pub -h test.mosquitto.org -p 1883 -q 1 -t cheriot-smartmeter/u/timebase/${METER_ID} -s << HERE
    $(date +%s --date="12 minutes ago") 10
    HERE

to set the timebase zero (Z) to 10 minutes ago and the rate (R) to 10.
(Making the timebase zero two _simulated hours_ ago.)

## Development

### Changing the JS FFI

If you change the information cached and exposed in the `userJS_snapshot` structure (defined in `user.hh`),
you'll want to update the constants in `js/ffi.js`, tweak `js/ffigen.js`, and run `microvium ffigen.js` (in the `js/` directory) to rebuild `userffi.h`.
