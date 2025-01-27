-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

option("system-id")
    set_default("random")
    set_description("Set the system id in the MQTT topic. Default is a random string")

-- System Config Compartment
compartment("system_config")
    set_default(false)
    add_includedirs("../..")
    add_deps("unwind_error_handler")
    add_files("system_config.cc")

    on_load(function(target)
        target:add('options', "system-id")
        local SYSTEM_ID = get_config("system-id")
        target:add("defines", "SYSTEM_ID=\"" .. tostring(SYSTEM_ID) .. "\"")
    end)
    
