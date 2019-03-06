#
#
# Copyright (C) 2017 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=bqtool
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/bqtool/Default
	SECTION:=base
	CATEGORY:=gl-inet
	TITLE:=GL iNet usd or sd card speed test tools
	DEPENDS:= +libc
endef

Package/bqtool = $(Package/bqtool/Default)

define Package/bqtool/description
usd or sd card speed test tools.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

MAKE_FLAGS += \
			  CFLAGS+="$(TARGET_CFLAGS)"

define Package/bqtool/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bqtool $(1)/usr/bin/bqtool

	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,bqtool))
