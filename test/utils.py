import os
import uuid
import time
import shutil

class Nemu():
    def __init__(self):
        self.uuid = uuid.uuid4().hex
        self.test_dir = "/tmp/" + self.uuid

        os.mkdir(self.test_dir)
        with open(self.test_dir + "/nemu.cfg", 'w') as out:
            with open("nemu.cfg.in", 'r') as input:
                for line in input:
                    out.write(line.replace("%%DIR%%", self.uuid))

    def cleanup(self):
        shutil.rmtree(self.test_dir)

    def test_dir(self):
        return self.test_dir

class Tmux():
    def __init__(self, path):
        self.uuid = uuid.uuid4().hex
        cmd = "tmux -L " + self.uuid + " new-session -d " + \
                os.getenv("NEMU_BIN_DIR") + "/nemu --cfg " + path + "/nemu.cfg"
        print(cmd)
        os.popen(cmd)
        time.sleep(0.5)

    def send(self, key):
        cmd = "tmux -L " + self.uuid + " send-keys " + key
        os.popen(cmd)
        time.sleep(0.5)

    def cleanup(self):
        cmd = "tmux -L " + self.uuid + " kill-server"
        os.popen(cmd)
        time.sleep(0.5)
