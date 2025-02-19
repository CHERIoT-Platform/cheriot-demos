import * as democonfig from "./democonfig.mjs";

const mqttClient = mqtt.connect(
  democonfig.MQTT_BROKER_WSS_URL,
  { clean: true
  , connectTimeout: 4000
  });

mqttClient.on("message", (t, m) =>
  {
    console.log("Message", t.toString(), m.toString());
    if (t.startsWith("cheriot-smartmeter/g/update"))
    {
      document.getElementById("latest-update").textContent = m.toString();
    }
  });
mqttClient.on("connect", () =>
  {
    console.log("Connected");
    mqttClient.subscribe("cheriot-smartmeter/g/update/#", (err) =>
      {
        if(err) { console.log("Failed to subscribe", err); }
      });
  });
