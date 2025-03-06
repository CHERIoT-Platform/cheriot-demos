import * as democonfig from "./democonfig.mjs";

/////
// Snoop on the "user" timebase so that "now" evolves in parallel
/////

var timebaseZero;
var timebaseRate;

function timebaseRetime(sec)
{
  if (timebaseZero === null)
  {
    return sec;
  }

  if (sec >= timebaseZero)
  {
    return timebaseZero + timebaseRate * (sec - timebaseZero);
  }

  return sec;
}

/////
// MQTT Client
/////

const mqttClient = mqtt.connect(
  democonfig.MQTT_BROKER_WSS_URL,
  { clean: true
  , connectTimeout: 4000
  });

mqttClient.on("message", (t, m) =>
  {
    const mString = m.toString();
    console.log("Message", t.toString(), mString);
    if (t.startsWith("cheriot-smartmeter/g/update"))
    {
      const report = /^\s*(\d+)\s+(\d+)\s+.*$/.exec(mString);
      updateConsumptionData(report);
    }
    else if (t.startsWith("cheriot-smartmeter/u/timebase"))
    {
      const newTimebase = /^\s*(\d+)\s+(\d+)\s*$/.exec(mString);
      timebaseZero = parseInt(newTimebase[1]);
      timebaseRate = parseInt(newTimebase[2]);
      console.log(`New timebase: ${timebaseZero} ${timebaseRate}`);
    }
  });
mqttClient.on("connect", () =>
  {
    console.log("Connected");
  });

/////
// Consumption chart
/////

let consumptionData = [];

const consumptionChart = new Chart(document.getElementById("consumptionChart"),
  { type: 'line'
  , data:
    { datasets: [ {label: 'Draw', data: consumptionData } ] }
  , options:
    { scales:
      { x:
        { title:
          { display: true
          , text: "Time"
          }
        , type: 'time'
        , time:
          { minUnit: 'second' }
        }
      }
    }
  });

function updateConsumptionData(report)
{
  const now = parseInt(report[1]) * 1000; // JS likes to work in mSec

  // Assume we haven't missed anything
  consumptionData.push({ x: now, y: parseInt(report[2]) })

  // Prune old data
  consumptionData.splice(1, consumptionData.findIndex(
    (dataElement) => dataElement.x >= now - 7200000) - 1);

  consumptionChart.update();
}

/////
// Grid alert form handling
/////

const gridAlertForm = document.getElementById("gridAlertForm");
const gridAlertDuration = gridAlertForm.querySelector("#gridAlertDuration");
const gridAlertSelect = gridAlertForm.querySelector("#gridAlertSelect");
const gridAlertSubmit = gridAlertForm.querySelector("#gridAlertSubmit");

gridAlertForm.addEventListener("submit", (event) =>
  {
    event.preventDefault();

    const now = timebaseRetime(Date.now() / 1000 | 0);
    const duration = gridAlertDuration.value * 60;
    const value = gridAlertSelect.value;

    console.log(`Publishing grid alert: ${lastMeterId} ${now} ${duration} ${value}`);
    if (mqttClient)
    {
      mqttClient.publish(
        `cheriot-smartmeter/g/request/${lastMeterId}`,
        `${now} ${duration} ${value}`,
        { qos: 1 });
    }
  });


/////
// Meter ID form handling
/////

const meterIDForm = document.getElementById("meterIDForm");
const meterIDValue = meterIDForm.querySelector("#meterIdentity");
const meterIDSubmit = meterIDForm.querySelector("#meterIDSubmit");

function buttonEnables(needMeterID)
{
    meterIDSubmit.disabled = !needMeterID;
    gridAlertSubmit.disabled = needMeterID;
}

var lastMeterId; // for unsubscription
function onNewMeterID(newID)
{
    if (mqttClient && lastMeterId !== null)
    {
      mqttClient.unsubscribe(`cheriot-smartmeter/g/update/${lastMeterId}`,
        (err) => { if(err) { console.log("Failed to unsubscribe from updates?", err); } }
      );
      mqttClient.unsubscribe(`cheriot-smartmeter/u/timebase/${lastMeterId}`,
        (err) => { if(err) { console.log("Failed to unsubscribe from timebase?", err); } }
      );
    }

    lastMeterId = newID;
    timebaseZero = null;

    if (mqttClient)
    {
      mqttClient.subscribe(`cheriot-smartmeter/g/update/${lastMeterId}`,
        (err) => { if(err) { console.log("Failed to subscribe?", err); } }
      );
      mqttClient.subscribe(`cheriot-smartmeter/u/timebase/${lastMeterId}`,
        (err) => { if(err) { console.log("Failed to subscribe to timebase?", err); } }
      );

    }

    buttonEnables(false);
}

meterIDValue.addEventListener("input", (event) =>
  {
    buttonEnables(true);
  });

meterIDForm.addEventListener("submit", (event) =>
  {
    event.preventDefault();
    onNewMeterID(meterIDValue.value);
  });

if (democonfig.DEFAULT_METER_ID !== null)
{
  meterIDValue.value = democonfig.DEFAULT_METER_ID;
  onNewMeterID(meterIDValue.value);
}
