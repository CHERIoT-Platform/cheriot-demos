from time import sleep_ms, ticks_ms, ticks_add, ticks_diff
from machine import I2C, Pin, UART
from i2c_lcd import I2cLcd
import neopixel

sonata_uart = UART(1, 9600, rx=13, tx=12)

DEFAULT_I2C_ADDR = 0x27
i2c = I2C(scl=Pin(22), sda=Pin(21), freq=400000) 
lcd = I2cLcd(i2c, DEFAULT_I2C_ADDR, 2, 16)

next_custom_char=0
def make_custom_char(s):
    global next_custom_char
    if next_custom_char > 7:
        raise Exception('Max 8 custom characters')
    def line_to_byte(l):
        return sum([1 << n for n in range(5) if l[4-n]=='X'])
    lines = s.strip().splitlines()
    char_bytes = bytearray([line_to_byte(l) for l in lines])
    lcd.custom_char(next_custom_char, char_bytes)
    custom_char = chr(next_custom_char)
    next_custom_char += 1
    return custom_char

cherry_left=make_custom_char('''
....X
...X.
..X..
.XXX.
XXXXX
XXXXX
XXXXX
.XXX.
''')
cherry_right=make_custom_char('''
X....
.X...
..X..
.XXX.
XXXXX
XXXXX
XXXXX
.XXX.
''')
def make_battery(n):
    lines = '''
.XXX.
XX.XX
''' + (5-n) * 'X...X\n' + (n+1) * 'XXXXX\n'
    return make_custom_char(lines)
battery = [make_battery(n) for n in range(5)]
battery.append(make_custom_char('.XXX.\n' + 7 * 'XXXXX\n'))

lcd.clear()
lcd.move_to(0, 0)
lcd.putstr(f'  SCI CHERIoT {cherry_left}{cherry_right}')
lcd.move_to(0, 1)
lcd.putstr('Smart Meter Demo')
# lcd.putstr(' '.join(battery))
# for i in range(10):
#     for b in battery:
#         lcd.move_to(0,1)
#         lcd.putstr(b)
#         sleep_ms(1000)

class BaseAppliance:
    """
    Synchronise state with house e.g. write led
    """
    def sync(self):
        pass

class OnOffAppliance:
    def __init__(self, load):
        self.load_when_on = load
    def do_action(self, s):
        self.is_on = s == b'on'
        self.sync()
    def get_load(self):
        return self.load_when_on if self.is_on else 0

class YellowLed(OnOffAppliance):
    def __init__(self):
        OnOffAppliance.__init__(self, 1)
        self.pin = Pin(17, Pin.OUT)
        self.is_on = False

    def sync(self):
        self.pin.value(1 if self.is_on else 0)

class RGBLed(OnOffAppliance):
    RED = [x * 50 for x in (1,0,0)]
    OFF = [0,0,0]
    
    def __init__(self):
        OnOffAppliance.__init__(self, 5)
        rgb_pin = Pin(16, Pin.OUT)
        self.rgb_np = neopixel.NeoPixel(rgb_pin, 4)
        self.is_on = False
    def sync(self):
        c = RGBLed.RED if self.is_on else RGBLed.OFF
        for n in range(4):
            self.rgb_np[n]=c
        self.rgb_np.write()
        
class Battery(BaseAppliance):
    MAX_CHARGE=5
    def __init__(self):
        self.state_of_charge=0
        self.rate_of_charge=0
    def get_load(self):
        discharging = self.state_of_charge > 0 and self.rate_of_charge < 0
        charging = self.state_of_charge < self.MAX_CHARGE and self.rate_of_charge > 0
        if charging or discharging:
            next_soc = self.state_of_charge + self.rate_of_charge
            self.state_of_charge = max(0, min(self.MAX_CHARGE, next_soc))
            return self.rate_of_charge
        else:
            return 0
    def sync(self):
        lcd.move_to(0,0)
        lcd.putstr(battery[self.state_of_charge])
    def do_action(self, s):
        try:
            self.rate_of_charge=int(s)
        except:
            print(f"Battery action failed: {s}")
        
appliances = {
    b'led': YellowLed(),
    b'rgb': RGBLed(),
    b'battery': Battery(),
}

REPORT_INTERVAL=1000
start_ticks = ticks_ms()
next_report = ticks_add(start_ticks, REPORT_INTERVAL)

while True:
    if ticks_diff(ticks_ms(), next_report) > 0:
        total_load=0
        for a in appliances.values():
            a.sync()
            total_load+=a.get_load()
        report=f'powerSample {total_load}\n'
        print(report)
        sonata_uart.write(report)
        next_report = ticks_add(next_report, REPORT_INTERVAL)
    command = sonata_uart.readline()
    if command is not None:
        print(command)
        words = command.split()
        if len(words) == 2:
            [target, action] = command.split()
            app = appliances.get(target, None)
            if app is not None:
                app.do_action(action)  
    else:
        sleep_ms(1)
