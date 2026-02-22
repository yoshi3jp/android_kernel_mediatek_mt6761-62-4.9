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

# This software is contributed or developed by KYOCERA Corporation.
# (C) 2019 KYOCERA Corporation
# (C) 2022 KYOCERA Corporation
#

ccflags-y += -I$(src)

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/mt6765
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include

obj-$(CONFIG_MTK_LEDS) += mtk_leds_drv.o

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
obj-y += $(subst ",,mt6765)/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif


# obj-y		+= kc_leds_drv.o
# obj-y		+= kc_leds_gpio_drv.o
obj-y		+= kyocera/leds-lp5569kc.o
obj-y		+= kyocera/leds-lv5216kc.o
obj-y		+= kyocera/kc_leds_api.o
