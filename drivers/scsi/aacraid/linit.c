/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.	<alan@redhat.com>
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
 *
 * Copyright (c) 2000-2010 Adaptec, Inc. (aacraid@adaptec.com)
 * Copyright (c) 2010-2015 PMC-Sierra, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Module Name:
 *   linit.c
 *
 * Abstract: Linux Driver entry module for Adaptec RAID Array Controller
 */


#include <linux/version.h> /* for the following test */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
#include <linux/compat.h>
#endif
#if (!defined(UTS_RELEASE) && ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)) && !defined(__VMKLNX__))
#include <linux/utsrelease.h>
#endif
#if (defined(HAS_COMPILE_H) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)) && !defined(CONFIG_COMMUNITY_KERNEL) && !defined(UTS_MACHINE))
#include <linux/compile.h>
#endif
#include <linux/blkdev.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
#include <linux/moduleparam.h>
#else
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#endif
#include <linux/pci.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
#include <linux/aer.h>
#endif
#include <linux/slab.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
#include <linux/smp_lock.h>
#else
#include <linux/mutex.h>
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
#include <linux/pci-aspm.h>
#endif
#include <linux/spinlock.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
#if (!defined(__VMKLNX30__) && !defined(__VMKLNX__))
#include <linux/syscalls.h>
#else
#if defined(__ESX5__)
#include "vmklinux_9/vmklinux_scsi.h"
#else
#include "vmklinux26/vmklinux26_scsi.h"
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
#include <linux/ioctl32.h>
#endif
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)) || defined(SCSI_HAS_SSLEEP)
#include <linux/delay.h>
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) || defined(HAS_KTHREAD))
#include <linux/kthread.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#include <scsi/scsi.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,1)) && !defined(FAILED))
#define SUCCESS 0x2002
#define FAILED  0x2003
#endif
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && defined(DID_BUS_BUSY) && !defined(BLIST_NO_ULD_ATTACH))
#include <scsi/scsi_devinfo.h>	/* Pick up BLIST_NO_ULD_ATTACH? */
#endif
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_eh.h>
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && !defined(BLIST_NO_ULD_ATTACH))
#define no_uld_attach inq_periph_qual
#elif ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(BLIST_NO_ULD_ATTACH))
#define no_uld_attach hostdata
#endif
#else
#include "scsi.h"
#include "hosts.h"
#include "sd.h"
#define no_uld_attach hostdata
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(SCSI_HAS_DUMP))
#include "scsi_dump.h"
#endif
#include <linux/blk.h>	/* for io_request_lock definition */
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
#if (((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT)) && !defined(HAS_BOOT_CONFIG))
#if ((KERNEL_VERSION(2,4,19) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,21)))
# include <asm-x86_64/ioctl32.h>
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# include <asm/ioctl32.h>
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
# include <linux/ioctl32.h>
#endif
  /* Cast the function, since sys_ioctl does not match */
# define aac_ioctl32(x,y) register_ioctl32_conversion((x), \
    (int(*)(unsigned int,unsigned int,unsigned long,struct file*))(y))
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# include <asm/uaccess.h>
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN)))
#include <linux/reboot.h>
#endif

// in aacraid.h
#if (!defined(CONFIG_COMMUNITY_KERNEL) && !defined(CONFIG_DISKDUMP) && !defined(CONFIG_DISKDUMP_MODULE) && !defined(CONFIG_CRASH_DUMP) && !defined(CONFIG_CRASH_DUMP_MODULE))
#undef SCSI_HAS_DUMP
#endif
#if (defined(HAS_KDUMP_CONFIG))
#undef SCSI_HAS_DUMP
#endif
#if (defined(SCSI_HAS_DUMP))
#if (defined(HAS_DISKDUMP_H))
#include <linux/diskdump.h>
#endif
#if (defined(lkcd_dump_mode) && !defined(crashdump_mode))
# define crashdump_mode() lkcd_dump_mode()
#endif
//
static void aac_poll(struct scsi_device *);
#if (!defined(HAS_DUMP_SSLEEP) && (defined(HAS_DISKDUMPLIB_H) || defined(crashdump_mode)) && (defined(CONFIG_DISKDUMP) || defined(CONFIG_DISKDUMP_MODULE) || defined(CONFIG_CRASH_DUMP) || defined(CONFIG_CRASH_DUMP_MODULE)))

#if (defined(HAS_DISKDUMPLIB_H))
#include <linux/diskdumplib.h>
#endif

/* partial compat.h, prior to aacraid.h's pollution of definitions */
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)) && !defined(SCSI_HAS_SSLEEP))
#undef ssleep
#define ssleep(x) scsi_sleep((x)*HZ)
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(HAS_MSLEEP))
#define msleep(x) set_current_state(TASK_UNINTERRUPTIBLE); schedule_timeout(x)
#endif
#if (!defined(HAS_DUMP_MDELAY))
#include <linux/delay.h>
#endif

void aac_diskdump_msleep(unsigned int msecs)
{
	if (crashdump_mode()) {
#if (defined(HAS_DUMP_MDELAY))
		diskdump_mdelay(msecs);
#else
		mdelay(msecs);
#endif
#if (defined(HAS_DISKDUMPLIB_H))
		diskdump_update();
#endif
	} else {
		msleep(msecs);
	}
}

#include <linux/nmi.h>

void aac_diskdump_ssleep(unsigned int seconds)
{
	unsigned int i;

	if (crashdump_mode()) {
		for (i = 0; i < seconds; i++) {
#if (defined(HAS_DUMP_MDELAY))
			diskdump_mdelay(1000);
#else
			mdelay(1000);
#endif
#if (!defined(HAS_DUMP_MDELAY))
			touch_nmi_watchdog();
#endif
#if (defined(HAS_DISKDUMPLIB_H))
			diskdump_update();
#endif
		}
	} else {
		ssleep(seconds);
	}
}

#if (defined(HAS_DUMP_MDELAY))
void aac_diskdump_mdelay(unsigned int msec)
{
	diskdump_mdelay(msec);
}
#endif
#endif
#endif
#include "aacraid.h"
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#include "fwdebug.h"
#endif

#if (defined(__VMKLNX__))
#define ESX4X_AAC_DRIVER_VERSION	"1.2.1"
#else
#define AAC_DRIVER_VERSION		"1.2.1"
#endif

#ifndef AAC_DRIVER_BRANCH
#define AAC_DRIVER_BRANCH		""
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
#define AAC_DRIVER_BUILD_DATE		__DATE__
#else
#define AAC_DRIVER_BUILD_DATE		__DATE__ " " __TIME__
#endif
#define AAC_DRIVERNAME			"aacraid"

#if (defined(AAC_DRIVER_BUILD))
#define _str(x) #x
#define str(x) _str(x)
#if (defined(__VMKLNX__))
#define AAC_DRIVER_FULL_VERSION ESX4X_AAC_DRIVER_VERSION "." str(AAC_DRIVER_BUILD)
#else
#define AAC_DRIVER_FULL_VERSION	AAC_DRIVER_VERSION "." str(AAC_DRIVER_BUILD) "" AAC_DRIVER_BRANCH
#endif
#else
#define AAC_DRIVER_FULL_VERSION	AAC_DRIVER_VERSION AAC_DRIVER_BRANCH " " AAC_DRIVER_BUILD_DATE
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
spinlock_t io_request_lock;
#endif

MODULE_AUTHOR("Red Hat Inc and Adaptec");
MODULE_DESCRIPTION("Dell PERC2, 2/Si, 3/Si, 3/Di, "
		   "Adaptec Advanced Raid Products, "
		   "HP NetRAID-4M, IBM ServeRAID & ICP SCSI driver");
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,7))
MODULE_LICENSE("GPL");
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || defined(MODULE_VERSION))
MODULE_VERSION(AAC_DRIVER_FULL_VERSION);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
static DEFINE_MUTEX(aac_mutex);
#endif
#if (defined(AAC_CSMI))
extern struct list_head aac_devices;
LIST_HEAD(aac_devices);
#else
#if (defined(CONFIG_COMMUNITY_KERNEL))
static LIST_HEAD(aac_devices);
#else
extern struct list_head aac_devices;
LIST_HEAD(aac_devices); /* fwprint */
#endif
#endif
#if (!defined(HAS_BOOT_CONFIG))
static int aac_cfg_major = -1;
#endif
char aac_driver_version[] = AAC_DRIVER_FULL_VERSION;

/*
 * Because of the way Linux names scsi devices, the order in this table has
 * become important.  Check for on-board Raid first, add-in cards second.
 *
 * Note: The last field is used to index into aac_drivers below.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
#ifdef DECLARE_PCI_DEVICE_TABLE
static DECLARE_PCI_DEVICE_TABLE(aac_pci_tbl) = {
#elif (defined(__devinitconst))
static const struct pci_device_id aac_pci_tbl[] __devinitconst = {
#else
static const struct pci_device_id aac_pci_tbl[] __devinitdata = {
#endif
#else
static const struct pci_device_id aac_pci_tbl[] = {
#endif
	{ 0x1028, 0x0001, 0x1028, 0x0001, 0, 0, 0 }, /* PERC 2/Si (Iguana/PERC2Si) */
	{ 0x1028, 0x0002, 0x1028, 0x0002, 0, 0, 1 }, /* PERC 3/Di (Opal/PERC3Di) */
	{ 0x1028, 0x0003, 0x1028, 0x0003, 0, 0, 2 }, /* PERC 3/Si (SlimFast/PERC3Si */
	{ 0x1028, 0x0004, 0x1028, 0x00d0, 0, 0, 3 }, /* PERC 3/Di (Iguana FlipChip/PERC3DiF */
	{ 0x1028, 0x0002, 0x1028, 0x00d1, 0, 0, 4 }, /* PERC 3/Di (Viper/PERC3DiV) */
	{ 0x1028, 0x0002, 0x1028, 0x00d9, 0, 0, 5 }, /* PERC 3/Di (Lexus/PERC3DiL) */
	{ 0x1028, 0x000a, 0x1028, 0x0106, 0, 0, 6 }, /* PERC 3/Di (Jaguar/PERC3DiJ) */
	{ 0x1028, 0x000a, 0x1028, 0x011b, 0, 0, 7 }, /* PERC 3/Di (Dagger/PERC3DiD) */
	{ 0x1028, 0x000a, 0x1028, 0x0121, 0, 0, 8 }, /* PERC 3/Di (Boxster/PERC3DiB) */
	{ 0x9005, 0x0283, 0x9005, 0x0283, 0, 0, 9 }, /* catapult */
	{ 0x9005, 0x0284, 0x9005, 0x0284, 0, 0, 10 }, /* tomcat */
	{ 0x9005, 0x0285, 0x9005, 0x0286, 0, 0, 11 }, /* Adaptec 2120S (Crusader) */
	{ 0x9005, 0x0285, 0x9005, 0x0285, 0, 0, 12 }, /* Adaptec 2200S (Vulcan) */
	{ 0x9005, 0x0285, 0x9005, 0x0287, 0, 0, 13 }, /* Adaptec 2200S (Vulcan-2m) */
	{ 0x9005, 0x0285, 0x17aa, 0x0286, 0, 0, 14 }, /* Legend S220 (Legend Crusader) */
	{ 0x9005, 0x0285, 0x17aa, 0x0287, 0, 0, 15 }, /* Legend S230 (Legend Vulcan) */

	{ 0x9005, 0x0285, 0x9005, 0x0288, 0, 0, 16 }, /* Adaptec 3230S (Harrier) */
	{ 0x9005, 0x0285, 0x9005, 0x0289, 0, 0, 17 }, /* Adaptec 3240S (Tornado) */
	{ 0x9005, 0x0285, 0x9005, 0x028a, 0, 0, 18 }, /* ASR-2020ZCR SCSI PCI-X ZCR (Skyhawk) */
	{ 0x9005, 0x0285, 0x9005, 0x028b, 0, 0, 19 }, /* ASR-2025ZCR SCSI SO-DIMM PCI-X ZCR (Terminator) */
	{ 0x9005, 0x0286, 0x9005, 0x028c, 0, 0, 20 }, /* ASR-2230S + ASR-2230SLP PCI-X (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x028d, 0, 0, 21 }, /* ASR-2130S (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x029b, 0, 0, 22 }, /* AAR-2820SA (Intruder) */
	{ 0x9005, 0x0286, 0x9005, 0x029c, 0, 0, 23 }, /* AAR-2620SA (Intruder) */
	{ 0x9005, 0x0286, 0x9005, 0x029d, 0, 0, 24 }, /* AAR-2420SA (Intruder) */
	{ 0x9005, 0x0286, 0x9005, 0x029e, 0, 0, 25 }, /* ICP9024RO (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x029f, 0, 0, 26 }, /* ICP9014RO (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x02a0, 0, 0, 27 }, /* ICP9047MA (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x02a1, 0, 0, 28 }, /* ICP9087MA (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x02a3, 0, 0, 29 }, /* ICP5445AU (Hurricane44) */
	{ 0x9005, 0x0285, 0x9005, 0x02a4, 0, 0, 30 }, /* ICP9085LI (Marauder-X) */
	{ 0x9005, 0x0285, 0x9005, 0x02a5, 0, 0, 31 }, /* ICP5085BR (Marauder-E) */
	{ 0x9005, 0x0286, 0x9005, 0x02a6, 0, 0, 32 }, /* ICP9067MA (Intruder-6) */
	{ 0x9005, 0x0287, 0x9005, 0x0800, 0, 0, 33 }, /* Themisto Jupiter Platform */
	{ 0x9005, 0x0200, 0x9005, 0x0200, 0, 0, 33 }, /* Themisto Jupiter Platform */
	{ 0x9005, 0x0286, 0x9005, 0x0800, 0, 0, 34 }, /* Callisto Jupiter Platform */
	{ 0x9005, 0x0285, 0x9005, 0x028e, 0, 0, 35 }, /* ASR-2020SA SATA PCI-X ZCR (Skyhawk) */
	{ 0x9005, 0x0285, 0x9005, 0x028f, 0, 0, 36 }, /* ASR-2025SA SATA SO-DIMM PCI-X ZCR (Terminator) */
	{ 0x9005, 0x0285, 0x9005, 0x0290, 0, 0, 37 }, /* AAR-2410SA PCI SATA 4ch (Jaguar II) */
	{ 0x9005, 0x0285, 0x1028, 0x0291, 0, 0, 38 }, /* CERC SATA RAID 2 PCI SATA 6ch (DellCorsair) */
	{ 0x9005, 0x0285, 0x9005, 0x0292, 0, 0, 39 }, /* AAR-2810SA PCI SATA 8ch (Corsair-8) */
	{ 0x9005, 0x0285, 0x9005, 0x0293, 0, 0, 40 }, /* AAR-21610SA PCI SATA 16ch (Corsair-16) */
	{ 0x9005, 0x0285, 0x9005, 0x0294, 0, 0, 41 }, /* ESD SO-DIMM PCI-X SATA ZCR (Prowler) */
	{ 0x9005, 0x0285, 0x103C, 0x3227, 0, 0, 42 }, /* AAR-2610SA PCI SATA 6ch */
	{ 0x9005, 0x0285, 0x9005, 0x0296, 0, 0, 43 }, /* ASR-2240S (SabreExpress) */
	{ 0x9005, 0x0285, 0x9005, 0x0297, 0, 0, 44 }, /* ASR-4005 */
	{ 0x9005, 0x0285, 0x1014, 0x02F2, 0, 0, 45 }, /* IBM 8i (AvonPark) */
	{ 0x9005, 0x0285, 0x1014, 0x0312, 0, 0, 45 }, /* IBM 8i (AvonPark Lite) */
	{ 0x9005, 0x0286, 0x1014, 0x9580, 0, 0, 46 }, /* IBM 8k/8k-l8 (Aurora) */
	{ 0x9005, 0x0286, 0x1014, 0x9540, 0, 0, 47 }, /* IBM 8k/8k-l4 (Aurora Lite) */
	{ 0x9005, 0x0285, 0x9005, 0x0298, 0, 0, 48 }, /* ASR-4000 (BlackBird) */
	{ 0x9005, 0x0285, 0x9005, 0x0299, 0, 0, 49 }, /* ASR-4800SAS (Marauder-X) */
	{ 0x9005, 0x0285, 0x9005, 0x029a, 0, 0, 50 }, /* ASR-4805SAS (Marauder-E) */
	{ 0x9005, 0x0286, 0x9005, 0x02a2, 0, 0, 51 }, /* ASR-3800 (Hurricane44) */

	{ 0x9005, 0x0285, 0x1028, 0x0287, 0, 0, 52 }, /* Perc 320/DC*/
	{ 0x1011, 0x0046, 0x9005, 0x0365, 0, 0, 53 }, /* Adaptec 5400S (Mustang)*/
	{ 0x1011, 0x0046, 0x9005, 0x0364, 0, 0, 54 }, /* Adaptec 5400S (Mustang)*/
	{ 0x1011, 0x0046, 0x9005, 0x1364, 0, 0, 55 }, /* Dell PERC2/QC */
	{ 0x1011, 0x0046, 0x103c, 0x10c2, 0, 0, 56 }, /* HP NetRAID-4M */

	{ 0x9005, 0x0285, 0x1028, PCI_ANY_ID, 0, 0, 57 }, /* Dell Catchall */
	{ 0x9005, 0x0285, 0x17aa, PCI_ANY_ID, 0, 0, 58 }, /* Legend Catchall */
	{ 0x9005, 0x0285, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 59 }, /* Adaptec Catch All */
	{ 0x9005, 0x0286, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 60 }, /* Adaptec Rocket Catch All */
	{ 0x9005, 0x0288, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 61 }, /* Adaptec NEMER/ARK Catch All */
	{ 0x9005, 0x028b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 62 }, /* Adaptec PMC Series 6 (Tupelo) */
	{ 0x9005, 0x028c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 63 }, /* Adaptec PMC Series 7 (Denali) */
	{ 0x9005, 0x028d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 64 }, /* Adaptec PMC Series 8 */
	{ 0x9005, 0x028f, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 65 }, /* Adaptec PMC Series 9 */
	{ 0,}
};
MODULE_DEVICE_TABLE(pci, aac_pci_tbl);

/*
 * dmb - For now we add the number of channels to this structure.
 * In the future we should add a fib that reports the number of channels
 * for the card.  At that time we can remove the channels from here
 */
static struct aac_driver_ident aac_drivers[] = {
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 2/Si (Iguana/PERC2Si) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Opal/PERC3Di) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Si (SlimFast/PERC3Si */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Iguana FlipChip/PERC3DiF */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Viper/PERC3DiV) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Lexus/PERC3DiL) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 1, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Jaguar/PERC3DiJ) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Dagger/PERC3DiD) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Boxster/PERC3DiB) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "catapult        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* catapult */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "tomcat          ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* tomcat */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2120S   ", 1, AAC_QUIRK_31BIT | AAC_QUIRK_34SG },		       /* Adaptec 2120S (Crusader) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2200S   ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG },		       /* Adaptec 2200S (Vulcan) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2200S   ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Adaptec 2200S (Vulcan-2m) */
	{ aac_rx_init, "aacraid",  "Legend  ", "Legend S220     ", 1, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Legend S220 (Legend Crusader) */
	{ aac_rx_init, "aacraid",  "Legend  ", "Legend S230     ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Legend S230 (Legend Vulcan) */

	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 3230S   ", 2 }, /* Adaptec 3230S (Harrier) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 3240S   ", 2 }, /* Adaptec 3240S (Tornado) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2020ZCR     ", 2 }, /* ASR-2020ZCR SCSI PCI-X ZCR (Skyhawk) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2025ZCR     ", 2 }, /* ASR-2025ZCR SCSI SO-DIMM PCI-X ZCR (Terminator) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "ASR-2230S PCI-X ", 2 }, /* ASR-2230S + ASR-2230SLP PCI-X (Lancer) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "ASR-2130S PCI-X ", 1 }, /* ASR-2130S (Lancer) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "AAR-2820SA      ", 1 }, /* AAR-2820SA (Intruder) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "AAR-2620SA      ", 1 }, /* AAR-2620SA (Intruder) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "AAR-2420SA      ", 1 }, /* AAR-2420SA (Intruder) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9024RO       ", 2 }, /* ICP9024RO (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9014RO       ", 1 }, /* ICP9014RO (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9047MA       ", 1 }, /* ICP9047MA (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9087MA       ", 1 }, /* ICP9087MA (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP5445AU       ", 1 }, /* ICP5445AU (Hurricane44) */
	{ aac_rx_init, "aacraid",  "ICP     ", "ICP9085LI       ", 1 }, /* ICP9085LI (Marauder-X) */
	{ aac_rx_init, "aacraid",  "ICP     ", "ICP5085BR       ", 1 }, /* ICP5085BR (Marauder-E) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9067MA       ", 1 }, /* ICP9067MA (Intruder-6) */
	{ NULL        , "aacraid",  "ADAPTEC ", "Themisto        ", 0, AAC_QUIRK_SLAVE }, /* Jupiter Platform */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "Callisto        ", 2, AAC_QUIRK_MASTER }, /* Jupiter Platform */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2020SA       ", 1 }, /* ASR-2020SA SATA PCI-X ZCR (Skyhawk) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2025SA       ", 1 }, /* ASR-2025SA SATA SO-DIMM PCI-X ZCR (Terminator) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2410SA SATA ", 1, AAC_QUIRK_17SG }, /* AAR-2410SA PCI SATA 4ch (Jaguar II) */
	{ aac_rx_init, "aacraid",  "DELL    ", "CERC SR2        ", 1, AAC_QUIRK_17SG }, /* CERC SATA RAID 2 PCI SATA 6ch (DellCorsair) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2810SA SATA ", 1, AAC_QUIRK_17SG }, /* AAR-2810SA PCI SATA 8ch (Corsair-8) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-21610SA SATA", 1, AAC_QUIRK_17SG }, /* AAR-21610SA PCI SATA 16ch (Corsair-16) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2026ZCR     ", 1 }, /* ESD SO-DIMM PCI-X SATA ZCR (Prowler) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2610SA      ", 1 }, /* SATA 6Ch (Bearcat) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2240S       ", 1 }, /* ASR-2240S (SabreExpress) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4005        ", 1 }, /* ASR-4005 */
	{ aac_rx_init, "ServeRAID","IBM     ", "ServeRAID 8i    ", 1 }, /* IBM 8i (AvonPark) */
	{ aac_rkt_init, "ServeRAID","IBM     ", "ServeRAID 8k-l8 ", 1 }, /* IBM 8k/8k-l8 (Aurora) */
	{ aac_rkt_init, "ServeRAID","IBM     ", "ServeRAID 8k-l4 ", 1 }, /* IBM 8k/8k-l4 (Aurora Lite) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4000        ", 1 }, /* ASR-4000 (BlackBird & AvonPark) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4800SAS     ", 1 }, /* ASR-4800SAS (Marauder-X) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4805SAS     ", 1 }, /* ASR-4805SAS (Marauder-E) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "ASR-3800        ", 1 }, /* ASR-3800 (Hurricane44) */

	{ aac_rx_init, "percraid", "DELL    ", "PERC 320/DC     ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG }, /* Perc 320/DC*/
	{ aac_sa_init, "aacraid",  "ADAPTEC ", "Adaptec 5400S   ", 4, AAC_QUIRK_34SG }, /* Adaptec 5400S (Mustang)*/
	{ aac_sa_init, "aacraid",  "ADAPTEC ", "AAC-364         ", 4, AAC_QUIRK_34SG }, /* Adaptec 5400S (Mustang)*/
	{ aac_sa_init, "percraid", "DELL    ", "PERCRAID        ", 4, AAC_QUIRK_34SG }, /* Dell PERC2/QC */
	{ aac_sa_init, "hpnraid",  "HP      ", "NetRAID         ", 4, AAC_QUIRK_34SG }, /* HP NetRAID-4M */

	{ aac_rx_init, "aacraid",  "DELL    ", "RAID            ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Dell Catchall */
	{ aac_rx_init, "aacraid",  "Legend  ", "RAID            ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Legend Catchall */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "RAID            ", 2 }, /* Adaptec Catch All */
	{ aac_rkt_init, "aacraid", "ADAPTEC ", "RAID            ", 2 }, /* Adaptec Rocket Catch All */
	{ aac_nark_init, "aacraid", "ADAPTEC ", "RAID            ", 2 }, /* Adaptec NEMER/ARK Catch All */
	{ aac_src_init, "aacraid", "ADAPTEC ", "RAID            ", 2 }, /* Adaptec PMC Series 6 (Tupelo) */
	{ aac_srcv_init, "aacraid", "ADAPTEC ", "RAID            ", 2 }, /* Adaptec PMC Series 7 (Denali) */
	{ aac_srcv_init, "aacraid", "ADAPTEC ", "RAID            ", 2 }, /* Adaptec PMC Series 8 */
	{ aac_srcv_init, "aacraid", "ADAPTEC ", "RAID            ", 2 } /* Adaptec PMC Series 9 */
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))

#if (((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT)) && !defined(HAS_BOOT_CONFIG))
/*
 * Promote 32 bit apps that call get_next_adapter_fib_ioctl to 64 bit version
 */
static int aac_get_next_adapter_fib_ioctl(unsigned int fd, unsigned int cmd,
		unsigned long arg, struct file *file)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	struct fib_ioctl f;
	mm_segment_t fs;
	int retval;

	memset (&f, 0, sizeof(f));
	if (copy_from_user(&f, (void __user *)arg, sizeof(f) - sizeof(u32)))
		return -EFAULT;
	fs = get_fs();
	set_fs(get_ds());
	retval = sys_ioctl(fd, cmd, (unsigned long)&f);
	set_fs(fs);
	return retval;
#else
	struct fib_ioctl __user *f;

	f = compat_alloc_user_space(sizeof(*f));
	if (!access_ok(VERIFY_WRITE, f, sizeof(*f)))
		return -EFAULT;

	clear_user(f, sizeof(*f));
	if (copy_in_user(f, (void __user *)arg, sizeof(*f) - sizeof(u32)))
		return -EFAULT;

	return sys_ioctl(fd, cmd, (unsigned long)f);
#endif
}
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#define sys_ioctl NULL	/* register_ioctl32_conversion defaults to this when NULL passed in as a handler */
#endif
#endif

#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0))
#if (!defined(SCSI_HAS_SCSI_IN_DETECTION) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
static struct Scsi_Host * aac_dummy;
#endif

/**
 *	aac_detect	-	Probe for aacraid cards
 *	@template: SCSI driver template
 *
 *	This is but a stub to convince the 2.4 scsi layer to scan targets,
 *	the pci scan has already picked up the adapters.
 */
static int aac_detect(Scsi_Host_Template *template)
{
    adbg_init(dev,KERN_INFO, "aac_detect(%p)\n", template);
#if (defined(__VMKERNEL_MODULE__) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)))
	if (!VmkLinux_SetModuleVersion(AAC_DRIVERNAME " (" AAC_DRIVER_FULL_VERSION ")")) {
		return 0;
	}
#endif
#if (!defined(SCSI_HAS_SCSI_IN_DETECTION) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	/* By changing the host list we trick a scan */
	if (aac_dummy) {
		adbg_init(dev,KERN_INFO,"scsi_host_put(%p)\n", aac_dummy);
		scsi_host_put(aac_dummy);
		aac_dummy = NULL;
	}
	adbg_init(dev,KERN_INFO,"aac_detect()=%d\n", !list_empty(&aac_devices));
	return !list_empty(&aac_devices);
#else
	return template->present = 1;
#endif
}

#endif

#if (defined(__VMKERNEL_MODULE__) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)))
static void __devexit aac_remove_one(struct pci_dev *pdev);

static int aac_release(struct Scsi_Host *sh) {
	struct aac_dev *aac = (struct aac_dev *)sh->hostdata;

	if (aac)
		aac_remove_one(aac->pdev);
	return 0;
}
#endif
/**
 *	aac_queuecommand	-	queue a SCSI command
 *	@cmd:		SCSI command to queue
 *	@done:		Function to call on command completion
 *
 *	Queues a command for execution by the associated Host Adapter.
 *
 *	TODO: unify with aac_scsi_cmd().
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
static int aac_queuecommand(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
#else
static int aac_queuecommand(struct Scsi_Host *shost, struct scsi_cmnd *cmd)
#endif
{
        adbg_queuecmd_debug_timing(cmd);

#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	if (dev->shutdown) {
		printk(KERN_INFO
		  "aac_queuecommand(%p={.cmnd[0]=%x},%p) post-shutdown\n",
		  cmd, cmd->cmnd[0], done);
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aac_queuecommand(%p={.cmnd[0]=%x},%p) post-shutdown\n",
		  cmd, cmd->cmnd[0], done));
	}
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
	cmd->scsi_done = done;
#endif


#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,6)) && !defined(SCSI_DEVICE_HAS_TIMEOUT))
	if ((cmd->eh_state != SCSI_STATE_QUEUED)
	 && (cmd->device->type == TYPE_DISK)
	 && (sdev_channel(cmd->device) == CONTAINER_CHANNEL)
	 && (cmd->eh_timeout.expires < (60 * HZ))) {
		/* The controller is doing error recovery */
		mod_timer(&cmd->eh_timeout, 60 * HZ);
	}
#endif
	cmd->SCp.phase = AAC_OWNER_LOWLEVEL;
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	return (aac_adapter_scsi_cmd(dev, cmd) ? FAILED : 0);
#else
	return (aac_scsi_cmd(cmd) ? FAILED : 0);
#endif
}


/**
 *	aac_info		-	Returns the host adapter name
 *	@shost:		Scsi host to report on
 *
 *	Returns a static string describing the device in question
 */

static const char *aac_info(struct Scsi_Host *shost)
{
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0))
	struct aac_dev *dev;
#if (!defined(SCSI_HAS_SCSI_IN_DETECTION) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	if (shost == aac_dummy)
		return shost->hostt->name;
#endif
	dev = (struct aac_dev *)shost->hostdata;
	if (!dev
	 || (dev->cardtype >= (sizeof(aac_drivers)/sizeof(aac_drivers[0]))))
		return shost->hostt->name;
	if (dev->scsi_host_ptr != shost)
		return shost->hostt->name;
#else
	struct aac_dev *dev = (struct aac_dev *)shost->hostdata;
#endif
	return aac_drivers[dev->cardtype].name;
}

/**
 *	aac_get_driver_ident
 *	@devtype: index into lookup table
 *
 *	Returns a pointer to the entry in the driver lookup table.
 */

struct aac_driver_ident* aac_get_driver_ident(int devtype)
{
	return &aac_drivers[devtype];
}

/**
 *	aac_biosparm	-	return BIOS parameters for disk
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
 *	@disk: SCSI disk object to process
 *	@device: kdev_t of the disk in question
#endif
 *	@sdev: The scsi device corresponding to the disk
 *	@bdev: the block device corresponding to the disk
 *	@capacity: the sector capacity of the disk
 *	@geom: geometry block to fill in
 *
 *	Return the Heads/Sectors/Cylinders BIOS Disk Parameters for Disk.
 *	The default disk geometry is 64 heads, 32 sectors, and the appropriate
 *	number of cylinders so as not to exceed drive capacity.  In order for
 *	disks equal to or larger than 1 GB to be addressable by the BIOS
 *	without exceeding the BIOS limitation of 1024 cylinders, Extended
 *	Translation should be enabled.   With Extended Translation enabled,
 *	drives between 1 GB inclusive and 2 GB exclusive are given a disk
 *	geometry of 128 heads and 32 sectors, and drives above 2 GB inclusive
 *	are given a disk geometry of 255 heads and 63 sectors.  However, if
 *	the BIOS detects that the Extended Translation setting does not match
 *	the geometry in the partition table, then the translation inferred
 *	from the partition table will be used by the BIOS, and a warning may
 *	be displayed.
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
static int aac_biosparm(struct scsi_device *sdev, struct block_device *bdev,
			sector_t capacity, int *geom)
#else
static int aac_biosparm(Scsi_Disk *disk, kdev_t dev, int *geom)
#endif
{
	struct diskparm *param = (struct diskparm *)geom;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	unsigned char *buf;
#else
	struct buffer_head * buf;
	sector_t capacity = disk->capacity;
#endif

	dprintk((KERN_DEBUG "aac_biosparm.\n"));

	/*
	 *	Assuming extended translation is enabled - #REVISIT#
	 */
	if (capacity >= 2 * 1024 * 1024) { /* 1 GB in 512 byte sectors */
		if(capacity >= 4 * 1024 * 1024) { /* 2 GB in 512 byte sectors */
			param->heads = 255;
			param->sectors = 63;
		} else {
			param->heads = 128;
			param->sectors = 32;
		}
	} else {
		param->heads = 64;
		param->sectors = 32;
	}

	param->cylinders = cap_to_cyls(capacity, param->heads * param->sectors);

	/*
	 *	Read the first 1024 bytes from the disk device, if the boot
	 *	sector partition table is valid, search for a partition table
	 *	entry whose end_head matches one of the standard geometry
	 *	translations ( 64/32, 128/32, 255/63 ).
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	buf = scsi_bios_ptable(bdev);
#else
	buf = bread(MKDEV(MAJOR(dev), MINOR(dev)&~0xf), 0, block_size(dev));
#endif
	if (!buf)
		return 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if(*(__le16 *)(buf + 0x40) == cpu_to_le16(0xaa55)) {
		struct partition *first = (struct partition * )buf;
#else
	if(*(unsigned short *)(buf->b_data + 0x1fe) == cpu_to_le16(0xaa55)) {
		struct partition *first = (struct partition * )(buf->b_data + 0x1be);
#endif
		struct partition *entry = first;
		int saved_cylinders = param->cylinders;
		int num;
		unsigned char end_head, end_sec;

		for(num = 0; num < 4; num++) {
			end_head = entry->end_head;
			end_sec = entry->end_sector & 0x3f;

			if(end_head == 63) {
				param->heads = 64;
				param->sectors = 32;
				break;
			} else if(end_head == 127) {
				param->heads = 128;
				param->sectors = 32;
				break;
			} else if(end_head == 254) {
				param->heads = 255;
				param->sectors = 63;
				break;
			}
			entry++;
		}

		if (num == 4) {
			end_head = first->end_head;
			end_sec = first->end_sector & 0x3f;
		}

		param->cylinders = cap_to_cyls(capacity, param->heads * param->sectors);
		if (num < 4 && end_sec == param->sectors) {
			if (param->cylinders != saved_cylinders)
				dprintk((KERN_DEBUG "Adopting geometry: heads=%d, sectors=%d from partition table %d.\n",
					param->heads, param->sectors, num));
		} else if (end_head > 0 || end_sec > 0) {
			dprintk((KERN_DEBUG "Strange geometry: heads=%d, sectors=%d in partition table %d.\n",
				end_head + 1, end_sec, num));
			dprintk((KERN_DEBUG "Using geometry: heads=%d, sectors=%d.\n",
					param->heads, param->sectors));
		}
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	kfree(buf);
#else
	brelse(buf);
#endif
	return 0;
}

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)) && (LINUX_VERSION_CODE <= KERNEL_VERSION(3,18,0)))
static int aac_slave_alloc(struct scsi_device *sdev)
{

    sdev->tagged_supported = 1;
    scsi_activate_tcq(sdev, sdev->host->can_queue);

    return 0;
}

static void aac_slave_destroy(struct scsi_device *sdev)
{
     scsi_deactivate_tcq(sdev, 1);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
/**
 *	aac_slave_configure		-	compute queue depths
 *	@sdev:	SCSI device we are considering
 *
 *	Selects queue depths for each target device based on the host adapter's
 *	total capacity and the queue depth supported by the target device.
 *	A queue depth of one automatically disables tagged queueing.
 */

static int aac_slave_configure(struct scsi_device *sdev)
{
	struct aac_dev *aac = (struct aac_dev *)sdev->host->hostdata;
	int chn, tid, is_native_device = 0;

	chn = aac_logical_to_phys(sdev_channel(sdev));
	tid = sdev_id(sdev);
	if (chn < AAC_MAX_BUSES && tid < AAC_MAX_TARGETS &&
		aac->hba_map[chn][tid].devtype == AAC_DEVTYPE_NATIVE_RAW)	
		is_native_device = 1;

//#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)) && !defined(BLIST_RETRY_HWERROR))
//	if (sdev_channel(sdev) == CONTAINER_CHANNEL)
//		sdev->retry_hwerror = 1;
//#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(BLIST_NO_ULD_ATTACH))
	sdev->no_uld_attach = 0;
#endif
	if (!is_native_device) {
		if (aac->jbod && (sdev->type == TYPE_DISK))
			sdev->removable = 1;
		if ((sdev->type == TYPE_DISK) &&
			(sdev_channel(sdev) != CONTAINER_CHANNEL) &&
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7))
			(!aac->jbod || sdev->inq_periph_qual) &&
#else
			(!aac->jbod || (sdev->inquiry[0] >> 5)) &&
#endif
			(!aac->raid_scsi_mode || (sdev_channel(sdev) != 2))) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
			if (expose_physicals == 0)
				return -ENXIO;
#endif
			if (expose_physicals < 0)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && !defined(BLIST_NO_ULD_ATTACH))
			{
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(BLIST_NO_ULD_ATTACH))
				sdev->no_uld_attach = (void *)1;
#else
				sdev->no_uld_attach = 1;
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && !defined(BLIST_NO_ULD_ATTACH))
				/* Force it not to match sd */
				sdev->inquiry[0] |= 1 << 5;
				sdev->type = TYPE_DISK | (1 << 5);
			}
#endif
		}
	}

	if (is_native_device || 
		(sdev->tagged_supported && (sdev->type == TYPE_DISK) &&
			(!aac->raid_scsi_mode || (sdev_channel(sdev) != 2)) &&
			!sdev->no_uld_attach)) {
		struct scsi_device * dev;
		struct Scsi_Host *host = sdev->host;
		unsigned num_lsu = 0;
		unsigned num_one = 0;
		unsigned depth;
		unsigned cid;

#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,6)) || defined(SCSI_DEVICE_HAS_TIMEOUT))
		/*
		 * Firmware has an individual device recovery time typically
		 * of 35 seconds, give us a margin.
		 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
		if (sdev->timeout < (45 * HZ))
			sdev->timeout = 45 * HZ;
#else
		if (sdev->request_queue->rq_timeout < (45 * HZ))
			blk_queue_rq_timeout(sdev->request_queue, 45*HZ);
#endif
#endif
		if (!is_native_device) {
			for (cid = 0; cid < aac->maximum_num_containers; ++cid)
				if (aac->fsa_dev[cid].valid)
					++num_lsu;
			__shost_for_each_device(dev, host) {
				if (dev->tagged_supported &&
					(dev->type == TYPE_DISK) &&
					(!aac->raid_scsi_mode ||
						(sdev_channel(sdev) != 2)) &&
					!dev->no_uld_attach) {
				 if ((sdev_channel(dev) != CONTAINER_CHANNEL)
				  || !aac->fsa_dev[sdev_id(dev)].valid)
					++num_lsu;
				} else
				 ++num_one;
			}
			if (num_lsu == 0)
				++num_lsu;
			depth = (host->can_queue - num_one) / num_lsu;
			if (depth > 256)
				depth = 256;
			else if (depth < 2)
				depth = 2;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
			scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, depth);
#else
			scsi_change_queue_depth(sdev, depth);
#endif
		} else {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
			scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, aac->hba_map[chn][tid].qd_limit); 
#else
			scsi_change_queue_depth(sdev, aac->hba_map[chn][tid].qd_limit);
#endif
		}

#if (!defined(__ESX5__) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,24)) && !defined(PCI_HAS_SET_DMA_MAX_SEG_SIZE))
		if (sdev->request_queue) {
			if (!(((struct aac_dev *)host->hostdata)->adapter_info.options &
				AAC_OPT_NEW_COMM))
				blk_queue_max_segment_size(sdev->request_queue, 65536);
			else
				blk_queue_max_segment_size(sdev->request_queue,
					host->max_sectors << 9);
		}
#endif
	} else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
		scsi_adjust_queue_depth(sdev, 0, 1);
#else
		scsi_change_queue_depth(sdev, 1);
#endif

#if (((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,6)) || defined(SCSI_DEVICE_HAS_TIMEOUT)) && defined(AAC_EXTENDED_TIMEOUT))
	{
		extern int extendedtimeout;

		if (extendedtimeout != -1)
			sdev->timeout = extendedtimeout * HZ;
	}
#endif
    sdev->tagged_supported = 1;

	return 0;
}
#else
/**
 *	aac_queuedepth		-	compute queue depths
 *	@host:	SCSI host in question
 *	@dev:	SCSI device we are considering
 *
 *	Selects queue depths for each target device based on the host adapter's
 *	total capacity and the queue depth supported by the target device.
 *	A queue depth of one automatically disables tagged queueing.
 */

static void aac_queuedepth(struct Scsi_Host * host, struct scsi_device * dev )
{
	struct scsi_device * dptr;
	unsigned num_lsu = 0;
	unsigned num_one = 0;
	unsigned depth;
	struct aac_dev *aac = (struct aac_dev *)host->hostdata;
	unsigned cid;

   	adbg_init(KERN_INFO "aac_queuedepth(%p,%p)\n", host, dev);
	for (cid = 0; cid < aac->maximum_num_containers; ++cid)
		if (aac->fsa_dev[cid].valid)
			++num_lsu;
	for(dptr = dev; dptr != NULL; dptr = dptr->next)
		if (dptr->host == host) {
			if ((dev->type == TYPE_DISK) && !dev->no_uld_attach) {
				if ((sdev_channel(dev) != CONTAINER_CHANNEL)
				 || !aac->fsa_dev[sdev_id(dev)].valid)
					++num_lsu;
			} else
				++num_one;
		}

	dprintk((KERN_DEBUG "can_queue=%d num_lsu=%d num_one=%d\n", host->can_queue, num_lsu, num_one));
    	adbg_init(dev,KERN_INFO, "can_queue=%d num_lsu=%d num_one=%d\n", host->can_queue, num_lsu, num_one);

	if (num_lsu == 0)
		++num_lsu;
	depth = (host->can_queue - num_one) / num_lsu;
	if (depth > 255)
		depth = 255;
	else if (depth < 2)
		depth = 2;
	/* Rebalance */
	dprintk((KERN_DEBUG "aac_queuedepth.\n"));
	dprintk((KERN_DEBUG "Device #   Q Depth   Online\n"));
	dprintk((KERN_DEBUG "---------------------------\n"));
	adbg_init(dev,KERN_INFO ,"aac_queuedepth.\n");
	adbg_init(dev,KERN_INFO ,"Device #   Q Depth   Online\n");
	adbg_init(dev,KERN_INFO ,"---------------------------\n");
	for(dptr = dev; dptr != NULL; dptr = dptr->next)
	{
		if(dptr->host == host) {
			if ((dptr->type == TYPE_DISK) && !dptr->no_uld_attach)
				dptr->queue_depth = depth;
			else
				dptr->queue_depth = 1;

			dprintk((KERN_DEBUG "  %2d         %d        %d\n",
			  dptr->id, dptr->queue_depth, scsi_device_online(dptr)));
            		adbg_init(dev,KERN_INFO ," %2d         %d        %d\n",
			  dptr->id, dptr->queue_depth, scsi_device_online(dptr));
		}
	}
}
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

/**
 *	aac_change_queue_depth		-	alter queue depths
 *	@sdev:	SCSI device we are considering
 *	@depth:	desired queue depth
 *
 *	Alters queue depths for target device based on the host adapter's
 *	total capacity and the queue depth supported by the target device.
 */

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)) || (defined(RHEL_MAJOR) && (RHEL_MAJOR == 6 && RHEL_MINOR >= 2)))
static int aac_change_queue_depth(struct scsi_device *sdev, int depth,
				  int reason)
#else
static int aac_change_queue_depth(struct scsi_device *sdev, int depth)
#endif
{
	struct aac_dev *aac = (struct aac_dev *)(sdev->host->hostdata);
	int chn, tid, is_native_device = 0;
	
	chn = aac_logical_to_phys(sdev_channel(sdev));
	tid = sdev_id(sdev);
	if (chn < AAC_MAX_BUSES && tid < AAC_MAX_TARGETS &&
		aac->hba_map[chn][tid].devtype == AAC_DEVTYPE_NATIVE_RAW)	
		is_native_device = 1;
		
	if (sdev->tagged_supported && (sdev->type == TYPE_DISK) &&
	    (sdev_channel(sdev) == CONTAINER_CHANNEL)) {
		struct scsi_device * dev;
		struct Scsi_Host *host = sdev->host;
		unsigned num = 0;

		__shost_for_each_device(dev, host) {
			if (dev->tagged_supported && (dev->type == TYPE_DISK) &&
			    (sdev_channel(dev) == CONTAINER_CHANNEL))
				++num;
			++num;
		}
		if (num >= host->can_queue)
			num = host->can_queue - 1;
		if (depth > (host->can_queue - num))
			depth = host->can_queue - num;
		if (depth > 256)
			depth = 256;
		else if (depth < 2)
			depth = 2;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
		scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, depth);
#else
		scsi_change_queue_depth(sdev, depth);
#endif
	} else if(is_native_device) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
			scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, aac->hba_map[chn][tid].qd_limit);
#else
			scsi_change_queue_depth(sdev, aac->hba_map[chn][tid].qd_limit);
#endif
	} else {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
		scsi_adjust_queue_depth(sdev, 0, 1);
#else
		scsi_change_queue_depth(sdev, 1);
#endif
	}
	return sdev->queue_depth;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))

static ssize_t aac_store_queue_depth(struct device *dev, const char * buf,
  size_t count)
{
	aac_change_queue_depth(to_scsi_device(dev),
	  simple_strtoul(buf, NULL, 10));
	return strlen(buf);
}

static ssize_t aac_show_queue_depth(struct device *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  to_scsi_device(dev)->queue_depth);
}

static struct device_attribute aac_queue_depth_attr = {
	.attr = {
		.name =	"queue_depth",
		.mode = S_IWUSR|S_IRUGO,
	},
	.store = aac_store_queue_depth,
	.show = aac_show_queue_depth
};
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
static ssize_t aac_show_raid_level(struct device *dev, char *buf)
#else
static ssize_t aac_show_raid_level(struct device *dev, struct device_attribute *attr, char *buf)
#endif
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct aac_dev *aac = (struct aac_dev *)(sdev->host->hostdata);
	if (sdev_channel(sdev) != CONTAINER_CHANNEL)
		return snprintf(buf, PAGE_SIZE, sdev->no_uld_attach
		  ? "Hidden\n" :
		  ((aac->jbod && (sdev->type == TYPE_DISK)) ? "JBOD\n" : ""));
	return snprintf(buf, PAGE_SIZE, "%s\n",
	  get_container_type(aac->fsa_dev[sdev_id(sdev)].type));
}

static struct device_attribute aac_raid_level_attr = {
	.attr = {
		.name = "level",
		.mode = S_IRUGO,
	},
	.show = aac_show_raid_level
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
static ssize_t aac_show_unique_id(struct device *dev, char *buf)
#else
static ssize_t aac_show_unique_id(struct device *dev,
	     struct device_attribute *attr, char *buf)
#endif
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct aac_dev *aac = (struct aac_dev *)(sdev->host->hostdata);
	unsigned char sn[16];

	memset(sn, 0, sizeof(sn));

	if (sdev_channel(sdev) == CONTAINER_CHANNEL)
		memcpy(sn, aac->fsa_dev[sdev_id(sdev)].identifier, sizeof(sn));

	return snprintf(buf, 16 * 2 + 2,
		"%02X%02X%02X%02X%02X%02X%02X%02X"
		"%02X%02X%02X%02X%02X%02X%02X%02X\n",
		sn[0], sn[1], sn[2], sn[3],
		sn[4], sn[5], sn[6], sn[7],
		sn[8], sn[9], sn[10], sn[11],
		sn[12], sn[13], sn[14], sn[15]);
}

static struct device_attribute aac_unique_id_attr = {
	.attr = {
		.name = "unique_id",
		.mode = S_IRUGO,
	},
	.show = aac_show_unique_id
};

static struct device_attribute *aac_dev_attrs[] = {
	&aac_raid_level_attr,
	&aac_unique_id_attr,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
	&aac_queue_depth_attr,
#endif
	NULL,
};
#endif

#if (!defined(HAS_BOOT_CONFIG))

static int aac_ioctl(struct scsi_device *sdev, int cmd, void __user * arg)
{
	struct aac_dev *dev = (struct aac_dev *)sdev->host->hostdata;
	int retval;	
	adbg_ioctl(dev, KERN_DEBUG, "aac_ioctl(%p, %x, %p)\n", sdev, cmd, arg);
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	retval = aac_do_ioctl(dev, cmd, arg);
	adbg_ioctl(dev, KERN_DEBUG, "aac_ioctl returns %d\n", retval);
	return retval;
}
#endif

extern void aac_hba_callback(void *context, struct fib * fibptr);


#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(__VMKLNX30__) || defined(__VMKLNX__))
/**
 *	aac_eh_device_reset	-	Reset command handling
 *	@cmd:	SCSI command block causing the reset
 *
 *	Issue a reset of a SCSI device. We are ourselves not truely a SCSI
 *	controller and our firmware will do the work for us anyway. Thus this
 *	is a no-op.
 *	We return SUCCESS to avoid a "vmkfstools -L lunreset ..." failure
 */

static int aac_eh_device_reset(struct scsi_cmnd *cmd)
{
	return SUCCESS;
}

/**
 *	aac_eh_bus_reset	-	Reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 *	Issue a reset of a SCSI bus. We are ourselves not truely a SCSI
 *	controller and our firmware will do the work for us anyway. Thus this
 *	is a no-op.
 *	We return SUCCESS to avoid a "vmkfstools -L busreset ..." failure
 */

static int aac_eh_bus_reset(struct scsi_cmnd* cmd)
{
	return SUCCESS;
}

#endif
#if (defined(__arm__))
//DEBUG
#define AAC_DEBUG_INSTRUMENT_RESET
#endif
static int aac_eh_abort(struct scsi_cmnd* cmd)
{
	struct scsi_device * dev = cmd->device;
	struct Scsi_Host * host = dev->host;
	struct aac_dev * aac = (struct aac_dev *)host->hostdata;
	int count, found;
	u32 bus, cid;
	int ret = FAILED;

	bus = aac_logical_to_phys(scmd_channel(cmd));
	cid = scmd_id(cmd);
	if (bus < AAC_MAX_BUSES && cid < AAC_MAX_TARGETS &&
		aac->hba_map[bus][cid].devtype == AAC_DEVTYPE_NATIVE_RAW) {
		struct fib *fib;
		struct aac_hba_tm_req *tmf;
		int status;
		u64 address;
		__le32 managed_request_id;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
		printk(KERN_ERR "%s: Host adapter abort request (%d,%d,%d,%llu)\n",
		 AAC_DRIVERNAME,
		 host->host_no, sdev_channel(dev), sdev_id(dev), dev->lun);
#else		
		printk(KERN_ERR "%s: Host adapter abort request (%d,%d,%d,%d)\n",
		 AAC_DRIVERNAME,
		 host->host_no, sdev_channel(dev), sdev_id(dev), dev->lun);
#endif
		found = 0;
		for (count = 0; count < (host->can_queue + AAC_NUM_MGT_FIB); ++count) {
			fib = &aac->fibs[count];
			if (*(u8 *)fib->hw_fib_va != 0 &&
				(fib->flags & FIB_CONTEXT_FLAG_NATIVE_HBA) &&
				(fib->callback_data == cmd)) {
				found = 1;
				managed_request_id = ((struct aac_hba_cmd_req *)
					fib->hw_fib_va)->request_id;
				break;
			}
		}
		if (!found)
			return ret;

		/* start a HBA_TMF_ABORT_TASK TMF request */
		if (!(fib = aac_fib_alloc(aac))) {
			return ret;
		}

		tmf = (struct aac_hba_tm_req *)fib->hw_fib_va;
		memset(tmf, 0, sizeof(*tmf));
		tmf->tmf = HBA_TMF_ABORT_TASK;
		tmf->it_nexus = aac->hba_map[bus][cid].rmw_nexus;
		tmf->lun[1] = cmd->device->lun;
		tmf->managed_request_id = managed_request_id;
		
		address = (u64)fib->hw_error_pa;
		tmf->error_ptr_hi = cpu_to_le32((u32)(address >> 32));
		tmf->error_ptr_lo = cpu_to_le32((u32)(address & 0xffffffff));
		tmf->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);
	
		fib->hbacmd_size = sizeof(*tmf);
		cmd->SCp.sent_command = 0;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_unlock_irq(host->host_lock);
#else
		spin_unlock_irq(host->lock);
#endif
#else
		spin_unlock_irq(&io_request_lock);
#endif
#endif
		status = aac_hba_send(HBA_IU_TYPE_SCSI_TM_REQ, fib,
				  (fib_callback) aac_hba_callback,
				  (void *) cmd);

		/* Wait up to 2 minutes for completion */
		for (count = 0; count < 120; ++count) {
			if (cmd->SCp.sent_command) {
				ret = SUCCESS;	
				break;
			}
			msleep(1000);
		}	

		if (ret != SUCCESS)
		 printk(KERN_ERR "%s: Host adapter abort request timed out\n",
			AAC_DRIVERNAME);
	
		/* check status */
		/* ... */

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_lock_irq(host->host_lock);
#else
		spin_lock_irq(host->lock);
#endif
#else
		spin_lock_irq(&io_request_lock);
#endif
#endif

	} else {
#if (defined(AAC_DEBUG_INSTRUMENT_RESET) || (0 && defined(BOOTCD)))
		struct scsi_cmnd * command;
		int active;
		unsigned long DebugFlags;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		unsigned long flags;
#endif
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
		unsigned short saved_queue_depth;
		unsigned int saved_device_blocked;
		/*
	 	 * Bug in these kernels allows commands to come through to 
	 	 * controller. We provide an additional hack, a coarse lock to 
	 	 * prevent new commands from being issued during this delicate
	 	 * phase. We can not use scsi_adjust_queue_depth, and besides,
	 	 * that is not the point, we have to have a test that punts
	 	 * commands back at the lower layers while we are in error 
		 * recovery; where the test for it is embedded
	 	 * in a host_lock deadlock state.
	 	 */
		saved_device_blocked = dev->device_blocked;
		dev->device_blocked = dev->max_device_blocked;
		saved_queue_depth = dev->queue_depth;
		dev->queue_depth = 0;
		if (saved_queue_depth < 2) save_queue_depth =
			aac->fsa_dev[scmd_id(cmd)].queue_depth;
		if (saved_queue_depth < 2)
			saved_queue_depth = 2;
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_unlock_irq(host->host_lock);
#else
		spin_unlock_irq(host->lock);
#endif
#else
		spin_unlock_irq(&io_request_lock);
#endif
#endif
		printk(KERN_ERR
	  		"%s: Host adapter abort request.\n"
	  		"%s: Outstanding commands on (%d,%d,%d,%d):\n",
	  		AAC_DRIVERNAME, AAC_DRIVERNAME,
	  		host->host_no,sdev_channel(dev),sdev_id(dev),dev->lun);

		if (nblank(fwprintf(x))) {
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
		  		"abort request.\n"
		  		"Outstanding commands on (%d,%d,%d,%d):",
		  	host->host_no,sdev_channel(dev),sdev_id(dev),dev->lun));
			DebugFlags = aac->FwDebugFlags;
			aac->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
		}
		active = adbg_dump_command_queue(cmd);

		if (nblank(fwprintf(x)))
			aac->FwDebugFlags = DebugFlags;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		dev->device_blocked = dev->max_device_blocked;
		dev->queue_depth = 0;
#endif
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_lock_irq(host->host_lock);
#else
		spin_lock_irq(host->lock);
#endif
#else
		spin_lock_irq(&io_request_lock);
#endif
#endif
#else

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
		printk(KERN_ERR "%s: Host adapter abort request (%d,%d,%d,%llu)\n",
		 AAC_DRIVERNAME,
		 host->host_no, sdev_channel(dev), sdev_id(dev), dev->lun);
#else
		printk(KERN_ERR "%s: Host adapter abort request (%d,%d,%d,%d)\n",
		 AAC_DRIVERNAME,
		 host->host_no, sdev_channel(dev), sdev_id(dev), dev->lun);
#endif
#endif /* AAC_DEBUG_INSTRUMENT_RESET */
		switch (cmd->cmnd[0]) {
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(SERVICE_ACTION_IN))
		case SERVICE_ACTION_IN:
			if (!(aac->raw_io_interface) ||
		    		!(aac->raw_io_64) ||
		    		((cmd->cmnd[1] & 0x1f) != SAI_READ_CAPACITY_16))
				break;
#endif
		case INQUIRY:
		case READ_CAPACITY:
		 /* Mark associated FIB to not complete, eh handler does this */
		 for (count = 0; count < (host->can_queue + AAC_NUM_MGT_FIB); ++count) {
			struct fib * fib = &aac->fibs[count];
			if (fib->hw_fib_va->header.XferState &&
			  (fib->flags & FIB_CONTEXT_FLAG) &&
			  (fib->callback_data == cmd)) {
				fib->flags |= FIB_CONTEXT_FLAG_TIMED_OUT;
				cmd->SCp.phase = AAC_OWNER_ERROR_HANDLER;
				ret = SUCCESS;
			}
		 }
		 break;
		case TEST_UNIT_READY:
		 /* Mark associated FIB to not complete, eh handler does this */
		 for (count = 0; count < (host->can_queue + AAC_NUM_MGT_FIB); ++count) {
#if (!defined(AAC_DEBUG_INSTRUMENT_RESET) && !(0 && defined(BOOTCD)))
			struct scsi_cmnd * command;
#endif
			struct fib * fib = &aac->fibs[count];
			if ((fib->hw_fib_va->header.XferState & cpu_to_le32(Async | NoResponseExpected)) &&
			  (fib->flags & FIB_CONTEXT_FLAG) &&
			  ((command = fib->callback_data)) &&
			  (command->device == cmd->device)) {
				fib->flags |= FIB_CONTEXT_FLAG_TIMED_OUT;
				command->SCp.phase = AAC_OWNER_ERROR_HANDLER;
				if (command == cmd)
					ret = SUCCESS;
			}
		 }
		}
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
		dev->queue_depth = saved_queue_depth;
		dev->device_blocked = saved_device_blocked;
#endif
#endif /* AAC_DEBUG_INSTRUMENT_RESET */
	}

	return ret;
}

/*
 *	aac_eh_reset	- Reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 */
static int aac_eh_reset(struct scsi_cmnd* cmd)
{
	struct scsi_device * dev = cmd->device;
	struct Scsi_Host * host = dev->host;
	struct aac_dev * aac = (struct aac_dev *)host->hostdata;
	int count;
	u32 bus, cid;
	int ret = FAILED;

	adbg_dump_pending_fibs(aac, cmd);

	bus = aac_logical_to_phys(scmd_channel(cmd));
	cid = scmd_id(cmd);
	if (bus < AAC_MAX_BUSES && cid < AAC_MAX_TARGETS &&
		aac->hba_map[bus][cid].devtype == AAC_DEVTYPE_NATIVE_RAW) {
		struct fib *fib;
		int status;
		u64 address;
		u8 command;

		printk(KERN_ERR "%s: Host adapter reset request. SCSI hang ?\n",
			AAC_DRIVERNAME);
		
		if (!(fib = aac_fib_alloc(aac))) {
			return ret;
		}
		
		if (aac->hba_map[bus][cid].reset_state == 0) {
			struct aac_hba_tm_req *tmf;
		
			/* start a HBA_TMF_LUN_RESET TMF request */
			tmf = (struct aac_hba_tm_req *)fib->hw_fib_va;
			memset(tmf, 0, sizeof(*tmf));
			tmf->tmf = HBA_TMF_LUN_RESET;
			tmf->it_nexus = aac->hba_map[bus][cid].rmw_nexus;
			tmf->lun[1] = cmd->device->lun;
		
			address = (u64)fib->hw_error_pa;
			tmf->error_ptr_hi = cpu_to_le32((u32)(address >> 32));
			tmf->error_ptr_lo = cpu_to_le32((u32)(address & 0xffffffff));
			tmf->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);
			fib->hbacmd_size = sizeof(*tmf);
			
			command = HBA_IU_TYPE_SCSI_TM_REQ;
			aac->hba_map[bus][cid].reset_state++;
		} else if (aac->hba_map[bus][cid].reset_state >= 1) {
			struct aac_hba_reset_req *rst;

			/* already tried, start a hard reset now */
			rst = (struct aac_hba_reset_req *)fib->hw_fib_va;
			memset(rst, 0, sizeof(*rst));
			/* reset_type is already zero... */
			rst->it_nexus = aac->hba_map[bus][cid].rmw_nexus;
		
			address = (u64)fib->hw_error_pa;
			rst->error_ptr_hi = cpu_to_le32((u32)(address >> 32));
			rst->error_ptr_lo = cpu_to_le32((u32)(address & 0xffffffff));
			rst->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);
			fib->hbacmd_size = sizeof(*rst);
			
			command = HBA_IU_TYPE_SATA_REQ;
			aac->hba_map[bus][cid].reset_state = 0;
		}
		cmd->SCp.sent_command = 0;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_unlock_irq(host->host_lock);
#else
		spin_unlock_irq(host->lock);
#endif
#else
		spin_unlock_irq(&io_request_lock);
#endif
#endif
		status = aac_hba_send(command, fib,
				  (fib_callback) aac_hba_callback,
				  (void *) cmd);

		/* Wait up to 2 minutes for completion */
		for (count = 0; count < 120; ++count) {
			if (cmd->SCp.sent_command) {
				ret = SUCCESS;	
				break;
			}
			msleep(1000);
		}	

		if (ret != SUCCESS)
		 printk(KERN_ERR "%s: Host adapter reset request timed out\n",
			AAC_DRIVERNAME);
	
		/* check status */
		/* ... */

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_lock_irq(host->host_lock);
#else
		spin_lock_irq(host->lock);
#endif
#else
		spin_lock_irq(&io_request_lock);
#endif
#endif

	} else {
#if (!defined(AAC_DEBUG_INSTRUMENT_RESET) && defined(__arm__))
		return SUCCESS; /* Cause an immediate retry of the command with a ten second delay after successful tur */
#else
		struct scsi_cmnd * command;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		unsigned long flags;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_RESET) || (0 && defined(BOOTCD)))
		int active;
		unsigned long DebugFlags;
#endif /* AAC_DEBUG_INSTRUMENT_RESET */
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
		unsigned short saved_queue_depth;
		unsigned int saved_device_blocked;
		/*
	 	 * Bug in these kernels allows commands to come through to 
	 	 * controller. We provide an additional hack, a coarse lock to 
	 	 * prevent new commands from being issued during this delicate
	 	 * phase. We can not use scsi_adjust_queue_depth, and besides,
	 	 * that is not the point, we have to have a test that punts
	 	 * commands back at the lower layers while we are in error 
		 * recovery; where the test for it is embedded
	 	 * in a host_lock deadlock state.
	 	 */
		saved_device_blocked = dev->device_blocked;
		dev->device_blocked = dev->max_device_blocked;
		saved_queue_depth = dev->queue_depth;
		dev->queue_depth = 0;
		if (saved_queue_depth < 2) saved_queue_depth =
			aac->fsa_dev[scmd_id(cmd)].queue_depth;
		if (saved_queue_depth < 2)
			saved_queue_depth = 2;
#endif

		/* Mark the assoc. FIB to not complete, eh handler does this */
		for (count = 0; count < (host->can_queue + AAC_NUM_MGT_FIB); ++count) {
			struct fib * fib = &aac->fibs[count];
			if (fib->hw_fib_va->header.XferState &&
		  		(fib->flags & FIB_CONTEXT_FLAG) &&
		  		(fib->callback_data == cmd)) {
				fib->flags |= FIB_CONTEXT_FLAG_TIMED_OUT;
				cmd->SCp.phase = AAC_OWNER_ERROR_HANDLER;
			}
		}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_unlock_irq(host->host_lock);
#else
		spin_unlock_irq(host->lock);
#endif
#else
		spin_unlock_irq(&io_request_lock);
#endif
#endif
		printk(KERN_ERR "%s: Host adapter reset request. SCSI hang ?\n",
			AAC_DRIVERNAME);
#if (!defined(CONFIG_COMMUNITY_KERNEL))
		fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "reset request. SCSI hang ?"));
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_RESET) || (0 && defined(BOOTCD)))
		adbg_dump_pending_fibs(aac, cmd);

		printk(KERN_ERR
	  	 AAC_DRIVERNAME ": Outstanding commands on (%d,%d,%d,%d):\n",
	  	 host->host_no, sdev_channel(dev), sdev_id(dev), dev->lun);
		if (nblank(fwprintf(x))) {
		 DebugFlags = aac->FwDebugFlags;
		 aac->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
		 fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
		  "Outstanding commands on (%d,%d,%d,%d):\n",
		  host->host_no, sdev_channel(dev), sdev_id(dev), dev->lun));
		}
		active = adbg_dump_command_queue(cmd);
		if (nblank(fwprintf(x)))
			aac->FwDebugFlags = DebugFlags;
#endif /* AAC_DEBUG_INSTRUMENT_RESET */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		dev->device_blocked = dev->max_device_blocked;
		dev->queue_depth = 0;
#endif
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_lock_irq(host->host_lock);
#else
		spin_lock_irq(host->lock);
#endif
#else
		spin_lock_irq(&io_request_lock);
#endif
#endif

		if ((count = aac_check_health(aac)))
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
		{
			dev->queue_depth = saved_queue_depth;
			dev->device_blocked = saved_device_blocked;
#endif
			return count;
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
		}
#endif
		/*
	 	 * Wait for all commands to complete to this specific
	 	 * target (block maximum 60 seconds).
	 	 */
		for (count = 60; count; --count) {
			int active = aac->in_reset;

			if (active == 0)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
			__shost_for_each_device(dev, host) {
			 spin_lock_irqsave(&dev->list_lock, flags);
			 list_for_each_entry(command, &dev->cmd_list, list) {
				if ((command != cmd) &&
				    (command->SCp.phase==AAC_OWNER_FIRMWARE)) {
					active++;
					break;
				}
			 }
			 spin_unlock_irqrestore(&dev->list_lock, flags);
			 if (active)
				break;

			}
#else
			for (dev = host->host_queue; dev != (struct scsi_device *)NULL; dev = dev->next) {
			 for(command = dev->device_queue; command; command = command->next) {
				if ((command != cmd)
				 && (command->SCp.phase==AAC_OWNER_FIRMWARE)) {
					++active;
					break;
				}
			 }
			}
#endif
			/*
		 	 * We can exit If all the commands are complete
		 	 */
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
			dev = cmd->device;
#endif
			if (active == 0)
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
			{
				dev->queue_depth = saved_queue_depth;
				dev->device_blocked = saved_device_blocked;
#endif
				return SUCCESS;
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
			}
#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
			spin_unlock_irq(host->host_lock);
#else
			spin_unlock_irq(host->lock);
#endif
#else
			spin_unlock_irq(&io_request_lock);
#endif
#endif
			ssleep(1);
#if (defined(SCSI_HAS_DUMP) && (defined(HAS_DISKDUMPLIB_H) || defined(crashdump_mode)) && (defined(CONFIG_DISKDUMP) || defined(CONFIG_CRASH_DUMP)))
			if (crashdump_mode())
				aac_poll(dev);
#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
			dev->device_blocked = dev->max_device_blocked;
			dev->queue_depth = 0;
#endif
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
			spin_lock_irq(host->host_lock);
#else
			spin_lock_irq(host->lock);
#endif
#else
			spin_lock_irq(&io_request_lock);
#endif
#endif
		}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_unlock_irq(host->host_lock);
#else
		spin_unlock_irq(host->lock);
#endif
#else
		spin_unlock_irq(&io_request_lock);
#endif
#endif
		printk(KERN_ERR "%s: SCSI bus appears hung\n", AAC_DRIVERNAME);
#if (!defined(CONFIG_COMMUNITY_KERNEL))
		fwprintf((aac,HBA_FLAGS_DBG_FW_PRINT_B,"SCSI bus appears hung"));
#endif
		/*
	 	 * This adapter needs a blind reset, only do so for Adapters
	 	 * that support a register, instead of a commanded, reset.
	 	 */
		if (((aac->supplement_adapter_info.SupportedOptions2 & 
         		AAC_OPTION_MU_RESET) || 
         		(aac->supplement_adapter_info.SupportedOptions2 & 
         		AAC_OPTION_DOORBELL_RESET)) &&
	  		aac_check_reset &&
	  		((aac_check_reset != 1) ||
	   		!(aac->supplement_adapter_info.SupportedOptions2 &
	    		AAC_OPTION_IGNORE_RESET)))
			aac_reset_adapter(aac, 2); /* Bypass wait for command quiesce */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		dev->device_blocked = dev->max_device_blocked;
		dev->queue_depth = 0;
#endif
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_lock_irq(host->host_lock);
#else
		spin_lock_irq(host->lock);
#endif
#else
		spin_lock_irq(&io_request_lock);
#endif
#endif
#if ((KERNEL_VERSION(2,5,0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)))
		dev->queue_depth = saved_queue_depth;
		dev->device_blocked = saved_device_blocked;
#endif
		ret = SUCCESS;
#endif
	}
	
	/* Cause an immediate retry of the command with a ten second delay 
	 * after successful tur
	 */
	return ret;
}


#if (defined(SCSI_HAS_DUMP))
#if (defined(SCSI_HAS_DUMP_SANITY_CHECK))

static int aac_sanity_check(struct scsi_device * sdev)
{
	return 0;
}

#endif
static void aac_poll(struct scsi_device * sdev)
{
	struct Scsi_Host * shost = sdev->host;
	struct aac_dev *dev = (struct aac_dev *)shost->hostdata;
	unsigned long flags;

#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
	spin_lock_irqsave(shost->host_lock, flags);
#else
	spin_lock_irqsave(shost->lock, flags);
#endif
#else
	spin_lock_irqsave(&io_request_lock, flags);
#endif
	aac_adapter_intr(dev);
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
	spin_unlock_irqrestore(shost->host_lock, flags);
#else
	spin_unlock_irqrestore(shost->lock, flags);
#endif
#else
	spin_unlock_irqrestore(&io_request_lock, flags);
#endif
}
#endif
#if (!defined(HAS_BOOT_CONFIG))

/**
 *	aac_cfg_open		-	open a configuration file
 *	@inode: inode being opened
 *	@file: file handle attached
 *
 *	Called when the configuration device is opened. Does the needed
 *	set up on the handle and then returns
 *
 *	Bugs: This needs extending to check a given adapter is present
 *	so we can support hot plugging, and to ref count adapters.
 */

static int aac_cfg_open(struct inode *inode, struct file *file)
{
	struct aac_dev *aac;
	int err = -ENODEV;

#if (defined(__ESXi4__))
	unsigned major_number = imajor(inode); //ESXi4 support
#else
	unsigned minor_number = iminor(inode);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
	lock_kernel();  /* BKL pushdown: nothing else protects this list */
#else
	mutex_lock(&aac_mutex);	/* BKL pushdown: nothing else protects this list */
#endif
	list_for_each_entry(aac, &aac_devices, entry) {
#if (defined(__ESXi4__))
		if (aac->major_number == major_number) {
#else
		if (aac->id == minor_number) {
#endif
			file->private_data = aac;
			err = 0;
			break;
		}
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
	unlock_kernel();
#else
	mutex_unlock(&aac_mutex);
#endif

	return err;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
/**
 *	aac_cfg_release		-	close down an AAC config device
 *	@inode: inode of configuration file
 *	@file: file handle of configuration file
 *
 *	Called when the last close of the configuration file handle
 *	is performed.
 */

static int aac_cfg_release(struct inode * inode, struct file * file )
{
	return 0;
}

#endif
/**
 *	aac_cfg_ioctl		-	AAC configuration request
 *	@inode: inode of device
 *	@file: file handle
 *	@cmd: ioctl command code
 *	@arg: argument
 *
 *	Handles a configuration ioctl. Currently this involves wrapping it
 *	up and feeding it into the nasty windowsalike glue layer.
 *
 *	Bugs: Needs locking against parallel ioctls lower down
 *	Bugs: Needs to handle hot plugging
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int aac_cfg_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
#else
static long aac_cfg_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
#endif
{
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	struct aac_dev * aac;
	list_for_each_entry(aac, &aac_devices, entry) {
#if (defined(__ESXi4__))
		if (aac->major_number == imajor(inode)) { //ESXi4 support
#else
		if (aac->id == iminor(inode)) {
#endif
			file->private_data = aac;
			break;
		}
	}
	if (file->private_data == NULL)
		return -ENODEV;
#else
	struct aac_dev * aac;
	aac = (struct aac_dev *) file->private_data;
#endif
	if (!capable(CAP_SYS_RAWIO) || aac->adapter_shutdown)
		return -EPERM;
	{
		int retval;
		if (cmd != FSACTL_GET_NEXT_ADAPTER_FIB){
			adbg_ioctl(aac, KERN_DEBUG,"aac_cfg_ioctl(%p,%x,%lx)\n",
			   file, cmd, arg);
		}
		retval = aac_do_ioctl(
		  file->private_data, cmd, (void __user *)arg);
		if (cmd != FSACTL_GET_NEXT_ADAPTER_FIB) {
			adbg_ioctl(aac, KERN_DEBUG,"aac_cfg_ioctl(%p,%x,%lx)\n",
			   file, cmd, arg);
		}
		return retval;
	}
	return aac_do_ioctl(file->private_data, cmd, (void __user *)arg);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
#ifdef CONFIG_COMPAT
static long aac_compat_do_ioctl(struct aac_dev *dev, unsigned cmd, unsigned long arg)
{
	long ret;
#if (LINUX_VERSION_CODE <  KERNEL_VERSION(3,0,0))
	lock_kernel();
#else
	mutex_lock(&aac_mutex);
#endif
	switch (cmd) {
	case FSACTL_MINIPORT_REV_CHECK:
	case FSACTL_SENDFIB:
	case FSACTL_OPEN_GET_ADAPTER_FIB:
	case FSACTL_CLOSE_GET_ADAPTER_FIB:
	case FSACTL_SEND_RAW_SRB:
	case FSACTL_GET_PCI_INFO:
	case FSACTL_QUERY_DISK:
	case FSACTL_DELETE_DISK:
	case FSACTL_FORCE_DELETE_DISK:
	case FSACTL_GET_CONTAINERS:
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	case FSACTL_GET_VERSION_MATCHING:
#endif
	case FSACTL_SEND_LARGE_FIB:
#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
	case FSACTL_REGISTER_FIB_SEND:
#endif
		ret = aac_do_ioctl(dev, cmd, (void __user *)arg);
		break;

	case FSACTL_GET_NEXT_ADAPTER_FIB: {
		struct fib_ioctl __user *f;

		f = compat_alloc_user_space(sizeof(*f));

		adbg_ioctl(dev, KERN_INFO, "FSACTL_GET_NEXT_ADAPTER_FIB:"
		  " compat_alloc_user_space(%lu)=%p\n", sizeof(*f), f);

		ret = 0;
		if (clear_user(f, sizeof(*f)))
		{
			adbg_ioctl(dev, KERN_INFO, "clear_user(%p,%lu)\n", f, sizeof(*f));
			ret = -EFAULT;
		}

		if (copy_in_user(f, (void __user *)arg, sizeof(struct fib_ioctl) - sizeof(u32)))
		{
			adbg_ioctl(dev,KERN_INFO,"copy_in_user(%p,%p,%lu)\n", f,
			  (void __user *)arg,
			  sizeof(struct fib_ioctl) - sizeof(u32));
			ret = -EFAULT;
		}

		if (!ret)
			ret = aac_do_ioctl(dev, cmd, f);
		break;
	}

	default:
#if (defined(AAC_CSMI))
		ret = aac_csmi_ioctl(dev, cmd, (void __user *)arg);
		if (ret == -ENOTTY)
#endif
		ret = -ENOIOCTLCMD;
		break;
	}
#if (LINUX_VERSION_CODE <  KERNEL_VERSION(3,0,0))
	unlock_kernel();
#else
	mutex_unlock(&aac_mutex);
#endif
	return ret;
}

static int aac_compat_ioctl(struct scsi_device *sdev, int cmd, void __user *arg)
{
	struct aac_dev *dev = (struct aac_dev *)sdev->host->hostdata;
	return aac_compat_do_ioctl(dev, cmd, (unsigned long)arg);
}

static long aac_compat_cfg_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	return aac_compat_do_ioctl((struct aac_dev *)file->private_data, cmd, arg);
}
#endif
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_model(struct class_device *class_dev,
		char *buf)
#else
static ssize_t aac_show_model(struct device *device,
			      struct device_attribute *attr, char *buf)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
	int len;
#if (defined(CONFIG_COMMUNITY_KERNEL))

	if (dev->supplement_adapter_info.AdapterTypeText[0]) {
		char * cp = dev->supplement_adapter_info.AdapterTypeText;
		while (*cp && *cp != ' ')
			++cp;
		while (*cp == ' ')
			++cp;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(buf, PAGE_SIZE, "%s\n", cp);
#else
		len = sprintf(buf, "%s\n", cp);
#endif
	} else
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(buf, PAGE_SIZE, "%s\n",
#else
		len = sprintf(buf, "%s\n",
#endif
		  aac_drivers[dev->cardtype].model);
#else
	struct scsi_inq scsi_inq;
	char *cp;

	setinqstr(dev, &scsi_inq, 255);
	cp = &scsi_inq.pid[sizeof(scsi_inq.pid)-1];
	while (*cp && *cp == ' ' && cp > scsi_inq.pid)
		--cp;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len = snprintf(buf, PAGE_SIZE,
#else
	len = sprintf(buf,
#endif
	  "%.*s\n", (int)(cp - scsi_inq.pid) + 1, scsi_inq.pid);
#endif
	return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_vendor(struct class_device *class_dev,
		char *buf)
#else
static ssize_t aac_show_vendor(struct device *device,
			       struct device_attribute *attr, char *buf)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
	int len;
#if (defined(CONFIG_COMMUNITY_KERNEL))

	if (dev->supplement_adapter_info.AdapterTypeText[0]) {
		char * cp = dev->supplement_adapter_info.AdapterTypeText;
		while (*cp && *cp != ' ')
			++cp;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(buf, PAGE_SIZE, "%.*s\n",
#else
		len = sprintf(buf, "%.*s\n",
#endif
		  (int)(cp - (char *)dev->supplement_adapter_info.AdapterTypeText),
		  dev->supplement_adapter_info.AdapterTypeText);
	} else
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len = snprintf(buf, PAGE_SIZE, "%s\n",
#else
	len = sprintf(buf, "%s\n",
#endif
	  aac_drivers[dev->cardtype].vname);
#else
	struct scsi_inq scsi_inq;
	char *cp;

	setinqstr(dev, &scsi_inq, 255);
	cp = &scsi_inq.vid[sizeof(scsi_inq.vid)-1];
	while (*cp && *cp == ' ' && cp > scsi_inq.vid)
		--cp;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len = snprintf(buf, PAGE_SIZE,
#else
	len = sprintf(buf,
#endif
	  "%.*s\n", (int)(cp - scsi_inq.vid) + 1, scsi_inq.vid);
#endif
	return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_flags(struct class_device *class_dev, char *buf)
#else
static ssize_t aac_show_flags(struct device *cdev,
			      struct device_attribute *attr, char *buf)
#endif
{
	int len = 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(cdev)->hostdata;
#endif

	if (nblank(dprintk(x)))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(buf, PAGE_SIZE, "dprintk\n");
#else
		len = sprintf(buf, "dprintk\n");
#endif
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	if (nblank(fwprintf(x)))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len, "fwprintf\n");
#else
		len += sprintf(buf + len, "fwprintf\n");
#endif
#endif
#if (defined(AAC_DETAILED_STATUS_INFO))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len += snprintf(buf + len, PAGE_SIZE - len,
#else
	len += sprintf(buf + len,
#endif
			"AAC_DETAILED_STATUS_INFO\n");
#endif
#	if (defined(AAC_DEBUG_INSTRUMENT_INIT))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_INIT\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_SETUP))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_SETUP\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_AAC_CONFIG))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_AAC_CONFIG\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_AIF))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_AIF\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_IOCTL\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_TIMING\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_RESET))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_RESET\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_FIB))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_FIB\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_CONTEXT\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_2TB))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_2TB\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB=0x%X\n",
		  AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB);
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_IO))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_IO\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_SG))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_SG\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_SG_PROBE\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_VM_NAMESERVE\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_SERIAL))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_SERIAL\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_SYNCHRONIZE\n");
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_SHUTDOWN\n");
#	endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) || (defined(SERVICE_ACTION_IN) && defined(SAI_READ_CAPACITY_16)))
	if (dev->raw_io_interface && dev->raw_io_64)
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
				"SAI_READ_CAPACITY_16\n");
#endif
#	if (defined(SCSI_HAS_VARY_IO))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "SCSI_HAS_VARY_IO\n");
#	endif
#if (defined(SCSI_HAS_DUMP))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len += snprintf(buf + len, PAGE_SIZE - len, "SCSI_HAS_DUMP\n");
#else
	len += sprintf(buf + len, "SCSI_HAS_DUMP\n");
#endif
#endif
#	if (defined(BOOTCD))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len, "BOOTCD\n");
#else
		len += sprintf(buf + len, "BOOTCD\n");
#endif
#	endif
#	if (defined(AAC_DEBUG_INSTRUMENT_PENDING))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
#else
		len += sprintf(buf + len,
#endif
		  "AAC_DEBUG_INSTRUMENT_PENDING=%u\n",
		   atomic_read(&dev->queues->queue[
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
		    (dev->comm_interface == AAC_COMM_APRE)
		      ? ApreCmdQueue
		      : AdapNormCmdQueue
#else
		    AdapNormCmdQueue
#endif
		  ].numpending));
#	endif
	if (dev->jbod)
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len, "SUPPORTED_JBOD\n");
#else
		len += sprintf(buf + len, "SUPPORTED_JBOD\n");
#endif
	if (dev->supplement_adapter_info.SupportedOptions2 &
		AAC_OPTION_POWER_MANAGEMENT)
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len,
				"SUPPORTED_POWER_MANAGEMENT\n");
#else
		len += sprintf(buf + len, "SUPPORTED_POWER_MANAGEMENT\n");
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
	if (dev->msi)
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len += snprintf(buf + len, PAGE_SIZE - len, "PCI_HAS_MSI\n");
#else
		len += sprintf(buf + len, "PCI_HAS_MSI\n");
#endif
#endif
	return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_kernel_version(struct class_device *class_dev,
		char *buf)
#else
static ssize_t aac_show_kernel_version(struct device *device,
				       struct device_attribute *attr,
				       char *buf)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
	int len, tmp;

	tmp = le32_to_cpu(dev->adapter_info.kernelrev);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len = snprintf(buf, PAGE_SIZE, "%d.%d-%d[%d]\n",
#else
	len = sprintf(buf, "%d.%d-%d[%d]\n",
#endif
	  tmp >> 24, (tmp >> 16) & 0xff, tmp & 0xff,
	  le32_to_cpu(dev->adapter_info.kernelbuild));
	return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_monitor_version(struct class_device *class_dev,
		char *buf)
#else
static ssize_t aac_show_monitor_version(struct device *device,
					struct device_attribute *attr,
					char *buf)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
	int len, tmp;

	tmp = le32_to_cpu(dev->adapter_info.monitorrev);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len = snprintf(buf, PAGE_SIZE, "%d.%d-%d[%d]\n",
#else
	len = sprintf(buf, "%d.%d-%d[%d]\n",
#endif
	  tmp >> 24, (tmp >> 16) & 0xff, tmp & 0xff,
	  le32_to_cpu(dev->adapter_info.monitorbuild));
	return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_bios_version(struct class_device *class_dev,
		char *buf)
#else
static ssize_t aac_show_bios_version(struct device *device,
				     struct device_attribute *attr,
				     char *buf)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
	int len, tmp;

	tmp = le32_to_cpu(dev->adapter_info.biosrev);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	len = snprintf(buf, PAGE_SIZE, "%d.%d-%d[%d]\n",
#else
	len = sprintf(buf, "%d.%d-%d[%d]\n",
#endif
	  tmp >> 24, (tmp >> 16) & 0xff, tmp & 0xff,
	  le32_to_cpu(dev->adapter_info.biosbuild));
	return len;
}
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)))

static ssize_t aac_show_driver_version(struct class_device *class_dev,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", aac_driver_version);
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
ssize_t aac_show_serial_number(struct class_device *class_dev, char *buf)
#else
static ssize_t aac_show_serial_number(struct device *device,
			       struct device_attribute *attr, char *buf)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
	int len = 0;

	if (le32_to_cpu(dev->adapter_info.serial[0]) != 0xBAD0)
#if (defined(AAC_DEBUG_INSTRUMENT_SERIAL))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(buf, PAGE_SIZE, "%06X|%.*s\n",
#else
		len = sprintf(buf, "%06X|%.*s\n",
#endif
		  le32_to_cpu(dev->adapter_info.serial[0]),
		  (int)sizeof(dev->supplement_adapter_info.MfgPcbaSerialNo),
		  dev->supplement_adapter_info.MfgPcbaSerialNo);
#else
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(buf, PAGE_SIZE, "%06X\n",
#else
		len = sprintf(buf, "%06X\n",
#endif
		  le32_to_cpu(dev->adapter_info.serial[0]));
#endif
	/*
	 * "DDTS# 11875: vmware 4.0 : Shows some junk value in serial number field"
	 *		 Added this fix to copy serial number into buffer	
	 */

	if (len)
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(buf, PAGE_SIZE, "%.*s\n",
#else
		len = sprintf(buf, "%.*s\n",
#endif
		  (int)sizeof(dev->supplement_adapter_info.MfgPcbaSerialNo),
		  dev->supplement_adapter_info.MfgPcbaSerialNo);
	return len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_max_channel(struct class_device *class_dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  class_to_shost(class_dev)->max_channel);
}
#else
static ssize_t aac_show_max_channel(struct device *device,
				    struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  class_to_shost(device)->max_channel);
}
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_SETUP))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_hba_max_channel(struct class_device *class_dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  ((struct aac_dev*)class_to_shost(class_dev)->hostdata)
	    ->maximum_num_channels);
}

static ssize_t aac_show_hba_max_physical(struct class_device *class_dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  ((struct aac_dev*)class_to_shost(class_dev)->hostdata)
	    ->maximum_num_physicals);
}

static ssize_t aac_show_hba_max_array(struct class_device *class_dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  ((struct aac_dev*)class_to_shost(class_dev)->hostdata)
	    ->maximum_num_containers);
}
#else
static ssize_t aac_show_hba_max_channel(struct device *device,
					struct device_attribute *attr,
					char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  ((struct aac_dev*)class_to_shost(device)->hostdata)
	    ->maximum_num_channels);
}

static ssize_t aac_show_hba_max_physical(struct device *device,
					 struct device_attribute *attr,
					 char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  ((struct aac_dev*)class_to_shost(device)->hostdata)
	    ->maximum_num_physicals);
}

static ssize_t aac_show_hba_max_array(struct device *device,
				      struct device_attribute *attr,
				      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  ((struct aac_dev*)class_to_shost(device)->hostdata)
	    ->maximum_num_containers);
}
#endif

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_max_id(struct class_device *class_dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  class_to_shost(class_dev)->max_id);
}
#else
static ssize_t aac_show_max_id(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  class_to_shost(device)->max_id);
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_store_reset_adapter(struct class_device *class_dev,
		const char *buf, size_t count)
#else
static ssize_t aac_store_reset_adapter(struct device *device,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
#endif
{
	int retval = -EACCES;

	if (!capable(CAP_SYS_ADMIN))
		return retval;
//	if (buf[0] == '?')
//		Dump UART log into the KERN_INFO printk
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	retval = aac_reset_adapter((struct aac_dev*)class_to_shost(class_dev)->hostdata, buf[0] == '!');
#else
	retval = aac_reset_adapter((struct aac_dev*)class_to_shost(device)->hostdata, buf[0] == '!');
#endif
	if (retval >= 0)
		retval = count;
	return retval;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_reset_adapter(struct class_device *class_dev,
		char *buf)
#else
static ssize_t aac_show_reset_adapter(struct device *device,
				      struct device_attribute *attr,
				      char *buf)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
	struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
	int len, tmp;

	tmp = aac_adapter_check_health(dev);
	if ((tmp == 0) && dev->in_reset)
		tmp = -EBUSY;
	len = snprintf(buf, PAGE_SIZE, "0x%x\n", tmp);
	return len;
}

#endif
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_store_uart_adapter(struct class_device *class_dev,
		const char *buf, size_t count)
#else
static ssize_t aac_store_uart_adapter(struct device *device,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
#endif
{
	if (nblank(fwprintf(x))) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
		struct aac_dev *dev = (struct aac_dev*)class_to_shost(class_dev)->hostdata;
#else
		struct aac_dev *dev = (struct aac_dev*)class_to_shost(device)->hostdata;
#endif
		unsigned len = count;
		unsigned long seconds = get_seconds();

		/* Trim off trailing space */
		while ((len > 0) && ((buf[len-1] == '\n') ||
		  (buf[len-1] == '\r') || (buf[len-1] == '\t') ||
		  (buf[len-1] == ' ')))
			--len;
		if (len > (dev->FwDebugBufferSize - 10))
			len = dev->FwDebugBufferSize - 10;
		seconds = seconds;
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B, "%02u:%02u:%02u: %.*s",
		  (int)((seconds / 3600) % 24), (int)((seconds / 60) % 60),
		  (int)(seconds % 60), len, buf));
	}
	return count;
}
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#if (!defined(CONFIG_COMMUNITY_KERNEL))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static ssize_t aac_show_uart_adapter(struct class_device *class_dev,
		char *buf)
#else
static ssize_t aac_show_uart_adapter(struct device *device,
				     struct device_attribute *attr,
				     char *buf)
#endif
{
	return snprintf(buf, PAGE_SIZE, nblank(fwprintf(x)) ? "YES\n" : "NO\n");
}

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_model = {
#else
static struct device_attribute aac_model = {
#endif
	.attr = {
		.name = "model",
		.mode = S_IRUGO,
	},
	.show = aac_show_model,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_vendor = {
#else
static struct device_attribute aac_vendor = {
#endif
	.attr = {
		.name = "vendor",
		.mode = S_IRUGO,
	},
	.show = aac_show_vendor,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_flags = {
#else
static struct device_attribute aac_flags = {
#endif
	.attr = {
		.name = "flags",
		.mode = S_IRUGO,
	},
	.show = aac_show_flags,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_kernel_version = {
#else
static struct device_attribute aac_kernel_version = {
#endif
	.attr = {
		.name = "hba_kernel_version",
		.mode = S_IRUGO,
	},
	.show = aac_show_kernel_version,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_monitor_version = {
#else
static struct device_attribute aac_monitor_version = {
#endif
	.attr = {
		.name = "hba_monitor_version",
		.mode = S_IRUGO,
	},
	.show = aac_show_monitor_version,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_bios_version = {
#else
static struct device_attribute aac_bios_version = {
#endif
	.attr = {
		.name = "hba_bios_version",
		.mode = S_IRUGO,
	},
	.show = aac_show_bios_version,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10))
static struct class_device_attribute aac_lld_version = {
	.attr = {
		.name = "driver_version",
		.mode = S_IRUGO,
	},
	.show = aac_show_driver_version,
};
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_serial_number = {
#else
static struct device_attribute aac_serial_number = {
#endif
	.attr = {
		.name = "serial_number",
		.mode = S_IRUGO,
	},
	.show = aac_show_serial_number,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_max_channel = {
#else
static struct device_attribute aac_max_channel = {
#endif
	.attr = {
		.name = "max_channel",
		.mode = S_IRUGO,
	},
	.show = aac_show_max_channel,
};
#if (defined(AAC_DEBUG_INSTRUMENT_SETUP))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_hba_max_channel = {
#else
static struct device_attribute aac_hba_max_channel = {
#endif
	.attr = {
		.name = "hba_max_channel",
		.mode = S_IRUGO,
	},
	.show = aac_show_hba_max_channel,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_hba_max_physical = {
#else
static struct device_attribute aac_hba_max_physical = {
#endif
	.attr = {
		.name = "hba_max_physical",
		.mode = S_IRUGO,
	},
	.show = aac_show_hba_max_physical,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_hba_max_array = {
#else
static struct device_attribute aac_hba_max_array = {
#endif
	.attr = {
		.name = "hba_max_array",
		.mode = S_IRUGO,
	},
	.show = aac_show_hba_max_array,
};
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_max_id = {
#else
static struct device_attribute aac_max_id = {
#endif
	.attr = {
		.name = "max_id",
		.mode = S_IRUGO,
	},
	.show = aac_show_max_id,
};
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_reset = {
#else
static struct device_attribute aac_reset = {
#endif
	.attr = {
		.name = "reset_host",
		.mode = S_IWUSR|S_IRUGO,
	},
	.store = aac_store_reset_adapter,
	.show = aac_show_reset_adapter,
};
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute aac_uart = {
#else
static struct device_attribute aac_uart = {
#endif
	.attr = {
		.name = "uart",
		.mode = S_IWUSR|S_IRUGO,
	},
	.store = aac_store_uart_adapter,
	.show = aac_show_uart_adapter,
};
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct class_device_attribute *aac_attrs[] = {
#else
static struct device_attribute *aac_attrs[] = {
#endif
	&aac_model,
	&aac_vendor,
	&aac_flags,
	&aac_kernel_version,
	&aac_monitor_version,
	&aac_bios_version,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10))
	&aac_lld_version,
#endif
	&aac_serial_number,
	&aac_max_channel,
#if (defined(AAC_DEBUG_INSTRUMENT_SETUP))
	&aac_hba_max_channel,
	&aac_hba_max_physical,
	&aac_hba_max_array,
#endif
	&aac_max_id,
	&aac_reset,
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	&aac_uart,
#endif
	NULL
};
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))

ssize_t aac_get_serial_number(struct device *device, char *buf)
{
	return aac_show_serial_number(device, &aac_serial_number, buf);
}
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(CONFIG_SCSI_PROC_FS))

/**
 *	aac_procinfo	-	Implement /proc/scsi/<drivername>/<n>
 *	@proc_buffer: memory buffer for I/O
 *	@start_ptr: pointer to first valid data
 *	@offset: offset into file
 *	@bytes_available: space left
 *	@host_no: scsi host ident
 *	@write: direction of I/O
 *
 *	Used to export driver statistics and other infos to the world outside
 *	the kernel using the proc file system. Also provides an interface to
 *	feed the driver with information.
 *
 *		For reads
 *			- if offset > 0 return -EINVAL
 *			- if offset == 0 write data to proc_buffer and set the start_ptr to
 *			beginning of proc_buffer, return the number of characters written.
 *		For writes
 *			- writes currently not supported, return -EINVAL
 *
 *	Bugs:	Only offset zero is handled
 */
static int aac_procinfo(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	struct Scsi_Host * shost,
#endif
	char *proc_buffer, char **start_ptr,off_t offset,
	int bytes_available,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	int host_no,
#endif
	int write)
{
	struct aac_dev * dev = (struct aac_dev *)NULL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	struct Scsi_Host * shost = (struct Scsi_Host *)NULL;
#endif
	char *buf;
	int len;
	int total_len = 0;

	*start_ptr = proc_buffer;
#if (defined(AAC_LM_SENSOR) || defined(IOP_RESET))
	if(offset > 0)
#else
	if ((!nblank(fwprintf(x)) && write) || offset > 0)
#endif
		return 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	list_for_each_entry(dev, &aac_devices, entry) {
		shost = dev->scsi_host_ptr;
		if (shost->host_no == host_no)
			break;
	}
	if (shost == (struct Scsi_Host *)NULL)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		return 0;
#else
		return -ENODEV;
#endif
#endif
	dev = (struct aac_dev *)shost->hostdata;
	if (dev == (struct aac_dev *)NULL)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		return 0;
#else
		return -ENODEV;
#endif
	if (!write) {
		buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!buf)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
			return 0;
#else
			return -ENOMEM;
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(proc_buffer, bytes_available,
		  "Driver version: Adaptec Raid Controller: (v %s)\n", aac_driver_version);
#else
		len = sprintf(proc_buffer,
		  "Driver version: Adaptec Raid Controller: (v %s)\n", aac_driver_version);
#endif
		total_len = len;
		proc_buffer += len;
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "Board ID: 0x0%x%x\n", dev->pdev->subsystem_device, dev->pdev->subsystem_vendor);
#else
			len = sprintf(proc_buffer, "Board ID: 0x0%x%x\n", dev->pdev->subsystem_device, dev->pdev->subsystem_vendor);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "PCI ID: 0x0%x%x\n", dev->pdev->device, dev->pdev->vendor);
#else
			len = sprintf(proc_buffer, "PCI ID: 0x0%x%x\n", dev->pdev->device, dev->pdev->vendor);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_vendor(shost_to_class(shost), buf);
#else
			len = aac_show_vendor(shost_to_class(shost), &aac_vendor, buf);
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "Vendor: %.*s\n", len - 1, buf);
#else
			len = sprintf(proc_buffer, "Vendor: %.*s\n", len - 1, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_model(shost_to_class(shost), buf);
#else
			len = aac_show_model(shost_to_class(shost), &aac_model, buf);
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "Model: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "Model: %.*s", len, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_flags(shost_to_class(shost), buf);
#else
			len = aac_show_flags(shost_to_class(shost), &aac_flags, buf);
#endif
			if (len) {
				char *cp = proc_buffer;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
				len = snprintf(cp, bytes_available - total_len,
				  "flags=%.*s", len, buf);
#else
				len = sprintf(cp, "flags=%.*s", len, buf);
#endif
				total_len += len;
				proc_buffer += len;
				while (--len > 0) {
					if (*cp == '\n')
						*cp = '+';
					++cp;
				}
			}
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_kernel_version(shost_to_class(shost), buf);
#else
			len = aac_show_kernel_version(shost_to_class(shost), &aac_kernel_version, buf);
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "kernel: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "kernel: %.*s", len, buf);
			total_len += len;
			proc_buffer += len;
#endif
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_monitor_version(shost_to_class(shost), buf);
#else
			len = aac_show_monitor_version(shost_to_class(shost), &aac_monitor_version, buf);
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "monitor: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "monitor: %.*s", len, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_bios_version(shost_to_class(shost), buf);
#else
			len = aac_show_bios_version(shost_to_class(shost), &aac_bios_version, buf);
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "bios: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "bios: %.*s", len, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_serial_number(shost_to_class(shost), buf);
#else
			len = aac_get_serial_number(shost_to_class(shost), buf);
#endif
			if (len) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
				len = snprintf(proc_buffer, bytes_available - total_len,
				  "serial: %.*s", len, buf);
#else
				len = sprintf(proc_buffer, "serial: %.*s", len, buf);
#endif
				total_len += len;
			}
		}
		kfree(buf);
		return total_len;
	}
#if (defined(IOP_RESET))
	{
		static char reset[] = "reset_host";
		if (strnicmp (proc_buffer, reset, sizeof(reset) - 1) == 0) {
			if (!capable(CAP_SYS_ADMIN))
				return -EPERM;
			(void)aac_reset_adapter(dev,
			    proc_buffer[sizeof(reset) - 1] == '!');
			return bytes_available;
		}
	}
#endif
	if (nblank(fwprintf(x))) {
		static char uart[] = "uart=";
		if (strnicmp (proc_buffer, uart, sizeof(uart) - 1) == 0) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			(void)aac_store_uart_adapter(shost_to_class(shost),
			  &proc_buffer[sizeof(uart) - 1],
			  bytes_available - (sizeof(uart) - 1));
#else
			(void)aac_store_uart_adapter(shost_to_class(shost),
			  &aac_uart,		
			  &proc_buffer[sizeof(uart) - 1],
			  bytes_available - (sizeof(uart) - 1));
#endif
			return bytes_available;
		}
	}
#if (defined(AAC_LM_SENSOR))
	{
		int ret, tmp, index;
		s32 temp[5];
		static char temperature[] = "temperature=";
		if (strnicmp (proc_buffer, temperature, sizeof(temperature) - 1))
			return bytes_available;
		for (index = 0;
		  index < (sizeof(temp)/sizeof(temp[0]));
		  ++index)
			temp[index] = 0x80000000;
		ret = sizeof(temperature) - 1;
		for (index = 0;
		  index < (sizeof(temp)/sizeof(temp[0]));
		  ++index) {
			int sign, mult, c;
			if (ret >= bytes_available)
				break;
			c = proc_buffer[ret];
			if (c == '\n') {
				++ret;
				break;
			}
			if (c == ',') {
				++ret;
				continue;
			}
			sign = 1;
			mult = 0;
			tmp = 0;
			if (c == '-') {
				sign = -1;
				++ret;
			}
			for (;
			  (ret < bytes_available) && ((c = proc_buffer[ret]));
			  ++ret) {
				if (('0' <= c) && (c <= '9')) {
					tmp *= 10;
					tmp += c - '0';
					mult *= 10;
				} else if ((c == '.') && (mult == 0))
					mult = 1;
				else
					break;
			}
			if ((ret < bytes_available)
			 && ((c == ',') || (c == '\n')))
				++ret;
			if (!mult)
				mult = 1;
			if (sign < 0)
				tmp = -tmp;
			temp[index] = ((tmp << 8) + (mult >> 1)) / mult;
			if (c == '\n')
				break;
		}
		ret = index;
		if (nblank(dprintk(x))) {
			for (index = 0; index < ret; ++index) {
				int sign;
				tmp = temp[index];
				sign = tmp < 0;
				if (sign)
					tmp = -tmp;
				dprintk((KERN_DEBUG "%s%s%d.%08doC",
				  (index ? "," : ""),
				  (sign ? "-" : ""),
				  tmp >> 8, (tmp % 256) * 390625));
			}
		}
		/* Send temperature message to Firmware */
		(void)aac_adapter_sync_cmd(dev, RCV_TEMP_READINGS,
		  ret, temp[0], temp[1], temp[2], temp[3], temp[4],
		  NULL, NULL, NULL, NULL, NULL);
		return bytes_available;
	}
#endif
	return -EINVAL;
}
#endif
#endif
#if (!defined(HAS_BOOT_CONFIG))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct file_operations aac_cfg_fops = {
#else
static const struct file_operations aac_cfg_fops = {
#endif
	.owner		= THIS_MODULE,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
	.ioctl		= aac_cfg_ioctl,
#else
	.unlocked_ioctl = aac_cfg_ioctl,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
#ifdef CONFIG_COMPAT
	.compat_ioctl   = aac_compat_cfg_ioctl,
#endif
#endif
	.open		= aac_cfg_open,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	.release	= aac_cfg_release
#endif
};
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(SCSI_HAS_DUMP))
static struct scsi_dump_ops aac_dump_ops = {
#if (defined(SCSI_HAS_DUMP_SANITY_CHECK))
	.sanity_check	= aac_sanity_check,
#endif
	.poll		= aac_poll,
};

#define aac_driver_template aac_driver_template_dump.hostt
static struct SHT_dump aac_driver_template_dump = {
	.hostt = {
#else

static struct scsi_host_template aac_driver_template = {
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	.detect				= aac_detect,
#endif
#if (defined(__VMKERNEL_MODULE__) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)))
	.release			= aac_release,
#endif
	.module				= THIS_MODULE,
#if (defined(__VMKLNX30__) || defined(__VMKLNX__))
	.name				= "aacraid",
#else
	.name				= "AAC",
#endif
	.proc_name			= AAC_DRIVERNAME,
	.info				= aac_info,
#if (!defined(HAS_BOOT_CONFIG))
	.ioctl				= aac_ioctl,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
#if (defined(CONFIG_COMPAT) && !defined(HAS_BOOT_CONFIG))
	.compat_ioctl			= aac_compat_ioctl,
#endif
#endif
	.queuecommand			= aac_queuecommand,
	.bios_param			= aac_biosparm,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(CONFIG_SCSI_PROC_FS))
	.proc_info			= aac_procinfo,
#endif
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	.shost_attrs			= aac_attrs,
	.slave_configure		= aac_slave_configure,
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)) && (LINUX_VERSION_CODE <= KERNEL_VERSION(3,18,0)))
    .slave_alloc            = aac_slave_alloc,
    .slave_destroy          = aac_slave_destroy,
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
	.change_queue_depth		= aac_change_queue_depth,
#endif
#ifdef RHEL_MAJOR
#if (RHEL_MAJOR == 6 && RHEL_MINOR >= 2)
	.lockless			= 1,
#endif
#endif
	.sdev_attrs			= aac_dev_attrs,
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	.eh_device_reset_handler	= aac_eh_device_reset,
	.eh_bus_reset_handler		= aac_eh_bus_reset,
#endif
	.eh_abort_handler		= aac_eh_abort,
	.eh_host_reset_handler		= aac_eh_reset,
	.can_queue			= AAC_NUM_IO_FIB,
	.this_id			= MAXIMUM_NUM_CONTAINERS,
	.sg_tablesize			= 16,
	.max_sectors			= 128,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0))
    .use_blk_tags           = 1,
#endif
#if (AAC_NUM_IO_FIB > 256)
	.cmd_per_lun			= 256,
#else
	.cmd_per_lun			= AAC_NUM_IO_FIB,
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	.use_new_eh_code		= 1,
#endif
	.use_clustering			= ENABLE_CLUSTERING,
#if (((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)) || defined(ENABLE_SG_CHAINING)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)))
	.use_sg_chaining		= ENABLE_SG_CHAINING,
#endif
	.emulated			= 1,
#if (defined(SCSI_HAS_VARY_IO))
	.vary_io			= 1,
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
#if (defined(SCSI_HAS_DUMP))
#if (defined(SCSI_HAS_DUMP_SANITY_CHECK))
	.dump_sanity_check		= aac_sanity_check,
#endif
	.dump_poll			= aac_poll,
#endif
#elif (defined(SCSI_HAS_DUMP))
	.disk_dump			= 1,
},
	.dump_ops			= &aac_dump_ops,
#endif
};

static void __aac_shutdown(struct aac_dev * aac)
{
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO
	  "__aac_shutdown(%p={.aif_thread=%d,.thread_pid=%d,.shutdown=%d) - ENTER\n"
	  aac, aac->aif_thread, aac->thread_pid, aac->shutdown);
#endif
	if (aac->aif_thread && (aac->thread_pid > 0)) {
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
		aac->thread_die = 1;
#else
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
		printk(KERN_INFO "kill_proc(%d,SIGKILL,0)\n", aac->thread_pid);
#endif
		kill_proc(aac->thread_pid, SIGKILL, 0);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
		printk(KERN_INFO "wait_for_completion(%p)\n",
		  &aac->aif_completion);
#endif
		wait_for_completion(&aac->aif_completion);
	}
#else
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO
	  "__aac_shutdown(%p={.aif_thread=%d,.thread=%p,.shutdown=%d) - ENTER\n",
	  aac, aac->aif_thread, aac->thread, aac->shutdown);
#endif
	if (aac->aif_thread) {
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
		printk(KERN_INFO "kthread_stop(%p)\n", aac->thread);
#endif
		int i;
		for (i = 0; i < (aac->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB); i++) {
			struct fib *fib = &aac->fibs[i];
			if (!(fib->hw_fib_va->header.XferState & cpu_to_le32(NoResponseExpected | Async)) &&
			    (fib->hw_fib_va->header.XferState & cpu_to_le32(ResponseExpected)))
				up(&fib->event_wait);
		}
		kthread_stop(aac->thread);
	}
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO "aac_send_shutdown(%p)\n", aac);
#endif
	aac->adapter_shutdown = 1;
	aac_send_shutdown(aac);
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO "aac_adapter_disable_int(%p)\n", aac);
#endif
	aac_adapter_disable_int(aac);
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO "free_irq(%d,%p)\n", aac->pdev->irq, aac);
#endif
	aac_free_irq(aac);
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO "__aac_shutdown(%p) - EXIT\n", aac);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN) || ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN)))))
	aac->shutdown = 1;
#endif
}
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN))))

static void aac_device_shutdown(struct device *dev)
{
	struct aac_dev *aac;

	list_for_each_entry(aac, &aac_devices, entry)
		if (!aac->shutdown && (dev == &aac->pdev->dev)) {
			scsi_block_requests(aac->scsi_host_ptr);
			__aac_shutdown(aac);
		}
}
#endif

void aac_init_char(void)
{
#if (!defined(HAS_BOOT_CONFIG))
	/*
	 * ESXi4 do not support character device with multiple minor numbers
	 * So we will have to create interfaces with different major numbers 
	 * for each such interface
	 */
#if (defined(__ESXi4__))
	struct aac_dev		*aac;
	char			name[5];

	list_for_each_entry(aac, &aac_devices, entry) {
		sprintf(name, "aac%d", aac->id);
		aac_cfg_major = register_chrdev( 0, name, &aac_cfg_fops);
		if (aac_cfg_major < 0) {
			printk(KERN_WARNING
				"aacraid: unable to register \"%s\" device.\n", name);
		} else
			aac->major_number = aac_cfg_major;
	}
#else
	aac_cfg_major = register_chrdev( 0, "aac", &aac_cfg_fops);
	if (aac_cfg_major < 0) {
		printk(KERN_WARNING
			"aacraid: unable to register \"aac\" device.\n");
	}
#endif
#endif
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
static int __devinit aac_probe_one(struct pci_dev *pdev,
		const struct pci_device_id *id)
#else
static int aac_probe_one(struct pci_dev *pdev,
		const struct pci_device_id *id)
#endif
{
	unsigned index = id->driver_data;
	struct Scsi_Host *shost;
	struct aac_dev *aac;
	struct list_head *insert = &aac_devices;
	int error = -ENODEV;
	int unique_id = 0;
	u32 i = 0;
#if (defined(__arm__) || defined(CONFIG_EXTERNAL))
	static struct pci_dev * slave = NULL;
	static int nslave = 0;
#endif
	extern int aac_sync_mode;

#if defined(__powerpc__) || defined(__PPC__) || defined(__ppc__)
	/* EEH support: FW takes time to complete the PCI hot reset.
	 * EEH will perform a hot plug activity to unload and load 
	 * the driver. Delay lets the controller complete the reset
	 * and load the driver.
	 */
	msleep(1000);
#endif

#if (defined(__arm__) || defined(CONFIG_EXTERNAL))
	if (aac_drivers[index].quirks & AAC_QUIRK_SLAVE) {
		/* detect adjoining slaves */
		if (slave) {
			if ((pci_resource_start(pdev, 0)
			  + pci_resource_len(pdev, 0))
			  == pci_resource_start(slave, 0))
				slave = pdev;
			else if ((pci_resource_start(slave, 0)
			  + (pci_resource_len(slave, 0) * nslave))
			  != pci_resource_start(pdev, 0)) {
				printk(KERN_WARNING AAC_DRIVERNAME
				  ": multiple sets of slave controllers discovered\n");
				nslave = 0;
				slave = pdev;
			}
		} else
			slave = pdev;
		if (pci_resource_start(slave,0)) {
			error = pci_enable_device(pdev);
			if (error) {
				printk(KERN_WARNING AAC_DRIVERNAME
				  ": failed to enable slave\n");
				nslave = 0;
				slave = NULL;
				return error;
			}
			++nslave;
			pci_set_master(pdev);
		} else {
			printk(KERN_WARNING AAC_DRIVERNAME
			  ": slave BAR0 is not set\n");
			nslave = 0;
			slave = NULL;
			return error;
		}
		return 1;
	}
#endif
	list_for_each_entry(aac, &aac_devices, entry) {
		if (aac->id > unique_id)
			break;
		insert = &aac->entry;
		unique_id++;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |
					PCIE_LINK_STATE_CLKPM);
#endif
	error = pci_enable_device(pdev);
	if (error)
		goto out;
	error = -ENODEV;
#if (defined(__arm__) || defined(CONFIG_EXTERNAL))
	if ((aac_drivers[index].quirks & AAC_QUIRK_MASTER) && (slave)) {
		unsigned long base = pci_resource_start(pdev, 0);
		struct master_registers {
			u32 x[51];
			u32	E_CONFIG1;
			u32 y[3];
			u32	E_CONFIG2;
		} __iomem * map = ioremap(base, AAC_MIN_FOOTPRINT_SIZE);
		if (!map) {
			printk(KERN_WARNING AAC_DRIVERNAME
			  ": unable to map master adapter to configure slaves.\n");
		} else {
			((struct master_registers *)map)->E_CONFIG2
			  = cpu_to_le32(pci_resource_start(slave, 0));
			((struct master_registers *)map)->E_CONFIG1
			  = cpu_to_le32(0x5A000000 + nslave);
			iounmap(map);
		}
		nslave = 0;
		slave = NULL;
	}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
	if (pci_set_dma_mask(pdev, DMA_32BIT_MASK) ||
			pci_set_consistent_dma_mask(pdev, DMA_32BIT_MASK))
#else
	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) ||
			pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)))
#endif
		goto out_disable_pdev;
	/*
	 * If the quirk31 bit is set, the adapter needs adapter
	 * to driver communication memory to be allocated below 2gig
	 */
	if (aac_drivers[index].quirks & AAC_QUIRK_31BIT)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
		if (pci_set_dma_mask(pdev, DMA_31BIT_MASK) ||
				pci_set_consistent_dma_mask(pdev, DMA_31BIT_MASK))
#else
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(31)) ||
				pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(31)))
#endif
			goto out_disable_pdev;

	pci_set_master(pdev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14))
	aac_driver_template.name = aac_drivers[index].name;
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
	shost = vmk_scsi_register(&aac_driver_template, sizeof(struct aac_dev), pdev->bus->number, pdev->devfn);
#else
	shost = scsi_host_alloc(&aac_driver_template, sizeof(struct aac_dev));
#endif
	adbg_init(aac,KERN_INFO,"scsi_host_alloc(%p,%zu)=%p\n",
	  		&aac_driver_template, sizeof(struct aac_dev), shost);
	if (!shost)
		goto out_disable_pdev;

	shost->irq = pdev->irq;
	shost->base = pci_resource_start(pdev, 0);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	scsi_set_pci_device(shost, pdev);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
	scsi_set_device(shost, &pdev->dev);
#endif
	shost->unique_id = unique_id;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	/*
	 *	This function is called after the device list
	 *	has been built to find the tagged queueing
	 *	depth supported for each device.
	 */
	shost->select_queue_depths = aac_queuedepth;
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(SERVICE_ACTION_IN))
	shost->max_cmd_len = 16;
#endif
	if(aac_cfg_major == -2)
	{
		aac_init_char();
	}
	
	aac = (struct aac_dev *)shost->hostdata;
	aac->scsi_host_ptr = shost;
	aac->pdev = pdev;
	aac->name = aac_driver_template.name;
	aac->id = shost->unique_id;
	aac->cardtype = index;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
//	aac->msi = 0;
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN))))
	pdev->dev.driver->shutdown = aac_device_shutdown;
#endif
	INIT_LIST_HEAD(&aac->entry);

#if (defined(CONFIG_VMNIX) && !defined(__VMKERNEL_MODULE__) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)))
	shost->bus = pdev->bus->number;
	shost->devfn = pdev->devfn;
	shost->devid = (void *)aac;
#endif
	do {
		aac->fibs = kmalloc(sizeof(struct fib) * (shost->can_queue + AAC_NUM_MGT_FIB), GFP_KERNEL);
	} while (!aac->fibs && (shost->can_queue -= 16) >= (64 - AAC_NUM_MGT_FIB));	
	if (!aac->fibs)
		goto out_free_host;
	memset(aac->fibs, 0, sizeof(struct fib) * (shost->can_queue + AAC_NUM_MGT_FIB));
	spin_lock_init(&aac->fib_lock);

	aac->msix_vector_tbl = (u32 *)kmalloc(sizeof(u32)*num_online_cpus(), GFP_KERNEL);
	for(i=0; i < num_online_cpus(); i++)
                aac->msix_vector_tbl[i]=num_online_cpus();

        /*
	 *	Map in the registers from the adapter.
	 */
	aac->base_size = AAC_MIN_FOOTPRINT_SIZE;
	if ((*aac_drivers[index].init)(aac))
		goto out_unmap;

	if (aac->sync_mode) {
		if (aac_sync_mode) 
			printk(KERN_INFO "%s%d: Sync. mode enforced by driver parameter. This will cause a significant performance decrease!\n", aac->name, aac->id);
		else  
			printk(KERN_INFO "%s%d: Async. mode not supported by current driver, sync. mode enforced.\nPlease update driver to get full performance.\n", aac->name, aac->id);
	}

#if (!defined(CONFIG_COMMUNITY_KERNEL))
	if (nblank(fwprintf(x)) && !aac->sa_firmware) {
		aac_get_fw_debug_buffer(aac);
#if (defined(UTS_MACHINE))
#if (defined(CONFIG_M586))
#undef UTS_MACHINE
#define UTS_MACHINE "i586"
#elif (defined(CONFIG_M686))
#undef UTS_MACHINE
#define UTS_MACHINE "i686"
#elif (defined(CONFIG_PPC64))
#undef UTS_MACHINE
#define UTS_MACHINE "ppc64"
#endif
#else
#if (defined(CONFIG_M386))
#define UTS_MACHINE "i386"
#elif (defined(CONFIG_M486))
#define UTS_MACHINE "i486"
#elif (defined(CONFIG_MK6))
#define UTS_MACHINE "i486"
#elif (defined(CONFIG_M586))
#define UTS_MACHINE "i586"
#elif (defined(CONFIG_MPENTIUMII))
#define UTS_MACHINE "i586"
#elif (defined(CONFIG_M686))
#define UTS_MACHINE "i686"
#elif (defined(CONFIG_MPENTIUMIII))
#define UTS_MACHINE "i686"
#elif (defined(CONFIG_MPENTIUM4))
#define UTS_MACHINE "i686"
#elif (defined(CONFIG_MK7))
#define UTS_MACHINE "athlon"
#elif (defined(CONFIG_MK8))
#define UTS_MACHINE "x86_64"
#elif (defined(CONFIG_IA32E))
#define UTS_MACHINE "ia32e"
#elif (defined(CONFIG_IA64))
#define UTS_MACHINE "ia64"
#elif (defined(CONFIG_X86_64))
#define UTS_MACHINE "x86_64"
#elif (defined(CONFIG_PPC64))
#define UTS_MACHINE "ppc64"
#elif (defined(CONFIG_PPC))
#define UTS_MACHINE "ppc"
#elif (defined(CONFIG_CPU_XSCALE_80200))
#define UTS_MACHINE "xscale"
#elif (defined(CONFIG_ARM))
#define UTS_MACHINE "arm"
#elif (defined(CONFIG_X86))
#define UTS_MACHINE "i386"
#endif
#endif
#if (defined(RHEL_VERSION))
#define _str(x) #x
#define str(x) _str(x)
#if (RHEL_VERSION < 3)
#define OSNAME "RHAS"
#else
#define OSNAME "RHEL"
#endif
#if (defined(RHEL_UPDATE))
#if (RHEL_UPDATE != 0)
#define OSVERSION str(RHEL_VERSION) "u" str(RHEL_UPDATE)
#else
#define OSVERSION str(RHEL_VERSION)
#endif
#else
#define OSVERSION str(RHEL_VERSION)
#endif
#elif (defined(CONFIG_SUSE_KERNEL))
#define OSNAME "SuSE"
#define OSVERSION
#elif (defined(CONFIG_SLES_KERNEL))
#define OSNAME "SLES"
#define OSVERSION
#endif
#if (defined(OSNAME))
#if (defined(UTS_MACHINE))
#define ARCH_NAME "(" OSNAME OSVERSION " " UTS_MACHINE ") "
#else
#define ARCH_NAME "(" OSNAME OSVERSION ") "
#endif
#elif (defined(UTS_MACHINE))
#define ARCH_NAME "(" UTS_MACHINE ") "
#else
#define ARCH_NAME
#endif
#if (!defined(UTS_SYSNAME))
#define UTS_SYSNAME "Linux"
#endif
#if (defined(UTS_RELEASE))
#define UTS_ARGS
#else
#define UTS_RELEASE "%d.%d.%d"
#define UTS_ARGS , LINUX_VERSION_CODE >> 16, (LINUX_VERSION_CODE >> 8) & 0xFF, LINUX_VERSION_CODE & 0xFF
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(BOOTCD))
		fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, UTS_SYSNAME " "
		  UTS_RELEASE " " ARCH_NAME AAC_DRIVERNAME " driver "
		  AAC_DRIVER_FULL_VERSION " " AAC_DRIVER_BUILD_DATE
		  "\nBAR0=0x%lx BAR1=0x%lx BAR2=0x%lx host->base=0x%lx rx=%p"
		  " base=%p Index=%p base_size=%lu" UTS_ARGS,
		  pci_resource_start(pdev, 0),
		  pci_resource_start(pdev, 1),
		  pci_resource_start(pdev, 2),
		  shost->base, aac->regs.rx, aac->base, aac->IndexRegs,
		  (unsigned long)aac->base_size));
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0))
		fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, UTS_SYSNAME " "
		  UTS_RELEASE " " ARCH_NAME AAC_DRIVERNAME " driver "
		  AAC_DRIVER_FULL_VERSION " " AAC_DRIVER_BUILD_DATE UTS_ARGS));
#endif
#endif
	}
#endif
	adbg_init(aac,KERN_INFO,"BAR0=0x%pa[p] BAR1=0x%pa[p] BAR2=0x%pa[p]"
	  " host->base=0x%lx rx=%p base=%p Index=%p base_size=%lu\n",
	  (void *)pci_resource_start(pdev, 0),
	  (void *)pci_resource_start(pdev, 1),
	  (void *)pci_resource_start(pdev, 2),
	  shost->base, aac->regs.rx, aac->base, aac->IndexRegs,
	  (unsigned long)aac->base_size);
	/*
	 *	Start any kernel threads needed
	 */
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	aac->thread_pid = kernel_thread((int (*)(void *))aac_command_thread,
	  aac, 0);
	if (aac->thread_pid < 0) {
		printk(KERN_ERR "aacraid: Unable to create command thread.\n");
#if (0 && defined(BOOTCD))
		fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aacraid: Unable to create command thread."));
#endif
		goto out_deinit;
	}
#else
	aac->thread = kthread_run(aac_command_thread, aac, AAC_DRIVERNAME);
	if (IS_ERR(aac->thread)) {
		printk(KERN_ERR "aacraid: Unable to create command thread.\n");
		error = PTR_ERR(aac->thread);
		aac->thread = NULL;
#if (0 && defined(BOOTCD))
		fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aacraid: Unable to create command thread."));
#endif
		goto out_deinit;
	}
#endif

	/*
	 * If we had set a smaller DMA mask earlier, set it to 4gig
	 * now since the adapter can dma data to at least a 4gig
	 * address space.
	 */
	if (aac_drivers[index].quirks & AAC_QUIRK_31BIT)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
		if (pci_set_dma_mask(pdev, DMA_32BIT_MASK))
#else
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)))
#endif
			goto out_deinit;

	aac->maximum_num_channels = aac_drivers[index].channels;
	error = aac_get_adapter_info(aac);
	if (error < 0)
		goto out_deinit;

	/*
	 * Lets override negotiations and drop the maximum SG limit to 34
	 */
	if ((aac_drivers[index].quirks & AAC_QUIRK_34SG) &&
			(shost->sg_tablesize > 34)) {
		shost->sg_tablesize = 34;
		shost->max_sectors = (shost->sg_tablesize * 8) + 112;
	}

	if ((aac_drivers[index].quirks & AAC_QUIRK_17SG) &&
			(shost->sg_tablesize > 17)) {
		shost->sg_tablesize = 17;
		shost->max_sectors = (shost->sg_tablesize * 8) + 112;
	}

#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)) || defined(PCI_HAS_SET_DMA_MAX_SEG_SIZE))
	error = pci_set_dma_max_seg_size(pdev,
		(aac->adapter_info.options & AAC_OPT_NEW_COMM) ?
			(shost->max_sectors << 9) : 65536);
	if (error)
		goto out_deinit;

#endif
	/*
	 * Firmware printf works only with older firmware.
	 */
	if (aac_drivers[index].quirks & AAC_QUIRK_34SG)
		aac->printf_enabled = 1;
	else
		aac->printf_enabled = 0;

	/*
	 * max channel will be the physical channels plus 1 virtual channel
	 * all containers are on the virtual channel 0 (CONTAINER_CHANNEL)
	 * physical channels are address by their actual physical number+1
	 */
	if (aac->nondasd_support || expose_physicals || aac->jbod)
		shost->max_channel = aac->maximum_num_channels;
	else
		shost->max_channel = 0;
#if defined(__powerpc__) || defined(__PPC__) || defined(__ppc__)
	aac_get_config_status(aac, 1);
#else
    aac_get_config_status(aac, 0);
#endif
	aac_get_containers(aac);
	list_add(&aac->entry, insert);

	shost->max_id = aac->maximum_num_containers;
	if (shost->max_id < aac->maximum_num_physicals)
		shost->max_id = aac->maximum_num_physicals;
	if (shost->max_id < MAXIMUM_NUM_CONTAINERS)
		shost->max_id = MAXIMUM_NUM_CONTAINERS;
	else
		shost->this_id = shost->max_id;

	/*
	 * dmb - we may need to move the setting of these parms somewhere else once
	 * we get a fib that can report the actual numbers
	 */
	shost->max_lun = AAC_MAX_LUN;

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4))
	scsi_register_uinfo(shost, pdev->bus->number, pdev->devfn, (void *)aac);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	vmk_scsi_register_uinfo(shost, pdev->bus->number, pdev->devfn, (void *)aac);
#else
	shost->xportFlags = VMKLNX_SCSI_TRANSPORT_TYPE_PSCSI;
#endif
#endif
	pci_set_drvdata(pdev, shost);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)) 
    error = scsi_init_shared_tag_map(shost, shost->can_queue);
	if (error)
		goto out_deinit;
#endif

	error = scsi_add_host(shost, &pdev->dev);
	if (error)
		goto out_deinit;
	scsi_scan_host(shost);

#if defined(__ESX5__)
	if (aac->pdev->device == PMC_DEVICE_S6 ||
	    aac->pdev->device == PMC_DEVICE_S7 ||
	    aac->pdev->device == PMC_DEVICE_S8 ||
	    aac->pdev->device == PMC_DEVICE_S9) {
		if (aac->max_msix > 1)
			vmklnx_scsi_register_poll_handler(shost, aac->msixentry[0].vector,
				aac->a_ops.adapter_intr_poll, &(aac->aac_msix[0]));
		else
			vmklnx_scsi_register_poll_handler(shost, aac->pdev->irq,
				aac->a_ops.adapter_intr, &(aac->aac_msix[0]));

	} else {
		vmklnx_scsi_register_poll_handler(shost, aac->pdev->irq,
			aac->a_ops.adapter_intr, aac);
	}
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || (0 && defined(BOOTCD)))
	fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
	  "aac_probe_one() returns success"));
#endif

	if (aac->pdev->device == PMC_DEVICE_S6) {
		if (aac->msi)
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This Tupelo driver is MSI enabled", aac->name, aac->id));
		else
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This Tupelo driver is INTx enabled", aac->name, aac->id));
	} else if(aac->pdev->device == PMC_DEVICE_S7 ||
		  aac->pdev->device == PMC_DEVICE_S8) {
		if (aac->max_msix > 1)
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This driver is MSI-x enabled", aac->name, aac->id));
		else if (aac->msi)
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This driver is MSI enabled", aac->name, aac->id));
		else
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This driver is INTx enabled", aac->name, aac->id));
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
	pci_enable_pcie_error_reporting(pdev);
#endif
	pci_save_state(pdev);
	return 0;

 out_deinit:
	__aac_shutdown(aac);
 out_unmap:
	aac_fib_map_free(aac);
	if (aac->comm_addr)
		pci_free_consistent(aac->pdev, aac->comm_size, aac->comm_addr,
		  aac->comm_phys);
	kfree(aac->queues);
	aac_adapter_ioremap(aac, 0);
	kfree(aac->fibs);
	kfree(aac->fsa_dev);
	kfree(aac->msix_vector_tbl);
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	spin_lock_destroy(&aac->fib_lock);
#endif
 out_free_host:
	scsi_host_put(shost);
 out_disable_pdev:
	pci_disable_device(pdev);
 out:
	return error;
}

#if (defined(CONFIG_PM))
extern void aac_define_int_mode(struct aac_dev *dev);
void aac_release_resources(struct aac_dev *aac) 
{
	aac_adapter_disable_int(aac);
	aac_free_irq(aac);
}

static int aac_acquire_resources(struct aac_dev *dev) {
	unsigned long status;
	/*
	 *	First clear out all interrupts.  Then enable the one's that we
	 *	can handle.
	 */
	while (!((status=src_readl(dev, MUnit.OMR)) & KERNEL_UP_AND_RUNNING) ||
		status == 0xffffffff)
			msleep(1);

	aac_adapter_disable_int(dev);
	aac_adapter_enable_int(dev);

	if ((dev->pdev->device == PMC_DEVICE_S7 ||
	     dev->pdev->device == PMC_DEVICE_S8 ||
	     dev->pdev->device == PMC_DEVICE_S9))
		aac_define_int_mode(dev);

	if (dev->msi_enabled)
		aac_src_access_devreg(dev, AAC_ENABLE_MSIX);
	
	if(aac_acquire_irq(dev))
		goto error_iounmap;

	aac_adapter_enable_int(dev);

	if (!dev->sync_mode)
		aac_adapter_start(dev);

#if defined(__powerpc__) || defined(__PPC__) || defined(__ppc__)
	aac_get_config_status(dev, 1);
#endif

	return 0;

error_iounmap:
	return -1;
	
}
static int aac_suspend(struct pci_dev *pdev, pm_message_t state) {
	
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct aac_dev *aac = (struct aac_dev *)shost->hostdata;
	
	scsi_block_requests(shost);
	aac_send_shutdown(aac);

	aac_release_resources(aac);

	pci_set_drvdata(pdev, shost);
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

static int aac_resume(struct pci_dev *pdev) {
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct aac_dev *aac = (struct aac_dev *)shost->hostdata;
	int r;
	
	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);
	r = pci_enable_device(pdev);

	if (r)
		goto fail_device;

	pci_set_master(pdev);
	if (aac_acquire_resources(aac))
		goto fail_device;
	/*
	* reset this flag to unblock ioctl() as it was set at aac_send_shutdown() to block ioctls from upperlayer
	*/
	aac->adapter_shutdown = 0;
	scsi_unblock_requests(shost);

        return 0;

fail_device:
	printk(KERN_INFO "%s%d: resume failed.\n", aac->name, aac->id);
	scsi_host_put(shost);
	pci_disable_device(pdev);
	return -ENODEV;
}
#endif

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)) || defined(PCI_HAS_SHUTDOWN)))
static void aac_shutdown(struct pci_dev *dev)
{
	struct Scsi_Host *shost = pci_get_drvdata(dev);
	scsi_block_requests(shost);
	__aac_shutdown((struct aac_dev *)shost->hostdata);
}

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
static void __devexit aac_remove_one(struct pci_dev *pdev)
#else
static void aac_remove_one(struct pci_dev *pdev)
#endif
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct aac_dev *aac = (struct aac_dev *)shost->hostdata;

	scsi_remove_host(shost);

	__aac_shutdown(aac);
	aac_fib_map_free(aac);
	pci_free_consistent(aac->pdev, aac->comm_size, aac->comm_addr,
			aac->comm_phys);
	kfree(aac->queues);

	aac_adapter_ioremap(aac, 0);

	kfree(aac->fibs);
	kfree(aac->fsa_dev);

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	spin_lock_destroy(&aac->fib_lock);
#endif
	list_del(&aac->entry);
	scsi_host_put(shost);
	pci_disable_device(pdev);
#if (!defined(HAS_BOOT_CONFIG))
	if (list_empty(&aac_devices)) {
		unregister_chrdev(aac_cfg_major, "aac");
		aac_cfg_major = -2;
	}
#endif
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
void aac_flush_ios(struct aac_dev *aac)
{
	int i;
	struct scsi_cmnd *cmd;

	for (i=0; i<aac->scsi_host_ptr->can_queue; i++)
	{
		cmd = (struct scsi_cmnd *)aac->fibs[i].callback_data;

		if ( cmd && (cmd->SCp.phase == AAC_OWNER_FIRMWARE))
		{
			scsi_dma_unmap(cmd);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
			aac_fib_free_tag(&aac->fibs[i]);
#else
			aac_fib_free(&aac->fibs[i]);
#endif

			if (aac->handle_pci_error)
				cmd->result = DID_NO_CONNECT << 16;
			else
				cmd->result = DID_RESET << 16;

			cmd->scsi_done(cmd);
		}
	}
}

pci_ers_result_t aac_pci_error_detected(struct pci_dev *pdev, 
					enum pci_channel_state error)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
 	struct aac_dev *aac = shost_priv(shost);

	printk("aacraid: PCI error detected %x\n", error);

        switch (error) {
        case pci_channel_io_normal:
        	return PCI_ERS_RESULT_CAN_RECOVER;
        case pci_channel_io_frozen:

		aac->handle_pci_error = 1;

		scsi_block_requests(aac->scsi_host_ptr);
		aac_flush_ios(aac);
		aac_release_resources(aac);

		pci_disable_pcie_error_reporting(pdev);
		aac_adapter_ioremap(aac, 0);

               	return PCI_ERS_RESULT_NEED_RESET;
        case pci_channel_io_perm_failure:
		aac->handle_pci_error = 1;
		aac_flush_ios(aac);
               	return PCI_ERS_RESULT_DISCONNECT;
        }

        return PCI_ERS_RESULT_NEED_RESET;
}

pci_ers_result_t aac_pci_mmio_enabled(struct pci_dev *dev)
{

	printk(KERN_ERR "aacraid: PCI error - mmio enabled \n");

        return PCI_ERS_RESULT_NEED_RESET;
}

pci_ers_result_t aac_pci_slot_reset(struct pci_dev *pdev)
{	
	printk(KERN_ERR "aacraid: PCI error - slot reset \n");
	
	pci_restore_state(pdev);
	if (pci_enable_device(pdev))
	{
		printk(KERN_WARNING AAC_DRIVERNAME
				": failed to enable slave\n");
		goto fail_device;
	}

	pci_set_master(pdev);

	if (pci_enable_device_mem(pdev))
	{
		printk(KERN_ERR"pci_enable_device_mem failed \n");
		goto fail_device;
	}

	return PCI_ERS_RESULT_RECOVERED;

fail_device:
	printk(KERN_ERR "aacraid: PCI error - slot reset failed\n");
	return PCI_ERS_RESULT_DISCONNECT;
}


void aac_pci_resume(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct scsi_device *sdev=NULL;
	struct aac_dev *aac = (struct aac_dev *)shost_priv(shost);

	pci_cleanup_aer_uncorrect_error_status(pdev);

	if (aac_adapter_ioremap(aac, aac->base_size)) {

		printk(KERN_ERR "aacraid: ioremap failed\n");
		/* remap failed, go back ... */
		aac->comm_interface = AAC_COMM_PRODUCER;
		if (aac_adapter_ioremap(aac, AAC_MIN_FOOTPRINT_SIZE)) {
			printk(KERN_WARNING
					"aacraid: unable to map adapter.\n");

			return;
		}
	}

	msleep(10000);

	aac_acquire_resources(aac);

	/*
	 * reset this flag to unblock ioctl() as it was set at aac_send_shutdown() to block ioctls from upperlayer
	 */
	aac->adapter_shutdown = 0;
	aac->handle_pci_error = 0;

	shost_for_each_device(sdev, shost)
		if (sdev->sdev_state == SDEV_OFFLINE)
			sdev->sdev_state = SDEV_RUNNING;
	scsi_unblock_requests(aac->scsi_host_ptr);
	scsi_scan_host(aac->scsi_host_ptr);
	pci_save_state(pdev);

	printk(KERN_ERR "aacraid: PCI error - resume\n");
}

static struct pci_error_handlers aac_pci_err_handler = {
	.error_detected 	= aac_pci_error_detected,
	.mmio_enabled 		= aac_pci_mmio_enabled,
	.slot_reset 		= aac_pci_slot_reset,
	.resume 		= aac_pci_resume,
};
#endif

static struct pci_driver aac_pci_driver = {
	.name		= AAC_DRIVERNAME,
	.id_table	= aac_pci_tbl,
	.probe		= aac_probe_one,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
	.remove		= __devexit_p(aac_remove_one),
#else
	.remove		= aac_remove_one,
#endif
#if (defined(CONFIG_PM))
	.suspend	= aac_suspend,
	.resume		= aac_resume,
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)) || defined(PCI_HAS_SHUTDOWN)))
	.shutdown	= aac_shutdown,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
	.err_handler    = &aac_pci_err_handler,
#endif
};

//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN)))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
static int aac_reboot_event(struct notifier_block * n, ulong code, void *p)
{
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO "aac_reboot_event(%p,%lu,%p) - ENTER\n", n, code, p);
#endif
	if ((code == SYS_RESTART)
	 || (code == SYS_HALT)
	 || (code == SYS_POWER_OFF)) {
		struct aac_dev *aac;

		/*
		 * We would like to do a block and __aac_shutdown but we
		 * can not because sd_shutdown has yet to be called and
		 * it will trigger additional commands to the driver.
		 */
		list_for_each_entry(aac, &aac_devices, entry)
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
		{
#endif
			aac_send_shutdown(aac);
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
			aac->shutdown = 1;
		}
#endif
	}
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	printk(KERN_INFO "aac_reboot_event(%p,%lu,%p) - EXIT NOTIFY_DONE\n",
	  n, code, p);
#endif
	return NOTIFY_DONE;
}

static struct notifier_block aac_reboot_notifier =
{
	aac_reboot_event,
	NULL,
	0
};

#endif
static int __init aac_init(void)
{
	int error;

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4) && !defined(__VMKLNX__))
	if (!vmk_set_module_version(AAC_DRIVERNAME " (" AAC_DRIVER_BUILD_DATE ")"))
		return 0;
#endif
	spin_lock_init(&io_request_lock);
#if (!defined(__VMKLNX__))
	aac_driver_template.driverLock = &io_request_lock;
#endif
#endif
	printk(KERN_INFO "Adaptec %s driver %s\n",
	  AAC_DRIVERNAME, aac_driver_version);
//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN)))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	register_reboot_notifier(&aac_reboot_notifier);
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(SCSI_HAS_SCSI_IN_DETECTION) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	scsi_register_module(MODULE_SCSI_HA,&aac_driver_template);
	/* Reverse 'detect' action */
	aac_driver_template.present = 0;
#endif
#if (defined(__VMKLNX30__) && (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)))
	if (!vmklnx_set_module_version("%s", aac_driver_version))
		return -ENODEV;
#endif

	error = pci_register_driver(&aac_pci_driver);
#if (defined(CONFIG_COMMUNITY_KERNEL))
	if (error < 0)
//#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	{
		unregister_reboot_notifier(&aac_reboot_notifier);
#endif
		return error;
//#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	}
#endif
#else
	if (error < 0 || list_empty(&aac_devices)) {
//#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		unregister_reboot_notifier(&aac_reboot_notifier);
#endif
		if (error >= 0) {
			pci_unregister_driver(&aac_pci_driver);
			error = -ENODEV;
		}
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(SCSI_HAS_SCSI_IN_DETECTION) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
		scsi_unregister_module(MODULE_SCSI_HA,&aac_driver_template);
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
		spin_lock_destroy(&io_request_lock);
#endif
		return error;
	}
#endif

	aac_init_char();

#if (!defined(HAS_BOOT_CONFIG))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT))
	aac_ioctl32(FSACTL_MINIPORT_REV_CHECK, sys_ioctl);
	aac_ioctl32(FSACTL_SENDFIB, sys_ioctl);
	aac_ioctl32(FSACTL_OPEN_GET_ADAPTER_FIB, sys_ioctl);
	aac_ioctl32(FSACTL_GET_NEXT_ADAPTER_FIB,
	  aac_get_next_adapter_fib_ioctl);
	aac_ioctl32(FSACTL_CLOSE_GET_ADAPTER_FIB, sys_ioctl);
	aac_ioctl32(FSACTL_SEND_RAW_SRB, sys_ioctl);
	aac_ioctl32(FSACTL_GET_PCI_INFO, sys_ioctl);
	aac_ioctl32(FSACTL_QUERY_DISK, sys_ioctl);
	aac_ioctl32(FSACTL_DELETE_DISK, sys_ioctl);
	aac_ioctl32(FSACTL_FORCE_DELETE_DISK, sys_ioctl);
	aac_ioctl32(FSACTL_GET_CONTAINERS, sys_ioctl);
#if (defined(FSACTL_REGISTER_FIB_SEND))
	aac_ioctl32(FSACTL_REGISTER_FIB_SEND, sys_ioctl);
#endif
	aac_ioctl32(FSACTL_GET_VERSION_MATCHING, sys_ioctl);
	aac_ioctl32(FSACTL_SEND_LARGE_FIB, sys_ioctl);
#if (defined(AAC_CSMI))
	aac_csmi_register_ioctl32_conversion();
#endif
#endif
#endif
#endif

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && !defined(SCSI_HAS_SCSI_IN_DETECTION) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	/* Trigger a target scan in the 2.4 tree */
	if (!aac_dummy) {
		aac_dummy = scsi_host_alloc(&aac_driver_template,0);
	}
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && (!defined(SCSI_HAS_SCSI_IN_DETECTION) || defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__)))
	scsi_register_module(MODULE_SCSI_HA,&aac_driver_template);
#endif

	return 0;
}

static void __exit aac_exit(void)
{
#if (!defined(HAS_BOOT_CONFIG))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT))
	unregister_ioctl32_conversion(FSACTL_MINIPORT_REV_CHECK);
	unregister_ioctl32_conversion(FSACTL_SENDFIB);
	unregister_ioctl32_conversion(FSACTL_OPEN_GET_ADAPTER_FIB);
	unregister_ioctl32_conversion(FSACTL_GET_NEXT_ADAPTER_FIB);
	unregister_ioctl32_conversion(FSACTL_CLOSE_GET_ADAPTER_FIB);
	unregister_ioctl32_conversion(FSACTL_SEND_RAW_SRB);
	unregister_ioctl32_conversion(FSACTL_GET_PCI_INFO);
	unregister_ioctl32_conversion(FSACTL_QUERY_DISK);
	unregister_ioctl32_conversion(FSACTL_DELETE_DISK);
	unregister_ioctl32_conversion(FSACTL_FORCE_DELETE_DISK);
	unregister_ioctl32_conversion(FSACTL_GET_CONTAINERS);
#if (defined(FSACTL_REGISTER_FIB_SEND))
	unregister_ioctl32_conversion(FSACTL_REGISTER_FIB_SEND);
#endif
	unregister_ioctl32_conversion(FSACTL_GET_VERSION_MATCHING);
	unregister_ioctl32_conversion(FSACTL_SEND_LARGE_FIB);
#if (defined(AAC_CSMI))
	aac_csmi_unregister_ioctl32_conversion();
#endif
#endif
#endif
	if (aac_cfg_major > -1)
	{
		unregister_chrdev(aac_cfg_major, "aac");
		aac_cfg_major = -1;
	}
#endif
//#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	unregister_reboot_notifier(&aac_reboot_notifier);
#endif
	pci_unregister_driver(&aac_pci_driver);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	scsi_unregister_module(MODULE_SCSI_HA,&aac_driver_template);
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	spin_lock_destroy(&io_request_lock);
#endif
}

module_init(aac_init);
module_exit(aac_exit);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
EXPORT_NO_SYMBOLS;
#endif
