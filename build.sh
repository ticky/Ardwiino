#!/bin/bash
cd build
cmake .. -DBOARD=waveshare_rp2040_zero
make -j`nproc`