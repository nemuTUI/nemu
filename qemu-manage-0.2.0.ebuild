# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

inherit cmake-utils

DESCRIPTION="Qemu ncurses interface"
HOMEPAGE="https://unixdev.ru/qemu-manage"
SRC_URI="http://unixdev.ru/src/${P}.tar.gz"

LICENSE="WTFPL-2"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="sys-libs/ncurses:0="
DEPEND="dev-libs/boost"
DEPEND="dev-db/sqlite"
DEPEND="sys-process/procps"
DEPEND="dev-libs/libusb"
DEPEND="|| ( sys-fs/eudev sys-fs/udev )"
RDEPEND="${DEPEND}"
