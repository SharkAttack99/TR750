#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=bq25890-i2c
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/bq25890-i2c
  SUBMENU:=Other modules
  TITLE:=I2C bq25890 Driver
  FILES:=$(PKG_BUILD_DIR)/bq25890-i2c.ko
  # AUTOLOAD:=$(call AutoLoad,75,bq25890-i2c)
  KCONFIG:=
  DEPENDS:=kmod-i2c-core
endef

define KernelPackage/bq25890-i2c/description
 Kernel module for bq25890 chiper initialization.
endef

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG))))

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
	$(EXTRA_KCONFIG)

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,bq25890-i2c))

