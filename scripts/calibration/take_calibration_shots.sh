#!/bin/bash
#
# Takes numPics pictures, each about 1 second apart
# Before running, make sure the system is setup to NOT require
# external triggering (particularly for camera 2).
# If running in single-pi mode, the  /boot/firmware/config.txt
# file should have the following lines commented out:
#        dtoverlay=imx296,cam0
#        dtoverlay=imx296,sync-sink
#
# In addition, if using camera 2, the IR filter must be removed because
# there will be no strobing.
#

cam_tuning_file=$(./get_tuning_file.sh)

if [[ $# -lt 3 ]];  then  echo Format: "$0" baseFName numPics camera_number\(0 or 1\); exit;  fi
	echo "====================== WILL START IN 5 SECONDS ======================="
sleep 5
for((m=1; m<=${2}; ++m))
do
	echo "====================== GET READY FOR ANOTHER PICTURE - No. $m ======================="
	sleep 2
	rpicam-still --camera ${3} --timeout 1000 --gain 1.3 --nopreview --verbose 0 --tuning-file=$cam_tuning_file --shutter 11000 --width 1456 --height 1088 --denoise cdn_off -o ${1}${m}.png
done
