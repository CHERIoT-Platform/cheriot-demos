// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <multiwaiter.h>
#include <thread.h>
#include <token.h>

#include "../config_broker/config_broker.h"
#include "config_consumer.h"

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<false, "ConfigConsumer">;

namespace ConfigConsumer
{

	/**
	 * Thread entry point.  The waits for changes to one
	 * or more configuration values and then calls the
	 * appropriate handler.
	 */
	void __cheri_libcall run(ConfigItem configItems[],
	                         size_t     numOfItems,
	                         uint16_t   maxTimeouts)
	{
		// Just for the demo keep track of the number to timeouts to give
		// a clean exit
		uint16_t num_timeouts = 0;

		// Create the multi waiter
		struct MultiWaiter *mw = nullptr;
		Timeout             t1{MS_TO_TICKS(1000)};
		multiwaiter_create(&t1, MALLOC_CAPABILITY, &mw, numOfItems);
		if (mw == nullptr)
		{
			Debug::log("thread {} failed to create multiwaiter",
			           thread_id_get());
			return;
		}

		// Create a set of wait events
		struct EventWaiterSource events[numOfItems];

		// We need to do an initial read of each item to at
		// least get the version futex to wait on (there may not
		// be a value yet. So set up the initial value of the
		// events as if each item has been updated.
		for (auto i = 0; i < numOfItems; i++)
		{
			events[i] = {nullptr, EventWaiterFutex, 1};
		}

		// Loop waiting for config changes.  The flow in here
		// is read and then wait for change to account for the
		// need for an initial read.
		while (true)
		{
			// find out which value changed
			for (auto i = 0; i < numOfItems; i++)
			{
				if (events[i].value == 1)
				{
					Debug::log("Item {} of {} changed", i, numOfItems);

					auto c    = &configItems[i];
					auto item = get_config(c->capability);

					if (item.versionFutex == nullptr)
					{
						Debug::log("thread {} failed to get {}",
						           thread_id_get(),
						           c->capability);
						continue;
					}

					c->version      = item.version;
					c->versionFutex = item.versionFutex;

					Debug::log("thread {} got version:{} of {}",
					           thread_id_get(),
					           c->version,
					           item.name);

					if (item.data == nullptr)
					{
						Debug::log("No data yet for {}", item.name);
						continue;
					}

					// Make a fast claim on the data now, the handler
					// can decide if it wants to make a full claim
					Timeout t{5000};
					int     claimed = heap_claim_fast(&t, item.data, nullptr);
					if (claimed != 0)
					{
						Debug::log(
						  "thread {} failed fast claim for {} {} with {}",
						  thread_id_get(),
						  item.name,
						  item.data,
						  claimed);
						continue;
					}

					// Call the handler for this item
					Debug::log("Calling handler for {}", item.name);
					if (c->handler(item.data) != 0)
					{
						Debug::log("thread {} handler failed for {} {}",
						           thread_id_get(),
						           item.name,
						           item.data);
					}
					Debug::log("After handler for {}", item.name);
				}
			}

			// Reset the event waiter
			for (auto i = 0; i < numOfItems; i++)
			{
				events[i] = {configItems[i].versionFutex,
				             EventWaiterFutex,
				             configItems[i].version};
			}

			// Wait for a version to change
			Debug::log("Waiting for new events");
			Timeout t{MS_TO_TICKS(10000)};
			if (multiwaiter_wait(&t, mw, events, numOfItems) != 0)
			{
				num_timeouts++;
				Debug::log(
				  "thread {} wait timeout {}", thread_id_get(), num_timeouts);
				// For the demo exit the thread when we stop getting updates
				if (maxTimeouts > 0 && num_timeouts >= maxTimeouts)
				{
					break;
				}
			}
			else
			{
				num_timeouts = 0;
			}
		}
	}

} // namespace ConfigConsumer