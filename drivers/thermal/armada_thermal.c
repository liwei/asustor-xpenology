/*
 * Marvell Armada 370/XP thermal sensor driver
 *
 * Copyright (C) 2013 Marvell
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/thermal.h>

#if defined(CONFIG_SYNO_LSP_ARMADA)
#define THERMAL_VALID_MASK		0x1
#else /* CONFIG_SYNO_LSP_ARMADA */
#define THERMAL_VALID_OFFSET		9
#define THERMAL_VALID_MASK		0x1
#define THERMAL_TEMP_OFFSET		10
#define THERMAL_TEMP_MASK		0x1ff
#endif /* CONFIG_SYNO_LSP_ARMADA */

/* Thermal Manager Control and Status Register */
#define PMU_TDC0_SW_RST_MASK		(0x1 << 1)
#define PMU_TM_DISABLE_OFFS		0
#define PMU_TM_DISABLE_MASK		(0x1 << PMU_TM_DISABLE_OFFS)
#define PMU_TDC0_REF_CAL_CNT_OFFS	11
#define PMU_TDC0_REF_CAL_CNT_MASK	(0x1ff << PMU_TDC0_REF_CAL_CNT_OFFS)
#define PMU_TDC0_OTF_CAL_MASK		(0x1 << 30)
#define PMU_TDC0_START_CAL_MASK		(0x1 << 25)

#if defined(CONFIG_SYNO_LSP_ARMADA)
#define A375_Z1_CAL_RESET_LSB		0x8011e214
#define A375_Z1_CAL_RESET_MSB		0x30a88019
#define A375_Z1_WORKAROUND_BIT		BIT(9)

#define A375_UNIT_CONTROL_SHIFT		27
#define A375_UNIT_CONTROL_MASK		0x7
#define A375_READOUT_INVERT		BIT(15)
#define A375_HW_RESETn			BIT(8)
#define A380_HW_RESET			BIT(8)

struct armada_thermal_data;
#else /* CONFIG_SYNO_LSP_ARMADA */
struct armada_thermal_ops;
#endif /* CONFIG_SYNO_LSP_ARMADA */

/* Marvell EBU Thermal Sensor Dev Structure */
struct armada_thermal_priv {
	void __iomem *sensor;
	void __iomem *control;
#if defined(CONFIG_SYNO_LSP_ARMADA)
	struct armada_thermal_data *data;
#else /* CONFIG_SYNO_LSP_ARMADA */
	struct armada_thermal_ops *ops;
#endif /* CONFIG_SYNO_LSP_ARMADA */
};

#if defined(CONFIG_SYNO_LSP_ARMADA)
struct armada_thermal_data {
	/* Initialize the sensor */
	void (*init_sensor)(struct platform_device *pdev,
			    struct armada_thermal_priv *);

	/* Test for a valid sensor value (optional) */
	bool (*is_valid)(struct armada_thermal_priv *);

	/* Formula coeficients: temp = (b + m * reg) / div */
	unsigned long coef_b;
	unsigned long coef_m;
	unsigned long coef_div;
	bool inverted;

	/* Register shift and mask to access the sensor temperature */
	unsigned int temp_shift;
	unsigned int temp_mask;
	unsigned int is_valid_shift;
};
#else /* CONFIG_SYNO_LSP_ARMADA */
struct armada_thermal_ops {
	/* Initialize the sensor */
	void (*init_sensor)(struct armada_thermal_priv *);

	/* Test for a valid sensor value (optional) */
	bool (*is_valid)(struct armada_thermal_priv *);
};
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if defined(CONFIG_SYNO_ARMADA)
static struct thermal_zone_device* g_syno_tz = NULL;
static int armada_get_temp(struct thermal_zone_device *thermal,
			  unsigned long *temp);

unsigned long int syno_armada_get_temperature(int *temperature)
{
	const unsigned long int coef_b = 1169498786UL;
	const unsigned long int coef_m = 2000000UL;
	const unsigned long int coef_div = 4289;
	const unsigned long int measurement_offset = 24;  // empirical value
	const unsigned long int lowest_report_value = 40;

	int ret = -EIO;
	unsigned long ulTemperature = 0;

	if (!g_syno_tz)
		return ret;

	ret = armada_get_temp(g_syno_tz, &ulTemperature);
	if (ret != 0)
		return ret;

	ulTemperature = ((ulTemperature * coef_div) + coef_b) / coef_m;
	ulTemperature = ((((10000 * ulTemperature) / 21445) * 1000) - 272674) / 1000;

	if (ulTemperature >= lowest_report_value + measurement_offset) {
		ulTemperature -= measurement_offset;
	} else {
		ulTemperature = lowest_report_value;
	}

	/* check unsigned long -> int overflow */
	if (ulTemperature != (unsigned long)(int)ulTemperature)
		return -ERANGE;
	*temperature = (int)ulTemperature;
	return 0;
}

EXPORT_SYMBOL(syno_armada_get_temperature);
#endif /* CONFIG_SYNO_ARMADA */

#if defined(CONFIG_SYNO_LSP_ARMADA)
static void armadaxp_init_sensor(struct platform_device *pdev,
				 struct armada_thermal_priv *priv)
#else /* CONFIG_SYNO_LSP_ARMADA */
static void armadaxp_init_sensor(struct armada_thermal_priv *priv)
#endif /* CONFIG_SYNO_LSP_ARMADA */
{
	unsigned long reg;

	reg = readl_relaxed(priv->control);
	reg |= PMU_TDC0_OTF_CAL_MASK;
	writel(reg, priv->control);

	/* Reference calibration value */
	reg &= ~PMU_TDC0_REF_CAL_CNT_MASK;
	reg |= (0xf1 << PMU_TDC0_REF_CAL_CNT_OFFS);
	writel(reg, priv->control);

	/* Reset the sensor */
	reg = readl_relaxed(priv->control);
	writel((reg | PMU_TDC0_SW_RST_MASK), priv->control);

	writel(reg, priv->control);

	/* Enable the sensor */
	reg = readl_relaxed(priv->sensor);
	reg &= ~PMU_TM_DISABLE_MASK;
	writel(reg, priv->sensor);
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
static void armada370_init_sensor(struct platform_device *pdev,
				  struct armada_thermal_priv *priv)
#else /* CONFIG_SYNO_LSP_ARMADA */
static void armada370_init_sensor(struct armada_thermal_priv *priv)
#endif /* CONFIG_SYNO_LSP_ARMADA */
{
	unsigned long reg;

	reg = readl_relaxed(priv->control);
	reg |= PMU_TDC0_OTF_CAL_MASK;
	writel(reg, priv->control);

	/* Reference calibration value */
	reg &= ~PMU_TDC0_REF_CAL_CNT_MASK;
	reg |= (0xf1 << PMU_TDC0_REF_CAL_CNT_OFFS);
	writel(reg, priv->control);

	reg &= ~PMU_TDC0_START_CAL_MASK;
	writel(reg, priv->control);

	mdelay(10);
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
static void armada375_init_sensor(struct platform_device *pdev,
				  struct armada_thermal_priv *priv)
{
	unsigned long reg;
	bool quirk_needed =
		!!of_device_is_compatible(pdev->dev.of_node,
					  "marvell,armada375-z1-thermal");

	if (quirk_needed) {
		/* Ensure these registers have the default (reset) values */
		writel(A375_Z1_CAL_RESET_LSB, priv->control);
		writel(A375_Z1_CAL_RESET_MSB, priv->control + 0x4);
	}

	reg = readl(priv->control + 4);
	reg &= ~(A375_UNIT_CONTROL_MASK << A375_UNIT_CONTROL_SHIFT);
	reg &= ~A375_READOUT_INVERT;
	reg &= ~A375_HW_RESETn;

	if (quirk_needed)
		reg |= A375_Z1_WORKAROUND_BIT;

	writel(reg, priv->control + 4);
	mdelay(20);

	reg |= A375_HW_RESETn;
	writel(reg, priv->control + 4);
	mdelay(50);
}

static void armada380_init_sensor(struct platform_device *pdev,
				  struct armada_thermal_priv *priv)
{
	unsigned long reg = readl_relaxed(priv->control);

	/* Reset hardware once */
	if (!(reg & A380_HW_RESET)) {
		reg |= A380_HW_RESET;
		writel(reg, priv->control);
		mdelay(10);
	}
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

static bool armada_is_valid(struct armada_thermal_priv *priv)
{
	unsigned long reg = readl_relaxed(priv->sensor);

#if defined(CONFIG_SYNO_LSP_ARMADA)
	return (reg >> priv->data->is_valid_shift) & THERMAL_VALID_MASK;
#else /* CONFIG_SYNO_LSP_ARMADA */
	return (reg >> THERMAL_VALID_OFFSET) & THERMAL_VALID_MASK;
#endif /* CONFIG_SYNO_LSP_ARMADA */
}

static int armada_get_temp(struct thermal_zone_device *thermal,
			  unsigned long *temp)
{
	struct armada_thermal_priv *priv = thermal->devdata;
	unsigned long reg;
#if defined(CONFIG_SYNO_LSP_ARMADA)
	unsigned long m, b, div;
#endif /* CONFIG_SYNO_LSP_ARMADA */

	/* Valid check */
#if defined(CONFIG_SYNO_LSP_ARMADA)
	if (priv->data->is_valid && !priv->data->is_valid(priv)) {
#else /* CONFIG_SYNO_LSP_ARMADA */
	if (priv->ops->is_valid && !priv->ops->is_valid(priv)) {
#endif /* CONFIG_SYNO_LSP_ARMADA */
		dev_err(&thermal->device,
			"Temperature sensor reading not valid\n");
		return -EIO;
	}

	reg = readl_relaxed(priv->sensor);
#if defined(CONFIG_SYNO_LSP_ARMADA)
	reg = (reg >> priv->data->temp_shift) & priv->data->temp_mask;

	/* Get formula coeficients */
	b = priv->data->coef_b;
	m = priv->data->coef_m;
	div = priv->data->coef_div;

	if (priv->data->inverted)
		*temp = ((m * reg) - b) / div;
	else
		*temp = (b - (m * reg)) / div;
#else /* CONFIG_SYNO_LSP_ARMADA */
	reg = (reg >> THERMAL_TEMP_OFFSET) & THERMAL_TEMP_MASK;
	*temp = (3153000000UL - (10000000UL*reg)) / 13825;
#endif /* CONFIG_SYNO_LSP_ARMADA */
	return 0;
}

static struct thermal_zone_device_ops ops = {
	.get_temp = armada_get_temp,
};

#if defined(CONFIG_SYNO_LSP_ARMADA)
static const struct armada_thermal_data armadaxp_data = {
	.init_sensor = armadaxp_init_sensor,
	.temp_shift = 10,
	.temp_mask = 0x1ff,
	.coef_b = 3153000000UL,
	.coef_m = 10000000UL,
	.coef_div = 13825,
};

static const struct armada_thermal_data armada370_data = {
	.is_valid = armada_is_valid,
	.init_sensor = armada370_init_sensor,
	.is_valid_shift = 9,
	.temp_shift = 10,
	.temp_mask = 0x1ff,
	.coef_b = 3153000000UL,
	.coef_m = 10000000UL,
	.coef_div = 13825,
};

static const struct armada_thermal_data armada375_data = {
	.is_valid = armada_is_valid,
	.init_sensor = armada375_init_sensor,
	.is_valid_shift = 10,
	.temp_shift = 0,
	.temp_mask = 0x1ff,
	.coef_b = 3171900000UL,
	.coef_m = 10000000UL,
	.coef_div = 13616,
};

static const struct armada_thermal_data armada380_data = {
	.is_valid = armada_is_valid,
	.init_sensor = armada380_init_sensor,
	.is_valid_shift = 10,
	.temp_shift = 0,
	.temp_mask = 0x3ff,
#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
	.coef_b = 2931108200UL,
	.coef_m = 5000000UL,
	.coef_div = 10502,
#else /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
	.coef_b = 1169498786UL,
	.coef_m = 2000000UL,
	.coef_div = 4289,
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
	.inverted = true,
};
#else /* CONFIG_SYNO_LSP_ARMADA */
static const struct armada_thermal_ops armadaxp_ops = {
	.init_sensor = armadaxp_init_sensor,
};

static const struct armada_thermal_ops armada370_ops = {
	.is_valid = armada_is_valid,
	.init_sensor = armada370_init_sensor,
};
#endif /* CONFIG_SYNO_LSP_ARMADA */

static const struct of_device_id armada_thermal_id_table[] = {
	{
		.compatible = "marvell,armadaxp-thermal",
#if defined(CONFIG_SYNO_LSP_ARMADA)
		.data       = &armadaxp_data,
#else /* CONFIG_SYNO_LSP_ARMADA */
		.data       = &armadaxp_ops,
#endif /* CONFIG_SYNO_LSP_ARMADA */
	},
	{
		.compatible = "marvell,armada370-thermal",
#if defined(CONFIG_SYNO_LSP_ARMADA)
		.data       = &armada370_data,
#else /* CONFIG_SYNO_LSP_ARMADA */
		.data       = &armada370_ops,
#endif /* CONFIG_SYNO_LSP_ARMADA */
	},
#if defined(CONFIG_SYNO_LSP_ARMADA)
	{
		.compatible = "marvell,armada375-thermal",
		.data       = &armada375_data,
	},
	{
		.compatible = "marvell,armada375-z1-thermal",
		.data       = &armada375_data,
	},
	{
		.compatible = "marvell,armada380-thermal",
		.data       = &armada380_data,
	},
#endif /* CONFIG_SYNO_LSP_ARMADA */
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, armada_thermal_id_table);

static int armada_thermal_probe(struct platform_device *pdev)
{
	struct thermal_zone_device *thermal;
	const struct of_device_id *match;
	struct armada_thermal_priv *priv;
	struct resource *res;

	match = of_match_device(armada_thermal_id_table, &pdev->dev);
	if (!match)
		return -ENODEV;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->sensor = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->sensor))
		return PTR_ERR(priv->sensor);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	priv->control = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->control))
		return PTR_ERR(priv->control);

#if defined(CONFIG_SYNO_LSP_ARMADA)
	priv->data = (struct armada_thermal_data *)match->data;
	priv->data->init_sensor(pdev, priv);
#else /* CONFIG_SYNO_LSP_ARMADA */
	priv->ops = (struct armada_thermal_ops *)match->data;
	priv->ops->init_sensor(priv);
#endif /* CONFIG_SYNO_LSP_ARMADA */

	thermal = thermal_zone_device_register("armada_thermal", 0, 0,
					       priv, &ops, NULL, 0, 0);
	if (IS_ERR(thermal)) {
		dev_err(&pdev->dev,
			"Failed to register thermal zone device\n");
		return PTR_ERR(thermal);
	}

	platform_set_drvdata(pdev, thermal);

#if defined(CONFIG_SYNO_ARMADA)
	g_syno_tz = thermal;
#endif /* CONFIG_SYNO_ARMADA */

	return 0;
}

static int armada_thermal_exit(struct platform_device *pdev)
{
	struct thermal_zone_device *armada_thermal =
		platform_get_drvdata(pdev);

	thermal_zone_device_unregister(armada_thermal);
#if defined(CONFIG_SYNO_LSP_ARMADA)
	// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
	platform_set_drvdata(pdev, NULL);
#endif /* CONFIG_SYNO_LSP_ARMADA */

	return 0;
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
static int armada_thermal_resume(struct platform_device *pdev)
{
	struct thermal_zone_device *thermal =
		platform_get_drvdata(pdev);
	struct armada_thermal_priv *priv = thermal->devdata;

	priv->data->init_sensor(pdev, priv);

	return 0;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

static struct platform_driver armada_thermal_driver = {
	.probe = armada_thermal_probe,
	.remove = armada_thermal_exit,
#if defined(CONFIG_SYNO_LSP_ARMADA) && defined(CONFIG_PM)
	.resume = armada_thermal_resume,
#endif /* CONFIG_SYNO_LSP_ARMADA && CONFIG_PM */
	.driver = {
		.name = "armada_thermal",
		.owner = THIS_MODULE,
#if defined(CONFIG_SYNO_LSP_ARMADA)
		.of_match_table = armada_thermal_id_table,
#else /* CONFIG_SYNO_LSP_ARMADA */
		.of_match_table = of_match_ptr(armada_thermal_id_table),
#endif /* CONFIG_SYNO_LSP_ARMADA */
	},
};

module_platform_driver(armada_thermal_driver);

MODULE_AUTHOR("Ezequiel Garcia <ezequiel.garcia@free-electrons.com>");
MODULE_DESCRIPTION("Armada 370/XP thermal driver");
MODULE_LICENSE("GPL v2");
