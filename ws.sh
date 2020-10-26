#!/bin/bash
#
# This code target to complement the bash PoC in miditi.sh
#
TMP_DIR=tmp
[ -e /dev/shm ] && TMP_DIR=/dev/shm/casio-lk-250
while [ -e "$TMP_DIR" ];
	do cat $TMP_DIR/ws
done
