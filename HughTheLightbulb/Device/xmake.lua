-- Copyright SCI Semiconductor and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

-- Update this to point to the location of the CHERIoT SDK
sdkdir = path.absolute("../../cheriot-rtos/sdk")

set_project("CHERIoT MQTT Example")

includes(sdkdir)

set_toolchains("cheriot-clang")

includes(path.join(sdkdir, "lib"))
includes("../../network-stack/lib")

option("board")
  set_default("sonata")

compartment("hugh")
  set_default(false)
  add_includedirs("../../network-stack/include")
  add_deps("freestanding", "TCPIP", "NetAPI", "TLS", "Firewall", "SNTP", "DNS", "MQTT", "time_helpers", "debug", "stdio")
  add_files("mqtt.cc")
  -- LCD drivers
  add_files("../../third_party/display_drivers/core/lcd_base.c")
  add_files("../../third_party/display_drivers/core/lucida_console_12pt.c")
  add_files("../../third_party/display_drivers/st7735/lcd_st7735.c")
  add_files("../../third_party/display_drivers/lcd_sonata_0.2.cc")
  add_files("../../third_party/display_drivers/lcd_sonata_1.0.cc")

  on_load(function(target)
    target:add('options', "IPv6")
    local IPv6 = get_config("IPv6")
    target:add("defines", "CHERIOT_RTOS_OPTION_IPv6=" .. tostring(IPv6))
  end)

firmware("HughBulb")
  set_policy("build.warning", true)
  add_deps("hugh")
  add_options("tls-rsa")
  on_load(function(target)
    target:values_set("board", "$(board)")
    target:values_set("threads", {
      {
        compartment = "hugh",
        priority = 1,
        entry_point = "hugh",
        -- TLS requires *huge* stacks!
        stack_size = 8160,
        trusted_stack_frames = 6
      },
      {
        compartment = "hugh",
        priority = 2,
        entry_point = "graphs",
        stack_size = 1152,
        trusted_stack_frames = 3
      },
      {
        -- TCP/IP stack thread.
        compartment = "TCPIP",
        priority = 1,
        entry_point = "ip_thread_entry",
        stack_size = 0x1000,
        trusted_stack_frames = 5
      },
      {
        -- Firewall thread, handles incoming packets as they arrive.
        compartment = "Firewall",
        -- Higher priority, this will be back-pressured by the message
        -- queue if the network stack can't keep up, but we want
        -- packets to arrive immediately.
        priority = 2,
        entry_point = "ethernet_run_driver",
        stack_size = 0x1000,
        trusted_stack_frames = 5
      }
    }, {expand = false})
  end)

