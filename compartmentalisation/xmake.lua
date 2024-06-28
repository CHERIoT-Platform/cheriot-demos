-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Hello World")
sdkdir = "../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/string"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/microvium"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("sonata")

compartment("js")
    add_defines("ENTRY_COMPARTMENT=\"js\"")
    add_files("js.cc")

compartment("js_sandbox")
    add_files("js_sandbox.cc")

compartment("secret")
    add_files("secret.cc")

compartment("insecure_js")
    add_defines("ENTRY_COMPARTMENT=\"insecure_js\"")
    add_defines("NO_COMPARTMENTS=1")
    add_files("js.cc")
    add_files("js_sandbox.cc")
    add_files("secret.cc")

-- Firmware image for the example.
firmware("sandboxed-javascript")
    add_deps("crt", "freestanding", "string", "microvium", "atomic_fixed", "debug")
    add_deps("js", "secret", "js_sandbox")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "js",
                priority = 1,
                entry_point = "run_tick",
                stack_size = 0xa00,
                trusted_stack_frames = 4
            },
            {
                compartment = "js",
                priority = 1,
                entry_point = "run",
                stack_size = 0xa00,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)

firmware("unsandboxed-javascript")
    add_deps("crt", "freestanding", "string", "microvium", "atomic_fixed", "debug")
    add_deps("insecure_js")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "insecure_js",
                priority = 1,
                entry_point = "run_tick",
                stack_size = 0x800,
                trusted_stack_frames = 4
            },
            {
                compartment = "insecure_js",
                priority = 1,
                entry_point = "run",
                stack_size = 0x800,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)
