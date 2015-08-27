#!/bin/sh

rm -f benchmark lockBench
gcc -o benchmark Benchmark.c
gcc -o lockBench LockBench.c

