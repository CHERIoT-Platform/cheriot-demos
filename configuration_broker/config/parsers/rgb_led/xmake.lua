-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


-- Parser for the RGB LED configuration
compartment("parser_rgb_led")
    set_default(false)
    add_includedirs("../../..")
    add_files("parser.cc")
