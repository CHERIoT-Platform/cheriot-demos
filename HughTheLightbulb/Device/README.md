Hugh the Lightbulb
==================

This directory contains the code that runs on the Sonata board.
It displays the unique ID for this lightbulb on the LCD and then makes RGB LED0 monitor the MQTT server for colour updates.

Compile without IPv6 and with network fault injection enabled 
```
xmake config --sdk=/cheriot-tools/ --IPv6=n --network-inject-faults=y -P .
xmake
xmake run
```
