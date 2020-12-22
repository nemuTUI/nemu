# Copyright 1999-2020 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=6

inherit cmake-utils linux-info

DESCRIPTION="ncurses UI for QEMU"
HOMEPAGE="https://github.com/nemuTUI/nemu"
SRC_URI="https://github.com/nemuTUI/nemu/archive/v${PV}.tar.gz"

LICENSE="BSD-2"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE="+vnc-client +ovf +spice dbus savevm svg utf"

RDEPEND="
	sys-libs/ncurses:0=[unicode]
	dev-db/sqlite:3=
	dev-libs/libusb:1=
	|| ( sys-fs/eudev sys-fs/udev sys-apps/systemd )
	>=app-emulation/qemu-2.12.0[vnc,virtfs,spice?]
	ovf? (
		dev-libs/libxml2
		app-arch/libarchive
	)
	dbus? ( sys-apps/dbus )
	svg? ( media-gfx/graphviz[svg] )"
DEPEND="${RDEPEND}
	sys-devel/gettext"

src_configure() {
	local mycmakeargs=(
		-DNM_WITH_VNC_CLIENT=$(usex vnc-client)
		-DNM_SAVEVM_SNAPSHOTS=$(usex savevm)
		-DNM_WITH_OVF_SUPPORT=$(usex ovf)
		-DNM_WITH_NETWORK_MAP=$(usex svg)
		-DNM_WITH_SPICE=$(usex spice)
		-DNM_WITH_DBUS=$(usex dbus)
		-DNM_WITH_UTF=$(usex utf)
		-DCMAKE_INSTALL_PREFIX=/usr
	)
	cmake-utils_src_configure
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

pkg_postinst() {
	elog "For non-root usage execute script:"
	elog "/usr/share/nemu/scripts/setup_nemu_nonroot.sh linux <username>"
	elog "and add udev rule:"
	elog "cp /usr/share/nemu/scripts/42-net-macvtap-perm.rules /lib/udev/rules.d"
	if use savevm; then
		elog ""
		elog "QEMU must be patched with qemu-qmp-savevm-VERSION.patch"
		elog "Get this patch from nEMU repository"
	fi
}
