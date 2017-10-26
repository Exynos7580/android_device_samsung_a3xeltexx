####################
# Samsung Package  #
####################

PRODUCT_PACKAGES += \
    SamsungServiceMode \
    dtbhtoolExynos
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/healthd:root/sbin/healthd


# samsung's sswap
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/sswap:root/sbin/sswap

