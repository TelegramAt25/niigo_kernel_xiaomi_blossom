#
# Makefile for all kinds of Fingerprint
#
obj-$(CONFIG_GOODIX_FINGERPRINT) += goodix/
obj-$(CONFIG_FPC_FINGERPRINT) += fpc/
obj-$(CONFIG_FOCALTECH_FINGERPRINT) += focaltech/
subdir-ccflags-y := \
    -fdata-sections -ffunction-sections -finline-functions -ftree-vectorize -funroll-loops

obj-$(CONFIG_FINGERPRINT_GOODIX_GF3208) += goodix/
obj-$(CONFIG_FINGERPRINT_FPC1020) += fpc/

