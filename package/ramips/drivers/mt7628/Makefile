# All rights reserved.
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mt7628
PKG_VERSION:=4.0.1.3
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

PKG_KCONFIG:=RALINK_MT7628 MT7628 RT_FIRST_IF_RF_OFFSET RT_FIRST_CARD \
		MT_WIFI  RT_FIRST_CARD_EEPROM RT_SECOND_CARD_EEPROM MULTI_INF_SUPPORT WIFI_BASIC_FUNC \
		MT7628_WSC_INCLUDED MT7628_WSC_V2_SUPPORT  MT7628_DOT11W_PMF_SUPPORT  MT7628_LLTD_SUPPORT MT7628_QOS_DLS_SUPPORT \
		MT7628_WAPI_SUPPORT MT7628_IGMP_SNOOP_SUPPORT MT7628_BLOCK_NET_IF MT7628_RATE_ADAPTION MT7628_NEW_RATE_ADAPT_SUPPORT \
		MT7628_AGS_SUPPORT MT7628_IDS_SUPPORT MT7628_WIFI_WORK_QUEUE MT7628_WIFI_SKB_RECYCLE  MT7628_SMART_CARRIER_SENSE_SUPPORT \
		MT7628_LED_CONTROL_SUPPORT MT7628_ATE_SUPPORT  MT7628_MEMORY_OPTIMIZATION  MT7628_PASSPOINT_R2 MT7628_SINGLE_SKU \
		MT7628_UAPSD MT_MAC RTMP_PCI_SUPPORT  RTMP_USB_SUPPORT RTMP_RBUS_SUPPORT MT_AP_SUPPORT MT_STA_SUPPORT \
		MT7628_AP_SUPPORT MT7628_WDS_SUPPORT MT7628_MBSS_SUPPORT MT7628_NEW_MBSSID_MODE MT7628_ENHANCE_NEW_MBSSID_MODE \
		MT7628_APCLI_SUPPORT MT7628_MAC_REPEATER_SUPPORT MT7628_DOT11R_FT_SUPPORT MT7628_SNIFFER_SUPPORT MT7628_CON_WPS_SUPPORT \
		MT7628_LLTD_SUPPORT MT7628_AIRPLAY_SUPPORT MT7628_BAND_STEERING  MT7628_STA_SUPPORT MT7628_ELIAN
PKG_CONFIG_DEPENDS:=$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)))

include $(INCLUDE_DIR)/package.mk

TAR_CMD=$(HOST_TAR) -C $(1)/ $(TAR_OPTIONS)

define KernelPackage/mt7628
  CATEGORY:=MTK Properties
  TITLE:=MTK MT7628 wifi AP driver
  FILES:=$(PKG_BUILD_DIR)/build/mt7628.ko
  AUTOLOAD:=$(call AutoLoad,90,mt7628)
  SUBMENU:=Drivers
  DEPENDS:=+kmod-mac80211
  MENU:=1
endef

define KernelPackage/mt7628/config
	source "$(SOURCE)/config.in"
endef

SOURCE_DIR:=$(PKG_BUILD_DIR)
export SOURCE_DIR

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) config.in $(PKG_BUILD_DIR)
	$(CP) -r src $(PKG_BUILD_DIR)
	$(CP) -r build $(PKG_BUILD_DIR)
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		SUBDIRS="$(PKG_BUILD_DIR)/build/" \
		$(foreach c, $(PKG_KCONFIG),$(if $(CONFIG_$c),CONFIG_$(c)=$(CONFIG_$(c))))\
		modules
endef

define KernelPackage/mt7628/install
	#$(INSTALL_DIR) $(1)/etc/wireless/mt7628/
	#$(INSTALL_BIN) ./files/mt7628.dat $(1)/etc/wireless/mt7628/
	$(CP) ./files/* $(1)

endef

$(eval $(call KernelPackage,mt7628))
