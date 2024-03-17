#!/bin/bash

# Remove the kernel module
rmmod loop_device.ko

# Insert the kernel module
insmod loop_device.ko

# Remove the device node
rm /dev/loop_device

# Create a new device node with major number 240 and minor number 0
mknod /dev/loop_device c 240 0

