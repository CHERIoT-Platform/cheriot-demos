-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


compartment("parser_init")
    set_default(false)
    add_files("parser_init.cc")
    
compartment("network_init")
    set_default(false)
    add_includedirs("../../../network-stack/include")
    add_files("network_init.cc")

