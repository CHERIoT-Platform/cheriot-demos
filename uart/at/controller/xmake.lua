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
    add_files("controller.cc")

compartment("display")
    add_deps("freestanding", "string", "debug")
    add_files("display.cc")
    add_files("../../../third_party/display_drivers/core/lcd_base.c")
    add_files("../../../third_party/display_drivers/core/lucida_console_12pt.c")
    add_files("../../../third_party/display_drivers/st7735/lcd_st7735.c", {defines = "CHERIOT_NO_AMBIENT_MALLOC"})
    add_files("../../../third_party/display_drivers/lcd_sonata_0.2.cc")
    add_files("../../../third_party/display_drivers/lcd_sonata_1.0.cc")

-- Firmware image for the example.
firmware("controller-demo")
    add_deps("controller", "display")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "controller",
                priority = 1,
                entry_point = "entry_point",
                stack_size = 0x1200,
                trusted_stack_frames = 4
            },
            {
                compartment = "display",
                priority = 2,
                entry_point = "entry_point",
                stack_size = 0x1000,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)
