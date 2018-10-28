# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit unpacker

DESCRIPTION="FLI CCD SDK"
HOMEPAGE=""
SRC_URI=""

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

DEPEND="sci-libs/fliusb"
RDEPEND="${DEPEND}"
S="${WORKDIR}/libfli-1.104"

src_unpack() {
    tar -zxf /home/eddy/C-files/mytakepic/extern/libfli-1.104.tgz
}

src_install() {
    insinto /usr/local/lib
    doins libfli.a
    insinto /usr/local/include
    doins libfli.h
    insinto /usr/share/pkgconfig
    doins fli.pc
}
