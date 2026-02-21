/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2020 KYOCERA Corporation
 */
/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __KDISP_COM_H__
#define __KDISP_COM_H__

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

/* Display detect */
#define KCDISP_DETECT_TIMEOUT			500
#define PANEL_NO_CHECK					0
#define PANEL_NOT_FOUND					-1
#define PANEL_FOUND						1
#define PANEL_DETECT_CHECK_RETRY_NUM	5

/* kdisp_sre */
enum {
	DISP_EXT_SRE_OFF,
	DISP_EXT_SRE_WEAK,
	DISP_EXT_SRE_MEDIUM,
	DISP_EXT_SRE_STRONG,
};

/* kdisp_touch_ctrl */
enum kdisp_touch_ctrl_cmd {
	KDISP_TOUCH_CTRL_PRE_PANEL_POWERON = 0,
	KDISP_TOUCH_CTRL_POST_PANEL_POWERON,
	KDISP_TOUCH_CTRL_PRE_PANEL_POWEROFF,
	KDISP_TOUCH_CTRL_POST_PANEL_POWEROFF,
	KDISP_TOUCH_CTRL_POST_PANEL_ON,
	KDISP_TOUCH_CTRL_PRE_PANEL_RESET
};

//enum kdisp_touch_state {
//       KDISP_TOUCH_STATE_PWROFF,
//       KDISP_TOUCH_STATE_PWRON,
//       KDISP_TOUCH_STATE_EASYWAKE,
//};

enum kdisp_easywake_state {
	KDISP_TOUCH_EASYWAKE_STATE_OFF,
	KDISP_TOUCH_EASYWAKE_STATE_ON,
};

#define KDISP_TOUCH_ENTER_UNBLANK	0
#define KDISP_TOUCH_ENTER_BLANK		1

#if 0
struct kc_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_det_active;
	struct pinctrl_state *gpio_state_det_suspend;
};
#endif

struct kcdisp_info {
	struct class			*class;
	struct device			*device;
	dev_t					dev_num;
	struct					cdev cdev;
//	struct miscdevice		miscdev;
	int						test_mode;
	int						panel_state;
//	int						sre_mode;
	int						refresh_disable;
	int						refresh_counter;
//	int						panel_detect_status;	// 0:no check/ 1:detect/ -1:not detect
//	int						panel_detection_1st;
//	struct mutex			kc_ctrl_lock;

	/* panel detect */
//	struct kc_pinctrl_res		pin_res;
//	bool						panel_det_en;
	int							panel_detect_status;	// 0:no check/ 1:detect/ -1:not detect
	int							panel_detect_1st;
//	int							panel_detect_dsi_read;
//	int							panel_detect_update;

	/* touch */
	bool						tp_incell_en;
	bool						easywake_en;
	int							easywake_req;
	int							easywake_state;
//	int							(*tp_display_resume) (void *data, int order);
//	int							(*tp_display_syspend) (void *data, int order);
//	int							(*tp_display_watchdog) (void *data);
//	void						(*tp_display_set_easywake_mode) (void *data, int easywake_mode);

	/* sre mode */
//	struct dsi_panel			*dsi_panel;
	int							sre_mode;
	bool						sre_mode_en;
	u32							sre_mode_max;
	struct notifier_block		reboot_notifier;
};

struct kcdisp_info *kdisp_drv_get_data(void);

/**** sre ***/
int kdisp_sre_init(void);
void kdisp_sre_update(void);
void kdisp_sre_off(void);
int kdisp_get_sre_mode(void);
int kdisp_set_sre_mode(int sre_mode);
//ssize_t kdisp_sre_mode_store(struct device *dev,
//	struct device_attribute *attr, const char *buf, size_t count);
//ssize_t kdisp_sre_mode_show(struct device *dev,
//		struct device_attribute *attr, char *buf);

/*** common ***/
void kdisp_drv_mutex_lock(void);
void kdisp_drv_mutex_unlock(void);
//void kdisp_drv_init(void);
void kdisp_drv_set_panel_state(int onoff);
int kdisp_drv_get_panel_state(void);

/* kdisp_touch_ctrl */
int kdisp_touch_ctrl_init(void);
int kdisp_touch_get_easywake_onoff(int blank);
int kdisp_touch_ctrl_power(enum kdisp_touch_ctrl_cmd cmd);
void kdisp_touch_set_easywake_forceoff(void);

#if 0
/**** connect_panel ***/
void kdisp_connect_init(struct dsi_panel *panel,
	struct kcdisp_info *kctrl_data);
int kdisp_connect_get_panel_detect(void);
void kdisp_connect_set_dsi_read_result(int result);
void kdisp_connect_panel_detect_update(void);
#endif
#endif /* __KDISP_COM_H__ */
