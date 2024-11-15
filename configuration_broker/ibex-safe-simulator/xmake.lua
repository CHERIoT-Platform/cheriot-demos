-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


set_project("CHERIoT Compartmentalised Config")
sdkdir = "../../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/string"))

option("board")
    set_default("ibex-safe-simulator")

-- Common libraries and compartments
includes("../common/json_parser")
includes("../common/config_broker") 
includes("../common/config_consumer")

-- Initialisation compartment
includes("init")

-- Mocked MQTT Client to provide configurtaion
includes("provider")

-- Configuration JSON parser sandboxes
includes("../config/parsers/rgb_led")
includes("../config/parsers/user_led")
includes("../config/parsers/logger")

-- Consumers
includes("consumers")

-- Firmware image for the example.
firmware("config-broker-ibex-sim")
    add_deps("freestanding", "debug", "string")

    -- libraries
    add_deps("json_parser")
    add_deps("config_consumer")
    
    -- compartments
    add_deps("parser_init")
    add_deps("provider")
    add_deps("config_broker")
    add_deps("parser_logger")
    add_deps("parser_rgb_led")
    add_deps("parser_user_led")
    add_deps("consumer1")
    add_deps("consumer2")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                -- Thread to receive config values.
                -- Starts in the parser_init compartment
                -- and then loops in the provider compartment.
                compartment = "parser_init",
                priority = 1,
                entry_point = "parser_init",
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


