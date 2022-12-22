# nEMU [Ncurses UI for QEMU]

[![Latest Tag](https://img.shields.io/github/tag/nemuTUI/nemu.svg)](https://github.com/nemuTUI/nemu/tags)
[![Packaging status](https://repology.org/badge/tiny-repos/nemu.svg)](https://repology.org/project/nemu/versions)
[![Linux build](https://github.com/nemuTUI/nemu/actions/workflows/linux.yml/badge.svg)](https://github.com/nemuTUI/nemu/actions/workflows/linux.yml)
[![FreeBSD build](https://github.com/nemuTUI/nemu/actions/workflows/freebsd.yml/badge.svg)](https://github.com/nemuTUI/nemu/actions/workflows/freebsd.yml)
[![MacOSX build](https://github.com/nemuTUI/nemu/actions/workflows/macosx.yml/badge.svg)](https://github.com/nemuTUI/nemu/actions/workflows/macosx.yml)

[![gif-demo](https://user-images.githubusercontent.com/5861368/152040930-cb4e7e69-08b0-4902-bc20-925e061ae414.png)](https://user-images.githubusercontent.com/5861368/152041148-f6acc0a3-445f-40a1-9fa2-e4c16ca76b0f.gif)
[![discord](https://img.shields.io/discord/1055425285593501706?color=%237289da&logo=discord&logoColor=white&label=discord)](https://discord.gg/s3NZCKGkqv)
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
 * Linux/FreeBSD/MacOS host
 * QEMU (>= 2.12.0 [minimum] >= 6.0.0 [all features])

## Packages
 * Alpine: `apk add nemu`
 * Debian and Ubuntu [packages](https://software.opensuse.org/download.html?project=home%3A0x501D&package=nemu)
 * Gentoo: `emerge app-emulation/nemu`
 * FreeBSD: `pkg install nemu` or `make -C /usr/ports/emulators/nemu install clean`
 * RPMs: [stable](https://copr.fedorainfracloud.org/coprs/grafin1992/nEMU/), [latest](https://copr.fedorainfracloud.org/coprs/grafin1992/nEMU-latest/)
 * MacOS: `brew install nemu`

## Remote clients
 * [nemu-droid](https://github.com/nemuTUI/nemu-droid)
