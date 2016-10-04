/*
 * Copyright (C) 2014  STMicroelectronics
 * Author: Sudeep Biswas <sudeep.biswas@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/linkage.h>
#include <asm/cp15.h>

/* Important offsets in the sti_suspend_buffer_data structure */
#define POKELOOP_CODE_ADD_OFFSET		0xc
#define LOCKING_CODE_ADD_OFFSET			0x10
#define TLB_LOCK_ENTRIES_ADD_OFFSET		0x1c

#define TLB_LOCKDOWN_ENTRY_A9_REG_P_BIT		0x1
#define TLB_LOCKDOWN_A9_REG_NEXT_ENTRY		0x10000000

#define	L2CC_CLEAN_INVALIDATE_WAY_REG_OFFSET	0x7fc
#define L2CC_D_LOCKDOWN0_REG_OFFSET		0x900
#define L2CC_I_LOCKDOWN0_REG_OFFSET		0x904

/* L2CC cache line size is always fixed and it is 32 bytes */
#define L2CC_LINE_SIZE_BYTES			32

ENTRY(sti_cps_exec)
	ldr	r1, =sti_suspend	@r1 => &sti_suspend
	ldr	r2, [r1], #4		@r2 => L2 va base, r1 => &way mask

	ldr	r11, [r1]		@r11 => way mask
	mov	r1, r2			@r1 => L2 base

	add	r2, r0, #(LOCKING_CODE_ADD_OFFSET)

	ldm     r2, {r4 - r6}	@r4 => sti_locking_code,r6 => nr_tlb_lock
	add	r2, r0, #(TLB_LOCK_ENTRIES_ADD_OFFSET)	@r2 => &tlb_lock_addr[]

	/* Invalidate MAIN TLB */
	mov	r3, #0x0
	mcr	p15, 0, r3, c8, c3, 0	@Main TLB & Micro TLB invalidated
	dsb
	isb

	movs	r5, r6		@r5 = r6
	beq	exit_raw	@if r6 == 0, no TLB entries to be locked, exit

	/*
	 * r3 will contain the reg value that locks the next pagetable walk
	 * results on to the first lockable TLB entry. Incrementing
	 * r3 by r7 will result in a value in r3 that will lock
	 * the next pagetable walk reuslts on to the second lockable
	 * TLB entry and so on. Maximum of four TLB lockdown
	 * entries are possible
	 */
	mov	r3, #(TLB_LOCKDOWN_ENTRY_A9_REG_P_BIT)
	ldr	r7, =(TLB_LOCKDOWN_A9_REG_NEXT_ENTRY)
	mov	r10, #0x0

tlb_lock_loop:
	ldr	r8, [r2], #4	@r8=>next va whose va <-> pa to be locked in TLB
	dsb

	/*
	 * Next TLB miss will trigger a page table walk and the resultant
	 * mapping will be stored in the TLB lockable entry indicated  by r3
	 */
	mcr     p15, 0, r3, c10, c0, 0	@r3 => next TLB lockdown address
	dsb
	isb

	/*
	 * Force a dummy read from the va (contained in r8) so that page table
	 * walk occurs and the resultant va<->pa mapping is locked into
	 * the next TLB lockdown entry indicated by r3.
	 * The page table walk will surely occur because TLB is invalidated
	 * and after the invalidation, this is the first time when the va
	 * is accessed
	 */
	ldr	r9, [r8]
	dsb

	/*
	 * Reset TLB allocation behaviour, meaning the next TLB entry to be
	 * allocated will be from the normal set associative area and NOT
	 * from the TLB lockdown entries.
	 * This step need to be done after all lockable entries are
	 * taken care. This means only once, just outside the tlb_lock_loop.
	 * But we still do it at the end of each iteration to be sure that
	 * no stray page table walk happens which results in overwriting a
	 * required va<->pa mapping corresponding to our vas stored in
	 * tlb_lock_addr.
	 */
	mcr     p15, 0, r10, c10, c0, 0
	dsb
	isb

	subs	r5, r5, #1	@decrement r5, if all required entries locked
	beq	exit_raw	@exit, otherwise jump back to tlb_lock_loop

	add	r3, r3, r7	@r3 => next TLB lockdown entry
	b	tlb_lock_loop

exit_raw:
	mov	r6, r4

	/*
	 * Copy some important L2CC reg offsets in registers before jumping
	 * to the sti_cps_lock_code, since sti_cps_lock_code cant do that, but
	 * requires these values.
	 * These offsets cant be generated using mov instruction & ldr
	 * must be used. Note that we copy sti_cps_lock_code from
	 * its assembled virtual address to the cache locked buffer before
	 * execution.
	 * Execution of sti_cps_lock_code will fail if we use ldr inside it
	 * that loads values from literal tables. Note that we dont want to
	 * copy the literal tables (assembler generated) along with
	 * sti_cps_lock_code.
	 * So we all together avoid any implicit usage of literal tables in the
	 * sti_cps_lock_code code.
	 */
	ldr     r3, =(L2CC_CLEAN_INVALIDATE_WAY_REG_OFFSET)
	ldr     r4, =(L2CC_D_LOCKDOWN0_REG_OFFSET)
	ldr     r5, =(L2CC_I_LOCKDOWN0_REG_OFFSET)

 THUMB( orr     r6, r6, #1 )
	blx	r6
ENDPROC(sti_cps_exec)

.balign		4
ENTRY(sti_cps_on_buffer)
	/*
	 * runs the poke tables to turn-off the SOC
	 * Do not add ENDPROC since this is copied in
	 * buffer and not called directly
	 */
	blx	r7	@ call the __pokeloop on CPU Cache

1:
	wfi		@ WaitForInterrupt until
			@ the SOC is turned-off
	b 1b

.balign 4
ENTRY(sti_cps_on_buffer_sz)
	.long	. - sti_cps_on_buffer

/**
 * sti_cps_lock_code runs from the cache lockable buffer
 * The data sequence of the cache lockable buffer is:
 * |pokeloop|pokedata_for_entry|pokedata_for_exit|sti_cps_on_buffer|
 * cache line alignment padding|sti_cps_lock_code|
 * sti_cps_lock_code locks the |pokeloop...|cache line alignment padding|
 * part of the buffer to the L2 cache. Note that sti_cps_lock_code will
 * not compete with the code that is been locked
 * (though it lies in the same buffer) because of two reasons:
 * (a) The whole relevant buffer i.e.
 * |pokeloop..|sti_cps_lock_code| can be occupied in a single L2CC way and
 * (b) sti_cps_lock_code code's physical address is higher than
 * |pokeloop...|cache line alignment padding|, with the assurance that no
 * code of sti_cps_lock_code can share any L2CC cache line with
 * |pokeloop...|cache line alignment padding|. In fact the
 * |cache line alignment padding| is there just to ensure this.
 **/
.balign		4
ENTRY(sti_cps_lock_code)
	add	r2, r0, #(POKELOOP_CODE_ADD_OFFSET)

	/*
	 * r7 => buffer start address, r9 => total size of the code/data
	 * starting from buffer start (stored inside r7) to be loaded &
	 * locked in L2CC way 0
	 */
	ldm	r2, {r7 - r9}

	str	r11, [r1, r3]	@load way mask to L2CC_CLEAN_INVALIDATE_WAY_REG
op_not_complete:
	ldr	r2, [r1, r3]	@read back L2CC_CLEAN_INVALIDATE_WAY_REG
	cmp	r2, #0x0	@if 0 => All ways of  L2CC invalidated
	bne	op_not_complete	@otherwise jump back to op_not_complete

	/*
	 * change the way mask so that it indicates that we are going to lock
	 * all the ways of the L2CC except the way 0. For example in case of
	 * 8 ways L2CC, the original mask value will be 0xff. But after
	 * changing, it will contain 0xfe.
	 */
	sub	r3, r11, #0x1

	/*
	 * lock all ways except way 0 for data & instruction w.r.t. core0
	 * access. Other AXI masters are not active at this moment, even if
	 * the lock by all masters is supported.
	 */
	str	r3, [r1, r4]
	str	r3, [r1, r5]

	/*
	 * r2 keep track of how many cache lines to be loaded and then after
	 * locked in L2CC way 0
	 */
	mov	r2, #0x0
preload:
	/*
	 * Force access of buffer cache line so that it is loaded in
	 * L2CC way 0
	 */
	pld	[r7, r2]
	add	r2, r2, #(L2CC_LINE_SIZE_BYTES)	@r2 added by cache line size
	cmp	r2, r9	@compare r2 and r9, r9 contains total size to be loaded
	bne	preload	@if not all code loaded, go back to load next cache line

	/*
	 * r3 indicates that all ways unlocked except way 0
	 * which will be locked
	 */
	mov	r3, #0x1

	/*
	 * lock way 0 and unlock all other for data & instruction w.r.t. core0
	 * access. Other AXI masters are not active at this moment, even if
	 * lock by all masters is supported.
	 */
	str	r3, [r1, r4]
	str	r3, [r1, r5]

	ldm     r0, {r4 - r6}

	mov	r0, r4	@ move __va(enter_table) to be ready
			@ when pokeloop is executed

 THUMB( orr     r6, r6, #1 )
 THUMB( orr     r7, r7, #1 )
	blx	r6	@ call the sti_cps_on_buffer
			@ placed in the L2CC

.balign 4
ENTRY(sti_cps_lock_code_sz)
	.long	. - sti_cps_lock_code

/* sti_defrost_kernel is where cpu0 start executing after CPS wakeup */
ENTRY(sti_defrost_kernel)
	b cpu_resume	@jump to kernel resume that restore the full ARM context
ENDPROC(sti_defrost_kernel)
