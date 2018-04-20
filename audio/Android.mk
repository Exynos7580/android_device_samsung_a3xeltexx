# Copyright (C) 2018 Evgeniy Stenkin <stenkinevgeniy@gmail.com>
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


ifeq ($(TARGET_AUDIOHAL_VARIANT), exynos7580)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	audio_hw.c \
	ril_interface.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libaudioutils \
	libtinyalsa \
	libtinycompress \
	libaudioroute \
	libdl \
	libsecril-client


LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	external/tinycompress/include \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-route) \
	$(call include-path-for, audio-effects)

LOCAL_MODULE := audio.primary.$(TARGET_BOOTLOADER_BOARD_NAME)

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
