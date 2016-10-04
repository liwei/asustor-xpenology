#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Synology Monaco NAS Board GPIO Setup
 *
 * Maintained by: Comsumer Platform Team <cpt@synology.com>
 *
 * Copyright 2009-2015 Synology, Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/gpio.h>
#include <linux/synobios.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#ifdef  MY_ABC_HERE
extern char gszSynoHWVersion[];
#endif

#define GPIO_UNDEF				0xFF

#define DISK_LED_OFF			0
#define DISK_LED_GREEN_SOLID	1
#define DISK_LED_ORANGE_SOLID	2
#define DISK_LED_ORANGE_BLINK	3
#define DISK_LED_GREEN_BLINK    4

#define SYNO_LED_OFF		0
#define SYNO_LED_ON		1
#define SYNO_LED_BLINKING	2

#define OUTPUT 0
#define INPUT 1
#define ACTIVE_LOW 0
#define ACTIVE_HIGH 1
#define ACTIVE_KEEP_VALUE 2

struct syno_gpio_device {
	const char *name;
	u8 nr_gpio;
	u8 gpio_port[4];
	u8 gpio_direction[4];
	u8 gpio_init_value[4];
};

struct syno_gpio_device fan_ctrl = {
	.name				= "fan ctrl",
	.nr_gpio			= 3,
	.gpio_port			= {122, 121, 120}, /* MAX MID LOW */
	.gpio_direction		= {OUTPUT, OUTPUT, OUTPUT},
	.gpio_init_value	= {ACTIVE_KEEP_VALUE, ACTIVE_KEEP_VALUE, ACTIVE_KEEP_VALUE}
};

struct syno_gpio_device hdd_pm_ctrl = {
	.name				= "sata power",
	.nr_gpio			= 2,
	.gpio_port			= {112, 115},
	.gpio_direction		= {OUTPUT, OUTPUT},
	.gpio_init_value	= {ACTIVE_KEEP_VALUE, ACTIVE_KEEP_VALUE}
};

struct syno_gpio_device fan_sense = {
	.name				= "fan sense",
	.nr_gpio			= 1,
	.gpio_port			= {114},
	.gpio_direction		= {INPUT},
};

struct syno_gpio_device model_id = {
	.name 				= "model id",
	.nr_gpio			= 3,
	.gpio_port			= {125, 126, 127},
	.gpio_direction		= {INPUT, INPUT, INPUT}
};

struct syno_gpio_device disk_led_ctrl = {
	.name				= "disk led ctrl",
	.nr_gpio			= 1,
	.gpio_port			= {118},
	.gpio_direction		= {OUTPUT},
	.gpio_init_value	= {ACTIVE_LOW}
};

struct syno_gpio_device phy_led_ctrl = {
	.name               = "phy led ctrl",
	.nr_gpio			= 1,
	.gpio_port			= {113},
	.gpio_direction		= {OUTPUT},
	.gpio_init_value	= {ACTIVE_HIGH}
};

unsigned int SynoModelIDGet(void)
{
	return 0;
}

int SYNO_MONACO_GPIO_PIN(int pin, int *pValue, int isWrite)
{
	int ret = -1;

	if (!pValue)
		goto END;

	if (1 == isWrite)
		gpio_set_value(pin, *pValue);
	else
		*pValue = gpio_get_value(pin);

	ret = 0;
END:
	return 0;
}

int SYNO_MONACO_GPIO_BLINK(int pin, int blink)
{
	return 0;
}

void SYNO_ENABLE_HDD_LED(int blEnable)
{
	gpio_set_value(disk_led_ctrl.gpio_port[0], blEnable);
}

void SYNO_ENABLE_PHY_LED(int blEnable)
{
	gpio_set_value(phy_led_ctrl.gpio_port[0], blEnable);
}

int SYNO_SOC_HDD_LED_SET(int index, int status)
{
	return 0;
}

int SYNO_CTRL_EXT_CHIP_HDD_LED_SET(int index, int status)
{
	return 0;
}

int SYNO_CTRL_USB_HDD_LED_SET(int status)
{
	return 0;
}

int SYNO_CTRL_POWER_LED_SET(int status)
{
	return 0;
}

int SYNO_CTRL_HDD_POWERON(int index, int value)
{
	if ((index <= 0) || (hdd_pm_ctrl.nr_gpio < index))
		goto ERROR;

	if (hdd_pm_ctrl.gpio_port[index - 1])
		gpio_set_value(hdd_pm_ctrl.gpio_port[index - 1], value);

	return 0;
ERROR:
	printk(KERN_NOTICE "HDD index error : HDD[%d] %s\n", index, __func__);
	WARN_ON_ONCE(1);
	return -1;
}

int SYNO_CTRL_FAN_PERSISTER(int index, int status, int isWrite)
{
	if ((index <= 0) || fan_ctrl.nr_gpio < index)
		goto ERROR;

	gpio_set_value(fan_ctrl.gpio_port[index - 1], status);

	return 0;
ERROR:
	printk(KERN_NOTICE "FAN index error : FAN[%d] %s\n",index, __func__);
	WARN_ON_ONCE(1);
	return -1;
}

int SYNO_CTRL_FAN_STATUS_GET(int index, int *pValue)
{
	if ((index <= 0) || fan_sense.nr_gpio < index)
		goto ERROR;

	*pValue = gpio_get_value(fan_sense.gpio_port[index - 1]);

	return 0;
ERROR:
	printk(KERN_NOTICE "FAN SENSE index error : FAN[%d] %s\n", index, __func__);
	WARN_ON_ONCE(1);
	return -1;
}

int SYNO_CTRL_ALARM_LED_SET(int status)
{
	return 0;
}

int SYNO_CTRL_BACKPLANE_STATUS_GET(int *pStatus)
{
	return 0;
}

int SYNO_CTRL_BUZZER_CLEARED_GET(int *pValue)
{
	return 0;
}

/* SYNO_CHECK_HDD_PRESENT
 * Check HDD present for evansport
 * input : index - disk index, 1-based.
 * output: 0 - HDD not present,  1 - HDD present.
 */
int SYNO_CHECK_HDD_PRESENT(int index)
{
	return 1;
}

/* SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER
 * Query support HDD dynamic Power .
 * output: 0 - support, 1 - not support.
 */
int SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER(void)
{
	return 1;
}

struct disk_cnt {
	char *hw_version;
	int max_disk_id;
};

static inline int MAX_DISK(struct disk_cnt *tbl)
{
	int i=0;
	while (tbl[i].hw_version) {
		if (syno_is_hw_version(tbl[i].hw_version))
			return tbl[i].max_disk_id;
		i++;
	}
	return 255;
}

unsigned char SYNOMonacoIsBoardNeedPowerUpHDD(u32 disk_id)
{
	int max_disk = 0;
	if (syno_is_hw_version(HW_DS216play)) {
		max_disk = 2;
	}

	return (disk_id <= max_disk)? 1 : 0;
}

EXPORT_SYMBOL(SYNOMonacoIsBoardNeedPowerUpHDD);
EXPORT_SYMBOL(SYNO_MONACO_GPIO_PIN);
EXPORT_SYMBOL(SYNO_MONACO_GPIO_BLINK);
EXPORT_SYMBOL(SYNO_ENABLE_HDD_LED);
EXPORT_SYMBOL(SYNO_ENABLE_PHY_LED);
EXPORT_SYMBOL(SYNO_SOC_HDD_LED_SET);
EXPORT_SYMBOL(SYNO_CTRL_EXT_CHIP_HDD_LED_SET);
EXPORT_SYMBOL(SYNO_CTRL_USB_HDD_LED_SET);
EXPORT_SYMBOL(SYNO_CTRL_POWER_LED_SET);
EXPORT_SYMBOL(SYNO_CTRL_HDD_POWERON);
EXPORT_SYMBOL(SYNO_CTRL_FAN_PERSISTER);
EXPORT_SYMBOL(SYNO_CTRL_FAN_STATUS_GET);
EXPORT_SYMBOL(SYNO_CTRL_ALARM_LED_SET);
EXPORT_SYMBOL(SYNO_CTRL_BACKPLANE_STATUS_GET);
EXPORT_SYMBOL(SYNO_CTRL_BUZZER_CLEARED_GET);
EXPORT_SYMBOL(SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER);

static int syno_gpio_device_init(struct syno_gpio_device *gpio_dev, 
		struct platform_device *pdev)
{
	u8 i = 0, value = -1;
	int ret = -1;
	const struct syno_gpio_device *gd = gpio_dev;

	if (NULL == gd)
		goto ERROR;

	if ((gd->nr_gpio > 4) || (gd->nr_gpio == 0))
		goto ERROR;

	for(i = 0; i < gd->nr_gpio; i++) {
		ret = devm_gpio_request(&pdev->dev, gd->gpio_port[i], gd->name);
		if (ret)
			goto ERROR;

		if (ACTIVE_KEEP_VALUE == gd->gpio_direction[i])
			value = gpio_get_value(gd->gpio_port[i]);
		else
			value = gd->gpio_init_value[i];

		if (0 == gd->gpio_direction[i])
			ret = gpio_direction_output(gd->gpio_port[i], value);
		else if (1 == gd->gpio_direction[i])
			ret = gpio_direction_input(gd->gpio_port[i]);

		if (ret)
			goto ERROR;
	}

	printk(KERN_NOTICE "synology : %16s initialized.\n", gd->name);
	return 0;

ERROR:
	return -1;
}

static int synology_gpio_probe(struct platform_device *pdev)
{
	syno_gpio_device_init(&fan_ctrl, pdev);
	syno_gpio_device_init(&hdd_pm_ctrl, pdev);
	syno_gpio_device_init(&fan_sense, pdev);
	syno_gpio_device_init(&model_id, pdev);
	syno_gpio_device_init(&disk_led_ctrl, pdev);
	syno_gpio_device_init(&phy_led_ctrl, pdev);

	return 0;
}
static int synology_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id of_synology_gpio_match[] = {
       { .compatible = "syno, fan-ctrl", },
       { .compatible = "syno, fan-sense", },
       { .compatible = "syno, soc-hdd-pm", },
       { .compatible = "syno, gpio", },
       {},
};

static struct platform_driver synology_gpio_driver = {
	.probe      = synology_gpio_probe,
	.remove     = synology_gpio_remove,
	.driver     = {
		.name   = "synology-gpio",
		.owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(of_synology_gpio_match),
	},
};

static int __init synology_gpio_init(void)
{
	return platform_driver_register(&synology_gpio_driver);
}
device_initcall(synology_gpio_init);
