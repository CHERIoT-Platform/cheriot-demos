// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Publish status based on the System config
void send_status(SObj mqtt, std::string topic, systemConfig::Config *config);

// Clear the status
void clear_status(SObj mqtt, std::string topic);
