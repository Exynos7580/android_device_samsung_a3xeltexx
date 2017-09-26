####################
# Samsung Package  #
####################

PRODUCT_PACKAGES += \
    SamsungServiceMode

#PRODUCT_COPY_FILES += \
#    $(LOCAL_PATH)/rootdir/healthd:root/sbin/healthd


# samsung's sswap
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rootdir/sswap:root/sbin/sswap
