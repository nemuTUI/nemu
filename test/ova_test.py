import os
import unittest
from utils import Nemu
from utils import Tmux

class TestOVA(unittest.TestCase):
    def test_01_install(self):
        """* OVA import"""
        self.maxDiff = None
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        keys = [["O"], [os.getenv("NEMU_TEST_DIR") + "/testdata/dsl.ova"],
                ["Down", 2], ["dsl"], ["Enter"], ["q"]]

        for key in keys:
            rc = tmux.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize \
-drive node-name=hd0,media=disk,if=virtio,file=/tmp/nemu_{nemu.uuid}/dsl/Damn_Small_Linux-disk1.vmdk \
-m 256 -enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-pidfile /tmp/nemu_{nemu.uuid}/dsl/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/dsl/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("dsl"), expected)
        nemu.cleanup()
