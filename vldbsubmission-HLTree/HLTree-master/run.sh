#!/bin/bash
for thread_num in 1 #1 2 4 8 16 32 64 128 #1 4 8 12 16 20 24 28 32 36 #1 2 4 8 16 32 64 128
do
for operate in 1 #2
do

echo operate = ${operate} thread_num = ${thread_num}
./main-gu-zipfian -t ${thread_num} -d ${operate} #> result/u5_0.${skewness}_${thread_num}.txt
rm /mnt/ext4/utree/pool-main
sleep 3
done
done

