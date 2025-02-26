from time import sleep_ms, ticks_ms 
from machine import I2C, Pin, UART
from i2c_lcd import I2cLcd 

sonata_uart = UART(1, 9600, rx=13, tx=12)

DEFAULT_I2C_ADDR = 0x27
i2c = I2C(scl=Pin(22), sda=Pin(21), freq=400000) 
lcd = I2cLcd(i2c, DEFAULT_I2C_ADDR, 2, 16)

# Set up custom characters on LCD
cherry_left=1
lcd.custom_char(cherry_left, bytearray([0x01, 0x02, 0x04, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E]))
cherry_right=2
lcd.custom_char(cherry_right, bytearray([0x10, 0x08, 0x04, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E]))
cherry_left_char=chr(cherry_left)
cherry_right_char=chr(cherry_right)

lcd.move_to(0, 0)
lcd.putstr(f' SCI CHERIoT {cherry_left_char}{cherry_right_char}')
lcd.move_to(0, 1)
lcd.putstr('Smart Meter Demo')

led = Pin(17, Pin.OUT)

while True:
    command = sonata_uart.readline()
    if command is not None:
        print(command)
        words = command.split()
        if len(words) == 2:
            [target, action] = command.split()
            if target ==  b'led':
                if action == b'on':
                    led.value(1)
                else:
                    led.value(0)
    else:
        sleep_ms(1)
