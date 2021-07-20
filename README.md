# nEMU [Ncurses UI for QEMU]

[![Build Status](https://travis-ci.com/nemuTUI/nemu.svg?branch=master)](https://travis-ci.com/nemuTUI/nemu)
[![Latest Tag](https://img.shields.io/github/tag/nemuTUI/nemu.svg)](https://github.com/nemuTUI/nemu/tags)
[![Packaging status](https://repology.org/badge/tiny-repos/nemu.svg)](https://repology.org/project/nemu/versions)

## Features
 * Install/delete/clone/rename VM
 * Show VM status, CPU usage
 * Start/stop/shutdown/reset VM
 * Connect to VM via VNC or SPICE protocol
 * Full VM snapshots (since QEMU-6.0.0)
 * Show/Edit VM settings
 * USB support
 * Network via tap/macvtap interfaces
 * VirtFS support (Plan 9 host files sharing)
 * Import OVA (OVF 1.0/2.0)
 * D-Bus support
 * Remote control API

## UI demo
[![Alt New user interface](https://img.youtube.com/vi/y8RT6-AF1BA/3.jpg)](https://www.youtube.com/watch?v=y8RT6-AF1BA)
[![Form navigation](https://img.youtube.com/vi/KuLLnyLbcyw/3.jpg)](https://www.youtube.com/watch?v=KuLLnyLbcyw)
### Old user interface (versions < 2.0.0)
[![Alt Install OpenBSD VM example](https://img.youtube.com/vi/GdqSk1cto50/1.jpg)](https://www.youtube.com/watch?v=GdqSk1cto50)
[![Alt Redirecting serial line terminals to tty,socket](https://img.youtube.com/vi/j5jeFa9Pl9E/1.jpg)](https://www.youtube.com/watch?v=j5jeFa9Pl9E)
[![Alt Snapshots preview](https://img.youtube.com/vi/lYkiolMg42Y/1.jpg)](https://www.youtube.com/watch?v=lYkiolMg42Y)
[![Alt Overview](https://img.youtube.com/vi/jOtCY--LEN8/1.jpg)](https://www.youtube.com/watch?v=jOtCY--LEN8)

## Environment Requirements
 * Linux/FreeBSD host
 * QEMU (>= 2.12.0 [minimum] >= 6.0.0 [all features])

## Packages
 * Debian and Ubuntu [packages](https://software.opensuse.org/download.html?project=home%3ASmartFinn%3AnEMU&package=nemu)
 * Gentoo: `emerge app-emulation/nemu`
 * FreeBSD: `pkg install nemu` or `make -C /usr/ports/emulators/nemu install clean`
 * RPMs: [stable](https://copr.fedorainfracloud.org/coprs/grafin1992/nEMU/), [latest](https://copr.fedorainfracloud.org/coprs/grafin1992/nEMU-latest/)

## Remote clients
 * nemu-droid: [releases](https://github.com/nemuTUI/nemu-droid/releases)
