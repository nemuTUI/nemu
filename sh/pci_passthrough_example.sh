#!/bin/bash

VENDOR="8086 1520"
PCIID="0000:04:10.0"

# do once at boot time:
echo 1 > /sys/module/vfio_iommu_type1/parameters/allow_unsafe_interrupts
#  if using Single Root I/O Virtualization (SR-IOV)
#  esure SR-IOV and VT-d are enabled in BIOS.
#  Enable IOMMU in Linux by adding intel_iommu=on to the kernel parameters,
#  I prefer CONFIG_INTEL_IOMMU_DEFAULT_ON=y in kernel config
#  Then create VFs:
VFS_NUM=$(cat /sys/class/net/igb_sr1/device/sriov_totalvfs)
echo $VFS_NUM > /sys/class/net/igb_sr1/device/sriov_numvfs
# done

echo ${PCIID}  > /sys/bus/pci/devices/${PCIID}/driver/unbind
echo ${VENDOR} > /sys/bus/pci/drivers/vfio-pci/new_id
chown user /dev/vfio/20

# add extra arg to nEMU VM:
# -device vfio-pci,host=04:10.0
