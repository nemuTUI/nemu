# Copyright 1999-2022 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=8

inherit cmake linux-info git-r3

DESCRIPTION="Ncurses UI for QEMU"
HOMEPAGE="https://github.com/nemuTUI/nemu"
EGIT_REPO_URI="https://github.com/nemuTUI/nemu"
SRC_URI=""

LICENSE="BSD-2"
SLOT="0"
IUSE="+ovf dbus network-map remote-api +usb"

RDEPEND="
	dev-libs/json-c
	sys-libs/ncurses
	dev-db/sqlite:3=
	>=app-emulation/qemu-6.0.0-r2[vnc,virtfs,spice]
	ovf? (
		dev-libs/libxml2
		app-arch/libarchive
	)
	dbus? ( sys-apps/dbus )
	remote-api?  ( dev-libs/openssl )
	network-map? ( media-gfx/graphviz[svg] )
	usb? (
		dev-libs/libusb:1
		|| ( sys-apps/systemd-utils[udev] sys-apps/systemd )
	)"
DEPEND="${RDEPEND}
	sys-devel/gettext"

src_configure() {
	local mycmakeargs=(
		-DNM_WITH_OVF_SUPPORT=$(usex ovf)
		-DNM_WITH_NETWORK_MAP=$(usex network-map)
		-DNM_WITH_REMOTE=$(usex remote-api)
		-DNM_WITH_DBUS=$(usex dbus)
		-DNM_WITH_USB=$(usex usb)
		-DCMAKE_INSTALL_PREFIX=/usr
	)
	cmake_src_configure
}

pkg_pretend() {
	if use kernel_linux; then
		if ! linux_config_exists; then
			eerror "Unable to check your kernel"
		else
			CONFIG_CHECK="~VETH ~MACVTAP"
			ERROR_VETH="You will need the Virtual ethernet pair device driver compiled"
			ERROR_VETH+=" into your kernel or loaded as a module to use the"
			ERROR_VETH+=" local network settings feature."
			ERROR_MACVTAP="You will also need support for MAC-VLAN based tap driver."

			check_extra_config
		fi
	fi
}

src_install() {
	cmake_src_install
	docompress -x /usr/share/man/man1/nemu.1.gz
}

pkg_postinst() {
	elog "For non-root usage execute script:"
	elog "/usr/share/nemu/scripts/setup_nemu_nonroot.sh linux <username>"
	elog "and add udev rule:"
	elog "cp /usr/share/nemu/scripts/42-net-macvtap-perm.rules /lib/udev/rules.d"
}
