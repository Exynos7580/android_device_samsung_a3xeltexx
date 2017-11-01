###
##
# Device tree for Samsung Galaxy A3 2016 with Exynos7580 SoC
# Copyright (C) 2017, Victor Lourme <contact@zeroside.co>
##
###

# Inherit from Exynos7580-common
include device/samsung/exynos7580-common/BoardConfigCommon.mk

# Define device codename we support
TARGET_OTA_ASSERT_DEVICE := a3xelte,a3xeltexx,a3xelteub,a3xeltedo,a3xeltekx

# Define device-tree path
DEVICE_PATH := device/samsung/a3xeltexx

# Include path
TARGET_SPECIFIC_HEADER_PATH += $(DEVICE_PATH)/include

# Bluetooth
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(DEVICE_PATH)/bluetooth

# Hardware
BOARD_HARDWARE_CLASS += $(DEVICE_PATH)/cmhw

# Init
TARGET_INIT_VENDOR_LIB := libinit_sec
TARGET_UNIFIED_DEVICE := true

# Kernel
TARGET_KERNEL_CONFIG := lineage-a3xeltexx_defconfig
TARGET_KERNEL_SOURCE := kernel/samsung/a3xeltexx
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64

# Extracted with libbootimg
BOARD_CUSTOM_BOOTIMG_MK := hardware/samsung/mkbootimg.mk
BOARD_KERNEL_SEPARATED_DT := true
BOARD_MKBOOTIMG_ARGS := --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --tags_offset 0x00000100  --board SRPOJ08A000KU
TARGET_CUSTOM_DTBTOOL := dtbhtoolExynos

# Partitions
BOARD_HAS_NO_MISC_PARTITION := false
TARGET_USERIMAGES_USE_EXT4 := true
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432		# 32MB
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 39845888		# 38MB
BOARD_CACHEIMAGE_PARTITION_SIZE := 209715200		# 200MB
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 3145728000		# 3GB
BOARD_USERDATAIMAGE_PARTITION_SIZE := 12096372736	# 11GB
BOARD_FLASH_BLOCK_SIZE := 4096

# .dat compression
BLOCK_BASED_OTA := true

# PowerHAL
TARGET_POWERHAL_VARIANT := samsung

# Properties
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/system.prop

# Charger
BACKLIGHT_PATH := /sys/devices/14800000.dsim/backlight/panel/brightness

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/ramdisk/fstab.samsungexynos7580

# Radio
BOARD_RIL_CLASS := ../../../$(DEVICE_PATH)/ril
BOARD_MODEM_TYPE := tss310

# NFC
BOARD_NFC_CHIPSET := pn547

# inherit from the proprietary version
-include vendor/samsung/a3xeltexx/BoardConfigVendor.mk
