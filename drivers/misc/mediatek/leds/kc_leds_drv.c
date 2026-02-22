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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "kc_leds_drv.h"
#include <linux/kc_leds_drv.h>

#include "mtk_leds_drv.h"


extern void mt6370_pmu_led_bright_set(struct led_classdev *led_cdev,
	enum led_brightness bright);

extern int mt6370_pmu_led_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off);
extern unsigned int kcGetMaxbrightness(void);
extern int setMaxbrightness(int max_level, int enable);
extern inline int mt6370_pmu_led_update_bits(struct led_classdev *led_cdev,
	uint8_t reg_addr, uint8_t reg_mask, uint8_t reg_data);


static struct kc_rgb_info *kc_rgb_data = NULL;
static struct kc_led_info *kc_leds_data;



void kc_rgb_led_info_get(int num , struct led_classdev *data)
{
	int i;
	
	if(kc_rgb_data == NULL)
	{
		kc_rgb_data = kmalloc(3 * sizeof(struct kc_rgb_info), GFP_KERNEL);
		kc_rgb_data[0].name = "red";
		kc_rgb_data[0].point = -1;
		
		kc_rgb_data[1].name = "green";
		kc_rgb_data[1].point = -1;	

		kc_rgb_data[2].name = "blue";
		kc_rgb_data[2].point = -1;		
	}
	
	for(i = 0 ; i < 3 ; i++)
	{
		if(strcmp(kc_rgb_data[i].name , data->name) == 0)
		{
			kc_rgb_data[i].dev_class = data;
			kc_rgb_data[i].point = num;
		}
	}
	
		

}

/****************************************************************************
 * DEBUG MACROS
 ***************************************************************************/
 /*
static int debug_enable_led = 1;
#define LEDS_DRV_DEBUG(format, args...) do { \
	if (debug_enable_led) {	\
		pr_debug(format, ##args);\
	} \
} while (0)

*/


/*
void kc_leds_rgb_set_work(struct work_struct *work)
{
	return;
}
*/

static int _kc_leds_rgb_set(int brightness)
{
	int red,green,blue;
	
	unsigned long on_timer = kc_leds_data->on_time;
	unsigned long off_timer = kc_leds_data->off_time;
	
	if(kc_rgb_data[0].point == -1 || kc_rgb_data == NULL || brightness < 0)
	{
		TRICOLOR_DEBUG_LOG("%s RGB INFO ERROR\n",__func__);
		return -1;
	}
	
	red 	= (brightness & 0x00FF0000) >> 16;
	green 	= (brightness & 0x0000FF00) >> 8;
	blue 	= (brightness & 0x00000FF);
	
	TRICOLOR_DEBUG_LOG("%s color = %x red = %x green = %x blue = %x\n",__func__, brightness , red , green , blue );
	
	kc_rgb_data[1].dev_class->brightness = green;
	kc_rgb_data[0].dev_class->brightness = red;
	
	if(kc_leds_data->on_time == 0 || kc_leds_data->off_time == 0)
	{
		on_timer = 2;
		off_timer = 3;
	}

	if(kc_leds_data->blue_support == false && green == 0 && blue > 0)
		green = blue;
	
	mt6370_pmu_led_blink_set(kc_rgb_data[0].dev_class,&on_timer,&off_timer);
	mt6370_pmu_led_blink_set(kc_rgb_data[1].dev_class,&on_timer,&off_timer);
	

	mt6370_pmu_led_bright_set(kc_rgb_data[0].dev_class , red );
	mt6370_pmu_led_bright_set(kc_rgb_data[1].dev_class , green );
	
	if(kc_leds_data->red_first_control_flag == true)
	{
		mt6370_pmu_led_update_bits(kc_rgb_data[0].dev_class,0x92,0x80,0x80);
		kc_leds_data->red_first_control_flag = false;
	}
	
	
	if(kc_leds_data->blue_support == true && kc_rgb_data[2].point != -1)
	{
		mt6370_pmu_led_blink_set(kc_rgb_data[2].dev_class,&on_timer,&off_timer);
		mt6370_pmu_led_bright_set(kc_rgb_data[1].dev_class , green );
	}
	return 0;
}


static void kc_leds_rgb_set(struct led_classdev *led_cdev,
			   enum led_brightness level)
{
	TRICOLOR_DEBUG_LOG("%s level = %x\n",__func__, level );

	_kc_leds_rgb_set(level);
}



static ssize_t led_rgb_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	u32 color_info;
	int ret;
	
	ret = kstrtou32(buf, 10, &color_info);
	
	if (ret)
		return ret;
	
	_kc_leds_rgb_set(color_info);
	
	return count;
}


static DEVICE_ATTR(rgb, 0664, NULL, led_rgb_store);

static struct attribute *led_rgb_attrs[] = {
	&dev_attr_rgb.attr,
	NULL
};

static int pwm_set(const char *buf,int mode)
{
	u32 time;
	int ret;
	
	unsigned long on_timer;
	unsigned long off_timer;
	
	ret = kstrtou32(buf, 10, &time);
	
	if (ret)
		return ret;
	
	if(mode == 0)
		kc_leds_data->on_time = time;
	else
		kc_leds_data->off_time = time;
	
	
	on_timer = kc_leds_data->on_time;
	off_timer = kc_leds_data->off_time;
	
	if(on_timer == 0 || off_timer ==  0)
	{
		on_timer = 500;
		off_timer = 0;
	}
	
	mt6370_pmu_led_blink_set(kc_rgb_data[0].dev_class,&on_timer,&off_timer);
	mt6370_pmu_led_blink_set(kc_rgb_data[1].dev_class,&on_timer,&off_timer);
	
	if(kc_leds_data->blue_support == true && kc_rgb_data[2].point != -1)
		mt6370_pmu_led_blink_set(kc_rgb_data[2].dev_class,&on_timer,&off_timer);
	
	return 0;
}

static ssize_t pwm_lo_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	pwm_set(buf,0);

	return count;
}

static ssize_t pwm_hi_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	pwm_set(buf,1);

	return count;
}

static DEVICE_ATTR(pwm_lo, 0664, NULL, pwm_lo_store);
static DEVICE_ATTR(pwm_hi, 0664, NULL, pwm_hi_store);

static struct attribute *pwm_attrs[] = {
	&dev_attr_pwm_lo.attr,
	&dev_attr_pwm_hi.attr,
	NULL
};



static const struct attribute_group led_rgb_attr_group = {
	.attrs = led_rgb_attrs,
};

static const struct attribute_group pwm_attr_group = {
	.attrs = pwm_attrs,
};



static long kclights_leds_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int32_t ret = -1;
	T_LEDLIGHT_IOCTL		st_ioctl;

    TRICOLOR_DEBUG_LOG("%s(): [IN]",__func__);
	
    switch (cmd) {
    case LEDLIGHT_SET_BLINK:
        TRICOLOR_DEBUG_LOG("%s(): LEDLIGHT_SET_BLINK",__func__);
        ret = copy_from_user(&st_ioctl,
                    argp,
                    sizeof(T_LEDLIGHT_IOCTL));
        if (ret) {
            TRICOLOR_DEBUG_LOG("%s(): Error leds_ioctl(cmd = LEDLIGHT_SET_BLINK)", __func__);
            return -EFAULT;
        }

        if ((st_ioctl.data[1]!=0 && st_ioctl.data[2]==0) ||
            (st_ioctl.data[1]==0 && st_ioctl.data[2]!=0)) {
            TRICOLOR_DEBUG_LOG("%s(): Error on/off time parameters invalid", __func__);
            return -EFAULT;
        }

		mutex_lock(&kc_leds_data->request_lock);
        TRICOLOR_DEBUG_LOG("%s(): mutex_lock", __func__);

		kc_leds_data->mode      = st_ioctl.data[0];
		kc_leds_data->on_time   = st_ioctl.data[1];
		kc_leds_data->off_time  = st_ioctl.data[2];
		kc_leds_data->off_color = st_ioctl.data[3];

		TRICOLOR_DEBUG_LOG("prep mode=[%d] on_time=[%d] off_time=[%d] off_color=[0x%08x]",
			kc_leds_data->mode, kc_leds_data->on_time, kc_leds_data->off_time, kc_leds_data->off_color);

        TRICOLOR_DEBUG_LOG("%s(): mutex_unlock", __func__);
		mutex_unlock(&kc_leds_data->request_lock);
        break;
    default:
        TRICOLOR_DEBUG_LOG("%s(): default", __func__);
        return -ENOTTY;
    }

    TRICOLOR_DEBUG_LOG("%s(): [OUT]",__func__);

    return 0;
}



static struct file_operations kclights_leds_fops = {
    .owner        = THIS_MODULE,
	.open           = simple_open,
    .unlocked_ioctl = kclights_leds_ioctl,
    .compat_ioctl   = kclights_leds_ioctl,
};

////////////////////////////////////////////////////////////////////////////////

extern void mt6370_pmu_bled_en_set(bool flag);
static bled_en_state = 0;
static int kc_bl_limit = 255;
static int kc_bl_req_level = 255;
static DEFINE_MUTEX(kc_bl_mutex);

void kc_leds_bl_mutex_lock(void)
{
	mutex_lock(&kc_bl_mutex);
}

void kc_leds_bl_mutex_unlock(void)
{
	mutex_unlock(&kc_bl_mutex);
}

void kc_leds_store_bl_req_level(int level)
{
	kc_bl_req_level = level;
}

int kc_leds_show_bl_req_level(void)
{
	return kc_bl_req_level;
}

void kc_leds_set_bl_max_level(int max_level)
{
	pr_err("[LIGHT]%s kc_bl_limit=%d\n", __func__, kc_bl_limit);

	kc_bl_limit = max_level;
}

int kc_leds_get_bl_max_level(void)
{
	return kc_bl_limit;
}

void kc_wled_bled_en_set(bool flag)
{
	TRICOLOR_NOTICE_LOG("%s: flag=%d", __func__, flag);

	bled_en_state = flag;
	mt6370_pmu_bled_en_set(bled_en_state);
}

static ssize_t backlight_allow_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	u32 flag;

	ret = kstrtou32(buf, 10, &flag);
	if (!ret) {
		kc_wled_bled_en_set(flag);
	} else {
		TRICOLOR_ERR_LOG("%s: param error", __func__);
	}

	return count;
}

static ssize_t backlight_allow_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
    return sprintf(buf, "%d\n", bled_en_state);
}

static ssize_t backlight_max_brightness_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	int max_level;

	ret = kstrtou32(buf, 10, &max_level);
	if (!ret) {
		if (max_level < 255) {
			setMaxbrightness(max_level, 1);
		} else {
			setMaxbrightness(max_level, 0);
		}
	}

	return count;
}

static ssize_t backlight_max_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	int max_level;

	max_level = kc_leds_get_bl_max_level();

    return sprintf(buf, "%d\n", max_level);
}

static ssize_t backlight_brightness_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	unsigned int level;

	level = kc_leds_show_bl_req_level();

    return sprintf(buf, "%d\n", level);
}

static ssize_t backlight_value_show(struct device *dev, struct device_attribute *dev_attr, char * buf)
{
	unsigned int value;

	value = mt_get_bl_brightness();

    return sprintf(buf, "%d\n", value);
}

static DEVICE_ATTR(bl_allow, 0664, backlight_allow_show, backlight_allow_store);
static DEVICE_ATTR(bl_max_brightness, 0664, backlight_max_brightness_show, backlight_max_brightness_store);
static DEVICE_ATTR(bl_brightness, 0664, backlight_brightness_show, NULL);
static DEVICE_ATTR(bl_value, 0664, backlight_value_show, NULL);

static struct attribute *backlight_attrs[] = {
	&dev_attr_bl_allow.attr,
	&dev_attr_bl_max_brightness.attr,
	&dev_attr_bl_brightness.attr,
	&dev_attr_bl_value.attr,
	NULL
};


static const struct attribute_group backlight_attr_group = {
	.attrs = backlight_attrs,
};



static int kc_leds_probe(struct platform_device *pdev)
{
	int rc;
	TRICOLOR_DEBUG_LOG("%s\n",__func__);
	
	kc_leds_data = kzalloc(sizeof(struct kc_led_info), GFP_KERNEL);
	
	if(!kc_leds_data)
		return -ENOMEM;

	mutex_init(&kc_leds_data->request_lock);

	dev_set_drvdata(&pdev->dev, kc_leds_data);

	kc_leds_data->cdev.name = "ledinfo";

	kc_leds_data->cdev.brightness_set = kc_leds_rgb_set;
	kc_leds_data->cdev.max_brightness = 0x00FFFFFF;
	
	kc_leds_data->on_time = 0;
	kc_leds_data->off_time = 0;
	kc_leds_data->red_first_control_flag = true;
	
	
	kc_leds_data->mdev.minor = MISC_DYNAMIC_MINOR;
	kc_leds_data->mdev.name = "leds-ledlight";
	kc_leds_data->mdev.fops = &kclights_leds_fops;
	rc =  misc_register(&kc_leds_data->mdev);
		
	kc_leds_data->blue_support = of_property_read_bool(pdev->dev.of_node, "blue_support");

	if(kc_leds_data->blue_support == false)
		TRICOLOR_DEBUG_LOG("%s BLUE NOT SUPPORT\n",__func__);
	
	rc = led_classdev_register(&pdev->dev, &kc_leds_data->cdev);
	
	if(rc > 0)
		goto probe_error;
	
	rc = 0;
	rc += sysfs_create_group(&kc_leds_data->cdev.dev->kobj,&led_rgb_attr_group);
	rc += sysfs_create_group(&kc_leds_data->cdev.dev->kobj,&pwm_attr_group);
	rc += sysfs_create_group(&kc_leds_data->cdev.dev->kobj,&backlight_attr_group);
	
	if(rc > 0)
		goto probe_error;
	
	return 0;
	
probe_error:
	led_classdev_unregister(&kc_leds_data->cdev);
	return rc;

}


static int kc_leds_remove(struct platform_device *pdev)
{
	if(kc_rgb_data != NULL)
		kfree(kc_rgb_data);

	
	led_classdev_unregister(&kc_leds_data->cdev);
	
	sysfs_remove_group(&kc_leds_data->cdev.dev->kobj,	&led_rgb_attr_group);
	sysfs_remove_group(&kc_leds_data->cdev.dev->kobj,	&pwm_attr_group);
	sysfs_remove_group(&kc_leds_data->cdev.dev->kobj,	&backlight_attr_group);
	
	return 0;
}

/*

static void kc_leds_shutdown(struct platform_device *pdev)
{
	return;
}

*/

static const struct of_device_id kc_led_of_match[] = {
	{ .compatible = "kc,kc_leds", },
	{}
};

MODULE_DEVICE_TABLE(of, kc_led_of_match);

static struct platform_driver kc_leds_driver = {
	.driver = {
		   	.name = "kc_leds",
		   	.owner = THIS_MODULE,
			.of_match_table = kc_led_of_match,
		   },
	.probe = kc_leds_probe,
	.remove = kc_leds_remove,
//	.shutdown = kc_leds_shutdown,
};

static int __init kc_leds_init(void)
{
	int ret;

	TRICOLOR_DEBUG_LOG("%s\n",__func__);

	ret = platform_driver_register(&kc_leds_driver);

	return ret;
}

static void __exit kc_leds_exit(void)
{
	platform_driver_unregister(&kc_leds_driver);
}


late_initcall(kc_leds_init);
module_exit(kc_leds_exit);

MODULE_AUTHOR("Kyocera Inc.");
MODULE_DESCRIPTION("LED driver for MediaTek MT65xx chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("kc-leds-mt65xx");
