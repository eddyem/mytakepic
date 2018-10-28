# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit linux-info linux-mod 

DESCRIPTION="FLI USB kernel module"
HOMEPAGE=""
SRC_URI=""

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

DEPEND=""
RDEPEND="${DEPEND}"
S="${WORKDIR}"

MODULE_NAMES="fliusb(misc:fliusb:fliusb)"
BUILD_TARGETS='default'

src_unpack() {
    cd ${S}
    tar -zvxf /home/eddy/C-files/mytakepic/extern/fliusb.tgz
}

src_compile() {
    cd ${S}/fliusb
    linux-mod_src_compile
}

src_install() {
    linux-mod_src_install
    pushd /home/eddy/C-files/mytakepic/extern
    insinto /lib/udev/rules.d
    doins fliusb.rules
}
