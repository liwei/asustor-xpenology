/*
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *    Intel Corporation
 *    2200 Mission College Blvd.
 *    Santa Clara, CA  97052
 *
 */

/*
 * The following code is for Intel Media SOC Gen3 base support.
*/

/*
 * This file contains PCI access simulation code for Intel Media SOC Gen3.
*/


#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <asm/pci_x86.h>

#undef DBG
#undef DEBUG_PCI_SIM

#ifdef DEBUG_PCI_SIM
#define DBG(a...) printk(a)
#else
#define DBG(a...) do {} while (0)
#endif

typedef struct {
   u32 value;
   u32 mask;
} sim_reg_t;

typedef struct {
   int dev;
   int func;
   int reg;
   sim_reg_t sim_reg;
} sim_dev_reg_t;

#define MB (1024 * 1024)
#define KB (1024)
#define SIZE_TO_MASK(size) (~(size - 1))


#define CHICKENBIT_MASK 0x7FE

static sim_dev_reg_t av_dev_reg_fixups[] = {
	{  2, 0, 0x10, { 0, SIZE_TO_MASK(16*MB) } },
	{  2, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{  2, 1, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{  3, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{  4, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{  4, 1, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{  6, 0, 0x10, { 0, SIZE_TO_MASK(512*KB) } },
	{  6, 1, 0x10, { 0, SIZE_TO_MASK(512*KB) } },
	{  6, 2, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{  8, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	{  8, 1, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{  8, 2, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{  9, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	{  9, 0, 0x14, { 0, SIZE_TO_MASK(64*KB) } },
	{ 10, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 10, 0, 0x14, { 0, SIZE_TO_MASK(256*MB) } },
	{ 11, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 0, 0x14, { 0, SIZE_TO_MASK(256) } },

	{ 11, 1, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x18, { 0, SIZE_TO_MASK(256) } },

	{ 11, 3, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 3, 0x14, { 0, SIZE_TO_MASK(256) } },

	{ 11, 4, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 5, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 11, 6, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 7, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{ 12, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{ 12, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 12, 1, 0x10, { 0, SIZE_TO_MASK(1024) } },
	{ 13, 0, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 1, 0x10, { 0, SIZE_TO_MASK(32*KB) } },

	{ 14, 0,    8, { 0x01060100, 0 } },

	{ 14, 0, 0x10, { 0, 0 } },
	{ 14, 0, 0x14, { 0, 0 } },
	{ 14, 0, 0x18, { 0, 0 } },
	{ 14, 0, 0x1C, { 0, 0 } },
	{ 14, 0, 0x20, { 0, 0 } },
	{ 14, 0, 0x24, { 0, SIZE_TO_MASK(0x200) } },

	{ 15, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 15, 0, 0x14, { 0, SIZE_TO_MASK(64*KB) } },

	{ 16, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 16, 0, 0x14, { 0, SIZE_TO_MASK(64*MB) } },
	{ 16, 0, 0x18, { 0, SIZE_TO_MASK(64*MB) } },

	{ 17, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{ 18, 0, 0x10, { 0, SIZE_TO_MASK(1*KB) } }
};

static sim_dev_reg_t av_dev_reg_fixups3[] = {
	{  2, 0, 0x10, { 0, SIZE_TO_MASK(16*MB) } },
	{  2, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{  2, 1, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{  3, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{  4, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{  4, 1, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{  6, 0, 0x10, { 0, SIZE_TO_MASK(512*KB) } },
	{  6, 1, 0x10, { 0, SIZE_TO_MASK(512*KB) } },
	{  6, 2, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{  8, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	{  8, 1, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{  8, 2, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{  9, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	{  9, 0, 0x14, { 0, SIZE_TO_MASK(64*KB) } },
	{ 10, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 10, 0, 0x14, { 0, SIZE_TO_MASK(256*MB) } },
	{ 11, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 0, 0x14, { 0, SIZE_TO_MASK(256) } },

	{ 11, 1, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x18, { 0, SIZE_TO_MASK(256) } },

	{ 11, 3, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 3, 0x14, { 0, SIZE_TO_MASK(256) } },

	{ 11, 4, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 5, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 11, 6, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 7, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{ 12, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{ 12, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 12, 1, 0x10, { 0, SIZE_TO_MASK(1024) } },
	{ 13, 0, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 1, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 2, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 3, 0x10, { 0, SIZE_TO_MASK(32*KB) } },

	{ 14, 0,    8, { 0x01060100, 0 } },

	{ 14, 0, 0x10, { 0, 0 } },
	{ 14, 0, 0x14, { 0, 0 } },
	{ 14, 0, 0x18, { 0, 0 } },
	{ 14, 0, 0x1C, { 0, 0 } },
	{ 14, 0, 0x20, { 0, 0 } },
	{ 14, 0, 0x24, { 0, SIZE_TO_MASK(0x200) } },

	{ 15, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 15, 0, 0x14, { 0, SIZE_TO_MASK(64*KB) } },

	{ 16, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 16, 0, 0x14, { 0, SIZE_TO_MASK(64*MB) } },
	{ 16, 0, 0x18, { 0, SIZE_TO_MASK(64*MB) } },

	{ 17, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{ 18, 0, 0x10, { 0, SIZE_TO_MASK(1*KB) } },

	{ 27, 0, 0x10, { 0, SIZE_TO_MASK(256) } },


	{ 27, 0, 0x14, { 0, SIZE_TO_MASK(256) } }
};

/*
 * This table is for Gen 5 fixing
 */
static sim_dev_reg_t av_dev_reg_fixups5[] = {
	{  2, 0, 0x10, { 0, SIZE_TO_MASK(16*MB) } },
	{  2, 0, 0x14, { 0, SIZE_TO_MASK(256) } },

        /* Multi-Function Decoder */
	{  3, 0, 0x10, { 0, SIZE_TO_MASK(16*KB) } },
	{  3, 0, 0x14, { 0, SIZE_TO_MASK(4*KB) } },
	{  3, 0, 0x18, { 0, SIZE_TO_MASK(4*KB) } },
	{  3, 0, 0x1C, { 0, SIZE_TO_MASK(4*KB) } },

	{  4, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },

	{  6, 0, 0x10, { 0, SIZE_TO_MASK(512*KB) } },
	{  6, 1, 0x10, { 0, SIZE_TO_MASK(512*KB) } },
	{  6, 2, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{  8, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	{  8, 1, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{  8, 2, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{  9, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	{  9, 0, 0x14, { 0, SIZE_TO_MASK(64*KB) } },

	{ 10, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 10, 0, 0x14, { 0, SIZE_TO_MASK(256*MB) } },

        /* UART */
	{ 11, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 11, 0, 0x18, { 0, SIZE_TO_MASK(256) } },
        /* GPIO */
	{ 11, 1, 0x10, { 0, SIZE_TO_MASK(256) } },
	/* I2C */
	{ 11, 2, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x18, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x1C, { 0, SIZE_TO_MASK(256) } },
        /* Smart Card */
	{ 11, 3, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 3, 0x14, { 0, SIZE_TO_MASK(256) } },
        /* SPI Master */
	{ 11, 4, 0x10, { 0, SIZE_TO_MASK(256) } },
	/* MSPOD */
	{ 11, 5, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	/* PWM */
	{ 11, 6, 0x10, { 0, SIZE_TO_MASK(256) } },
	/* DFV */
	{ 11, 7, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{ 12, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{ 12, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 12, 1, 0x10, { 0, SIZE_TO_MASK(1024) } },

	/* USB */
	{ 13, 0, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 1, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 2, 0x10, { 0, SIZE_TO_MASK(32*KB) } },

	{ 16, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 16, 0, 0x14, { 0, SIZE_TO_MASK(64*MB) } },
	{ 16, 0, 0x18, { 0, SIZE_TO_MASK(64*MB) } },

	{ 17, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },

	{ 18, 0, 0x10, { 0, SIZE_TO_MASK(1*KB) } },

	{ 19, 0, 0x10, { 0, SIZE_TO_MASK(1*KB) } },

	{ 20, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },

	/* SPI Slave */
	{ 21, 0, 0x10, { 0, SIZE_TO_MASK(256) } },

	{ 22, 0, 0x10, { 0, SIZE_TO_MASK(256*KB) } },
	{ 22, 0, 0x14, { 0, SIZE_TO_MASK(64*KB) } },

	/* SPI Flash */
	{ 23, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 23, 0, 0x14, { 0, SIZE_TO_MASK(64*MB) } },
	{ 23, 0, 0x18, { 0, SIZE_TO_MASK(256) } },

	{ 24, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{ 25, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },

	{ 26, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{ 27, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 27, 0, 0x14, { 0, SIZE_TO_MASK(256) } }
};

/*
 * This table is for ce2600 fixing
 */
static sim_dev_reg_t av_dev_reg_fixups_ce2600[] = {
	/* L2 Switch-DMA0*/
	{  0, 0, 0x10, { 0, SIZE_TO_MASK(512) } },
	/* L2 Switch-DMA1*/
	{  1, 0, 0x10, { 0, SIZE_TO_MASK(512) } },

	{  4, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	/* Docsis-DMA*/
	{  5, 0, 0x10, { 0, SIZE_TO_MASK(512) } },

	{  9, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	{  9, 0, 0x14, { 0, SIZE_TO_MASK(64*KB) } },
	/* UART */
	{ 11, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 11, 0, 0x18, { 0, SIZE_TO_MASK(256) } },
    /* GPIO */
	{ 11, 1, 0x10, { 0, SIZE_TO_MASK(256) } },
	/* I2C */
	{ 11, 2, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x18, { 0, SIZE_TO_MASK(256) } },
	{ 11, 2, 0x1C, { 0, SIZE_TO_MASK(256) } },
    /* Smart Card */
	{ 11, 3, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 11, 3, 0x14, { 0, SIZE_TO_MASK(256) } },
	/* MSPOD */
	{ 11, 5, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	/* PWM */
	{ 11, 6, 0x10, { 0, SIZE_TO_MASK(256) } },
	/* DFV */
	{ 11, 7, 0x10, { 0, SIZE_TO_MASK(64*KB) } },

	{ 12, 0, 0x10, { 0, SIZE_TO_MASK(128*KB) } },
	{ 12, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	{ 12, 1, 0x10, { 0, SIZE_TO_MASK(1024) } },

	/* USB */
	{ 13, 0, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 1, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	{ 13, 2, 0x10, { 0, SIZE_TO_MASK(32*KB) } },
	/* Mutex */
	{ 14, 0, 0x10, { 0, SIZE_TO_MASK(2*KB) } },
	/* Watchdog Timer */
	{ 15, 0, 0x10, { 0, SIZE_TO_MASK(256) } },

	{ 16, 0, 0x10, { 0, SIZE_TO_MASK(64*KB) } },
	{ 16, 0, 0x14, { 0, SIZE_TO_MASK(64*MB) } },
	{ 16, 0, 0x18, { 0, SIZE_TO_MASK(64*MB) } },

	{ 20, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	/* SPI Flash */
	{ 23, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 23, 0, 0x14, { 0, SIZE_TO_MASK(64*MB) } },
	{ 23, 0, 0x18, { 0, SIZE_TO_MASK(256) } },

	{ 27, 0, 0x10, { 0, SIZE_TO_MASK(256) } },
	{ 27, 0, 0x14, { 0, SIZE_TO_MASK(256) } },
	/* L2 Switch */
	{ 29, 0, 0x10, { 0, SIZE_TO_MASK(256*KB) } },
	/* MoCA */
	{ 30, 0, 0x10, { 0, SIZE_TO_MASK(1*MB) } },
	/* Docsis */
	{ 31, 0, 0x10, { 0, SIZE_TO_MASK(128*MB) } }
};

static const int num_av_dev_reg_fixups = sizeof(av_dev_reg_fixups) / sizeof(av_dev_reg_fixups[0]);
static const int num_av_dev_reg_fixups3 = sizeof(av_dev_reg_fixups3) / sizeof(av_dev_reg_fixups3[0]);
static const int num_av_dev_reg_fixups5 = sizeof(av_dev_reg_fixups5) / sizeof(av_dev_reg_fixups5[0]);
static const int num_av_dev_reg_fixups_ce2600 = sizeof(av_dev_reg_fixups_ce2600) / sizeof(av_dev_reg_fixups_ce2600[0]);

static u32 sata_cfg_phys_addr = 0;


static void init_sim_regs(void) {
	int i;

	pci_direct_conf1.read(0, 1, PCI_DEVFN(14, 0), 0x10, 4, &sata_cfg_phys_addr);
	for (i = 0; i < num_av_dev_reg_fixups; i++) {
		if (av_dev_reg_fixups[i].dev == 14) {
			if (av_dev_reg_fixups[i].reg == 0x24) {
                                /* SATA AHCI base address has an offset 0x400 from the SATA base
                                 * physical address.
                                 */
				av_dev_reg_fixups[i].sim_reg.value = sata_cfg_phys_addr + 0x400;
			}
		} else {
			pci_direct_conf1.read(0, 1, PCI_DEVFN(av_dev_reg_fixups[i].dev,
			av_dev_reg_fixups[i].func), av_dev_reg_fixups[i].reg, 4,
			&av_dev_reg_fixups[i].sim_reg.value);
		}
	}
}

static void init_sim_regs3(void) {
	int i;

	pci_direct_conf1.read(0, 1, PCI_DEVFN(14, 0), 0x10, 4, &sata_cfg_phys_addr);
	for (i = 0; i < num_av_dev_reg_fixups3; i++) {
		if (av_dev_reg_fixups3[i].dev == 14) {
			if (av_dev_reg_fixups3[i].reg == 0x24) {
                                /* SATA AHCI base address has an offset 0x400 from the SATA base
                                 * physical address.
                                 */
				av_dev_reg_fixups3[i].sim_reg.value = sata_cfg_phys_addr + 0x400;
			}
		} else {
			pci_direct_conf1.read(0, 1, PCI_DEVFN(av_dev_reg_fixups3[i].dev,
			av_dev_reg_fixups3[i].func), av_dev_reg_fixups3[i].reg, 4,
			&av_dev_reg_fixups3[i].sim_reg.value);
		}
	}
}

static void init_sim_regs5(void) {
	int i;

	for (i = 0; i < num_av_dev_reg_fixups5; i++) {
		pci_direct_conf1.read(0, 1, PCI_DEVFN(av_dev_reg_fixups5[i].dev,
		av_dev_reg_fixups5[i].func), av_dev_reg_fixups5[i].reg, 4,
		&av_dev_reg_fixups5[i].sim_reg.value);
	}
}

static void init_sim_regs_ce2600(void) {
	int i;

	for (i = 0; i < num_av_dev_reg_fixups_ce2600; i++) {
		pci_direct_conf1.read(0, 1, PCI_DEVFN(av_dev_reg_fixups_ce2600[i].dev,
		av_dev_reg_fixups_ce2600[i].func), av_dev_reg_fixups_ce2600[i].reg, 4,
		&av_dev_reg_fixups_ce2600[i].sim_reg.value);
	}
}


sim_reg_t *get_sim_reg(unsigned int bus, unsigned int devfn, int reg, int len) {
	unsigned int dev;
	unsigned int func;
	int i;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	/* A/V bridge devices are on bus 1. */
	if (bus == 1) {
		for (i = 0; i < num_av_dev_reg_fixups; i++) {
			if ((reg & ~3) == av_dev_reg_fixups[i].reg
				&& dev == av_dev_reg_fixups[i].dev
				&& func == av_dev_reg_fixups[i].func)
			{
				return &av_dev_reg_fixups[i].sim_reg;
			}
		}
	}

   return NULL;
}

sim_reg_t *get_sim_reg3(unsigned int bus, unsigned int devfn, int reg, int len) {
	unsigned int dev;
	unsigned int func;
	int i;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	/* A/V bridge devices are on bus 1. */
	if (bus == 1) {
		for (i = 0; i < num_av_dev_reg_fixups3; i++) {
			if ((reg & ~3) == av_dev_reg_fixups3[i].reg
				&& dev == av_dev_reg_fixups3[i].dev
				&& func == av_dev_reg_fixups3[i].func)
			{
				return &av_dev_reg_fixups3[i].sim_reg;
			}
		}
	}

   return NULL;
}

sim_reg_t *get_sim_reg5(unsigned int bus, unsigned int devfn, int reg, int len) {
	unsigned int dev;
	unsigned int func;
	int i;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	/* A/V bridge devices are on bus 1. */
	if (bus == 1) {
		for (i = 0; i < num_av_dev_reg_fixups5; i++) {
			if ((reg & ~3) == av_dev_reg_fixups5[i].reg
				&& dev == av_dev_reg_fixups5[i].dev
				&& func == av_dev_reg_fixups5[i].func)
			{
				return &av_dev_reg_fixups5[i].sim_reg;
			}
		}
	}

   return NULL;
}

sim_reg_t *get_sim_reg_ce2600(unsigned int bus, unsigned int devfn, int reg, int len) {
	unsigned int dev;
	unsigned int func;
	int i;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	/* A/V bridge devices are on bus 1. */
	if (bus == 1) {
		for (i = 0; i < num_av_dev_reg_fixups_ce2600; i++) {
			if ((reg & ~3) == av_dev_reg_fixups_ce2600[i].reg
				&& dev == av_dev_reg_fixups_ce2600[i].dev
				&& func == av_dev_reg_fixups_ce2600[i].func)
			{
				return &av_dev_reg_fixups_ce2600[i].sim_reg;
			}
		}
	}

   return NULL;
}


static inline void extract_bytes(u32 *value, int reg, int len) {
	uint32_t mask;

	*value >>= ((reg & 3) * 8);
	mask = 0xFFFFFFFF >> ((4 - len) * 8);
	*value &= mask;
}

static int gen3_conf_read(unsigned int seg, unsigned int bus,
			  unsigned int devfn, int reg, int len, u32 *value)
{
	unsigned long flags;
	unsigned int dev;
	unsigned int func;
	u32 av_bridge_base;
	u32 av_bridge_limit;
	int retval;
	sim_reg_t *sim_reg = NULL;
	int simulated_read;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	simulated_read = true;
	retval = 0;

	sim_reg = get_sim_reg(bus, devfn, reg, len);

	if (sim_reg != NULL) {
		raw_spin_lock_irqsave(&pci_config_lock, flags);
		*value = sim_reg->value;
		raw_spin_unlock_irqrestore(&pci_config_lock, flags);

                /* EHCI registers has 0x100 offset. */
		if (bus == 1 && dev == 13 && reg == 0x10 && func < 2) {
			if (*value != sim_reg->mask) {
				*value |= 0x100;
			}
		}
		extract_bytes(value, reg, len);
	/* Emulate TDI USB controllers. */
	} else if (bus == 1 && dev == 13 && (func == 0 || func == 1) && ((reg & ~3) == PCI_VENDOR_ID)) {
		*value = 0x0101192E;
		extract_bytes(value, reg, len);
        /* b0:d1:f0 is A/V bridge. */
	} else if (bus == 0 && dev == 1 && func == 0) {
		switch (reg) {

                        /* Make BARs appear to not request any memory. */
			case PCI_BASE_ADDRESS_0:
			case PCI_BASE_ADDRESS_0 + 1:
			case PCI_BASE_ADDRESS_0 + 2:
			case PCI_BASE_ADDRESS_0 + 3:
				*value = 0;
				break;

                        /* Since subordinate bus number register is hardwired
                         * to zero and read only, so do the simulation.
                         */
			case PCI_PRIMARY_BUS:
				if (len == 4) {
					*value = 0x00010100;
				} else {
					simulated_read = false;
				}
				break;

			case PCI_SUBORDINATE_BUS:
				*value = 1;
				break;

			case PCI_MEMORY_BASE:
			case PCI_MEMORY_LIMIT:
                                /* Get the A/V bridge base address. */
				pci_direct_conf1.read(0, 0, PCI_DEVFN(1, 0), PCI_BASE_ADDRESS_0, 4,
					&av_bridge_base);

				av_bridge_limit = av_bridge_base + (512*1024*1024 - 1);
				av_bridge_limit >>= 16;
				av_bridge_limit &= 0xFFF0;

				av_bridge_base >>= 16;
				av_bridge_base &= 0xFFF0;

				if (reg == PCI_MEMORY_LIMIT) {
					*value = av_bridge_limit;
				} else if (len == 2) {
					*value = av_bridge_base;
				} else {
					*value = (av_bridge_limit << 16) | av_bridge_base;
				}
				break;
                        /* Make prefetchable memory limit smaller than prefetchable
                         * memory base, so not claim prefetchable memory space.
                         */
			case PCI_PREF_MEMORY_BASE:
				*value = 0xFFF0;
				break;
			case PCI_PREF_MEMORY_LIMIT:
				*value = 0x0;
				break;
                        /* Make IO limit smaller than IO base, so not claim IO space. */
			case PCI_IO_BASE:
				*value = 0xF0;
				break;
			case PCI_IO_LIMIT:
				*value = 0;
				break;

			default:
				simulated_read = false;
				break;
		}
	} else {
		simulated_read = false;
	}

	if (!simulated_read) {
		retval = pci_direct_conf1.read(seg, bus, devfn, reg, len, value);
	}

	DBG("gen3_conf_read: %2x:%2x.%d[%2X(%d)] = %X\n", bus, dev, func, reg, len, *value);
	return retval;
}

static int gen3_conf_write(unsigned int seg, unsigned int bus,
			   unsigned int devfn, int reg, int len, u32 value)
{
	unsigned long flags;
	int dev;
	int func;
	int retval = -1;
	sim_reg_t *sim_reg = NULL;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);

	DBG("gen3_conf_write: %2x:%2x.%d[%2X(%d)] <- %X\n", bus, dev, func, reg, len, value);

	sim_reg = get_sim_reg(bus, devfn, reg, len);
	if (sim_reg != NULL) {
		raw_spin_lock_irqsave(&pci_config_lock, flags);
		sim_reg->value = (value & sim_reg->mask) | (sim_reg->value & ~sim_reg->mask);
		raw_spin_unlock_irqrestore(&pci_config_lock, flags);
		retval = 0;
	} else if (bus == 0 && dev == 1 && func == 0 && ((reg & ~3) == PCI_BASE_ADDRESS_0)) {
                /* Discard writes to A/V bridge BAR. */
		retval = 0;
	} else {
		retval = pci_direct_conf1.write(seg, bus, devfn, reg, len, value);
	}

	return retval;
}

static int ce_soc_conf_read(unsigned int seg, unsigned int bus,
			  unsigned int devfn, int reg, int len, u32 *value)
{
	unsigned long flags, usb_number = 0;
	u32 id = 0;
	unsigned int dev;
	unsigned int func;
	u32 av_bridge_base;
	u32 av_bridge_limit;
	int retval;
	sim_reg_t *sim_reg = NULL;
	int simulated_read;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	intelce_get_soc_info(&id, NULL);
	simulated_read = true;
	retval = 0;
	switch (id) {
			case CE4200_SOC_DEVICE_ID:
				sim_reg = get_sim_reg3(bus, devfn, reg, len);
				usb_number = 4;
				break;
			case CE5300_SOC_DEVICE_ID:
				sim_reg = get_sim_reg5(bus, devfn, reg, len);
				usb_number = 3;
				break;
			case CE2600_SOC_DEVICE_ID:
			default:
				sim_reg = get_sim_reg_ce2600(bus, devfn, reg, len);
				usb_number = 3;
				break;
	}

	if (sim_reg != NULL) {
		raw_spin_lock_irqsave(&pci_config_lock, flags);
		*value = sim_reg->value;
		raw_spin_unlock_irqrestore(&pci_config_lock, flags);

                /* EHCI registers has 0x100 offset. */
		if (bus == 1 && dev == 13 && reg == 0x10 && func < usb_number) {
			if (*value != sim_reg->mask) {
				*value |= 0x100;
			}
		}
		extract_bytes(value, reg, len);
        /* b0:d1:f0 is A/V bridge. */
	} else if (bus == 0 && dev == 1 && func == 0) {
		switch (reg) {

                        /* Make BARs appear to not request any memory. */
			case PCI_BASE_ADDRESS_0:
			case PCI_BASE_ADDRESS_0 + 1:
			case PCI_BASE_ADDRESS_0 + 2:
			case PCI_BASE_ADDRESS_0 + 3:
				*value = 0;
				break;

                        /* Since subordinate bus number register is hardwired
                         * to zero and read only, so do the simulation.
                         */
			case PCI_PRIMARY_BUS:
				if (len == 4) {
					*value = 0x00010100;
				} else {
					simulated_read = false;
				}
				break;

			case PCI_SUBORDINATE_BUS:
				*value = 1;
				break;

			case PCI_MEMORY_BASE:
			case PCI_MEMORY_LIMIT:
                                /* Get the A/V bridge base address. */
				pci_direct_conf1.read(0, 0, PCI_DEVFN(1, 0), PCI_BASE_ADDRESS_0, 4,
					&av_bridge_base);

				av_bridge_limit = av_bridge_base + (512*1024*1024 - 1);
				av_bridge_limit >>= 16;
				av_bridge_limit &= 0xFFF0;

				av_bridge_base >>= 16;
				av_bridge_base &= 0xFFF0;

				if (reg == PCI_MEMORY_LIMIT) {
					*value = av_bridge_limit;
				} else if (len == 2) {
					*value = av_bridge_base;
				} else {
					*value = (av_bridge_limit << 16) | av_bridge_base;
				}
				break;
                        /* Make prefetchable memory limit smaller than prefetchable
                         * memory base, so not claim prefetchable memory space.
                         */
			case PCI_PREF_MEMORY_BASE:
				*value = 0xFFF0;
				break;
			case PCI_PREF_MEMORY_LIMIT:
				*value = 0x0;
				break;
                        /* Make IO limit smaller than IO base, so not claim IO space. */
			case PCI_IO_BASE:
				*value = 0xF0;
				break;
			case PCI_IO_LIMIT:
				*value = 0;
				break;

			default:
				simulated_read = false;
				break;
		}
	} else {
		simulated_read = false;
	}

	if (!simulated_read) {
		retval = pci_direct_conf1.read(seg, bus, devfn, reg, len, value);
	}

	DBG("ce_soc_conf_read: %2x:%2x.%d[%2X(%d)] = %X\n", bus, dev, func, reg, len, *value);
	return retval;
}

static int ce_soc_conf_write(unsigned int seg, unsigned int bus,
			   unsigned int devfn, int reg, int len, u32 value)
{
	unsigned long flags;
	u32 id = 0;
	int dev;
	int func;
	int retval = -1;
	sim_reg_t *sim_reg = NULL;

	dev = PCI_SLOT(devfn);
	func = PCI_FUNC(devfn);
	intelce_get_soc_info(&id, NULL);
	switch (id) {
			case CE4200_SOC_DEVICE_ID:
				sim_reg = get_sim_reg3(bus, devfn, reg, len);
				break;
			case CE5300_SOC_DEVICE_ID:
				sim_reg = get_sim_reg5(bus, devfn, reg, len);
				break;
			case CE2600_SOC_DEVICE_ID:
			default:
				sim_reg = get_sim_reg_ce2600(bus, devfn, reg, len);
				break;
	}
	if (sim_reg != NULL) {
			raw_spin_lock_irqsave(&pci_config_lock, flags);
			sim_reg->value = (value & sim_reg->mask) | (sim_reg->value & ~sim_reg->mask);
			raw_spin_unlock_irqrestore(&pci_config_lock, flags);
			retval = 0;
	} else if (bus == 0 && dev == 1 && func == 0 && ((reg & ~3) == PCI_BASE_ADDRESS_0)) {
			/* Discard writes to A/V bridge BAR. */
			retval = 0;
	} else {
			retval = pci_direct_conf1.write(seg, bus, devfn, reg, len, value);
	}

	return retval;

}

struct pci_raw_ops gen3_pci_conf = {
	.read =	gen3_conf_read,
	.write = gen3_conf_write,
};
/*We use same structure here for CE4200, CE5300 and CE2600 platform*/
struct pci_raw_ops ce_soc_pci_conf = {
	.read =	ce_soc_conf_read,
	.write = ce_soc_conf_write,
};

static unsigned int soc_device_id, soc_device_rev;
int  intelce_get_soc_info(unsigned int *pid, unsigned int *prev)
{
	if (pid) {
		*pid = soc_device_id;
	}
	if (prev) {
		*prev = soc_device_rev;
	}
	return 0;

}
EXPORT_SYMBOL(intelce_get_soc_info);

static unsigned int soc_board_type = 0;
int intelce_get_board_type(unsigned int *board)
{
	if (board) {
		*board = soc_board_type;
	}
	return 0;
}
EXPORT_SYMBOL(intelce_get_board_type);

int intelce_set_board_type(unsigned int board)
{
	static int init = 0;

	if (init) {
		return 0;
	}
	soc_board_type = board;
	init = 1;
	return 0;
}

static int __init gen3_pci_init(void)
{
	unsigned int pcimode = 0;
	unsigned int *p;
	unsigned int board;

	pci_direct_conf1.read(0, 0, PCI_DEVFN(0, 0), PCI_DEVICE_ID, 2, &soc_device_id);
	pci_direct_conf1.read(0, 0, PCI_DEVFN(0, 0), PCI_REVISION_ID, 1, &soc_device_rev);
	switch (soc_device_id) {
		case CE3100_SOC_DEVICE_ID:
		case CE4100_SOC_DEVICE_ID:
			init_sim_regs();
			raw_pci_ops = &gen3_pci_conf;
			break;
		case CE4200_SOC_DEVICE_ID:
	        /* check if the chicken bit enabled*/
	        pci_direct_conf1.write(0, 0, 0, 0xd0, 4, 0xd00040f0);
			pci_direct_conf1.read(0, 0, 0, 0xd4, 4, &pcimode);
			printk("pcimode=0x%x\n", pcimode);
			if(CHICKENBIT_MASK == (pcimode & CHICKENBIT_MASK)) {
				init_sim_regs3();
			raw_pci_ops = &ce_soc_pci_conf;
			} else if (0!=pcimode) {
				printk("wrong pci mode\n");
			}
			break;
		case CE5300_SOC_DEVICE_ID:
	        /* check if the chicken bit enabled*/
		pci_direct_conf1.write(0, 0, 0, 0xd0, 4, 0x060040f0);
			pci_direct_conf1.read(0, 0, 0, 0xd4, 4, &pcimode);
			printk("pcimode=0x%x\n", pcimode);
			if(CHICKENBIT_MASK == (pcimode & CHICKENBIT_MASK)) {
				init_sim_regs5();
		raw_pci_ops = &ce_soc_pci_conf;
			} else if (0!=pcimode) {
				printk("wrong pci mode\n");
			}
			break;
		case CE2600_SOC_DEVICE_ID:
#define CE2600_SRAM_BASE 0xC8000000
#define CE2600_SRAM_LENGTH 0x20000
			p = (uint32_t *)ioremap_nocache(CE2600_SRAM_BASE, CE2600_SRAM_LENGTH);
			if (p != NULL) {
				board = be32_to_cpu(readl(p + 0x1FF0/4)) - 1;
				intelce_set_board_type(board);
				iounmap(p);
			}
		default:
			/* check if the chicken bit enabled*/
		pci_direct_conf1.write(0, 0, 0, 0xd0, 4, 0x060040f0);
			pci_direct_conf1.read(0, 0, 0, 0xd4, 4, &pcimode);
			printk("pcimode=0x%x\n", pcimode);
			if(CHICKENBIT_MASK == (pcimode & CHICKENBIT_MASK)) {
				init_sim_regs_ce2600();
		raw_pci_ops = &ce_soc_pci_conf;
			} else if (0!=pcimode) {
				printk("wrong pci mode\n");
			}
			break;
	}
	return 0;
}

arch_initcall_sync(gen3_pci_init);
