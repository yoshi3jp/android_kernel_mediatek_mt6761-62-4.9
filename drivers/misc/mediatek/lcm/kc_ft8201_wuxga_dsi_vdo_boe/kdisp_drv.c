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
#include <linux/reboot.h>
#include "kdisp_com.h"

static struct kcdisp_info kcdisp_data;

extern void kdisp_mtkfb_reboot_shutdown(void);
extern void kctp_reboot_shutdown(void);

struct kcdisp_info *kdisp_drv_get_data(void)
{
	return &kcdisp_data;
}

void kdisp_drv_set_panel_state(int onoff)
{
	struct kcdisp_info *kcdisp_data;

	kcdisp_data = kdisp_drv_get_data();
	kcdisp_data->panel_state = onoff;
}

int kdisp_drv_get_panel_state(void)
{
	struct kcdisp_info *kcdisp_data;

	kcdisp_data = kdisp_drv_get_data();
	return kcdisp_data->panel_state;
}

static void kdisp_drv_init(void)
{
	struct kcdisp_info *kcdisp_data;

	kcdisp_data = kdisp_drv_get_data();
//	kdisp_bl_ctrl_init(kc_ctrl_data);
//	kdisp_diag_init();
//	kdisp_connect_init(panel, kcdisp_data);
//	kdisp_panel_init(np, kc_ctrl_data);
	kdisp_sre_init();
	kdisp_touch_ctrl_init();

//	if (!kdisp_connect_get_panel_detect()) {
//		kdisp_com_set_panel_state(0);
//	} else {
//		kdisp_com_set_panel_state(1);
//	}
}

#define CLASS_NAME "kcdisp"
#define DRIVER_NAME "kcdisp"
static ssize_t kcdisp_test_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_info *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->test_mode);
}

static ssize_t kcdisp_test_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_info *kcdisp = dev_get_drvdata(dev);
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
	struct kcdisp_info *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->sre_mode);
}

static ssize_t kcdisp_sre_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int sre_mode = 0;
	int rc = 0;

	rc = kstrtoint(buf, 10, &sre_mode);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	kdisp_set_sre_mode(sre_mode);
	pr_err("[KCLCM]sre_mode=%d\n", sre_mode);

	return count;
}

static ssize_t kcdisp_disp_connect_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_info *kcdisp = dev_get_drvdata(dev);

	pr_notice("[KCLCM]detect_1st=%d detect_status=%d\n",
		kcdisp->panel_detect_1st, kcdisp->panel_detect_status);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			kcdisp->panel_detect_status);
}

static ssize_t kcdisp_disp_connect_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_info *kcdisp = dev_get_drvdata(dev);
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
	struct kcdisp_info *kcdisp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->refresh_disable);
}

static ssize_t kcdisp_refresh_disable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_info *kcdisp = dev_get_drvdata(dev);
	int rc = 0;

//	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
//		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
//		return 0;
//	}

	rc = kstrtoint(buf, 10, &kcdisp->refresh_disable);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_notice("[KCLCM]refresh_disable=%d\n", kcdisp->refresh_disable);

	return count;
}

ssize_t kcdisp_touch_easywake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcdisp_info *kcdisp = kdisp_drv_get_data();

	return snprintf(buf, PAGE_SIZE, "%d\n", kcdisp->easywake_req);
}

ssize_t kcdisp_touch_easywake_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct kcdisp_info *kcdisp = kdisp_drv_get_data();
	int rc = 0;

	rc = kstrtoint(buf, 10, &kcdisp->easywake_req);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return rc;
	}

	pr_notice("[KCLCM]easywake mode=%d\n", kcdisp->easywake_req);

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
static DEVICE_ATTR(easywake_mode, S_IRUGO | S_IWUSR,
	kcdisp_touch_easywake_show, kcdisp_touch_easywake_store);

static struct attribute *kcdisp_attrs[] = {
	&dev_attr_test_mode.attr,
	&dev_attr_sre_mode.attr,
	&dev_attr_disp_connect.attr,
	&dev_attr_refresh_disable.attr,
	&dev_attr_easywake_mode.attr,
	NULL,
};

static struct attribute_group kcdisp_attr_group = {
	.attrs = kcdisp_attrs,
};

#if 0
void kdisp_drv_mutex_lock(void)
{
	struct kcdisp_info *kcdisp = kdisp_drv_get_data();

	mutex_lock(&kcdisp->kc_ctrl_lock);
}

void kdisp_drv_mutex_unlock(void)
{
	struct kcdisp_info *kcdisp = kdisp_drv_get_data();

	mutex_unlock(&kcdisp->kc_ctrl_lock);
}
#endif

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

static int kc_lcm_init_sysfs(struct kcdisp_info *kcdisp)
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

extern int kc_lcm_get_panel_detect(void);

static int kcdisp_reboot_notify(struct notifier_block *nb,
				unsigned long code, void *unused)
{
	pr_notice("[KCDISP]%s +\n", __func__);
	
	if (kc_lcm_get_panel_detect() == PANEL_NOT_FOUND) {
		pr_err("[KCDISP]%s skip for panel not detect\n",__func__);
		return NOTIFY_DONE;
	}
	
	kdisp_touch_set_easywake_forceoff();
	kctp_reboot_shutdown();
	kdisp_mtkfb_reboot_shutdown();
	
	pr_notice("[KCDISP]%s -\n", __func__);

	return NOTIFY_DONE;
}

static int kc_lcm_platform_probe(struct platform_device *pdev)
{
	struct kcdisp_info *kcdisp;
	int error;

	kcdisp = &kcdisp_data;

	kcdisp->test_mode           = 0;
	kcdisp->sre_mode            = 0;
	kcdisp->refresh_disable     = 0;
	kcdisp->refresh_counter		= 0;
	kcdisp->panel_detect_status = PANEL_NO_CHECK;
	kcdisp->panel_detect_1st = 0;
	kcdisp->easywake_req       = 0;

//	mutex_init(&kcdisp->kc_ctrl_lock);

	kc_lcm_init_sysfs(kcdisp);

	kdisp_drv_init();
	kcdisp->reboot_notifier.notifier_call = kcdisp_reboot_notify,
	error = register_reboot_notifier(&kcdisp->reboot_notifier);
	if (error) {
		dev_err(&pdev->dev, "[KCDISP]failed to register reboot notifier: %d\n",
			error);
	}

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
