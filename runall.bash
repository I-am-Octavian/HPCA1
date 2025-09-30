#!/bin/bash

sudo nice -n -20 taskset -c 7 perf stat -e "fp_ret_sse_avx_ops.all,l1_data_cache_fills_all,ls_dc_accesses" ./ijk
sudo nice -n -20 taskset -c 7 perf stat -e "fp_ret_sse_avx_ops.all,l1_data_cache_fills_all,ls_dc_accesses" ./kij
sudo nice -n -20 taskset -c 7 perf stat -e "fp_ret_sse_avx_ops.all,l1_data_cache_fills_all,ls_dc_accesses" ./tiledijk
sudo nice -n -20 taskset -c 7 perf stat -e "fp_ret_sse_avx_ops.all,l1_data_cache_fills_all,ls_dc_accesses" ./tiledkij

