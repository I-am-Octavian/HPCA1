# HPCA

## Processor details

2 FMA (float * float + float)
2 FADD
Both 256 bit wide(4 doubles)

Thus total flops = 4.2(Ghz) * ((2\*2\*4) + (4*2)) = 100.8 GFLOPS

## Memory speeds

### For Caches

Bandwidth = Bytes per cycle * Freq of CPU

L1 bandwidth = 64 * 4.2 = 268.8 GBPS\
L2 bandwidth = 32 * 4.2 = 134.4 GBPS\
L3 bandwidth = 24 * 4.2 = 100.8 GBPS\

### Memory

Each transfer is of 8 bytes in DDR4

RAM (3200 MT/s) = 3200 * 8 * 2 = 51.2 GBPS

# Problem 1(a)

To run problem 1, modify Main.cpp to use the approprite matmul variant desired and using:
```
bash runall.bash
```

We used Clang-14 compiler with O2 optimizations in all problems.

# Problem 1(b)

1. Clone the gapbs
2. Change the Makefile to not use OpenMP while compiling
3. Copy the code in bfs_treps.cc to the bfs.cc in gapbs
4. Run the following command:
```
sudo nice -n -20 taskset -c 7 ./bfs -g 25 -n 1
```
This prints the treps data to the terminal

Plotting code in plot-1.ipynb file


# Problem 2(a)

Run the command in the cloned gapbs directory:
```
sudo nice -n -20 taskset -c 7 perf stat -e "instructions,cycles" -I 500 --no-big-num -o ipc_data ./bfs -g 25 -n 30
```

Clone rohdina benchmark. Compile the OMP versions using 
```
make OMP
```

then
```
cd bin/linux/omp
```

Then run 
```
sudo nice -n -20 taskset -c 7 perf stat -e "instructions,cycles" -I 500 --no-big-num -o ipc_data_lud ./lud_omp -n 1 -s 32000
```

# Problem 2(b)

For regression data:-

BFS:
```
sudo nice -n -20 taskset -c 7 perf stat -e "instructions,cycles,branch-misses,cache-misses,l1_dtlb_misses,l2_dtlb_misses,l2_itlb_misses" -I 10 --no-big-num -o 2b_data./bfs -g 25 -n 30
```

LUD:
```
sudo nice -n -20 taskset -c 7 perf stat -e "instructions,cycles,branch-misses,cache-misses,l1_dtlb_misses,l2_dtlb_misses,l2_itlb_misses" -I 10 --no-big-num -o 2b_data_lud_new ./lud_omp -n 1 -s 32000
```

To plot the CPI stack, we collect the same data at an interval of 100ms instead of 10ms
