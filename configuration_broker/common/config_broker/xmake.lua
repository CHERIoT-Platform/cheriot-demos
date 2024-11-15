--Copyright Configured Things Ltd and CHERIoT Contributors.
--SPDX - License -Identifier : MIT

-- Configuration Broker 
debugOption("config_broker")
compartment("config_broker")
    set_default(false)
    add_rules("cheriot.component-debug")
    add_files("config_broker.cc")
