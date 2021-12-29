import os
import uuid
import time
import shutil
import subprocess

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

    def result(self, vm, wait = 0):
        # wait for nemu stops
        dbfile = "/tmp/" + self.uuid + "/nemu.pid"
        wait_max = 100 # wait for 10 seconds max
        wait_cur = 0
        while os.path.exists(dbfile):
            time.sleep(0.1)
            wait_cur += 1
            if wait_max >= wait_cur:
                break
        sub = subprocess.run([os.getenv("NEMU_BIN_DIR") + "/nemu",
            "--cfg", "/tmp/" + self.uuid + "/nemu.cfg", "--cmd", vm],
            capture_output=True)
        return sub.stdout.decode('utf-8')

    def test_dir(self):
        return self.test_dir

class Tmux():
    def __init__(self):
        self.uuid = uuid.uuid4().hex

    def setup(self, path):
        sub = subprocess.run(["tmux", "-L", self.uuid,
                "new-session", "-d", os.getenv("NEMU_BIN_DIR") + "/nemu",
                "--cfg", path + "/nemu.cfg"])

        # wait for nemu starts
        dbfile = "/tmp/" + self.uuid + "/nemu.pid"
        wait_max = 100 # wait for 10 seconds max
        wait_cur = 0
        while not os.path.exists(dbfile):
            time.sleep(0.1)
            wait_cur += 1
            if wait_max >= wait_cur:
                break

        return sub.returncode

    def send(self, key, count = 1):
        rc = 0

        for i in range(count):
            sub = subprocess.run(["tmux", "-L", self.uuid, "send-keys", key])
            rc += sub.returncode

        return rc

    def cleanup(self):
        sub = subprocess.run(["tmux", "-L", self.uuid, "kill-server"])

        return sub.returncode
