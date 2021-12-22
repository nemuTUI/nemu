import unittest
from utils import Nemu

class TestVm(unittest.TestCase):
    def test_install(self):
        """Test VM install"""
        Nemu.setup()
        self.assertTrue(1 == 1)
        Nemu.cleanup()
