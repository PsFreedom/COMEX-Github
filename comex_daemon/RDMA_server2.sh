#!/bin/bash

#IP="192.168.34.1"

#for (( i=0; i<$1; i++ ))
#do
   #time sudo nice --10 ./rdma_both $i &
    ./rdma_both 2 2 &
#done
wait
