# loop-device

Linux kernel device character driver that creates /dev/loop device that loops the input into /tmp/output file within a hex format (16 bytes per row). In other words, in the case of running run: 

cat my_file.bin > /dev/loop_dev

hexdump my_file.bin > /tmp/output1

diff /tmp/output /tmp/output1    // No diff

#how to run
cmd > make // ruild the code
cmd > insmod loop_dev.ko // Insert the kernel module
and check the major number for the device:
cmd > dmesg
The last log line should be "Registered a major number N". The N is the device number.
To complete device creation replace 240 with N and run last command
cmd > mknod /dev/loop_dev c N 0
