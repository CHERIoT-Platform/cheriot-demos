#pragma once
#include <compartment-macros.h>
#include <stdlib.h>

/**
 * Returns the (per-run) encryption key.  This should be called only by the
 * display compartment, which includes it in the QR code.
 */
__cheri_compartment("crypto") const uint8_t *key_bytes();

/**
 * Decrypt a message containing three colours.  Returns an error code on
 * failure or 0 on success.  If decryption was successful, the result is
 * written into `colour`.
 */
__cheri_compartment("crypto") int decrypt(uint8_t        colour[3],
                                          const uint8_t *payload,
                                          size_t         payloadLength);

/**
 * Returns the topic suffix for inclusion in the QR code.
 */
__cheri_compartment("hugh_network") const char *topic_suffix();

/**
 * Length of the topic suffix.
 */
static constexpr size_t TopicSuffixLength = 8;

/**
 * Length of the encryption key.
 */
static constexpr size_t KeyLength = 32;
