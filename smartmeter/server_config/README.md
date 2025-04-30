# Server configuration notes for RPI500 used with smart meter demo

## Networking

RPI config tool ethernet manual config IP4 10.0.0.10 netmask 255.0.0.0
Tick use only for traffic on its network.

## Install DHCP/ DNS server dnsmasq

dnsmasq
/etc/dnsmasq.conf
interface=eth0
dhcp-range=10.0.0.11,10.254.254.254,12h
addn-hosts=/etc/demo.hosts

/etc/hosts
10.0.0.10   cheriot.demo

# Pretend to CHERIoT that RPi is pool.ntp.org for sntp
/etc/demo.hosts
10.0.0.10   pool.ntp.org

## Configure ntpd
sudo apt install ntpsec

/etc/ntpsec/ntp.conf
# tell NTP to work in offline mode after 10s without peers
tos orphan 15 orphanwait 10

# my router was advertising an ntp server but this was overriding ntp.conf
# and preventing synchronisation for some reason. Stop this.
/etc/default/ntpsec
IGNORE_DHCP=yes

# mosquitto from debian might be OK but I added latest from mosquitto.org repo
cd /etc/apt/souces.list.d/
sudo wget http://repo.mosquitto.org/debian/mosuitto-bookworm.list
cd /etc/apt/keyrings
sudo wget http://repo.mosqutto.org/debian/mosquitto-repo.gpg
sudo apt update
sudo apt install mosquitto

/etc/mosquitto/conf.d/demo.conf
# unencrypted listener for local websockets
listener 8080 10.0.0.10
protocol websockets
socket_domain ipv4
log_type all
allow_anonymous true
connection_messages true

# mqtt listener with self-signed demo cert and key
listener 8883 10.0.0.10
protocol mqtt
tls_keyform pem
keyfile /var/lib/mosquitto/key.pem
certfile /var/lib/mosquitto/cert.pem
log_type all
allow_anonymous true
connection_messages true

cp cheriot.demo.crt /var/lib/mosquitto/cert.pem
cp cheriot.demo.key /var/lib/mosquitto/key.pem

# Get recent node from nodesource
curl -fsSL https://deb.nodesource.com/setup_23.x -o nodesource_setup.sh
sudo -E bash nodesource_setup.sh
sudo apt-get install -y nodejs

# Run up frontend server as per ../frontends/README.md
npm install
./node_modules/.bin/rollup --config ./static.in/user-editor.rollup-config.mjs
node ./server.js

# On MacOS
brew install dnsmasq mosquitto ntp
sudo /opt/homebrew/opt/dnsmasq/sbin/dnsmasq --keep-in-foreground -C dnsmasq.conf
/opt/homebrew/opt/mosquitto/sbin/mosquitto -v -c mosquitto.conf
sudo /opt/homebrew/sbin/ntpd -d -c ntp.conf
picocom /dev/tty.usbserial-LN28282 -b 921600 --imap=lfcrlf
# /etc/hosts
10.0.0.10 cheriot.demo

# Generating ssl cert. and bear SSL trust anchor header
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -days 3650 -nodes -keyout cheriot.demo.key -out cheriot.demo.crt -subj "/CN=cheriot.demo"
brssl ta cheriot.demo.crt > cheriot/cheriot.demo.h

# Buidling smartmeter/cheriot
xmake config --sdk=~/llvm-project/Build/install --broker-host=cheriot.demo --broker-anchor=cheriot.demo.h --IPv6=n --unique-id=SCIHouse
xmake
xmake run

