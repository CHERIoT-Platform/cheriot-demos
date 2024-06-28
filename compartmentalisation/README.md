Compartmentalisation example
============================

This demo uses compartments to protect the confidentiality of a secret and the availability of some business logic.
It uses a JavaScript interpreter both to run the business logic and to provide a simulation of an attacker who has managed to mount a code-reuse attack and gain arbitrary-code execution.
Code reuse attacks are very difficult on a CHERIoT system (given that you cannot bypass memory safety or coarse-grained control-flow integrity) but probably not impossible.

The demo has some embedded JavaScript that animates the LEDs on a board and will load new JavaScript bytecode over the UART.
The [`compile.sh`](compile.sh) script will compile and load a new JavaScript program.
This requires you to set the `CHERIOT_USB_DEVICE` environment variable to the board's UART.
For example:

```
$ export CHERIOT_USB_DEVICE=/dev/tty.USB3
$ ./compile hello.js
```

The JavaScript is compiled to bytecode with [Microvium](https://microvium.com).
If this is not installed, the `compile.sh` script will try to install it with `npm`.
If `npm` is not installed, you may need to install it manually.

Building and running the two demo configurations
------------------------------------------------

The demo will default to targeting the Sonata board.
It can also be used with the Arty A7, if you specify the correct target when building.
The firmware is built just like any other CHERIoT RTOS system:

```
$ xmake config --sdk={path to toolchain}
$ xmake
```

This will build *two* configurations.
In `sandboxed-javascript`, there are three compartments:

 - One that reads the JavaScript from the UART.
 - One that runs the JavaScript.
 - One that holds the secret.

In `unsandboxed-javascript`, these are all combined into a single compartment.
This demonstrates the security benefits of the compartmentalisation model: try the attacks on both.

If you are normally able to use `xmake run` for your device, you should be able to simply by specifying the target that you want to run:

```
$ xmake run [un]sandboxed-javascript
```

For the Arty A7 with the UART loader, you will need to first run the script that generates the VHX file:

```
$ ../cheriot-rtos/scripts/ibex-build-firmware.sh build/cheriot/cheriot/release/sandboxed-javascript
```

You will then need to send the `firmware/cpu0_iram.vhx` file to the UART.

The attacker
------------

The demo will print the address of the secret in memory when it starts.
Your task, as an attacker, is to try to leak this secret.
If this is too hard, try to crash the system such that it no longer runs (this *should* be possible, but is very hard).

It reads JavaScript programs (as compiled bytecode) from the UART and executes them.
The JavaScript has a set of FFI functions exposed that allow arbitrary capability manipulation, including pointer chasing.
This simulates an attacker building a code-reuse attack, with a lot more power than is normally possible on a CHERI system.
The attacker can:

 - Read the stack capability, global capability, and program counter capability into any of eight (virtual) register slots.
 - Read the permissions, bounds, address, and length of a capability in a register.
 - Read capabilities from memory via any of its eight capabilities registers into a register.
 - Read 32-bit words from memory via any of its eight capabilities registers into a register.
 - Set the address of a capability in any of the registers.
 - Write a capability from a register into memory via a capability in any of the registers.
 - Run arbitrary JavaScript to perform any of the above actions, intermixed with other computation.

This is the equivalent of the one of the most powerful weird machines that it is possible to create on a CHERI system from code reuse attacks.

You can find documentation on the full set of the functions exposed for an attacker to use in [`cheri.js`](cheri.js).

Provided scripts
----------------

There are three scripts provided as a starting point for trying to attack the system:

 - [`hello.js`](hello.js) is not malicious, it just prints some messages and animates the LEDs.
 - [`crash.js`](crash.js) tries to crash the system using a null-pointer dereference.
 - [`leak.js`](leak.js) tries to leak the secret.

The `leak.js` script hard codes the address of the secret, make sure that this matches the line printed by the demo at the start.
