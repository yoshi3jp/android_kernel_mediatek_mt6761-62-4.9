/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2016 KYOCERA Corporation
 * (C) 2017 KYOCERA Corporation
 * (C) 2018 KYOCERA Corporation
 * (C) 2019 KYOCERA Corporation
 * (C) 2020 KYOCERA Corporation
 * (C) 2021 KYOCERA Corporation
 */
/*
 * This program is free software; you can redistribute it and/or
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/wakelock.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/reboot.h>

#include <linux/leds.h>
#include <linux/kc_leds_drv.h>

// function switch
//#define DISABLE_DISP_DETECT
//#define FORCE_WLED_ALWAYS_ON
//#define FORCE_WLED_ON
#define KEEP_DEVICE_STATE_ON_PROBE
//#define ENABLE_PWM
//#define LIGHT_ALC_ENABLE

#ifdef FORCE_WLED_ALWAYS_ON
#ifndef FORCE_WLED_ON
#define FORCE_WLED_ON
#endif
#ifdef KEEP_DEVICE_STATE_ON_PROBE
#undef KEEP_DEVICE_STATE_ON_PROBE
#endif
#endif


// debug
//#define DEBUG (1)
//#define LV5216_DEBUG

#ifdef LV5216_DEBUG
#define LV5216_V_LOG(msg, ...)	\
	pr_notice("[LEDDRV][LV5216][%s][V](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#define LV5216_D_LOG(msg, ...)	\
	pr_notice("[LEDDRV][LV5216][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#else
#define LV5216_V_LOG(msg, ...)
#define LV5216_D_LOG(msg, ...)	\
	pr_debug ("[LEDDRV][LV5216][%s][D](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#endif

#define LV5216_E_LOG(msg, ...)	\
	pr_err   ("[LEDDRV][LV5216][%s][E](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)
#define LV5216_N_LOG(msg, ...)	\
	pr_notice("[LEDDRV][LV5216][%s][N](%d): " msg "\n", __func__, __LINE__, ## __VA_ARGS__)


#define LV5216_DRV_NAME "LV5216"

#define I2C_RETRIES_NUM				(5)
#define LV5216_RESET_GPIO			(0)
#define LV5216_I2C_WRITE_MSG_NUM	(1)
#define LV5216_I2C_READ_MSG_NUM		(2)
#define LV5216_WRITE_BUF_LEN		(2)

#define LV5216_COMBINATION_ILLUM1		0x82818181
#define LV5216_COMBINATION_ILLUM1_MAX	10
static uint32_t combination_illumination_tbl[LV5216_COMBINATION_ILLUM1_MAX] = {
	0x00FF0000,
	0x0000FF00,
	0x000000FF,
	0x00FFFF00,
	0x00FF00FF,
	0x0000FFFF,
	0x00FF00FF,
	0x00FFFF00,
	0x000000FF,
	0x0000FF00
};
static int lv5216_led_illum_count = 0;

#define LV5216_REG_MAX (40)
struct lv5216_reg_info {
	uint8_t	value;
	bool	written;
};

struct lv5216_reg_write_info {
	uint8_t	reg;
	uint8_t	val;
	uint8_t	mask;
};

static struct lv5216_reg_info lv5216_reg_info_save[LV5216_REG_MAX] =
{
/*00,00h*/	{	0x00,	false },
/*01,01h*/	{	0x00,	false },
/*02,02h*/	{	0x08,	false },
/*03,03h*/	{	0x48,	false },
/*04,04h*/	{	0x00,	false },
/*05,05h*/	{	0x00,	false },
/*06,06h*/	{	0x0A,	false },
/*07,07h*/	{	0x08,	false },
/*08,08h*/	{	0x0A,	false },
/*09,09h*/	{	0x9F,	false },
/*10,0Ah*/	{	0x02,	false },
/*11,0Bh*/	{	0x80,	false },
/*12,0Ch*/	{	0x00,	false },
/*13,0Dh*/	{	0x00,	false },
/*14,0Eh*/	{	0x00,	false },
/*15,0Fh*/	{	0x00,	false },
/*16,10h*/	{	0x00,	false },
/*17,11h*/	{	0x00,	false },
/*18,12h*/	{	0x00,	false },
/*19,13h*/	{	0x00,	false },
/*20,14h*/	{	0x07,	false },
};

#define LV5216_LED_ENABLE			0x1f
#define LV5216_LED_DISABLE			0x00
#define LV5216_LED_BRIGHT_MASK		0xff

#define LV5216_VLED_ON				0x81
#define LV5216_VLED_OFF				0x00

#define NV_BK_LIGHT_CTL_LOWER_L_I_DEF	(0x02)
#define KEYLED_THRESHOLD_DEFAULT	NV_BK_LIGHT_CTL_LOWER_L_I_DEF

#define LABEL_MLED		"mled"
#define LABEL_RGB		"rgb"
#define LABEL_CLED		"cled"
#define LABEL_KEYLED	"keyled"
#define LABEL_SLED		"sled"
#define LABEL_ALED		"aled"

#define NUM_OF_COLORVARIATION	(5)
#define COLORVARIATION_DEFAULT	(3)

enum {
	LED_COLOR_B = 0,
	LED_COLOR_R,
	LED_COLOR_G,
	LED_COLOR_RB,
	LED_COLOR_GB,
	LED_COLOR_RG,
	LED_COLOR_RGB,
	LED_COLOR_MAX
};

static int rgb_table[LED_COLOR_MAX+1] = {
	-1,
	LED_COLOR_R,
	LED_COLOR_G,
	LED_COLOR_RG,
	LED_COLOR_B,
	LED_COLOR_RB,
	LED_COLOR_GB,
	LED_COLOR_RGB
};

#define	lv5216_get_color(n)	rgb_table[n]

enum {
	LEDLIGHT_PATTERN_MANUAL = 0,
	LEDLIGHT_PATTERN_1,
	LEDLIGHT_PATTERN_2,
	LEDLIGHT_PATTERN_3,
	LEDLIGHT_PATTERN_4,
	LEDLIGHT_PATTERN_5,
	LEDLIGHT_PATTERN_6,
	LEDLIGHT_PATTERN_7,
	LEDLIGHT_PATTERN_8,
	LEDLIGHT_PATTERN_9,
	LEDLIGHT_PATTERN_10,
	LEDLIGHT_PATTERN_11,
	LEDLIGHT_PATTERN_12,
	LEDLIGHT_PATTERN_MAX
};

#define BRIGHTNESS_MAX	(0xFFFFFFFFul)
#define BL_BRIGHTNESS_MAX	255

enum {
	LED_TYPE_MLED = 0,
	LED_TYPE_RGB,
	LED_TYPE_CLED,
	LED_TYPE_KEYLED,
	LED_TYPE_SLED,
	LED_TYPE_ALED,
	LED_TYPE_MAX
};

enum {
	CTRL_MODE_MANUAL = 0,
	CTRL_MODE_ALC,
	CTRL_MODE_ALC_CAMERA,
	CTRL_MODE_DIM,
	CTRL_MODE_MAX
};

#define GET_CTRL_MODE_EXT(a)		(a & 0xf0)
#define GET_CTRL_MODE(a)			(a & 0x0f)
#define CTRL_MODE_EXT_CAMERA		0x10
//static uint32_t ext_ctrl_mode;

struct lv5216_led_param {
	uint32_t				value;

	// for rgb
	uint32_t				blink_control;
	uint32_t				blink_low_pause_time;
	uint32_t				blink_high_pause_time;
	uint32_t				blink_off_color;

	// for mled/keyled
	uint32_t				ctrl_mode_mled;
	uint32_t				ctrl_mode_keyled;

	// for keyled
	uint32_t				keyled_threshold;
};

struct lv5216_data;
struct lv5216_led_data;
typedef void (*lv5216_ctrl_func_t)(struct lv5216_led_data *led,
	struct lv5216_led_param *param_next);

struct lv5216_led_data {
	struct lv5216_data			*parent;
	struct led_classdev			cdev;
	bool						cdev_registered;
	struct work_struct			work;
	struct workqueue_struct		*workqueue;
	int							led_type;
	lv5216_ctrl_func_t			ctrl_func;
	struct mutex				param_next_lock;
	struct lv5216_led_param		param_next;
	struct lv5216_led_param		param_prev;
};

struct lv5216_blink_ctl {
	bool						next;
	struct lv5216_led_param		param;
	bool						blink_round_en;
	uint32_t					blink_round_ontime;
	uint32_t					blink_round_offtime;
};

struct lv5216_data {
	struct i2c_client			*client;
	int							reset_gpio;
//	int							keyled_gpio;
	int							keyled_on_gpio;
	int							sled_gpio;
//	struct mdm_led				*mdm_led;

	struct mutex				control_lock;
	struct lv5216_led_data		led_data[LED_TYPE_MAX];
	struct mutex				param_lock;
	struct lv5216_led_param		param;
	int							power_state;
	int							power_vkey_state;
	int							photo_sensor_state;
	struct delayed_work			blink_work;
	struct lv5216_blink_ctl		blink_ctl;
	struct wake_lock			wake_lock;
	struct workqueue_struct		*work_queue;
	int							colorvariation;
	struct mutex				mled_lock;

	struct pinctrl				*dc_pinctrl; //dc: device check
	struct pinctrl_state        *dc_pd; //pd:pull down
	struct pinctrl_state        *dc_np; //np:non pull
	int							basecheck_gpio;
	struct notifier_block		reboot_notifier;
};

static struct lv5216_data *lv5216_data;

static atomic_t g_panel_class = ATOMIC_INIT(0);

static const uint8_t mled_reg03_reg01_manual[256][2] = {
	{	0x00, 0x00	},	//0
	{	0x20, 0x1D	},	//1
	{	0x20, 0x1D	},	//2
	{	0x20, 0x1D	},	//3
	{	0x20, 0x1D	},	//4
	{	0x20, 0x1D	},	//5
	{	0x20, 0x1D	},	//6
	{	0x20, 0x1D	},	//7
	{	0x20, 0x1F	},	//8
	{	0x20, 0x1F	},	//9
	{	0x20, 0x1F	},	//10
	{	0x20, 0x1F	},	//11
	{	0x40, 0x1F	},	//12
	{	0x40, 0x1F	},	//13
	{	0x40, 0x1F	},	//14
	{	0x40, 0x1F	},	//15
	{	0x60, 0x1F	},	//16
	{	0x60, 0x1F	},	//17
	{	0x60, 0x1F	},	//18
	{	0x21, 0x1F	},	//19
	{	0x21, 0x1F	},	//20
	{	0x21, 0x1F	},	//21
	{	0x21, 0x1F	},	//22
	{	0x41, 0x1F	},	//23
	{	0x41, 0x1F	},	//24
	{	0x41, 0x1F	},	//25
	{	0x41, 0x1F	},	//26
	{	0x61, 0x1F	},	//27
	{	0x61, 0x1F	},	//28
	{	0x61, 0x1F	},	//29
	{	0x61, 0x1F	},	//30
	{	0x22, 0x1F	},	//31
	{	0x22, 0x1F	},	//32
	{	0x22, 0x1F	},	//33
	{	0x42, 0x1F	},	//34
	{	0x42, 0x1F	},	//35
	{	0x42, 0x1F	},	//36
	{	0x42, 0x1F	},	//37
	{	0x62, 0x1F	},	//38
	{	0x62, 0x1F	},	//39
	{	0x62, 0x1F	},	//40
	{	0x62, 0x1F	},	//41
	{	0x23, 0x1F	},	//42
	{	0x23, 0x1F	},	//43
	{	0x23, 0x1F	},	//44
	{	0x23, 0x1F	},	//45
	{	0x43, 0x1F	},	//46
	{	0x43, 0x1F	},	//47
	{	0x43, 0x1F	},	//48
	{	0x63, 0x1F	},	//49
	{	0x63, 0x1F	},	//50
	{	0x63, 0x1F	},	//51
	{	0x63, 0x1F	},	//52
	{	0x24, 0x1F	},	//53
	{	0x24, 0x1F	},	//54
	{	0x24, 0x1F	},	//55
	{	0x24, 0x1F	},	//56
	{	0x44, 0x1F	},	//57
	{	0x44, 0x1F	},	//58
	{	0x44, 0x1F	},	//59
	{	0x44, 0x1F	},	//60
	{	0x64, 0x1F	},	//61
	{	0x64, 0x1F	},	//62
	{	0x64, 0x1F	},	//63
	{	0x64, 0x1F	},	//64
	{	0x25, 0x1F	},	//65
	{	0x25, 0x1F	},	//66
	{	0x25, 0x1F	},	//67
	{	0x25, 0x1F	},	//68
	{	0x45, 0x1F	},	//69
	{	0x45, 0x1F	},	//70
	{	0x45, 0x1F	},	//71
	{	0x45, 0x1F	},	//72
	{	0x65, 0x1F	},	//73
	{	0x65, 0x1F	},	//74
	{	0x65, 0x1F	},	//75
	{	0x65, 0x1F	},	//76
	{	0x26, 0x1F	},	//77
	{	0x26, 0x1F	},	//78
	{	0x26, 0x1F	},	//79
	{	0x26, 0x1F	},	//80
	{	0x46, 0x1F	},	//81
	{	0x46, 0x1F	},	//82
	{	0x46, 0x1F	},	//83
	{	0x46, 0x1F	},	//84
	{	0x66, 0x1F	},	//85
	{	0x66, 0x1F	},	//86
	{	0x66, 0x1F	},	//87
	{	0x66, 0x1F	},	//88
	{	0x27, 0x1F	},	//89
	{	0x27, 0x1F	},	//90
	{	0x27, 0x1F	},	//91
	{	0x47, 0x1F	},	//92
	{	0x47, 0x1F	},	//93
	{	0x47, 0x1F	},	//94
	{	0x47, 0x1F	},	//95
	{	0x67, 0x1F	},	//96
	{	0x67, 0x1F	},	//97
	{	0x67, 0x1F	},	//98
	{	0x28, 0x1F	},	//99
	{	0x28, 0x1F	},	//100
	{	0x28, 0x1F	},	//101
	{	0x28, 0x1F	},	//102
	{	0x48, 0x1F	},	//103
	{	0x48, 0x1F	},	//104
	{	0x48, 0x1F	},	//105
	{	0x68, 0x1F	},	//106
	{	0x68, 0x1F	},	//107
	{	0x68, 0x1F	},	//108
	{	0x68, 0x1F	},	//109
	{	0x29, 0x1F	},	//110
	{	0x29, 0x1F	},	//111
	{	0x29, 0x1F	},	//112
	{	0x49, 0x1F	},	//113
	{	0x49, 0x1F	},	//114
	{	0x49, 0x1F	},	//115
	{	0x49, 0x1F	},	//116
	{	0x69, 0x1F	},	//117
	{	0x69, 0x1F	},	//118
	{	0x69, 0x1F	},	//119
	{	0x2A, 0x1F	},	//120
	{	0x2A, 0x1F	},	//121
	{	0x2A, 0x1F	},	//122
	{	0x2A, 0x1F	},	//123
	{	0x4A, 0x1F	},	//124
	{	0x4A, 0x1F	},	//125
	{	0x4A, 0x1F	},	//126
	{	0x6A, 0x1F	},	//127
	{	0x6A, 0x1F	},	//128
	{	0x6A, 0x1F	},	//129
	{	0x6A, 0x1F	},	//130
	{	0x2B, 0x1F	},	//131
	{	0x2B, 0x1F	},	//132
	{	0x2B, 0x1F	},	//133
	{	0x4B, 0x1F	},	//134
	{	0x4B, 0x1F	},	//135
	{	0x4B, 0x1F	},	//136
	{	0x4B, 0x1F	},	//137
	{	0x6B, 0x1F	},	//138
	{	0x6B, 0x1F	},	//139
	{	0x6B, 0x1F	},	//140
	{	0x2C, 0x1F	},	//141
	{	0x2C, 0x1F	},	//142
	{	0x2C, 0x1F	},	//143
	{	0x2C, 0x1F	},	//144
	{	0x4C, 0x1F	},	//145
	{	0x4C, 0x1F	},	//146
	{	0x4C, 0x1F	},	//147
	{	0x6C, 0x1F	},	//148
	{	0x6C, 0x1F	},	//149
	{	0x6C, 0x1F	},	//150
	{	0x6C, 0x1F	},	//151
	{	0x2D, 0x1F	},	//152
	{	0x2D, 0x1F	},	//153
	{	0x2D, 0x1F	},	//154
	{	0x4D, 0x1F	},	//155
	{	0x4D, 0x1F	},	//156
	{	0x4D, 0x1F	},	//157
	{	0x4D, 0x1F	},	//158
	{	0x6D, 0x1F	},	//159
	{	0x6D, 0x1F	},	//160
	{	0x6D, 0x1F	},	//161
	{	0x2E, 0x1F	},	//162
	{	0x2E, 0x1F	},	//163
	{	0x2E, 0x1F	},	//164
	{	0x2E, 0x1F	},	//165
	{	0x4E, 0x1F	},	//166
	{	0x4E, 0x1F	},	//167
	{	0x4E, 0x1F	},	//168
	{	0x4E, 0x1F	},	//169
	{	0x6E, 0x1F	},	//170
	{	0x6E, 0x1F	},	//171
	{	0x6E, 0x1F	},	//172
	{	0x2F, 0x1F	},	//173
	{	0x2F, 0x1F	},	//174
	{	0x2F, 0x1F	},	//175
	{	0x4F, 0x1F	},	//176
	{	0x4F, 0x1F	},	//177
	{	0x4F, 0x1F	},	//178
	{	0x6F, 0x1F	},	//179
	{	0x6F, 0x1F	},	//180
	{	0x6F, 0x1F	},	//181
	{	0x30, 0x1F	},	//182
	{	0x30, 0x1F	},	//183
	{	0x30, 0x1F	},	//184
	{	0x50, 0x1F	},	//185
	{	0x50, 0x1F	},	//186
	{	0x50, 0x1F	},	//187
	{	0x70, 0x1F	},	//188
	{	0x70, 0x1F	},	//189
	{	0x70, 0x1F	},	//190
	{	0x31, 0x1F	},	//191
	{	0x31, 0x1F	},	//192
	{	0x31, 0x1F	},	//193
	{	0x51, 0x1F	},	//194
	{	0x51, 0x1F	},	//195
	{	0x51, 0x1F	},	//196
	{	0x71, 0x1F	},	//197
	{	0x71, 0x1F	},	//198
	{	0x71, 0x1F	},	//199
	{	0x32, 0x1F	},	//200
	{	0x32, 0x1F	},	//201
	{	0x32, 0x1F	},	//202
	{	0x52, 0x1F	},	//203
	{	0x52, 0x1F	},	//204
	{	0x52, 0x1F	},	//205
	{	0x72, 0x1F	},	//206
	{	0x72, 0x1F	},	//207
	{	0x72, 0x1F	},	//208
	{	0x33, 0x1F	},	//209
	{	0x33, 0x1F	},	//210
	{	0x33, 0x1F	},	//211
	{	0x53, 0x1F	},	//212
	{	0x53, 0x1F	},	//213
	{	0x53, 0x1F	},	//214
	{	0x73, 0x1F	},	//215
	{	0x73, 0x1F	},	//216
	{	0x73, 0x1F	},	//217
	{	0x34, 0x1F	},	//218
	{	0x34, 0x1F	},	//219
	{	0x34, 0x1F	},	//220
	{	0x54, 0x1F	},	//221
	{	0x54, 0x1F	},	//222
	{	0x54, 0x1F	},	//223
	{	0x74, 0x1F	},	//224
	{	0x74, 0x1F	},	//225
	{	0x74, 0x1F	},	//226
	{	0x35, 0x1F	},	//227
	{	0x35, 0x1F	},	//228
	{	0x35, 0x1F	},	//229
	{	0x55, 0x1F	},	//230
	{	0x55, 0x1F	},	//231
	{	0x55, 0x1F	},	//232
	{	0x75, 0x1F	},	//233
	{	0x75, 0x1F	},	//234
	{	0x75, 0x1F	},	//235
	{	0x36, 0x1F	},	//236
	{	0x36, 0x1F	},	//237
	{	0x36, 0x1F	},	//238
	{	0x56, 0x1F	},	//239
	{	0x56, 0x1F	},	//240
	{	0x56, 0x1F	},	//241
	{	0x76, 0x1F	},	//242
	{	0x76, 0x1F	},	//243
	{	0x76, 0x1F	},	//244
	{	0x37, 0x1F	},	//245
	{	0x37, 0x1F	},	//246
	{	0x37, 0x1F	},	//247
	{	0x57, 0x1F	},	//248
	{	0x57, 0x1F	},	//249
	{	0x57, 0x1F	},	//250
	{	0x77, 0x1F	},	//251
	{	0x7A, 0x1F	},	//252
	{	0x5C, 0x1F	},	//253
	{	0x3E, 0x1F	},	//254
	{	0x7F, 0x1F	},	//255

};

#ifdef LIGHT_ALC_ENABLE
static const uint8_t mled_reg03_alc_init[16] = {
/*22,16h*/	0x03,
/*23,17h*/	0x07,
/*24,18h*/	0x14,
/*25,19h*/	0x14,
/*26,1Ah*/	0x1F,
/*27,1Bh*/	0x1F,
/*28,1Ch*/	0x1F,
/*29,1Dh*/	0x1F,
/*30,1Eh*/	0x1F,
/*31,1Fh*/	0x1F,
/*32,20h*/	0x37,
/*33,21h*/	0x4B,
/*34,22h*/	0x67,
/*35,23h*/	0x67,
/*36,24h*/	0x67,
/*37,25h*/	0x67,
};

static const uint8_t mled_sensor_brightness[16] = {
	0x04,
	0x0D,
	0x28,
	0x28,
	0x43,
	0x43,
	0x43,
	0x43,
	0x43,
	0x43,
	0x79,
	0xA6,
	0xD7,
	0xD7,
	0xD7,
	0xD7,
};
#endif //LIGHT_ALC_ENABLE

#if 0
static const struct lv5216_reg_write_info regs_rgb_color_1[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x00, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x00, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x1F, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x09, 0xFF	},
};

static const struct lv5216_reg_write_info regs_rgb_color_2[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x1F, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x00, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x00, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x09, 0xFF	},
};

static const struct lv5216_reg_write_info regs_rgb_color_3[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x00, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x1F, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x00, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x09, 0xFF	},
};

static const struct lv5216_reg_write_info regs_rgb_color_4[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x1F, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x00, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x1F, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x09, 0xFF	},
};

static const struct lv5216_reg_write_info regs_rgb_color_5[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x00, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x1F, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x1F, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x09, 0xFF	},
};

static const struct lv5216_reg_write_info regs_rgb_color_6[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x1F, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x1F, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x00, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x09, 0xFF	},
};

static const struct lv5216_reg_write_info regs_rgb_color_7[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x1F, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x1F, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x1F, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x09, 0xFF	},
};

static const struct lv5216_reg_write_info regs_rgb_color_11[] =
{
	{	0x02, 0x08, 0xFF	},
	{	0x0C, 0x00, 0xFF	},
	{	0x0E, 0x1F, 0xFF	},
	{	0x0F, 0x00, 0xFF	},
	{	0x10, 0x00, 0xFF	},
	{	0x11, 0x00, 0xFF	},
	{	0x12, 0x00, 0xFF	},
	{	0x13, 0x00, 0xFF	},
	{	0x0D, 0x08, 0xFF	},
};
#endif

struct lv5216_rgb_current_regs {
	uint8_t	reg06;
	uint8_t	reg07;
	uint8_t	reg08;
};

struct lv5216_rgb_current_info {
	struct lv5216_rgb_current_regs reg_color[LED_COLOR_MAX];
};

static const struct lv5216_rgb_current_info rgb_current_setting[NUM_OF_COLORVARIATION] = {
	{
		{
			{ 0x00, 0x00, 0x0A }, // B
			{ 0x04, 0x00, 0x00 }, // R
			{ 0x00, 0x02, 0x00 }, // G
			{ 0x07, 0x00, 0x01 }, // R + B
			{ 0x00, 0x01, 0x05 }, // B + G
			{ 0x05, 0x01, 0x00 }, // R + G
			{ 0x03, 0x02, 0x03 }, // R + B + G
		}
	},
	{
		{
			{ 0x00, 0x00, 0x0A }, // B
			{ 0x04, 0x00, 0x00 }, // R
			{ 0x00, 0x02, 0x00 }, // G
			{ 0x07, 0x00, 0x01 }, // R + B
			{ 0x00, 0x01, 0x05 }, // B + G
			{ 0x05, 0x01, 0x00 }, // R + G
			{ 0x03, 0x02, 0x03 }, // R + B + G
		}
	},
	{
		{
			{ 0x00, 0x00, 0x0A }, // B
			{ 0x04, 0x00, 0x00 }, // R
			{ 0x00, 0x02, 0x00 }, // G
			{ 0x07, 0x00, 0x01 }, // R + B
			{ 0x00, 0x01, 0x05 }, // B + G
			{ 0x05, 0x01, 0x00 }, // R + G
			{ 0x03, 0x02, 0x03 }, // R + B + G
		}
	},
	{
		{
			{ 0x00, 0x00, 0x0A }, // B
			{ 0x04, 0x00, 0x00 }, // R
			{ 0x00, 0x02, 0x00 }, // G
			{ 0x07, 0x00, 0x01 }, // R + B
			{ 0x00, 0x01, 0x05 }, // B + G
			{ 0x05, 0x01, 0x00 }, // R + G
			{ 0x03, 0x02, 0x03 }, // R + B + G
		}
	},
	{
		{
			{ 0x00, 0x00, 0x0A }, // B
			{ 0x04, 0x00, 0x00 }, // R
			{ 0x00, 0x02, 0x00 }, // G
			{ 0x07, 0x00, 0x01 }, // R + B
			{ 0x00, 0x01, 0x05 }, // B + G
			{ 0x05, 0x01, 0x00 }, // R + G
			{ 0x03, 0x02, 0x03 }, // R + B + G
		}
	}
};

static const struct lv5216_rgb_current_regs rgb_current_onoff[LED_COLOR_MAX] = {
	{ 0, 0, 1 }, // B
	{ 1, 0, 0 }, // R
	{ 0, 1, 0 }, // G
	{ 1, 0, 1 }, // R + B
	{ 0, 1, 1 }, // B + G
	{ 1, 1, 0 }, // R + G
	{ 1, 1, 1 }, // R + B + G
};

#define KCLIGHT_WLED_BOOT_DEFAULT	102

static bool board_check = false;
static int bl_skip_flag = 1;

/* Local Function */
#ifdef LIGHT_ALC_ENABLE
static int lv5216_i2c_read(struct i2c_client *client, uint8_t uc_reg, uint8_t *rbuf, int len);
#endif //LIGHT_ALC_ENABLE
static int lv5216_i2c_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val);
static void lv5216_work(struct work_struct *work);
static void lv5216_blink_work(struct work_struct *work);
//#ifdef CONFIG_PM
#if 0 //test
static int lv5216_suspend(struct i2c_client *client, pm_message_t mesg);
static int lv5216_resume(struct i2c_client *client);
#endif
static int lv5216_remove(struct i2c_client *client);
static int lv5216_probe(struct i2c_client *client, const struct i2c_device_id *id);
//static int32_t __init lv5216_led_init(void);
//static void __exit lv5216_led_exit(void);
static bool light_led_disp_enabled(void);
static void lv5216_rgb_current_write(struct i2c_client *client, int color);
static bool light_lcd_power_off(void);
static int32_t light_led_disp_set(e_light_main_wled_disp disp_status);
void lv5216_set_maxbrightness(int max_level, int enable);
static int lv5216_i2c_masked_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val, uint8_t mask);

static int kclight_reboot_notify(struct notifier_block *nb,
				unsigned long code, void *unused)
{
	struct lv5216_data *data = lv5216_data;

	pr_notice("[KCLIGHT]%s +\n", __func__);

	lv5216_i2c_masked_write(data->client, 0x00, 0x00, 0xFF);
	gpio_set_value(data->reset_gpio, 0);
	usleep_range(15000, 15000);

	kfree(data);
	lv5216_data = NULL;

	pr_notice("[KCLIGHT]%s -\n", __func__);

	return NOTIFY_DONE;
}

#if 0
static int lv5216_dump_state(char *buffer, struct kernel_param *kp)
{
	int i;
	int count = 0;

	count += sprintf(&buffer[count], "dump registers.\n");
	count += sprintf(&buffer[count], "addr value written\n");

	for (i = 0; i < ARRAY_SIZE(lv5216_reg_info_save); i++) {
		count += sprintf(&buffer[count], "0x%02x 0x%02x  %d\n",
			i,
			lv5216_reg_info_save[i].value,
			lv5216_reg_info_save[i].written);
	}

	return count;
}
module_param_call(state, NULL, lv5216_dump_state,
		  NULL, 0440);
#endif

bool lv5216_get_board_id(void) {
	return board_check;
}
EXPORT_SYMBOL(lv5216_get_board_id);

#ifdef LIGHT_ALC_ENABLE
static int lv5216_i2c_read(struct i2c_client *client, uint8_t uc_reg, uint8_t *rbuf, int len)
{
	int ret = 0;
	int retry = 0;
	struct i2c_msg i2cMsg[LV5216_I2C_READ_MSG_NUM];
	u8 reg = 0;
	int i = 0;

	LV5216_V_LOG("[IN] client=0x%p reg=0x%02X len=%d", client, uc_reg, len);

	if (client == NULL) {
		LV5216_E_LOG("fail client=0x%p", client);
		return -ENODEV;
	}

	reg = uc_reg;

	i2cMsg[0].addr = client->addr;
	i2cMsg[0].flags = 0;
	i2cMsg[0].len = 1;
	i2cMsg[0].buf = &reg;

	i2cMsg[1].addr = client->addr;
	i2cMsg[1].flags = I2C_M_RD;
	i2cMsg[1].len = len;
	i2cMsg[1].buf = rbuf;

	do {
		ret = i2c_transfer(client->adapter, &i2cMsg[0], LV5216_I2C_READ_MSG_NUM);
		LV5216_V_LOG("i2c_transfer() call end ret=%d", ret);
	} while ((ret != LV5216_I2C_READ_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	if (ret != LV5216_I2C_READ_MSG_NUM) {
		LV5216_E_LOG("fail (try:%d) uc_reg=0x%02x, rbuf=0x%02x ret=%d", retry, uc_reg, *rbuf, ret);
		ret = -1;
	} else {
		ret = 0;
		LV5216_V_LOG("i2c read success");
		for (i = 0; i < len; i++)
		{
			LV5216_D_LOG("i2c read  reg=0x%02x,value=0x%02x", (unsigned int)(uc_reg + i), (unsigned int)*(rbuf + i));
		}
	}
	LV5216_V_LOG("[OUT] ret=%d", ret);

	return ret;
}
#endif //LIGHT_ALC_ENABLE

static int lv5216_i2c_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val)
{
	int ret = 0;
	int retry = 0;
	struct i2c_msg i2cMsg;
	u8 ucwritebuf[LV5216_WRITE_BUF_LEN];

	LV5216_V_LOG("[IN] client=0x%p reg=0x%02x val=0x%02X", client, uc_reg, uc_val);

	if (client == NULL) {
		LV5216_E_LOG("fail client=0x%p", client);
		return -ENODEV;
	}

	ucwritebuf[0] = uc_reg;
	ucwritebuf[1] = uc_val;
	i2cMsg.addr  = client->addr;
	i2cMsg.flags = 0;
	i2cMsg.len   =  sizeof(ucwritebuf);
	i2cMsg.buf   =  &ucwritebuf[0];

	LV5216_D_LOG("i2c write reg=0x%02x,value=0x%02x", uc_reg, uc_val);

	do {
		ret = i2c_transfer(client->adapter, &i2cMsg, LV5216_I2C_WRITE_MSG_NUM);
		LV5216_V_LOG("i2c_transfer() call end ret=%d", ret);
	} while ((ret != LV5216_I2C_WRITE_MSG_NUM) && (++retry < I2C_RETRIES_NUM));

	if (ret != LV5216_I2C_WRITE_MSG_NUM) {
		LV5216_E_LOG("fail (try:%d) uc_reg=0x%02x, uc_val=0x%02x ret=%d", retry, ucwritebuf[0], ucwritebuf[1], ret);
		lv5216_reg_info_save[uc_reg].written = false;
		ret = -1;
	} else {
		LV5216_V_LOG("success reg=0x%02x val=0x%02x", ucwritebuf[0], ucwritebuf[1]);
		lv5216_reg_info_save[uc_reg].written = true;
		lv5216_reg_info_save[uc_reg].value = uc_val;
		ret = 0;
	}

	LV5216_V_LOG("[OUT] ret=%d", ret);

	return ret;
}

static int lv5216_i2c_masked_write(struct i2c_client *client, uint8_t uc_reg, uint8_t uc_val, uint8_t mask)
{
	uint8_t new_value;
	int ret;

	LV5216_V_LOG("[IN] reg=0x%02x val=0x%02x mask=0x%02x", uc_reg, uc_val, mask);

	new_value = (lv5216_reg_info_save[uc_reg].value & ~mask) | (uc_val & mask);

	if (lv5216_reg_info_save[uc_reg].written &&
		(new_value == lv5216_reg_info_save[uc_reg].value)) {
		LV5216_V_LOG("no change");
		return 0;
	}

	ret = lv5216_i2c_write(client, uc_reg, new_value);

	LV5216_V_LOG("[OUT] ret=%d", ret);

	return ret;
}

//static void lv5216_write_regs(struct i2c_client *client,
//	const struct lv5216_reg_write_info *regs, size_t count)
//{
//	int	i;

//	LV5216_V_LOG("[IN]");
//	for (i = 0; i < count; i++) {
//		lv5216_i2c_masked_write(client, regs[i].reg, regs[i].val, regs[i].mask);
//	}
//	LV5216_V_LOG("[OUT]");
//}

static void led_set(struct led_classdev *cdev, enum led_brightness value)
{
	struct lv5216_led_data *led =
		container_of(cdev, struct lv5216_led_data, cdev);
	struct lv5216_data *data = led->parent;

	LV5216_D_LOG("[IN] name=%s value=0x%08x", cdev->name, value);

	mutex_lock(&data->param_lock);
	mutex_lock(&led->param_next_lock);
	memcpy(&led->param_next, &data->param, sizeof(led->param_next));
	led->param_next.value = (uint32_t)value;
	mutex_unlock(&led->param_next_lock);
	mutex_unlock(&data->param_lock);

	queue_work(led->workqueue, &led->work);

	LV5216_D_LOG("[OUT]");
}

static enum led_brightness led_get(struct led_classdev *cdev)
{
	int32_t lret = 0;
	struct lv5216_led_data *led =
		container_of(cdev, struct lv5216_led_data, cdev);

	LV5216_D_LOG("[IN] name=%s", cdev->name);

	lret = (int32_t)led->param_next.value;

	LV5216_D_LOG("[OUT] lret=0x%08x", lret);

	return lret;
}

static void lv5216_power_control(struct lv5216_data *data, int led_type, bool on)
{
	struct i2c_client *client = data->client;
	int state_prev = data->power_state;
	int state_next = data->power_state;
	uint8_t value_prev;
	uint8_t value_next;
	uint8_t value_ext;

	LV5216_V_LOG("[IN] led_type=%d on=%d", led_type, on);

	if (on) {
		state_next |=  (1 << led_type);
	} else {
		state_next &= ~(1 << led_type);
	}

	LV5216_V_LOG("[IN] state_next=%x state_prev=%x", state_next, state_prev);

	if (state_next) {
		value_next = 0x81;
	} else {
		value_next = 0x00;
	}

	if (state_prev) {
		value_prev = 0x81;
	} else {
		value_prev = 0x00;
	}

//	if(lv5216_led_stat_aboot){
//		value_ext = value_next | 0x81;
//	}
//	else{
		value_ext = value_next;
//	}

	LV5216_V_LOG("value_ext=0x%0x", value_ext);

	lv5216_i2c_masked_write(client, 0x00, value_ext, 0xFF);

	if (value_next != value_prev) {
		LV5216_D_LOG("change power control 0x%02x->0x%02x",
			value_prev, value_next);
		if (value_next == 0x81) {
			LV5216_D_LOG("enable vled");
			usleep_range(2000, 2000);
		}
	} else {
		LV5216_D_LOG("no change");
	}

	data->power_state = state_next;

	LV5216_V_LOG("[OUT] state_next=0x%08x state_prev=0x%08x", state_next, state_prev);
}

#ifdef LIGHT_ALC_ENABLE
static void lv5216_photo_sensor_control(struct lv5216_data *data, int led_type, bool on)
{
	struct i2c_client *client = data->client;
	int state_prev = data->photo_sensor_state;
	int state_next = data->photo_sensor_state;

	LV5216_V_LOG("[IN] led_type=%d on=%d", led_type, on);

	if (on) {
		state_next |=  (1 << led_type);
	} else {
		state_next &= ~(1 << led_type);
	}

	if (!state_prev && state_next) {
		LV5216_V_LOG("enable photo sensor");
		lv5216_i2c_masked_write(client, 0x14, 0x47, 0xFF);
		usleep_range(30000, 30000);
	} else if (state_prev && !state_next) {
		LV5216_V_LOG("disable photo sensor");
		lv5216_i2c_masked_write(client, 0x14, 0x00, 0x40);
	} else {
		LV5216_V_LOG("no change");
	}

	data->photo_sensor_state = state_next;

	LV5216_V_LOG("[OUT] state_next=0x%08x state_prev=0x%08x", state_next, state_prev);
}

static int lv5216_get_photo_sensor_level(struct lv5216_data *data)
{
	struct i2c_client *client = data->client;
	uint8_t read_buf = 0;
	int ret = 0;
	int level = 0;

	LV5216_V_LOG("[IN]");

	ret = lv5216_i2c_read(client, 0x15, &read_buf, 1);
	if (ret >= 0) {
		level = read_buf >> 4;
	}

	LV5216_V_LOG("[OUT] level=0x%08x", level);

	return level;
}

static void lv5216_ctrl_update_alc_table(struct i2c_client *client, uint32_t max_level)
{
	uint32_t level;
	uint32_t value;
	int i;

	LV5216_V_LOG("[IN] max_value=0x%08x", max_level);

	value = mled_reg03_alc_init[0];
	for (i = 0; i < ARRAY_SIZE(mled_sensor_brightness); i++) {
		level = mled_sensor_brightness[i];
		if (level <= max_level) {
			value = mled_reg03_alc_init[i];
		}
		lv5216_i2c_masked_write(client, 0x16 + i, value, 0xFF);
	}

	LV5216_V_LOG("[OUT]");
}
#endif //LIGHT_ALC_ENABLE

#if 0
static void led_set_sensor(struct led_classdev *cdev, enum led_brightness value)
{
	LV5216_V_LOG("[IN]");
	LV5216_V_LOG("[OUT]");
}

static enum led_brightness led_get_sensor(struct led_classdev *cdev)
{
	int32_t lret = 0;
#ifdef LIGHT_ALC_ENABLE
	struct lv5216_led_data *led =
		container_of(cdev, struct lv5216_led_data, cdev);
	struct lv5216_data *data = led->parent;
	struct lv5216_led_param *param_prev = &led->param_prev;
	int ctrl_mode_prev = GET_CTRL_MODE(param_prev->ctrl_mode_mled);
	int photo_level;
	uint32_t sensor_brightness;
	uint32_t max_level;

	LV5216_V_LOG("[IN] name=%s ctrl_mode_prev=0x%02x param_prev->value=0x%02x", cdev->name, ctrl_mode_prev, param_prev->value);

	mutex_lock(&data->control_lock);
	if (param_prev->value &&
		(ctrl_mode_prev == CTRL_MODE_ALC || ctrl_mode_prev == CTRL_MODE_ALC_CAMERA)) {
		max_level = led->cdev.max_brightness;
		if (max_level > 0xFF)
			max_level = 0xFF;

		photo_level = lv5216_get_photo_sensor_level(data);
		sensor_brightness = mled_sensor_brightness[photo_level&0x0F];
		if (sensor_brightness > max_level)
			sensor_brightness = max_level;
		led->cdev.sensor_brightness = sensor_brightness;
		LV5216_V_LOG("photo_level = 0x%02x, sensor_brightness = 0x%02x", photo_level, sensor_brightness);
	}
	lret = (int32_t)led->cdev.sensor_brightness;
	mutex_unlock(&data->control_lock);

	LV5216_V_LOG("[OUT] lret=0x%08x", lret);
#else
	lret = led_get(cdev);
#endif
	return lret;
}
#endif

static void lv5216_ctrl_mled(struct lv5216_led_data *led,
	struct lv5216_led_param *param_next)
{
	struct lv5216_data *data = led->parent;
	struct i2c_client *client = data->client;
	int level;
#ifdef LIGHT_ALC_ENABLE
	struct lv5216_led_param *param_prev = &led->param_prev;
	int photo_level;
	int slope;
	uint32_t value;
	uint32_t max_level;
	uint32_t sensor_brightness;
#endif //LIGHT_ALC_ENABLE

	LV5216_V_LOG("[IN] value=0x%08x", param_next->value);
	mutex_lock(&data->mled_lock);

	if (!light_led_disp_enabled()) {
		LV5216_V_LOG("discard light_led_disp_enabled()");
		goto exit;
	}

	param_next->value = min(param_next->value, led->cdev.max_brightness);
	LV5216_V_LOG("[IN] min value=0x%08x", param_next->value);

	if (light_lcd_power_off()){
			param_next->value = 0;
	}

	if (param_next->value) {
		lv5216_power_control(data, led->led_type, true);

		switch (GET_CTRL_MODE(param_next->ctrl_mode_mled)) {
		case CTRL_MODE_DIM:
			LV5216_V_LOG("CTRL_MODE_DIM");
#ifdef LIGHT_ALC_ENABLE
			if (param_prev->value &&
				(GET_CTRL_MODE(param_prev->ctrl_mode_mled) == CTRL_MODE_ALC ||
				 GET_CTRL_MODE(param_prev->ctrl_mode_mled) == CTRL_MODE_ALC_CAMERA)) {
				slope = true;
			} else {
				slope = false;
			}
#endif //LIGHT_ALC_ENABLE
			lv5216_i2c_masked_write(client, 0x03, 0x20, 0xFF);
#ifdef LIGHT_ALC_ENABLE
			if (slope) {
				lv5216_i2c_masked_write(client, 0x0B, 0x28, 0x38);
			}
#endif //LIGHT_ALC_ENABLE
			lv5216_i2c_masked_write(client, 0x01, 0x1D, 0x1F);
#ifdef LIGHT_ALC_ENABLE
			if (slope) {
				mutex_unlock(&data->control_lock);
				usleep_range(360 * 1000, 360 * 1000);
				mutex_lock(&data->control_lock);
			}
			lv5216_i2c_masked_write(client, 0x14, 0x00, 0x20);
			lv5216_photo_sensor_control(data, led->led_type, false);
			if (slope) {
				lv5216_i2c_masked_write(client, 0x0B, 0x00, 0x38);
			}
#endif //LIGHT_ALC_ENABLE
			break;
		case CTRL_MODE_ALC_CAMERA:
			LV5216_V_LOG("CTRL_MODE_ALC_CAMERA");
			/* FALL THROUGH */
		case CTRL_MODE_ALC:
			LV5216_V_LOG("CTRL_MODE_ALC");
#ifdef LIGHT_ALC_ENABLE
			max_level = led->cdev.max_brightness;
			if (max_level > 0xFF)
				max_level = 0xFF;

			lv5216_ctrl_update_alc_table(client, max_level);
			if (!param_prev->value ||
			(GET_CTRL_MODE(param_prev->ctrl_mode_mled) != CTRL_MODE_ALC &&
			 GET_CTRL_MODE(param_prev->ctrl_mode_mled) != CTRL_MODE_ALC_CAMERA)) {
				lv5216_photo_sensor_control(data, led->led_type, true);
				photo_level = lv5216_get_photo_sensor_level(data);
				sensor_brightness = mled_sensor_brightness[photo_level&0x0F];

				if (sensor_brightness > max_level)
					sensor_brightness = max_level;

				value = mled_reg03_reg01_manual[sensor_brightness][0];
			 	
				lv5216_i2c_masked_write(client, 0x03, value, 0xFF);
				lv5216_i2c_masked_write(client, 0x01, 0x1F, 0x1F);
				lv5216_i2c_masked_write(client, 0x14, 0x38, 0x3F);

				led->cdev.sensor_brightness = sensor_brightness;
				LV5216_V_LOG("CTRL_MODE_ALC photo_level=%d sensor_brightness=%d max_level=%d value=%d"
								, photo_level, sensor_brightness, max_level, value);
			}
			break;
#endif //LIGHT_ALC_ENABLE
		case CTRL_MODE_MANUAL:
			LV5216_V_LOG("CTRL_MODE_MANUAL");
			/* FALL THROUGH */
		default:
			LV5216_V_LOG("default");
			level = param_next->value & 0xFF;

			lv5216_i2c_masked_write(client, 0x03, mled_reg03_reg01_manual[level][0], 0xFF);
			lv5216_i2c_masked_write(client, 0x01, mled_reg03_reg01_manual[level][1], 0x1F);
			
#ifdef LIGHT_ALC_ENABLE
			lv5216_i2c_masked_write(client, 0x14, 0x00, 0x20);
			lv5216_photo_sensor_control(data, led->led_type, false);
#endif //LIGHT_ALC_ENABLE
			break;
		}
	} else {
#ifdef LIGHT_ALC_ENABLE
		lv5216_i2c_masked_write(client, 0x0B, 0x00, 0x38);
#endif //LIGHT_ALC_ENABLE
		lv5216_i2c_masked_write(client, 0x01, 0x00, 0x1F);
#ifdef LIGHT_ALC_ENABLE
		lv5216_photo_sensor_control(data, led->led_type, false);
#endif //LIGHT_ALC_ENABLE
		lv5216_power_control(data, led->led_type, false);
	}

exit:
	mutex_unlock(&data->mled_lock);
	LV5216_V_LOG("[OUT]");
}

static void lv5216_ctrl_sled(struct lv5216_led_data *led,
							 struct lv5216_led_param *param_next)
{
	struct lv5216_data *data = led->parent;

	LV5216_V_LOG("[IN]");

	if(param_next->value & 0x00ffffff){
		LV5216_V_LOG("SUBLED ON");
		if (gpio_is_valid(data->sled_gpio))
			gpio_set_value(data->sled_gpio, 1);
		else
			LV5216_E_LOG("No valid SUBLED GPIO specified %d", data->sled_gpio);
	} else {
		LV5216_V_LOG("SUBLED OFF");
		if (gpio_is_valid(data->sled_gpio))
			gpio_set_value(data->sled_gpio, 0);
		else
			LV5216_E_LOG("No valid SUBLED GPIO specified %d", data->sled_gpio);
	}

	LV5216_V_LOG("[OUT]");
	return;
}

static void lv5216_queue_rgb_work(void)
{
	struct lv5216_data *data = lv5216_data;
	struct lv5216_led_data *led;

	LV5216_V_LOG("[IN]");
	if (data) {
		led = &data->led_data[LED_TYPE_RGB];
		if (led->cdev_registered) {
			if (led->param_next.value) {
				queue_work(led->workqueue, &led->work);
			}
		}
	}
	LV5216_V_LOG("[OUT]");
}

static void lv5216_rgb_off(struct lv5216_data *data)
{
	struct i2c_client *client = data->client;
	LV5216_V_LOG("[IN]");
	wake_unlock(&data->wake_lock);
	cancel_delayed_work(&data->blink_work);
	lv5216_i2c_masked_write(client, 0x02, 0x00, 0x07);
	lv5216_i2c_masked_write(client, 0x0C, 0x00, 0xFF);
	lv5216_i2c_masked_write(client, 0x0D, 0x00, 0xFF);
	lv5216_i2c_masked_write(client, 0x00, 0x00, 0x20);
	lv5216_led_illum_count = 0;
	LV5216_V_LOG("[OUT]");
}

static void lv5216_rgb_on(struct i2c_client *client, uint32_t value)
{
	uint8_t rgb_on;
	int color;

	LV5216_V_LOG("[IN] value=0x%08x", value);

	rgb_on = ((value & 0x00ff0000) ? 0x01 : 0) |
			 ((value & 0x0000ff00) ? 0x02 : 0) |
			 ((value & 0x000000ff) ? 0x04 : 0);

	// current
	color = lv5216_get_color(rgb_on);
	lv5216_rgb_current_write(client, color);

	// rgb on
	lv5216_i2c_masked_write(client, 0x02, rgb_on, 0x07);

	LV5216_V_LOG("[OUT]");
}

static void lv5216_rgb_blink_oneshot(struct i2c_client *client, struct lv5216_led_param *param)
{
	const uint32_t blink_cycle_list[] = {
		250, 500, 1000, 2000, 4000, 8000
	};
	uint8_t rgb_on;
	uint8_t rgb_off;
	uint8_t on_trig;
	uint8_t off_trig;
	int32_t total_time;
	uint8_t blink_cycle;
	uint8_t i;

	LV5216_D_LOG("[IN] value=0x%08x off=0x%08x on=%dms off=%dms",
		param->value, param->blink_off_color,
		param->blink_high_pause_time, param->blink_low_pause_time);

	total_time = param->blink_high_pause_time + param->blink_low_pause_time;

	// blink cycle
	blink_cycle = 0;
	for (i = ARRAY_SIZE(blink_cycle_list) - 1; i > 0; i--) {
		if (total_time >= blink_cycle_list[i]) {
			blink_cycle = i;
			break;
		}
	}

	// on/off trigger
	on_trig = 0;
	off_trig = ((64UL * param->blink_high_pause_time) + total_time - 1)
		/ total_time;
	if (off_trig == 0)
		off_trig = 1;
	else if (off_trig > 63)
		off_trig = 63;

	// on/off color
	rgb_on = ((param->value & 0x00ff0000) ? 0x01 : 0) |
		 ((param->value & 0x0000ff00) ? 0x02 : 0) |
		 ((param->value & 0x000000ff) ? 0x04 : 0);
	rgb_off = ((param->blink_off_color & 0x00ff0000) ? 0x01 : 0) |
		  ((param->blink_off_color & 0x0000ff00) ? 0x02 : 0) |
		  ((param->blink_off_color & 0x000000ff) ? 0x04 : 0);

	// current
	lv5216_rgb_current_write(client, lv5216_get_color(rgb_on));
	lv5216_rgb_current_write(client, lv5216_get_color(rgb_off));

	// rgb
	lv5216_i2c_masked_write(client, 0x02, rgb_on & rgb_off, 0x07);

	// fade
	lv5216_i2c_masked_write(client, 0x0C, 0x00, 0xFF);

	// gradation
	if ((rgb_on ^ rgb_off) & 0x01) {
		if (rgb_on & 0x01) {
			lv5216_i2c_masked_write(client, 0x0E, off_trig, 0xFF);
			lv5216_i2c_masked_write(client, 0x0F, on_trig, 0xFF);
		} else {
			lv5216_i2c_masked_write(client, 0x0E, on_trig, 0xFF);
			lv5216_i2c_masked_write(client, 0x0F, off_trig, 0xFF);
		}
	} else {
		lv5216_i2c_masked_write(client, 0x0E, 0x00, 0xFF);
		lv5216_i2c_masked_write(client, 0x0F, 0x00, 0xFF);
	}

	if ((rgb_on ^ rgb_off) & 0x02) {
		if (rgb_on & 0x02) {
			lv5216_i2c_masked_write(client, 0x10, off_trig, 0xFF);
			lv5216_i2c_masked_write(client, 0x11, on_trig, 0xFF);
		} else {
			lv5216_i2c_masked_write(client, 0x10, on_trig, 0xFF);
			lv5216_i2c_masked_write(client, 0x11, off_trig, 0xFF);
		}
	} else {
		lv5216_i2c_masked_write(client, 0x10, 0x00, 0xFF);
		lv5216_i2c_masked_write(client, 0x11, 0x00, 0xFF);
	}

	if ((rgb_on ^ rgb_off) & 0x04) {
		if (rgb_on & 0x04) {
			lv5216_i2c_masked_write(client, 0x12, off_trig, 0xFF);
			lv5216_i2c_masked_write(client, 0x13, on_trig, 0xFF);
		} else {
			lv5216_i2c_masked_write(client, 0x12, on_trig, 0xFF);
			lv5216_i2c_masked_write(client, 0x13, off_trig, 0xFF);
		}
	} else {
		lv5216_i2c_masked_write(client, 0x12, 0x00, 0xFF);
		lv5216_i2c_masked_write(client, 0x13, 0x00, 0xFF);
	}

	lv5216_i2c_masked_write(client, 0x0D, 0x08 | blink_cycle, 0xFF);

	LV5216_D_LOG("[OUT]");
}

static void lv5216_rgb_blink_time_round(struct lv5216_led_param *param, struct lv5216_blink_ctl *blink_ctl)
{
	LV5216_V_LOG("%s: + blink_high_pause_time=%d blink_low_pause_time=%d blink_round_ontime=%d blink_round_offtime=%d",
		__func__, param->blink_high_pause_time, param->blink_low_pause_time, blink_ctl->blink_round_ontime, blink_ctl->blink_round_offtime);

	param->blink_high_pause_time = blink_ctl->blink_round_ontime;
	param->blink_low_pause_time  = blink_ctl->blink_round_offtime;

	LV5216_V_LOG("%s: - blink_high_pause_time=%d blink_low_pause_time=%d blink_round_ontime=%d blink_round_offtime=%d",
		__func__, param->blink_high_pause_time, param->blink_low_pause_time, blink_ctl->blink_round_ontime, blink_ctl->blink_round_offtime);
}

static void lv5216_rgb_blink(struct lv5216_data *data, struct lv5216_led_param *param)
{
	bool blink;
	struct i2c_client *client = data->client;
	int32_t total_time;
	struct lv5216_led_data *led = &data->led_data[LED_TYPE_RGB];

	LV5216_V_LOG("[IN]");

	if (!param->value)
		return;

	if (param->blink_high_pause_time && param->blink_low_pause_time)
		blink = true;
	else
		blink = false;

	if (blink) {
		total_time = param->blink_high_pause_time + param->blink_low_pause_time;
		if ((total_time >= 1000)&&(!param->blink_control)) {
			if (data->blink_ctl.blink_round_en) {
				lv5216_rgb_blink_time_round(param, &data->blink_ctl);
			}
			lv5216_power_control(data, LED_TYPE_RGB, true);
			lv5216_rgb_blink_oneshot(client, param);
		} else {
			lv5216_power_control(data, LED_TYPE_RGB, true);
			wake_lock(&data->wake_lock);

			data->blink_ctl.next  = true;
			data->blink_ctl.param = *param;
			queue_delayed_work(led->workqueue, &data->blink_work, msecs_to_jiffies(1));
		}
	} else {
		lv5216_power_control(data, LED_TYPE_RGB, true);
		lv5216_rgb_on(client, param->value);
	}

	LV5216_V_LOG("[OUT]");
}

static void lv5216_rgb_current_write(struct i2c_client *client, int color)
{
	struct lv5216_data *data = i2c_get_clientdata(client);
	const struct lv5216_rgb_current_regs *reg_color;
	const struct lv5216_rgb_current_regs *reg_onoff;
	LV5216_V_LOG("[IN] colvar:%d color:%d",data->colorvariation, color);

	if (color >= 0 && color < LED_COLOR_MAX) {
		reg_color = &rgb_current_setting[data->colorvariation].reg_color[color];
		reg_onoff = &rgb_current_onoff[color];
		if (reg_onoff->reg06)
			lv5216_i2c_masked_write(client, 0x06, reg_color->reg06, 0xFF);
		if (reg_onoff->reg07)
			lv5216_i2c_masked_write(client, 0x07, reg_color->reg07, 0xFF);
		if (reg_onoff->reg08)
			lv5216_i2c_masked_write(client, 0x08, reg_color->reg08, 0xFF);
	}
	LV5216_V_LOG("[IN/OUT]");
}

static void lv5216_ctrl_rgb(struct lv5216_led_data *led,
	struct lv5216_led_param *param_next)
{
	struct lv5216_data *data = led->parent;

	LV5216_V_LOG("[IN] value=0x%08x", param_next->value);

	if (param_next->value) {
		lv5216_rgb_off(data);
		lv5216_rgb_blink(data, param_next);
	} else {
		lv5216_rgb_off(data);
		lv5216_power_control(data, led->led_type, false);
	}
	LV5216_V_LOG("[OUT]");
}

static void lv5216_ctrl_cled(struct lv5216_led_data *led,
	struct lv5216_led_param *param_next)
{
	struct lv5216_data *data = led->parent;
	struct i2c_client *client = led->parent->client;

	LV5216_V_LOG("[IN] value=0x%08x", param_next->value);

	if (param_next->value) {
		lv5216_power_control(data, led->led_type, true);
		lv5216_i2c_masked_write(client, 0x02, 0x80, 0x80);
	} else {
		lv5216_i2c_masked_write(client, 0x02, 0x00, 0x80);
		lv5216_power_control(data, led->led_type, false);
	}

	LV5216_V_LOG("[OUT]");
}

static void power_control_vkey(struct lv5216_data *data, int led_type, bool on)
{
	int state_next = data->power_vkey_state;
	uint8_t value_next;

	LV5216_V_LOG("[IN] led_type=%d on=%d", led_type, on);

	if (on) {
		state_next |=  (1 << led_type);
	} else {
		state_next &= ~(1 << led_type);
	}

	LV5216_V_LOG("[IN] state_next=%x", state_next);

	if (state_next) {
		value_next = 1;
	} else {
		value_next = 0;
	}

//	gpio_set_value(data->keyled_gpio, value_next);

	data->power_vkey_state = state_next;

	LV5216_V_LOG("[OUT] state_next=0x%08x", state_next);
}

void lv5216_power_control_vkey(bool on)
{
	LV5216_V_LOG("[OUT] on=%d", on);
	power_control_vkey(lv5216_data, LED_TYPE_ALED, on);
}

static void lv5216_ctrl_keyled(struct lv5216_led_data *led,
	struct lv5216_led_param *param_next)
{
	struct lv5216_data *data = led->parent;
#ifdef LIGHT_ALC_ENABLE
	int photo_level;
#endif //LIGHT_ALC_ENABLE

	LV5216_V_LOG("[IN] value=0x%08x", param_next->value);

	if (param_next->value) {
//		lv5216_power_control(data, led->led_type, true);
#ifdef LIGHT_ALC_ENABLE
		if (param_next->ctrl_mode_keyled == CTRL_MODE_ALC) {
			lv5216_photo_sensor_control(data, led->led_type, true);
			photo_level = lv5216_get_photo_sensor_level(data);
			LV5216_D_LOG("photo_level=%d threshold=%d", photo_level, param_next->keyled_threshold);
			if (photo_level <= param_next->keyled_threshold) {
				power_control_vkey(data, led->led_type, 1);
			} else {
				power_control_vkey(data, led->led_type, 0);
			}
		} else {
#endif //LIGHT_ALC_ENABLE
			power_control_vkey(data, led->led_type, 1);
			usleep_range(300, 300);
			gpio_set_value(data->keyled_on_gpio, 1);
#ifdef LIGHT_ALC_ENABLE
			lv5216_photo_sensor_control(data, led->led_type, false);
		}
#endif //LIGHT_ALC_ENABLE
	} else {
		gpio_set_value(data->keyled_on_gpio, 0);
		usleep_range(300, 300);
		power_control_vkey(data, led->led_type, 0);
#ifdef LIGHT_ALC_ENABLE
		lv5216_photo_sensor_control(data, led->led_type, false);
#endif //LIGHT_ALC_ENABLE
//		lv5216_power_control(data, led->led_type, false);
	}

	LV5216_V_LOG("[OUT]");
}

static void lv5216_blink_work(struct work_struct *work)
{

	struct lv5216_data *data =
		container_of(work, struct lv5216_data, blink_work.work);
	struct lv5216_led_data *led = &data->led_data[LED_TYPE_RGB];
	uint32_t value;

	LV5216_V_LOG("[IN]");

	mutex_lock(&data->control_lock);
	if (!data->blink_ctl.next) {
		data->blink_ctl.next = true;
		lv5216_rgb_on(data->client, data->blink_ctl.param.blink_off_color);
		queue_delayed_work(led->workqueue, &data->blink_work,
			msecs_to_jiffies(data->blink_ctl.param.blink_low_pause_time));
	} else {
		data->blink_ctl.next = false;
		value = data->blink_ctl.param.value;
		if (value == (LV5216_COMBINATION_ILLUM1&0x00ffffff)) {
			LV5216_V_LOG("ILLUMI #1 value=%x count=%d", value, lv5216_led_illum_count);
			if (lv5216_led_illum_count > LV5216_COMBINATION_ILLUM1_MAX) {
				lv5216_led_illum_count = 0;
			}
			value = combination_illumination_tbl[lv5216_led_illum_count];
			lv5216_led_illum_count++;
			LV5216_V_LOG("ILLUMI #2 value=%x count=%d", value, lv5216_led_illum_count);
		}
		lv5216_rgb_on(data->client, value);
		queue_delayed_work(led->workqueue, &data->blink_work,
			msecs_to_jiffies(data->blink_ctl.param.blink_high_pause_time));
	}
	mutex_unlock(&data->control_lock);

	LV5216_V_LOG("[OUT]");
}

static void lv5216_work(struct work_struct *work)
{
	struct lv5216_led_data *led =
		container_of(work, struct lv5216_led_data, work);
	struct lv5216_data *data = led->parent;
	struct lv5216_led_param param_next;

	LV5216_V_LOG("[IN] led_type[%d]", led->led_type);

	mutex_lock(&led->param_next_lock);
	memcpy(&param_next, &led->param_next, sizeof(param_next));
	mutex_unlock(&led->param_next_lock);

	if (led->ctrl_func) {
		mutex_lock(&data->control_lock);
		led->ctrl_func(led, &param_next);
		mutex_unlock(&data->control_lock);
	}

	memcpy(&led->param_prev, &param_next, sizeof(led->param_prev));

	LV5216_V_LOG("[OUT]");
}

//
// for device ioctl
//
#define LEDLIGHT_PARAM_NUM      (5)
#define LEDLIGHT                'L'
#define LEDLIGHT_SET_BLINK		_IOW(LEDLIGHT, 0, T_LEDLIGHT_IOCTL)
#define LEDLIGHT_SET_PARAM      _IOW(LEDLIGHT, 1, T_LEDLIGHT_IOCTL)
#define LEDLIGHT_SET_HWCHECK    _IOW(LEDLIGHT, 2, T_LEDLIGHT_IOCTL)

#define LEDLIGHT_PARAM_MLED     (1)
#define LEDLIGHT_PARAM_RGB      (2)
#define LEDLIGHT_PARAM_CLED     (3)
#define LEDLIGHT_PARAM_KEYLED   (4)

#define LEDLIGHT_PARAM_CHANGE_COLORVARIATION   (10)

#define LEDLIGHT_PARAM_HWCHECK_VLEDONOFF	1

typedef struct _t_ledlight_ioctl {
	uint32_t data[LEDLIGHT_PARAM_NUM];
} T_LEDLIGHT_IOCTL;

static int32_t leds_open(struct inode* inode, struct file* filp)
{
	LV5216_V_LOG("[IN/OUT]");
	return 0;
}

static int32_t leds_release(struct inode* inode, struct file* filp)
{
	LV5216_V_LOG("[IN/OUT]");
	return 0;
}

static long leds_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int32_t ret = -1;
	T_LEDLIGHT_IOCTL st_ioctl;
	struct lv5216_data *data = lv5216_data;
	struct lv5216_led_param *param = &data->param;

	LV5216_V_LOG("[IN]");

	if (!data) {
		LV5216_N_LOG("Error data == NULL");
		return -EFAULT;
	}
	switch (cmd) {
	case LEDLIGHT_SET_PARAM:
		LV5216_V_LOG("LEDLIGHT_SET_CONTROL 0x%08x", LEDLIGHT_SET_PARAM);
		ret = copy_from_user(&st_ioctl,
					argp,
					sizeof(T_LEDLIGHT_IOCTL));
		if (ret) {
			LV5216_N_LOG("Error leds_ioctl(cmd = LEDLIGHT_SET_CONTROL_MODE)");
			return -EFAULT;
		}
		LV5216_V_LOG("st_ioctl data[0]=[0x%08x] data[1]=[0x%08x] data[2]=[0x%08x] data[3]=[0x%08x] data[4]=[0x%08x]",
			st_ioctl.data[0], st_ioctl.data[1], st_ioctl.data[2], st_ioctl.data[3], st_ioctl.data[4]);

		switch(st_ioctl.data[0]) {
		case LEDLIGHT_PARAM_MLED:
			mutex_lock(&data->param_lock);
			param->ctrl_mode_mled = st_ioctl.data[1];
//			ext_ctrl_mode = GET_CTRL_MODE_EXT(param->ctrl_mode_mled);
			mutex_unlock(&data->param_lock);
			LV5216_D_LOG("LEDLIGHT_PARAM_MLED ctrl_mode_mled=0x%x", param->ctrl_mode_mled);
			break;
		case LEDLIGHT_PARAM_RGB:
			mutex_lock(&data->param_lock);
			param->blink_high_pause_time = st_ioctl.data[1];
			param->blink_low_pause_time  = st_ioctl.data[2];
			param->blink_control         = st_ioctl.data[3];
			param->blink_off_color       = st_ioctl.data[4];
			mutex_unlock(&data->param_lock);
			LV5216_D_LOG("LEDLIGHT_PARAM_RGB blink_control=%d high=%d low=%d off_color=0x%08x",
				param->blink_control, param->blink_high_pause_time,
				param->blink_low_pause_time, param->blink_off_color);
			break;
		case LEDLIGHT_PARAM_KEYLED:
			mutex_lock(&data->param_lock);
			param->ctrl_mode_keyled = st_ioctl.data[1];
			param->keyled_threshold = st_ioctl.data[2];
			mutex_unlock(&data->param_lock);
			LV5216_D_LOG("LEDLIGHT_PARAM_KEYLED ctrl_mode_keyled=%d keyled_threshold=%d",
				param->ctrl_mode_keyled, param->keyled_threshold);
			break;
		case LEDLIGHT_PARAM_CHANGE_COLORVARIATION:
			if (st_ioctl.data[1] >= 0 && st_ioctl.data[1] < NUM_OF_COLORVARIATION) {
				data->colorvariation = st_ioctl.data[1];
				LV5216_D_LOG("LEDLIGHT_PARAM_CHANGE_COLORVARIATION colvar=%d", data->colorvariation);
			} else {
				LV5216_N_LOG("invalid param 0x%08x", st_ioctl.data[1]);
				return -EFAULT;
			}
			lv5216_queue_rgb_work();
			break;
		default:
			LV5216_N_LOG("invalid param 0x%08x", st_ioctl.data[0]);
			return -EFAULT;
			break;
		}
		break;
	case LEDLIGHT_SET_BLINK:
		LV5216_N_LOG("not supported. cmd 0x%08x", cmd);
		return -ENOTTY;
	case LEDLIGHT_SET_HWCHECK:
		ret = copy_from_user(&st_ioctl,
					argp,
					sizeof(T_LEDLIGHT_IOCTL));
		if (ret) {
			LV5216_N_LOG("Error leds_ioctl(cmd = LEDLIGHT_SET_CONTROL_MODE)");
			return -EFAULT;
		}

		LV5216_N_LOG("LEDLIGHT_SET_HWCHECK cmd=0x%08x", st_ioctl.data[0]);
		switch(st_ioctl.data[0]) {
		case LEDLIGHT_PARAM_HWCHECK_VLEDONOFF:
			LV5216_N_LOG("LEDLIGHT_PARAM_HWCHECK_VLEDONOFF onoff=0x%08x", st_ioctl.data[1]);
			if (st_ioctl.data[1] == 0)
				lv5216_i2c_masked_write(data->client, 0x00, 0x00, 0xFF);
			else
				lv5216_i2c_masked_write(data->client, 0x00, 0x81, 0xFF);
			break;
		default:
			LV5216_N_LOG("hw check invalid param 0x%08x", st_ioctl.data[0]);
			return -EFAULT;
			break;
		}
		break;
	default:
		LV5216_N_LOG("invalid cmd 0x%08x", cmd);
		return -ENOTTY;
	}

	LV5216_V_LOG("[OUT]");

	return 0;
}

static struct file_operations leds_fops = {
	.owner		  = THIS_MODULE,
	.open		 = leds_open,
	.release	= leds_release,
	.unlocked_ioctl = leds_ioctl,
};

static struct miscdevice leds_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "leds-ledlight",
	.fops  = &leds_fops,
};

static int lv5216_remove(struct i2c_client *client)
{
	struct lv5216_data *data = i2c_get_clientdata(client);
	struct lv5216_led_data *led;
	int i;

	LV5216_V_LOG("[IN]");

	lv5216_data = NULL;

	if (data) {
		misc_deregister(&leds_device);

		for (i = 0; i < LED_TYPE_MAX; i++) {
			led = &data->led_data[i];

			if (led->cdev_registered) {
				led_classdev_unregister(&led->cdev);
				flush_work(&led->work);
				led->cdev_registered = false;
			}
			mutex_destroy(&led->param_next_lock);
		}
		destroy_workqueue(data->work_queue);
		data->work_queue = NULL;
//		if (gpio_is_valid(data->keyled_gpio)) {
//			gpio_free(data->keyled_gpio);
//		}
#ifndef KEEP_DEVICE_STATE_ON_PROBE
		gpio_free(data->reset_gpio);
#endif
		i2c_set_clientdata(client, NULL);
		mutex_destroy(&data->param_lock);
		mutex_destroy(&data->control_lock);
		mutex_destroy(&data->mled_lock);
		wake_lock_destroy(&data->wake_lock);
		kfree(data);
	}

	LV5216_V_LOG("[OUT]");

	return 0;
}

static void lv5216_init_pwm_enable(void)
{
	LV5216_V_LOG("[IN]");
#ifdef ENABLE_PWM
		LV5216_N_LOG("pwm enable.");
		lv5216_reg_info_save[0x01].value |= 0x80;
		lv5216_reg_info_save[0x01].written = false;
#else
	LV5216_N_LOG("pwm disable.");
#endif
	LV5216_V_LOG("[OUT]");
}

static int lv5216_init_client(struct i2c_client *client)
{
	struct lv5216_data *data = i2c_get_clientdata(client);
#ifdef FORCE_WLED_ON
	uint8_t read_buf;
#endif
	LV5216_V_LOG("[IN] client=0x%p", client);

#ifndef KEEP_DEVICE_STATE_ON_PROBE
	// GPIO	HARDWARE Reset L->H
	gpio_set_value(data->reset_gpio, 0);
	usleep_range(30, 30);
#endif
	gpio_set_value(data->reset_gpio, 1);
	usleep_range(30, 30);

#ifndef KEEP_DEVICE_STATE_ON_PROBE
	lv5216_i2c_masked_write(client, 0x01, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x02, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x03, 0x00, 0x00);
#endif
#ifndef KEEP_DEVICE_STATE_ON_PROBE
	lv5216_i2c_masked_write(client, 0x06, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x07, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x08, 0x00, 0x00);
#endif
	lv5216_i2c_masked_write(client, 0x09, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x0A, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x0B, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x0C, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x0D, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x0E, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x0F, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x10, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x11, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x12, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x13, 0x00, 0x00);
	lv5216_i2c_masked_write(client, 0x14, 0x00, 0x00);

//	if ( wled_stat_aboot_level != 0 ) {
		lv5216_power_control(data, LED_TYPE_MLED, true);
//	}

#ifdef FORCE_WLED_ON
	// force WLED ON
	LV5216_V_LOG("force WLED ON");

	lv5216_i2c_write(client, 0x00, LV5216_VLED_ON);
	usleep_range(2000, 2000);

//	lv5216_i2c_read(client, 0x15, &read_buf, 1);
	lv5216_i2c_write(client, 0x01, LV5216_LED_ENABLE);			// WLED ON
#endif
	LV5216_V_LOG("[OUT]");

	return 0;
}

static int kclight_check_device(struct i2c_client *client)
{
	struct pinctrl				*dc_pinctrl; //dc: device check
	struct pinctrl_state        *dc_pd; //pd:pull down
	struct pinctrl_state        *dc_np; //np:non pull
	int							basecheck_gpio;

	dc_pinctrl = devm_pinctrl_get(&client->dev);
	basecheck_gpio = of_get_named_gpio(client->dev.of_node, "kc,basecheck-gpio", 0);
	if (!gpio_is_valid(basecheck_gpio) || IS_ERR_OR_NULL(dc_pinctrl)) {
		LV5216_E_LOG("basecheck_gpio error");
	} else {
		// set pull down
		dc_pd = pinctrl_lookup_state(dc_pinctrl, "dev_check_pd");
		dc_np = pinctrl_lookup_state(dc_pinctrl, "dev_check_np");

		if ((IS_ERR(dc_pd)) || (IS_ERR(dc_np))) {
			LV5216_E_LOG("pinctrl error");
		}

		pinctrl_select_state(dc_pinctrl, dc_pd);
		usleep_range(2000, 3000);
		if (gpio_get_value(basecheck_gpio) == 0) {
			// LV5216
			LV5216_E_LOG("LV5216 detect");
			board_check = true;
		} else {
			// LP5569
			LV5216_E_LOG("LP5569 detect");
			// basecheck_gpio change to NP
			pinctrl_select_state(dc_pinctrl, dc_np);
		}
	}

	return board_check;
}

static ssize_t kclight_mled_max_brightness_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long state;
	ssize_t ret;

	pr_err("[KCLIGHT] %s: max_brightness=%s\n", __func__, buf);

//	sscanf(buf, "%d %d %d %d", &led_type, &onoff, &on_time, &off_time);
	ret = kstrtoul(buf, 10, &state);
	if (ret) {
		pr_err("[KCLIGHT] %s: max_brightness err\n", __func__);
		return count;
	}

	pr_err("[KCLIGHT] %s: max_brightness=%ld\n", __func__, state);
	lv5216_set_maxbrightness(state, 1);

	return count;
}

static ssize_t kclight_mled_max_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	struct lv5216_led_data *led;

	led = &lv5216_data->led_data[LED_TYPE_MLED];

	return sprintf(buf, "%d\n", led->cdev.max_brightness);
}

static DEVICE_ATTR(kc_max_brightness, 0664, kclight_mled_max_brightness_show, kclight_mled_max_brightness_store);

static struct attribute *kclight_mled_attrs[] = {
	&dev_attr_kc_max_brightness.attr,
	NULL
};

static const struct attribute_group kclight_mled_attr_group = {
	.attrs = kclight_mled_attrs,
};

static int lv5216_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct lv5216_data *data;
	struct lv5216_led_data *led;
	struct device_node *node;
	struct device_node *temp;
	const char *led_label;
	const char *linux_name;
	int i;
	int err = 0;
	u32 value = 0;

	LV5216_V_LOG("[IN] client=0x%p", client);

	if (!kclight_check_device(client)) {
		LV5216_E_LOG("LV5216 don't start");
		return -1;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		LV5216_E_LOG("fail i2c_check_functionality");
		goto exit;
	}

	node = client->dev.of_node;
	if (node == NULL){
		LV5216_E_LOG("client->dev.of_node == null");
		err = -ENODEV;
		goto exit;
	}

	data = kzalloc(sizeof(struct lv5216_data), GFP_KERNEL);
	if (!data) {
		LV5216_E_LOG("Failed kzalloc");
		err = -ENOMEM;
		goto exit;
	}

	wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, LV5216_DRV_NAME);
	mutex_init(&data->control_lock);
	mutex_init(&data->param_lock);
	mutex_init(&data->mled_lock);
	data->client = client;
	i2c_set_clientdata(client, data);
	INIT_DELAYED_WORK(&data->blink_work, lv5216_blink_work);

	data->work_queue = create_singlethread_workqueue(LV5216_DRV_NAME);
	if (!data->work_queue) {
		err = -ENODEV;
		goto fail_create_workqueue;
	}

//	data->keyled_gpio = -ENOSYS;
	data->keyled_on_gpio = -ENOSYS;
	data->sled_gpio = -ENOSYS;
	temp = NULL;
	while ((temp = of_get_next_child(node, temp))) {

		LV5216_V_LOG("read label");
		err = of_property_read_string(temp, "label", &led_label);
		if (err < 0) {
			LV5216_E_LOG("Failure reading label, Dev=[0x%08x] err=[%d]", (unsigned int)&client->dev, err);
			continue;
		}
		LV5216_V_LOG("read linux,name");
		err = of_property_read_string(temp, "linux,name", &linux_name);
		if (err < 0) {
			LV5216_E_LOG("Failure reading linux,name, Dev=[0x%08x] err=[%d]", (unsigned int)&client->dev, err);
			continue;
		}

		LV5216_V_LOG("label=%s linux,name=%s", led_label, linux_name);
		led = NULL;
		if (strcmp(led_label, LABEL_MLED) == 0) {
			LV5216_V_LOG("probe MLED");
			led = &data->led_data[LED_TYPE_MLED];
			led->led_type = LED_TYPE_MLED;
			led->ctrl_func = lv5216_ctrl_mled;
			led->workqueue = system_wq;
		} else if (strcmp(led_label, LABEL_RGB) == 0) {
			LV5216_V_LOG("probe RGB");
			led = &data->led_data[LED_TYPE_RGB];
			led->led_type = LED_TYPE_RGB;
			led->ctrl_func = lv5216_ctrl_rgb;
			led->workqueue = data->work_queue;

			data->blink_ctl.blink_round_en = of_property_read_bool(temp, "kc,blink-round-en");
			LV5216_E_LOG("blink_round_en=%d", data->blink_ctl.blink_round_en);
			if (data->blink_ctl.blink_round_en) {
				err = of_property_read_u32(temp, "kc,blink-round-ontime", &value);
				if (!err) {
					data->blink_ctl.blink_round_ontime = value;
					LV5216_E_LOG("blink_round_ontime=%d", data->blink_ctl.blink_round_ontime);
				} else {
					data->blink_ctl.blink_round_en = 0;
					LV5216_E_LOG("not support blink_round_ontime blink_round_en=%d", data->blink_ctl.blink_round_en);
				}

				err = of_property_read_u32(temp, "kc,blink-round-offtime", &value);
				if (!err) {
					data->blink_ctl.blink_round_offtime = value;
					LV5216_E_LOG("blink_round_offtime=%d", data->blink_ctl.blink_round_offtime);
				} else {
					data->blink_ctl.blink_round_en = 0;
					LV5216_E_LOG("not support blink_round_offtime blink_round_en=%d", data->blink_ctl.blink_round_en);
				}
			}
		} else if (strcmp(led_label, LABEL_CLED) == 0) {
			LV5216_V_LOG("probe MLED");
			led = &data->led_data[LED_TYPE_CLED];
			led->led_type = LED_TYPE_CLED;
			led->ctrl_func = lv5216_ctrl_cled;
			led->workqueue = system_wq;
		} else if (strcmp(led_label, LABEL_KEYLED) == 0) {
			LV5216_V_LOG("probe KEYLED");
			led = &data->led_data[LED_TYPE_KEYLED];
			led->led_type = LED_TYPE_KEYLED;
			led->ctrl_func = lv5216_ctrl_keyled;
			led->workqueue = system_wq;
//			data->keyled_gpio = of_get_named_gpio(client->dev.of_node, "kc,keyled-gpio", 0);
//			if (!gpio_is_valid(data->keyled_gpio)) {
//				LV5216_E_LOG("No valid KEYLED GPIO specified %d", data->keyled_gpio);
//				goto fail_id_check;
//			}
			data->keyled_on_gpio = of_get_named_gpio(client->dev.of_node, "kc,keyled-on-gpio", 0);
			if (!gpio_is_valid(data->keyled_on_gpio)) {
				LV5216_E_LOG("No valid KEYLED ON GPIO specified %d", data->keyled_on_gpio);
				goto fail_id_check;
			}
		} else if (strcmp(led_label, LABEL_SLED) == 0) {
			LV5216_V_LOG("probe SLED");
			led = &data->led_data[LED_TYPE_SLED];
			led->led_type = LED_TYPE_SLED;
			led->ctrl_func = lv5216_ctrl_sled;
			led->workqueue = system_wq;
			data->sled_gpio = of_get_named_gpio(client->dev.of_node, "kc,subled-gpio", 0);
			if (!gpio_is_valid(data->sled_gpio)) {
				LV5216_E_LOG("No valid SUBLED GPIO specified %d", data->sled_gpio);
				goto fail_id_check;
			}
		} else if (strcmp(led_label, LABEL_ALED) == 0) {
			LV5216_V_LOG("probe ALED");
			led = &data->led_data[LED_TYPE_ALED];
			led->led_type = LED_TYPE_ALED;
			led->ctrl_func = NULL;
			led->workqueue = system_wq;
		} else {
			LV5216_N_LOG("unknown label:%s", led_label);
		}

		if (led) {
			mutex_init(&led->param_next_lock);
			led->cdev.brightness_set = led_set;
			led->cdev.brightness_get = led_get;
			led->cdev.name			 = linux_name;
			led->cdev.max_brightness = BRIGHTNESS_MAX;
			if ( strcmp(led_label, LABEL_MLED) == 0 ) {
				led->cdev.brightness = KCLIGHT_WLED_BOOT_DEFAULT;
				led->param_next.value = led->cdev.brightness;
				led->cdev.max_brightness = BL_BRIGHTNESS_MAX;
				LV5216_N_LOG("brightness=%d", led->cdev.brightness);
			}
			INIT_WORK(&led->work, lv5216_work);
			led->parent = data;
			err = led_classdev_register(&client->dev, &led->cdev);
			if (err < 0) {
				LV5216_E_LOG("unable to register led %s", led->cdev.name);
				goto fail_id_check;
			}
			if (strcmp(led_label, LABEL_MLED) == 0) {
				err = sysfs_create_group(&led->cdev.dev->kobj, &kclight_mled_attr_group);
				if (err) {
					LV5216_E_LOG("unable to register sysfs mled %s", led->cdev.name);
					goto fail_id_check;
				}
			}
			led->cdev_registered = true;
		}
	}

	err = misc_register(&leds_device);
	if (err < 0) {
		LV5216_E_LOG("unable to register misc device");
		goto fail_misc_register;
	}

	data->reset_gpio = of_get_named_gpio(client->dev.of_node, "kc,reset-gpio", 0);
	if (!gpio_is_valid(data->reset_gpio)) {
		LV5216_E_LOG("No valid RESET GPIO specified %d", data->reset_gpio);
		err = -ENODEV;
		goto fail_get_reset_gpio;
	}

#ifndef KEEP_DEVICE_STATE_ON_PROBE
	err = gpio_request(data->reset_gpio, LV5216_DRV_NAME);
	LV5216_V_LOG("gpio_request GPIO=%d err=%d", data->reset_gpio, err);
	if (err < 0) {
		LV5216_E_LOG("failed to request GPIO=%d, ret=%d",
				data->reset_gpio,
				err);
		goto fail_request_reset_gpio;
	}
#endif

//	if (gpio_is_valid(data->keyled_gpio)) {
//		err = gpio_request(data->keyled_gpio, LV5216_DRV_NAME);
//		LV5216_V_LOG("gpio_request GPIO=%d err=%d", data->keyled_gpio, err);
//		if (err < 0) {
//			LV5216_E_LOG("failed to request GPIO=%d, ret=%d",
//					data->keyled_gpio,
//					err);
//			goto fail_request_keyled_gpio;
//		}
//	}

	if (gpio_is_valid(data->keyled_on_gpio)) {
		err = gpio_request(data->keyled_on_gpio, LV5216_DRV_NAME);
		LV5216_V_LOG("gpio_request ON GPIO=%d err=%d", data->keyled_on_gpio, err);
		if (err < 0) {
			LV5216_E_LOG("failed to request ON GPIO=%d, ret=%d",
					data->keyled_on_gpio,
					err);
			goto fail_request_keyled_on_gpio;
		}
	}

	lv5216_init_pwm_enable();

	err = lv5216_init_client(client);
	if (err)
	{
		LV5216_E_LOG("Failed LV5216_init_client");
		goto fail_init_client;
	}

	data->param.keyled_threshold = KEYLED_THRESHOLD_DEFAULT;
	data->colorvariation = COLORVARIATION_DEFAULT;

	data->reboot_notifier.notifier_call = kclight_reboot_notify;
	err = register_reboot_notifier(&data->reboot_notifier);
	if (err) {
		LV5216_E_LOG("[KCLIGHT] %s: failed to register reboot notifier: %d\n", __func__, err);
	}

	lv5216_data = data;
//	ext_ctrl_mode = 0;

	LV5216_V_LOG("[OUT]");

	return 0;

fail_init_client:
	if (gpio_is_valid(data->keyled_on_gpio)) {
		gpio_free(data->keyled_on_gpio);
	}
fail_request_keyled_on_gpio:
//	if (gpio_is_valid(data->keyled_gpio)) {
//		gpio_free(data->keyled_gpio);
//	}
//fail_request_keyled_gpio:
#ifndef KEEP_DEVICE_STATE_ON_PROBE
	gpio_free(data->reset_gpio);
fail_request_reset_gpio:
#endif
fail_get_reset_gpio:
	misc_deregister(&leds_device);
fail_misc_register:
fail_id_check:
	for (i = 0; i < LED_TYPE_MAX; i++) {
		led = &data->led_data[i];

		if (led->cdev_registered) {
			led_classdev_unregister(&led->cdev);
			flush_work(&led->work);
			led->cdev_registered = false;
		}
		mutex_destroy(&led->param_next_lock);
	}
	destroy_workqueue(data->work_queue);
	data->work_queue = NULL;
fail_create_workqueue:
	i2c_set_clientdata(client, NULL);
	mutex_destroy(&data->param_lock);
	mutex_destroy(&data->control_lock);
	mutex_destroy(&data->mled_lock);
	wake_lock_destroy(&data->wake_lock);
	kfree(data);
exit:
	LV5216_E_LOG("[OUT] err=%d", err);
	return err;
}

static const struct i2c_device_id lv5216_id[] = {
	{ LV5216_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lv5216_id);

static struct of_device_id lv5216_match_table[] = {
	{ .compatible = LV5216_DRV_NAME,},
	{ },
};

static struct i2c_driver lv5216_driver = {
	.driver = {
		.name   = LV5216_DRV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = lv5216_match_table,
	},
	.probe  = lv5216_probe,
	.remove = lv5216_remove,
	.id_table = lv5216_id,
};

static int32_t __init lv5216_led_init(void)
{
	int32_t rc = 0;

	LV5216_V_LOG("[IN]");

	rc = i2c_add_driver(&lv5216_driver);
	if (rc != 0) {
		LV5216_E_LOG("can't add i2c driver");
		rc = -ENOTSUPP;
		return rc;
	}

	LV5216_V_LOG("[OUT]");

	return rc;

}

static void __exit lv5216_led_exit(void)
{
	LV5216_V_LOG("[IN]");

	i2c_del_driver(&lv5216_driver);

	LV5216_V_LOG("[OUT]");
}

int32_t lv5216_light_led_disp_set_panel(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class)
{
	LV5216_V_LOG("[IN] panel_class=0x%x", panel_class);
	
	atomic_set(&g_panel_class,panel_class);

	switch( panel_class ){
	case LIGHT_LCD_PANEL0:
		LV5216_V_LOG("panel class = LIGHT_LCD_PANEL0");
		break;
	case LIGHT_LCD_PANEL1:
		LV5216_V_LOG("panel class = LIGHT_LCD_PANEL1");
		break;
	case LIGHT_LCD_PANEL2:
		LV5216_V_LOG("panel class = LIGHT_LCD_PANEL2");
		break;
	default:
		LV5216_E_LOG("unknown panel class");
		break;
	}

	return light_led_disp_set(disp_status);
}
EXPORT_SYMBOL(lv5216_light_led_disp_set_panel);

#ifndef DISABLE_DISP_DETECT

static void lv5216_queue_mled_work(void)
{
	struct lv5216_data *data = lv5216_data;
	struct lv5216_led_data *led;

	LV5216_V_LOG("[IN]");

	if (data) {
		led = &data->led_data[LED_TYPE_MLED];
		if (led->cdev_registered) {
			if (led->param_next.value) {
				queue_work(led->workqueue, &led->work);
			}
		}
	}

	LV5216_V_LOG("[OUT]");
}

static atomic_t g_display_detect = ATOMIC_INIT(0);

static bool light_led_disp_enabled(void)
{
	return (atomic_read(&g_display_detect) == 1) ? true : false;
}

static int32_t light_led_disp_set(e_light_main_wled_disp disp_status)
{
	int32_t ret = 0;

	LV5216_V_LOG("[IN] disp_status=[0x%x]", (uint32_t)disp_status);

	if ((atomic_read(&g_display_detect)) != 0) {
		LV5216_V_LOG("already determined.");
		return ret;
	}

	switch(disp_status) {
	case LIGHT_MAIN_WLED_LCD_EN:
		LV5216_N_LOG("LIGHT_MAIN_WLED_LCD_EN");
		atomic_set(&g_display_detect,1);
//		lv5216_queue_mled_work();
		break;
	case LIGHT_MAIN_WLED_LCD_DIS:
		LV5216_N_LOG("LIGHT_MAIN_WLED_LCD_DIS");
		atomic_set(&g_display_detect,-1);
		break;
	default:
		break;
	}

	LV5216_V_LOG("[OUT] ret=%d", ret);
	return ret;
}

static atomic_t g_detect_lcd_pwren = ATOMIC_INIT(0);

static bool light_lcd_power_off(void)
{
	return (atomic_read(&g_detect_lcd_pwren) == -1) ? true : false;
}

int32_t lv5216_light_led_disp_power_set(e_light_main_wled_disp disp_status)
{
	struct lv5216_data *data = lv5216_data;
	struct lv5216_led_data *led;
	struct lv5216_led_param param_next;
	int32_t ret = 0;

	LV5216_V_LOG("[IN] disp_status=[0x%x]", (uint32_t)disp_status);

	if (!lv5216_data) {
		LV5216_E_LOG("lv5216_data null");
		return 0;
	}

	switch(disp_status) {
	case LIGHT_MAIN_WLED_LCD_PWREN:
		LV5216_V_LOG("LIGHT_MAIN_WLED_LCD_PWREN");
		atomic_set(&g_detect_lcd_pwren,1);
		if (light_led_disp_enabled()) {
			if (bl_skip_flag) {
				bl_skip_flag = 0;
			} else {
				lv5216_queue_mled_work();
			}
		}
		break;
	case LIGHT_MAIN_WLED_LCD_PWRDIS:
		LV5216_V_LOG("LIGHT_MAIN_WLED_LCD_PWRDIS");
		atomic_set(&g_detect_lcd_pwren,-1);
		if (light_led_disp_enabled()) {
			if (data) {
				led = &data->led_data[LED_TYPE_MLED];
				if (led->cdev_registered) {
	                mutex_lock(&led->param_next_lock);
	                memcpy(&param_next, &led->param_next, sizeof(param_next));
	                mutex_unlock(&led->param_next_lock);
				    lv5216_ctrl_mled(led, &param_next);
					LV5216_V_LOG(" prev.value=%x  next.value=%x \n", led->param_prev.value,led->param_next.value);
				}
			}
		}
		break;
	default:
		break;
	}

	LV5216_V_LOG("[OUT] ret=%d", ret);
	return ret;
}
EXPORT_SYMBOL(lv5216_light_led_disp_power_set);

#else  /* DISABLE_DISP_DETECT */

static bool light_led_disp_enabled(void)
{
	return true;
}

static int32_t light_led_disp_set(e_light_main_wled_disp disp_status)
{
	LV5216_N_LOG("DISABLE_DISP_DETECT");
	return 0;
}

int32_t lv5216_light_led_disp_power_set(e_light_main_wled_disp disp_status)
{
	LV5216_N_LOG("invalid DISCARD_BL_LCDPOWER_OFF disp_status=0x%x",  (uint32_t)disp_status);
	return 0;
}
EXPORT_SYMBOL(lv5216_light_led_disp_power_set);

static bool light_lcd_power_off(void)
{
	return 0;
}
#endif  /* DISABLE_DISP_DETECT */

int32_t lv5216_light_led_disp_test_led(int onoff)
{
	struct lv5216_data *data = lv5216_data;
	struct lv5216_led_data *led;
	struct lv5216_led_param param_next;
	int32_t ret = 0;

	LV5216_N_LOG("[IN] onoff=[0x%x]", (uint32_t)onoff);

	led = &data->led_data[LED_TYPE_KEYLED];
	if (onoff==1) {
		param_next.value = 0x00ff0000;
	} else {
		param_next.value = 0x00000000;
	}
	lv5216_ctrl_keyled(led, &param_next);

	LV5216_N_LOG("[OUT] ret=%d", ret);
	return ret;
}
EXPORT_SYMBOL(lv5216_light_led_disp_test_led);

void lv5216_set_maxbrightness(int max_level, int enable)
{
	struct lv5216_led_data *led;
	struct lv5216_data *data = lv5216_data;

	pr_notice("[KCLIGHT]%s: max_level=%d enable=%d", __func__, max_level, enable);

	if (!lv5216_data) {
		LV5216_E_LOG("lv5216_data null");
		return;
	}

	mutex_lock(&data->mled_lock);

	led = &data->led_data[LED_TYPE_MLED];
	if (enable == 1) {
		led->cdev.max_brightness = max_level;
	} else {
		led->cdev.max_brightness = BL_BRIGHTNESS_MAX;
	}

	mutex_unlock(&data->mled_lock);

	lv5216_queue_mled_work();
}
EXPORT_SYMBOL(lv5216_set_maxbrightness);

module_init(lv5216_led_init);
module_exit(lv5216_led_exit);

MODULE_AUTHOR("KYOCERA Corporation");
MODULE_DESCRIPTION("LED");
MODULE_LICENSE("GPL");
