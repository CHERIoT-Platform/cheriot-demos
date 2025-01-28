Hugh the Lightbulb V2
=====================

This directory contains the code that runs on the Sonata board.
It displays the unique ID for this lightbulb on the LCD and then makes RGB LED0 monitor the MQTT server for colour updates.

Compile without IPv6 and with network fault injection and scheduler accounting enabled:

```
xmake config --sdk=/cheriot-tools/ --IPv6=n --network-inject-faults=y --scheduler-accounting=y  -P .
xmake
xmake run
```

This version requires at least Sonata 1.0.

Version 2 has several improvements of v1 (which can run on Sonata 0.2):

 - Messages are end-to-end encrypted between the controller and device.
 - Pairing the controller is done via QR code.
   Flip switch 1 to toggle the display between the traditional display and showing the QR code.
   The QR code also includes the encryption key.
 - The application logic is now split into three compartments.

The encryption key is generated in the crypto compartment and is exposed to the display compartment for rendering.
The display compartment is not part of the attack surface.
It can read the switches but otherwise simply writes to the screen and monitors CPU and memory usage.

The network compartment does not have access to keying material.
It has direct access to the RGB LED and an MQTT endpoint, but passes received data to the crypto compartment and updates the LED only if the crypto compartment successfully decrypts messages.
