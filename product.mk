PRODUCT_PACKAGES += \
        libMcClient \
        libMcRegistry \
        mcDriverDaemon \
	libhwjpeg

####################
# Audio            #
####################

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/audio/audio_policy.conf:system/etc/audio_policy.conf \
    $(LOCAL_PATH)/configs/audio/mixer_paths.xml:system/etc/mixer_paths.xml \
    $(LOCAL_PATH)/configs/audio/mixer_paths_0.xml:system/etc/mixer_paths_0.xml

#TARGET_AUDIOHAL_VARIANT=samsung

PRODUCT_PACKAGES += \
    audio.primary.universal7580 \
    audio.a2dp.default \
    audio.r_submix.default \
    audio.usb.default \
    tinymix

####################
# Bluetooth        #
####################

# Bluetooth config
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/bluetooth/bt_vendor.conf:system/etc/bluetooth/bt_vendor.conf

PRODUCT_PACKAGES += \
    libbt-vendor

####################
# Boot animation   #
####################

# Height x Width
TARGET_SCREEN_HEIGHT := 1280
TARGET_SCREEN_WIDTH := 720

##########
# Camera #
##########

BOARD_USE_SAMSUNG_CAMERAFORMAT_NV21 := true
TARGET_HAS_LEGACY_CAMERA_HAL1 := true

PRODUCT_PACKAGES += \
    camera.universal7580 \
    camera.vendor.universal7580 \
    libxml2 \
    Snap

####################
# Charger          #
####################

# Use cm images if available, aosp ones otherwise
PRODUCT_PACKAGES += \
        charger_res_images \
        cm_charger_res_images

####################
# Screen size      #
####################

# Device uses high-density artwork where available
PRODUCT_AAPT_CONFIG := xlarge
PRODUCT_AAPT_PREF_CONFIG := xhdpi

# A list of dpis to select prebuilt apk, in precedence order.
PRODUCT_AAPT_PREBUILT_DPI := hdpi mdpi

#######
# FM  #
#######

PRODUCT_PACKAGES += \
    FMRadio

BOARD_HAVE_FMRADIO_BCM := true

####################
# Filesystem       #
####################

PRODUCT_PACKAGES += \
    make_ext4fs \
    setup_fs

####################
# GPS              #
####################

# GPS config
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/gps/gps.conf:system/etc/gps.conf \
    $(LOCAL_PATH)/configs/gps/gps.xml:system/etc/gps.xml

####################
# Graphics         #
####################

PRODUCT_PACKAGES += \
    hwcomposer.exynos5

###########
# healthd #
###########

WITH_CM_CHARGER = false

BOARD_HAL_STATIC_LIBRARIES := libhealthd.universal7580

####################
# Key-layout       #
####################

# Key-layout path
PRODUCT_COPY_FILES += \
        $(LOCAL_PATH)/idc/Synaptics_HID_TouchPad.idc:system/usr/idc/Synaptics_HID_TouchPad.idc \
        $(LOCAL_PATH)/idc/Synaptics_RMI4_TouchPad_Sensor.idc:system/usr/idc/Synaptics_RMI4_TouchPad_Sensor.idc \
        $(LOCAL_PATH)/keylayout/Button_Jack.kl:system/usr/keylayout/Button_Jack.kl \
        $(LOCAL_PATH)/keylayout/gpio_keys.kl:system/usr/keylayout/gpio_keys.kl \
        $(LOCAL_PATH)/keylayout/sec_touchkey.kl:system/usr/keylayout/sec_touchkey.kl

####################
# Lights           #
####################

PRODUCT_PACKAGES += \
    lights.universal7580

####################
# mDNIe            #
####################

PRODUCT_PACKAGES += \
    AdvancedDisplay

####################
# Media            #
####################

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/media/media_codecs.xml:system/etc/media_codecs.xml \
    $(LOCAL_PATH)/configs/media/media_profiles.xml:system/etc/media_profiles.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:system/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video_le.xml:system/etc/media_codecs_google_video_le.xml

####################
# MTP              #
####################

PRODUCT_PACKAGES += \
    com.android.future.usb.accessory

####################
# NFC              #
####################

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/nfc/libnfc-sec-hal.conf:system/etc/libnfc-sec-hal.conf \
    $(LOCAL_PATH)/configs/nfc/libnfc-sec.conf:system/etc/libnfc-brcm.conf \
    $(LOCAL_PATH)/configs/nfc/nfcee_access.xml:system/etc/nfcee_access.xml \

PRODUCT_PACKAGES += \
    nfc_nci.universal7580 \
    libnfc \
    libnfc_jni \
    NfcNci \
    Tag \
    com.android.nfc_extras

####################
# Overlays         #
####################

DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/overlay

####################
# Permission       #
####################

# Permissions list
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.heartrate.xml:system/etc/permissions/android.hardware.sensor.heartrate.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:system/etc/permissions/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:system/etc/permissions/android.hardware.sensor.stepdetector.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:system/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/com.android.nfc_extras.xml:system/etc/permissions/com.android.nfc_extras.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.software.midi.xml:system/etc/permissions/android.software.midi.xml \
    frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml

####################
# Power            #
####################

PRODUCT_PACKAGES += \
    power.universal7580

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
    $(LOCAL_PATH)/rootdir/cbd:root/sbin/cbd \
    $(LOCAL_PATH)/rootdir/sswap:root/sbin/sswap

####################
# Radio            #
####################

PRODUCT_PACKAGES += \
    libprotobuf-cpp-full \
    modemloader \
    libsecril-client \
    libsecril-client-sap

####################
# Samsung Package  #
####################

PRODUCT_PACKAGES += \
    SamsungServiceMode \
    dtbhtoolExynos

####################
# Shims            #
####################

PRODUCT_PACKAGES += \
    libshim_gpsd

####################
# STLport          #
####################

PRODUCT_PACKAGES += \
    libstlport

####################
# Wi-Fi            #
####################

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/wifi/wpa_supplicant_overlay.conf:system/etc/wifi/wpa_supplicant_overlay.conf \
    $(LOCAL_PATH)/configs/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    $(LOCAL_PATH)/configs/wifi/p2p_supplicant_overlay.conf:system/etc/wifi/p2p_supplicant_overlay.conf



ADDITIONAL_DEFAULT_PROPERTIES += \
    wifi.interface=wlan0

PRODUCT_PACKAGES += \
    hostapd \
    libnetcmdiface \
    macloader \
    wifiloader \
    wpa_supplicant \
    wpa_supplicant.conf \
    ebtables \
    ethertypes \
    libebtc
