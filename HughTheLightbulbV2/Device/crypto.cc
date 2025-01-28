// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "interface.hh"
#include <debug.hh>
#include <errno.h>
#include <fail-simulator-on-error.h>
#include <platform-entropy.hh>

#include "../../third_party/crypto/libhydrogen/hydrogen.h"

using CHERI::Capability;

using Debug = ConditionalDebug<true, "Hugh the lightbulb (crypto)">;

namespace
{

	/**
	 * Encryption context.  Currently unused.  If it is used, the mobile app
	 * must also be updated.
	 *
	 * An implementation could be made robust against replay attacks by using
	 * this as a counter and publishing it via MQTT periodically.
	 */
	const char context[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	// Make sure that the value in the header is the value libhydrogen expects.
	static_assert(KeyLength == hydro_secretbox_KEYBYTES);

	/**
	 * Lazily generate a random key the first time it's accessed and then
	 * return the same key from subsequent calls.
	 */
	const std::array<uint8_t, hydro_secretbox_KEYBYTES> &key()
	{
		// Use C++ thread-safe initialisation to make sure libhydrogen is
		// initialised and then generate the key.
		static std::array<uint8_t, hydro_secretbox_KEYBYTES> key = []() {
			hydro_init();
			std::array<uint8_t, hydro_secretbox_KEYBYTES> tmp;
			hydro_secretbox_keygen(tmp.data());
			return tmp;
		}();
		return key;
	}
} // namespace

__cheri_compartment("crypto") const uint8_t *key_bytes()
{
	CHERI::Capability bytes = key().data();
	bytes.permissions() &= {CHERI::Permission::Load};
	return bytes;
}

__cheri_compartment("crypto") int decrypt(uint8_t        colour[3],
                                          const uint8_t *payload,
                                          size_t         payloadLength)
{
	if (payloadLength != (3 + hydro_secretbox_HEADERBYTES))
	{
		Debug::log("Payload is not valid for a colour ({} bytes, expected {})",
		           payloadLength, (3U + hydro_secretbox_HEADERBYTES));
		return -EINVAL;
	}
	int err = hydro_secretbox_decrypt(colour,
	                                  static_cast<const uint8_t *>(payload),
	                                  payloadLength,
	                                  0,
	                                  context,
	                                  key().data());
	if (err != 0)
	{
		Debug::log("Decryption failed: {}", err);
	}
	return err;
}
