inherit autotools-brokensep pkgconfig

DESCRIPTION = "GPS LBS Core"
PR = "r1"
LICENSE = "Qualcomm-Technologies-Inc.-Proprietary"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta-qcom/files/qcom-licenses/Qualcomm-Technologies-Inc.-Proprietary;md5=92b1d0ceea78229551577d4284669bb8"

FILESPATH =+ "${WORKSPACE}:"
SRC_URI = "file://gps/framework/native/lcp/data-items"
DEPENDS = "glib-2.0 loc-pla loc-base-util"
EXTRA_OECONF ="--with-glib"

S = "${WORKDIR}/gps/framework/native/data-items"
