/*
 * Copyright (C) 2013 STMicroelectronics (R&D) Limited.
 * Author(s): Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "smp.h"
#include "sti.h"

#ifdef CONFIG_FIXED_PHY
#include <linux/phy.h>
#include <linux/phy_fixed.h>

static int __init of_add_fixed_phys(void)
{
	int ret;
	struct device_node *np, *node;
	u32 f_link[4];
	struct fixed_phy_status status = {};

	for_each_node_by_name(np, "stmfp") {
		for_each_child_of_node(np, node) {
			int phy_id;

			ret = of_property_read_u32_array(node,
							 "fixed-link",
							 f_link, 4);
			if (ret < 0)
				continue;

			status.link = 1;
			status.duplex = f_link[0];
			status.speed = f_link[1];
			status.pause = f_link[2];
			status.asym_pause = f_link[3];

			of_property_read_u32(node, "st,phy-addr", &phy_id);
			ret = fixed_phy_add(PHY_POLL, phy_id, &status);
			if (ret) {
				of_node_put(np);
				return ret;
			}
		}
	}

	for_each_node_by_name(np, "dwmac") {
		int phy_id;

		ret = of_property_read_u32_array(np, "fixed-link", f_link, 4);
		if (ret < 0)
			return ret;

		status.link = 1;
		status.duplex = f_link[0];
		status.speed = f_link[1];
		status.pause = f_link[2];
		status.asym_pause = f_link[3];

		of_property_read_u32(np, "snps,phy-addr", &phy_id);

		ret = fixed_phy_add(PHY_POLL, phy_id, &status);
		if (ret) {
			of_node_put(np);
			return ret;
		}
	}
	return 0;
}
arch_initcall(of_add_fixed_phys);
#endif /* CONFIG_FIXED_PHY */

void __init sti_l2x0_init(void)
{
	u32 way_size = 0x4;
	u32 aux_ctrl;

	if (of_machine_is_compatible("st,stid127"))
		way_size = 0x3;

	/* may be this can be encoded in macros like BIT*() */
	aux_ctrl = (0x1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) |
		(0x1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) |
		(0x1 << L2X0_AUX_CTRL_INSTR_PREFETCH_SHIFT) |
		(way_size << L2X0_AUX_CTRL_WAY_SIZE_SHIFT);

	l2x0_of_init(aux_ctrl, L2X0_AUX_CTRL_MASK);
}

static void __init sti_timer_init(void)
{
	of_clk_init(NULL);
	clocksource_of_init();
	sti_l2x0_init();
}

static const char *sti_dt_match[] __initdata = {
	"st,stih415",
	"st,stih416",
	"st,stid127",
	"st,stih407",
	"st,stih410",
	NULL
};

#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
void __init sti_init_early(void)
{
#ifdef CONFIG_STM_PCIE_TRACKER_BUG
	extern void stm_hook_ioremap(void);

	if (of_machine_is_compatible("st,stih407")
	    || of_machine_is_compatible("st,stih410"))
		stm_hook_ioremap();
#endif
}
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */

DT_MACHINE_START(STM, "STi SoC with Flattened Device Tree")
	.map_io		= debug_ll_io_init,
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
#else /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	.init_early	= sti_init_early,
#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
	.init_late	= sti_init_machine_late,
	.init_time	= sti_timer_init,
	.init_machine	= sti_init_machine,
	.smp		= smp_ops(sti_smp_ops),
	.dt_compat	= sti_dt_match,
MACHINE_END
