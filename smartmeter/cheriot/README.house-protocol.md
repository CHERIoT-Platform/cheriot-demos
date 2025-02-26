# What

This is a sketch of a protocol between the CHERIoT smartmeter demo components and
and external device representing the house's power distribution system.

This is based on Murali's proposal at
https://github.com/CHERIoT-Platform/cheriot-demos/pull/20#issuecomment-2670632663

# Messages

For ease of debugging, this protocol uses ASCII.
Messages are delimited with newlines (`0x0a`).
Messages are composed of space (`0x20`) separated tokens.
The tokens within a message specify a command (possibly with subcommands) and then its arguments.

## Smartmeter to House

(In the smartmeter, these messages are to be sent by the `user` compartment.)

### Set diagnostic LEDs

    led [on|off] [n]

Change diagnostic LEDs on the house side.
The optional `n` field is a bitmask of LEDs to be turned on or off;
if unspecified, an unspecified default is used.

### Control battery charge / discharge

    batteryControl N POWER

Indicates that battery N should be charged at a given POWER (in Watts).
If RATE is negative, the battery should be discharged.
The house should respond with a battery status message.

### Power draw

    powerTarget N POWER

Indicates the target POWER draw (in Watts) for consumer N.

## House to Smartmeter

(In the smartmeter, these messages are to be processed by the `sensor` compartment.)

### Battery status

    batteryStatus N CAPACITY CHARGE

Indicates that battery N has total CAPACITY (in Watt-hours) and is currently holding CHARGE (also Watt-hours).

This message should be sent every minute for each battery before its `powerSample` report, and
it should also be sent in response to `batteryControl` message.

### Power draw

    powerSample POWER

A report of total POWER (in Watts) draw for the past minute.
In a real system, this would be measured directly by the smartmeter.
Negative POWER values indicate that the house is feeding back to the grid.

This message should be sent every minute after all `batteryStatus` reports.
