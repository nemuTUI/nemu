#!/bin/bash

ver_str=$(uname -r | sed -r 's/(([0-9]+.){2}[0-9]+).*/\1/')
major=$(echo ${ver_str} | cut -f 1 -d '.')
minor=$(echo ${ver_str} | cut -f 2 -d '.')
patch=$(echo ${ver_str} | cut -f 3 -d '.')
ver_num=$((($major << 16) + ($minor << 8) + $patch))

echo -n $ver_num

#132641 2.6.33
