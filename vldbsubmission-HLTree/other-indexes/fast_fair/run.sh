#!/bin/bash

for skewness in 99 #0 90 99
do

for thread_num in 1 4 8 12 16 20 24 28 32 36 
do

echo thread_num = ${thread_num} skewness = ${skewness} 
./main-gu-zipfian -t ${thread_num} #> result_0706/u100_0.${skewness}_${thread_num}.txt
#rm /mnt/ext4/fastfair/pool-*
rm /mnt/ext4/fastfair/main_pool
sleep 1
done
done


