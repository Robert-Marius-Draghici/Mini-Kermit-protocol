#!/bin/bash

SPEED=10
DELAY=10
LOSS=5
CORRUPT=20
#FILES=(file1.txt file2.txt file3.txt) 
FILES=(fisier1.bin fisier2.bin ) 

killall link
killall recv
killall send

./link_emulator/link speed=$SPEED delay=$DELAY loss=$LOSS corrupt=$CORRUPT &> /dev/null &
sleep 1
 ./kreceiver &
sleep 1

./ksender "${FILES[@]}"

echo "==========================="
for file in "${FILES[@]}"
do
	diff -s $file recv_$file
done
echo "==========================="
