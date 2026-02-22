/*
 * This software is contributed or developed by KYOCERA Corporation.
 * (C) 2019 KYOCERA Corporation
 * (C) 2022 KYOCERA Corporation
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
 
#ifndef _KC_LEDS_DRV_LINUX_H
#define _KC_LEDS_DRV_LINUX_H

typedef enum {
//	LIGHT_MAIN_WLED_DIS     = 0x0000,
	LIGHT_MAIN_WLED_LCD_EN  = 0x0001,
//	LIGHT_MAIN_WLED_LED_EN  = 0x0002,
	LIGHT_MAIN_WLED_LCD_PWREN  = 0x0004,
	LIGHT_MAIN_WLED_LCD_DIS = 0x0010,
//	LIGHT_MAIN_WLED_LED_DIS = 0x0020,
	LIGHT_MAIN_WLED_LCD_PWRDIS = 0x0040,
//	LIGHT_MAIN_WLED_EN      = (LIGHT_MAIN_WLED_LCD_EN|LIGHT_MAIN_WLED_LED_EN),
} e_light_main_wled_disp;

typedef enum {
	LIGHT_SUB_WLED_LCD_PWREN  = 0x0004,
	LIGHT_SUB_WLED_LCD_PWRDIS = 0x0040,
} e_light_sub_wled_disp;

typedef enum {
	LIGHT_LCD_PANEL0 = 0x0000,
	LIGHT_LCD_PANEL1 = 0x0001,
	LIGHT_LCD_PANEL2 = 0x0002,
	LIGHT_LCD_PANEL3 = 0x0003,
} e_light_lcd_panel;

extern int32_t light_led_disp_set_panel(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class);
extern int32_t light_led_disp_power_set(e_light_main_wled_disp disp_status);
extern int32_t light_led_subdisp_power_set(e_light_sub_wled_disp disp_status);

extern bool lv5216_get_board_id(void);
extern bool lp5569_get_board_id(void);

extern void kc_leds_set_maxbrightness(int max_level, int enable);
extern int32_t kc_wled_bled_en_set(e_light_main_wled_disp disp_status);
extern int32_t kc_wled_bled_connect_set(e_light_main_wled_disp disp_status, e_light_lcd_panel panel_class);

#endif
