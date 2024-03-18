#!/bin/bash

# Remove the kernel module
rmmod loop_dev.ko

# Insert the kernel module
insmod loop_dev.ko

# Remove the device node
rm /dev/loop_dev

# Create a new device node with major number 240 and minor number 0
mknod /dev/loop_dev c 240 0

