-- Copyright SCI Semiconductor and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT MQTT Example")

sdkdir = os.getenv("CHERIOT_RTOS_SDK") or path.absolute("../../cheriot-rtos/sdk")
includes(sdkdir)
set_toolchains("cheriot-clang")

netdir = os.getenv("CHERIOT_NETWORK_STACK") or path.absolute("../../network-stack")
includes(path.join(netdir, "lib"))

local function on_load_ipv6(target)
  target:add('options', "IPv6")
  local IPv6 = get_config("IPv6")
  target:add("defines", "CHERIOT_RTOS_OPTION_IPv6=" .. tostring(IPv6))
end

option("board")
  set_default("sonata-1.1")

compartment("housekeeping")
  add_includedirs(path.join(netdir,"include"))

  add_files("housekeeping.cc")

  on_load(function(target)
    on_load_ipv6(target)
  end)

compartment("sensor")
  add_includedirs(path.join(netdir,"include"))

  add_files("sensor.cc")

  on_load(function(target)
    target:values_set("shared_objects", { sensor_data = 40 }, {expand = false})
  end)

compartment("grid")
  add_includedirs(path.join(netdir,"include"))

  add_files("grid.cc")

  on_load(function(target)
    on_load_ipv6(target)

    target:values_set("shared_objects",
      { grid_planned_outage = 12,
        grid_request = 12 },
      {expand = false})
  end)

compartment("provider")
  add_includedirs(path.join(netdir,"include"))

  add_files("provider.cc")
  on_load(function(target)
    on_load_ipv6(target)

    target:values_set("shared_objects",
      { provider_schedule = 104,
        provider_variance = 20 },
      {expand = false})
  end)

compartment("userJS")
  add_files("userJS.cc")

compartment("user")
  add_includedirs(path.join(netdir,"include"))

  add_files("user.cc")

  on_load(function(target)
    on_load_ipv6(target)

    target:values_set("shared_objects",
      { userJS_snapshot = 168 },
      {expand = false})

  end)

firmware("smartmeter")
  set_policy("build.warning", true)

  add_deps("housekeeping", "sensor", "grid", "provider", "user", "userJS")

  -- RTOS deps
  add_deps("debug", "freestanding", "microvium", "stdio", "string", "strtol")

  -- Network Stack deps
  add_deps("DNS", "Firewall", "MQTT", "NetAPI", "SNTP", "TCPIP", "TLS", "time_helpers")

  add_options("tls-rsa")
  on_load(function(target)
    target:values_set("board", "$(board)")
    target:values_set("threads", {
      {
        -- TCP/IP stack thread.
        compartment = "TCPIP",
        priority = 1,
        entry_point = "ip_thread_entry",
        stack_size = 4096,
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
      },
      {
        compartment = "housekeeping",
        priority = 1,
        entry_point = "housekeeping_entry",
        stack_size = 2048,
        trusted_stack_frames = 6
      },
      {
        compartment = "sensor",
        priority = 1,
        entry_point = "sensor_entry",
        stack_size = 768,
        trusted_stack_frames = 3
      },
      {
        compartment = "grid",
        priority = 1,
        entry_point = "grid_entry",
        -- TLS requires *huge* stacks!
        stack_size = 8160,
        trusted_stack_frames = 6
      },
      {
        compartment = "provider",
        priority = 1,
        entry_point = "provider_entry",
        -- TLS requires *huge* stacks!
        stack_size = 8160,
        trusted_stack_frames = 6
      },
      {
        compartment = "user",
        priority = 1,
        entry_point = "user_data_entry",
        stack_size = 0xa00,
        trusted_stack_frames = 4
      },
      {
        compartment = "user",
        priority = 1,
        entry_point = "user_net_entry",
        -- TLS requires *huge* stacks!
        stack_size = 8160,
        trusted_stack_frames = 6
      },
    }, {expand = false})
  end)

