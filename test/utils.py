import os

class Nemu():
    def setup():
        print("setup")
        print(os.getenv("NEMU_BIN_DIR"))

    def cleanup():
        print("cleanup")
