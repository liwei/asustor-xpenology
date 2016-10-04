/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *	   Sudeep Biswas	  <sudeep.biswas@st.com>
 *	   Laurent MEUNIER	  <laurent.meunier@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 * ------------------------------------------------------------------------- */

#ifndef __STI_SUSPEND_H__
#define __STI_SUSPEND_H__

#include <linux/suspend.h>
#include <linux/list.h>

#define STI_SUSPEND_DESC_LEN 32

#define MAX_SUSPEND_STATE 2

#define MAX_GIC_SPI_INT 255

#define MAX_DDR_PCTL_NUMBER 2

/*
 * Max of 4 TLB entries are lockable in Cortex A9.
 * This means 4 different memory mapped devices can be
 * accessed after the DDR is in self-refresh, which
 * should be enough. This is to avoid pagetable walk
 * after the DDR is in self-refresh.
 */
#define MAX_TLB_LOCK_ENTRY 4

/*  detailed DDR (lmi) possible states */
enum ddr_low_power_state {
	DDR_ON,
	DDR_SR,
	DDR_OFF
};

enum clocks_state {
	CLOCKS_ON,
	CLOCK_REDUCED,
	CLOCKS_OFF /* clocks off also means power off */
};

struct sti_suspend_table {
	unsigned long base_address;
	long *enter;
	unsigned long enter_size;
	long *exit;
	unsigned long exit_size;
	struct list_head node;
};

/*
 * struct sti_suspend_buffer_data is used by CPS
 * entering code in assembly language.
 * table_enter: virtual address of poke table used for entry
 * table_exit: virtual address of poke table used for exit
 * sti_buffer_code: virtual address of code running from cache locked buffer
 * pokeloop: virtual address of pokeloop code copied to cache locked buffer
 * sti_locking_code: virtual address of code that locks the buffer in cache
 * sz: size of the code and data that needs to be locked in cache
 * nr_tlb_lock: number of TLB entries to be locked in TLB lockdown (max 4)
 * tlb_lock_addr[]: array containing virtual addresses whose translation is
 * to be locked in TLB to avoid page table walk after DDR is in self refresh
 */
struct sti_suspend_buffer_data {
	void *table_enter;
	void *table_exit;
	void *sti_buffer_code;
	void *pokeloop;
	void *sti_locking_code;
	unsigned long sz;
	unsigned int nr_tlb_lock;
	unsigned int tlb_lock_addr[MAX_TLB_LOCK_ENTRY];
};

/*
 * struct sti_platform_suspend is the singleton object representing the
 * suspend framework
 */
struct sti_platform_suspend {
	void __iomem *l2cachebase;
	u32 l2waymask;
	struct sti_hw_state_desc *hw_states;
	int hw_state_nr;
	int index;
	struct platform_suspend_ops const ops;
};

/**
 * struct sti_low_power_syscfg_info - low power entry fields for secure chip
 * @ddr3_cfg_offset:	DDR3_0 Config Register offset in SYSCFG_CORE
 * @ddr3_stat_offset:	DDRSS 0 Status Register offset in SYSCFG_CORE
 * @cfg_shift:		DDR3SS0_PCTL_C_SYSREQ bit shift in DDR3_0 Config Reg
 * @stat_shift:		DDR3SS0_PCTL_C_SYSACK bit shift in DDRSS 0 Status Reg
 */
struct sti_low_power_syscfg_info {
	int ddr3_cfg_offset;
	int ddr3_stat_offset;
	int cfg_shift;
	int stat_shift;
};

struct sti_ddr3_low_power_info {
	struct sti_low_power_syscfg_info sysconf_info;
	unsigned long sysconf_base;
};

/*
 * struct sti_hw_state_desc is the structure representing the
 * low power states. Currently two low power states are supported
 * namely HPS and CPS. So suspend framework has 2 objects for the
 * same.
 */
struct sti_hw_state_desc {
	int target_state;
	char desc[STI_SUSPEND_DESC_LEN];
	enum ddr_low_power_state ddr_state;
	enum clocks_state clock_state;
	struct list_head state_tables;
	unsigned char *cache_buffer;
	struct sti_suspend_buffer_data buffer_data;
	int init_time_prepare;
	int mmu_on_while_ddr_off;

	int (*setup)(struct sti_hw_state_desc *,
		     unsigned long *,
		     unsigned int,
		     struct sti_ddr3_low_power_info *);

	int (*enter)(struct sti_hw_state_desc *);
	int (*prepare)(struct sti_hw_state_desc *);
	void *state_private_data;
};

/* Helper function */
unsigned int sti_get_l2cc_way_size(void);

/* declaration of the entry functions for HPS mode */
int sti_hps_setup(struct sti_hw_state_desc *, unsigned long *, unsigned int,
		  struct sti_ddr3_low_power_info *);

int sti_hps_enter(struct sti_hw_state_desc *);

int sti_hps_prepare(struct sti_hw_state_desc *);

/* declaration of the entry functions for CPS mode */
int sti_cps_setup(struct sti_hw_state_desc *, unsigned long *, unsigned int,
		  struct sti_ddr3_low_power_info *);
int sti_cps_enter(struct sti_hw_state_desc *);

int sti_cps_prepare(struct sti_hw_state_desc *);

#endif
