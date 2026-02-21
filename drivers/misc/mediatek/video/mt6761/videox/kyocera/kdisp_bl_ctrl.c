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
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/kc_led.h>
#include "../mdss_dsi.h"
#include "kdisp_com.h"

#define KDISP_BL_DELAYTIME		40

static int kdisp_bl_ctrl_led_type = 0;

#define BOOT_LED_STAT_BOARD_ID    0x01000000
int kdisp_stat_aboot_board_id = 0;

static int __init kdisp_set_led_stat_aboot(char *buf)
{
	int ret;
	unsigned long led_stat_aboot = 0;

	ret = kstrtoul(buf, 0, &led_stat_aboot);
	if (ret) {
		pr_err("[KCDISP] %s: failed to kstrtoul ret=%d", __func__, ret);
	} else {
		if (led_stat_aboot&BOOT_LED_STAT_BOARD_ID) {
			kdisp_stat_aboot_board_id = 1;
		} else {
			kdisp_stat_aboot_board_id = 0;
		}
		pr_err("[KCDISP] %s: led board_id=%d", __func__, kdisp_stat_aboot_board_id);
	}
	return 0;
}
early_param("led_stat", kdisp_set_led_stat_aboot);

static void on_comp_notify(struct work_struct *p)
{
#ifdef ENABLE_LCD_DETECTION
	pr_debug("%s: notify ON to LED\n", __func__);

	if (kdisp_bl_ctrl_led_type == 0){
		lv5216_light_led_disp_power_set(LIGHT_MAIN_WLED_LCD_PWREN);
	} else {
		lp5569_light_led_disp_power_set(LIGHT_MAIN_WLED_LCD_PWREN);
	}
#endif /* ENABLE_LCD_DETECTION */
}

void kdisp_bl_ctrl_onoff(struct kc_ctrl_info *kctrl_data, bool onoff)
{
#ifdef ENABLE_LCD_DETECTION
	if (kctrl_data->wled_disp_power_set == NULL) {
		pr_err("%s: wled_disp_power_set error\n", __func__);
	}

	if(onoff) {
		schedule_delayed_work(&kctrl_data->on_comp_work, msecs_to_jiffies(KDISP_BL_DELAYTIME));
	} else {
		cancel_delayed_work_sync(&kctrl_data->on_comp_work);
		pr_debug("%s: notify OFF to LED\n", __func__);
		kctrl_data->wled_disp_power_set(LIGHT_MAIN_WLED_LCD_PWRDIS);
	}
#endif /* ENABLE_LCD_DETECTION */
}

void kdisp_bl_ctrl_init(struct device_node *np, struct kc_ctrl_info *kctrl_data)
{
	int rc = 0;
	const char *string;

	kctrl_data->wled_disp_power_set = NULL;

	if (kdisp_stat_aboot_board_id) {
			kdisp_bl_ctrl_led_type = 1;
			kctrl_data->wled_disp_power_set = lp5569_light_led_disp_power_set;
	} else {
			kdisp_bl_ctrl_led_type = 0;
			kctrl_data->wled_disp_power_set = lv5216_light_led_disp_power_set;
	}

	INIT_DELAYED_WORK(&kctrl_data->on_comp_work, on_comp_notify);
}
