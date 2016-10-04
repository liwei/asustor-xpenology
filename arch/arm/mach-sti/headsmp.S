/*
 *  arch/arm/mach-sti/headsmp.S
 *
 * Copyright (C) 2013 STMicroelectronics (R&D) Limited.
 *		http://www.st.com
 *
 * Cloned from linux/arch/arm/mach-vexpress/headsmp.S
 *
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>
#include <linux/init.h>

#define MPIDR_HWID_BITMASK 0xFFF
#define A9MP_ICCICR_REG_OFFSET   0x100
#define A9MP_ICCPMR_REG_OFFSET   0x104
#define ACTLR_SMP_SHIFT          6
#define ACTLR_SMP_MASK           (1 << ACTLR_SMP_SHIFT)

/*
 * ST specific entry point for secondary CPUs.  This provides
 * a "holding pen" into which all secondary cores are held until we're
 * ready for them to initialise.
 */
ENTRY(sti_secondary_startup)
	mrc	p15, 0, r1, c1, c0, 1
	bic	r1, r1, #(ACTLR_SMP_MASK)
	mcr	p15, 0, r1, c1, c0, 1
	bl      v7_invalidate_l1

	/*
	 * Enable CPU interface interrupt fwding for the
	 * current secondary core
	 */
	mrc	p15, 4, r1, c15, c0, 0
	mov	r2, #1
	str	r2, [r1, #A9MP_ICCICR_REG_OFFSET]
	mov	r2, #0xff
	str	r2, [r1, #A9MP_ICCPMR_REG_OFFSET]

	/* Read the current secondary core number that we are running on ! */
	mrc	p15, 0, r0, c0, c0, 5
	ldr     r7, =MPIDR_HWID_BITMASK
	and	r0, r0, r7

	/*
	 * Read the linux kernel global pen holding variable and
	 * compare it with the current CPU number.
	 * If matches, go ahead to boot the current CPU, otherwise
	 * wait for the next turn. While waiting fall on to a WFI
	 */
	adr	r4, 1f
	ldmia	r4, {r5, r6}
	sub	r4, r4, r5
	add	r6, r6, r4
pen:	ldr	r7, [r6]
	cmp	r7, r0
	beq	out
	wfi
	b	pen

	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
out:	b	secondary_startup
ENDPROC(sti_secondary_startup)
	.align
1:	.long	.
	.long	pen_release
#ifdef CONFIG_SYNO_LSP_MONACO_SDK2_15_4
ENTRY(sti_abap_c_start)
	wfi
	adr	r6, slave_jump_add
2:	ldr	r5, [r6]
	cmp     r5, #(~0)
	beq	2b
	bx	r5
slave_jump_add:
	.word ~0
ENDPROC(sti_abap_c_start)
stm_abap_c_end:
	.globl sti_abap_c_size
sti_abap_c_size:
	.long stm_abap_c_end - sti_abap_c_start

#endif /* CONFIG_SYNO_LSP_MONACO_SDK2_15_4 */
