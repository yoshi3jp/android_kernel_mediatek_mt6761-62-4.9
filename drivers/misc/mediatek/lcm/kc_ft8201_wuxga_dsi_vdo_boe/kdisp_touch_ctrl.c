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

static void kdisp_touch_update_easywake_state(struct kcdisp_info *kctrl_data, int blank)
{
	if(!kctrl_data->easywake_en){
//		pr_err("%s: Mode does not support easywake\n", __func__);
		return;
	}

	pr_err("%s: blank=%d easywake_state=%d easywake_req=%d\n",
			__func__, blank, kctrl_data->easywake_state, kctrl_data->easywake_req);

	kctrl_data->easywake_state = kctrl_data->easywake_req;

	pr_err("%s: blank=%d easywake_state=%d easywake_req=%d\n",
			__func__, blank, kctrl_data->easywake_state, kctrl_data->easywake_req);
}

void kdisp_touch_set_easywake_forceoff(void)
{
	struct kcdisp_info *kctrl_data;

	pr_err("[KCDISP]%s\n", __func__);
	kctrl_data = kdisp_drv_get_data();
	kctrl_data->easywake_req = KDISP_TOUCH_EASYWAKE_STATE_OFF;
}

/*
[UNBLANK State]
[KDISP_TOUCH_ENTER_BLANK]
               | LPWG ON REQ     | LPWG OFF REQ    |
---------------+-----------------+-----------------|
LPWG ON State  | TPsus->LPWG ON  | TPsus->LPWG OFF |
---------------+-----------------+-----------------|
LPWG OFF State | TPsus->LPWG ON  | TPsus->LPWG OFF |
---------------+-----------------+-----------------|

[BLANK State]
[KDISP_TOUCH_ENTER_UNBLANK]
               | LPWG ON REQ     | LPWG OFF REQ    |
---------------+-----------------+-----------------|
LPWG ON State  | TPres->LPWG OFF | TPsus->LPWG OFF |
---------------+-----------------+-----------------|
LPWG OFF State | TPres->LPWG ON  | TPsus->LPWG ON  |
---------------+-----------------+-----------------|
*/
int kdisp_touch_get_easywake_onoff(int blank)
{
	int onoff = 0;
	struct kcdisp_info *kctrl_data;

	kctrl_data = kdisp_drv_get_data();

	if(!kctrl_data->easywake_en){
//		pr_err("%s: Mode does not support easywake\n", __func__);
		return 0;
	}

	if (blank == KDISP_TOUCH_ENTER_BLANK) {
		if (kctrl_data->easywake_req == KDISP_TOUCH_EASYWAKE_STATE_ON) {
			onoff = 1;
		}
	} else {
		if (kctrl_data->easywake_state == KDISP_TOUCH_EASYWAKE_STATE_ON) {
			onoff = 1;
		}
	}

	pr_err("%s: blank=%d easywake_state=%d easywake_req=%d onoff=%d\n",
			__func__, blank, kctrl_data->easywake_state, kctrl_data->easywake_req, onoff);

	return onoff;
}
EXPORT_SYMBOL(kdisp_touch_get_easywake_onoff);

int kdisp_touch_ctrl_power(enum kdisp_touch_ctrl_cmd cmd)
{
	struct kcdisp_info *kctrl_data;

	kctrl_data = kdisp_drv_get_data();

	if (!kctrl_data->tp_incell_en) {
		return 0;
	}

	pr_err("%s CMD=%d\n",__func__, cmd);

	switch(cmd) {
	case KDISP_TOUCH_CTRL_PRE_PANEL_POWERON:
		break;
	case KDISP_TOUCH_CTRL_POST_PANEL_POWERON:
		break;
	case KDISP_TOUCH_CTRL_PRE_PANEL_POWEROFF:
		break;
	case KDISP_TOUCH_CTRL_POST_PANEL_POWEROFF:
		kdisp_touch_update_easywake_state(kctrl_data, 1);
		break;
	case KDISP_TOUCH_CTRL_POST_PANEL_ON:
		kdisp_touch_update_easywake_state(kctrl_data, 0);
		break;
	case KDISP_TOUCH_CTRL_PRE_PANEL_RESET:
		break;
	default:
		pr_err("%s touch cmd error(%d)\n",__func__, cmd);
		break;
	}

	return 0;
}

int kdisp_touch_ctrl_init(void)
{
	struct kcdisp_info *kctrl_data;

	kctrl_data = kdisp_drv_get_data();

	kctrl_data->easywake_en    = 1;
	kctrl_data->easywake_req   = KDISP_TOUCH_EASYWAKE_STATE_OFF;
	kctrl_data->easywake_state = KDISP_TOUCH_EASYWAKE_STATE_OFF;
	kctrl_data->tp_incell_en   = 1;

	pr_notice("%s:tp_incell_en=%d\n",
			__func__, kctrl_data->tp_incell_en);
	pr_err("%s:tp_incell_en=%d\n",
			__func__, kctrl_data->tp_incell_en);

	return 0;
}
