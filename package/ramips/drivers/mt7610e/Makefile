# All rights reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mt7610e
PKG_VERSION:=4.0.1.3
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

PKG_KCONFIG:= \
	RALINK_MT7620 RT_SECOND_CARD MT7610_AP MT7610_AP_V24_DATA_STRUCTURE MT7610_AP_LED MT7610_AP_WSC MT7610_AP_WSC_V2 \
	MT7610_AP_LLTD MT7610_AP_WDS MT7610_AP_MBSS NEW_MBSSID_MODE MT7610_AP_APCLI MT7610_AP_MAC_REPEATER \
	MT7610_AP_IGMP_SNOOP MT7610_AP_DFS MT7610_AP_CARRIER M7610_CON_WPS_SUPPORT MT7610_AP_80211N_DRAFT3 \
	MT7610_AP_ATE MT7610_AP_QA MT7610_AP_FLASH MT7610_AP_BIG_ENDIAN MT7610_AP_TSSI_COMPENSATION \
	RTMP_TEMPERATURE_COMPENSATION MT7610_AP_SINGLE_SKU

PKG_CONFIG_DEPENDS:=$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)))

include $(INCLUDE_DIR)/package.mk

TAR_CMD=$(HOST_TAR) -C $(1)/ $(TAR_OPTIONS)

define KernelPackage/mt7610e
  CATEGORY:=MTK Properties
  TITLE:=MTK MT7610e wifi AP driver
  FILES:=$(PKG_BUILD_DIR)/build/mt7610e.ko
  AUTOLOAD:=$(call AutoLoad,91,mt7610e)
  SUBMENU:=Drivers
  MENU:=1
endef

define KernelPackage/mt7610e/config
	source "$(SOURCE)/config.in"
endef

SOURCE_DIR:=$(PKG_BUILD_DIR)
export SOURCE_DIR

define Build/Prepare
	$(call Build/Prepare/Default)
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" V=1 \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		SUBDIRS="$(PKG_BUILD_DIR)/build/" \
		$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)=$(CONFIG_$(c))))\
		modules
endef

define KernelPackage/mt7610e/install
	$(INSTALL_DIR) $(1)/lib/wifi/
	$(INSTALL_BIN) ./files/mt7610e.sh $(1)/lib/wifi/
	$(INSTALL_DIR) $(1)/etc/wireless/mt7610e/
	$(INSTALL_BIN) ./files/mt7610e.dat $(1)/etc/wireless/mt7610e/
	-$(INSTALL_BIN) ./files/mt7610e.eeprom.bin $(1)/etc/wireless/mt7610e/
endef

$(eval $(call KernelPackage,mt7610e))
