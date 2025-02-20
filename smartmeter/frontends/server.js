const express = require("express");
const fs = require("fs");
const path = require("path");
const process = require("node:process");
const url = require("url");

const Microvium = require("microvium");
const MQTT = require("mqtt");

const democonfig = require("./static/democonfig.mjs");

const PORT = process.env.PORT || 4000;

/// Drive Microvium through compilation
function compileMVM(sourceText)
{
  // Create an empty VM
  const vm = Microvium.create();

  // Add Microvium's default globals such as `vmExport` and `vmImport`
  Microvium.addDefaultGlobals(vm);

  const importDependency = Microvium.nodeStyleImporter(vm,
    { basedir: "./mjs"
    });

  try {
    // Evaluate the script
    vm.evaluateModule({ sourceText, debugFilename: 'firmware.js', importDependency });

    // Create the snapshot
    const snapshot = vm.createSnapshot();

    // console.log(Microvium.decodeSnapshot(snapshot));

    // The `snapshot.data` is a Node Buffer (Uint8Array) containing the snapshot bytes
    return { "result": snapshot.data, "error": null }
  } catch (e) {
    return { "result": null, "error": e.message }
  }
}

// Synchronously connect to the MQTT broker
const mqttClientID = `CHERIoT${Math.random().toString(16)}`;
const mqttClient = MQTT.connect(
  democonfig.MQTT_BROKER_MQTTS_URL,
  { mqttClientID
  , clean: true
  , connectTimeout: 5000
  , reconnectPeriod: 1000
  , ca: fs.readFileSync("./mosquitto.org.crt")
  });


// Diagnostic callbacks
mqttClient.on('connect', () => { console.log("MQTT connected"); });
mqttClient.on('error', (error) => { console.log("MQTT Error", error) });
mqttClient.on('reconnect', (error) => { console.log("MQTT Reconnect", error) });

const expressApp = express()

expressApp.use('/static', express.static('static'))
expressApp.use('/mqttjs', express.static('node_modules/mqtt/dist'))

expressApp.post('/postjs/:meter',
  (req, res, next) => { console.log("POST JS", req.params.meter); next(); },
  express.text(), // XXX Probably not once we have a frontend
  (req, res) =>
  {
    const vmresult = compileMVM(req.body);
    console.log(req.body, vmresult);

    if (vmresult.result === null)
    {
      res.status(400).send(vmresult.error);
      return;
    }

    if (vmresult.result.length > 4096)
    {
      res.status(400).send("Compiled bytecode exceeds client network buffer");
      return;
    }

    mqttClient.publish(
      `cheriot-smartmeter/u/js/${req.params.meter}`,
      vmresult.result,
      { qos: 2, retain: false },
      (error) =>
      {
        if (error)
        {
          console.error("MQTT Publish", error)
          res.status(500).send(error);
        }
        else
        {
          res.status(200).send("OK");
        }
      });
  });

expressApp.listen(PORT, () =>
  {
    console.log(`server listening on  ${PORT}`);
  });
