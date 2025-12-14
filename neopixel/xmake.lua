-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Neopixel demo")
sdkdir = "../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sonata-1.1")

compartment("neopixel")
    -- memcpy
    add_deps("freestanding", "debug")
    add_files("neopixel.cc")

-- Firmware image for the example.
firmware("neopixel-demo")
    add_deps("neopixel")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "neopixel",
                priority = 1,
                entry_point = "neopixel_entry",
                stack_size = 0x800,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)
