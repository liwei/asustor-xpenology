#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Synology Alpine NAS Board GPIO Setup
 *
 * Maintained by:  KueiHuan Chen <khchen@synology.com>
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
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/synobios.h>
#include <linux/export.h>

#ifndef HW_DS2015xs
#define HW_DS2015xs "DS2015xs"
#endif
#ifndef HW_DS1515
#define HW_DS1515 "DS1515"
#endif
#ifndef HW_DS715p
#define HW_DS715p "DS715+"
#endif
#ifndef HW_DS215p
#define HW_DS215p "DS215+"
#endif
#ifndef HW_DS416
#define HW_DS416 "DS416"
#endif

#define GPIO_UNDEF              0xFF

/* copied from synobios.h */
#define DISK_LED_OFF            0
#define DISK_LED_GREEN_SOLID    1
#define DISK_LED_ORANGE_SOLID   2
#define DISK_LED_ORANGE_BLINK   3
#define DISK_LED_GREEN_BLINK    4

#ifdef MY_ABC_HERE
extern char gszSynoHWVersion[];
#endif

typedef struct __tag_SYNO_HDD_DETECT_GPIO {
	u8 hdd1_present_detect;
	u8 hdd2_present_detect;
	u8 hdd3_present_detect;
	u8 hdd4_present_detect;
	u8 hdd5_present_detect;
	u8 hdd6_present_detect;
	u8 hdd7_present_detect;
	u8 hdd8_present_detect;
} SYNO_HDD_DETECT_GPIO;

typedef struct __tag_SYNO_HDD_PM_GPIO {
	u8 hdd1_pm;
	u8 hdd2_pm;
	u8 hdd3_pm;
	u8 hdd4_pm;
	u8 hdd5_pm;
	u8 hdd6_pm;
	u8 hdd7_pm;
	u8 hdd8_pm;
} SYNO_HDD_PM_GPIO;

typedef struct __tag_SYNO_FAN_GPIO {
	u8 fan_1;
	u8 fan_2;
	u8 fan_fail;
	u8 fan_fail_2;
} SYNO_FAN_GPIO;

typedef struct __tag_SYNO_MODEL_GPIO {
	u8 model_id_0;
	u8 model_id_1;
	u8 model_id_2;
	u8 model_id_3;
} SYNO_MODEL_GPIO;

typedef struct __tag_SYNO_EXT_HDD_LED_GPIO {
	u8 hdd1_led_0;
	u8 hdd1_led_1;
	u8 hdd2_led_0;
	u8 hdd2_led_1;
	u8 hdd3_led_0;
	u8 hdd3_led_1;
	u8 hdd4_led_0;
	u8 hdd4_led_1;
	u8 hdd5_led_0;
	u8 hdd5_led_1;
	u8 hdd_led_mask;
} SYNO_EXT_HDD_LED_GPIO;

typedef struct __tag_SYNO_MULTI_BAY_GPIO {
	u8 inter_lock;
} SYNO_MULTI_BAY_GPIO;

typedef struct __tag_SYNO_SOC_HDD_LED_GPIO {
	u8 hdd1_fail_led;
	u8 hdd2_fail_led;
	u8 hdd3_fail_led;
	u8 hdd4_fail_led;
	u8 hdd5_fail_led;
	u8 hdd6_fail_led;
	u8 hdd7_fail_led;
	u8 hdd8_fail_led;
	u8 hdd1_act_led;
	u8 hdd2_act_led;
	u8 hdd3_act_led;
	u8 hdd4_act_led;
	u8 hdd5_act_led;
	u8 hdd6_act_led;
	u8 hdd7_act_led;
	u8 hdd8_act_led;
} SYNO_SOC_HDD_LED_GPIO;

typedef struct __tag_SYNO_RACK_GPIO {
	u8 buzzer_mute_req;
	u8 buzzer_mute_ack;
	u8 rps1_on;
	u8 rps2_on;
} SYNO_RACK_GPIO;

typedef struct __tag_SYNO_STATUS_LED_GPIO {
	u8 alarm_led;
	u8 power_led;
} SYNO_STATUS_LED_GPIO;

typedef struct __tag_SYNO_GPIO {
	SYNO_HDD_DETECT_GPIO    hdd_detect;
	SYNO_EXT_HDD_LED_GPIO   ext_sata_led;
	SYNO_SOC_HDD_LED_GPIO   soc_sata_led;
	SYNO_MODEL_GPIO         model;
	SYNO_FAN_GPIO           fan;
	SYNO_HDD_PM_GPIO        hdd_pm;
	SYNO_RACK_GPIO          rack;
	SYNO_MULTI_BAY_GPIO     multi_bay;
	SYNO_STATUS_LED_GPIO    status;
} SYNO_GPIO;

static SYNO_GPIO generic_gpio;

int SYNO_ALPINE_GPIO_PIN(int pin, int *pValue, int isWrite)
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

int SYNO_CTRL_HDD_POWERON(int index, int value)
{
	int ret = -1;

	switch (index) {
	case 1:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd1_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd1_pm, value);
		break;
	case 2:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd2_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd2_pm, value);
		break;
	case 3:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd3_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd3_pm, value);
		break;
	case 4:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd4_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd4_pm, value);
		break;
	case 5:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd5_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd5_pm, value);
		break;
	case 6:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd6_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd6_pm, value);
		break;
	case 7:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd7_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd7_pm, value);
		break;
	case 8:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd8_pm);
		gpio_set_value(generic_gpio.hdd_pm.hdd8_pm, value);
		break;
	default:
		goto END;
	}

	ret = 0;
END:
	return ret;
}

int SYNO_CTRL_FAN_PERSISTER(int index, int status, int isWrite)
{
	int ret = 0;
	u8 pin = GPIO_UNDEF;

	switch (index) {
	case 1:
		pin = generic_gpio.fan.fan_1;
		break;
	case 2:
		pin = generic_gpio.fan.fan_2;
		break;
	default:
		ret = -1;
		printk("%s fan not match\n", __FUNCTION__);
		goto END;
	}

	WARN_ON(GPIO_UNDEF == pin);
	gpio_set_value(pin, status);
END:
	return ret;
}

int SYNO_CTRL_ALARM_LED_SET(int status)
{
	WARN_ON(GPIO_UNDEF == generic_gpio.status.alarm_led);

	gpio_set_value(generic_gpio.status.alarm_led, status);
	return 0;
}

int SYNO_CHECK_HDD_PRESENT(int index)
{
	int iPrzVal = 1; /* defult is present */

	switch (index) {
	case 1:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd1_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd1_present_detect);
		}
		break;
	case 2:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd2_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd2_present_detect);
		}
		break;
	case 3:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd3_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd3_present_detect);
		}
		break;
	case 4:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd4_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd4_present_detect);
		}
		break;
	case 5:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd5_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd5_present_detect);
		}
		break;
	case 6:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd6_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd6_present_detect);
		}
		break;
	case 7:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd7_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd7_present_detect);
		}
		break;
	case 8:
		if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd8_present_detect) {
			iPrzVal = !gpio_get_value(generic_gpio.hdd_detect.hdd8_present_detect);
		}
		break;
	default:
		break;
	}

	return iPrzVal;
}

int SYNO_SOC_HDD_LED_SET(int index, int status)
{
	int ret = -1;
	int fail_led = 0;
	int present_led = 0;

	WARN_ON(GPIO_UNDEF == generic_gpio.soc_sata_led.hdd1_fail_led);

	/* assign pin info according to hdd */
	switch (index) {
	case 1:
		fail_led = generic_gpio.soc_sata_led.hdd1_fail_led;
		present_led = generic_gpio.hdd_detect.hdd1_present_detect;
		break;
	case 2:
		fail_led = generic_gpio.soc_sata_led.hdd2_fail_led;
		present_led = generic_gpio.hdd_detect.hdd2_present_detect;
		break;
	case 3:
		fail_led = generic_gpio.soc_sata_led.hdd3_fail_led;
		present_led = generic_gpio.hdd_detect.hdd3_present_detect;
		break;
	case 4:
		fail_led = generic_gpio.soc_sata_led.hdd4_fail_led;
		present_led = generic_gpio.hdd_detect.hdd4_present_detect;
		break;
	case 5:
		fail_led = generic_gpio.soc_sata_led.hdd5_fail_led;
		present_led = generic_gpio.hdd_detect.hdd5_present_detect;
		break;
	case 6:
		fail_led = generic_gpio.soc_sata_led.hdd6_fail_led;
		present_led = generic_gpio.hdd_detect.hdd6_present_detect;
		break;
	case 7:
		fail_led = generic_gpio.soc_sata_led.hdd7_fail_led;
		present_led = generic_gpio.hdd_detect.hdd7_present_detect;
		break;
	case 8:
		fail_led = generic_gpio.soc_sata_led.hdd8_fail_led;
		present_led = generic_gpio.hdd_detect.hdd8_present_detect;
		break;
	default:
		printk("Wrong HDD number [%d]\n", index);
		goto END;
	}

	/* Since faulty led and present led are combined,
	   we need to disable present led when light on faulty's */
	if (DISK_LED_ORANGE_SOLID == status || DISK_LED_ORANGE_BLINK == status) {
		gpio_set_value(fail_led, 1);
		gpio_set_value(present_led, 0);
	} else if ( DISK_LED_GREEN_SOLID == status || DISK_LED_GREEN_BLINK == status) {
		gpio_set_value(fail_led, 0);
		gpio_set_value(present_led, 1);
	} else if (DISK_LED_OFF == status) {
		gpio_set_value(fail_led, 0);
		gpio_set_value(present_led, 0);
	} else {
		printk("Wrong HDD led status [%d]\n", status);
		goto END;
	}

	ret = 0;
END:
	return ret;
}

/* SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER
 * Query support HDD dynamic Power .
 * output: 0 - support, 1 - not support.
 */
int SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER(void)
{
	int iRet = 0;

	/* if exist at least one hdd has enable pin and present detect pin ret=1 */
	if ((GPIO_UNDEF != generic_gpio.hdd_pm.hdd1_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd1_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd2_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd2_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd3_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd3_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd4_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd4_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd5_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd5_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd6_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd6_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd7_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd7_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd8_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd8_present_detect)) {

		iRet = 1;
	}
	return iRet;
}

EXPORT_SYMBOL(SYNO_ALPINE_GPIO_PIN);
EXPORT_SYMBOL(SYNO_CTRL_HDD_POWERON);
EXPORT_SYMBOL(SYNO_CTRL_FAN_PERSISTER);
EXPORT_SYMBOL(SYNO_CTRL_ALARM_LED_SET);
EXPORT_SYMBOL(SYNO_CHECK_HDD_PRESENT);
EXPORT_SYMBOL(SYNO_SOC_HDD_LED_SET);
EXPORT_SYMBOL(SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER);

/*
 *  DS2015xs GPIO config table
 *  (High=1)
 *
 *  Pin     In/Out    Function
 *   0       In       FAN 1
 *   1       In       FAN 2
 *   5      Out       High = Disk LED off
 *  18      Out       High = Enable ESATA 1 power
 *  19      Out       High = Enable ESATA 2 power
 *  10      Out       High = HDD 1 activity
 *  11      Out       High = HDD 2 activity
 *  22      Out       High = HDD 3 activity
 *  23      Out       High = HDD 4 activity
 *  24      Out       High = HDD 5 activity
 *  25      Out       High = HDD 6 activity
 *  26      Out       High = HDD 7 activity
 *  27      Out       High = HDD 8 activity
 *  30      Out       High = HDD 1 present
 *  31      Out       High = HDD 2 present
 *  32      Out       High = HDD 3 present
 *  33      Out       High = HDD 4 present
 *  34      Out       High = HDD 5 present
 *  35      Out       High = HDD 6 present
 *  36      Out       High = HDD 7 present
 *  37      Out       High = HDD 8 present
 *  38      Out       High = HDD 1 fault
 *  39      Out       High = HDD 2 fault
 *  40      Out       High = HDD 3 fault
 *  41      Out       High = HDD 4 fault
 *  42      Out       High = HDD 5 fault
 *   2      Out       High = HDD 6 fault
 *   3      Out       High = HDD 7 fault
 *   4      Out       High = HDD 8 fault
 *  43      Out       VTT off
 */

static void ALPINE_ds2015xs_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_ds2015xs = {
		.hdd_detect = {
			.hdd1_present_detect = 29,
			.hdd2_present_detect = 31,
			.hdd3_present_detect = 32,
			.hdd4_present_detect = 33,
			.hdd5_present_detect = 34,
			.hdd6_present_detect = 35,
			.hdd7_present_detect = 36,
			.hdd8_present_detect = 37,
		},
		.ext_sata_led = {
			.hdd1_led_0 = GPIO_UNDEF,
			.hdd1_led_1 = GPIO_UNDEF,
			.hdd2_led_0 = GPIO_UNDEF,
			.hdd2_led_1 = GPIO_UNDEF,
			.hdd3_led_0 = GPIO_UNDEF,
			.hdd3_led_1 = GPIO_UNDEF,
			.hdd4_led_0 = GPIO_UNDEF,
			.hdd4_led_1 = GPIO_UNDEF,
			.hdd5_led_0 = GPIO_UNDEF,
			.hdd5_led_1 = GPIO_UNDEF,
			.hdd_led_mask = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 38,
			.hdd2_fail_led = 39,
			.hdd3_fail_led = 40,
			.hdd4_fail_led = 41,
			.hdd5_fail_led = 42,
			.hdd6_fail_led = 2,
			.hdd7_fail_led = 3,
			.hdd8_fail_led = 4,
			.hdd1_act_led = 10,
			.hdd2_act_led = 11,
			.hdd3_act_led = 22,
			.hdd4_act_led = 23,
			.hdd5_act_led = 24,
			.hdd6_act_led = 25,
			.hdd7_act_led = 26,
			.hdd8_act_led = 27,
		},
		.model = {
			.model_id_0 = GPIO_UNDEF,
			.model_id_1 = GPIO_UNDEF,
			.model_id_2 = GPIO_UNDEF,
			.model_id_3 = GPIO_UNDEF,
		},
		.fan = {
			.fan_1 = GPIO_UNDEF,
			.fan_2 = GPIO_UNDEF,
			.fan_fail = 0,
			.fan_fail_2 = 1,
		},
		.hdd_pm = {
			.hdd1_pm = GPIO_UNDEF,
			.hdd2_pm = GPIO_UNDEF,
			.hdd3_pm = GPIO_UNDEF,
			.hdd4_pm = GPIO_UNDEF,
			.hdd5_pm = GPIO_UNDEF,
			.hdd6_pm = GPIO_UNDEF,
			.hdd7_pm = GPIO_UNDEF,
			.hdd8_pm = GPIO_UNDEF,
		},
		.rack = {
			.buzzer_mute_req = GPIO_UNDEF,
			.buzzer_mute_ack = GPIO_UNDEF,
			.rps1_on = GPIO_UNDEF,
			.rps2_on = GPIO_UNDEF,
		},
		.multi_bay = {
			.inter_lock = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = 18,
		},
	};

	*global_gpio = gpio_ds2015xs;
}

/*
 *  DS1515 GPIO config table
 *  (High=1)
 *
 *  Pin     In/Out    Function
 *   0       In       FAN 1
 *   1       In       FAN 2
 *   5      Out       High = Disk LED off
 *  18      Out       High = GPIO fan fail
 *  10      Out       High = HDD 1 activity
 *  11      Out       High = HDD 2 activity
 *  22      Out       High = HDD 3 activity
 *  23      Out       High = HDD 4 activity
 *  24      Out       High = HDD 5 activity
 *  29      Out       High = HDD 1 present
 *  31      Out       High = HDD 2 present
 *  32      Out       High = HDD 3 present
 *  33      Out       High = HDD 4 present
 *  34      Out       High = HDD 5 present
 *  38      Out       High = HDD 1 fault
 *  39      Out       High = HDD 2 fault
 *  40      Out       High = HDD 3 fault
 *  41      Out       High = HDD 4 fault
 *  42      Out       High = HDD 5 fault
 *  43      Out       VTT off
 */

static void ALPINE_ds1515_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_ds1515 = {
		.hdd_detect = {
			.hdd1_present_detect = 29,
			.hdd2_present_detect = 31,
			.hdd3_present_detect = 32,
			.hdd4_present_detect = 33,
			.hdd5_present_detect = 34,
			.hdd6_present_detect = GPIO_UNDEF,
			.hdd7_present_detect = GPIO_UNDEF,
			.hdd8_present_detect = GPIO_UNDEF,
		},
		.ext_sata_led = {
			.hdd1_led_0 = GPIO_UNDEF,
			.hdd1_led_1 = GPIO_UNDEF,
			.hdd2_led_0 = GPIO_UNDEF,
			.hdd2_led_1 = GPIO_UNDEF,
			.hdd3_led_0 = GPIO_UNDEF,
			.hdd3_led_1 = GPIO_UNDEF,
			.hdd4_led_0 = GPIO_UNDEF,
			.hdd4_led_1 = GPIO_UNDEF,
			.hdd5_led_0 = GPIO_UNDEF,
			.hdd5_led_1 = GPIO_UNDEF,
			.hdd_led_mask = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 38,
			.hdd2_fail_led = 39,
			.hdd3_fail_led = 40,
			.hdd4_fail_led = 41,
			.hdd5_fail_led = 42,
			.hdd6_fail_led = GPIO_UNDEF,
			.hdd7_fail_led = GPIO_UNDEF,
			.hdd8_fail_led = GPIO_UNDEF,
			.hdd1_act_led = 10,
			.hdd2_act_led = 11,
			.hdd3_act_led = 22,
			.hdd4_act_led = 23,
			.hdd5_act_led = 24,
			.hdd6_act_led = GPIO_UNDEF,
			.hdd7_act_led = GPIO_UNDEF,
			.hdd8_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = GPIO_UNDEF,
			.model_id_1 = GPIO_UNDEF,
			.model_id_2 = GPIO_UNDEF,
			.model_id_3 = GPIO_UNDEF,
		},
		.fan = {
			.fan_1 = GPIO_UNDEF,
			.fan_2 = GPIO_UNDEF,
			.fan_fail = 0,
			.fan_fail_2 = 1,
		},
		.hdd_pm = {
			.hdd1_pm = GPIO_UNDEF,
			.hdd2_pm = GPIO_UNDEF,
			.hdd3_pm = GPIO_UNDEF,
			.hdd4_pm = GPIO_UNDEF,
			.hdd5_pm = GPIO_UNDEF,
			.hdd6_pm = GPIO_UNDEF,
			.hdd7_pm = GPIO_UNDEF,
			.hdd8_pm = GPIO_UNDEF,
		},
		.rack = {
			.buzzer_mute_req = GPIO_UNDEF,
			.buzzer_mute_ack = GPIO_UNDEF,
			.rps1_on = GPIO_UNDEF,
			.rps2_on = GPIO_UNDEF,
		},
		.multi_bay = {
			.inter_lock = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = 18,
		},
	};

	*global_gpio = gpio_ds1515;
}

/*
 *  DS715+/DS215+ GPIO config table
 *  (High=1)
 *
 *  Pin     In/Out    Function
 *   0       In       FAN 1
 *   2       In       HDD_PRZ_1 (Low = HDD insert; High = plug out)
 *   3       In       HDD_PRZ_1 (Low = HDD insert; High = plug out)
 *   5      Out       High = SATA LED off
 *  34      Out       High = LAN LED on
 *  19      Out       High = Enable ESATA 1 power
 *  22      Out       High = Enable HDD 1 power (for deep sleep)
 *  23      Out       High = Enable HDD 1 power (for deep sleep)
 *  10      Out       High = HDD 1 activity LED
 *  11      Out       High = HDD 2 activity LED
 *  29      Out       High = HDD 1 present LED
 *  31      Out       High = HDD 2 present LED
 *  38      Out       High = HDD 1 faulty LED
 *  39      Out       High = HDD 2 faulty LED
 *  43      Out       VTT off
 */

static void ALPINE_2bay_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_2bay = {
		.hdd_detect = {
			.hdd1_present_detect = 29,
			.hdd2_present_detect = 31,
			.hdd3_present_detect = GPIO_UNDEF,
			.hdd4_present_detect = GPIO_UNDEF,
			.hdd5_present_detect = GPIO_UNDEF,
			.hdd6_present_detect = GPIO_UNDEF,
			.hdd7_present_detect = GPIO_UNDEF,
			.hdd8_present_detect = GPIO_UNDEF,
		},
		.ext_sata_led = {
			.hdd1_led_0 = GPIO_UNDEF,
			.hdd1_led_1 = GPIO_UNDEF,
			.hdd2_led_0 = GPIO_UNDEF,
			.hdd2_led_1 = GPIO_UNDEF,
			.hdd3_led_0 = GPIO_UNDEF,
			.hdd3_led_1 = GPIO_UNDEF,
			.hdd4_led_0 = GPIO_UNDEF,
			.hdd4_led_1 = GPIO_UNDEF,
			.hdd5_led_0 = GPIO_UNDEF,
			.hdd5_led_1 = GPIO_UNDEF,
			.hdd_led_mask = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 38,
			.hdd2_fail_led = 39,
			.hdd3_fail_led = GPIO_UNDEF,
			.hdd4_fail_led = GPIO_UNDEF,
			.hdd5_fail_led = GPIO_UNDEF,
			.hdd6_fail_led = GPIO_UNDEF,
			.hdd7_fail_led = GPIO_UNDEF,
			.hdd8_fail_led = GPIO_UNDEF,
			.hdd1_act_led = 10,
			.hdd2_act_led = 11,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
			.hdd5_act_led = GPIO_UNDEF,
			.hdd6_act_led = GPIO_UNDEF,
			.hdd7_act_led = GPIO_UNDEF,
			.hdd8_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = GPIO_UNDEF,
			.model_id_1 = GPIO_UNDEF,
			.model_id_2 = GPIO_UNDEF,
			.model_id_3 = GPIO_UNDEF,
		},
		.fan = {
			.fan_1 = GPIO_UNDEF,
			.fan_2 = GPIO_UNDEF,
			.fan_fail = 0,
			.fan_fail_2 = GPIO_UNDEF,
		},
		.hdd_pm = {
			.hdd1_pm = 22,
			.hdd2_pm = 23,
			.hdd3_pm = GPIO_UNDEF,
			.hdd4_pm = GPIO_UNDEF,
			.hdd5_pm = GPIO_UNDEF,
			.hdd6_pm = GPIO_UNDEF,
			.hdd7_pm = GPIO_UNDEF,
			.hdd8_pm = GPIO_UNDEF,
		},
		.rack = {
			.buzzer_mute_req = GPIO_UNDEF,
			.buzzer_mute_ack = GPIO_UNDEF,
			.rps1_on = GPIO_UNDEF,
			.rps2_on = GPIO_UNDEF,
		},
		.multi_bay = {
			.inter_lock = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
	};

	*global_gpio = gpio_2bay;
}

/*
 *  DS416 GPIO config table
 *  (High=1)
 *
 *  Pin     In/Out    Function
 *   0       In       FAN 1
 *   1       In       FAN 2
 *   2       In       HDD_PRZ_1 (Low = HDD insert; High = plug out)
 *   3       In       HDD_PRZ_2 (Low = HDD insert; High = plug out)
 *  35       In       HDD_PRZ_3 (Low = HDD insert; High = plug out)
 *  36       In       HDD_PRZ_4 (Low = HDD insert; High = plug out)
 *   5      Out       High = Disk LED off
 *  19      Out       High = Enable ESATA 1 power
 *  10      Out       High = HDD 1 activity
 *  11      Out       High = HDD 2 activity
 *  22      Out       High = HDD 3 activity
 *  23      Out       High = HDD 4 activity
 *  24      Out       High = Enable HDD 1 power (for deep sleep)
 *  25      Out       High = Enable HDD 2 power (for deep sleep)
 *  26      Out       High = Enable HDD 3 power (for deep sleep)
 *  27      Out       High = Enable HDD 4 power (for deep sleep)
 *  29      Out       High = HDD 1 present
 *  31      Out       High = HDD 2 present
 *  32      Out       High = HDD 3 present
 *  33      Out       High = HDD 4 present
 *  38      Out       High = HDD 1 fault
 *  39      Out       High = HDD 2 fault
 *  40      Out       High = HDD 3 fault
 *  41      Out       High = HDD 4 fault
 *  43      Out       VTT off
 */

static void ALPINE_ds416_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_ds416 = {
		.hdd_detect = {
			.hdd1_present_detect = 29,
			.hdd2_present_detect = 31,
			.hdd3_present_detect = 32,
			.hdd4_present_detect = 33,
			.hdd5_present_detect = GPIO_UNDEF,
			.hdd6_present_detect = GPIO_UNDEF,
			.hdd7_present_detect = GPIO_UNDEF,
			.hdd8_present_detect = GPIO_UNDEF,
		},
		.ext_sata_led = {
			.hdd1_led_0 = GPIO_UNDEF,
			.hdd1_led_1 = GPIO_UNDEF,
			.hdd2_led_0 = GPIO_UNDEF,
			.hdd2_led_1 = GPIO_UNDEF,
			.hdd3_led_0 = GPIO_UNDEF,
			.hdd3_led_1 = GPIO_UNDEF,
			.hdd4_led_0 = GPIO_UNDEF,
			.hdd4_led_1 = GPIO_UNDEF,
			.hdd5_led_0 = GPIO_UNDEF,
			.hdd5_led_1 = GPIO_UNDEF,
			.hdd_led_mask = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 38,
			.hdd2_fail_led = 39,
			.hdd3_fail_led = 40,
			.hdd4_fail_led = 41,
			.hdd5_fail_led = GPIO_UNDEF,
			.hdd6_fail_led = GPIO_UNDEF,
			.hdd7_fail_led = GPIO_UNDEF,
			.hdd8_fail_led = GPIO_UNDEF,
			.hdd1_act_led = 10,
			.hdd2_act_led = 11,
			.hdd3_act_led = 22,
			.hdd4_act_led = 23,
			.hdd5_act_led = GPIO_UNDEF,
			.hdd6_act_led = GPIO_UNDEF,
			.hdd7_act_led = GPIO_UNDEF,
			.hdd8_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = GPIO_UNDEF,
			.model_id_1 = GPIO_UNDEF,
			.model_id_2 = GPIO_UNDEF,
			.model_id_3 = GPIO_UNDEF,
		},
		.fan = {
			.fan_1 = GPIO_UNDEF,
			.fan_2 = GPIO_UNDEF,
			.fan_fail = 0,
			.fan_fail_2 = 1,
		},
		.hdd_pm = {
			.hdd1_pm = 24,
			.hdd2_pm = 25,
			.hdd3_pm = 26,
			.hdd4_pm = 27,
			.hdd5_pm = GPIO_UNDEF,
			.hdd6_pm = GPIO_UNDEF,
			.hdd7_pm = GPIO_UNDEF,
			.hdd8_pm = GPIO_UNDEF,
		},
		.rack = {
			.buzzer_mute_req = GPIO_UNDEF,
			.buzzer_mute_ack = GPIO_UNDEF,
			.rps1_on = GPIO_UNDEF,
			.rps2_on = GPIO_UNDEF,
		},
		.multi_bay = {
			.inter_lock = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
	};

	*global_gpio = gpio_ds416;
}

static void ALPINE_default_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_default = {
		.hdd_detect = {
			.hdd1_present_detect = GPIO_UNDEF,
			.hdd2_present_detect = GPIO_UNDEF,
			.hdd3_present_detect = GPIO_UNDEF,
			.hdd4_present_detect = GPIO_UNDEF,
			.hdd5_present_detect = GPIO_UNDEF,
			.hdd6_present_detect = GPIO_UNDEF,
			.hdd7_present_detect = GPIO_UNDEF,
			.hdd8_present_detect = GPIO_UNDEF,
		},
		.ext_sata_led = {
			.hdd1_led_0 = GPIO_UNDEF,
			.hdd1_led_1 = GPIO_UNDEF,
			.hdd2_led_0 = GPIO_UNDEF,
			.hdd2_led_1 = GPIO_UNDEF,
			.hdd3_led_0 = GPIO_UNDEF,
			.hdd3_led_1 = GPIO_UNDEF,
			.hdd4_led_0 = GPIO_UNDEF,
			.hdd4_led_1 = GPIO_UNDEF,
			.hdd5_led_0 = GPIO_UNDEF,
			.hdd5_led_1 = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = GPIO_UNDEF,
			.hdd2_fail_led = GPIO_UNDEF,
			.hdd3_fail_led = GPIO_UNDEF,
			.hdd4_fail_led = GPIO_UNDEF,
			.hdd5_fail_led = GPIO_UNDEF,
			.hdd6_fail_led = GPIO_UNDEF,
			.hdd7_fail_led = GPIO_UNDEF,
			.hdd8_fail_led = GPIO_UNDEF,
			.hdd1_act_led = GPIO_UNDEF,
			.hdd2_act_led = GPIO_UNDEF,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
			.hdd5_act_led = GPIO_UNDEF,
			.hdd6_act_led = GPIO_UNDEF,
			.hdd7_act_led = GPIO_UNDEF,
			.hdd8_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = GPIO_UNDEF,
			.model_id_1 = GPIO_UNDEF,
			.model_id_2 = GPIO_UNDEF,
			.model_id_3 = GPIO_UNDEF,
		},
		.fan = {
			.fan_1 = GPIO_UNDEF,
			.fan_2 = GPIO_UNDEF,
			.fan_fail = GPIO_UNDEF,
			.fan_fail_2 = GPIO_UNDEF,
		},
		.hdd_pm = {
			.hdd1_pm = GPIO_UNDEF,
			.hdd2_pm = GPIO_UNDEF,
			.hdd3_pm = GPIO_UNDEF,
			.hdd4_pm = GPIO_UNDEF,
			.hdd5_pm = GPIO_UNDEF,
			.hdd6_pm = GPIO_UNDEF,
			.hdd7_pm = GPIO_UNDEF,
			.hdd8_pm = GPIO_UNDEF,
		},
		.rack = {
			.buzzer_mute_req = GPIO_UNDEF,
			.buzzer_mute_ack = GPIO_UNDEF,
			.rps1_on = GPIO_UNDEF,
			.rps2_on = GPIO_UNDEF,
		},
		.multi_bay = {
			.inter_lock = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
	};

	*global_gpio = gpio_default;
}

void synology_gpio_init(void)
{
	if (0 == strncmp(gszSynoHWVersion, HW_DS2015xs, strlen(HW_DS2015xs))) {
		ALPINE_ds2015xs_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS2015xs);
	} else if (0 == strncmp(gszSynoHWVersion, HW_DS1515, strlen(HW_DS1515))) {
		ALPINE_ds1515_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS1515);
	} else if (0 == strncmp(gszSynoHWVersion, HW_DS715p, strlen(HW_DS715p))) {
		ALPINE_2bay_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS715p);
	} else if (0 == strncmp(gszSynoHWVersion, HW_DS215p, strlen(HW_DS215p))) {
		ALPINE_2bay_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS215p);
	} else if (0 == strncmp(gszSynoHWVersion, HW_DS416, strlen(HW_DS416))) {
		ALPINE_ds416_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS416);
	} else {
		ALPINE_default_GPIO_init(&generic_gpio);
		printk("Not supported hw version!\n");
	}
}
