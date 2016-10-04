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
#include <linux/gpio.h>
#include <linux/synobios.h>

#ifndef HW_DS416j
#define HW_DS416j "DS416j"
#endif

#ifndef HW_DS216
#define HW_DS216 "DS216"
#endif

#ifndef HW_DS416slim
#define HW_DS416slim "DS416slim"
#endif

#ifndef HW_DS216j
#define HW_DS216j "DS216j"
#endif

#ifndef HW_RS816
#define HW_RS816 "RS816"
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
} SYNO_HDD_DETECT_GPIO;

typedef struct __tag_SYNO_HDD_PM_GPIO {
	u8 hdd1_pm;
	u8 hdd2_pm;
	u8 hdd3_pm;
	u8 hdd4_pm;
} SYNO_HDD_PM_GPIO;

typedef struct __tag_SYNO_FAN_GPIO {
	u8 fan_1;
	u8 fan_2;
	u8 fan_3;
	u8 fan_fail_1;
	u8 fan_fail_2;
	u8 fan_fail_3;
} SYNO_FAN_GPIO;

typedef struct __tag_SYNO_MODEL_GPIO {
	u8 model_id_0;
	u8 model_id_1;
	u8 model_id_2;
} SYNO_MODEL_GPIO;

typedef struct __tag_SYNO_SOC_HDD_LED_GPIO {
	u8 hdd1_fail_led;
	u8 hdd2_fail_led;
	u8 hdd3_fail_led;
	u8 hdd4_fail_led;
	u8 hdd1_act_led;
	u8 hdd2_act_led;
	u8 hdd3_act_led;
	u8 hdd4_act_led;
} SYNO_SOC_HDD_LED_GPIO;

typedef struct __tag_SYNO_STATUS_LED_GPIO {
	u8 alarm_led;
	u8 power_led;
} SYNO_STATUS_LED_GPIO;

typedef struct __tag_SYNO_GPIO {
	SYNO_HDD_DETECT_GPIO    hdd_detect;
	SYNO_SOC_HDD_LED_GPIO   soc_sata_led;
	SYNO_MODEL_GPIO         model;
	SYNO_FAN_GPIO           fan;
	SYNO_HDD_PM_GPIO        hdd_pm;
	SYNO_STATUS_LED_GPIO    status;
	u8                      copy_button_gpio;
} SYNO_GPIO;

static SYNO_GPIO generic_gpio;

unsigned int SynoModelIDGet(SYNO_GPIO *pGpio)
{
	return (((gpio_get_value(pGpio->model.model_id_0) ? 1 : 0) << 2) |
			((gpio_get_value(pGpio->model.model_id_1) ? 1 : 0) << 1) |
			((gpio_get_value(pGpio->model.model_id_2) ? 1 : 0) << 0));
}

int SYNO_ARMADA_GPIO_PIN(int pin, int *pValue, int isWrite)
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
	case 3:
		pin = generic_gpio.fan.fan_3;
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

int SYNO_CTRL_FAN_STATUS_GET(int index, int *pValue)
{
	int failed = 0;

	switch (index) {
	case 1:
		WARN_ON(GPIO_UNDEF == generic_gpio.fan.fan_fail_1);
		failed = gpio_get_value(generic_gpio.fan.fan_fail_1);
		break;
	case 2:
		WARN_ON(GPIO_UNDEF == generic_gpio.fan.fan_fail_2);
		failed = gpio_get_value(generic_gpio.fan.fan_fail_2);
		break;
	case 3:
		WARN_ON(GPIO_UNDEF == generic_gpio.fan.fan_fail_3);
		failed = gpio_get_value(generic_gpio.fan.fan_fail_3);
		break;
	default:
		WARN_ON(1);
		break;
	}

	if (failed)
		*pValue = 0;
	else
		*pValue = 1;

	return 0;
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
	default:
		break;
	}

	return iPrzVal;
}

int SYNO_SOC_HDD_LED_SET(int index, int status)
{
	int ret = -1;
	int fail_led = 0;

	WARN_ON(GPIO_UNDEF == generic_gpio.soc_sata_led.hdd1_fail_led);

	/* assign pin info according to hdd */
	switch (index) {
	case 1:
		fail_led = generic_gpio.soc_sata_led.hdd1_fail_led;
		break;
	case 2:
		fail_led = generic_gpio.soc_sata_led.hdd2_fail_led;
		break;
	case 3:
		fail_led = generic_gpio.soc_sata_led.hdd3_fail_led;
		break;
	case 4:
		fail_led = generic_gpio.soc_sata_led.hdd4_fail_led;
		break;
	default:
		printk("Wrong HDD number [%d]\n", index);
		goto END;
	}

	/* Since faulty led and present led are combined,
	   we need to disable present led when light on faulty's */
	if (DISK_LED_ORANGE_SOLID == status || DISK_LED_ORANGE_BLINK == status) {
		gpio_set_value(fail_led, 1);
	} else if (DISK_LED_GREEN_SOLID == status || DISK_LED_GREEN_BLINK == status) {
		gpio_set_value(fail_led, 0);
	} else if (DISK_LED_OFF == status) {
		gpio_set_value(fail_led, 0);
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
 * output: 1 - support, 0 - not support.
 */
int SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER(void)
{
	int iRet = 0;

	if (syno_is_hw_version(HW_DS216j))
		return 1;

	/* if exist at least one hdd has enable pin and present detect pin ret=1 */
	if ((GPIO_UNDEF != generic_gpio.hdd_pm.hdd1_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd1_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd2_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd2_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd3_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd3_present_detect) ||
			(GPIO_UNDEF != generic_gpio.hdd_pm.hdd4_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd4_present_detect)) {
		iRet = 1;
	}
	return iRet;
}

int SYNO_COPY_BUTTON_GPIO_GET(void)
{
	WARN_ON(GPIO_UNDEF == generic_gpio.copy_button_gpio);
	// for matching userspace usage, return 0 if button is pressed, else = 1
	return gpio_get_value(generic_gpio.copy_button_gpio);
}

EXPORT_SYMBOL(SYNO_ARMADA_GPIO_PIN);
EXPORT_SYMBOL(SYNO_CTRL_HDD_POWERON);
EXPORT_SYMBOL(SYNO_CTRL_FAN_PERSISTER);
EXPORT_SYMBOL(SYNO_CTRL_FAN_STATUS_GET);
EXPORT_SYMBOL(SYNO_CTRL_ALARM_LED_SET);
EXPORT_SYMBOL(SYNO_CHECK_HDD_PRESENT);
EXPORT_SYMBOL(SYNO_SOC_HDD_LED_SET);
EXPORT_SYMBOL(SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER);
EXPORT_SYMBOL(SYNO_COPY_BUTTON_GPIO_GET);// for matching userspace usage, return 0 if button is pressed, else = 1

/*
 *  DS416j GPIO config table
 *
 *  Pin     In/Out    Function

 *  12       In       Model ID 0
 *  21       In       Model ID 1
 *  45       In       Model ID 2
 *  39       In       HDD 1 present
 *  40       In       HDD 2 present
 *  41       In       HDD 3 present
 *  43       In       HDD 4 present
 *  52       In       Fan 1 fail
 *  53       In       Fan 2 fail
 *  54       In       USB3 overcurrent
 *  55       In       USB2 overcurrent
 *   6      Out       LED on
 *  13      Out       HDD 1 fault LED
 *  14      Out       HDD 2 fault LED
 *  15      Out       HDD 3 fault LED
 *  16      Out       HDD 4 fault LED
 *  26      Out       HDD 1 power enable
 *  27      Out       HDD 2 power enable
 *  37      Out       HDD 3 power enable
 *  38      Out       HDD 4 power enable
 *  48      Out       Fan Low
 *  49      Out       Fan Mid
 *  50      Out       Fan High
 *  58      Out       USB3 power enable
 *  59      Out       USB2 power enable
 */

static void ARMADA_ds416j_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_ds416j = {
		.hdd_detect = {
			.hdd1_present_detect = 39,
			.hdd2_present_detect = 40,
			.hdd3_present_detect = 41,
			.hdd4_present_detect = 43,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 13,
			.hdd2_fail_led = 14,
			.hdd3_fail_led = 15,
			.hdd4_fail_led = 16,
			.hdd1_act_led = GPIO_UNDEF,
			.hdd2_act_led = GPIO_UNDEF,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = 12,
			.model_id_1 = 21,
			.model_id_2 = 45,
		},
		.fan = {
			.fan_1 = 50,
			.fan_2 = 49,
			.fan_3 = 48,
			.fan_fail_1 = 52,
			.fan_fail_2 = 53,
			.fan_fail_3 = GPIO_UNDEF,
		},
		.hdd_pm = {
			.hdd1_pm = 26,
			.hdd2_pm = 27,
			.hdd3_pm = 37,
			.hdd4_pm = 38,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
		.copy_button_gpio = GPIO_UNDEF,
	};

	*global_gpio = gpio_ds416j;
}

/*
 *  DS216 GPIO config table
 *
 *  Pin     In/Out    Function

 *  12       In       Model ID 0
 *  21       In       Model ID 1
 *  45       In       Model ID 2
 *  39       In       HDD 1 present
 *  40       In       HDD 2 present
 *  41       In       USB3 overcurrent
 *  42       In       USB copy button press intterupt
 *  43       In       USB2 overcurrent
 *  46       In       SD card insert interrupt
 *   6      Out       LED on
 *  13      Out       HDD 1 fault LED
 *  14      Out       HDD 2 fault LED
 *  26      Out       HDD 1 power enable
 *  27      Out       HDD 2 power enable
 *  15      Out       Fan High
 *  37      Out       Fan Mid
 *  38      Out       Fan Low
 *  44      Out       USB3 power enable
 *  47      Out       USB2 power enable
 */

static void ARMADA_ds216_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_ds216 = {
		.hdd_detect = {
			.hdd1_present_detect = 39,
			.hdd2_present_detect = 40,
			.hdd3_present_detect = GPIO_UNDEF,
			.hdd4_present_detect = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 13,
			.hdd2_fail_led = 14,
			.hdd3_fail_led = GPIO_UNDEF,
			.hdd4_fail_led = GPIO_UNDEF,
			.hdd1_act_led = GPIO_UNDEF,
			.hdd2_act_led = GPIO_UNDEF,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = 12,
			.model_id_1 = 21,
			.model_id_2 = 45,
		},
		.fan = {
			.fan_1 = 38,
			.fan_2 = 37,
			.fan_3 = 15,
			.fan_fail_1 = GPIO_UNDEF,
			.fan_fail_2 = GPIO_UNDEF,
			.fan_fail_3 = GPIO_UNDEF,
		},
		.hdd_pm = {
			.hdd1_pm = 26,
			.hdd2_pm = 27,
			.hdd3_pm = GPIO_UNDEF,
			.hdd4_pm = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
		.copy_button_gpio = 6, // for matching userspace usage, return 0 if button is pressed, else = 1
	};

	*global_gpio = gpio_ds216;
}

/*
 *  DS416slim GPIO config table
 *
 *  Pin     In/Out    Function

 *  12       In       Model ID 0
 *  21       In       Model ID 1
 *  45       In       Model ID 2
 *  39       In       HDD 1 present
 *  40       In       HDD 2 present
 *  41       In       HDD 3 present
 *  43       In       HDD 4 present
 *  52       In       Fan 1 fail
 *  54       In       USB3 overcurrent
 *  55       In       USB2 overcurrent
 *   6      Out       LED on
 *  26      Out       HDD 1 power enable
 *  27      Out       HDD 2 power enable
 *  37      Out       HDD 3 power enable
 *  38      Out       HDD 4 power enable
 *  48      Out       Fan High
 *  49      Out       Fan Mid
 *  50      Out       Fan Low
 *  58      Out       USB3 power enable
 *  59      Out       USB2 power enable
 */

static void ARMADA_ds416slim_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_ds416slim = {
		.hdd_detect = {
			.hdd1_present_detect = 39,
			.hdd2_present_detect = 40,
			.hdd3_present_detect = 41,
			.hdd4_present_detect = 43,
		},
		.soc_sata_led = {
			.hdd1_fail_led = GPIO_UNDEF,
			.hdd2_fail_led = GPIO_UNDEF,
			.hdd3_fail_led = GPIO_UNDEF,
			.hdd4_fail_led = GPIO_UNDEF,
			.hdd1_act_led = GPIO_UNDEF,
			.hdd2_act_led = GPIO_UNDEF,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = 12,
			.model_id_1 = 21,
			.model_id_2 = 45,
		},
		.fan = {
			.fan_1 = 50,
			.fan_2 = 49,
			.fan_3 = 48,
			.fan_fail_1 = 52,
			.fan_fail_2 = GPIO_UNDEF,
			.fan_fail_3 = GPIO_UNDEF,
		},
		.hdd_pm = {
			.hdd1_pm = 26,
			.hdd2_pm = 27,
			.hdd3_pm = 37,
			.hdd4_pm = 38,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
		.copy_button_gpio = GPIO_UNDEF,
	};

	*global_gpio = gpio_ds416slim;
}

/*
 *  DS216j GPIO config table
 *
 *  Pin     In/Out    Function

 *  12       In       Model ID 0
 *  21       In       Model ID 1
 *  45       In       Model ID 2
 *  52       In       Fan 1 fail
 *  54       In       USB1 overcurrent
 *  55       In       USB2 overcurrent
 *   6      Out       LED on
 *  13      Out       HDD 1 fault LED
 *  14      Out       HDD 2 fault LED
 *  15      Out       HDD 1 power enable
 *  16      Out       HDD 2 power enable
 *  48      Out       Fan High
 *  49      Out       Fan Mid
 *  50      Out       Fan Low
 *  58      Out       USB1 power enable
 *  59      Out       USB2 power enable
 */

static void ARMADA_ds216j_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_ds216j = {
		.hdd_detect = {
			.hdd1_present_detect = GPIO_UNDEF,
			.hdd2_present_detect = GPIO_UNDEF,
			.hdd3_present_detect = GPIO_UNDEF,
			.hdd4_present_detect = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 13,
			.hdd2_fail_led = 14,
			.hdd3_fail_led = GPIO_UNDEF,
			.hdd4_fail_led = GPIO_UNDEF,
			.hdd1_act_led = GPIO_UNDEF,
			.hdd2_act_led = GPIO_UNDEF,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = 12,
			.model_id_1 = 21,
			.model_id_2 = 45,
		},
		.fan = {
			.fan_1 = 50,
			.fan_2 = 49,
			.fan_3 = 48,
			.fan_fail_1 = 52,
			.fan_fail_2 = GPIO_UNDEF,
			.fan_fail_3 = GPIO_UNDEF,
		},
		.hdd_pm = {
			.hdd1_pm = 15,
			.hdd2_pm = 16,
			.hdd3_pm = GPIO_UNDEF,
			.hdd4_pm = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
	};

	*global_gpio = gpio_ds216j;
}

/*
 *  RS816 GPIO config table
 *
 *  Pin     In/Out    Function

 *  12       In       Model ID 0
 *  21       In       Model ID 1
 *  38       In       Buzzer Mute GPI
 *  39       In       Present Pin
 *  45       In       Model ID 2
 *  46       In       Fan 2 fail
 *  52       In       Fan 1 fail
 *  53       In       Fan 3 fail
 *  54       In       USB3 overcurrent
 *  55       In       USB2 overcurrent
 *   6      Out       LED off
 *  13      Out       HDD1 fault LED
 *  28      Out       Buzzer Mute GPO
 *  37      Out       HDD 3 power enable
 *  37      Out       HDD 4 power enable
 *  48      Out       Fan High
 *  49      Out       Fan Mid
 *  50      Out       Fan Low
 *  58      Out       USB3 power enable
 *  59      Out       USB3 power enable
 */

static void ARMADA_rs816_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_rs816 = {
		.hdd_detect = {
			.hdd1_present_detect = 39,
			.hdd2_present_detect = 39,
			.hdd3_present_detect = 39,
			.hdd4_present_detect = 39,
		},
		.soc_sata_led = {
			.hdd1_fail_led = 13,
			.hdd2_fail_led = GPIO_UNDEF,
			.hdd3_fail_led = GPIO_UNDEF,
			.hdd4_fail_led = GPIO_UNDEF,
			.hdd1_act_led = GPIO_UNDEF,
			.hdd2_act_led = GPIO_UNDEF,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = 12,
			.model_id_1 = 21,
			.model_id_2 = 45,
		},
		.fan = {
			.fan_1 = 48,
			.fan_2 = 49,
			.fan_3 = 50,
			.fan_fail_1 = 52,
			.fan_fail_2 = 46,
			.fan_fail_3 = 53,
		},
		.hdd_pm = {
			.hdd1_pm = GPIO_UNDEF,
			.hdd2_pm = GPIO_UNDEF,
			.hdd3_pm = 37,
			.hdd4_pm = 37,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
		.copy_button_gpio = GPIO_UNDEF,
	};

	*global_gpio = gpio_rs816;
}

static void ARMADA38X_default_GPIO_init(SYNO_GPIO *global_gpio)
{
	SYNO_GPIO gpio_default = {
		.hdd_detect = {
			.hdd1_present_detect = GPIO_UNDEF,
			.hdd2_present_detect = GPIO_UNDEF,
			.hdd3_present_detect = GPIO_UNDEF,
			.hdd4_present_detect = GPIO_UNDEF,
		},
		.soc_sata_led = {
			.hdd1_fail_led = GPIO_UNDEF,
			.hdd2_fail_led = GPIO_UNDEF,
			.hdd3_fail_led = GPIO_UNDEF,
			.hdd4_fail_led = GPIO_UNDEF,
			.hdd1_act_led = GPIO_UNDEF,
			.hdd2_act_led = GPIO_UNDEF,
			.hdd3_act_led = GPIO_UNDEF,
			.hdd4_act_led = GPIO_UNDEF,
		},
		.model = {
			.model_id_0 = GPIO_UNDEF,
			.model_id_1 = GPIO_UNDEF,
			.model_id_2 = GPIO_UNDEF,
		},
		.fan = {
			.fan_1 = GPIO_UNDEF,
			.fan_2 = GPIO_UNDEF,
			.fan_3 = GPIO_UNDEF,
			.fan_fail_1 = GPIO_UNDEF,
			.fan_fail_2 = GPIO_UNDEF,
			.fan_fail_3 = GPIO_UNDEF,
		},
		.hdd_pm = {
			.hdd1_pm = GPIO_UNDEF,
			.hdd2_pm = GPIO_UNDEF,
			.hdd3_pm = GPIO_UNDEF,
			.hdd4_pm = GPIO_UNDEF,
		},
		.status = {
			.power_led = GPIO_UNDEF,
			.alarm_led = GPIO_UNDEF,
		},
		.copy_button_gpio = GPIO_UNDEF,
	};

	*global_gpio = gpio_default;
}

void synology_gpio_init(void)
{
	if (syno_is_hw_version(HW_DS416j)) {
		ARMADA_ds416j_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS416j);
	} else if (syno_is_hw_version(HW_DS216j)) {
		ARMADA_ds216j_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS216j);
	} else if (syno_is_hw_version(HW_DS216)) {
		ARMADA_ds216_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS216);
	} else if (syno_is_hw_version(HW_DS416slim)) {
		ARMADA_ds416slim_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_DS416slim);
	} else if (syno_is_hw_version(HW_RS816)) {
		ARMADA_rs816_GPIO_init(&generic_gpio);
		printk("Synology %s GPIO Init\n", HW_RS816);
	} else {
		ARMADA38X_default_GPIO_init(&generic_gpio);
		printk("Not supported hw version!\n");
	}
}
