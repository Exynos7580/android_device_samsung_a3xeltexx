####################
# Samsung Package  #
####################

PRODUCT_PACKAGES += \
    SamsungServiceMode \
    dtbhtoolExynos \
    FMRadio
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/healthd:root/sbin/healthd
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/init.rc:root/

# samsung's sswap
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/sswap:root/sbin/sswap

BOARD_HAVE_FMRADIO_BCM := true
