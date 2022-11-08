# How to build nEMU on Linux.

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

* Become root by running "su" or another program
```sh
# make install
```
