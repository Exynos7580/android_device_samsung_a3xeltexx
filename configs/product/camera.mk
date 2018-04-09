####################
# Camera
####################

# Camera
TARGET_HAS_LEGACY_CAMERA_HAL1 := true
BOARD_GLOBAL_CFLAGS += -DCAMERA_VENDOR_L_COMPAT
BOARD_USE_SAMSUNG_CAMERAFORMAT_NV21 := true


PRODUCT_PACKAGES += \
        camera.universal7580 \
    	camera.vendor.universal7580 \
	Snap \
	camera.device@1.0-impl-legacy \
	android.hardware.camera.provider@2.4-impl-legacy
