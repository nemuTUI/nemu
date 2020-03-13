# nEMU [Ncurses UI for QEMU]

[![Build Status](https://travis-ci.org/nemuTUI/nemu.svg?branch=master)](https://travis-ci.org/nemuTUI/nemu)
[![Latest Tag](https://badgen.net/github/tag/nemuTUI/nemu)](https://github.com/nemuTUI/nemu/tags)
[![Packaging status](https://repology.org/badge/tiny-repos/nemu.svg)](https://repology.org/project/nemu/versions)

## Features
 * Install VM
 * Delete VM
 * Clone VM
 * Show VM status
 * Start/stop/shutdown/reset VM
 * Connect to VM via VNC or SPICE protocol
 * Full VM snapshots (experimental)
 * Show/Edit VM settings
 * USB support
 * Network via tap/macvtap interfaces
 * VirtFS support (Plan 9 host files sharing)
 * Import OVA

## Videos
[![Alt New user interface](https://img.youtube.com/vi/y8RT6-AF1BA/3.jpg)](https://www.youtube.com/watch?v=y8RT6-AF1BA)
### Old user interface (versions < 2.0.0)
[![Alt Install OpenBSD VM example](https://img.youtube.com/vi/GdqSk1cto50/1.jpg)](https://www.youtube.com/watch?v=GdqSk1cto50)
[![Alt Redirecting serial line terminals to tty,socket](https://img.youtube.com/vi/j5jeFa9Pl9E/1.jpg)](https://www.youtube.com/watch?v=j5jeFa9Pl9E)
[![Alt Snapshots preview](https://img.youtube.com/vi/lYkiolMg42Y/1.jpg)](https://www.youtube.com/watch?v=lYkiolMg42Y)
[![Alt Overview](https://img.youtube.com/vi/jOtCY--LEN8/1.jpg)](https://www.youtube.com/watch?v=jOtCY--LEN8)

## Environment Requirements
 * Linux/FreeBSD host
 * QEMU (>= 2.12.0)

## Packages
 * Debian and Ubuntu [packages](https://software.opensuse.org/download.html?project=home%3ASmartFinn%3AnEMU&package=nemu)
 * Gentoo: `emerge app-emulation/nemu`
 * FreeBSD: `pkg install nemu` or `make -C /usr/ports/emualtors/nemu install clean`
 * [RPMs](https://copr.fedorainfracloud.org/coprs/grafin1992/nEMU/)
