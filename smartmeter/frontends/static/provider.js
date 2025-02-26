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

var lastMidnight = null;
var lastTimestamp = null;

function updateTimestamps()
{
  const now = Date.now();
  lastTimestamp = timebaseRetime(now);
  lastMidnight = new Date(lastTimestamp);
  lastMidnight.setHours(0, 0, 0);

  // console.log("updateTimestamps", now, lastTimestamp, lastMidnight);
}

function displayHour()
{
  if (lastTimestamp === null)
  {
    // console.log("displayHour null");
    return -1;
  }

  const ret = (lastTimestamp - lastMidnight.getTime()) / 1000 / 3600;
  // console.log("displayHour", ret);

  return ret;
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
    if (t.startsWith("cheriot-smartmeter/p/update"))
    {
      document.getElementById("latest-update").textContent = mString;
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
// Rate data and chart
/////

const rateTables =
  { flat: Array.from({length: 24}, (v, i) => 760)
  , winterwk:
    [760,760,760,760,760,760,760,760,1580,1580,1580,1220,1220,1220,1220,1580,1580,1580,760,760,760,760,760,760]
  , summerwk:
    [760,760,760,760,760,760,760,760,1220,1220,1220,1580,1580,1580,1580,1220,1220,1220,760,760,760,760,760,760]
  , ulowe:
    [280,280,280,280,280,280,280,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,760,280]
  }

const rateDisplayData = new Array(48);

const rateChart = new Chart(document.getElementById("powerRateChart"),
  { type: 'line'
  , data:
    { labels: [...rateDisplayData.keys()]
    , datasets: [{ label: 'Rate', data: rateDisplayData, borderWidth: 1 }]
    }
  , options:
    { scales:
      { x:
        { title: 
          { display: true
          , text: "Hours since today's midnight"
          }
        , beginAtZero: true
        , min: 0
        , max: 47
        }
      , y:
        { beginAtZero: true
        , title:
          { display: true
          , text: "p/kWh"
          }
        , ticks:
          { callback: (value, index, ticks) => value.toFixed(1) }
        }
      }
    , plugins:
      { annotation:
        { common: { drawTime: "afterDraw" }
        , annotations:
          { line1:
            { type: 'line'
            , borderColor: 'rgb(255, 99, 132)'
            , borderWidth: 2
            , scaleID: 'x'
            , value: (ctx, obj) => displayHour()
            , endValue: (ctx, obj) => displayHour()
            , display: (ctx, obj) => lastMidnight !== null
            }
          }
        }
      }
    } 
  });

const rateData = new Array(48);

const rateForm = document.getElementById("providerRateForm");
const rateTodaySelect = rateForm.querySelector("#providerRateToday");
const rateTomorrowSelect = rateForm.querySelector("#providerRateTomorrow");
const rateSubmit = rateForm.querySelector("#providerRateSubmit");

function setRateData(anim)
{
  rateData.splice(0, 24, ...rateTables[rateTodaySelect.value]);
  rateData.splice(24, 24, ...rateTables[rateTomorrowSelect.value]);
  rateDisplayData.splice(0, 48, ...rateData.map((v) => v/100));
  rateChart.update(anim);
}

rateTodaySelect.addEventListener("input", (event) =>
  {
    setRateData();
  });
rateTomorrowSelect.addEventListener("input", (event) =>
  {
    setRateData();
  });

rateForm.addEventListener("submit", (event) =>
  {
    event.preventDefault();

    const rateMessage = `${lastMidnight.getTime() / 1000 | 0} ${rateData.map((v) => v.toString()).join(" ")}`;
    console.log("Rate message", rateMessage);
    if (mqttClient)
    {
      mqttClient.publish(
        `cheriot-smartmeter/p/schedule/${lastMeterId}`, rateMessage, { qos: 1 });
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
    rateSubmit.disabled = needMeterID;
}

var lastMeterId; // for unsubscription
function onNewMeterID(newID)
{
    if (mqttClient && lastMeterId !== null)
    {
      mqttClient.unsubscribe(`cheriot-smartmeter/p/update/${lastMeterId}`,
        (err) => { if(err) { console.log("Failed to unsubscribe from updates?", err); } }
      );
      mqttClient.unsubscribe(`cheriot-smartmeter/u/timebase/${lastMeterId}`,
        (err) => { if(err) { console.log("Failed to unsubscribe from timebase?", err); } }
      );
    }

    lastMeterId = newID;

    if (mqttClient)
    {
      mqttClient.subscribe(`cheriot-smartmeter/p/update/${lastMeterId}`,
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

function allUpdates()
{
  // Initialize with default settings
  updateTimestamps();
  setRateData('none');
}

allUpdates();
setInterval(allUpdates, 5000);
