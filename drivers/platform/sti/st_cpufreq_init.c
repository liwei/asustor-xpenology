/*
 * Create cpufreq OPP list at runtime
 * Author: Ajit Pal Singh <ajitpal.singh@st.com>
 *
 * Copyright (C) 2014 STMicroelectronics (R&D) Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/opp.h>
#include <linux/of.h>

/* In MHz */
static int st_cpufreq_freq_list[] = {2000000, 1500000, 1200000, 800000,
				     500000};

#define N_FREQS	ARRAY_SIZE(st_cpufreq_freq_list)

static int __init st_cpufreq_init(void)
{
	struct device *cpu_dev;
	struct clk *cpu_clk;
	unsigned int clk_rate;
	struct property *prop;
	int i = 0;

	cpu_dev = get_cpu_device(0);
	if (!cpu_dev) {
		pr_err("failed to get cpu0 device\n");
		return -ENODEV;
	}

	prop = of_find_property(cpu_dev->of_node, "operating-points", NULL);
	if (prop && prop->value) {
		pr_err("OPP list provided through DT\n");
		return 0;
	}

	cpu_clk = devm_clk_get(cpu_dev, NULL);
	if (IS_ERR(cpu_clk)) {
		pr_err("failed to get cpu0 clock\n");
		return PTR_ERR(cpu_clk);
	}

	clk_rate = clk_get_rate(cpu_clk) / 1000;
	if (!clk_rate) {
		pr_err("failed to get cpu clk rate\n");
		return -EINVAL;
	}

	/*
	 * The algorithm below works on the following principles:
	 * 1) The freq set by the TP is always taken as the max OPP.
	 * 2) If TP sets freq which is not in the driver freq table,
	 *    it is still added as an OPP. In this case the freq in the table,
	 *    which is lesser than the TP set freq, is added next to the
	 *    opp list and so on.
	 */

	while ((i < N_FREQS && clk_rate < st_cpufreq_freq_list[i]))
		i++;

	if (!i || (clk_rate > st_cpufreq_freq_list[i]) || (i == N_FREQS)) {
		if (opp_add(cpu_dev, clk_rate * 1000, 1000000)) {
			pr_err("failed to add OPP %d\n", clk_rate);
			return -EINVAL;
		}
	}

	for (; i < N_FREQS; i++)
		if (opp_add(cpu_dev, st_cpufreq_freq_list[i] * 1000, 1000000))
				pr_err("failed to add OPP %d\n", clk_rate);

	return 0;
}

device_initcall(st_cpufreq_init);

MODULE_AUTHOR("<ajitpal.singh@st.com>");
MODULE_DESCRIPTION("Creates opp list for cpufreq-cpu0 at runtime");
MODULE_LICENSE("GPL v2");
