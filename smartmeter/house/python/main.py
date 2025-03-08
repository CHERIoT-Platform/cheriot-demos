from time import sleep_ms, ticks_ms, ticks_add, ticks_diff
from machine import I2C, Pin, UART, PWM
from i2c_lcd import I2cLcd
import neopixel

sonata_uart = UART(1, 9600, rx=13, tx=12)

DEFAULT_I2C_ADDR = 0x27
i2c = I2C(scl=Pin(22), sda=Pin(21), freq=400000) 
lcd = I2cLcd(i2c, DEFAULT_I2C_ADDR, 2, 16)
lcd.clear()
'''
For given string consisting of 8 lines of 5 characters
return an 8 byte array where each byte has a bits set
For convenience when creating custom glyphs.
'''
def str_to_glyph_bytes(s):
    def line_to_byte(l):
        return sum([1 << n for n in range(5) if l[4-n]=='X'])
    lines = s.strip().splitlines()
    return bytearray([line_to_byte(l) for l in lines])

next_custom_char=0
def make_custom_char(s):
    global next_custom_char
    if next_custom_char > 7:
        raise Exception('Max 8 custom characters')

    char_bytes = str_to_glyph_bytes(s)
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

house_char_str='''
..X..
.XXX.
XXXXX
.XXX.
.X.X.
.XXX.
.X.X.
.X.X.
'''

# This char looks like a electricity pylon
grid_char=chr(0xb7)
# And this one a tiny bit like a turbine
turbine_char=chr(0xb2)

marquee_text=f'SCI CHERIoT {cherry_left}{cherry_right} Smart Meter Demo {cherry_left}{cherry_right} '
marquee_offset=0

def make_battery(n):
    lines = '''
.XXX.
XX.XX
''' + (5-n) * 'X...X\n' + (n+1) * 'XXXXX\n'
    return make_custom_char(lines)
battery_chars = [make_battery(n) for n in range(5)]
battery_chars.append(make_custom_char('.XXX.\n' + 7 * 'XXXXX\n'))

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
    def toggle(self):
        print('toggle')
        self.is_on = not self.is_on
        self.sync()

class YellowLed(OnOffAppliance):
    def __init__(self):
        OnOffAppliance.__init__(self, 1)
        self.pin = Pin(17, Pin.OUT)
        self.is_on = False

    def sync(self):
        self.pin.value(1 if self.is_on else 0)

class RGBLed(BaseAppliance):
    RED = [x * 50 for x in (1,0,0)]
    OFF = [0,0,0]
    
    def __init__(self):
        OnOffAppliance.__init__(self, 4)
        rgb_pin = Pin(16, Pin.OUT)
        self.rgb_np = neopixel.NeoPixel(rgb_pin, 4)
        self.load = 0
    def get_load(self):
        return self.load
    def sync(self):
        for n in range(4):
            c = RGBLed.RED if n < self.load else RGBLed.OFF
            self.rgb_np[n]=c
        self.rgb_np.write()
    def do_action(self, s):
        try:
            self.load=int(s)
        except:
            self.load+=1
        if self.load < 0 or self.load > 4:
            self.load=0
        self.sync()
        
class Battery(BaseAppliance):
    MAX_CHARGE=50
    MAX_RATE_OF_CHARGE=3
    def __init__(self):
        self.state_of_charge=0
        self.net_target=0
        self.last_rate=0
    def get_load(self, other_load):
        # at what rate do we aim to charge /discharge the battery
        # we limit to between -MAX_RATE_OF_CHARGE, MAX_RATE_OF_CHARGE
        target_rate = max(-self.MAX_RATE_OF_CHARGE, min(self.MAX_RATE_OF_CHARGE, self.net_target - other_load))
        discharging = self.state_of_charge > 0 and target_rate < 0
        charging = self.state_of_charge < self.MAX_CHARGE and target_rate > 0
        if charging or discharging:
            next_soc = self.state_of_charge + target_rate
            self.state_of_charge = max(0, min(self.MAX_CHARGE, next_soc))
            self.last_rate=target_rate
        else:
            self.last_rate=0
        return self.last_rate
    def sync(self):
        pass
    def get_glyph(self):
        return battery_chars[int(self.state_of_charge / 10)]
    def do_action(self, s):
        try:
            self.net_target=int(s)
        except:
            print(f"Battery action failed: {s}")
            
class Turbine(BaseAppliance):
    def __init__(self):
        self.wind=0
        self.pin=PWM(Pin(18,Pin.OUT),freq=1000,duty=0)
        self.pin.duty(0)
        self.pin2=PWM(Pin(19,Pin.OUT),freq=1000,duty=2)
        self.pin2.duty(0)
    def get_load(self):
        return -self.wind
    def sync(self):
        duty = 0
        if self.wind > 0:
            # 200 to 450 seems to be a sensible range
            duty = 200 + 25 * self.wind
        self.pin2.duty(duty)
    def bump(self):
        self.wind = self.wind + 1
        if self.wind > 5:
            self.wind = 0
        self.sync()
    def do_action(self, s):
        pass
    def deinit(self):
        print('deinit')
        self.pin.duty(0)
        self.pin2.duty(0)
        self.pin.deinit()
        self.pin2.deinit()
        

yellow_led = YellowLed()
rgb = RGBLed()
turbine = Turbine()
battery = Battery()
appliances = {
    b'led': yellow_led,
    b'battery': battery,
    b'turbine': turbine,
    b'heatpump':rgb,
}
loads=[yellow_led, rgb]

REPORT_INTERVAL=1000
start_ticks = ticks_ms()
next_report = ticks_add(start_ticks, REPORT_INTERVAL)

left_button=Pin(25, Pin.IN, Pin.PULL_UP)
right_button=Pin(26, Pin.IN, Pin.PULL_UP)
left_button_last_value=True
right_button_last_value=True

# Let things settle for a minute.
sleep_ms(1000)

try:
    while True:
        # The following combined with the sleep at the bottom of the loop
        # is sufficient to debounce the buttons. We trigger on the falling
        # edge (button first pressed).
        left_button_value = left_button.value()
        if left_button_last_value and not left_button_value:
            rgb.do_action('')
        left_button_last_value = left_button_value
        right_button_value = right_button.value()
        if right_button_last_value and not right_button_value:
            turbine.bump()
        right_button_last_value = right_button_value
        
        if ticks_diff(ticks_ms(), next_report) > 0:
            house_load=0
            for a in loads:
                house_load+=a.get_load()
                a.sync()
            # (negative) turbine load 
            turbine_load = turbine.get_load()
            total_load = house_load + turbine_load
            # The battery computes its load based on
            # net_target and other house load
            battery_load=battery.get_load(total_load)
            total_load+=battery_load
            battery.sync()
            lcd.move_to(0,0)
            lcd.putstr(f'H{house_load:+2} {turbine_char}{turbine_load:+2} {battery.get_glyph()}{battery_load:+2} {grid_char}{total_load:+2}')
            lcd.move_to(0,1)
            lcd.putstr(marquee_text[marquee_offset:marquee_offset+16])
            if marquee_offset + 16 > len(marquee_text):
                lcd.putstr(marquee_text[0:marquee_offset+16 - len(marquee_text)])
            marquee_offset += 1
            if marquee_offset >= len(marquee_text):
                marquee_offset = 0
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
            sleep_ms(100)
except Exception as e:
    turbine.deinit()
    raise e
