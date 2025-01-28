// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>

#include "crypto.h"


/**
 * Initaliser for libHyrogen 
 */
void __cheri_compartment("crypto") crypto_init()
{
	hydro_init();
}

/** 
 * Create a signature.
*/
int __cheri_compartment("crypto") crypto_sign(
                    uint8_t signature[hydro_sign_BYTES], 
                    const void *message, 
                    size_t messageLength,
                    const char    context[hydro_sign_CONTEXTBYTES],
                    const uint8_t secretKey[hydro_sign_SECRETKEYBYTES]) {

    return hydro_sign_create(signature, message, messageLength, context, secretKey);
}

/** 
 * Verify a signature in a payload.
 *
 */
int __cheri_compartment("crypto") crypto_verify_signature(
					const uint8_t signature[hydro_sign_BYTES],
                    const void *message, 
                    size_t messageLength,
                    const char context[hydro_sign_CONTEXTBYTES],
                    const uint8_t publicKey[hydro_sign_PUBLICKEYBYTES]) {

	return hydro_sign_verify(signature, message, messageLength, context, publicKey);
}

