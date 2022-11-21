# How to build nEMU on Linux/FreeBSD.

* Get sources
```sh
$ git clone https://github.com/nemuTUI/nemu
```

* Build with ninja and all features enabled
```sh
$ cd nemu
$ mkdir build && cd build
$ cmake -G Ninja .. -DNM_WITH_NETWORK_MAP=ON -DNM_WITH_DBUS=ON -DNM_WITH_REMOTE=ON
$ cmake --build .
```

# How to build nEMU on MacOSX.

* Get sources
```sh
$ git clone https://github.com/nemuTUI/nemu
```

* Install dependencies
```sh
$ brew install cmake sqlite json-c openssl libarchive qemu tigervnc-viewer
```

* Build (openssl and libarchive versions may differ)
```sh
$ cd nemu
$ mkdir build && cd build
$ cmake ../ -DOPENSSL_ROOT_DIR=/usr/local/Cellar/openssl@1.1/1.1.1s \
    -DLibArchive_INCLUDE_DIR=/usr/local/Cellar/libarchive/3.6.1/include \
    -DNM_WITH_NCURSES=ON
$ cmake --build .
```

* Setup VNC client (SPICE is disabled in QEMU from homebrew)

Edit config file (default path: .config/nemu/nemu.cfg)
```
spice_default = 0
vnc_bin = /Applications/TigerVNC Viewer 1.12.0.app/Contents/MacOS/TigerVNC Viewer
```
