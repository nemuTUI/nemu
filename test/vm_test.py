import unittest
import subprocess
from utils import Nemu
from utils import Tmux

class TestVm(unittest.TestCase):
    def test_install(self):
        """Test VM install"""
        nemu = Nemu()
        tmux = Tmux()
        tmux.setup(nemu.test_dir)
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
        #rc = tmux.cleanup()
        self.assertTrue(0 == rc)
        print(nemu.result("testvm", 0.1))

        nemu.cleanup()
