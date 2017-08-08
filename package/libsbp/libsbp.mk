################################################################################
#
# libsbp
#
################################################################################

LIBSBP_VERSION = v2.2.JSON-2
LIBSBP_SITE = https://github.com/jkretzmer/libsbp
LIBSBP_SITE_METHOD = git
LIBSBP_INSTALL_STAGING = YES
LIBSBP_SUBDIR = c

$(eval $(cmake-package))
