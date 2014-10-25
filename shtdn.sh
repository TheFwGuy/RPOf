#! /bin/bash
#
##
# @file shtdn.sh
# @brief Shutdown script
# @author Stefano Bodini
#
# More info about the GPIO: http://elinux.org/RPi_Low-level_peripherals
# Derived from https://github.com/g0to/misc_scripts/blob/master/raspi_gpio_actions.sh
#
# # GPIO numbers should be from this list
# 0, 1, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 21, 22, 23, 24, 25
# Note that the GPIO numbers that you program here refer to the pins
# of the BCM2835 and *not* the numbers on the pin header. 
# So, if you want to activate GPIO7 on the header you should be 
# using GPIO4 in this script. Likewise if you want to activate GPIO0
# on the header you should be using GPIO17 here.
#

# Uses GPIO18 as shutdown input signal
# Uses GPIO17 as shutdown feedback signal

SHUTDOWN_INPUT="18"
SHUTDOWN_FEEDBACK="17"

if [[ $EUID -ne 0 ]]; then
   echo "Error: This script must be run as root" >&2
   exit 1
fi

echo "$SHUTDOWN_FEEDBACK" > /sys/class/gpio/export
echo "out" > /sys/class/gpio/gpio"$SHUTDOWN_FEEDBACK"/direction
echo "1" > /sys/class/gpio/gpio"$SHUTDOWN_FEEDBACK"/value

echo "$SHUTDOWN_INPUT" > /sys/class/gpio/export
echo "in" > /sys/class/gpio/gpio"$SHUTDOWN_INPUT"/direction

while ( true )
do
    # check if the pin is connected to GND and, if so, halt the system
    if [ $(</sys/class/gpio/gpio"$SHUTDOWN_PIN"/value) == 0 ]
    then
        echo "$SHUTDOWN_INPUT" > /sys/class/gpio/unexport
        shutdown -h now "System halted by a GPIO action"
    fi
    
    sleep 60
done