#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# drivers/image/jpeg/Makefile

ifeq ($(CONFIG_MACH_MT6765),y)
  ccflags-y += -DSMI_CG_SUPPORT
  ccflags-y += -DQOS_MT6765_SUPPORT
  ccflags-y += -DPLATFORM_MT6765
endif

ifeq (y, $(filter y,$(CONFIG_MACH_MT6885) $(CONFIG_MACH_MT6893)))
  ccflags-y += -DJPEG_HYBRID_DEC_DRIVER
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/mtk
#obj-$(CONFIG_MTK_JPEG) += jpeg_driver.o
obj-y += jpeg_driver.o

ifdef CONFIG_MTK_PSEUDO_M4U
  ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
  ccflags-y += -I$(srctree)/drivers/iommu/
  ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)
endif

jpeg_driver-objs := jpeg_drv_enc.o jpeg_drv_dec.o jpeg_drv.o jpeg_ion.o
