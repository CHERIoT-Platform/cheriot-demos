-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


-- Provider Compartment
compartment("provider")
    add_includedirs("../..")
    add_files("mqtt_stub.cc")
    add_files("config.cc")

