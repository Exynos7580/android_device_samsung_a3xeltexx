LOCAL_PATH := $(call my-dir)

## See build/core/Makefile

## Overload bootimg generation: Same as the original, but with SEANDROIDENFORCE
$(INSTALLED_BOOTIMAGE_TARGET): $(MKBOOTIMG) $(INTERNAL_BOOTIMAGE_FILES) $(BOOTIMAGE_EXTRA_DEPS)
	$(call pretty,"Target boot image: $@")
	$(hide) $(MKBOOTIMG) $(INTERNAL_BOOTIMAGE_ARGS) $(INTERNAL_MKBOOTIMG_VERSION_ARGS) $(BOARD_MKBOOTIMG_ARGS) --output $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE))
	@echo "Made boot image: $@"
	$(hide) echo -n "SEANDROIDENFORCE" >> $(PRODUCT_OUT)/boot.img


## Overload recoveryimg generation: Same as the original, but with SEANDROIDENFORCE
$(INSTALLED_RECOVERYIMAGE_TARGET): $(MKBOOTIMG) $(recovery_ramdisk) $(recovery_kernel) \
		$(RECOVERYIMAGE_EXTRA_DEPS)
	@echo "----- Making recovery image ------"
	$(call build-recoveryimage-target, $@)
	$(hide) echo -n "SEANDROIDENFORCE" >> $(PRODUCT_OUT)/recovery.img

