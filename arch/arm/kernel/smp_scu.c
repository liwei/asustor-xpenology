/*
 *  linux/arch/arm/kernel/smp_scu.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/io.h>

#include <asm/smp_plat.h>
#include <asm/smp_scu.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>

#define SCU_CTRL		0x00
#define SCU_CONFIG		0x04
#define SCU_CPU_STATUS		0x08
#define SCU_INVALIDATE		0x0c
#define SCU_FPGA_REVISION	0x10

#if defined(CONFIG_SMP) || defined(CONFIG_SYNO_LSP_ARMADA)
/*
 * Get the number of CPU cores from the SCU configuration
 */
unsigned int __init scu_get_core_count(void __iomem *scu_base)
{
#if defined(CONFIG_SYNO_LSP_ARMADA)
	unsigned int ncores = readl_relaxed(scu_base + SCU_CONFIG);
#else /* CONFIG_SYNO_LSP_ARMADA */
	unsigned int ncores = __raw_readl(scu_base + SCU_CONFIG);
#endif /* CONFIG_SYNO_LSP_ARMADA */
	return (ncores & 0x03) + 1;
}

/*
 * Enable the SCU
 */
void scu_enable(void __iomem *scu_base)
{
	u32 scu_ctrl;

#ifdef CONFIG_ARM_ERRATA_764369
	/* Cortex-A9 only */
	if ((read_cpuid_id() & 0xff0ffff0) == 0x410fc090) {
#if defined(CONFIG_SYNO_LSP_ARMADA)
		scu_ctrl = readl_relaxed(scu_base + 0x30);
		if (!(scu_ctrl & 1))
			writel_relaxed(scu_ctrl | 0x1, scu_base + 0x30);
#else /* CONFIG_SYNO_LSP_ARMADA */
		scu_ctrl = __raw_readl(scu_base + 0x30);
		if (!(scu_ctrl & 1))
			__raw_writel(scu_ctrl | 0x1, scu_base + 0x30);
#endif /* CONFIG_SYNO_LSP_ARMADA */
	}
#endif

#if defined(CONFIG_SYNO_LSP_ARMADA)
	scu_ctrl = readl_relaxed(scu_base + SCU_CTRL);
#else /* CONFIG_SYNO_LSP_ARMADA */
	scu_ctrl = __raw_readl(scu_base + SCU_CTRL);
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if defined (CONFIG_SYNO_LSP_MONACO)
	/* Enable SCU standby mode To allow L2 cache controller idle mode */
	scu_ctrl |= BIT(5);
	__raw_writel(scu_ctrl, scu_base + SCU_CTRL);
#endif /* CONFIG_SYNO_LSP_MONACO */
	/* already enabled? */
	if (scu_ctrl & 1)
		return;

	scu_ctrl |= 1;
#if defined(CONFIG_SYNO_LSP_ARMADA)
	writel_relaxed(scu_ctrl, scu_base + SCU_CTRL);
#else /* CONFIG_SYNO_LSP_ARMADA */
	__raw_writel(scu_ctrl, scu_base + SCU_CTRL);
#endif /* CONFIG_SYNO_LSP_ARMADA */

	/*
	 * Ensure that the data accessed by CPU0 before the SCU was
	 * initialised is visible to the other CPUs.
	 */
	flush_cache_all();
}
#endif

/*
 * Set the executing CPUs power mode as defined.  This will be in
 * preparation for it executing a WFI instruction.
 *
 * This function must be called with preemption disabled, and as it
 * has the side effect of disabling coherency, caches must have been
 * flushed.  Interrupts must also have been disabled.
 */
int scu_power_mode(void __iomem *scu_base, unsigned int mode)
{
	unsigned int val;
	int cpu = MPIDR_AFFINITY_LEVEL(cpu_logical_map(smp_processor_id()), 0);

	if (mode > 3 || mode == 1 || cpu > 3)
		return -EINVAL;

#if defined(CONFIG_SYNO_LSP_ARMADA)
	val = readb_relaxed(scu_base + SCU_CPU_STATUS + cpu) & ~0x03;
	val |= mode;
	writeb_relaxed(val, scu_base + SCU_CPU_STATUS + cpu);
#else /* CONFIG_SYNO_LSP_ARMADA */
	val = __raw_readb(scu_base + SCU_CPU_STATUS + cpu) & ~0x03;
	val |= mode;
	__raw_writeb(val, scu_base + SCU_CPU_STATUS + cpu);
#endif /* CONFIG_SYNO_LSP_ARMADA */

	return 0;
}
