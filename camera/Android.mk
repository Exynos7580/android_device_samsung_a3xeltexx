LOCAL_PATH := $(call my-dir)

PROPRIETARY_PATH := ../../../../vendor/samsung/$(TARGET_DEVICE)/proprietary

# Camera HAL Wrapper
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    CameraWrapper.cpp


LOCAL_SHARED_LIBRARIES := \
    libhardware liblog libcamera_client libutils libcutils

LOCAL_C_INCLUDES += \
    system/core/include \
    system/media/camera/include

LOCAL_MODULE := camera.universal7580
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional


include $(BUILD_SHARED_LIBRARY)

# Stock Camera HAL
include $(CLEAR_VARS)

LOCAL_MODULE		:= camera.vendor.universal7580
LOCAL_MODULE_SUFFIX 	:= .so
LOCAL_SRC_FILES		:= $(PROPRIETARY_PATH)/lib/hw/camera.universal7580.so
LOCAL_MODULE_CLASS 	:= SHARED_LIBRARIES
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS	:= optional

include $(BUILD_PREBUILT)

