UART Echo Test
==============

This is a trivial UART loopback program for the Sonata board. It configures the
pinmux to conect uart1 to PMOD0 pins 2 (TX) and 3 (RX) and then enters a loop
reading from the UART and echoing back any characters received. The baud rate is
set to 9600. 

The PMOD connector is the 15 x 2 right angled connector with holes. It is
labelled PMOD0 on the left side (when facing the holes) and PMOD1 on the other.
The pins used for the UART can be found like so:

```
P +--------------------------------------
M | 3V | GD |  4 | RX | TX |  1 | ...
O +--------------------------------------
D | 3V | GD |  8 |  7 |  6 |  5 | ...
0 +--------------------------------------
```
