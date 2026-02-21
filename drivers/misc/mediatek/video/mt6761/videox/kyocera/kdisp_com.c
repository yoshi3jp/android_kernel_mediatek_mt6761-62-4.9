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
#include <linux/moduleparam.h>
#include "../mdss.h"
#include "../mdss_dsi.h"
#include "../mdss_panel.h"
#include "../mdss_debug.h"
#include "kdisp_com.h"

#define KFBM_DISABLE 0
#define KFBM_ENABLE  1
static int kfbm_stat = KFBM_DISABLE;

static int kdisp_com_get_kfbm_status(char *buf)
{
	if (strcmp(buf, "kcfactory") == 0)
	{
		kfbm_stat = KFBM_ENABLE;
	}
	return 0;
}
early_param("androidboot.mode", kdisp_com_get_kfbm_status);

int kdisp_com_is_invalid_all(void)
{
	if (kfbm_stat == KFBM_ENABLE) {
		pr_err("%s: KFBM_ENABLE\n", __func__);
		if (kdisp_connect_get_panel_detect() == PANEL_NOT_FOUND) {
			return 1;
		}
	}

	return 0;
}

int kdisp_com_is_invalid(void)
{
	/*
	if (kdisp_diag_get_dmflag() != 0 && strcmp(current->comm, "kdispdiag") != 0) {
		return 1;
	} else {
		return 0;
	}
	*/
	return 0;
}

void kdisp_com_init(struct platform_device *pdev,
	struct device_node *np, struct kc_ctrl_info *kc_ctrl_data)
{
	//kdisp_diag_init();

	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	ctrl_pdata = platform_get_drvdata(pdev);

	kdisp_bl_ctrl_init(np, kc_ctrl_data);
	kdisp_connect_init(pdev, np, kc_ctrl_data);
	kdisp_panel_init(pdev, np, kc_ctrl_data);
//	disp_ext_panel_power_onoff(ctrl_pdata,1);
}

struct kc_ctrl_info *kdisp_get_kc_ctrl_info( struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
    struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

    pdata = dev_get_platdata(&mfd->pdev->dev);
    ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
                panel_data);

    return &ctrl_pdata->kc_ctrl_data;
}


struct mutex dsi_mtx;
extern int mdss_dsi_ctl_phy_reset(struct mdss_dsi_ctrl_pdata *ctrl, u32 event);
extern void mdss_dsi_err_intr_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, u32 mask,int enable);
int kdisp_com_dsihost_force_recovery(struct mdss_panel_data *pdata)
{
	/*
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_err("%s: force recovery!!!!\n", __func__);

	mutex_lock(&dsi_mtx);

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_ON);
	mdss_dsi_ctl_phy_reset(ctrl,
			DSI_EV_DLNx_FIFO_OVERFLOW);
	mdss_dsi_err_intr_ctrl(ctrl,
			DSI_INTR_ERROR_MASK, 1);
	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle,
			  MDSS_DSI_ALL_CLKS,
			  MDSS_DSI_CLK_OFF);

	mutex_unlock(&dsi_mtx);
	*/

	return 0;
}

int disp_ext_panel_power_onoff(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int enable)
{
	int ret;

	pr_info("%s+: enable=%d\n",__func__, enable);
	
	if (enable) 
	{
		/*
		ret = regulator_enable(ctrl_pdata->kc_ctrl_data.vdd);
		
		if(ret)
		{
			pr_err("%s vdd on error\n",__func__);
		}
		*/
		
	}
	else
	{
		ret = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");

		if (ret) {
			pr_err("request reset gpio failed, rc=%d\n",ret);
		}
		gpio_direction_output(ctrl_pdata->rst_gpio,0);
		
		usleep_range(100, 100);
		
		gpio_free(ctrl_pdata->rst_gpio);
		
		usleep_range(120000, 120000);
		
		//ret = regulator_disable(ctrl_pdata->kc_ctrl_data.vdd);
		
		if(ret)
		{
			pr_err("%s vdd off error\n",__func__);
		}
	}

	return 0;
}
