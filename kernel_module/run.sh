#!/bin/bash 

insmod comex_swap.ko
rmmod ./comex_swap.ko
tail /var/log/messages

