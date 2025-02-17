-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT UART demo")
sdkdir = "../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sonata-1.1")

compartment("uart")
    -- memcpy
    add_deps("freestanding", "debug")
    add_files("uart.cc")

-- Firmware image for the example.
firmware("uart-demo")
    add_deps("uart")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "uart",
                priority = 1,
                entry_point = "uart_entry",
                stack_size = 0x800,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)
