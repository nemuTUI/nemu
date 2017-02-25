# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

inherit cmake-utils

DESCRIPTION="ncurses interface for qemu"
HOMEPAGE="https://unixdev.ru/qemu-manage"
SRC_URI="http://unixdev.ru/src/${P}.tar.gz"

LICENSE="WTFPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="
	sys-libs/ncurses:0=[unicode]
	>=dev-libs/boost-1.56
	dev-db/sqlite:3=
	sys-process/procps
	dev-libs/libusb:1=
	|| ( sys-fs/eudev sys-fs/udev )
"
RDEPEND="${DEPEND}"
