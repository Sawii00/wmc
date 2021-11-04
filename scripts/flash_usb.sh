#!/bin/sh

STM32_Programmer_CLI -c port=usb1 pid=0xDF11 vid=0x0483 -e all
STM32_Programmer_CLI -c port=usb1 pid=0xDF11 vid=0x0483 -w ./build/wmc.bin 0x08000000
