LOCAL_PATH := $(call my-dir)

PROPRIETARY_PATH := ../../../../vendor/samsung/$(TARGET_DEVICE)/proprietary

# Camera HAL Wrapper
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    CameraWrapper.cpp


LOCAL_SHARED_LIBRARIES := \
    libhardware liblog libcamera_client libutils

LOCAL_C_INCLUDES += \
    system/media/camera/include

LOCAL_MODULE := camera.universal7580
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional


include $(BUILD_SHARED_LIBRARY)

