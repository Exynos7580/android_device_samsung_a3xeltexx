#
# Copyright (C) 2017 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

TARGET_OTA_ASSERT_DEVICE := a3xelte,a3xeltexx

DEVICE_PATH := device/samsung/a3xeltexx

TARGET_POWERHAL_VARIANT := a3xelte



####################
# Platform         #
####################

BOARD_VENDOR := samsung
TARGET_BOARD_PLATFORM := exynos5
TARGET_SOC := exynos7580
TARGET_SLSI_VARIANT := cm

TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true

TARGET_BOOTLOADER_BOARD_NAME := universal7580

####################
# Arch             #
####################

FORCE_32_BIT := true

TARGET_GLOBAL_CFLAGS += -mfpu=neon -mfloat-abi=softfp
TARGET_GLOBAL_CPPFLAGS += -mfpu=neon -mfloat-abi=softfp
TARGET_BOARD_SUFFIX := _32
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := cortex-a53
TARGET_CPU_CORTEX_A53 := true


BOARD_MEMCPY_AARCH32 := true


####################
# Bluetooth        #
####################

# Bluetooth
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_HAVE_SAMSUNG_BLUETOOTH := true
BOARD_CUSTOM_BT_CONFIG := $(DEVICE_PATH)/bluetooth/libbt_vndcfg.txt
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(DEVICE_PATH)/bluetooth

####################
# Boot             #
####################

### Kernel Configuration
TARGET_KERNEL_CONFIG := lineageos_a3xelte_defconfig
TARGET_KERNEL_SOURCE := kernel/samsung/exynos7580-common
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64

### Boot.img configuration
BOARD_CUSTOM_BOOTIMG_MK := hardware/samsung/mkbootimg.mk
BOARD_KERNEL_SEPARATED_DT := true
BOARD_MKBOOTIMG_ARGS := --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --tags_offset 0x00000100 --board SRPOJ08A000KU
TARGET_CUSTOM_DTBTOOL := dtbhtoolExynos
BOARD_KERNEL_PAGESIZE := 2048
#BOARD_KERNEL_CMDLINE := The bootloader ignores the cmdline from the boot.img
TARGET_USES_UNCOMPRESSED_KERNEL := true

####################
# Boot animation   #
####################

TARGET_BOOTANIMATION_PRELOAD := true
TARGET_BOOTANIMATION_TEXTURE_CACHE := true

####################
# Charging         #
####################

# Charger
BOARD_CHARGING_MODE_BOOTING_LPM := /sys/class/power_supply/battery/batt_lp_charging
BOARD_CHARGER_ENABLE_SUSPEND := true
BOARD_CHARGER_SHOW_PERCENTAGE := true
CHARGING_ENABLED_PATH := /sys/class/power_supply/battery/batt_lp_charging

####################
# CM Hardware      #
####################

# CyanogenMod Hardware
BOARD_HARDWARE_CLASS += $(DEVICE_PATH)/cmhw

####################
# DVFS             #
####################


BOARD_GLOBAL_CFLAGS += -DSAMSUNG_DVFS

####################
# Fonts            #
####################

EXTENDED_FONT_FOOTPRINT := true

####################
# Fstab / Recovery #
####################

# fstab file
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/rootdir/fstab.samsungexynos7580

####################
# Graphics         #
####################

BOARD_USES_HWC_SERVICES = true

# OpenGL
USE_OPENGL_RENDERER := true

# Maximum size of the  GLES Shaders that can be cached for reuse.
MAX_EGL_CACHE_KEY_SIZE := 12*1024

# Maximum GLES shader cache size
MAX_EGL_CACHE_SIZE := 2048*1024

# Screen casting
#BOARD_USES_WFD := true

# Virtual display
#BOARD_USES_VIRTUAL_DISPLAY := true

# FIMG2API
#BOARD_USES_SKIA_FIMGAPI := true

# HDMI
BOARD_HDMI_INCAPABLE := true
BOARD_USES_GSC_VIDEO := true

EXYNOS5_ENHANCEMENTS := true

ifdef EXYNOS5_ENHANCEMENTS
TARGET_GLOBAL_CFLAGS += -DEXYNOS5_ENHANCEMENTS
endif

####################
# Includes         #
####################

# Include folder
TARGET_SPECIFIC_HEADER_PATH += $(DEVICE_PATH)/include

# CM Hardware
BOARD_HARDWARE_CLASS += hardware/samsung/cmhw

# Properties
#TARGET_SYSTEM_PROP := $(DEVICE_PATH)/system.prop

# LED path
BACKLIGHT_PATH := "/sys/class/backlight/panel/brightness"

# SELinux
BOARD_SEPOLICY_DIRS := $(DEVICE_PATH)/sepolicy

# Seccomp filters
BOARD_SECCOMP_POLICY += $(DEVICE_PATH)/seccomp

####################
# Init             #
####################

TARGET_INIT_VENDOR_LIB := libinit_sec

####################
# Kernel           #
####################

TARGET_LINUX_KERNEL_VERSION := 3.10
TARGET_KERNEL_ARCH := arm64
ifeq ($(FORCE_32_BIT),true)
TARGET_KERNEL_HEADER_ARCH := arm64
TARGET_KERNEL_CROSS_COMPILE_PREFIX := aarch64-linux-android-
KERNEL_TOOLCHAIN := $(ANDROID_BUILD_TOP)/prebuilts/gcc/$(HOST_OS)-x86/aarch64/aarch64-linux-android-4.9/bin
endif
BOARD_KERNEL_BASE := 0x10000000
BOARD_KERNEL_IMAGE_NAME := Image
BOARD_KERNEL_PAGESIZE := 2048
ifneq ($(FORCE_32_BIT),true)
TARGET_USES_UNCOMPRESSED_KERNEL := true
endif

####################
# NFC              #
####################

BOARD_NFC_HAL_SUFFIX := universal7580
BOARD_HAVE_NFC := true

####################
# OMX              #
####################

# Samsung OpenMAX Video
BOARD_USE_STOREMETADATA := true
BOARD_USE_METADATABUFFERTYPE := true
BOARD_USE_DMA_BUF := true
BOARD_USE_ANB_OUTBUF_SHARE := true
BOARD_USE_IMPROVED_BUFFER := true
BOARD_USE_NON_CACHED_GRAPHICBUFFER := true
BOARD_USE_GSC_RGB_ENCODER := true
BOARD_USE_CSC_HW := false
BOARD_USE_QOS_CTRL := false
BOARD_USE_VP8ENC_SUPPORT := true

####################
# Partitions       #
####################

# Partitions config
BOARD_HAS_NO_MISC_PARTITION := false
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true

# Partitions size
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432              # 32MB
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 39845888          # 38MB
BOARD_CACHEIMAGE_PARTITION_SIZE := 209715200            # 200MB
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 3145728000          # 3GB
BOARD_USERDATAIMAGE_PARTITION_SIZE := 12096372736       # 11GB
BOARD_FLASH_BLOCK_SIZE := 131072

# DAT Compression
BLOCK_BASED_OTA := true

####################
# Properties       #
####################

# Properties
TARGET_SYSTEM_PROP := $(DEVICE_PATH)/system.prop

####################
# Radio            #
####################

# Radio Class Path
BOARD_RIL_CLASS := ../../../$(DEVICE_PATH)/ril

# Radio Modem Type
BOARD_MODEM_TYPE := tss310
BOARD_VENDOR := samsung
BOARD_PROVIDES_RILD := true

####################
# Recovery         #
####################

BOARD_HAS_DOWNLOAD_MODE := true

####################
# Render Script    #
####################

BOARD_OVERRIDE_RS_CPU_VARIANT_32 := cortex-a53
BOARD_OVERRIDE_RS_CPU_VARIANT_64 := cortex-a53

####################
# (G)Scaler        #
####################

BOARD_USES_SCALER := true

####################
# Sensors          #
####################


TARGET_NO_SENSOR_PERMISSION_CHECK := true

####################
# Surface          #
####################

# VSYNC
PRESENT_TIME_OFFSET_FROM_VSYNC_NS := 0
VSYNC_EVENT_PHASE_OFFSET_NS := 0
SF_VSYNC_EVENT_PHASE_OFFSET_NS := 0
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3



####################
# Wi-Fi            #
####################

BOARD_HAVE_SAMSUNG_WIFI          := true
BOARD_WLAN_DEVICE                := bcmdhd
WIFI_BAND                        := 802_11_ABG
WPA_SUPPLICANT_VERSION           := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER      := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_HOSTAPD_DRIVER             := NL80211
BOARD_HOSTAPD_PRIVATE_LIB        := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
WIFI_DRIVER_FW_PATH_PARAM        := "/sys/module/dhd/parameters/firmware_path"
WIFI_DRIVER_NVRAM_PATH           := "/system/etc/wifi/nvram_net.txt"
WIFI_DRIVER_FW_PATH_STA          := "/system/etc/wifi/bcmdhd_sta.bin"
WIFI_DRIVER_FW_PATH_AP           := "/system/etc/wifi/bcmdhd_apsta.bin"


#from TO21 sources:

TARGET_LINUX_KERNEL_VERSION := 3.10

# Gralloc
BOARD_USES_EXYNOS5_COMMON_GRALLOC := true

TARGET_USES_UNIVERSAL_LIBHWJPEG := true

# HWComposer
BOARD_USES_VPP := true

# Device Tree
BOARD_USES_DT := true

# SCALER
BOARD_USES_DEFAULT_CSC_HW_SCALER := true
BOARD_USES_SCALER_M2M1SHOT := true


# Inherit from the proprietary version
-include vendor/samsung/a3xeltexx/BoardConfigVendor.mk
