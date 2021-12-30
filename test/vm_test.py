import unittest
import subprocess
from utils import Nemu
from utils import Tmux

class TestVm(unittest.TestCase):
    def test_install(self):
        """Test VM install"""
        self.maxDiff = None
        tmpl = "%%PATH%%/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /dev/null.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/%%UUID%%/testvm/testvm_a.img \
-m 256 -enable-kvm -cpu host -M %%MTYPE%% -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,netdev=netdev0 -netdev \
tap,ifname=testvm_eth0,script=no,downscript=no,id=netdev0,\
vhost=on -pidfile /tmp/%%UUID%%/testvm/qemu.pid -qmp \
unix:/tmp/%%UUID%%/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        tmpl = tmpl.replace("%%UUID%%", nemu.uuid)
        tmpl = tmpl.replace("%%PATH%%", nemu.qemu_bin())
        tmpl = tmpl.replace("%%MTYPE%%", nemu.qemu_mtype())
        rc = tmux.send("I")
        self.assertTrue(0 == rc)
        rc = tmux.send("testvm")
        self.assertTrue(0 == rc)
        rc = tmux.send("Down", 3)
        self.assertTrue(0 == rc)
        rc = tmux.send("256")
        self.assertTrue(0 == rc)
        rc = tmux.send("Down")
        self.assertTrue(0 == rc)
        rc = tmux.send("10")
        self.assertTrue(0 == rc)
        rc = tmux.send("Down", 3)
        self.assertTrue(0 == rc)
        rc = tmux.send("/dev/null.iso")
        self.assertTrue(0 == rc)
        rc = tmux.send("Enter")
        self.assertTrue(0 == rc)
        tmux.send("q")
        self.assertTrue(0 == rc)
        self.assertEqual(nemu.result("testvm"), tmpl)
        nemu.cleanup()
