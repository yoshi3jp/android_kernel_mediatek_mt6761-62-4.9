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
 * (C) 2019 KYOCERA Corporation
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

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_notice("[KERNEL/"LOG_TAG"]"fmt, ##args)
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


#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH		(720)
#define FRAME_HEIGHT	(1480)
#define LCM_DENSITY		(320)

#define LCM_PHYSICAL_WIDTH	(62352)
#define LCM_PHYSICAL_HEIGHT	(128168)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static struct LCM_setting_table_V3 lcm_suspend_setting[] = {
	{0x05, 0x28, 0, {}},
	{0x05, 0x10, 0, {}},
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 120, {}},
};

static struct LCM_setting_table_V3 init_setting[] = {
	{REGFLAG_ESCAPE_ID,REGFLAG_HSCLK_EN_V3, 1, {}},
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 5, {}},
	{0x05, 0x11, 0, {} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 20, {}},
	{0x15, 0x51, 1, {0xFF} },
	{0x15, 0x53, 1, {0x20} },
	{0x39, 0xF0, 2, {0x5A, 0x5A} },
	{0x39, 0xC0, 3, {0xD8, 0xD8, 0x00} },
	{0x39, 0xF0, 2, {0xA5, 0xA5} },
	{0x39, 0xF0, 2, {0x5A, 0x5A} },
	{0x39, 0xBD, 2, {0x11, 0x44} },
	{0x39, 0xF0, 2, {0xA5, 0xA5} },
	{0x39, 0xF0, 2, {0x5A, 0x5A} },
	{0x39, 0xF2, 4, {0x0C, 0x0C, 0xB9, 0x01} },
	{0x39, 0xF0, 2, {0xA5, 0xA5} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 120, {}},
};

static struct LCM_setting_table_V3 post_init_setting[] = {
	{0x05, 0x29, 0, {}},
};

#ifdef DSITXV3
static struct LCM_setting_table_V3 bl_level[] = {
	{0x15, 0x51, 1, {0xFF} },
	{0x15, 0x53, 1, {0x20} },
};
#else
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF
static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{0x53, 1, {0x20} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
#endif

#define PANEL_NO_CHECK					0
#define PANEL_NOT_FOUND					-1
#define PANEL_FOUND						1
#define PANEL_DETECT_CHECK_RETRY_NUM	5

struct kcdisp_data {
	struct class			*class;
	struct device			*device;
	dev_t					dev_num;
	struct					cdev cdev;
	struct miscdevice		miscdev;
	int						test_mode;
	int						sre_mode;
	int						refresh_disable;
	int						panel_detect_status;	// 0:no check/ 1:detect/ -1:not detect
	int						panel_detection_1st;
};

static struct kcdisp_data kcdisp_data;
static int kc_lcm_get_panel_detect(void);

///////////////////////////////////////////////////////////////

#ifndef BUILD_LK

#define CLASS_NAME "kcdisp"
#define DRIVER_NAME "kcdisp"
static ssize_t kcdisp_test_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->test_mode);
}

static ssize_t kcdisp_test_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	rc = kstrtoint(buf, 10, &kcdisp->test_mode);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_err("[KCLCM]test_mode=%d\n", buf);

	return count;
}

static ssize_t kcdisp_sre_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->sre_mode);
}

static ssize_t kcdisp_sre_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	rc = kstrtoint(buf, 10, &kcdisp->sre_mode);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_err("[KCLCM]sre_mode=%d\n", buf);

	return count;
}

static ssize_t kcdisp_disp_connect_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "lcm_det=%d\n",
			kcdisp->panel_detect_status);
}

static ssize_t kcdisp_disp_connect_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	rc = kstrtoint(buf, 10, &kcdisp->panel_detect_status);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_err("[KCLCM]panel_detect_status=%d\n", buf);

	return count;
}

static ssize_t kcdisp_refresh_disable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->refresh_disable);
}

static ssize_t kcdisp_refresh_disable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_data *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return 0;
	}

	rc = kstrtoint(buf, 10, &kcdisp->refresh_disable);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_notice("[KCLCM]refresh_disable=%d\n", kcdisp->refresh_disable);

	return count;
}

static DEVICE_ATTR(test_mode, S_IRUGO | S_IWUSR,
	kcdisp_test_mode_show, kcdisp_test_mode_store);
static DEVICE_ATTR(sre_mode, S_IRUGO | S_IWUSR,
	kcdisp_sre_mode_show, kcdisp_sre_mode_store);
static DEVICE_ATTR(disp_connect, S_IRUGO | S_IWUSR,
	kcdisp_disp_connect_show, kcdisp_disp_connect_store);
static DEVICE_ATTR(refresh_disable, S_IRUGO | S_IWUSR,
	kcdisp_refresh_disable_show, kcdisp_refresh_disable_store);

static struct attribute *kcdisp_attrs[] = {
	&dev_attr_test_mode.attr,
	&dev_attr_sre_mode.attr,
	&dev_attr_disp_connect.attr,
	&dev_attr_refresh_disable.attr,
	NULL,
};

static struct attribute_group kcdisp_attr_group = {
	.attrs = kcdisp_attrs,
};

#if 0
static int kcdisp_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static int kcdisp_release(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static long kcdisp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	(void)filp;
	(void)cmd;
	(void)arg;

	return 0;
}
#endif

const struct file_operations kcdisp_fops = {
	.owner = THIS_MODULE,
#if 0
	.open           = kcdisp_open,
	.release        = kcdisp_release,
	.unlocked_ioctl = kcdisp_ioctl,
	.compat_ioctl	= kcdisp_ioctl,
#endif
};

static int kc_lcm_init_sysfs(struct kcdisp_data *kcdisp)
{
	int ret = 0;

	ret = alloc_chrdev_region(&kcdisp->dev_num, 0, 1, DRIVER_NAME);
	if (ret  < 0) {
		pr_err("alloc_chrdev_region failed ret = %d\n", ret);
		goto done;
	}

	kcdisp->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(kcdisp->class)) {
		pr_err("%s class_create error!\n",__func__);
		goto done;
	}

	kcdisp->device = device_create(kcdisp->class, NULL,
		kcdisp->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(kcdisp->device)) {
		ret = PTR_ERR(kcdisp->device);
		pr_err("device_create failed %d\n", ret);
		goto done;
	}

	dev_set_drvdata(kcdisp->device, kcdisp);

	cdev_init(&kcdisp->cdev, &kcdisp_fops);
	ret = cdev_add(&kcdisp->cdev,
			MKDEV(MAJOR(kcdisp->dev_num), 0), 1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto done;
	}

	ret = sysfs_create_group(&kcdisp->device->kobj,
			&kcdisp_attr_group);
	if (ret)
		pr_err("unable to register rotator sysfs nodes\n");

done:
	return ret;
}

static int kc_lcm_platform_probe(struct platform_device *pdev)
{
	struct kcdisp_data *kcdisp;

	kcdisp = &kcdisp_data;

	kcdisp->test_mode           = 0;
	kcdisp->sre_mode            = 0;
	kcdisp->refresh_disable     = 0;
	kcdisp->panel_detect_status = PANEL_NO_CHECK;
	kcdisp->panel_detection_1st = 0;

	kc_lcm_init_sysfs(kcdisp);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "kc,kc_lcm",},
	{}
};
MODULE_DEVICE_TABLE(of, lcm_of_ids);
#endif

static struct platform_driver lcm_driver = {
	.probe = kc_lcm_platform_probe,
	.driver = {
		   .name = "kc_lcm",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init kc_lcm_init(void)
{
	pr_notice("[KCDISP]LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_debug("[KCDISP]LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit kc_lcm_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("[KCLCM]LCM: Unregister lcm driver done\n");
}

module_init(kc_lcm_init);
module_exit(kc_lcm_exit);
MODULE_AUTHOR("Kyocera");
MODULE_DESCRIPTION("KC Display subsystem Driver");
MODULE_LICENSE("GPL");
#endif
///////////////////////////////////////////////////////////////

static int kc_lcm_get_panel_detect(void)
{
	if (!kcdisp_data.panel_detection_1st) {
		if (kcdisp_data.panel_detect_status == PANEL_NOT_FOUND) {
			kcdisp_data.refresh_disable = 1;
		}

		kcdisp_data.panel_detection_1st = 1;
	}

	return kcdisp_data.panel_detect_status;
}

int kdisp_lcm_get_panel_detect(void)
{
	return kc_lcm_get_panel_detect();
}

static unsigned int lcm_check_refresh_disable(void)
{
	return kcdisp_data.refresh_disable;
}

static void lcm_notify_panel_detect(int isDSIConnected)
{
	if (isDSIConnected == 0) {
		kcdisp_data.panel_detect_status = PANEL_NOT_FOUND;
	} else {
		kcdisp_data.panel_detect_status = PANEL_FOUND;
	}

	pr_notice("[KCDISP]%s panel detect:%d\n",__func__,
					kcdisp_data.panel_detect_status);
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


#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = BURST_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	lcm_dsi_mode = BURST_VDO_MODE;
#endif
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

	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 12;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 12;
	params->dsi.horizontal_backporch = 16;
	params->dsi.horizontal_frontporch = 128;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.ssc_range = 5;
#ifndef CONFIG_FPGA_EARLY_PORTING
	params->dsi.PLL_CLOCK = 250;
	params->dsi.PLL_CK_VDO = 250;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

	params->dsi.cont_clock = 1;
}

static void lcm_init_power(void)
{
	pr_notice("[KCDISP]lcm_init_power +\n");
	pr_notice("[KCDISP]lcm_init_power -\n");
}

static void lcm_pre_off(void)
{
	pr_notice("[KCDISP]lcm_pre_off\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

//	pr_err("[KCDISP]lcm_pre_off oled");
	dsi_set_cmdq_V3(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table_V3), 1);
}

static void lcm_post_off(void)
{
	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	pr_notice("[KCDISP]lcm_post_off\n");
	
#ifdef BUILD_LK

	SET_RESET_PIN(0);
	MDELAY(15);

    mt_set_gpio_mode(GPIO_DISP_LRSTB_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_DISP_LRSTB_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
	MDELAY(15);

	mt_set_gpio_mode(GPIO_LCD_BIAS_EN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_BIAS_EN_PIN, GPIO_OUT_ZERO);
	MDELAY(30);

	mt_set_gpio_mode(GPIO_VMIPI_18_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_VMIPI_18_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_VMIPI_18_PIN, GPIO_OUT_ZERO);
	MDELAY(5);

	mt_set_gpio_mode(GPIO_VTP_3P0_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_VTP_3P0_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_VTP_3P0_PIN, GPIO_OUT_ZERO);
	MDELAY(10);

	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT0);
	MDELAY(30);

#else
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
	UDELAY(1*1000);

	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT0);
	UDELAY(5*1000);
	
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_OUT0);
	UDELAY(10*1000);
#endif

}

void lcm_pre_on(void)
{
	pr_notice("[KCDISP]lcm_pre_on\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

//	pr_notice("[KCDISP]VOLED3p0 ON\n");
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_BIAS_OUT1);
	UDELAY(5*1000);

//	pr_notice("[KCDISP]VMIPI ON\n");
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_VMIPI_OUT1);
	UDELAY(10*1000);

//	pr_notice("[KCDISP]lcm_pre_on -\n");
}

void lcm_post_on(void)
{
	pr_notice("[KCDISP]lcm_post_on\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

	dsi_set_cmdq_V3(post_init_setting, sizeof(post_init_setting) / sizeof(struct LCM_setting_table_V3), 1);
}

static void lcm_init(void)
{
   LCM_LOGI("[KCDISP]lcm_init +\n");

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
	}

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO_DISP_LRSTB_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_DISP_LRSTB_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
	MDELAY(5);

	mt_set_gpio_mode(GPIO_VMIPI_18_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_VMIPI_18_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_VMIPI_18_PIN, GPIO_OUT_ONE);
	MDELAY(5);

	mt_set_gpio_mode(GPIO_LCD_BIAS_EN_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_BIAS_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCD_BIAS_EN_PIN, GPIO_OUT_ONE);
	MDELAY(30);

	mt_set_gpio_mode(GPIO_DISP_LRSTB_PIN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_DISP_LRSTB_PIN, GPIO_DIR_OUT);	
    mt_set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
   	MDELAY(5);
    mt_set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ZERO);
    MDELAY(3);
    mt_set_gpio_out(GPIO_DISP_LRSTB_PIN, GPIO_OUT_ONE);
    MDELAY(10);

#else
//	pr_notice("[KCDISP]RST H\n");
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_ACTIVE);
	UDELAY(5*1000);

//	pr_notice("[KCDISP]RST L\n");
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_SLEEP);
	UDELAY(3*1000);

//	pr_notice("[KCDISP]RST H\n");
	disp_dts_gpio_select_state(DTS_GPIO_STATE_LCD_RST_ACTIVE);
	UDELAY(10*1000);
#endif

//	pr_notice("[KCDISP]CMD\n");
    dsi_set_cmdq_V3(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table_V3), 1);  

    LCM_LOGI("[KCDISP]lcm_init -\n");
}

static void lcm_suspend(void)
{
	LCM_LOGI("[KCDISP]lcm_suspend +\n");
	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return;
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

#ifdef DSITXV3
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35521 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	dsi_set_cmdq_V3(bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table_V3), 1);
}
#else

static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {

		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
		}
	}
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35521 backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}
#endif

/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return FALSE;
	}

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
#endif
	return FALSE;
}

struct LCM_DRIVER kc_s6e8aa5x01_hdplus_dsi_vdo_samsung_lcm_drv = {
	.name = "kc_s6e8aa5x01_hdplus_dsi_vdo_samsung",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.kdisp_lcm_drv.pre_off = lcm_pre_off,
	.kdisp_lcm_drv.post_off = lcm_post_off,
	.kdisp_lcm_drv.pre_on = lcm_pre_on,
	.kdisp_lcm_drv.post_on = lcm_post_on,
	.kdisp_lcm_drv.check_refresh_disable = lcm_check_refresh_disable,
	.kdisp_lcm_drv.panel_detect_notify = lcm_notify_panel_detect,
};

