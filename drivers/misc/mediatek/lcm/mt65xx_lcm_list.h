/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT65XX_LCM_LIST_H__
#define __MT65XX_LCM_LIST_H__

#include <lcm_drv.h>

extern struct LCM_DRIVER nt36525b_vdo_hdp_boe_dijing_lcm_drv;
extern struct LCM_DRIVER nt36525b_vdo_hdp_boe_xinli_lcm_drv;
extern struct LCM_DRIVER nt36525b_vdo_hdp_boe_helitai_lcm_drv;
extern struct LCM_DRIVER nt36525b_vdo_hdp_panda_shengchao_lcm_drv;
extern struct LCM_DRIVER icnl9911c_vdo_hdp_boe_xinli_lcm_drv;
extern struct LCM_DRIVER icnl9911c_vdo_hdp_boe_tianma_lcm_drv;
extern struct LCM_DRIVER ft8006s_vdo_hdp_boe_helitai_lcm_drv;
extern struct LCM_DRIVER ft8006s_ab_vdo_hdp_boe_helitai_lcm_drv;
extern struct LCM_DRIVER ft8006s_ac_vdo_hdp_boe_helitai_lcm_drv;
extern struct LCM_DRIVER hx83102d_vdo_hdp_boe_xinli_lcm_drv;

#ifdef BUILD_LK
extern void mdelay(unsigned long msec);
#endif

#endif
