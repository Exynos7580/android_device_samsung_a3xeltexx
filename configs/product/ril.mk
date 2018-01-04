# RIL
PRODUCT_PACKAGES += \
    libprotobuf-cpp-full \
    modemloader \
    libxml2 \
    rild \
    libril \
    libreference-ril \
    libsecril-client \
    libsecril-client-sap \
    android.hardware.radio@1.0 \
    android.hardware.radio.deprecated@1.0

# cpboot-daemon for modem
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/ril/external/cbd:system/bin/cbd \
    $(LOCAL_PATH)/ramdisk/vendor/rild.rc:system/vendor/etc/init/rild.rc \
    $(LOCAL_PATH)/ramdisk/vendor/rild-dsds.rc:system/vendor/etc/init/rild-dsds.rc
