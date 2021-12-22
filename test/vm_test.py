import unittest
from utils import Nemu
from utils import Tmux

class TestVm(unittest.TestCase):
    def test_install(self):
        """Test VM install"""
        nemu = Nemu()
        self.assertTrue(1 == 1)
        tmux = Tmux(nemu.test_dir)
        tmux.send("I")
        tmux.send("testvm")
        for i in range (3):
            tmux.send("Down")
        tmux.send("256")
        tmux.send("Down")
        tmux.send("10")
        for i in range (3):
            tmux.send("Down")
        tmux.send("/dev/null.iso")
        tmux.send("Enter")
        nemu.cleanup()
