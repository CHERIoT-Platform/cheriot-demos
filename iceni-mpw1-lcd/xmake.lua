-- Copyright SCI Semiconductor and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

-- Update this to point to the location of the CHERIoT SDK
sdkdir = path.absolute("/Users/rmn30/iceni/cheriot-rtos/sdk")

set_project("Iceni MPW1 Demo")

includes(sdkdir)

set_toolchains("cheriot-clang")

includes(path.join(sdkdir, "lib"))

option("board")
  set_default("/Users/rmn30/iceni/iceni-rtos_bsp/boards/iceni-mpw1.json")

compartment("mpw1-demo")
  set_default(false)
  add_deps("freestanding", "stdio", "debug", "cxxrt")
  add_files("mpw1-demo.cc")
  -- LCD drivers
  add_files("../third_party/display_drivers/core/lcd_base.c")
  add_files("../third_party/display_drivers/core/lucida_console_12pt.c")
  add_files("../third_party/display_drivers/st7735/lcd_st7735.c", {defines = "CHERIOT_NO_AMBIENT_MALLOC"})
  -- add_files("../../third_party/display_drivers/lcd_sonata_0.2.cc")
  -- add_files("../../third_party/display_drivers/lcd_sonata_1.0.cc")
  add_files("../third_party/display_drivers/lcd_iceni_mpw1.cc")

firmware("mp1-demo-fw")
  set_policy("build.warning", true)
  add_deps("mpw1-demo")
  on_load(function(target)
    target:values_set("board", "$(board)")
    target:values_set("threads", {
      {
        compartment = "mpw1-demo",
        priority = 1,
        entry_point = "mpw1_demo_entry",
        stack_size = 1280,
        trusted_stack_frames = 4
      },
    }, {expand = false})
  end)
