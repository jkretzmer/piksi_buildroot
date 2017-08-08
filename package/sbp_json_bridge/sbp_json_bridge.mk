################################################################################
#
# sbp_json_bridge
#
################################################################################

SBP_JSON_BRIDGE_VERSION = 0.1
SBP_JSON_BRIDGE_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_json_bridge/src"
SBP_JSON_BRIDGE_SITE_METHOD = local
SBP_JSON_BRIDGE_DEPENDENCIES = czmq libsbp libpiksi

define SBP_JSON_BRIDGE_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_JSON_BRIDGE_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/sbp_json_bridge $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
