#!/bin/sh
set -e

if [ -z "$1" ] ; then
	echo Usage: $0 {javascript file}
	exit 1
fi

if [ ! -f ./node_modules/.bin/microvium ] ; then
	if ! type npm >/dev/null 2>&1 ; then
		if type apt >/dev/null 2>&1 ; then
			echo npm is not installed.  Trying to install it from apt.
			echo If you are not using the devcontainer, this may fail.
			sudo apt update
			sudo apt install nodejs make gcc g++
		else
			echo npm is not installed.  Please install it.
			exit 1
		fi
	fi
	npm install microvium
fi

if [ -z ${CHERIOT_USB_DEVICE} ]; then
  echo "CHERIOT_USB_DEVICE is not set, this must be set to the path of the USB device for the CHERIoT board's UART"
  echo "If you are running in a dev container and cannot forward the USB device, you can set this to a file and then write the output to that file to the USB device on the host"
  exit 1
fi

echo Loading JavaScript:
cat $1

./node_modules/.bin/microvium -s /dev/null --output-bytes $1 > ${CHERIOT_USB_DEVICE}
