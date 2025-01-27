-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


-- Parser for the System configuration
compartment("parser_system_config")
    set_default(false)
    add_includedirs("../../..")
    add_files("parser.cc")
