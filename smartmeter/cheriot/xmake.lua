-- Copyright SCI Semiconductor and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT MQTT Example")

sdkdir = os.getenv("CHERIOT_RTOS_SDK") or path.absolute("../../cheriot-rtos/sdk")
includes(sdkdir)
set_toolchains("cheriot-clang")

netdir = os.getenv("CHERIOT_NETWORK_STACK") or path.absolute("../../network-stack")
includes(path.join(netdir, "lib"))

option("board")
  set_default("sonata-1.1")

option("broker-host")
  set_default("test.mosquitto.org")

option("broker-anchor")
  set_default("mosquitto.org.h")

option("unique-id")
  set_default("random")

rule("smartmeter.mqtt")
  on_load(function (target)
    -- Note: port 8883 to be encrypted and tolerating unautenticated connections
    target:add('options', "broker-host")
    local broker_host = get_config("broker-host")
    target:add("defines", table.concat({"MQTT_BROKER_HOST=\"", tostring(broker_host), "\""}))

    target:add('options', "broker-anchor")
    local broker_anchor = get_config("broker-anchor")
    target:add("defines", table.concat({"MQTT_BROKER_ANCHOR=\"", tostring(broker_anchor), "\""}))
  end)

rule("housekeeping.unique-id")
  on_load(function (target)
    target:add('options', "unique-id")
    local unique_id = get_config("unique-id")
    target:add("defines", table.concat({"MQTT_UNIQUE_ID=\"", tostring(unique_id), "\""}))
  end)

compartment("housekeeping")
  add_includedirs(path.join(netdir,"include"))

  add_files("housekeeping.cc")

  add_rules("cheriot.network-stack.ipv6", "housekeeping.unique-id")


compartment("sensor")
  add_includedirs(path.join(netdir,"include"))

  add_files("sensor.cc")

  on_load(function(target)
    target:values_set("shared_objects", { sensor_data = 40 }, {expand = false})
  end)

compartment("grid")
  add_includedirs(path.join(netdir,"include"))

  add_files("grid.cc")

  add_rules("cheriot.network-stack.ipv6", "smartmeter.mqtt")

  on_load(function(target)
    target:values_set("shared_objects",
      { grid_planned_outage = 12
      , grid_request = 12 },
      {expand = false})
  end)

compartment("provider")
  add_includedirs(path.join(netdir,"include"))

  add_files("provider.cc")

  add_rules("cheriot.network-stack.ipv6", "smartmeter.mqtt")

  on_load(function(target)
    target:values_set("shared_objects",
      { provider_schedule = 104
      , provider_variance = 20 },
      {expand = false})
  end)

compartment("userJS")
  add_files("userJS.cc")

compartment("user")
  add_includedirs(path.join(netdir,"include"))

  add_files("user.cc")

  add_rules("cheriot.network-stack.ipv6", "smartmeter.mqtt")

  on_load(function(target)
    target:values_set("shared_objects",
      { userJS_snapshot = 168 },
      {expand = false})
  end)

compartment("monolith")
  add_includedirs(path.join(netdir,"include"))

  add_files("housekeeping.cc")
  add_files("sensor.cc")
  add_files("grid.cc")
  add_files("provider.cc")
  add_files("userJS.cc")
  add_files("user.cc")

  add_rules("cheriot.network-stack.ipv6", "smartmeter.mqtt", "housekeeping.unique-id")

  on_load(function(target)
    target:add("defines", "MONOLITH_BUILD_WITHOUT_SECURITY")
    target:values_set("shared_objects",
      { grid_planned_outage = 12
      , grid_request = 12
      , provider_schedule = 104
      , provider_variance = 20
      },
      {expand = false})
  end)

function mkthreads(overrideCompartmentName)
 return {
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
      compartment = overrideCompartmentName or "housekeeping",
      priority = 1,
      entry_point = "housekeeping_entry",
      stack_size = 2048,
      trusted_stack_frames = 6
    },
    {
      compartment = overrideCompartmentName or "sensor",
      priority = 1,
      entry_point = "sensor_entry",
      stack_size = 768,
      trusted_stack_frames = 3
    },
    {
      compartment = overrideCompartmentName or "grid",
      priority = 1,
      entry_point = "grid_entry",
      -- TLS requires *huge* stacks!
      stack_size = 8160,
      trusted_stack_frames = 6
    },
    {
      compartment = overrideCompartmentName or "provider",
      priority = 1,
      entry_point = "provider_entry",
      -- TLS requires *huge* stacks!
      stack_size = 8160,
      trusted_stack_frames = 6
    },
    {
      compartment = overrideCompartmentName or "user",
      priority = 1,
      entry_point = "user_data_entry",
      stack_size = 0xa00,
      trusted_stack_frames = 4
    },
    {
      compartment = overrideCompartmentName or "user",
      priority = 1,
      entry_point = "user_net_entry",
      -- TLS requires *huge* stacks!
      stack_size = 8160,
      trusted_stack_frames = 6
    }
  }
end

function mkfirmware(name, body, threads)
  firmware(name)
    set_policy("build.warning", true)

    -- RTOS deps
    add_deps("debug", "freestanding", "microvium", "stdio", "string", "strtol")

    -- Network Stack deps
    add_deps("DNS", "Firewall", "MQTT", "NetAPI", "SNTP", "TCPIP", "TLS", "time_helpers")

    add_options("tls-rsa")
    on_load(function(target)
      target:values_set("board", "$(board)")
      target:values_set("threads", threads, {expand = false})
    end)

    body()
end


mkfirmware("smartmeter",
  function()
    add_deps("housekeeping", "sensor", "grid", "provider", "user", "userJS")
  end,
  mkthreads(nil --[[ many compartments --]]))

mkfirmware("smartmeter-monolith",
  function()
    add_deps("monolith")
  end,
  mkthreads("monolith"))
