// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>

void __cheri_compartment("provider") provider_run();

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Crypto">;
using Error = ConditionalDebug<true, "Crypto">;

using CHERI::Capability;

#include "signature.h"
#include "../../../third_party/crypto/crypto.h"

// Keys used for signatures - for now compiled in but
// should be runtime supplied, such as read from the SD
// card. 
#include "./keys/config_pub_key.h"
#include "./keys/status_pri_key.h"
#include "./keys/status_pub_key.h"

// Size of the header in signed messages
#define SIGN_HEADER_BYTES hydro_sign_CONTEXTBYTES + hydro_sign_BYTES

namespace SIGNATURE {

/** 
 * Verify a signature in a payload.
 *
 * Packed as Context[8] + Signature[64] + Message
 */
Message verify_signature(const void *payload, size_t payloadLength)
{
	size_t messageOffset = SIGN_HEADER_BYTES;
	if (payloadLength <= messageOffset)
	{
		Error::log("Message too short to verify");
		return {nullptr, 0};
	}
	const char *context = (char *)payload;
	const uint8_t *signature = (uint8_t *)(context + hydro_sign_CONTEXTBYTES);
	void *message = (void *)(context + messageOffset);
	size_t messageLength = payloadLength - messageOffset;
	
	//
	// Stuff to look up key from context goes here
	//
	uint8_t *key = config_pub_key;
	
	if (crypto_verify_signature(signature, message, messageLength, context, key) == 0)
	{
		Debug::log("Signature Verified");
		Capability roMessage {message};
		roMessage.permissions() &= {CHERI::Permission::Load};
		roMessage.bounds() = messageLength;
		return {roMessage, messageLength};  
	}
	else {
		Error::log("Signature validation failed");
		return {nullptr, 0};
	}
}

/**
 * Create a signed message.
 *
 * Packed as Context[8] + Signature[64] + Message
 */
Message sign(SObj allocator, const char* context,
               const char *message, size_t messageLength) {

	Timeout t{100};
	size_t s_size = SIGN_HEADER_BYTES + messageLength;
	void *signed_message = heap_allocate(&t, allocator, s_size);
	if (!Capability{signed_message}.is_valid())
	{
		Debug::log("Failed to allocate {} of heap for signed message", s_size);
		return {nullptr, 0};
	}

	char *s_context = (char *)signed_message;
	uint8_t *s_signature = (uint8_t *)s_context + hydro_sign_CONTEXTBYTES;
	char *s_message = (char *)s_signature + hydro_sign_BYTES;

	strncpy(s_context, context, hydro_sign_CONTEXTBYTES);
	strncpy(s_message, message, messageLength);
	
	// Stuff to look up key from context goes here
	//
	uint8_t *key = status_pri_key;
	
	crypto_sign(s_signature, message, messageLength, context, status_pri_key);
	Debug::log("Signature generated");
	
	// Create a read only capabilty that is bound to the size of the message
	Capability<void>s_cap = {signed_message};
	s_cap.permissions() &= {CHERI::Permission::Load};
	s_cap.bounds() = s_size;

	return {s_cap, s_size};
}
 
} // namespace CRYPTO 