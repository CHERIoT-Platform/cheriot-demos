// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <debug.hh>

using Debug = ConditionalDebug<true, "Rand">;

#include <platform-entropy.hh>

extern "C" uint32_t rand_32()
{
	static EntropySource rng;
	uint64_t r = rng();
	uint32_t r32 = r & 0xffffffff;
	return r32;
}