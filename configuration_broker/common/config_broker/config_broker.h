// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "cdefs.h"
#include "compartment-macros.h"
#include "token.h"
#include <atomic>
#include <compartment.h>
#include <locks.hh>

/**
 * Internal representation of a token which allows an operation on
 * a configuration item.  This is put in a Capability sealed with one
 * of three different keys to define how it can be used.
 * Size and update_interval only used when setting the parser, but
 * keeping a common struct keeps the code a bit cleaner.
 */
struct ConfigToken
{
	size_t     size;           // Size of the item
	uint32_t   updateInterval; // Min interval in mS between updates
	const char Name[];         // Name of the configuration item
};

/**
 * Macros to create and use a Sealed Capability to read a config item
 */
#define DEFINE_READ_CONFIG_CAPABILITY(name)                                    \
                                                                               \
	DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(                                    \
	  struct {                                                                 \
		  size_t     notUsed1;                                                 \
		  uint32_t   notUsed2;                                                 \
		  const char Name[sizeof(name)];                                       \
	  },                                                                       \
	  config_broker,                                                           \
	  ReadConfigKey,                                                           \
	  __read_config_capability_##name,                                         \
	  0,                                                                       \
	  0,                                                                       \
	  name);

#define READ_CONFIG_CAPABILITY(name)                                           \
	STATIC_SEALED_VALUE(__read_config_capability_##name)

/**
 * Macros to create and use a Sealed Capability to write a config item
 */
#define DEFINE_WRITE_CONFIG_CAPABILITY(name)                                   \
                                                                               \
	DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(                                    \
	  struct {                                                                 \
		  size_t     notUsed1;                                                 \
		  uint32_t   notUsed2;                                                 \
		  const char Name[sizeof(name)];                                       \
	  },                                                                       \
	  config_broker,                                                           \
	  WriteConfigKey,                                                          \
	  __write_config_capability_##name,                                        \
	  0,                                                                       \
	  0,                                                                       \
	  name);

#define WRITE_CONFIG_CAPABILITY(name)                                          \
	STATIC_SEALED_VALUE(__write_config_capability_##name)

/**
 * Marcos to create and use a Sealed Capability to set the parser
 * and properties for a config item
 */
#define DEFINE_PARSER_CONFIG_CAPABILITY(name, Size, UpdateInterval)            \
                                                                               \
	DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(                                    \
	  struct {                                                                 \
		  size_t     size;                                                     \
		  uint32_t   update_interval;                                          \
		  const char Name[sizeof(name)];                                       \
	  },                                                                       \
	  config_broker,                                                           \
	  ParserConfigKey,                                                         \
	  __parser_config_capability_##name,                                       \
	  Size,                                                                    \
	  UpdateInterval,                                                          \
	  name);

#define PARSER_CONFIG_CAPABILITY(name)                                         \
	STATIC_SEALED_VALUE(__parser_config_capability_##name)

/**
 * External view of a configuration item.
 */
struct ConfigItem
{
	const char            *name;         // name
	uint32_t               version;      // version
	void                  *data;         // value
	std::atomic<uint32_t> *versionFutex; // Futex to wait for version change
};

/**
 * Set the value of a configuration item.
 *
 * Returns 0 for success.
 */
int __cheri_compartment("config_broker")
  set_config(SObj configWriteCapability, const void *src, size_t srcLength);

/**
 * Read the value of a configuration item.
 *
 * Always returns a ConfigItem with the following properties
 *
 *   id           - the name of item
 *   version      - the version returned in *data
 *   data         - a read only heap pointer the value.
 *                  May be null if the value has not yet been set.
 *                  The broker will free this allocation when the
 *                  value changes, so callers should make their own
 *                  claim on this.
 *   versionFutex - a pointer that can be used as a futex to wait
 *                  for version changes. This will be nullptr if
 *                  the caller does not have access to the item.
 */
ConfigItem __cheri_compartment("config_broker")
  get_config(SObj configReadCapability);

/**
 * Set the parser for a configuration item.
 *
 * Returns 0 on success
 *
 * The parser will be called whenever there is an attempt to
 * change the value of a config item, and should be a callback
 * to a sandbox compartment as the data is not trusted at this
 * point.
 */
int __cheri_compartment("config_broker")
  set_parser(SObj                 configValidateCapability,
             __cheri_callback int parse(const void *src, void *dst));
