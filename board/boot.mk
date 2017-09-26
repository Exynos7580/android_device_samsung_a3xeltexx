####################
# Boot             #
####################

### Kernel Configuration
TARGET_KERNEL_CONFIG := lineageos_a3xelte_defconfig
TARGET_KERNEL_SOURCE := kernel/samsung/exynos7580-common
TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_HEADER_ARCH := arm64

### Boot.img configuration
#BOARD_CUSTOM_BOOTIMG_MK := hardware/samsung/mkbootimg.mk
#BOARD_KERNEL_SEPARATED_DT := true
BOARD_MKBOOTIMG_ARGS := --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --tags_offset 0x00000100 --dt $(DEVICE_PATH)/dtb.img
#TARGET_CUSTOM_DTBTOOL := dtbhtoolExynos
BOARD_KERNEL_PAGESIZE := 2048
#BOARD_KERNEL_CMDLINE := The bootloader ignores the cmdline from the boot.img
#BOARD_KERNEL_SEPARATED_DT := true
#TARGET_CUSTOM_DTBTOOL := dtbhtoolExynos
TARGET_USES_UNCOMPRESSED_KERNEL := true

