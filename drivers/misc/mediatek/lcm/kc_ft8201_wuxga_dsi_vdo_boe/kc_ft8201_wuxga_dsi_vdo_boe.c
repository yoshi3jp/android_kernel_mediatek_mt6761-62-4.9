/*
 * Copyright (C) 2015 MediaTek Inc.
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
/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2020 KYOCERA Corporation
*/

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include "disp_dts_gpio.h"
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#endif

#include <linux/kc_leds_drv.h>

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGE(string, args...)   dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGE(fmt, args...)      pr_err("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define GPIO_LCD_BIAS_EN_PIN		(GPIO151 | 0x80000000)
#define GPIO_DISP_LRSTB_PIN			(GPIO45 | 0x80000000)
#define GPIO_VTP_3P0_PIN			(GPIO170 | 0x80000000)
#define GPIO_VMIPI_18_PIN			(GPIO150 | 0x80000000)

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		msleep(n)  //(n >= 20ms)
#define UDELAY(n)		udelay(n)  //(n < 20ms)

#define dsi_set_cmdq_V3(para_tbl,size,force_update) \
	lcm_util.dsi_set_cmdq_V3(para_tbl,size,force_update)
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/delay.h> 
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

#include "kdisp_com.h"
#include <../video/mt6765/dispsys/ddp_hal.h>
#include <../video/mt6765/dispsys/ddp_clkmgr.h>
/*
#include <../video/mt6765/dispsys/ddp_dsi.h>
#include <../video/mt6765/dispsys/ddp_ovl.h>
#include <../cmdq/v3/cmdq_record.h>
*/

#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH		(1200)
#define FRAME_HEIGHT	(1920)
#define LCM_DENSITY		(420)

#define LCM_PHYSICAL_WIDTH	(135360)
#define LCM_PHYSICAL_HEIGHT	(216580)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//static struct LCM_setting_table_V3 pre_off_setting[] = {
//    {0x05, 0x28, 0, {}},
//    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 20, {}},
//};

static struct LCM_setting_table_V3 lcm_suspend_setting[] = {
    {0x05, 0x28, 0, {}},
    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 20, {}},
    {0x05, 0x10, 0, {}},
    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 200, {}},
};

static struct LCM_setting_table_V3 lcm_suspend_lpwg_setting[] = {
    {0x05, 0x28, 0, {}},
    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 20, {}},
    {0x15, 0x04, 1, {0x5A}},
	{0x15, 0x05, 1, {0x5A}},
    {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 20, {}},
};

static struct LCM_setting_table_V3 init_setting[] = {
	{0x05, 0x11, 0, {} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 150, {}},
	{0x15, 0x50, 1, {0x5A} },
	{0x15, 0x51, 1, {0x2F} },
	{0x05, 0x29, 0, {} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 50, {}},
};

//static struct LCM_setting_table_V3 post_init_setting[] = {
//	{0x05, 0x29, 0, {} },
//	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 50, {}},
//};

static struct LCM_setting_table_V3 sre_off_setting[] = {
};

static struct LCM_setting_table_V3 sre_weak_setting[] = {
};

static struct LCM_setting_table_V3 sre_medium_setting[] = {
};

static struct LCM_setting_table_V3 sre_strong_setting[] = {
};

void kdisp_sre_set(int sre_mode)
{
	pr_err("[KCDISP]kdisp_sre_set + mode=%d\n", sre_mode);

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

int kc_lcm_get_panel_detect(void)
{
	struct kcdisp_info *kcdisp = kdisp_drv_get_data();

	if (!kcdisp->panel_detect_1st) {
		if (kcdisp->panel_detect_status == PANEL_NOT_FOUND) {
			kcdisp->refresh_disable = 1;
			kc_wled_bled_en_set(0);
		}

		kcdisp->panel_detect_1st = 1;
	}

	return kcdisp->panel_detect_status;
}

int kdisp_lcm_get_panel_detect(void)
{
	return kc_lcm_get_panel_detect();
}

static unsigned int lcm_check_refresh_disable(void)
{
	struct kcdisp_info *kcdisp = kdisp_drv_get_data();
	
	if(kcdisp->refresh_counter < 16)
	{
		kcdisp->refresh_counter = kcdisp->refresh_counter + 1;
		return 1;	//refresh_disable
	}
	else
		return kcdisp->refresh_disable;
}

static void lcm_notify_panel_detect(int isDSIConnected)
{
	struct kcdisp_info *kcdisp = kdisp_drv_get_data();

	if (isDSIConnected == 0) {
		kcdisp->panel_detect_status = PANEL_NOT_FOUND;
		kdisp_drv_set_panel_state(0);
	} else {
		kcdisp->panel_detect_status = PANEL_FOUND;
		kdisp_drv_set_panel_state(1);
	}

	pr_notice("[KCDISP]%s panel detect:%d\n",__func__,
					kcdisp->panel_detect_status);
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;


//#if (LCM_DSI_CMD_MODE)
//	params->dsi.mode = CMD_MODE;
//	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
//	lcm_dsi_mode = CMD_MODE;
//#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
//#endif
	LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active 	= 8;
	params->dsi.vertical_backporch 		= 32;
	params->dsi.vertical_frontporch 	= 177;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active 	= 5;
	params->dsi.horizontal_backporch 	= 5;
	params->dsi.horizontal_frontporch 	= 5;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.ssc_range = 5;
#ifndef CONFIG_FPGA_EARLY_PORTING
//#if (LCM_DSI_CMD_MODE)
//	params->dsi.PLL_CLOCK = 220;
//#else
	params->dsi.PLL_CLOCK = 485;
//#endif
	#ifndef BUILD_LK
	params->dsi.PLL_CK_VDO = 485;
	#endif
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x5F;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x94;

//	params->dsi.cont_clock = 1;
}

void kdisp_touch_reset_control(bool high)
{
	pr_err("[KCDISP]%s balue = %d\n",high);
	
	if(high == true)
		disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_RST_OUT1);
	else
		disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_RST_OUT0);
}
static void lcm_init_power(void)
{
	LCM_LOGI("[KCDISP]lcm_init_power +\n");
	LCM_LOGI("[KCDISP]lcm_init_power -\n");
}

static void lcm_pre_off(void)
{
	LCM_LOGI("[KCDISP]lcm_pre_off\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		LCM_LOGE("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	kdisp_drv_set_panel_state(0);
	kc_wled_bled_en_set(0);

//	kdisp_sre_off();
//	dsi_set_cmdq_V3(pre_off_setting, sizeof(pre_off_setting) / sizeof(struct LCM_setting_table_V3), 1);

//	if(kdisp_touch_get_easywake_onoff(KDISP_TOUCH_ENTER_BLANK))
//	{
//		LCM_LOGI("[KCDISP]%s lcm_suspend_lpwg_setting\n", __func__);
//		dsi_set_cmdq_V3(lcm_suspend_lpwg_setting, sizeof(lcm_suspend_lpwg_setting) / sizeof(struct LCM_setting_table_V3), 1);
//	}
}

static void lcm_post_off(void)
{
	LCM_LOGI("[DISP]lcm_post_off\n");
	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	UDELAY(1*1000);

	if(!kdisp_touch_get_easywake_onoff(KDISP_TOUCH_ENTER_BLANK)) {
		
		/** TP ****/
		disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_RST_OUT0);
		UDELAY(1000);
		disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_SLEEP);
		UDELAY(1000);
		/** TP ****/

		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
		UDELAY(1*1000);
		
		LCM_LOGI("[KCDISP]lcm_post_off VMIPI18 OFF\n");
		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT0);
		MDELAY(70);
	}

	kdisp_touch_ctrl_power(KDISP_TOUCH_CTRL_POST_PANEL_POWEROFF);
}

void lcm_pre_on(void)
{
	LCM_LOGI("[KCDISP]lcm_pre_on\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		LCM_LOGE("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT1);
	UDELAY(10*1000);
}

void lcm_post_on(void)
{
	LCM_LOGI("[KCDISP]lcm_post_on\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		LCM_LOGE("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

//	dsi_set_cmdq_V3(post_init_setting, sizeof(post_init_setting) / sizeof(struct LCM_setting_table_V3), 1);
//	kdisp_sre_update();

	kc_wled_bled_en_set(1);
	kdisp_drv_set_panel_state(1);

	kdisp_touch_ctrl_power(KDISP_TOUCH_CTRL_POST_PANEL_ON);
}

static void lcm_init(void)
{
   LCM_LOGI("[KCDISP]lcm_init +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		LCM_LOGE("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}
	
	//LPWG
	if(kdisp_touch_get_easywake_onoff(KDISP_TOUCH_ENTER_UNBLANK))
	{
		LCM_LOGI("[KCDISP]lcm_init lpwg\n");
		disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_RST_OUT0);
		UDELAY(1*1000);
		display_bias_disable();
		MDELAY(10);
		disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
		MDELAY(60);
	}
	//LPWG END

	//disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
	UDELAY(1*1000);

	LCM_LOGI("[KCDISP]lcm 5v on\n");

	display_bias_enable();

	MDELAY(20);

	/** TP ****/
	disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_ACTIVE);
	UDELAY(100);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_RST_OUT0);
	UDELAY(3*1000);
	disp_dts_gpio_select_state(DTS_GPIO_STATE_TP_RST_OUT1);
	UDELAY(3*1000);
	/** TP ****/

	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_ACTIVE);
	MDELAY(100);

	dsi_set_cmdq_V3(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table_V3), 1);
	kdisp_sre_update();

    LCM_LOGI("[KCDISP]lcm_init -\n");
}

static void lcm_suspend(void)
{
	LCM_LOGI("[KCDISP]lcm_suspend +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		LCM_LOGE("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	kdisp_sre_off();

	if (!kdisp_touch_get_easywake_onoff(KDISP_TOUCH_ENTER_BLANK))
	{
		dsi_set_cmdq_V3(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table_V3), 1);

		LCM_LOGI("[KCDISP]lcm_suspend pinctrl 5v off\n");
		display_bias_disable();
		MDELAY(15);
	} else {		
		LCM_LOGI("[KCDISP]%s lcm_suspend lpwg_setting\n", __func__);
		dsi_set_cmdq_V3(lcm_suspend_lpwg_setting, sizeof(lcm_suspend_lpwg_setting) / sizeof(struct LCM_setting_table_V3), 1);
	}

	LCM_LOGI("[KCDISP]lcm_suspend -\n");
}

static void lcm_resume(void)
{
	LCM_LOGI("[KCDISP]lcm_resume +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	lcm_init();

	LCM_LOGI("[KCDISP]lcm_resume -\n");
}


/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
	char buffer[3];
	int array[4];

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return FALSE;
	}

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x5F, buffer, 1);

	if (buffer[0] != 0x94) {
		LCM_LOGE("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	
	LCM_LOGE("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return TRUE;
}

struct LCM_DRIVER kc_ft8201_wuxga_dsi_vdo_boe_lcm_drv = {
	.name = "kc_ft8201_wuxga_dsi_vdo_boe",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.esd_check = lcm_esd_check,
	.kdisp_lcm_drv.pre_off = lcm_pre_off,
	.kdisp_lcm_drv.post_off = lcm_post_off,
	.kdisp_lcm_drv.pre_on = lcm_pre_on,
	.kdisp_lcm_drv.post_on = lcm_post_on,
	.kdisp_lcm_drv.check_refresh_disable = lcm_check_refresh_disable,
	.kdisp_lcm_drv.panel_detect_notify = lcm_notify_panel_detect,
//	.kdisp_lcm_drv.mutex_lock = kdisp_drv_mutex_lock,
//	.kdisp_lcm_drv.mutex_unlock = kdisp_drv_mutex_unlock,
};

