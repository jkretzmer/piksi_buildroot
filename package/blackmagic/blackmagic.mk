################################################################################
#
# blackmagic
#
################################################################################

BLACKMAGIC_VERSION = 89075b1fead75305d87bcbe1472f77e7c1826cda
BLACKMAGIC_SITE = https://github.com/swift-nav/blackmagic
BLACKMAGIC_SITE_METHOD = git

define BLACKMAGIC_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D)/src
endef

define BLACKMAGIC_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/src/blackmagic $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
