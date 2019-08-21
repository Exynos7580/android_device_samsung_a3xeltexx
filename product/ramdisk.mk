####################
# Ramdisk          #
####################

PRODUCT_PACKAGES += \
    fstab.samsungexynos7580 \
    init.baseband.rc \
    init.samsung.rc \
    init.rild.rc \
    init.samsungexynos7580.rc \
    init.samsungexynos7580.usb.rc \
    init.wifi.rc \
    ueventd.samsungexynos7580.rc

# cpboot-daemon
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/cbd:root/sbin/cbd

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/healthd:root/sbin/healthd

# samsung's sswap
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/sswap:root/sbin/sswap

