#
#
# Copyright (C) 2017 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=gl-iospeed
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/gl-iospeed/Default
	SECTION:=base
	CATEGORY:=gl-inet
	TITLE:=GL iNet usd or sd card speed test tools
endef

Package/gl-iospeed = $(Package/gl-iospeed/Default)

define Package/gl-iospeed/description
usd or sd card speed test tools.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/gl-iospeed/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gl_iospeed $(1)/usr/bin/gl_iospeed
endef

$(eval $(call BuildPackage,gl-iospeed))
