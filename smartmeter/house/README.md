# CHERIoT smart meter demo -- house code

This directory contains micropython code to run on the esp32 board supplied
with the KS5009 "Smart Home" used for the demo.

## Installation

Assuming the esp32 is already flashed with MicroPython you can use `rshell`
(available via `pip`) to install the python files on the board, for example:

```
rshell -p /dev/tty.wchusbserial110 rsync python/ /pyboard/ 
```
