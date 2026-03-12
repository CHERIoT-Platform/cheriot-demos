# External Interrupts on Sonata

The sonata v1.3 release introduced GPIO pin-change interrupts. One can select a
set of GPIO registers of interest and configure the interrupt mode. The
following interrupt modes are available:
- any edge
- rising edge
- falling edge
- low level

See https://lowrisc.github.io/sonata-system/doc/ip/gpio.html for a complete
description of the peripheral.