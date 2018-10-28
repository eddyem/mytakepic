# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit cmake-utils

DESCRIPTION="Simple FLI CCD, focuser and filter wheel management tool"
HOMEPAGE="https://github.com/eddyem/mytakepic/tree/master/fli_control"
SRC_URI=""

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="png raw"

DEPEND="
    sci-libs/fliusb
    sci-libs/libfli
    sci-libs/cfitsio
"
RDEPEND="${DEPEND}"
S="${WORKDIR}"

src_unpack() {
    cp -r /home/eddy/C-files/mytakepic/fli_control/* ${S} || die
}

CMAKE_USE_DIR="${S}"
BUILD_DIR=${S} 

src_configure() {
    local mycmakeargs=(
	"-DCMAKE_INSTALL_PREFIX=/usr/local"
	"-DUSE_PNG=$(usex png)"
	"-DUSE_RAW=$(usex raw)"
    )
     cmake-utils_src_configure
}
