-- SPDX-License-Identifier: MIT

set_project("external-interrupts")
sdkdir = "../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sonata-1.3")

compartment("entry")
    add_deps("freestanding", "debug")
    add_files("entry.cc")

firmware("external-interrupts-fw")
    add_deps("entry")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "entry",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x200,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)
