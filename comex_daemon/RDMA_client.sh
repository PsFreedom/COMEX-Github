#!/bin/bash

IP2="192.168.34.2"
IP="192.168.13.1"

#for (( i=0; i<$1; i++ ))
#do
    #{ time sudo nice --10 ./rdma_both $IP $i;} 2>&1 | tee -a /export/cxd9974/out$2/$i.txt &
    ./rdma_both 2 0 $IP 0 $IP2 0
    #./rdma_both 1 1 $IP 0
#done
wait
