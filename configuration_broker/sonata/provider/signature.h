// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <stddef.h>
#include <token.h>

// Helper functions
namespace SIGNATURE {

struct Message {
    CHERI::Capability<void> data;
    size_t length;
};

// Verify a signature and return the embedded message 
Message verify_signature(const void *payload, size_t payloadLength);

// Sign a message and return new message which includes a capabilty to a heap allocated buffer
// containing the message and the signature
Message sign(SObj allocator, const char* context,
               const char *message, size_t messageLength);

} // namespace CRYPTO

