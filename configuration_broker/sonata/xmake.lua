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
includes(path.join(sdkdir, "lib"))
includes("../../network-stack/lib")

-- Common libraries and compartments
includes("../common/json_parser")
includes("../common/config_broker") 
includes("../common/config_consumer")

-- init compartments
includes("init")

-- System configuration
includes("system_config")

-- MQTT Client to provide configurtaion
includes("provider")

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
    add_deps("parser_init")
    add_deps("network_init")

    add_deps("system_config")

    add_deps("provider")

    add_deps("config_broker")

    add_deps("parser_system_config")
    add_deps("parser_rgb_led")
    add_deps("parser_user_led")
    
    add_deps("consumers")
    
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                -- Thread to set system configuration
                -- from switches.  Also acts as the
                -- init service for the parsers.
                compartment = "parser_init",
                priority = 2,
                entry_point = "parser_init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
            {
                -- Thread to Get data from MQTT
                -- Also acts as the init service
                -- for networking
                compartment = "network_init",
                priority = 2,
                entry_point = "network_init",
                stack_size = 8160,
                trusted_stack_frames = 10
            },
            {
                -- Thread consume safe config
                -- updates
                compartment = "consumers",
                priority = 1,
                entry_point = "init",
                stack_size = 0x500,
                trusted_stack_frames = 8
            },
            {
                -- TCP/IP stack thread.
                compartment = "TCPIP",
                priority = 2,
                entry_point = "ip_thread_entry",
                stack_size = 0x1000,
                trusted_stack_frames = 5
            },
            {
                -- Firewall thread, handles incoming packets as they arrive.
                compartment = "Firewall",
                -- Higher priority, this will be back-pressured by the message
                -- queue if the network stack can't keep up, but we want
                -- packets to arrive immediately.
                priority = 4,
                entry_point = "ethernet_run_driver",
                stack_size = 0x1000,
                trusted_stack_frames = 5
            }
        }, {expand = false})
    end)


