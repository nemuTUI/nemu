# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

inherit cmake-utils

DESCRIPTION="ncurses interface for QEMU"
HOMEPAGE="https://unixdev.ru/nemu"
SRC_URI="http://unixdev.ru/src/${P}.tar.gz"

LICENSE="WTFPL-2"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE="+vnc"

RDEPEND="
	sys-libs/ncurses:0=[unicode]
	dev-db/sqlite:3=
	sys-process/procps
	dev-libs/libusb:1=
	|| ( sys-fs/eudev sys-fs/udev )"
DEPEND="${RDEPEND}
	>=dev-libs/boost-1.56
	app-emulation/qemu
	vnc? ( net-misc/tigervnc )"
