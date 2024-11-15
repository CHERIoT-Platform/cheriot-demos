// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <debug.hh>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Hydro test">;

#define CONTEXT "Examples"
#define MESSAGE "Arbitrary data to hash"
#define MESSAGE_LEN 22

#include "hydrogen.h"

void sign() {
	hydro_sign_keypair key_pair;
	hydro_sign_keygen(&key_pair);

	uint8_t signature[hydro_sign_BYTES];

	/* Sign the message using the secret key */
	hydro_sign_create(signature, MESSAGE, MESSAGE_LEN, CONTEXT, key_pair.sk);

	/* Verify the signature using the public key */
	if (hydro_sign_verify(signature, MESSAGE, MESSAGE_LEN, CONTEXT, key_pair.pk) == 0) {
		Debug::log("Verified");
	}
	else
	{
		Debug::log("Verify failed");
	}
	
}

/// Thread entry point.
void __cheri_compartment("crypto") test()
{

	hydro_init();	
	sign();
}
