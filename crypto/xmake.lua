-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT


set_project("CHERIoT Compartmentalised Config")
sdkdir = path.absolute("../cheriot-rtos/sdk")
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/string"))

option("board")
    set_default("sonata")

option("CHERIOT")
  set_default(true)
  set_description("Define platform for libhydrogen")

compartment("crypto")
    add_deps("freestanding", "debug", "cxxrt")

    add_includedirs("./libhydrogen")
    add_files("test.cc")
    add_files("rand_32.cc")
    add_files("./libhydrogen/hydrogen.c")
    
    on_load(function(target)
        target:add('options', "CHERIOT")
        local Cheriot = get_config("CHERIOT")
        target:add("defines", "__cheriot__=" .. tostring(Cheriot))
    end)

-- Firmware image for the example.
firmware("config-broker-sonata")
    add_deps("freestanding", "debug", "string")

    -- compartments
    add_deps("crypto")
    
    
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "crypto",
                priority = 1,
                entry_point = "test",
                stack_size = 0x500,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)


