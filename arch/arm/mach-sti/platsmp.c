/*
 *  arch/arm/mach-sti/platsmp.c
 *
 * Copyright (C) 2013 STMicroelectronics (R&D) Limited.
 *		http://www.st.com
 *
 * Cloned from linux/arch/arm/mach-vexpress/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#include <linux/reboot.h>
#include <linux/notifier.h>
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>
#include <asm/cp15.h>
#include <asm/idmap.h>

#include <linux/power/st_lpm_def.h>
#include "smp.h"

static void __iomem *sbc_base;
static unsigned long sbc_ph_base;
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
void *abap_prg_base;
static unsigned long abap_ph_base;

#ifdef CONFIG_KEXEC
static int kexec_case;

static int kexec_notify_cb(struct notifier_block *nb, ulong event, void *buf)
{
	if (event == SYS_RESTART)
		kexec_case = 1;
	return NOTIFY_DONE;
};

static struct notifier_block kexec_notifier = {
	.notifier_call = kexec_notify_cb,
};
#endif
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */

static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	/* wmb so that all cores can see this and before cache flush */
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static DEFINE_SPINLOCK(boot_lock);

void __cpuinit sti_secondary_init(unsigned int cpu)
{
	trace_hardirqs_off();

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int __cpuinit sti_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * it to jump to the secondary entrypoint.
	 */
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}

void __init sti_smp_prepare_cpus(unsigned int max_cpus)
{
	void __iomem *scu_base = NULL;
	struct resource res;

	struct device_node *np = of_find_compatible_node(
					NULL, NULL, "arm,cortex-a9-scu");
	void __iomem *cpu_strt_ptr =
		(void *)CONFIG_PAGE_OFFSET + 0x200;
	unsigned int sbc_offset = DATA_OFFSET + PEN_HOLD_VAR_OFFSET_4xx;

	unsigned long entry_pa = virt_to_phys(sti_secondary_startup);

	if (np) {
		scu_base = of_iomap(np, 0);
		scu_enable(scu_base);
		of_node_put(np);

		if (max_cpus > 1) {
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
			/*
			 * register a shutdown notification to catch
			 * kexec call
			 */
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
			/*
			 * Tell the boot monitor the
			 * address of sti_secondary_startup
			 */
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#ifdef CONFIG_KEXEC
			register_reboot_notifier(&kexec_notifier);
#endif
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
			np = of_find_compatible_node(NULL, NULL, "st,lpm");

			/*
			 * If LPM not found, fallback to legacy pen holding
			 * policy in RAM
			 */
			if (IS_ERR_OR_NULL(np)) {
				__raw_writel(entry_pa, cpu_strt_ptr);
				/*
				 * wmb so that data is actually written
				 * before cache flush is done
				 */
				smp_wmb();
				__cpuc_flush_dcache_area(cpu_strt_ptr, 4);
				outer_clean_range(__pa(cpu_strt_ptr),
						  __pa(cpu_strt_ptr + 1));
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4

				np = of_find_node_by_name(NULL, "abap-regs");
				if (IS_ERR_OR_NULL(np))
					return;

				abap_prg_base = of_iomap(np, 0);
				if (!of_address_to_resource(np, 0, &res))
					abap_ph_base = res.start;

				of_node_put(np);
				if (!abap_prg_base)
					return;
				/*
				 * we never know if this boot is a
				 * kexec initiated warm boot case of a
				 * new kernel, so signal the sec core(s)
				 * by writing also to the abap area.
				 * No harm in case we came here during
				 * other boot cases !
				 */
				__raw_writel(entry_pa, abap_prg_base +
						sti_abap_c_size - sizeof(int));

#ifndef CONFIG_KEXEC
				iounmap(abap_prg_base);
#endif
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
				return;
			}

			sbc_base = of_iomap(np, 0);
			if (!of_address_to_resource(np, 0, &res))
				sbc_ph_base = res.start;

			of_node_put(np);
			if (!sbc_base)
				return;

			writel(entry_pa, sbc_base + sbc_offset);
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#ifndef CONFIG_KEXEC
			iounmap(sbc_base);
#endif
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
		}
	}
}

#ifdef CONFIG_HOTPLUG_CPU

static inline void cpu_enter_lowpower(void)
{
	unsigned int v;

	flush_cache_louis();
	asm volatile(
	"	mcr	p15, 0, %1, c7, c5, 0\n"
	"	dsb\n"
	/*
	 * Turn off coherency
	 */
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, #0x40\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, %2\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v)
	  : "r" (0), "Ir" (CR_C)
	  : "cc");
}

static inline void cpu_leave_lowpower(void)
{
	unsigned int v;

	asm volatile(
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	orr	%0, %0, %1\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	orr	%0, %0, #0x40\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	  : "=&r" (v)
	  : "Ir" (CR_C)
	  : "cc");
}

#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
static inline void platform_do_lowpower_ram(unsigned int cpu, int *spurious)
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
{
	/*
	 * there is no power-control hardware on this platform, so all
	 * we can do is put the core into WFI; this is safe as the calling
	 * code will have already disabled interrupts
	 */
	for (;;) {
		dsb();
		wfi();

		if (pen_release == cpu_logical_map(cpu)) {
			/*
			 * OK, proper wakeup, we're done
			 */
			break;
		}

		/*
		 * Getting here, means that we have come out of WFI without
		 * having been woken up - this shouldn't happen
		 *
		 * Just note it happening - when we're woken, we can report
		 * its occurrence.
		 */
		(*spurious)++;
	}
}

#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#ifdef CONFIG_KEXEC
static void kexec_sleep(unsigned int cpu)
{
	unsigned long val;
	typedef void (*phys_reset_t)(unsigned long);
	phys_reset_t phys_reset;
	unsigned int rst_add;

	/* If no SBC available, try to use abap area of the SOC */
	if (!sbc_base || !sbc_ph_base) {
		/*
		 * if abap not available or code size that needs to copy
		 * is greater than abap area (8 word size registers), fail !
		 */
		if (!abap_prg_base || !abap_ph_base || sti_abap_c_size > 32)
			return;

		/*
		 * copy the pen holding code from the linked location
		 * to abap area
		 */
		memcpy(abap_prg_base, sti_abap_c_start, sti_abap_c_size);
		rst_add = abap_ph_base;
	} else {
		val = readl_relaxed(sbc_base +
					DATA_OFFSET + PEN_HOLD_VAR_OFFSET_4xx);

		/*
		 * val must be ~0, this is ensured by lpm driver's
		 * shutdown routine that is called before this.
		 * Otherwise no point in jumping to lpm based pen holding
		 * code. We could have directly written ~0 to pen holding var
		 * from here, but lpm driver is already doing this in shutdown
		 * taking care of all warm restart use cases that involves lpm !
		 */
		if (val != (~0))
			return;

		rst_add = sbc_ph_base +
				DATA_OFFSET + PEN_HOLD_VAR_OFFSET_4xx + 4 + 1;
	}

	setup_mm_for_reboot(); /* setup identity mapping(s) */
	flush_cache_all();
	cpu_proc_fin();
	flush_cache_all();
	phys_reset = (phys_reset_t)virt_to_phys(cpu_reset);
	phys_reset(rst_add);
	/* Should never get here. */
}
#endif
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
static void platform_do_lowpower_lpm(unsigned int cpu)
{
	unsigned long val;

	typedef void (*phys_reset_t)(unsigned long);
	phys_reset_t phys_reset;

	/* If no SBC available fall back to ram low power */
	if (!sbc_base || !sbc_ph_base)
		return;

	val = readl_relaxed(sbc_base + DATA_OFFSET + PEN_HOLD_VAR_OFFSET_4xx);

	/*
	 * If val is not equal to ~0, means we came here from
	 * cpu hotplug request or standby request. In both cases
	 * we dont need to put secondary cores in SBC-DMEM based
	 * pen holding at this point. We simply return from here
	 * and fallback to the simple ram based platform_do_lowpower_ram.
	 * In case of kexec, we will come here after shutdown routines
	 * of the drivers already executed. This means val will be equal
	 * to ~0, as LPM driver's shutdown will be already executed and
	 * that will set the PEN_HOLD_VAR_OFFSET_4xx value equal to ~0.
	 */
	if (val != (~0))
		return;

	setup_mm_for_reboot(); /* setup identity mapping(s) */
	flush_cache_all();
	cpu_proc_fin();  /* Disable caches & MMU */
	flush_cache_all();
	phys_reset = (phys_reset_t)virt_to_phys(cpu_reset);
	phys_reset(sbc_ph_base + DATA_OFFSET + PEN_HOLD_VAR_OFFSET_4xx + 4 + 1);
	/* Should never get here. */
}
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void __ref sti_cpu_die(unsigned int cpu)
{
	int spurious = 0;

#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
	/*
	 * We're ready for shutdown now, so do it.
	 * First check if it came here beacuse of
	 * a kexec !
	 */
#ifdef CONFIG_KEXEC
	if (kexec_case) {
		kexec_sleep(cpu);
		pr_err("kexec not handled, core%d failed to sleep\n", cpu);
		kexec_case = 0;
		BUG();
	}
#endif

	/*
	 * Go for a easy to wake-up WFI
	 * for the secondary cores !
	 */
	cpu_enter_lowpower();

	platform_do_lowpower(cpu, &spurious);
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	/*
	 * We're ready for shutdown now, so do it.
	 * First try a lpm base powerdown of the
	 * sec core. If error happens, fall back
	 * to the ram based WFI
	 */
	platform_do_lowpower_lpm(cpu);

	/*
	 * Control reaching here means sec core is
	 * unable to enter the die state through
	 * lpm. We will fallback to the ram
	 * based WFI
	 */
	cpu_enter_lowpower();

	platform_do_lowpower_ram(cpu, &spurious);
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */

	/*
	 * bring this CPU back into the world of cache
	 * coherency, and then restore interrupts
	 */
	cpu_leave_lowpower();

	if (spurious)
		pr_warn("CPU%u: %u spurious wakeup calls\n", cpu, spurious);
}

#endif /* CONFIG_HOTPLUG_CPU */

struct smp_operations __initdata sti_smp_ops = {
	.smp_prepare_cpus	= sti_smp_prepare_cpus,
	.smp_secondary_init	= sti_secondary_init,
	.smp_boot_secondary	= sti_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= sti_cpu_die,
#endif
};
