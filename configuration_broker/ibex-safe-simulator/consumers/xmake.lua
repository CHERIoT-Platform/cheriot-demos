-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


-- Consumer compartments 
compartment("consumer1")
    on_load(function(target)
        target:add("defines", "MAX_CONFIG_TIMEOUTS=2")
    end)
    add_includedirs("../..")
    add_files("consumer1.cc")

compartment("consumer2")
    on_load(function(target)
        target:add("defines", "MAX_CONFIG_TIMEOUTS=2")
    end)
    add_includedirs("../..")
    add_files("consumer2.cc")

