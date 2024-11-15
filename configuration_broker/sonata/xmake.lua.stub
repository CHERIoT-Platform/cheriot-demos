-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


set_project("CHERIoT Compartmentalised Config")
sdkdir = path.absolute("../../cheriot-rtos/sdk")
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/string"))

option("board")
    set_default("sonata")

-- network stack
--includes(path.join(sdkdir, "lib"))
--includes("../../network-stack/lib")

-- Common libraries and compartments
includes("../common/json_parser")
includes("../common/config_broker") 
includes("../common/config_consumer")

-- System Configuration compartment
includes("system_config")

-- MQTT Client to provide configurtaion
--includes("mqtt")
includes("mqtt_stub")

-- Parser init compartment
compartment("parser_init")
    add_files("parser_init/parser_init.cc")

-- Configuration JSON parser sandboxes
includes("../config/parsers/rgb_led")
includes("../config/parsers/user_led")
includes("../config/parsers/system_config")

-- Consumers
includes("consumers")

-- Firmware image for the example.
firmware("config-broker-sonata")
    add_deps("freestanding", "debug", "string")

    -- libraries
    add_deps("json_parser")
    add_deps("config_consumer")
    
    -- compartments
    add_deps("mqtt")
    add_deps("config_broker")

    add_deps("system_config")

    add_deps("parser_init")
    add_deps("parser_system_config")
    add_deps("parser_rgb_led")
    add_deps("parser_user_led")
    
    add_deps("consumers")
    
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                -- Thread to set system config.
                -- Starts in the parser_init compartment
                -- and then and loops in system_config
                compartment = "parser_init",
                priority = 4,
                entry_point = "parser_init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
            {
                -- Thread to consume config values
                compartment = "consumers",
                priority = 2,
                entry_point = "init",
                stack_size = 0x600,
                trusted_stack_frames = 4
            },
            {
                -- Thread to Get data from MQTT
                compartment = "mqtt",
                priority = 1,
                entry_point = "mqtt_init",
                stack_size = 8160,
                trusted_stack_frames = 8
            }
        }, {expand = false})
    end)


