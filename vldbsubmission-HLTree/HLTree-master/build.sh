#!/bin/bash

g++ -O3 -g -std=c++11 -m64 -D_REENTRANT -fno-strict-aliasing -I./atomic_ops ./nodepref.h -DINTEL -Wno-unused-value -Wno-format  -o ./main-gu-zipfian main-gu-zipfian.c -m64 -lpmemobj -lpmem -lpthread -I/root/install/pcm/src -L/root/install/pcm -lpcm
