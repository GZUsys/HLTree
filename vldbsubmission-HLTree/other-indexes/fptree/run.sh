#!/bin/bash

for skewness in 99 #0 90 #99
do

for thread_num in 1 #4 8 12 16 20 24 28 32 36 # 1 4 8 10 16 20 25 32 40 50 64 
do

echo thread_num = ${thread_num} skewness = ${skewness}
./main-gu-zipfian -t ${thread_num} 
#> result/u5_0.${skewness}_${thread_num}.txt
rm /mnt/ext4/fastfair/main-pool
#rm /mnt/ext4/fastfair/pool-*
#rm /mnt/ext4/fastfair/pool-*

sleep 1
done
done


