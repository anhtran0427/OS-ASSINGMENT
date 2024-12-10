#!/bin/bash

SRC_FILE=$(ls input -p |grep -v / )
echo $SRC_FILE

for fsrc in $SRC_FILE
do 
  ./os $fsrc >"user/$fsrc.txt"
done
