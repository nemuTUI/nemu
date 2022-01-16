import unittest
import subprocess
from utils import Nemu
from utils import Tmux

class TestVm(unittest.TestCase):
    def test_01_install(self):
        """Test VM install"""
        self.maxDiff = None
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 3], ["/dev/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /dev/null.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/{nemu.uuid}/testvm/testvm_a.img \
-m 256 -enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,netdev=netdev0 -netdev \
tap,ifname=testvm_eth0,script=no,downscript=no,id=netdev0,\
vhost=on -pidfile /tmp/{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()

    def test_02_edit_base_settings(self):
        """Test VM edit base settings"""
        self.maxDiff = None
        nemu = Nemu()
        tmux_init = Tmux()
        tmux_init.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 3], ["/dev/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux_init.send(*key)
            self.assertTrue(0 == rc)

        tmux_edit = Tmux()
        tmux_edit.setup(nemu.test_dir)
        keys_edit = [["e"], ["10"], ["Down"], ["BSpace", 3], ["512"],
                ["Down"], ["Right"], ["Down"], ["Right"], ["Down"], ["Left"],
                ["2"], ["Down"], ["Right"], ["Enter"], ["q"]]
        for key in keys_edit:
            rc = tmux_edit.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /dev/null.iso -drive \
node-name=hd0,media=disk,if=ide,file=\
/tmp/{nemu.uuid}/testvm/testvm_a.img \
-m 512 -smp 10 -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,netdev=netdev0 -netdev \
tap,ifname=testvm_eth0,script=no,downscript=no,id=netdev0,\
vhost=on -device virtio-net-pci,mac=de:ad:be:ef:00:02,netdev=netdev1 \
-netdev tap,ifname=testvm_eth1,script=no,downscript=no,id=netdev1 \
-pidfile /tmp/{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()
