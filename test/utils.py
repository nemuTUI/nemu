import os
import uuid
import time
import shutil
import subprocess

class Nemu():
    def __init__(self):
        self.uuid = uuid.uuid4().hex
        self.test_dir = "/tmp/nemu_" + self.uuid
        self.pidfile = self.test_dir + "/nemu.pid"

        os.mkdir(self.test_dir)
        with open(self.test_dir + "/nemu.cfg", 'w') as out:
            with open("nemu.cfg.in", 'r') as input:
                for line in input:
                    out.write(line.replace("%%DIR%%", self.uuid))

    def cleanup(self):
        shutil.rmtree(self.test_dir)

    def result(self, vm):
        # wait for nemu stops
        wait_max = 300 # wait for 30 seconds max
        wait_cur = 0
        while os.path.exists(self.pidfile):
            time.sleep(0.1)
            wait_cur += 1
            if wait_cur == wait_max:
                break
        sub = subprocess.run([os.getenv("NEMU_BIN_DIR") + "/nemu",
            "--cfg", "/tmp/nemu_" + self.uuid + "/nemu.cfg", "--cmd", vm],
            capture_output=True)
        return sub.stdout.decode('utf-8')

    def test_dir(self):
        return self.test_dir

    def uuid(self):
        return self.uuid

    @staticmethod
    def qemu_bin():
        return os.getenv("QEMU_BIN_DIR")

    @staticmethod
    def target_os():
        return os.getenv("NEMU_TARGET_OS")

    def qemu_mtype(self):
        cmd = [os.getenv("QEMU_BIN_DIR") + "/qemu-system-x86_64", "-M", "help"]
        sub = subprocess.run(cmd, capture_output=True)
        out = sub.stdout.decode('UTF-8')
        out = str(out).split('\n')

        for line in out:
            if ' (default)' in line:
                mtype = line.split()[0]
                break

        return mtype

class Tmux():
    def __init__(self):
        self.uuid = uuid.uuid4().hex

    def setup(self, path):
        # wait for nemu stops if running
        pidfile = path + "/nemu.pid"
        wait_max = 100 # wait for 10 seconds max
        wait_cur = 0
        while os.path.exists(pidfile):
            time.sleep(0.1)
            wait_cur += 1
            if wait_max >= wait_cur:
                break

        sub = subprocess.run(["tmux", "-f", "tmux.conf", "-L", self.uuid,
                "new-session", "-d", os.getenv("NEMU_BIN_DIR") + "/nemu",
                "--cfg", path + "/nemu.cfg"])

        # wait for nemu starts
        wait_cur = 0
        while not os.path.exists(pidfile):
            time.sleep(0.1)
            wait_cur += 1
            if wait_max >= wait_cur:
                break

        return sub.returncode

    def dump_screen(self, log_path):
            subprocess.run(["tmux", "-L", self.uuid, "capture-pane"])
            sub = subprocess.run(["tmux", "-L", self.uuid, "show-buffer"],
                    capture_output=True)
            subprocess.run(["tmux", "-L", self.uuid, "delete-buffer"])

            with open(log_path, 'w') as log:
                log.write(sub.stdout.decode('utf-8'))

    def send(self, key, count = 1):
        rc = 0

        for i in range(count):
            sub = subprocess.run(["tmux", "-L", self.uuid, "send-keys", key])
            rc += sub.returncode

        return rc

    def cleanup(self):
        sub = subprocess.run(["tmux", "-L", self.uuid, "kill-server"])

        return sub.returncode
