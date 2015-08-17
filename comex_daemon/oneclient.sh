#!/bin/bash

IP="192.168.14.2"

#for (( i=0; i<$1; i++ ))
#do
    #{ time sudo nice --10 ./rdma_both $IP $i;} 2>&1 | tee -a /export/cxd9974/out$2/$i.txt &
    ./rdma_both 1 1 $IP 2
#done
wait
