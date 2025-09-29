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