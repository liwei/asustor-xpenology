/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 *
 * Copyright (C) 2014 Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 *
 * The OPP code in function cpu0_set_target() is reused from
 * drivers/cpufreq/omap-cpufreq.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#include <linux/cpu.h>
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
#include <linux/cpu.h>
#include <linux/cpu_cooling.h>
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
#include <linux/cpufreq.h>
#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
#include <linux/cpumask.h>
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
#include <linux/pm_opp.h>
#else /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
#include <linux/opp.h>
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
#include <linux/thermal.h>

struct private_data {
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct thermal_cooling_device *cdev;
	unsigned int voltage_tolerance; /* in percentage */
};
#else /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */

static unsigned int transition_latency;
static unsigned int voltage_tolerance; /* in percentage */

static struct device *cpu_dev;
static struct clk *cpu_clk;
static struct regulator *cpu_reg;
static struct cpufreq_frequency_table *freq_table;
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */

#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
static int cpu0_set_target(struct cpufreq_policy *policy, unsigned int index)
#else /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
static int cpu0_verify_speed(struct cpufreq_policy *policy)
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static unsigned int cpu0_get_speed(unsigned int cpu)
{
	return clk_get_rate(cpu_clk) / 1000;
}

#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
static int cpu0_set_target(struct cpufreq_policy *policy,
			   unsigned int target_freq, unsigned int relation)
{
	struct cpufreq_freqs freqs;
	struct dev_pm_opp *opp;
	struct cpufreq_frequency_table *freq_table = policy->freq_table;
	struct clk *cpu_clk = policy->clk;
	struct private_data *priv = policy->driver_data;
	struct device *cpu_dev = priv->cpu_dev;
	struct regulator *cpu_reg = priv->cpu_reg;
	unsigned long volt = 0, volt_old = 0, tol = 0;
	unsigned int old_freq, new_freq;
	long freq_Hz, freq_exact;
	int ret;

	freq_Hz = clk_round_rate(cpu_clk, freq_table[index].frequency * 1000);
	if (freq_Hz <= 0)
		freq_Hz = freq_table[index].frequency * 1000;

	freq_exact = freq_Hz;
	new_freq = freq_Hz / 1000;
	old_freq = clk_get_rate(cpu_clk) / 1000;

	if (!IS_ERR(cpu_reg)) {
		rcu_read_lock();
		opp = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_Hz);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			dev_err(cpu_dev, "failed to find OPP for %ld\n",
				freq_Hz);
			return PTR_ERR(opp);
		}
		volt = dev_pm_opp_get_voltage(opp);
		rcu_read_unlock();
		tol = volt * priv->voltage_tolerance / 100;
		volt_old = regulator_get_voltage(cpu_reg);
	}

	dev_dbg(cpu_dev, "%u MHz, %ld mV --> %u MHz, %ld mV\n",
		old_freq / 1000, volt_old ? volt_old / 1000 : -1,
		new_freq / 1000, volt ? volt / 1000 : -1);

	/* scaling up?  scale voltage before frequency */
	if (!IS_ERR(cpu_reg) && new_freq > old_freq) {
		ret = regulator_set_voltage_tol(cpu_reg, volt, tol);
		if (ret) {
			dev_err(cpu_dev, "failed to scale voltage up: %d\n",
				ret);
			return ret;
		}
	}

	ret = clk_set_rate(cpu_clk, freq_exact);
	if (ret) {
		dev_err(cpu_dev, "failed to set clock rate: %d\n", ret);
		if (!IS_ERR(cpu_reg))
			regulator_set_voltage_tol(cpu_reg, volt_old, tol);
		return ret;
	}

	/* scaling down?  scale voltage after frequency */
	if (!IS_ERR(cpu_reg) && new_freq < old_freq) {
		ret = regulator_set_voltage_tol(cpu_reg, volt, tol);
		if (ret) {
			dev_err(cpu_dev, "failed to scale voltage down: %d\n",
				ret);
			clk_set_rate(cpu_clk, old_freq * 1000);
		}
	}

	return ret;
}

static int allocate_resources(int cpu, struct device **cdev,
				  struct regulator **creg, struct clk **cclk)
{
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	int ret = 0;
	char *reg_cpu0 = "cpu0", *reg_cpu = "cpu", *reg;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	/* Try "cpu0" for older DTs */
	if (!cpu)
		reg = reg_cpu0;
	else
		reg = reg_cpu;

try_again:
	cpu_reg = regulator_get_optional(cpu_dev, reg);

	if (IS_ERR(cpu_reg)) {
		/*
		 * If cpu's regulator supply node is present, but regulator is
		 * not yet registered, we should try defering probe.
		 */
		if (PTR_ERR(cpu_reg) == -EPROBE_DEFER) {
			dev_dbg(cpu_dev, "cpu%d regulator not ready, retry\n",
				cpu);
			return -EPROBE_DEFER;
		}

		/* Try with "cpu-supply" */
		if (reg == reg_cpu0) {
			reg = reg_cpu;
			goto try_again;
		}

		dev_warn(cpu_dev, "failed to get cpu%d regulator: %ld\n",
			 cpu, PTR_ERR(cpu_reg));
	}

	cpu_clk = clk_get(cpu_dev, NULL);
	if (IS_ERR(cpu_clk)) {
		/* put regulator */
		if (!IS_ERR(cpu_reg))
			regulator_put(cpu_reg);

		ret = PTR_ERR(cpu_clk);

		/*
		 * If cpu's clk node is present, but clock is not yet
		 * registered, we should try defering probe.
		 */
		if (ret == -EPROBE_DEFER)
			dev_dbg(cpu_dev, "cpu%d clock not ready, retry\n", cpu);
		else
			dev_err(cpu_dev, "failed to get cpu%d clock: %d\n", ret,
				cpu);
	} else {
		*cdev = cpu_dev;
		*creg = cpu_reg;
		*cclk = cpu_clk;
	}

	return ret;
}

static int cpu0_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;
	struct thermal_cooling_device *cdev;
	struct device_node *np;
	struct private_data *priv;
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	unsigned int transition_latency;
	int ret;

	ret = allocate_resources(policy->cpu, &cpu_dev, &cpu_reg, &cpu_clk);
	if (ret) {
		pr_err("%s: Failed to allocate resources\n: %d", __func__, ret);
		return ret;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		dev_err(cpu_dev, "failed to find cpu%d node\n", policy->cpu);
		ret = -ENOENT;
		goto out_put_reg_clk;
	}

	/* OPPs might be populated at runtime, don't check for error here */
	of_init_opp_table(cpu_dev);

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		dev_err(cpu_dev, "failed to init cpufreq table: %d\n", ret);
		goto out_put_node;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out_free_table;
	}

	of_property_read_u32(np, "voltage-tolerance", &priv->voltage_tolerance);

	if (of_property_read_u32(np, "clock-latency", &transition_latency))
		transition_latency = CPUFREQ_ETERNAL;

	if (!IS_ERR(cpu_reg)) {
		struct dev_pm_opp *opp;
		unsigned long min_uV, max_uV;
		int i;

		/*
		 * OPP is maintained in order of increasing frequency, and
		 * freq_table initialised from OPP is therefore sorted in the
		 * same order.
		 */
		for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
			;
		rcu_read_lock();
		opp = dev_pm_opp_find_freq_exact(cpu_dev,
				freq_table[0].frequency * 1000, true);
		min_uV = dev_pm_opp_get_voltage(opp);
		opp = dev_pm_opp_find_freq_exact(cpu_dev,
				freq_table[i-1].frequency * 1000, true);
		max_uV = dev_pm_opp_get_voltage(opp);
		rcu_read_unlock();
		ret = regulator_set_voltage_time(cpu_reg, min_uV, max_uV);
		if (ret > 0)
			transition_latency += ret * 1000;
	}

	/*
	 * For now, just loading the cooling device;
	 * thermal DT code takes care of matching them.
	 */
	if (of_find_property(np, "#cooling-cells", NULL)) {
		cdev = of_cpufreq_cooling_register(np, cpu_present_mask);
		if (IS_ERR(cdev))
			dev_err(cpu_dev,
				"running cpufreq without cooling device: %ld\n",
				PTR_ERR(cdev));
		else
			priv->cdev = cdev;
	}
	of_node_put(np);

	priv->cpu_dev = cpu_dev;
	priv->cpu_reg = cpu_reg;
	policy->driver_data = priv;

	policy->clk = cpu_clk;
	ret = cpufreq_generic_init(policy, freq_table, transition_latency);
	if (ret)
		goto out_cooling_unregister;

	return 0;

out_cooling_unregister:
	cpufreq_cooling_unregister(priv->cdev);
	kfree(priv);
out_free_table:
	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table);
out_put_node:
	of_node_put(np);
out_put_reg_clk:
	clk_put(cpu_clk);
	if (!IS_ERR(cpu_reg))
		regulator_put(cpu_reg);

	return ret;
}

static int cpu0_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;
	struct thermal_cooling_device *cdev;
	struct device_node *np;
	struct private_data *priv;
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	unsigned int transition_latency;
	int ret;

	ret = allocate_resources(policy->cpu, &cpu_dev, &cpu_reg, &cpu_clk);
	if (ret) {
		pr_err("%s: Failed to allocate resources\n: %d", __func__, ret);
		return ret;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		dev_err(cpu_dev, "failed to find cpu%d node\n", policy->cpu);
		ret = -ENOENT;
		goto out_put_reg_clk;
	}

	/* OPPs might be populated at runtime, don't check for error here */
	of_init_opp_table(cpu_dev);

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		dev_err(cpu_dev, "failed to init cpufreq table: %d\n", ret);
		goto out_put_node;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out_free_table;
	}

	of_property_read_u32(np, "voltage-tolerance", &priv->voltage_tolerance);

	if (of_property_read_u32(np, "clock-latency", &transition_latency))
		transition_latency = CPUFREQ_ETERNAL;

	if (!IS_ERR(cpu_reg)) {
		struct dev_pm_opp *opp;
		unsigned long min_uV, max_uV;
		int i;

		/*
		 * OPP is maintained in order of increasing frequency, and
		 * freq_table initialised from OPP is therefore sorted in the
		 * same order.
		 */
		for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
			;
		rcu_read_lock();
		opp = dev_pm_opp_find_freq_exact(cpu_dev,
				freq_table[0].frequency * 1000, true);
		min_uV = dev_pm_opp_get_voltage(opp);
		opp = dev_pm_opp_find_freq_exact(cpu_dev,
				freq_table[i-1].frequency * 1000, true);
		max_uV = dev_pm_opp_get_voltage(opp);
		rcu_read_unlock();
		ret = regulator_set_voltage_time(cpu_reg, min_uV, max_uV);
		if (ret > 0)
			transition_latency += ret * 1000;
	}

	/*
	 * For now, just loading the cooling device;
	 * thermal DT code takes care of matching them.
	 */
	if (of_find_property(np, "#cooling-cells", NULL)) {
		cdev = of_cpufreq_cooling_register(np, cpu_present_mask);
		if (IS_ERR(cdev))
			dev_err(cpu_dev,
				"running cpufreq without cooling device: %ld\n",
				PTR_ERR(cdev));
		else
			priv->cdev = cdev;
	}
	of_node_put(np);

	priv->cpu_dev = cpu_dev;
	priv->cpu_reg = cpu_reg;
	policy->driver_data = priv;

	policy->clk = cpu_clk;
	ret = cpufreq_generic_init(policy, freq_table, transition_latency);
	if (ret)
		goto out_cooling_unregister;

	return 0;

out_cooling_unregister:
	cpufreq_cooling_unregister(priv->cdev);
	kfree(priv);
out_free_table:
	dev_pm_opp_free_cpufreq_table(cpu_dev, &freq_table);
out_put_node:
	of_node_put(np);
out_put_reg_clk:
	clk_put(cpu_clk);
	if (!IS_ERR(cpu_reg))
		regulator_put(cpu_reg);

	return ret;
}

static int cpu0_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct private_data *priv = policy->driver_data;

	cpufreq_cooling_unregister(priv->cdev);
	dev_pm_opp_free_cpufreq_table(priv->cpu_dev, &policy->freq_table);
	clk_put(policy->clk);
	if (!IS_ERR(priv->cpu_reg))
		regulator_put(priv->cpu_reg);
	kfree(priv);

	return 0;
}

static struct cpufreq_driver cpu0_cpufreq_driver = {
	.flags = CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = cpu0_set_target,
	.get = cpufreq_generic_get,
	.init = cpu0_cpufreq_init,
	.exit = cpu0_cpufreq_exit,
	.name = "generic_cpu0",
	.attr = cpufreq_generic_attr,
};

static int cpu0_cpufreq_probe(struct platform_device *pdev)
{
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	int ret;

	/*
	 * All per-cluster (CPUs sharing clock/voltages) initialization is done
	 * from ->init(). In probe(), we just need to make sure that clk and
	 * regulators are available. Else defer probe and retry.
	 *
	 * FIXME: Is checking this only for CPU0 sufficient ?
	 */
	ret = allocate_resources(0, &cpu_dev, &cpu_reg, &cpu_clk);
	if (ret)
		return ret;

	clk_put(cpu_clk);
	if (!IS_ERR(cpu_reg))
		regulator_put(cpu_reg);

	ret = cpufreq_register_driver(&cpu0_cpufreq_driver);
	if (ret)
		dev_err(cpu_dev, "failed register driver: %d\n", ret);

	return ret;
}

static int cpu0_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&cpu0_cpufreq_driver);
	return 0;
}
#else /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
static int cpu0_set_target(struct cpufreq_policy *policy,
			   unsigned int target_freq, unsigned int relation)
{
	struct cpufreq_freqs freqs;
	struct opp *opp;
	unsigned long volt = 0, volt_old = 0, tol = 0;
	long freq_Hz, freq_exact;
	unsigned int index;
	int ret;

	ret = cpufreq_frequency_table_target(policy, freq_table, target_freq,
					     relation, &index);
	if (ret) {
		pr_err("failed to match target freqency %d: %d\n",
		       target_freq, ret);
		return ret;
	}

	freq_Hz = clk_round_rate(cpu_clk, freq_table[index].frequency * 1000);
	if (freq_Hz < 0)
		freq_Hz = freq_table[index].frequency * 1000;
	freq_exact = freq_Hz;
	freqs.new = freq_Hz / 1000;
	freqs.old = clk_get_rate(cpu_clk) / 1000;

	if (freqs.old == freqs.new)
		return 0;

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	if (cpu_reg) {
		rcu_read_lock();
		opp = opp_find_freq_ceil(cpu_dev, &freq_Hz);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			pr_err("failed to find OPP for %ld\n", freq_Hz);
			freqs.new = freqs.old;
			ret = PTR_ERR(opp);
			goto post_notify;
		}
		volt = opp_get_voltage(opp);
		rcu_read_unlock();
		tol = volt * voltage_tolerance / 100;
		volt_old = regulator_get_voltage(cpu_reg);
	}

	pr_debug("%u MHz, %ld mV --> %u MHz, %ld mV\n",
		 freqs.old / 1000, volt_old ? volt_old / 1000 : -1,
		 freqs.new / 1000, volt ? volt / 1000 : -1);

	/* scaling up?  scale voltage before frequency */
	if (cpu_reg && freqs.new > freqs.old) {
		ret = regulator_set_voltage_tol(cpu_reg, volt, tol);
		if (ret) {
			pr_err("failed to scale voltage up: %d\n", ret);
			freqs.new = freqs.old;
			goto post_notify;
		}
	}

	ret = clk_set_rate(cpu_clk, freq_exact);
	if (ret) {
		pr_err("failed to set clock rate: %d\n", ret);
		if (cpu_reg)
			regulator_set_voltage_tol(cpu_reg, volt_old, tol);
		freqs.new = freqs.old;
		goto post_notify;
	}

	/* scaling down?  scale voltage after frequency */
	if (cpu_reg && freqs.new < freqs.old) {
		ret = regulator_set_voltage_tol(cpu_reg, volt, tol);
		if (ret) {
			pr_err("failed to scale voltage down: %d\n", ret);
			clk_set_rate(cpu_clk, freqs.old * 1000);
			freqs.new = freqs.old;
		}
	}

post_notify:
	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);

	return ret;
}

static int cpu0_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret;

	ret = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (ret) {
		pr_err("invalid frequency table: %d\n", ret);
		return ret;
	}

	policy->cpuinfo.transition_latency = transition_latency;
	policy->cur = clk_get_rate(cpu_clk) / 1000;

	/*
	 * The driver only supports the SMP configuartion where all processors
	 * share the clock and voltage and clock.  Use cpufreq affected_cpus
	 * interface to have all CPUs scaled together.
	 */
	cpumask_setall(policy->cpus);

	cpufreq_frequency_table_get_attr(freq_table, policy->cpu);

	return 0;
}

static int cpu0_cpufreq_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_put_attr(policy->cpu);

	return 0;
}

static struct freq_attr *cpu0_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver cpu0_cpufreq_driver = {
	.flags = CPUFREQ_STICKY,
	.verify = cpu0_verify_speed,
	.target = cpu0_set_target,
	.get = cpu0_get_speed,
	.init = cpu0_cpufreq_init,
	.exit = cpu0_cpufreq_exit,
	.name = "generic_cpu0",
	.attr = cpu0_cpufreq_attr,
};

static int cpu0_cpufreq_probe(struct platform_device *pdev)
{
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
	struct device_node *np;
	int ret;

	cpu_dev = get_cpu_device(0);
	if (!cpu_dev) {
		pr_err("failed to get cpu0 device\n");
		return -ENODEV;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_err("failed to find cpu0 node\n");
		return -ENOENT;
	}
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	struct device_node *np, *parent;
	int ret;

	parent = of_find_node_by_path("/cpus");
	if (!parent) {
		pr_err("failed to find OF /cpus\n");
		return -ENOENT;
	}

	for_each_child_of_node(parent, np) {
		if (of_get_property(np, "operating-points", NULL))
			break;
	}

	if (!np) {
		pr_err("failed to find cpu0 node\n");
		ret = -ENOENT;
		goto out_put_parent;
	}

	cpu_dev = &pdev->dev;
	cpu_dev->of_node = np;
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */

	cpu_reg = devm_regulator_get(cpu_dev, "cpu0");
	if (IS_ERR(cpu_reg)) {
		/*
		 * If cpu0 regulator supply node is present, but regulator is
		 * not yet registered, we should try defering probe.
		 */
		if (PTR_ERR(cpu_reg) == -EPROBE_DEFER) {
			dev_err(cpu_dev, "cpu0 regulator not ready, retry\n");
			ret = -EPROBE_DEFER;
			goto out_put_node;
		}
		pr_warn("failed to get cpu0 regulator: %ld\n",
			PTR_ERR(cpu_reg));
		cpu_reg = NULL;
	}

	cpu_clk = devm_clk_get(cpu_dev, NULL);
	if (IS_ERR(cpu_clk)) {
		ret = PTR_ERR(cpu_clk);
		pr_err("failed to get cpu0 clock: %d\n", ret);
		goto out_put_node;
	}

#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
	/* OPPs might be populated at runtime, don't check for error here */
	of_init_opp_table(cpu_dev);
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	ret = of_init_opp_table(cpu_dev);
	if (ret) {
		pr_err("failed to init OPP table: %d\n", ret);
		goto out_put_node;
	}
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */

	ret = opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		pr_err("failed to init cpufreq table: %d\n", ret);
		goto out_put_node;
	}

	of_property_read_u32(np, "voltage-tolerance", &voltage_tolerance);

	if (of_property_read_u32(np, "clock-latency", &transition_latency))
		transition_latency = CPUFREQ_ETERNAL;

	if (cpu_reg) {
		struct opp *opp;
		unsigned long min_uV, max_uV;
		int i;

		/*
		 * OPP is maintained in order of increasing frequency, and
		 * freq_table initialised from OPP is therefore sorted in the
		 * same order.
		 */
		for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
			;
		rcu_read_lock();
		opp = opp_find_freq_exact(cpu_dev,
				freq_table[0].frequency * 1000, true);
		min_uV = opp_get_voltage(opp);
		opp = opp_find_freq_exact(cpu_dev,
				freq_table[i-1].frequency * 1000, true);
		max_uV = opp_get_voltage(opp);
		rcu_read_unlock();
		ret = regulator_set_voltage_time(cpu_reg, min_uV, max_uV);
		if (ret > 0)
			transition_latency += ret * 1000;
	}

	ret = cpufreq_register_driver(&cpu0_cpufreq_driver);
	if (ret) {
		pr_err("failed register driver: %d\n", ret);
		goto out_free_table;
	}

	of_node_put(np);
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	of_node_put(parent);
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	return 0;

out_free_table:
	opp_free_cpufreq_table(cpu_dev, &freq_table);
out_put_node:
	of_node_put(np);
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
out_put_parent:
	of_node_put(parent);
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	return ret;
}

static int cpu0_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&cpu0_cpufreq_driver);
	opp_free_cpufreq_table(cpu_dev, &freq_table);

	return 0;
}
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */

static struct platform_driver cpu0_cpufreq_platdrv = {
	.driver = {
		.name	= "cpufreq-cpu0",
		.owner	= THIS_MODULE,
	},
	.probe		= cpu0_cpufreq_probe,
	.remove		= cpu0_cpufreq_remove,
};
module_platform_driver(cpu0_cpufreq_platdrv);

#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
MODULE_AUTHOR("Viresh Kumar <viresh.kumar@linaro.org>");
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */
MODULE_AUTHOR("Shawn Guo <shawn.guo@linaro.org>");
MODULE_DESCRIPTION("Generic CPU0 cpufreq driver");
MODULE_LICENSE("GPL");
