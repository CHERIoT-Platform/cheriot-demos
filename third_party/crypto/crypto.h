// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <stddef.h>
#include <token.h>

#include "libhydrogen/hydrogen.h"

//
// A compartment wrapper for libhydrogen.  
//
// Note:  This needs to be a compartment rather that a shared library as libhydrogen 
// maintains some internal state in globals.
//
// At the moment it's a very simple wrapper for the few functions needed by
// the sonata configuration broker demo.
//

void __cheri_compartment("crypto") crypto_init();

// Public Key Signatures
int __cheri_compartment("crypto") crypto_sign(
                    uint8_t signature[hydro_sign_BYTES], 
                    const void *message, 
                    size_t messageLength,
                    const char    context[hydro_sign_CONTEXTBYTES],
                    const uint8_t secretKey[hydro_sign_SECRETKEYBYTES]);

int __cheri_compartment("crypto") crypto_verify_signature(
                    const uint8_t signature[hydro_sign_BYTES],
                    const void *message, 
                    size_t messageLength,
                    const char context[hydro_sign_CONTEXTBYTES],
                    const uint8_t publicKey[hydro_sign_PUBLICKEYBYTES]);


