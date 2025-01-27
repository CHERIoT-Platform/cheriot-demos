-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


-- Parser for the User LED configuration
compartment("parser_user_led")
    set_default(false)
    add_includedirs("../../..")
    add_files("parser.cc")
