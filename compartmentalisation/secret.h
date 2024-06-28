#pragma once
#include <compartment.h>
#include <stdint.h>

#ifdef NO_COMPARTMENTS
#	define COMPARTMENT(x)
#else
#	define COMPARTMENT(x) __cheri_compartment(x)
#endif

/**
 * Set the secret the first time.
 *
 * This blocks until there is a character available on the UART.  This is
 * *not* a good way of getting entropy, but it's the only one that we have
 * in the simulator.
 */
void COMPARTMENT("secret") set_secret();

/**
 * Report the current value of the secret and pick a new one.
 */
void COMPARTMENT("secret") check_secret(int32_t guess);

/**
 * Run the JavaScript code, in the sandbox.
 */
int COMPARTMENT("js_sandbox") load_javascript(uint8_t *bytecode, size_t size);

int COMPARTMENT("js_sandbox") tick();
void COMPARTMENT("js_sandbox") kill();
