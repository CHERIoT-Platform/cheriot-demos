Hugh The Lightbulb
==================

Hugh the lightbulb is a demonstration of an Internet-connected multicolour lightbulb, using Sonata's RGB LEDs.

The [`HughController`](HughController/) directory contains an Android app that displays a controller.
The [`Device`](Device/) directory contains the code that runs on Sonata.

When the Sonata device boots, it displays a unique connection string.
Enter this in the Android app and you can control the lightbulb from your phone.

[Lack of any] Security note
---------------------------

This uses a public MQTT server and does no authentication at all.
Do not copy this code for production use!
