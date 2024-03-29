#!/bin/bash

# Remove the kernel module
# This line can fail if the kernel module is not loaded yet, it will not impact the final result
rmmod loop_dev.ko

# Insert the kernel module
insmod loop_dev.ko

# Remove the device node
# This line can fail if the device node is not created, it will not impact the final result
rm /dev/loop_dev

# Create a new device node with major number 240 and minor number 0
# The Major number could be different for the system, if this line fails, need to check the major number
mknod /dev/loop_dev c 240 0

# major number could be found in kernel messages, run the following command:  
# cmd > dmesg
# Last log line should be "Registered a major number N". The N is the device number.
# To complete device creation replace 240 with N and run mknod command again


