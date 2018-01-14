# NFC
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/configs/nfc/libnfc-sec-hal.conf:system/etc/libnfc-sec-hal.conf \
	$(LOCAL_PATH)/configs/nfc/libnfc-sec.conf:system/etc/libnfc-brcm.conf \
	$(LOCAL_PATH)/configs/nfc/nfcee_access.xml:system/etc/nfcee_access.xml

PRODUCT_PACKAGES += \
	android.hardware.nfc@1.0-impl-bcm \
	nfc_nci.bcm2079x.default \
      	NfcNci \
      	Tag

PRODUCT_PROPERTY_OVERRIDES += \
	ro.nfc.sec_hal=true
