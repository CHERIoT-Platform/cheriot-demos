// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <multiwaiter.h>
#include <token.h>

namespace ConfigConsumer
{

	/**
	 * Defines a handler for a configuration item.
	 */
	struct ConfigItem
	{
		SObj capability;        // Sealed Read Capability
		int (*handler)(void *); // Handler to call
		uint32_t               version;
		std::atomic<uint32_t> *versionFutex;
	};

	// Method call by a thread to wait for and process updates
	// to configurtion items
	void __cheri_libcall run(ConfigItem configItems[], size_t numOfItems, uint16_t maxTimeouts=0);

} // namespace ConfigConsumer