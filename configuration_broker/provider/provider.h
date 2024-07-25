// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>

/**
 * Update a configuration item using the JSON string
 * received on a particular topic. With a real MQTT
 * client this would be the callback registered when
 * subscribing to the topic.
 */
int __cheri_compartment("provider")
  updateConfig(const char *topic, const char *message);
