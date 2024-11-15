-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


-- Parser for the logger configuration
compartment("parser_logger")
    set_default(false)
    add_includedirs("../../..")
    add_files("parser.cc")
