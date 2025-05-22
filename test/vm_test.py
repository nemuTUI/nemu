import unittest
from utils import Nemu
from utils import Tmux
from pathlib import Path

class TestVm(unittest.TestCase):
    def test_01_install(self):
        """* VM install"""
        self.maxDiff = None
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img \
-m 256 -enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()

    def test_02_edit_base_settings(self):
        """* Edit main settings"""
        self.maxDiff = None
        nemu = Nemu()
        tmux_init = Tmux()
        tmux_init.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
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
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso -drive \
node-name=hd0,media=disk,if=ide,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img \
-m 512 -smp 10 -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-device virtio-net-pci,mac=de:ad:be:ef:00:02,id=dev-deadbeef0002,netdev=net-deadbeef0002 \
-netdev tap,ifname=testvm_eth1,script=no,downscript=no,id=net-deadbeef0002,vhost=on \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()

    def test_03_clone(self):
        """* Clone VM"""
        self.maxDiff = None
        nemu = Nemu()
        tmux_init = Tmux()
        tmux_init.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux_init.send(*key)
            self.assertTrue(0 == rc)

        tmux_edit = Tmux()
        tmux_edit.setup(nemu.test_dir)
        keys_edit = [["l"], ["Enter"], ["q"]]
        for key in keys_edit:
            rc = tmux_edit.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm-clone/testvm-clone_a.img \
-m 256 -enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:02,id=dev-deadbeef0002,netdev=net-deadbeef0002 \
-netdev user,id=net-deadbeef0002 \
-pidfile /tmp/nemu_{nemu.uuid}/testvm-clone/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm-clone/qmp.sock,server,nowait -vga qxl \
-spice port=5901,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm-clone"), expected)
        nemu.cleanup()

    def test_04_edit_viewer_settings(self):
        """* Edit viewer settings"""
        self.maxDiff = None
        nemu = Nemu()
        tmux_init = Tmux()
        tmux_init.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux_init.send(*key)
            self.assertTrue(0 == rc)

        tmux_edit = Tmux()
        tmux_edit.setup(nemu.test_dir)
        keys_edit = [["v"], ["Right"], ["Down"], ["BSpace", 4], ["12345"],
                ["Down"], ["/dev/pts/7"], ["Down"], ["/tmp/ntty0"], ["Down"], ["Right"],
                ["Down"], ["Right"], ["Enter"], ["q"]]
        for key in keys_edit:
            rc = tmux_edit.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img \
-m 256 -enable-kvm -cpu host -M {nemu.qemu_mtype()} -device usb-tablet,bus=usbbus.0 \
-chardev socket,path=/tmp/ntty0,server,nowait,id=socket_testvm -device \
isa-serial,chardev=socket_testvm -chardev tty,path=/dev/pts/7,id=tty_testvm \
-device isa-serial,chardev=tty_testvm -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vnc :6445"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()

    def test_05_share_9pfs(self):
        """* Share host filesystem"""
        self.maxDiff = None
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux.send(*key)
            self.assertTrue(0 == rc)

        tmux_edit = Tmux()
        tmux_edit.setup(nemu.test_dir)
        keys_edit = [["h"], ["Right"], ["Down"], ["/tmp"], ["Down"],
                ["share"], ["Enter"], ["q"]]
        for key in keys_edit:
            rc = tmux_edit.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img \
-m 256 -fsdev local,security_model=none,id=fsdev0,path=/tmp \
-device virtio-9p-pci,fsdev=fsdev0,mount_tag=share \
-enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()

    def test_06_additional_drive(self):
        """* Add/Delete additional drive"""
        self.maxDiff = None
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux.send(*key)
            self.assertTrue(0 == rc)

        tmux_add = Tmux()
        tmux_add.setup(nemu.test_dir)
        keys_add = [["a"], ["1"], ["Down"], ["Left"], ["Down", 2],
                ["Right"], ["Enter"], ["q"]]
        for key in keys_add:
            rc = tmux_add.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso \
-drive node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img \
-device virtio-scsi-pci,id=scsi \
-drive node-name=hd1,media=disk,if=none,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_b.img,discard=unmap,detect-zeroes=unmap \
-device scsi-hd,drive=hd1 -m 256 \
-enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)

        tmux_del = Tmux()
        tmux_del.setup(nemu.test_dir)
        keys_del = [["V"], ["Enter"], ["q"]]
        for key in keys_del:
            rc = tmux_del.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso \
-drive node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img -m 256 \
-enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()

    def test_07_boot_settings(self):
        """* Edit boot settings"""
        self.maxDiff = None
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux.send(*key)
            self.assertTrue(0 == rc)

        tmux_edit = Tmux()
        tmux_edit.setup(nemu.test_dir)
        keys_edit = [["b"], ["Right"], ["Down"], ["BSpace", 4], ["_edit.iso"],
                ["Down"], ["/tmp/efi"], ["Down"], ["/tmp/flash"], ["Down"],
                ["/tmp/kern"], ["Down"], ["cmd"], ["Down"], ["/tmp/initrd"],
                ["Down"], ["12345"], ["Down"], ["Right"], ["Enter"], ["q"]]
        for key in keys_edit:
            rc = tmux_edit.send(*key)
            self.assertTrue(0 == rc)

        Path('/tmp/null_edit.iso').touch()

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -cdrom /tmp/null_edit.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img -m 256 \
-enable-kvm -cpu host -bios /tmp/efi -pflash /tmp/flash -M {nemu.qemu_mtype()} \
-kernel /tmp/kern -append cmd -initrd /tmp/initrd -gdb tcp::12345 -S \
-device virtio-net-pci,mac=de:ad:be:ef:00:01,id=dev-deadbeef0001,netdev=net-deadbeef0001 \
-netdev user,id=net-deadbeef0001 \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()

    def test_08_network(self):
        """* Network settings"""
        self.maxDiff = None
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
        keys = [["I"], ["testvm"], ["Down", 3], ["256"], ["Down"],
                ["10"], ["Down", 4], ["/tmp/null.iso"], ["Enter"], ["q"]]
        for key in keys:
            rc = tmux.send(*key)
            self.assertTrue(0 == rc)

        tmux_edit = Tmux()
        tmux_edit.setup(nemu.test_dir)
        keys_edit = [["i"], ["Enter"], ["End"], ["BSpace", 4], ["new"],
                ["Down"], ["Left"], ["Down"], ["BSpace"], ["2"],
                ["Down", 2], ["Left"], ["Down", 4], ["tcp::22-:2222"],
                ["Down"], ["/share"], ["Enter"], ["q", 2]]
        for key in keys_edit:
            rc = tmux_edit.send(*key)
            self.assertTrue(0 == rc)

        expected = f"{nemu.qemu_bin()}/qemu-system-x86_64 -daemonize -usb -device \
qemu-xhci,id=usbbus -boot d -cdrom /tmp/null.iso -drive \
node-name=hd0,media=disk,if=virtio,file=\
/tmp/nemu_{nemu.uuid}/testvm/testvm_a.img \
-m 256 -enable-kvm -cpu host -M {nemu.qemu_mtype()} -device \
vmxnet3,mac=de:ad:be:ef:00:02,id=dev-deadbeef0002,netdev=net-deadbeef0002 \
-netdev user,id=net-deadbeef0002,hostfwd=tcp::22-:2222,smb=/share \
-pidfile /tmp/nemu_{nemu.uuid}/testvm/qemu.pid -qmp \
unix:/tmp/nemu_{nemu.uuid}/testvm/qmp.sock,server,nowait -vga qxl \
-spice port=5900,disable-ticketing=on"
        self.assertEqual(nemu.result("testvm"), expected)
        nemu.cleanup()
