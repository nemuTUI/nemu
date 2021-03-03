#!/bin/bash

# Intel Corporation I350 Ethernet Controller SR-IOV VFIO helper
# Udev rules (set your PCI numbers):

#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:00.0", NAME="igb_sr0"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:00.1", NAME="igb_sr1"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:10.0", NAME="igb_vf0.0"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:10.4", NAME="igb_vf0.1"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:11.0", NAME="igb_vf0.2"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:11.4", NAME="igb_vf0.3"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:12.0", NAME="igb_vf0.4"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:12.4", NAME="igb_vf0.5"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:13.0", NAME="igb_vf0.6"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:10.1", NAME="igb_vf1.0"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:10.5", NAME="igb_vf1.1"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:11.1", NAME="igb_vf1.2"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:11.5", NAME="igb_vf1.3"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:12.1", NAME="igb_vf1.4"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:12.5", NAME="igb_vf1.5"
#ACTION=="add", SUBSYSTEM=="net", KERNELS=="0000:04:13.1", NAME="igb_vf1.6"

MSG="used|free|add <pciid>|del <pciid>"

oops()
{
    [ $? != 0 ] && echo "oops..." && exit 1 
}

if [ -z "$1" ]; then
  echo "Usage: $0 ${MSG}"
  exit 0
fi

VENDOR="8086 1520"

case $1 in
  ( used )
    vf_pci=($(ls -la /sys/bus/pci/drivers/vfio-pci \
      | awk '{ if ($9 ~ /^[0-9]+:[0-9]+:[0-9]+\.[0-9]+$/) print $9 }')
          ${vf_pci})
    for iface in ${vf_pci[@]}; do
      echo ${iface}
    done
    ;;

  ( free )
    free_ifs=$(ip -br l | awk '/igb_vf/ {print $1}')
    for iface in ${free_ifs[@]}; do
      pci_num=$(ethtool -i $iface | awk '/bus-info/ {print $NF}')
      echo "${iface} -> ${pci_num}" 
    done
    ;;

  ( add )
    if [ -z "$2" ]; then
      echo "PCI bus number required"
      exit 1
    fi
    echo ${2} > /sys/bus/pci/devices/${2}/driver/unbind
    oops
    echo ${2} > /sys/bus/pci/drivers/vfio-pci/bind
    oops
    vfio_id=$(ls -t1 /dev/vfio | head -1)
    echo "Fix perms to /dev/vfio/${vfio_id}"
    ;;

  ( del )
    if [ -z "$2" ]; then
      echo "PCI bus number required"
      exit 1
    fi
    echo ${2} > /sys/bus/pci/devices/${2}/driver/unbind
    oops
    echo ${2} > /sys/bus/pci/drivers/igbvf/bind
    oops
    ;;

  ( * )
    echo "Usage: $0 ${MSG}" 1>&2
    exit 1
    ;;
esac
