-- Copyright 3bian Limited and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT AT controller demo")
sdkdir = "../../../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sonata-1.1")

compartment("controller")
    add_deps("freestanding", "string", "debug")
    add_files("main.cc")

-- Firmware image for the example.
firmware("controller-demo")
    add_deps("controller")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "controller",
                priority = 1,
                entry_point = "entry_point",
                stack_size = 0x800,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)
