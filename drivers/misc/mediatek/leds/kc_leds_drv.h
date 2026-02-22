/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2019 KYOCERA Corporation
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#ifndef _KC_LEDS_DRV_H
#define _KC_LEDS_DRV_H

#include <linux/leds.h>
#include <mtk_leds_hal.h>

#include <asm/uaccess.h>
#include <linux/miscdevice.h>


#define TRICOLOR_DEBUG			1

#if TRICOLOR_DEBUG
#define TRICOLOR_DEBUG_LOG( msg, ... ) \
pr_debug("[TRICOLOR][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define TRICOLOR_DEBUG_LOG( msg, ... ) \
pr_err("[TRICOLOR][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#endif

#define TRICOLOR_NOTICE_LOG( msg, ... ) \
pr_notice("[TRICOLOR][%s][N](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)

#define TRICOLOR_ERR_LOG( msg, ... ) \
pr_err("[TRICOLOR][%s][E](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)

#define TRICOLOR_WARN_LOG( msg, ... ) \
pr_warn("[TRICOLOR][%s][W](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)



/****************************************************************************
 * LED DRV functions
 ***************************************************************************/
struct kc_rgb_info{
	char* 	name;
	int		point;
	 struct led_classdev *dev_class;
};

struct kc_led_info {
	struct led_classdev cdev;
	struct mutex		request_lock;
	//struct work_struct 	work;
	struct miscdevice	mdev;
	uint32_t    		mode;
	uint32_t    		on_time;
	uint32_t    		off_time;
	uint32_t    		off_color;
	bool				blue_support;
	bool				red_first_control_flag;
};



#define LEDLIGHT_BLINK_NUM		4
typedef struct _t_ledlight_ioctl {
	uint32_t data[LEDLIGHT_BLINK_NUM];
}T_LEDLIGHT_IOCTL;

#define LEDLIGHT			'L'
#define LEDLIGHT_SET_BLINK		_IOW(LEDLIGHT, 0, T_LEDLIGHT_IOCTL)

#define TRICOLOR_RGB_COLOR_RED			0x00FF0000
#define TRICOLOR_RGB_COLOR_GREEN		0x0000FF00
#define TRICOLOR_RGB_COLOR_BLUE			0x000000FF

#define TRICOLOR_RGB_MAX_BRIGHT_VAL      (0xFFFFFFFFu)
#define TRICOLOR_RGB_GET_R(color)        (((color) & 0x00FF0000u) >> 16)
#define TRICOLOR_RGB_GET_G(color)        (((color) & 0x0000FF00u) >> 8 )
#define TRICOLOR_RGB_GET_B(color)        (((color) & 0x000000FFu) >> 0 )
#define TRICOLOR_RGB_MASK                (0x00FFFFFFu)
#define TRICOLOR_RGB_OFF                 (0x00000000u)

#define LIGHT_MAIN_WLED_ALLOW true
#define LIGHT_MAIN_WLED_DISALLOW false


extern void kc_rgb_led_info_get(int num , struct led_classdev *data);

extern void kc_wled_bled_en_set(bool flag);

extern void get_timer(unsigned long *on,unsigned long *off);

extern void kc_leds_bl_mutex_lock(void);
extern void kc_leds_bl_mutex_unlock(void);
extern void kc_leds_set_bl_max_level(int max_level);
extern void kc_leds_store_bl_req_level(int level);
extern int kc_leds_show_bl_req_level(void);


#endif
