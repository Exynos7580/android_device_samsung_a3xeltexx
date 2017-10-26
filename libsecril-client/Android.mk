# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng

LOCAL_SRC_FILES:= \
    secril-client.cpp

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy

LOCAL_CFLAGS := 

ifeq ($(TARGET_BOARD_PLATFORM),exynos4)
LOCAL_CFLAGS += -DRIL_CALL_AUDIO_PATH_EXTRAVOLUME
endif
ifneq ($(filter m7450 mdm9x35 ss333 xmm7260 tss310,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS += -DSAMSUNG_NEXT_GEN_MODEM
endif

LOCAL_MODULE:= libsecril-client-a3
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
