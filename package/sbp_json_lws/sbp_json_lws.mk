################################################################################
#
# sbp_json_lws
#
################################################################################

SBP_JSON_LWS_VERSION = 0.1
SBP_JSON_LWS_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/sbp_json_lws/src"
SBP_JSON_LWS_SITE_METHOD = local
SBP_JSON_LWS_DEPENDENCIES = czmq libsbp libpiksi libwebsockets 

define SBP_JSON_LWS_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define SBP_JSON_LWS_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/sbp_json_lws $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
