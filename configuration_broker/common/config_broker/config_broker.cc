// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cheri.hh>
#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <errno.h>
#include <fail-simulator-on-error.h>
#include <futex.h>
#include <locks.hh>
#include <string.h>
#include <thread.h>

#include "config_broker.h"

// Import some useful things from the CHERI namespace.
using namespace CHERI;

/// Debugging can be enable with "xmake --config --debug-config_broker=true"
using Debug = ConditionalDebug<DEBUG_CONFIG_BROKER, "Config Broker">;

namespace
{
	/// Internal view of a Config Item.
	struct InternalConfigitem
	{
		std::atomic<uint32_t>     version;  // version - used as a futex
		void                     *data;     // current value
		const char               *name;     // name
		size_t                    size;     // size of the created object
		uint32_t                  minTicks; // Min system ticks between updates
		uint64_t                  nextUpdate; // Time of next valid update
		FlagLockPriorityInherited lock; // lock to prevent concurrent changes
		int __cheri_callback (*parser)(const void *src, void *dst);
		InternalConfigitem *next;
	};

	/**
	 * Set of config data items.  At scale we'd need to hold this in
	 * std::map so we can do an easy lookup by name, but for now a very
	 * simple link list is fine as the only operations we need are to
	 * add a new item and search for an item (there is no concept of
	 * deleting an item). The total number of items is limited by the
	 * the set of static sealed capabilities.
	 */
	InternalConfigitem *configData = nullptr;

/*
 * Keys for unsealing the various types of operation
 */
#define CONFIG_WRITE STATIC_SEALING_TYPE(WriteConfigKey)
#define CONFIG_READ STATIC_SEALING_TYPE(ReadConfigKey)
#define CONFIG_PARSER STATIC_SEALING_TYPE(ParserConfigKey)

	/**
	 * Unseal a config capability.
	 */
	ConfigToken *config_capability_unseal(SObj sealedCap, SKey key)
	{
		ConfigToken *token = token_unseal(key, Sealed<ConfigToken>{sealedCap});

		if (token == nullptr)
		{
			Debug::log("invalid config capability {}", sealedCap);
			return nullptr;
		}

		Debug::log("Unsealed - Name: {} size: {} interval: {}",
		           token->Name,
		           token->size,
		           token->updateInterval);

		return token;
	}

	/**
	 * Find a Config by name.  If it doesn't already exist
	 * create one.  Use a LockGuard to protect against two
	 * threads trying to create the same item.
	 */

	InternalConfigitem *find_or_create_config(ConfigToken *token)
	{
		static FlagLock lockFindOrCreate;
		LockGuard       g{lockFindOrCreate};

		for (auto c = configData; c != nullptr; c = c->next)
		{
			if (strcmp(c->name, token->Name) == 0)
			{
				return c;
			}
		}

		// Allocate a Config object
		InternalConfigitem *c = new (std::nothrow) InternalConfigitem();

		if (c != nullptr)
		{
			// Use the Name from the token that triggered the creation
			// as the name value, since sealed objects are guaranteed not
			// to be deallocated.
			c->name    = token->Name;
			c->version = 0;
			c->data    = nullptr;

			// Insert it at the start of the list
			c->next    = configData;
			configData = c;
		}

		return c;
	};

} // namespace

/**
 * Set a new value for the configuration item described by
 * the capability.
 */
int __cheri_compartment("config_broker")
  set_config(SObj sealedCap, const void *src, size_t srcLength)
{
	Debug::log(
	  "thread {} Set config called for {}", thread_id_get(), sealedCap);

	// Check that we've been given a valid capability
	ConfigToken *token = config_capability_unseal(sealedCap, CONFIG_WRITE);
	if (token == nullptr)
	{
		Debug::log("Invalid set config capability: {}", sealedCap);
		return -EPERM;
	}

	// Find or create a config structure
	InternalConfigitem *c = find_or_create_config(token);
	if (c == nullptr)
	{
		Debug::log("Failed to create item {}", token->Name);
		return -ENOMEM;
	}

	// Guard against concurrent updates to this item
	LockGuard g{c->lock};

	// Check we have a parser
	if (c->parser == nullptr)
	{
		Debug::log("Parser not defined for {}", token->Name);
		return -ENODEV;
	}

	// Check rate limiting
	auto     system_tick = thread_systemtick_get();
	uint64_t tick =
	  (static_cast<uint64_t>(system_tick.hi) << 32) + system_tick.lo;
	if ((c->nextUpdate > 0) && (tick < c->nextUpdate))
	{
		Debug::log(
		  "Rate limit exceeded: tick {} next update {}", tick, c->nextUpdate);
		return -EBUSY;
	}
	c->nextUpdate = tick + c->minTicks;

	// Allocate heap space for the new value
	auto newData = malloc(c->size);
	if (newData == nullptr)
	{
		Debug::log("Failed to allocate space for {}", token->Name);
		return -ENOMEM;
	}

	// Create a write only Capability to pass to the parser so
	// that it can't capture or read from it. This also clears
	// the Load/Store Capability (MC) permission so the config data
	// can only hold simple values.
	CHERI::Capability woNewData{newData};
	woNewData.permissions() &= {CHERI::Permission::Store};

	// Create a read only Capability of the source data to pass
	// to the parser so that it can't capture or change it. This
	// also clears the Load/Store Capability (MC) permission which
	// prevents capabilities being embedded in the source data.
	//
	// Set the bounds to the length of the source both to constrain
	// it and to avoid having to pass it in as a separate value.
	//
	CHERI::Capability roSrc{src};
	roSrc.permissions() &= {CHERI::Permission::Load};
	roSrc.bounds() = srcLength;

	// Call the parser
	if (c->parser(roSrc, woNewData) != 0)
	{
		Debug::log("Parser failed for {}", token->Name);
		free(newData);
		return -EINVAL;
	}

	// Neither we nor the subscribers need to be able to update the
	// value, so just track through a readOnly capability
	CHERI::Capability roData{newData};
	roData.permissions() &=
	  roData.permissions().without(CHERI::Permission::Store) &
	  roData.permissions().without(CHERI::Permission::LoadStoreCapability);

	// Keep track of the old value so we can free it
	auto oldData = c->data;

	// updated it
	c->data = roData;
	c->version++;
	Debug::log("Data version {} set to {}", c->version.load(), c->data);

	// Notify anyone waiting for the version to change.  Doing this before
	// we free the old value reduces the risk of them using the old value
	// after we free it, even though they should have their own claim.
	Debug::log("Waking subscribers {}", c->version.load());
	c->version.notify_all();

	// Free the old data value.  Any subscribers that received it should
	// have their own claim on it if needed
	if (oldData)
	{
		free(oldData);
	}

	return 0;
}

/**
 * Get the current value of a Configuration item.  The data
 * member will be nullptr if the item has not yet been set.
 */
ConfigItem __cheri_compartment("config_broker") get_config(SObj sealedCap)
{
	// Object to return.  Stack is initialised to zeros
	ConfigItem result;

	Debug::log(
	  "thread {} get_config called with {}", thread_id_get(), sealedCap);

	// Get the calling compartments name from
	// its sealed capability
	ConfigToken *token = config_capability_unseal(sealedCap, CONFIG_READ);

	if (token == nullptr)
	{
		// Didn't get passed a valid Read Capability
		Debug::log("Invalid read config capability {}", sealedCap);
		return result;
	}

	auto c = find_or_create_config(token);
	if (c == nullptr)
	{
		Debug::log("Failed to create item {}", token->Name);
		return result;
	}

	//
	// lock to prevent concurrent set/get on the same
	// item
	LockGuard g{c->lock};

	//
	// Populate the return object.
	//

	// Name is already read only as it came from the static
	// static capability
	result.name = c->name;

	// Provide the version value at this point in time
	result.version = c->version.load();

	// Data is already a read only pointer
	result.data = c->data;

	// Create a readonly pointer to the version that can
	// be used a futex for version changes.
	CHERI::Capability roFutex{&c->version};
	roFutex.permissions() &=
	  roFutex.permissions().without(CHERI::Permission::Store);
	result.versionFutex = roFutex;

	return result;
}

/**
 * Set the parser for a config item.
 */
int __cheri_compartment("config_broker")
  set_parser(SObj                 sealedCap,
             __cheri_callback int parser(const void *src, void *dst))
{
	Debug::log(
	  "thread {} set parser called with {}", thread_id_get(), sealedCap);

	ConfigToken *token = config_capability_unseal(sealedCap, CONFIG_PARSER);

	if (token == nullptr)
	{
		// Didn't get passed a valid Parser Capability
		Debug::log("Invalid set parser capability {}", sealedCap);
		return -1;
	}

	auto c = find_or_create_config(token);
	if (c == nullptr)
	{
		Debug::log("Failed to create item {}", token->Name);
		return -1;
	}

	c->size     = token->size;
	c->minTicks = MS_TO_TICKS(token->updateInterval);
	c->parser   = parser;

	return 0;
}