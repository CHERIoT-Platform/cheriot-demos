-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


-- Provider Compartment
compartment("provider")

    add_includedirs("../../../network-stack/include")
    add_deps("freestanding", "TCPIP", "NetAPI", "TLS", "Firewall", "SNTP", "DNS", "MQTT", "time_helpers", "debug", "stdio")
    add_deps("unwind_error_handler")
    
    add_includedirs("../..")
    add_files("mqtt.cc")
    add_files("config.cc")
    add_files("status.cc")

    on_load(function(target)
        target:add('options', "IPv6")
        local IPv6 = get_config("IPv6")
        target:add("defines", "CHERIOT_RTOS_OPTION_IPv6=" .. tostring(IPv6))
    end)

