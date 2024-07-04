-- Copyright Configred Things and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


set_project("CHERIoT Compartmentalised Config")
sdkdir = "../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/string"))

option("board")
    set_default("ibex-safe-simulator")

-- Library for Mocked logger service
library("logger")
    set_default(false)
    add_files("logger/logger.cc")

-- Library for Mocked RGB LED config service
library("rgb_led")
    set_default(false)
    add_files("rgb_led/rgb_led.cc")       

-- Library for Mocked User LED config service
library("user_led")
    set_default(false)
    add_files("user_led/user_led.cc")       

-- Mocked MQTT Client
compartment("mqtt")
    add_files("mqtt_stub/mqtt.cc")

-- Configuration Broker
debugOption("config_broker")
compartment("config_broker")
    add_rules("cheriot.component-debug")
    add_files("config_broker/config_broker.cc")

-- Configuration Provider
compartment("provider")
    add_files("provider/provider.cc")

-- Configuration JSON parser sandbox
compartment("parser")
    add_files("parser/parser.cc")
    add_files("parser/core_json.cc")

-- Consumers
compartment("consumer1")
    add_files("consumers/consumer1.cc")
compartment("consumer2")
    add_files("consumers/consumer2.cc")

-- Firmware image for the example.
firmware("config-broker-ibex-sim")
    add_deps("freestanding", "debug", "string")
    -- libraries
    add_deps("logger")
    add_deps("rgb_led")
    add_deps("user_led")
    -- compartments
    add_deps("mqtt")
    add_deps("provider")
    add_deps("config_broker")
    add_deps("parser")
    add_deps("consumer1")
    add_deps("consumer2")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                -- Thread to set config values.
                -- Starts and loops in the mqtt
                -- compartment.
                compartment = "mqtt",
                priority = 1,
                entry_point = "init",
                stack_size = 0x700,
                trusted_stack_frames = 8
            },
            {
                -- Thread to consume config values.
                -- Starts and loops in consumer1
                compartment = "consumer1",
                priority = 2,
                entry_point = "init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
            {
                -- Thread to consume config values.
                -- Starts and loops in consumer2
                compartment = "consumer2",
                priority = 2,
                entry_point = "init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
        }, {expand = false})
    end)


