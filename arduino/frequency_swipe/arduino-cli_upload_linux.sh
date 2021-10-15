#!/usr/bin/sh

# Upload using arduino-cli for linux

# Specify port
port='/dev/ttyACM1'

# Change permissions for the port
sudo chmod a+rw $port

# Compile and upload
arduino-cli compile -e -b arduino:avr:uno
arduino-cli upload -b arduino:avr:uno -p $port

# Use serial communication with putty (for debugging)
putty /dev/ttyACM1 -serial -sercfg 9600,8,n,1,N &
