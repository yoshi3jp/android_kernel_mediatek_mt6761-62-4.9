/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2020 KYOCERA Corporation
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include "kdisp_com.h"

extern void kdisp_sre_set(int sre_mode);
#if 0
static void kdisp_sre_set(int sre_mode)
{
	pr_err("[KCDISP]kdisp_sre_set + mode=%d\n", mode);

	return; //test

	if (sre_mode == DISP_EXT_SRE_OFF) {
		dsi_set_cmdq_V3(sre_off_setting, sizeof(sre_off_setting) / sizeof(struct LCM_setting_table_V3), 1);
	} else if (sre_mode == DISP_EXT_SRE_WEAK) {
		dsi_set_cmdq_V3(sre_weak_setting, sizeof(sre_weak_setting) / sizeof(struct LCM_setting_table_V3), 1);
	} else if (sre_mode == DISP_EXT_SRE_MEDIUM) {
		dsi_set_cmdq_V3(sre_medium_setting, sizeof(sre_medium_setting) / sizeof(struct LCM_setting_table_V3), 1);
	} else if (sre_mode == DISP_EXT_SRE_STRONG) {
		dsi_set_cmdq_V3(sre_strong_setting, sizeof(sre_strong_setting) / sizeof(struct LCM_setting_table_V3), 1);
	}

	pr_err("[KCDISP]kdisp_sre_set -\n");
}
#endif

void kdisp_sre_update(void)
{
	struct kcdisp_info *kctrl_data;

	kctrl_data = kdisp_drv_get_data();

	if(!kctrl_data->sre_mode_en){
		pr_debug("%s: Mode does not support SRE\n", __func__);
		return;
	}

	kdisp_sre_set(kctrl_data->sre_mode);
}

void kdisp_sre_off(void)
{
	struct kcdisp_info *kctrl_data;

	kctrl_data = kdisp_drv_get_data();

	if(!kctrl_data->sre_mode_en){
		pr_debug("%s: Mode does not support SRE\n", __func__);
		return;
	}

	kdisp_sre_set(0);
}

int kdisp_sre_init(void)
{
	struct kcdisp_info *kctrl_data;
	kctrl_data = kdisp_drv_get_data();

	kctrl_data->sre_mode = 0;
	kctrl_data->sre_mode_en = 0;
	kctrl_data->sre_mode_max = 4;

//	kctrl_data->sre_mode_en = utils->read_bool(utils->data,
//						"kc,sre-mode-enabled");
	if (!kctrl_data->sre_mode_en) {
		pr_err("[KCDISP]%s: Mode does not support SRE\n", __func__);
		return 0;
	}
//	rc = utils->read_u32(utils->data,
//						"kc,sre-mode-max", &kctrl_data->sre_mode_max);
//	if (rc) {
//		pr_err("%s: sre mode max parse failed\n", __func__);
//		goto read_error;
//	}

	pr_debug("%s: max=%u\n", __func__, kctrl_data->sre_mode_max);

	return 0;
//read_error:
//	kctrl_data->sre_mode_en = 0;
//	return -EINVAL;
}

int kdisp_get_sre_mode(void)
{
	struct kcdisp_info *kctrl_data;

	kctrl_data = kdisp_drv_get_data();

	if(!kctrl_data->sre_mode_en){
		pr_debug("%s: Mode does not support SRE\n", __func__);
		return DISP_EXT_SRE_OFF;
	}

	return kctrl_data->sre_mode;
}

int kdisp_set_sre_mode(int sre_mode)
{
	struct kcdisp_info *kctrl_data;

	kctrl_data = kdisp_drv_get_data();

	pr_debug("sre_mode = %d\n", sre_mode);

	if(!kctrl_data->sre_mode_en){
		pr_debug("%s: Mode does not support SRE\n", __func__);
		return 0;
	}
	if (sre_mode > kctrl_data->sre_mode_max) {
		pr_err("%s: invalid mode=%d\n", __func__, sre_mode);
		return 1;
	}

//	kdisp_drv_mutex_lock();

	if (sre_mode == kctrl_data->sre_mode) {
		pr_notice("%s: sre_mode is already %u\n", __func__, sre_mode);
	} else {
		if (kdisp_drv_get_panel_state()) {
			kdisp_sre_set(sre_mode);
		}

		kctrl_data->sre_mode = sre_mode;
	}

//	kdisp_drv_mutex_unlock();

	return 0;
}
