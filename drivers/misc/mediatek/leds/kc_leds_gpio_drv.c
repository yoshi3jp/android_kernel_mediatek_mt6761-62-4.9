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

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "mtk_leds_sw.h"
#include "mtk_leds_hal.h"

#include <linux/time.h>
#include <linux/alarmtimer.h>

#define GPIO_LED_NUM	3
#define LED_STATE_ON	1
#define LED_STATE_OFF	0

struct kc_led_gpio_info {
	struct platform_device	*pdev;
	struct led_classdev		cdev;
	struct 	mutex			request_lock;
	int						num_leds;
	const char				*name;
	/* blink */
	uint32_t    			led_type;
	uint32_t    			onoff;
	uint32_t    			on_time;
	uint32_t    			off_time;
	uint32_t				state;
	int						gpio_no;
	struct alarm 			led_timer;
};

static struct kc_led_gpio_info *kc_leds_gpio_data = NULL;

/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/
 
static int debug_enable_led = 1;
#define LEDS_DRV_DEBUG(format, args...) do { \
	if (debug_enable_led) {	\
		pr_err(format, ##args);\
	} \
} while (0)

static void kc_mt_mt65xx_led_set_gpio(struct kc_led_gpio_info *led, int level)
{
	int ret;
	int gpio_num;

	pr_debug("%s\n", __func__);

	if (led->gpio_no < 0) {
		return;
	}

	gpio_num = led->gpio_no;

	ret = devm_gpio_request_one(led->cdev.dev, gpio_num, GPIOF_DIR_OUT, led->name);
	if (ret < 0)
	{
		pr_err("%s: can't request gpio with gpio_data:%d err:%d\n", gpio_num, ret, __func__);
		return;
	}
	gpio_direction_output(gpio_num, (level<=0) ? 0 : 1);
	gpio_set_value(gpio_num, (level<=0) ? 0 : 1);
	devm_gpio_free(led->cdev.dev, gpio_num);
}

static void kc_leds_gpio_set(struct led_classdev *led_cdev,
			   enum led_brightness brightness)
{
	struct kc_led_gpio_info *kcled;

	if (led_cdev == NULL) {
		pr_err("%s: cdev error\n", __func__);
		return;
	}

	kcled = container_of(led_cdev, struct kc_led_gpio_info, cdev);

	kc_mt_mt65xx_led_set_gpio(kcled, brightness);
}

static enum alarmtimer_restart led_timer_function(struct alarm *alarm, ktime_t now)
{
	struct kc_led_gpio_info *kcled;
	int on_off_time = 0;
	int i;
	struct timespec time, time_now;
	ktime_t ktime;
	
	get_monotonic_boottime(&time_now);
	
	if (!kc_leds_gpio_data) {
		pr_err("%s: kc_leds_gpio_data error\n", __func__);
		return ALARMTIMER_NORESTART;
	}

	mutex_lock(&kc_leds_gpio_data->request_lock);

	kcled = kc_leds_gpio_data;

	pr_debug("%s\n", __func__);

	for (i = 0; i < GPIO_LED_NUM; i++) {
		if ((kcled->onoff != 0)&&(kcled->on_time != 0)&&(kcled->off_time != 0)) {
			if (kcled->state == LED_STATE_ON) {
				kc_mt_mt65xx_led_set_gpio(kcled, 0);
				kcled->state = LED_STATE_OFF;

				if (!on_off_time){
					on_off_time = kcled->off_time;
				} else if (on_off_time > kcled->off_time) {
					on_off_time = kcled->off_time;
				}
			} else {
				kc_mt_mt65xx_led_set_gpio(kcled, 255);
				kcled->state = LED_STATE_ON;
				if (!on_off_time){
					on_off_time = kcled->on_time;
				} else if (on_off_time > kcled->on_time) {
					on_off_time = kcled->on_time;
				}
			}
		}
		kcled++;
	}
	
	if (on_off_time != 0) {	
		if(on_off_time > 1000)
			time.tv_sec = on_off_time / 1000;
		else
			time.tv_sec = 0;
		
		time.tv_nsec = (on_off_time - time.tv_sec * 1000) * 1000000;
				
		ktime = ktime_set(time.tv_sec, time.tv_nsec);
		
		alarm_start_relative(&kc_leds_gpio_data->led_timer, ktime);	
	}

	mutex_unlock(&kc_leds_gpio_data->request_lock);

	return ALARMTIMER_RESTART;
}


static ssize_t gpio_led_ctrl_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int onoff;
	int led_type;
	int on_time;
	int off_time;
	struct kc_led_gpio_info *led;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct kc_led_gpio_info	*kcled;
	int i;
	int time_on_off = 0;
	
	struct timespec time, time_now;
	ktime_t ktime;	
	
	get_monotonic_boottime(&time_now);

	pr_debug("%s buf=%s\n", __func__, buf);

	if (!kc_leds_gpio_data) {
		pr_err("%s: kc_leds_gpio_data error\n", __func__);
		return count;
	}

	alarm_cancel(&kc_leds_gpio_data->led_timer);

	mutex_lock(&kc_leds_gpio_data->request_lock);

	sscanf(buf, "%d %d %d %d", &led_type, &onoff, &on_time, &off_time);

	pr_debug("%s led_type=%d onoff=%d on_time=%d off_time=%d\n",
				__func__, led_type, onoff, on_time, off_time);

	led = container_of(led_cdev, struct kc_led_gpio_info, cdev);
	led->led_type = led_type;
	led->onoff = onoff;
	led->on_time = on_time;
	led->off_time = off_time;

	kcled = kc_leds_gpio_data;
	for (i = 0; i < GPIO_LED_NUM; i++) {

		pr_debug("%s kcled[%d] onoff=%d on_time=%d off_time=%d\n",
					__func__, i, kcled->onoff, kcled->on_time, kcled->off_time);

		kc_mt_mt65xx_led_set_gpio(kcled, 0);
		kcled->state = LED_STATE_OFF;

		if (kcled->onoff != 0) {
			kc_mt_mt65xx_led_set_gpio(kcled, 255);
			kcled->state = LED_STATE_ON;
		}

		if ((kcled->onoff != 0)&&(kcled->on_time != 0)&&(kcled->off_time != 0)) {
			if (!time_on_off){
				time_on_off = kcled->on_time;
			} else if (time_on_off > kcled->on_time) {
				time_on_off = kcled->on_time;
			}
		}
		kcled++;
	}

	if (time_on_off != 0) {
		pr_debug("%s mod_timer time=%d\n", __func__, time);
		
		if(time_on_off > 1000)
			time.tv_sec = time_on_off / 1000;
		else
			time.tv_sec = 0;
		
		time.tv_nsec = (time_on_off - time.tv_sec * 1000) * 1000000;
		
		ktime = ktime_set(time.tv_sec, time.tv_nsec);
		
		alarm_start_relative(&kc_leds_gpio_data->led_timer, ktime);
	}

	mutex_unlock(&kc_leds_gpio_data->request_lock);

	return count;
}

static ssize_t gpio_led_ctrl_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct kc_led_gpio_info *led;

	led = container_of(led_cdev, struct kc_led_gpio_info, cdev);


	return sprintf(buf, "onoff=%d on_time=%d off_time=%d state=%d\n",
				led->onoff, led->on_time, led->off_time, led->state);
}

static DEVICE_ATTR(control, 0664, gpio_led_ctrl_show, gpio_led_ctrl_store);

static struct attribute *gpio_led_attrs[] = {
	&dev_attr_control.attr,
	NULL
};

static const struct attribute_group gpio_led_attr_group = {
	.attrs = gpio_led_attrs,
};


static int kc_leds_gpio_probe(struct platform_device *pdev)
{

	int rc;
	int num_leds = 0;
	int count = 0;
	struct device_node *node, *temp;
	
	struct kc_led_gpio_info *led, *led_array;
	int ret = 0;

	temp = NULL;
	node = pdev->dev.of_node;
	
	LEDS_DRV_DEBUG("%s\n",__func__);
	
	while ((temp = of_get_next_child(node, temp)))
		num_leds++;
	
	if(!num_leds)
		return -ECHILD;

	LEDS_DRV_DEBUG("%s num_leds=%d\n", __func__, num_leds);

	led_array = devm_kcalloc(&pdev->dev, num_leds, sizeof(struct kc_led_gpio_info),
				GFP_KERNEL);
	
	
	if (!led_array)
		return -ENOMEM;
	
	for_each_child_of_node(node, temp) 
	{
		led = &led_array[count];
		
		rc = of_property_read_string(temp, "kc,name",	&led->name);
		if(rc < 0) {
			pr_err("%s kc,name not found\n",__func__);
			goto fail_id_check;
		}

		led->num_leds = num_leds;
		led->pdev = pdev;
		led->cdev.brightness_set    = kc_leds_gpio_set;
		led->led_type = 0;
		led->onoff = 0;
		led->on_time = 0;
		led->off_time = 0;
		led->state = LED_STATE_OFF;
		mutex_init(&led->request_lock);

		rc = of_property_read_string(temp, "kc,sysfs_name",	&led->cdev.name);
		if(rc < 0) {
			pr_err("%s sysfsname not found\n",__func__);
			goto fail_id_check;
		}

		led->gpio_no = of_get_named_gpio(temp, "gpio_data", 0);
		pr_err("%s: GPIO of_get_named_gpio returns:%d\n", __func__, ret);
		if (led->gpio_no < 0)
		{
			pr_err("%s: of_get_named_gpio fail, use alternative method\n", __func__);
			goto fail_id_check;
		}

		rc = led_classdev_register(&pdev->dev, &led->cdev);

		sysfs_create_group(&led->cdev.dev->kobj,&gpio_led_attr_group);

		alarm_init(&led->led_timer, ALARM_BOOTTIME,
			led_timer_function);

		count++;
	}

	kc_leds_gpio_data = led_array;
	dev_set_drvdata(&pdev->dev, led_array);

	return 0;

fail_id_check:

	return 0;
}

static int kc_leds_gpio_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&kc_leds_gpio_data->cdev);

	sysfs_remove_group(&kc_leds_gpio_data->cdev.dev->kobj,	&gpio_led_attr_group);

	return 0;
}

static void kc_leds_gpio_shutdown(struct platform_device *pdev)
{
	return;
}

static const struct of_device_id kc_led_gpio_of_match[] = {
	{ .compatible = "kc,kc_leds_gpio", },
	{}
};

MODULE_DEVICE_TABLE(of, kc_led_gpio_of_match);

static struct platform_driver kc_leds_gpio_driver = {
	.driver = {
		   	.name = "kc_leds_gpio",
		   	.owner = THIS_MODULE,
			.of_match_table = kc_led_gpio_of_match,
		   },
	.probe = kc_leds_gpio_probe,
	.remove = kc_leds_gpio_remove,
	.shutdown = kc_leds_gpio_shutdown,
};

static int __init kc_leds_gpio_init(void)
{
	int ret;

	LEDS_DRV_DEBUG("%s\n", __func__);
	ret = platform_driver_register(&kc_leds_gpio_driver);

	if (ret) {
		LEDS_DRV_DEBUG("kc_leds_gpio_init:drv:E%d\n", ret);

		return ret;
	}
	return ret;
}

static void __exit kc_leds_gpio_exit(void)
{
	platform_driver_unregister(&kc_leds_gpio_driver);
}

late_initcall(kc_leds_gpio_init);
module_exit(kc_leds_gpio_exit);

MODULE_AUTHOR("Kyocera Inc.");
MODULE_DESCRIPTION("LED driver for MediaTek MT65xx chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("kc-leds-mt65xx");
