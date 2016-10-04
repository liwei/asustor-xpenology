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
 *  aachba.c
 *
 * Abstract: Contains Interfaces to manage IOs.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
//#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/version.h> /* For the following test */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/blkdev.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#endif
#include <asm/uaccess.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,16))
#include <linux/highmem.h> /* For flush_kernel_dcache_page */
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || (LINUX_VERSION_CODE >=  KERNEL_VERSION(3,2,0)))
#include <linux/module.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
#define MAJOR_NR SCSI_DISK0_MAJOR	/* For DEVICE_NR() */
#include <linux/blk.h>	/* for DEVICE_NR & io_request_lock definition */
#include "scsi.h"
#include "hosts.h"
#include "sd.h"
#define no_uld_attach hostdata
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6))
#include <linux/moduleparam.h>
#endif
#include <scsi/scsi.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,1)) && !defined(DID_OK))
#define DID_OK         0x00
#define DID_NO_CONNECT 0x01
#define DID_TIME_OUT   0x03
#define DID_BAD_TARGET 0x04
#define DID_ABORT      0x05
#define DID_PARITY     0x06
#define DID_ERROR      0x07
#define DID_RESET      0x08
#define SUCCESS        0x2002
#define FAILED         0x2003
#define SCSI_MLQUEUE_DEVICE_BUSY 0x1056
#define SCSI_MLQUEUE_HOST_BUSY   0x1055
#endif
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && defined(DID_BUS_BUSY) && !defined(BLIST_NO_ULD_ATTACH))
#include <scsi/scsi_devinfo.h>	/* Pick up BLIST_NO_ULD_ATTACH? */
#endif
#include <scsi/scsi_host.h>
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#include <scsi/scsi_tcq.h> /* For MSG_ORDERED_TAG */
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && !defined(BLIST_NO_ULD_ATTACH))
#define no_uld_attach inq_periph_qual
#elif ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(BLIST_NO_ULD_ATTACH))
#define no_uld_attach hostdata
#endif
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
#include "scsi_priv.h" /* For SCSI_CMND_MAGIC */
#endif
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#if (defined(MODULE))
#include <linux/proc_fs.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
#include <linux/smp_lock.h>
#else
#include <linux/mutex.h>
#endif
#endif
#if (defined(HAS_BOOTSETUP_H))
#include <asm/bootsetup.h>
#elif (!defined(HAS_NOT_SETUP))
#include <asm/setup.h>
#endif
#ifndef COMMAND_LINE_SIZE
# define COMMAND_LINE_SIZE 256
#endif
#endif

#include "aacraid.h"
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#include "fwdebug.h"
#endif

/* values for inqd_pdt: Peripheral device type in plain English */
#define	INQD_PDT_DA	0x00	/* Direct-access (DISK) device */
#define	INQD_PDT_PROC	0x03	/* Processor device */
#define	INQD_PDT_CHNGR	0x08	/* Changer (jukebox, scsi2) */
#define	INQD_PDT_COMM	0x09	/* Communication device (scsi2) */
#define	INQD_PDT_NOLUN2 0x1f	/* Unknown Device (scsi2) */
#define	INQD_PDT_NOLUN	0x7f	/* Logical Unit Not Present */

#define	INQD_PDT_DMASK	0x1F	/* Peripheral Device Type Mask */
#define	INQD_PDT_QMASK	0xE0	/* Peripheral Device Qualifer Mask */

/*
 *	Sense codes
 */

#define SENCODE_NO_SENSE			0x00
#define SENCODE_END_OF_DATA			0x00
#define SENCODE_BECOMING_READY			0x04
#define SENCODE_INIT_CMD_REQUIRED		0x04
#define SENCODE_UNRECOVERED_READ_ERROR		0x11
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#define SENCODE_DATA_PROTECT			0x0E
#endif
#define SENCODE_PARAM_LIST_LENGTH_ERROR		0x1A
#define SENCODE_INVALID_COMMAND			0x20
#define SENCODE_LBA_OUT_OF_RANGE		0x21
#define SENCODE_INVALID_CDB_FIELD		0x24
#define SENCODE_LUN_NOT_SUPPORTED		0x25
#define SENCODE_INVALID_PARAM_FIELD		0x26
#define SENCODE_PARAM_NOT_SUPPORTED		0x26
#define SENCODE_PARAM_VALUE_INVALID		0x26
#define SENCODE_RESET_OCCURRED			0x29
#define SENCODE_LUN_NOT_SELF_CONFIGURED_YET	0x3E
#define SENCODE_INQUIRY_DATA_CHANGED		0x3F
#define SENCODE_SAVING_PARAMS_NOT_SUPPORTED	0x39
#define SENCODE_DIAGNOSTIC_FAILURE		0x40
#define SENCODE_INTERNAL_TARGET_FAILURE		0x44
#define SENCODE_INVALID_MESSAGE_ERROR		0x49
#define SENCODE_LUN_FAILED_SELF_CONFIG		0x4c
#define SENCODE_OVERLAPPED_COMMAND		0x4E

/*
 *	Additional sense codes
 */

#define ASENCODE_NO_SENSE			0x00
#define ASENCODE_END_OF_DATA			0x05
#define ASENCODE_BECOMING_READY			0x01
#define ASENCODE_INIT_CMD_REQUIRED		0x02
#define ASENCODE_PARAM_LIST_LENGTH_ERROR	0x00
#define ASENCODE_INVALID_COMMAND		0x00
#define ASENCODE_LBA_OUT_OF_RANGE		0x00
#define ASENCODE_INVALID_CDB_FIELD		0x00
#define ASENCODE_LUN_NOT_SUPPORTED		0x00
#define ASENCODE_INVALID_PARAM_FIELD		0x00
#define ASENCODE_PARAM_NOT_SUPPORTED		0x01
#define ASENCODE_PARAM_VALUE_INVALID		0x02
#define ASENCODE_RESET_OCCURRED			0x00
#define ASENCODE_LUN_NOT_SELF_CONFIGURED_YET	0x00
#define ASENCODE_INQUIRY_DATA_CHANGED		0x03
#define ASENCODE_SAVING_PARAMS_NOT_SUPPORTED	0x00
#define ASENCODE_DIAGNOSTIC_FAILURE		0x80
#define ASENCODE_INTERNAL_TARGET_FAILURE	0x00
#define ASENCODE_INVALID_MESSAGE_ERROR		0x00
#define ASENCODE_LUN_FAILED_SELF_CONFIG		0x00
#define ASENCODE_OVERLAPPED_COMMAND		0x00

#define BYTE0(x) (unsigned char)(x)
#define BYTE1(x) (unsigned char)((x) >> 8)
#define BYTE2(x) (unsigned char)((x) >> 16)
#define BYTE3(x) (unsigned char)((x) >> 24)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
/* compatibility */
#ifndef SAM_STAT_CHECK_CONDITION
# define SAM_STAT_CHECK_CONDITION	(CHECK_CONDITION << 1)
#endif
#ifndef SAM_STAT_GOOD
# define SAM_STAT_GOOD			(GOOD << 1)
#endif
#ifndef SAM_STAT_TASK_SET_FULL
# define SAM_STAT_TASK_SET_FULL		(QUEUE_FULL << 1)
#endif
#ifndef SAM_STAT_BUSY
# define SAM_STAT_BUSY			(BUSY << 1)
#endif
#ifndef SAM_STAT_RESERVATION_CONFLICT
# define SAM_STAT_RESERVATION_CONFLICT	(RESERVATION_CONFLICT << 1)
#endif
#ifndef SAM_STAT_TASK_ABORTED
# define SAM_STAT_TASK_ABORTED		(TASK_ABORTED << 1)
#endif

#endif

/* ATA pass thru commands */
#ifndef ATA_12
#define ATA_12                0xa1      /* 12-byte pass-thru */
#endif

#ifndef ATA_16
#define ATA_16                0x85      /* 16-byte pass-thru */
#endif

/* MODE_SENSE data format */
typedef struct {
	struct {
		u8	data_length;
		u8	med_type;
		u8	dev_par;
		u8	bd_length;
	} __attribute__((packed)) hd;
	struct {
		u8	dens_code;
		u8	block_count[3];
		u8	reserved;
		u8	block_length[3];
	} __attribute__((packed)) bd;
		u8	mpc_buf[3];
} __attribute__((packed)) aac_modep_data;

/* MODE_SENSE_10 data format */
typedef struct {
	struct {
		u8	data_length[2];
		u8	med_type;
		u8	dev_par;
		u8	rsrvd[2];
		u8	bd_length[2];
	} __attribute__((packed)) hd;
	struct {
		u8	dens_code;
		u8	block_count[3];
		u8	reserved;
		u8	block_length[3];
	} __attribute__((packed)) bd;
		u8	mpc_buf[3];
} __attribute__((packed)) aac_modep10_data;

/*------------------------------------------------------------------------------
 *              S T R U C T S / T Y P E D E F S
 *----------------------------------------------------------------------------*/
/* SCSI inquiry data */
struct inquiry_data {
	u8 inqd_pdt;	/* Peripheral qualifier | Peripheral Device Type */
	u8 inqd_dtq;	/* RMB | Device Type Qualifier */
	u8 inqd_ver;	/* ISO version | ECMA version | ANSI-approved version */
	u8 inqd_rdf;	/* AENC | TrmIOP | Response data format */
	u8 inqd_len;	/* Additional length (n-4) */
	u8 inqd_pad1[2];/* Reserved - must be zero */
	u8 inqd_pad2;	/* RelAdr | WBus32 | WBus16 |  Sync  | Linked |Reserved| CmdQue | SftRe */
	u8 inqd_vid[8];	/* Vendor ID */
	u8 inqd_pid[16];/* Product ID */
	u8 inqd_prl[4];	/* Product Revision Level */
};

/* Excluding SUSE as it has issues when inbox driver does not have this support but outbox has it.
  Because SUSE uses /dev/disk/by-id mapping entries in the OS grub config and VPD 0X83 creates conflicts */
#if (!defined(CONFIG_SUSE_KERNEL))
/* Added for VPD 0x83 */
typedef struct {
    u8 CodeSet : 4;          // VPD_CODE_SET
    u8 Reserved : 4;
    u8 IdentifierType : 4;   // VPD_IDENTIFIER_TYPE
    u8 Reserved2 : 4;
    u8 Reserved3;
    u8 IdentifierLength;
    u8 VendId[8];
    u8 ProductId[16];
    u8 SerialNumber[8];                   // SN in ASCII

} TVPD_ID_Descriptor_Type_1;

typedef struct {
    u8 CodeSet : 4;          // VPD_CODE_SET
    u8 Reserved : 4;
    u8 IdentifierType : 4;   // VPD_IDENTIFIER_TYPE
    u8 Reserved2 : 4;
    u8 Reserved3;
    u8 IdentifierLength;
        struct TEU64Id {
          u32 Serial;
          u8 Reserved; // The serial number supposed to be 40 bits, bit we only support 32, so make the last byte zero.
          u8 VendId[3];
    } EU64Id;

} TVPD_ID_Descriptor_Type_2;

typedef struct {
    u8 CodeSet : 4;          // VPD_CODE_SET
    u8 Reserved : 4;
    u8 IdentifierType : 4;   // VPD_IDENTIFIER_TYPE
    u8 Reserved2 : 4;
    u8 Reserved3;
    u8 IdentifierLength;
    u8 Identifier[16];
} TVPD_ID_Descriptor_Type_3;

typedef struct {
    u8 DeviceType : 5;
    u8 DeviceTypeQualifier : 3;
    u8 PageCode;
    u8 Reserved;
    u8 PageLength;
    TVPD_ID_Descriptor_Type_1 IdDescriptorType1;
    TVPD_ID_Descriptor_Type_2 IdDescriptorType2;
    TVPD_ID_Descriptor_Type_3 IdDescriptorType3;
} TVPD_Page83;

#endif


/*
 *              M O D U L E   G L O B A L S
 */

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
static int aac_build_sg(struct scsi_cmnd* scsicmd, struct sgmap* sgmap);
static int aac_build_sg64(struct scsi_cmnd* scsicmd, struct sgmap64* psg);
static int aac_build_sgraw(struct scsi_cmnd* scsicmd, struct sgmapraw* psg);
static int aac_build_sgraw2(struct scsi_cmnd* scsicmd, struct aac_raw_io2* rio2, int sg_max);
static int aac_build_sghba(struct scsi_cmnd* scsicmd, struct aac_hba_cmd_req * hbacmd, int sg_max, u64 sg_address);
static int aac_scsi_cmd(struct scsi_cmnd * scsicmd);
static int aac_scsi_cmd_apre(struct scsi_cmnd * scsicmd);
#else
static long aac_build_sg(struct scsi_cmnd* scsicmd, struct sgmap* sgmap);
static long aac_build_sg64(struct scsi_cmnd* scsicmd, struct sgmap64* psg);
static long aac_build_sgraw(struct scsi_cmnd* scsicmd, struct sgmapraw* psg);
static long aac_build_sgraw2(struct scsi_cmnd* scsicmd, struct aac_raw_io2* rio2, int sg_max);
static long aac_build_sghba(struct scsi_cmnd* scsicmd, struct aac_hba_cmd_req * hbacmd, int sg_max, u64 sg_address);
#endif
static int aac_convert_sgraw2(struct aac_raw_io2* rio2, int pages, int nseg, int nseg_new);
static int aac_send_srb_fib(struct scsi_cmnd* scsicmd);
static int aac_send_hba_fib(struct scsi_cmnd* scsicmd);
#ifdef AAC_DETAILED_STATUS_INFO
static char *aac_get_status_string(u32 status);
#endif

/*
 *	Non dasd selection is handled entirely in aachba now
 */

static int nondasd = -1;
static int aac_cache = 2;	/* WCE=0 to avoid performance problems */
static int dacmode = -1;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
int aac_msi;
#else

#endif
#if (defined(__arm__) || defined(CONFIG_EXTERNAL))
int aac_commit = 1;
int startup_timeout = 540;
int aif_timeout = 540;
#else
int aac_commit = -1;
int startup_timeout = 180;
int aif_timeout = 120;
#endif

int aac_sync_mode = 0;  	/* only sync. transfer - disabled */
int aac_convert_sgl = 1;	/* convert non-conformable s/g list - enabled */
int aac_hba_mode = 1;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(nondasd, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(nondasd, "i");
#endif
MODULE_PARM_DESC(nondasd, "Control scanning of hba for nondasd devices."
	" 0=off, 1=on");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param_named(cache, aac_cache, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(cache, "Disable Queue Flush commands:\n"
	"\tbit 0 - Disable FUA in WRITE SCSI commands\n"
	"\tbit 1 - Disable SYNCHRONIZE_CACHE SCSI command\n"
	"\tbit 2 - Disable only if Battery is protecting Cache");
#else
MODULE_PARM(aac_cache, "i");
MODULE_PARM_DESC(aac_cache, "Disable Queue Flush commands:\n"
	"\tbit 0 - Disable FUA in WRITE SCSI commands\n"
	"\tbit 1 - Disable SYNCHRONIZE_CACHE SCSI command\n"
	"\tbit 2 - Disable only if Battery is protecting Cache");
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(dacmode, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(dacmode, "i");
#endif
MODULE_PARM_DESC(dacmode, "Control whether dma addressing is using 64 bit DAC."
	" 0=off, 1=on");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(aac_sync_mode, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(aac_sync_mode, "i");
#endif
MODULE_PARM_DESC(aac_sync_mode, "Force sync. transfer mode"
	" 0=off, 1=on");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(aac_convert_sgl, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(aac_convert_sgl, "i");
#endif
MODULE_PARM_DESC(aac_convert_sgl, "Convert non-conformable s/g list"
	" 0=off, 1=on");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(aac_hba_mode, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(aac_hba_mode, "i");
#endif
MODULE_PARM_DESC(aac_hba_mode, "HBA (bypass) mode support"
	" 0=off, 1=on");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param_named(commit, aac_commit, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(commit, "Control whether a COMMIT_CONFIG is issued to the"
	" adapter for foreign arrays.\n"
	"This is typically needed in systems that do not have a BIOS."
	" 0=off, 1=on");
#else
MODULE_PARM(aac_commit, "i");
MODULE_PARM_DESC(aac_commit, "Control whether a COMMIT_CONFIG is issued to the"
	" adapter for foreign arrays.\n"
	"This is typically needed in systems that do not have a BIOS."
	" 0=off, 1=on");
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param_named(msi, aac_msi, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(msi, "IRQ handling."
	" 0=PIC(default), 1=MSI, 2=MSI-X(unsupported, uses MSI)");
#else
MODULE_PARM(aac_msi, "i");
MODULE_PARM_DESC(aac_msi, "IRQ handling."
	" 0=PIC(default), 1=MSI, 2=MSI-X(unsupported, uses MSI)");
#endif
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(startup_timeout, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(startup_timeout, "i");
#endif
MODULE_PARM_DESC(startup_timeout, "The duration of time in seconds to wait for"
	" adapter to have it's kernel up and\n"
	"running. This is typically adjusted for large systems that do not"
	" have a BIOS.");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(aif_timeout, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(aif_timeout, "i");
#endif
MODULE_PARM_DESC(aif_timeout, "The duration of time in seconds to wait for"
	" applications to pick up AIFs before\n"
	"deregistering them. This is typically adjusted for heavily burdened"
	" systems.");

#if (!defined(CONFIG_COMMUNITY_KERNEL))
#if (defined(__arm__) || defined(CONFIG_EXTERNAL) || (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
static int coalescethreshold = 0;
#else
static int coalescethreshold = 16; /* 8KB coalesce knee */
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(coalescethreshold, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(coalescethreshold, "i");
#endif
MODULE_PARM_DESC(coalescethreshold, "Control the maximum block size of"
	" sequential requests that are fed back to the scsi_merge layer for"
	" coalescing. 0=off, 16 block (8KB) default.");

#endif
int numacb = -1;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(numacb, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(numacb, "i");
#endif
MODULE_PARM_DESC(numacb, "Request a limit to the number of adapter control"
	" blocks (FIB) allocated. Valid values are 512 and down. Default is"
	" to use suggestion from Firmware.");

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
int aac_remove_devnodes  = 0;
#else
int aac_remove_devnodes  = 1;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(aac_remove_devnodes, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(aac_remove_devnodes, "i");
#endif
MODULE_PARM_DESC(aac_remove_devnodes, "Remove device nodes(/dev/sd* and /dev/sg*) permanently when the device goes to offline state."
	" 0=off, 1=on(Default).");

int update_interval = 30 * 60;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(update_interval, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(update_interval, "i");
#endif
MODULE_PARM_DESC(update_interval, "Interval in seconds between time sync"
	" updates issued to adapter.");

int check_interval = 24 * 60 * 60;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(check_interval, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(check_interval, "i");
#endif
MODULE_PARM_DESC(check_interval, "Interval in seconds between adapter health"
	" checks.");

int aac_check_reset = 1;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param_named(check_reset, aac_check_reset, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(check_reset, "If adapter fails health check, reset the"
	" adapter. a value of -1 forces the reset to adapters programmed to"
	" ignore it.");
#else
MODULE_PARM(aac_check_reset, "i");
MODULE_PARM_DESC(aac_check_reset, "If adapter fails health check, reset the"
	" adapter. a value of -1 forces the reset to adapters programmed to"
	" ignore it.");
#endif
#if (defined(AAC_EXTENDED_TIMEOUT))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
int extendedtimeout = -1;
module_param(extendedtimeout, int, S_IRUGO|S_IWUSR);
#else
static int extendedtimeout = -1;
MODULE_PARM(extendedtimeout, "i");
#endif
MODULE_PARM_DESC(extendedtimeout, "Request a specific timeout to override I/O"
	" requests issed to the adapter.");
#endif

#if (defined(HAS_BOOT_CONFIG) || (defined(BOOTCD) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7))))
int expose_physicals = 0;
#else
int expose_physicals = -1;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(expose_physicals, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(expose_physicals, "i");
#endif
MODULE_PARM_DESC(expose_physicals, "Expose physical components of the arrays."
	" -1=protect 0=off, 1=on");

#if (defined(HAS_BOOT_CONFIG) || (defined(BOOTCD) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7))))
int expose_hidden_space = 0;
#else
int expose_hidden_space = -1;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param(expose_hidden_space, int, S_IRUGO|S_IWUSR);
#else
MODULE_PARM(expose_hidden_space, "i");
#endif
MODULE_PARM_DESC(expose_hidden_space, "Expose hidden space of the Array."
	" -1=protect 0=off, 1=on");

int aac_reset_devices;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param_named(reset_devices, aac_reset_devices, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(reset_devices, "Force an adapter reset at initialization.");
#else
MODULE_PARM(aac_reset_devices, "i");
MODULE_PARM_DESC(aac_reset_devices, "Force an adapter reset at initialization.");
#endif

#if ((LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)) && (defined(CONFIG_SLES_KERNEL) || defined(CONFIG_SUSE_KERNEL)) && defined(CONFIG_SLE_SP))
#if (CONFIG_SLE_SP == 1)
int aac_wwn = 2;
#else
int aac_wwn = 1;
#endif
#elif (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
int aac_wwn = 1;
#else
int aac_wwn = 1;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
module_param_named(wwn, aac_wwn, int, S_IRUGO|S_IWUSR);
#if ((LINUX_VERSION_CODE == KERNEL_VERSION(2,6,16)) && (defined(CONFIG_SLES_KERNEL) || defined(CONFIG_SUSE_KERNEL)) && defined(CONFIG_SLE_SP))
#if (CONFIG_SLE_SP == 1)
MODULE_PARM_DESC(wwn, "Select a WWN type for the arrays:\n"
	"\t0 - Disable\n"
	"\t1 - Array Meta Data Signature\n"
	"\t2 - Adapter Serial Number (default)");
#else
MODULE_PARM_DESC(wwn, "Select a WWN type for the arrays:\n"
	"\t0 - Disable\n"
	"\t1 - Array Meta Data Signature (default)\n"
	"\t2 - Adapter Serial Number");
#endif
#else
MODULE_PARM_DESC(wwn, "Select a WWN type for the arrays:\n"
	"\t0 - Disable\n"
	"\t1 - Array Meta Data Signature (default)\n"
	"\t2 - Adapter Serial Number");
#endif
#else
MODULE_PARM(aac_wwn, "i");
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,21)) && (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__)))
MODULE_PARM_DESC(aac_wwn, "Select a WWN type for the arrays:\n"
	"\t0 - Disable (default)\n"
	"\t1 - Array Meta Data Signature\n"
	"\t2 - Adapter Serial Number");
#else
MODULE_PARM_DESC(aac_wwn, "Select a WWN type for the arrays:\n"
	"\t0 - Disable\n"
	"\t1 - Array Meta Data Signature (default)\n"
	"\t2 - Adapter Serial Number");
#endif
#endif


#if (!defined(CONFIG_COMMUNITY_KERNEL) && !defined(__VMKLNX30__) && !defined(__VMKLNX__) && ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || !defined(HAS_BOOT_CONFIG)))
static char * aacraid;

static int aacraid_setup(struct aac_dev *dev, char *str)
{
	int i;
	char *key;
	char *value;
#if (defined(CONFIG_SLES_KERNEL) || defined(CONFIG_SUSE_KERNEL))
	static int dud = 0;
#endif
	struct {
		char * option_name;
		int * option_flag;
		int option_value;
	} options[] = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		{ "nondasd", &nondasd, 1 },
		{ "cache", &aac_cache, 2 },
		{ "dacmode", &dacmode, 1 },
		{ "sync_mode", &aac_sync_mode, 0 },
		{ "convert_sgl", &aac_convert_sgl, 1 },
		{ "hba_mode", &aac_hba_mode, 1 },
		{ "commit", &aac_commit, 1 },
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
		{ "msi", &aac_msi, 0 ),
#endif
#if (defined(__arm__) || defined(CONFIG_EXTERNAL) || (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
		{ "coalescethreshold", &coalescethreshold, 0 },
#else
		{ "coalescethreshold", &coalescethreshold, 16 },
#endif
		{ "update_interval", &update_interval, 30 * 60 },
		{ "check_interval", &check_interval, -1 },
		{ "check_reset", &aac_check_reset, 1 },
#if (defined(AAC_EXTENDED_TIMEOUT))
		{ "extendedtimeout", &extendedtimeout, AAC_EXTENDED_TIMEOUT },
#endif
		{ "expose_physicals", &expose_physicals, -1 },
		{ "expose_hidden_space", &expose_hidden_space, -1},
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
		{ "aac_remove_devnodes", &aac_remove_devnodes, 0},
#else
		{ "aac_remove_devnodes", &aac_remove_devnodes, 1},
#endif
		{ "reset_devices", &aac_reset_devices, 1 },
#endif
		{ "dd", &expose_physicals, 0 },
#if (defined(CONFIG_SLES_KERNEL) || defined(CONFIG_SUSE_KERNEL))
		{ "dud", &dud, 0 },
#endif
	};

	adbg_setup(dev,KERN_INFO,"aacraid_setup(\"%s\")\n", (str) ? str : "<null>");
	if (str) while ((key = strsep(&str, ",; \t\r\n"))) {
		if (!*key)
			continue;
		if ((strnicmp (key, "aacraid", 7) == 0)
		 && ((key[7] == '.') || (key[7] == '=')))
			key += 8;
		if (((value = strchr(key, ':')))
		 || ((value = strchr(key, '='))))
			*value++ = '\0';
		for (i = 0; i < (sizeof (options) / sizeof (options[0])); i++) {
			if (strnicmp (key, options[i].option_name,
			     strlen(options[i].option_name)) == 0) {
				*options[i].option_flag
				  = (value)
				    ? simple_strtoul(value, NULL, 0)
				    : options[i].option_value;
				break;
			}
		}
	}
#if (defined(CONFIG_SLES_KERNEL) || defined(CONFIG_SUSE_KERNEL))
	/* SuSE special */
	if (dud)
		expose_physicals = 0;
#endif

	return (1);
}

#endif
static inline int aac_valid_context(struct scsi_cmnd *scsicmd,
		struct fib *fibptr) {
	struct scsi_device *device;

	if (unlikely(!scsicmd || !scsicmd->scsi_done)) {
		dprintk((KERN_WARNING "aac_valid_context: scsi command corrupt\n"));
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT) || (0 && defined(BOOTCD)))
		if (!nblank(dprintk(x)))
			printk(KERN_WARNING
			  "aac_valid_context: scsi command corrupt %p->scsi_done=%p\n",
			  scsicmd, (scsicmd &&
			  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL))
			    ? scsicmd->scsi_done
			    : (void (*)(struct scsi_cmnd*))(uintptr_t)-1LL);
		if (nblank(fwprintf(x))) {
			extern struct list_head aac_devices; /* in linit.c */
			struct aac_dev *aac;
			list_for_each_entry(aac, &aac_devices, entry) {
				fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
				  "scsi command corrupt %p->scsi_done=%p",
				  scsicmd, (scsicmd &&
				  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL))
				    ? scsicmd->scsi_done
				    : (void (*)(struct scsi_cmnd*))(uintptr_t)-1LL));
			}
		}
#endif
		aac_fib_complete(fibptr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		aac_fib_free_tag(fibptr);
#else
		aac_fib_free(fibptr);
#endif
		return 0;
	}
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT) || (0 && defined(BOOTCD)))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12))
# define invalid_command_state(x) (((x)->state == SCSI_STATE_FINISHED) || !(x)->state)
#else
# define invalid_command_state(x) 0
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
# define invalid_command_magic(x) list_empty(&(x)->list)
#else
# define invalid_command_magic(x) (((x)->sc_magic != SCSI_CMND_MAGIC) && (x)->sc_magic)
#endif
	if (unlikely((scsicmd == (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL) ||
	  invalid_command_state(scsicmd) ||
	  (scsicmd->scsi_done == (void (*)(struct scsi_cmnd*))(uintptr_t)0x6b6b6b6b6b6b6b6bLL))) {
		printk(KERN_WARNING
		  "aac_valid_context: scsi command corrupt %p->scsi_done=%p%s%s\n",
		  scsicmd, (scsicmd &&
		  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL))
		    ? scsicmd->scsi_done
		    : (void (*)(struct scsi_cmnd*))(uintptr_t)-1LL,
		  (scsicmd &&
		  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL) &&
		  invalid_command_state(scsicmd)) ? " state" : "",
		  (scsicmd &&
		  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL) &&
		  invalid_command_magic(scsicmd))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		    ? " list" : ""
#else
		    ? " magic" : ""
#endif
		);
		if (nblank(fwprintf(x))) {
			extern struct list_head aac_devices; /* in linit.c */
			struct aac_dev *aac;
			list_for_each_entry(aac, &aac_devices, entry) {
				fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
				  "scsi command corrupt %p->scsi_done=%p%s%s",
				  scsicmd, (scsicmd &&
				  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL))
				    ? scsicmd->scsi_done
				    : (void (*)(struct scsi_cmnd*))(uintptr_t)-1LL,
				  (scsicmd &&
				  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL) &&
				  invalid_command_state(scsicmd))
				    ? " state" : "",
				  (scsicmd &&
				  (scsicmd != (struct scsi_cmnd*)(uintptr_t)0x6b6b6b6b6b6b6b6bLL) &&
				  invalid_command_magic(scsicmd))
				    ? " magic" : ""));
			}
		}
		aac_fib_complete(fibptr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		aac_fib_free_tag(fibptr);
#else
		aac_fib_free(fibptr);
#endif
		return 0;
	}
#undef invalid_command_state
#endif
	scsicmd->SCp.phase = AAC_OWNER_MIDLEVEL;
	device = scsicmd->device;
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT) || (0 && defined(BOOTCD)))
	if (unlikely(device == (void *)(uintptr_t)0x6b6b6b6b6b6b6b6bLL)) {
		printk(KERN_WARNING
		  "aac_valid_context: scsi device corrupt device=DEALLOCATED\n");
		if (nblank(fwprintf(x))) {
			extern struct list_head aac_devices; /* in linit.c */
			struct aac_dev *aac;
			list_for_each_entry(aac, &aac_devices, entry) {
				fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
				  "scsi device corrupt device=DEALLOCATED"));
			}
		}
		aac_fib_complete(fibptr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		aac_fib_free_tag(fibptr);
#else
		aac_fib_free(fibptr);
#endif
		return 0;
	}
#endif
	if (unlikely(!device)) {
		dprintk((KERN_WARNING "aac_valid_context: scsi device corrupt\n"));
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT) || (0 && defined(BOOTCD)))
		if (!nblank(dprintk(x)))
			printk(KERN_WARNING
			  "aac_valid_context: scsi device corrupt device=%p online=%d\n",
			  device, (!device) ? -1 : scsi_device_online(device));
		if (nblank(fwprintf(x))) {
			extern struct list_head aac_devices; /* in linit.c */
			struct aac_dev *aac;
			list_for_each_entry(aac, &aac_devices, entry) {
				fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
				  "scsi device corrupt device=%p online=%d",
				  device, (!device)
				    ? -1 : scsi_device_online(device)));
			}
		}
#endif
		aac_fib_complete(fibptr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		aac_fib_free_tag(fibptr);
#else
		aac_fib_free(fibptr);
#endif
		return 0;
	}
	return 1;
}

/**
 *	aac_get_config_status	-	check the adapter configuration
 *	@common: adapter to query
 *
 *	Query config status, and commit the configuration if needed.
 */
int aac_get_config_status(struct aac_dev *dev, int commit_flag)
{
	int status = 0;
	struct fib * fibptr;

	if (!(fibptr = aac_fib_alloc(dev)))
		return -ENOMEM;

	aac_fib_init(fibptr);
	{
		struct aac_get_config_status *dinfo;
		dinfo = (struct aac_get_config_status *) fib_data(fibptr);

		dinfo->command = cpu_to_le32(VM_ContainerConfig);
		dinfo->type = cpu_to_le32(CT_GET_CONFIG_STATUS);
		dinfo->count = cpu_to_le32(sizeof(((struct aac_get_config_status_resp *)NULL)->data));
	}

	status = aac_fib_send(ContainerCommand,
			    fibptr,
			    sizeof (struct aac_get_config_status_resp),
			    FsaNormal,
			    1, 1,
			    NULL, NULL);
	if (status < 0) {
		printk(KERN_WARNING "aac_get_config_status: SendFIB failed.\n");
#if (0 && defined(BOOTCD))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aac_get_config_status: SendFIB failed."));
#endif
	} else {
		struct aac_get_config_status_resp *reply
		  = (struct aac_get_config_status_resp *) fib_data(fibptr);
		dprintk((KERN_WARNING
		  "aac_get_config_status: response=%d status=%d action=%d\n",
		  le32_to_cpu(reply->response),
		  le32_to_cpu(reply->status),
		  le32_to_cpu(reply->data.action)));
#if (0 && defined(BOOTCD))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aac_get_config_status: response=%d status=%d action=%d",
		  le32_to_cpu(reply->response),
		  le32_to_cpu(reply->status),
		  le32_to_cpu(reply->data.action)));
#endif
		if ((le32_to_cpu(reply->response) != ST_OK) ||
		     (le32_to_cpu(reply->status) != CT_OK) ||
		     (le32_to_cpu(reply->data.action) > CFACT_PAUSE)) {
			printk(KERN_WARNING "aac_get_config_status: Will not issue the Commit Configuration\n");
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "aac_get_config_status: Will not issue the Commit Configuration"));
#endif
			status = -EINVAL;
		}
	}
	/* Do not set XferState to zero unless receives a response from F/W */
	if (status >= 0)
		aac_fib_complete(fibptr);

	/* Send a CT_COMMIT_CONFIG to enable discovery of devices */
	if (status >= 0) {
		if ((aac_commit == 1) || commit_flag) {
			struct aac_commit_config * dinfo;
			aac_fib_init(fibptr);
			dinfo = (struct aac_commit_config *) fib_data(fibptr);

			dinfo->command = cpu_to_le32(VM_ContainerConfig);
			dinfo->type = cpu_to_le32(CT_COMMIT_CONFIG);

			status = aac_fib_send(ContainerCommand,
				    fibptr,
				    sizeof (struct aac_commit_config),
				    FsaNormal,
				    1, 1,
				    NULL, NULL);
			/* Do not set XferState to zero unless
			 * receives a response from F/W */
			if (status >= 0)
				aac_fib_complete(fibptr);

		} else if (aac_commit == 0) {
			printk(KERN_WARNING
			  "aac_get_config_status: Foreign device configurations are being ignored\n");
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "aac_get_config_status: Foreign device configurations are being ignored"));
#endif
		}
	}
	/* FIB should be freed only after getting the response from the F/W */
	if (status != -ERESTARTSYS)
		aac_fib_free(fibptr);
	return status;
}

static void aac_expose_phy_device(struct scsi_cmnd *scsicmd)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	void *buf;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
	struct scatterlist *sg = scsi_sglist(scsicmd);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	if (scsicmd->use_sg) {
#if (!defined(__VMKLNX__) && !defined(__VMKLNX30__))
		buf = kmap_atomic(sg->page, KM_IRQ0) + sg->offset;
#else
#if defined(__ESX5__)
		buf = phys_to_virt(sg_dma_address(sg));
#else
		buf = phys_to_virt(sg->dma_address);
#endif
#endif
	} else {
		buf = scsicmd->request_buffer;
	}
#else
#if (defined(HAS_SG_PAGE))
	buf = kmap_atomic(sg_page(sg), KM_IRQ0) + sg->offset;
#else
	buf = kmap_atomic(sg->page, KM_IRQ0) + sg->offset;
#endif

#endif
	if(((*(char *)buf) & 0x20) && ((*(char *)buf) & 0x1f) == TYPE_DISK)
		(*(char *)buf) &= 0xdf;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
#if (!defined(__VMKLNX__)&& !defined(__VMKLNX30__))
	if (scsicmd->use_sg) {
#if (((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,16)) && defined(ARCH_HAS_FLUSH_ANON_PAGE)) || defined(CONFIG_PARISC) || defined(CONFIG_COMMUNITY_KERNEL))
		flush_kernel_dcache_page(kmap_atomic_to_page(buf - sg->offset));
#endif
		kunmap_atomic(buf - sg->offset, KM_IRQ0);
	}
#endif
#else
#if (defined(ARCH_HAS_FLUSH_ANON_PAGE) || defined(CONFIG_COMMUNITY_KERNEL))
	flush_kernel_dcache_page(kmap_atomic_to_page(buf - sg->offset));
#endif
	kunmap_atomic(buf - sg->offset, KM_IRQ0);
#endif

#elif ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__)) && !defined(__x86_64__))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4))
	void *pptr;
	vmk_verify_memory_for_io(scsicmd->request_bufferMA, 1);
	buf = phys_to_kmap(scsicmd->request_bufferMA, 1, &pptr);
	if(((*(char *)buf) & 0x20) && ((*(char *)buf) & 0x1f) == TYPE_DISK)
		(*(char *)buf) &= 0xdf;
	phys_to_kmapFree(buf, pptr);
#else
	vmk_verify_memory_for_io(scsicmd->request_bufferMA, 1);
	buf = vmk_phys_to_kmap(scsicmd->request_bufferMA, 1);
	if (((*(char *)buf) & 0x20) && ((*(char *)buf) & 0x1f) == TYPE_DISK)
		(*(char *)buf) &= 0xdf;
	vmk_phys_to_kmap_free(buf);
#endif
#else
	buf = scsicmd->request_buffer;
	if(((*(char *)buf) & 0x20) && ((*(char *)buf) & 0x1f) == TYPE_DISK)
		(*(char *)buf) &= 0xdf;
#endif
#else
	char inq_data;
	scsi_sg_copy_to_buffer(scsicmd,  &inq_data, sizeof(inq_data));
	if ((inq_data & 0x20) && (inq_data & 0x1f) == TYPE_DISK) {
		inq_data &= 0xdf;
		scsi_sg_copy_from_buffer(scsicmd, &inq_data, sizeof(inq_data));
	}
#endif
}

/**
 *	aac_get_containers	-	list containers
 *	@common: adapter to probe
 *
 *	Make a list of all containers on this controller
 */
int aac_get_containers(struct aac_dev *dev)
{
	struct fsa_dev_info *fsa_dev_ptr;
	u32 index;
	int status = 0;
	struct fib * fibptr;
	struct aac_get_container_count *dinfo;
	struct aac_get_container_count_resp *dresp;
	int maximum_num_containers = MAXIMUM_NUM_CONTAINERS;

	if (!(fibptr = aac_fib_alloc(dev)))
		return -ENOMEM;

	aac_fib_init(fibptr);
	dinfo = (struct aac_get_container_count *) fib_data(fibptr);
	dinfo->command = cpu_to_le32(VM_ContainerConfig);
	dinfo->type = cpu_to_le32(CT_GET_CONTAINER_COUNT);

	status = aac_fib_send(ContainerCommand,
		    fibptr,
		    sizeof (struct aac_get_container_count),
		    FsaNormal,
		    1, 1,
		    NULL, NULL);
	if (status >= 0) {
		dresp = (struct aac_get_container_count_resp *)fib_data(fibptr);
		maximum_num_containers = le32_to_cpu(dresp->ContainerSwitchEntries);
		if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
		    AAC_OPTION_SUPPORTED_240_VOLUMES)
			maximum_num_containers = le32_to_cpu(dresp->MaxSimpleVolumes);
		aac_fib_complete(fibptr);
	}
	/* FIB should be freed only after getting the response from the F/W */
	if (status != -ERESTARTSYS)
		aac_fib_free(fibptr);

	if (maximum_num_containers < MAXIMUM_NUM_CONTAINERS)
		maximum_num_containers = MAXIMUM_NUM_CONTAINERS;
	if ((dev->fsa_dev == NULL) || (dev->maximum_num_containers != maximum_num_containers))
	{
		fsa_dev_ptr = dev->fsa_dev;

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		dev->fsa_dev = kmalloc(sizeof(*fsa_dev_ptr) * maximum_num_containers,
				GFP_KERNEL);
#else
		dev->fsa_dev = kzalloc(sizeof(*fsa_dev_ptr) * maximum_num_containers,
				GFP_KERNEL);
#endif
		if (fsa_dev_ptr)
		{
			kfree(fsa_dev_ptr);
			fsa_dev_ptr = NULL;
		}

		if (!dev->fsa_dev)
			return -ENOMEM;
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		memset(dev->fsa_dev, 0, sizeof(struct fsa_dev) * maximum_num_containers);
#endif
		dev->maximum_num_containers = maximum_num_containers;
	}

	for (index = 0; index < dev->maximum_num_containers; index++) {
		dev->fsa_dev[index].devname[0] = '\0';
		dev->fsa_dev[index].valid = 0;
		status = aac_probe_container(dev, index);

		if (status < 0) {
			printk(KERN_WARNING "aac_get_containers: SendFIB failed.\n");
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "aac_get_containers: SendFIB failed."));
#endif
			break;
		}
	}
	return status;
}
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))

static void aac_io_done(struct scsi_cmnd * scsicmd)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)) /* suppress unused variable warning */
	unsigned long cpu_flags;
#endif

	if (unlikely((scsicmd == NULL) ||
	  (scsicmd == (void *)(uintptr_t)0x6b6b6b6b6b6b6b6bLL) ||
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT) || (0 && defined(BOOTCD)))
	  invalid_command_magic(scsicmd) ||
#endif
	  (scsicmd->scsi_done == (void (*)(struct scsi_cmnd*))NULL) ||
	  (scsicmd->scsi_done == (void (*)(struct scsi_cmnd*))(uintptr_t)0x6b6b6b6b6b6b6b6bLL))) {
		printk(KERN_WARNING "aac_io_done: scsicmd corrupted\n");
		return;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0))
	spin_lock_irqsave(&io_request_lock, cpu_flags);
#endif
	scsicmd->scsi_done(scsicmd);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0))
	spin_unlock_irqrestore(&io_request_lock, cpu_flags);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	{
		u64 lba;
		u32 count = 0;
		struct timeval now;
		struct aac_dev * dev;
		do_gettimeofday(&now);
		if ((scsicmd->cmnd[0] == WRITE_6) ||	/* 6 byte command */
		    (scsicmd->cmnd[0] == READ_6)) {
			lba = ((scsicmd->cmnd[1] & 0x1F) << 16) |
			    (scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
			count = scsicmd->cmnd[4];
			if (count == 0)
				count = 256;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(WRITE_16))
		} else if ((scsicmd->cmnd[0] == WRITE_16) || /* 16 byte command */
		    (scsicmd->cmnd[0] == READ_16)) {
			lba = ((u64)scsicmd->cmnd[2] << 56)
			    | ((u64)scsicmd->cmnd[3] << 48)
			    | ((u64)scsicmd->cmnd[4] << 40)
			    | ((u64)scsicmd->cmnd[5] << 32)
			    | ((u64)scsicmd->cmnd[6] << 24)
			    | (scsicmd->cmnd[7] << 16)
			    | (scsicmd->cmnd[8] << 8) | scsicmd->cmnd[9];
			count = (scsicmd->cmnd[10] << 24)
			      | (scsicmd->cmnd[11] << 16)
			      | (scsicmd->cmnd[12] << 8) | scsicmd->cmnd[13];
#endif
		} else if ((scsicmd->cmnd[0] == WRITE_12) /* 12 byte command */
		 || (scsicmd->cmnd[0] == READ_12)) {
			lba = ((u64)scsicmd->cmnd[2] << 24)
			    | (scsicmd->cmnd[3] << 16)
			    | (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
			count = (scsicmd->cmnd[6] << 24)
			      | (scsicmd->cmnd[7] << 16)
			      | (scsicmd->cmnd[8] << 8) | scsicmd->cmnd[9];
		} else if ((scsicmd->cmnd[0] == WRITE_10) /* 10 byte command */
		 || (scsicmd->cmnd[0] == READ_10)) {
			lba = ((u64)scsicmd->cmnd[2] << 24)
			    | (scsicmd->cmnd[3] << 16)
			    | (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
			count = (scsicmd->cmnd[7] << 8) | scsicmd->cmnd[8];
		} else
			lba = (u64)(long)scsicmd;
		dev = (struct aac_dev *)scsicmd->device->host->hostdata;
		printk(((count)
		  ? KERN_DEBUG "%lu.%06lu d%lu %llu[%u]\n"
		  : KERN_DEBUG "%lu.%06lu d%lu 0x%llx\n"),
		  now.tv_sec % 100, now.tv_usec,
		  (unsigned long)(atomic_read(&dev->queues->queue[
#if (defined(INITFLAGS_APRE_SUPPORTED))
		    (dev->comm_interface == AAC_COMM_APRE)
		      ? ApreCmdQueue
		      : AdapNormCmdQueue
#else
		    AdapNormCmdQueue
#endif
		  ].numpending)), (unsigned long long)lba, count);
#if (defined(INITFLAGS_APRE_SUPPORTED))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  ((count)
		    ? "%lu.%06lu d%lu %llu[%u]"
		    : "%lu.%06lu d%lu 0x%llx"),
		  now.tv_sec % 100, now.tv_usec,
		  (unsigned long)(dev->queues->queue[
		    (dev->comm_interface == AAC_COMM_APRE)
		      ? ApreCmdQueue
		      : AdapNormCmdQueue
		  ].numpending), (unsigned long long)lba, count));
#else
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  ((count)
		    ? "%lu.%06lu d%lu %llu[%u]"
		    : "%lu.%06lu d%lu 0x%llx"),
		  now.tv_sec % 100, now.tv_usec,
		  (unsigned long)(atomic_read(&dev->queues->queue[AdapNormCmdQueue].numpending)), lba, count));

#endif
	}
#endif
}

static inline void __aac_io_done(struct scsi_cmnd * scsicmd)
{
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	struct timeval now;
	struct aac_dev * dev;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	BUG_ON((scsicmd == NULL) || (scsicmd == (void *)(uintptr_t)0x6b6b6b6b6b6b6b6bLL));
	BUG_ON((scsicmd->scsi_done == NULL) || (scsicmd->scsi_done == (void (*)(struct scsi_cmnd*))(uintptr_t)0x6b6b6b6b6b6b6b6bLL));
#endif
	scsicmd->scsi_done(scsicmd);
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	do_gettimeofday(&now);
	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
	printk(KERN_DEBUG "%lu.%06lu d%lu %p\n",
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)(atomic_read(&dev->queues->queue[
#if (defined(INITFLAGS_APRE_SUPPORTED))
	    (dev->comm_interface == AAC_COMM_APRE)
	      ? ApreCmdQueue
	      : AdapNormCmdQueue
#else
	    AdapNormCmdQueue
#endif
	  ].numpending)), scsicmd);
#if (defined(INITFLAGS_APRE_SUPPORTED))
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "%lu.%06lu d%lu %p",
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)(dev->queues->queue[
	    (dev->comm_interface == AAC_COMM_APRE)
	      ? ApreCmdQueue
	      : AdapNormCmdQueue
	  ].numpending), scsicmd));
#else
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "%lu.%06lu d%lu %p",
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)atomic_read(&dev->queues->queue[AdapNormCmdQueue].numpending), scsicmd));
#endif
#endif
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static void aac_internal_transfer(struct scsi_cmnd *scsicmd, void *data, unsigned int offset, unsigned int len)
{
	void *buf;
	int transfer_len;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
	struct scatterlist *sg = scsi_sglist(scsicmd);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	if (scsicmd->use_sg) {
#if (!defined(__VMKLNX30__) && !defined(__VMKLNX__))
		buf = kmap_atomic(sg->page, KM_IRQ0) + sg->offset;
		transfer_len = min(sg->length, len + offset);
#else
#if defined(__ESX5__)
		buf = phys_to_virt(sg_dma_address(sg));
		transfer_len = min(sg_dma_len(sg), len + offset);
#else
		buf = phys_to_virt(sg->dma_address);
		transfer_len = min(sg->dma_length, len + offset);
#endif
#endif
	} else {
		buf = scsicmd->request_buffer;
		transfer_len = min(scsicmd->request_bufflen, len + offset);
	}
#else
#if (defined(HAS_SG_PAGE))
	buf = kmap_atomic(sg_page(sg), KM_IRQ0) + sg->offset;
#else
	buf = kmap_atomic(sg->page, KM_IRQ0) + sg->offset;
#endif
	transfer_len = min(sg->length, len + offset);

#endif
	transfer_len -= offset;
	if (buf && transfer_len > 0)
		memcpy(buf + offset, data, transfer_len);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
#if (!defined(__VMKLNX30__) && !defined(__VMKLNX__))
	if (scsicmd->use_sg) {
#if (((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,16)) && defined(ARCH_HAS_FLUSH_ANON_PAGE)) || defined(CONFIG_PARISC) || defined(CONFIG_COMMUNITY_KERNEL))
		flush_kernel_dcache_page(kmap_atomic_to_page(buf - sg->offset));
#endif
		kunmap_atomic(buf - sg->offset, KM_IRQ0);
	}
#endif
#else
#if (defined(ARCH_HAS_FLUSH_ANON_PAGE) || defined(CONFIG_COMMUNITY_KERNEL))
	flush_kernel_dcache_page(kmap_atomic_to_page(buf - sg->offset));
#endif
	kunmap_atomic(buf - sg->offset, KM_IRQ0);
#endif

#elif ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__)) && !defined(__x86_64__))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4))
	void *pptr;
	vmk_verify_memory_for_io(scsicmd->request_bufferMA, len + offset);
	buf = phys_to_kmap(scsicmd->request_bufferMA, len + offset, &pptr);
	transfer_len = min(scsicmd->request_bufflen, len + offset) - offset;
	if (buf && transfer_len > 0)
		memcpy(buf + offset, data, transfer_len);
	phys_to_kmapFree(buf, pptr);
#else
	vmk_verify_memory_for_io(scsicmd->request_bufferMA, len + offset);
	buf = vmk_phys_to_kmap(scsicmd->request_bufferMA, len + offset);
	transfer_len = min(scsicmd->request_bufflen, len + offset) - offset;
	if (buf && transfer_len > 0)
		memcpy(buf + offset, data, transfer_len);
	vmk_phys_to_kmap_free(buf);
#endif
#else
	buf = scsicmd->request_buffer;
	transfer_len = min(scsicmd->request_bufflen, len + offset) - offset;
	if (buf && transfer_len > 0)
		memcpy(buf + offset, data, transfer_len);
#endif
}

#endif
static void get_container_name_callback(void *context, struct fib * fibptr)
{
	struct aac_get_name_resp * get_name_reply;
	struct scsi_cmnd * scsicmd;

	scsicmd = (struct scsi_cmnd *) context;

	if (!aac_valid_context(scsicmd, fibptr))
		return;

	dprintk((KERN_DEBUG "get_container_name_callback[cpu %d]: t = %ld.\n", smp_processor_id(), jiffies));
#if ((0 && defined(BOOTCD)) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "get_container_name_callback"));
#endif
	BUG_ON(fibptr == NULL);

	get_name_reply = (struct aac_get_name_resp *) fib_data(fibptr);
	/* Failure is irrelevant, using default value instead */
#if ((0 && defined(BOOTCD)) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "  status=%d", le32_to_cpu(get_name_reply->status)));
#endif
	if ((le32_to_cpu(get_name_reply->status) == CT_OK)
	 && (get_name_reply->data[0] != '\0')) {
		char *sp = get_name_reply->data;
		sp[sizeof(((struct aac_get_name_resp *)NULL)->data)] = '\0';
#if ((0 && defined(BOOTCD)) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "  name=\"%s\"", sp));
#endif
		while (*sp == ' ')
			++sp;
		if (*sp) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
			struct inquiry_data inq;
#endif
			char d[sizeof(((struct inquiry_data *)NULL)->inqd_pid)];
			int count = sizeof(d);
			char *dp = d;
			do {
				*dp++ = (*sp) ? *sp++ : ' ';
			} while (--count > 0);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			aac_internal_transfer(scsicmd, d,
			  offsetof(struct inquiry_data, inqd_pid), sizeof(d));
#else

			scsi_sg_copy_to_buffer(scsicmd, &inq, sizeof(inq));
			memcpy(inq.inqd_pid, d, sizeof(d));
			scsi_sg_copy_from_buffer(scsicmd, &inq, sizeof(inq));
#endif
		}
	}

	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;

	aac_fib_complete(fibptr);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	aac_fib_free_tag(fibptr);
#else
	aac_fib_free(fibptr);
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif
}

/**
 *	aac_get_container_name	-	get container name, none blocking.
 */
static int aac_get_container_name(struct scsi_cmnd * scsicmd)
{
	int status;
	struct aac_get_name *dinfo;
	struct fib * cmd_fibcontext;
	struct aac_dev * dev;

	dev = (struct aac_dev *)scsicmd->device->host->hostdata;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (!(cmd_fibcontext = aac_fib_alloc_tag(dev, scsicmd)))
#else
	if (!(cmd_fibcontext = aac_fib_alloc(dev)))
#endif
		return -ENOMEM;

	aac_fib_init(cmd_fibcontext);
	dinfo = (struct aac_get_name *) fib_data(cmd_fibcontext);

	dinfo->command = cpu_to_le32(VM_ContainerConfig);
	dinfo->type = cpu_to_le32(CT_READ_NAME);
	dinfo->cid = cpu_to_le32(scmd_id(scsicmd));
	dinfo->count = cpu_to_le32(sizeof(((struct aac_get_name_resp *)NULL)->data));

	status = aac_fib_send(ContainerCommand,
		  cmd_fibcontext,
		  sizeof (struct aac_get_name_resp),
		  FsaNormal,
		  0, 1,
		  (fib_callback)get_container_name_callback,
		  (void *) scsicmd);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
#if ((0 && defined(BOOTCD)) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aac_get_container_name(%p(%d:%d:%d:%d))", scsicmd,
		  scsicmd->device->host->host_no, scmd_channel(scsicmd),
		  scmd_id(scsicmd), scsicmd->device->lun));
#endif
		return 0;
	}

	printk(KERN_WARNING "aac_get_container_name: aac_fib_send failed with status: %d.\n", status);
#if ((0 && defined(BOOTCD)) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "aac_get_container_name: aac_fib_send failed with status: %d.",
	  status));
#endif
	aac_fib_complete(cmd_fibcontext);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
    aac_fib_free_tag(cmd_fibcontext);
#else
	aac_fib_free(cmd_fibcontext);
#endif
	return -1;
}

static int aac_probe_container_callback2(struct scsi_cmnd * scsicmd)
{
	struct fsa_dev_info *fsa_dev_ptr = ((struct aac_dev *)(scsicmd->device->host->hostdata))->fsa_dev;

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	printk(KERN_INFO "aac_probe_container_callback2(%p)\n", scsicmd);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&((struct aac_dev *)(scsicmd->device->host->hostdata))->entry)) {
#endif
	fwprintf(((struct aac_dev *)(scsicmd->device->host->hostdata),
	  HBA_FLAGS_DBG_FW_PRINT_B, "aac_probe_container_callback2(%p)", scsicmd));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
	if ((fsa_dev_ptr[scmd_id(scsicmd)].valid & 1))
		return aac_scsi_cmd(scsicmd);

	scsicmd->result = DID_NO_CONNECT << 16;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	__aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif
	return 0;
}

static void _aac_probe_container2(void * context, struct fib * fibptr)
{
	struct fsa_dev_info *fsa_dev_ptr;
	int (*callback)(struct scsi_cmnd *);
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
	struct aac_dev *dev;
#endif
	struct scsi_cmnd * scsicmd = (struct scsi_cmnd *)context;
	int i;

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	printk(KERN_INFO "_aac_probe_container2(%p,%p)\n", scsicmd, fibptr);
#endif

	if (!aac_valid_context(scsicmd, fibptr))
		return;

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
	dev = fibptr->dev;
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&dev->entry)) {
#endif
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "_aac_probe_container2(%p(%d:%d:%d:%d),%p)", scsicmd,
	  scsicmd->device->host->host_no, scmd_channel(scsicmd),
	  scmd_id(scsicmd), scsicmd->device->lun, fibptr));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
	scsicmd->SCp.Status = 0;
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
	fsa_dev_ptr = dev->fsa_dev;
#else
	fsa_dev_ptr = fibptr->dev->fsa_dev;
#endif
	if (fsa_dev_ptr) {
		struct aac_mount * dresp = (struct aac_mount *) fib_data(fibptr);
		fsa_dev_ptr += scmd_id(scsicmd);

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		printk(KERN_INFO "_aac_probe_container2 (%d:%d:%d:%d)"
		  " resp={%d,%d,0x%x,%llu}\n",
		  scsicmd->device->host->host_no, scmd_channel(scsicmd),
		  scmd_id(scsicmd), scsicmd->device->lun,
		  le32_to_cpu(dresp->status), le32_to_cpu(dresp->mnt[0].vol),
		  le32_to_cpu(dresp->mnt[0].state),
		  ((u64)le32_to_cpu(dresp->mnt[0].capacity)) +
		    (((u64)le32_to_cpu(dresp->mnt[0].capacityhigh)) << 32));
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		if (!list_empty(&dev->entry)) {
#endif
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "_aac_probe_container2 (%d:%d:%d:%d)"
		  " resp={%d,%d,0x%x,%llu}\n",
		  scsicmd->device->host->host_no, scmd_channel(scsicmd),
		  scmd_id(scsicmd), scsicmd->device->lun,
		  le32_to_cpu(dresp->status), le32_to_cpu(dresp->mnt[0].vol),
		  le32_to_cpu(dresp->mnt[0].state),
		  ((u64)le32_to_cpu(dresp->mnt[0].capacity)) +
		    (((u64)le32_to_cpu(dresp->mnt[0].capacityhigh)) << 32)));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		}
#endif
#endif
		if ((le32_to_cpu(dresp->status) == ST_OK) &&
		    (le32_to_cpu(dresp->mnt[0].vol) != CT_NONE) &&
		    (le32_to_cpu(dresp->mnt[0].state) != FSCS_HIDDEN)) {
			if (!(fibptr->dev->supplement_adapter_info.SupportedOptions2 &
			    AAC_OPTION_VARIABLE_BLOCK_SIZE)) {
				dresp->mnt[0].fileinfo.bdevinfo.block_size = 0x200;
				fsa_dev_ptr->block_size = 0x200;
			} else {
				fsa_dev_ptr->block_size = le32_to_cpu(dresp->mnt[0].fileinfo.bdevinfo.block_size);
			}
			for (i = 0; i < 16; i++)
				fsa_dev_ptr->identifier[i] = dresp->mnt[0].fileinfo.bdevinfo.identifier[i];
			fsa_dev_ptr->valid = 1;
			/* sense_key holds the current state of the spin-up */
			if (dresp->mnt[0].state & cpu_to_le32(FSCS_NOT_READY))
				fsa_dev_ptr->sense_data.sense_key = NOT_READY;
			else if (fsa_dev_ptr->sense_data.sense_key == NOT_READY)
				fsa_dev_ptr->sense_data.sense_key = NO_SENSE;
			fsa_dev_ptr->type = le32_to_cpu(dresp->mnt[0].vol);
			fsa_dev_ptr->size
			  = ((u64)le32_to_cpu(dresp->mnt[0].capacity)) +
			    (((u64)le32_to_cpu(dresp->mnt[0].capacityhigh)) << 32);
			fsa_dev_ptr->ro = ((le32_to_cpu(dresp->mnt[0].state) & FSCS_READONLY) != 0);
		}
		if ((fsa_dev_ptr->valid & 1) == 0)
			fsa_dev_ptr->valid = 0;
		scsicmd->SCp.Status = le32_to_cpu(dresp->count);
	}
	aac_fib_complete(fibptr);
	aac_fib_free(fibptr);
	callback = (int (*)(struct scsi_cmnd *))(scsicmd->SCp.ptr);
	scsicmd->SCp.ptr = NULL;
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	printk(KERN_INFO "(*%p)(%p)\n", callback, scsicmd);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&dev->entry)) {
#endif
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B, "(*%p)(%p)", callback, scsicmd));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
	(*callback)(scsicmd);
	return;
}

static void _aac_probe_container1(void * context, struct fib * fibptr)
{
	struct scsi_cmnd * scsicmd;
	struct aac_mount * dresp;
	struct aac_query_mount *dinfo;
	int status;

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	printk(KERN_INFO "_aac_probe_container1(%p,%p)\n", context, fibptr);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&fibptr->dev->entry)) {
#endif
	fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "_aac_probe_container1(%p,%p)", context, fibptr));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
	dresp = (struct aac_mount *) fib_data(fibptr);
	if (!(fibptr->dev->supplement_adapter_info.SupportedOptions2 &
	    AAC_OPTION_VARIABLE_BLOCK_SIZE))
		dresp->mnt[0].capacityhigh = 0;
	if ((le32_to_cpu(dresp->status) != ST_OK) ||
	    (le32_to_cpu(dresp->mnt[0].vol) != CT_NONE)) {
		_aac_probe_container2(context, fibptr);
		return;
	}
	scsicmd = (struct scsi_cmnd *) context;

	if (!aac_valid_context(scsicmd, fibptr))
		return;

	aac_fib_init(fibptr);

	dinfo = (struct aac_query_mount *)fib_data(fibptr);

	if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
	    AAC_OPTION_VARIABLE_BLOCK_SIZE)
		dinfo->command = cpu_to_le32(VM_NameServeAllBlk);
	else
		dinfo->command = cpu_to_le32(VM_NameServe64);

	dinfo->count = cpu_to_le32(scmd_id(scsicmd));
	dinfo->type = cpu_to_le32(FT_FILESYS);

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
	    AAC_OPTION_VARIABLE_BLOCK_SIZE)
		printk(KERN_INFO "aac_fib_send(ContainerCommand,VM_NameServeAllBlk,...)\n");
	else
		printk(KERN_INFO "aac_fib_send(ContainerCommand,VM_NameServe64,...)\n");
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&fibptr->dev->entry)) {
#endif
	if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
	    AAC_OPTION_VARIABLE_BLOCK_SIZE)
		fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aac_fib_send(ContainerCommand,VM_NameServeAllBlk,...)"));
	else
		fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aac_fib_send(ContainerCommand,VM_NameServe64,...)"));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
	status = aac_fib_send(ContainerCommand,
			  fibptr,
			  sizeof(struct aac_query_mount),
			  FsaNormal,
			  0, 1,
			  _aac_probe_container2,
			  (void *) scsicmd);
	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS)
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
	else if (status < 0) {
		/* Inherit results from VM_NameServe, if any */
		dresp->status = cpu_to_le32(ST_OK);
		_aac_probe_container2(context, fibptr);
	}
}

static int _aac_probe_container(struct scsi_cmnd * scsicmd, int (*callback)(struct scsi_cmnd *))
{
	struct fib * fibptr;
	int status = -ENOMEM;

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	printk(KERN_INFO "_aac_probe_container(%p,%p)\n", scsicmd, callback);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&((struct aac_dev *)(scsicmd->device->host->hostdata))->entry)) {
#endif
	fwprintf(((struct aac_dev *)(scsicmd->device->host->hostdata),
	  HBA_FLAGS_DBG_FW_PRINT_B,
	  "_aac_probe_container(%p,%p)", scsicmd, callback));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
	if ((fibptr = aac_fib_alloc((struct aac_dev *)scsicmd->device->host->hostdata))) {
		struct aac_query_mount *dinfo;

		aac_fib_init(fibptr);

		dinfo = (struct aac_query_mount *)fib_data(fibptr);

		if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
		    AAC_OPTION_VARIABLE_BLOCK_SIZE)
			dinfo->command = cpu_to_le32(VM_NameServeAllBlk);
		else
			dinfo->command = cpu_to_le32(VM_NameServe);

		dinfo->count = cpu_to_le32(scmd_id(scsicmd));
		dinfo->type = cpu_to_le32(FT_FILESYS);
		scsicmd->SCp.ptr = (char *)callback;

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
		    AAC_OPTION_VARIABLE_BLOCK_SIZE)
			printk(KERN_INFO "aac_fib_send(ContainerCommand,VM_NameServeAllBlk,...)\n");
		else
			printk(KERN_INFO "aac_fib_send(ContainerCommand,VM_NameServe,...)\n");
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		if (!list_empty(&fibptr->dev->entry)) {
#endif
			if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
			    AAC_OPTION_VARIABLE_BLOCK_SIZE)
				fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  "aac_fib_send(ContainerCommand,VM_NameServeAllBlk,...)"));
			else
				fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  "aac_fib_send(ContainerCommand,VM_NameServe,...)"));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		}
#endif
#endif
		status = aac_fib_send(ContainerCommand,
			  fibptr,
			  sizeof(struct aac_query_mount),
			  FsaNormal,
			  0, 1,
			  _aac_probe_container1,
			  (void *) scsicmd);
		/*
		 *	Check that the command queued to the controller
		 */
		if (status == -EINPROGRESS) {
			scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
			return 0;
		}
		if (status < 0) {
			scsicmd->SCp.ptr = NULL;
			aac_fib_complete(fibptr);
			aac_fib_free(fibptr);
		}
	}
	if (status < 0) {
		struct fsa_dev_info *fsa_dev_ptr = ((struct aac_dev *)(scsicmd->device->host->hostdata))->fsa_dev;
		if (fsa_dev_ptr) {
			fsa_dev_ptr += scmd_id(scsicmd);
			if ((fsa_dev_ptr->valid & 1) == 0) {
				fsa_dev_ptr->valid = 0;
				return (*callback)(scsicmd);
			}
		}
	}
	return status;
}

/**
 *	aac_probe_container		-	query a logical volume
 *	@dev: device to query
 *	@cid: container identifier
 *
 *	Queries the controller about the given volume. The volume information
 *	is updated in the struct fsa_dev_info structure rather than returned.
 */
static int aac_probe_container_callback1(struct scsi_cmnd * scsicmd)
{
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	printk(KERN_INFO "aac_probe_container_callback1(%p)\n", scsicmd);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&((struct aac_dev *)(scsicmd->device->host->hostdata))->entry)) {
#endif
	fwprintf(((struct aac_dev *)(scsicmd->device->host->hostdata),
	  HBA_FLAGS_DBG_FW_PRINT_B, "aac_probe_container_callback1(%p)", scsicmd));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
	scsicmd->device = NULL;
	return 0;
}

int aac_probe_container(struct aac_dev *dev, int cid)
{
	struct scsi_cmnd *scsicmd = kmalloc(sizeof(*scsicmd), GFP_KERNEL);
	struct scsi_device *scsidev = kmalloc(sizeof(*scsidev), GFP_KERNEL);
	struct aac_query_mount *dinfo;
	struct fib *fibptr;
	struct aac_mount *dresp;
	struct fsa_dev_info *fsa_dev_ptr;
	int i;
	int status = -ENOMEM;

#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	printk(KERN_INFO "aac_probe_container(%p,%d)\n", dev, cid);
#endif
	if (!scsicmd || !scsidev) {
		kfree(scsicmd);
		kfree(scsidev);
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		printk(KERN_INFO "aac_probe_container returns -ENOMEM\n");
#endif
		return -ENOMEM;
	}
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	if (!list_empty(&dev->entry)) {
#endif
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "aac_probe_container(%p,%d)", dev, cid));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	}
#endif
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	scsicmd->list.next = NULL;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12))
	scsicmd->state = SCSI_STATE_QUEUED;
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(SCSI_CMND_MAGIC))
	scsicmd->sc_magic = 0;
#endif
#endif
	scsicmd->scsi_done = (void (*)(struct scsi_cmnd*))aac_probe_container_callback1;

	scsicmd->device = scsidev;
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6)) && (!defined(SCSI_HAS_SCSI_DEVICE_ONLINE)))
	scsidev->online = 1;
#else
	scsidev->sdev_state = 0;
#endif
	scsidev->id = cid;
	scsidev->host = dev->scsi_host_ptr;

	fsa_dev_ptr = dev->fsa_dev;

	fibptr = aac_fib_alloc(dev);
	if (NULL == fibptr )
		goto exit_probe;
	aac_fib_init(fibptr);

	if (NULL == fsa_dev_ptr)
		goto fib_free;
	fsa_dev_ptr += scmd_id(scsicmd);

	dinfo = (struct aac_query_mount *)fib_data(fibptr);

	if (fibptr->dev->supplement_adapter_info.SupportedOptions2 &
		AAC_OPTION_VARIABLE_BLOCK_SIZE)
			dinfo->command = cpu_to_le32(VM_NameServeAllBlk);
	else if((fibptr->dev->adapter_info.options & AAC_OPT_NEW_COMM) &&
		(fibptr->dev->adapter_info.options & AAC_OPTION_VARIABLE_BLOCK_SIZE))
		dinfo->command = cpu_to_le32(VM_NameServe64);
	else
		dinfo->command = cpu_to_le32(VM_NameServe);


	dinfo->count= cpu_to_le32(scmd_id(scsicmd));
	dinfo->type = cpu_to_le32(FT_FILESYS);

	status = aac_fib_send(ContainerCommand,
		fibptr,
		sizeof(struct aac_query_mount),
		FsaNormal,
		1,1,
		(void *)NULL,
		(void *) scsicmd);

	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		status = 0;
		goto ret;
	}

	if (status < 0) {
		goto fib_free;
	}

	dresp = (struct aac_mount*) fib_data(fibptr);

	if ((le32_to_cpu(dresp->status) == ST_OK) &&
	(le32_to_cpu(dresp->mnt[0].vol) != CT_NONE) &&
	(le32_to_cpu(dresp->mnt[0].state) != FSCS_HIDDEN)) {
		if (!(fibptr->dev->supplement_adapter_info.SupportedOptions2 &
			AAC_OPTION_VARIABLE_BLOCK_SIZE)) {
			dresp->mnt[0].fileinfo.bdevinfo.block_size = 0x200;
			fsa_dev_ptr->block_size = 0x200;
		} else {
			fsa_dev_ptr->block_size = le32_to_cpu(dresp->mnt[0].fileinfo.bdevinfo.block_size);
		}
		for (i = 0; i < 16; i++)
			fsa_dev_ptr->identifier[i] = dresp->mnt[0].fileinfo.bdevinfo.identifier[i];
		fsa_dev_ptr->valid = 1;
		/* sense_key holds the current state of the spin-up */
		if (dresp->mnt[0].state & cpu_to_le32(FSCS_NOT_READY))
				fsa_dev_ptr->sense_data.sense_key = NOT_READY;
		else if (fsa_dev_ptr->sense_data.sense_key == NOT_READY)
			fsa_dev_ptr->sense_data.sense_key = NO_SENSE;
		fsa_dev_ptr->type = le32_to_cpu(dresp->mnt[0].vol);
		fsa_dev_ptr->size
			= ((u64)le32_to_cpu(dresp->mnt[0].capacity)) +
			(((u64)le32_to_cpu(dresp->mnt[0].capacityhigh)) << 32);
		fsa_dev_ptr->ro = ((le32_to_cpu(dresp->mnt[0].state) & FSCS_READONLY) != 0);
	}
	if ((fsa_dev_ptr->valid & 1) == 0) 
		fsa_dev_ptr->valid = 0;
	scsicmd->SCp.Status = le32_to_cpu(dresp->count);

fib_free:
	aac_fib_complete(fibptr);
	aac_fib_free(fibptr);
	scsicmd->device = NULL;

exit_probe:
	if (status >= 0)
		goto ret;

	if ((NULL != fsa_dev_ptr) &&
		(fsa_dev_ptr->valid & 1) == 0){
		fsa_dev_ptr->valid = 0;
		return 0;
	}

ret:
	kfree(scsidev);
	status = scsicmd->SCp.Status;
	kfree(scsicmd);
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
	{
		struct fsa_dev_info * fsa_dev_ptr = &dev->fsa_dev[cid];
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))

		printk(KERN_INFO
		  "aac_probe_container returns %d"
		  " *(&%p->fsa_dev[%d]=%p)={%d,%d,%llu,\"%.*s\"}\n",
		  status, dev, cid, fsa_dev_ptr, fsa_dev_ptr->valid,
		  fsa_dev_ptr->type, fsa_dev_ptr->size,
		  (int)sizeof(fsa_dev_ptr->devname),
		  fsa_dev_ptr->devname);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE) || (0 && defined(BOOTCD)))
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		if (!list_empty(&dev->entry)) {
#endif
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aac_probe_container returns %d"
		  " *(&%p->fsa_dev[%d]=%p)={%d,%d,%llu,\"%.*s\"}",
		  status, dev, cid, fsa_dev_ptr, fsa_dev_ptr->valid,
		  fsa_dev_ptr->type, fsa_dev_ptr->size,
		  (int)sizeof(fsa_dev_ptr->devname),
		  fsa_dev_ptr->devname));
#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
		}
#endif
	}
#endif
	return status;
}

#if (defined(CONFIG_COMMUNITY_KERNEL))
/* Local Structure to set SCSI inquiry data strings */
struct scsi_inq {
	char vid[8];         /* Vendor ID */
	char pid[16];        /* Product ID */
	char prl[4];         /* Product Revision Level */
};

#endif
/**
 *	InqStrCopy	-	string merge
 *	@a:	string to copy from
 *	@b:	string to copy to
 *
 *	Copy a String from one location to another
 *	without copying \0
 */

static void inqstrcpy(char *a, char *b)
{

	while (*a != (char)0)
		*b++ = *a++;
}

static char *container_types[] = {
	"None",
	"Volume",
	"Mirror",
	"Stripe",
	"RAID5",
	"SSRW",
	"SSRO",
	"Morph",
	"Legacy",
	"RAID4",
	"RAID10",
	"RAID00",
	"V-MIRRORS",
	"PSEUDO R4",
	"RAID50",
	"RAID5D",
	"RAID5D0",
	"RAID1E",
	"RAID6",
	"RAID60",
	"Unknown"
};
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

char * get_container_type(unsigned tindex)
{
	if (tindex >= ARRAY_SIZE(container_types))
		tindex = ARRAY_SIZE(container_types) - 1;
	return container_types[tindex];
}
#endif

/* Function: setinqstr
 *
 * Arguments: [1] pointer to void [1] int
 *
 * Purpose: Sets SCSI inquiry data strings for vendor, product
 * and revision level. Allows strings to be set in platform dependant
 * files instead of in OS dependant driver source.
 */

#if (defined(CONFIG_COMMUNITY_KERNEL))
static void setinqstr(struct aac_dev *dev, void *data, int tindex)
#else
void setinqstr(struct aac_dev *dev, void *data, int tindex)
#endif
{
	struct scsi_inq *str;

	str = (struct scsi_inq *)(data); /* cast data to scsi inq block */
	memset(str, ' ', sizeof(*str));

	if (dev->supplement_adapter_info.AdapterTypeText[0]) {
		char * cp = dev->supplement_adapter_info.AdapterTypeText;
		int c;

		if ((cp[0] == 'A') && (cp[1] == 'O') && (cp[2] == 'C'))
			inqstrcpy("SMC", str->vid);
		else {
			c = sizeof(str->vid);
			while (*cp && *cp != ' ' && --c)
				++cp;
			c = *cp;
			*cp = '\0';
			inqstrcpy (dev->supplement_adapter_info.AdapterTypeText,
				   str->vid);
			*cp = c;
			while (*cp && *cp != ' ')
				++cp;
		}
		while (*cp == ' ')
			++cp;
		/* last six chars reserved for vol type */
		c = 0;
		if (strlen(cp) > sizeof(str->pid)) {
			c = cp[sizeof(str->pid)];
			cp[sizeof(str->pid)] = '\0';
		}
		inqstrcpy (cp, str->pid);
		if (c)
			cp[sizeof(str->pid)] = c;
	} else {
		struct aac_driver_ident *mp = aac_get_driver_ident(dev->cardtype);
		inqstrcpy (mp->vname, str->vid);
		/* last six chars reserved for vol type */
		inqstrcpy (mp->model, str->pid);
	}

	if (tindex < ARRAY_SIZE(container_types)){
		char *findit = str->pid;

		for ( ; *findit != ' '; findit++); /* walk till we find a space */
		/* RAID is superfluous in the context of a RAID device */
		if (memcmp(findit-4, "RAID", 4) == 0)
			*(findit -= 4) = ' ';
		if (((findit - str->pid) + strlen(container_types[tindex]))
		 < (sizeof(str->pid) + sizeof(str->prl)))
			inqstrcpy (container_types[tindex], findit + 1);
	}
	inqstrcpy ("V1.0", str->prl);
}

static void get_container_serial_callback(void *context, struct fib * fibptr)
{
	struct aac_get_serial_resp * get_serial_reply;
	struct scsi_cmnd * scsicmd;

	BUG_ON(fibptr == NULL);

	scsicmd = (struct scsi_cmnd *) context;
	if (!aac_valid_context(scsicmd, fibptr))
		return;

	get_serial_reply = (struct aac_get_serial_resp *) fib_data(fibptr);
	/* Failure is irrelevant, using default value instead */
	if (le32_to_cpu(get_serial_reply->status) == CT_OK) {

/* Excluding SUSE as it has issues when inbox driver does not have this support but outbox has it.
  Because SUSE uses /dev/disk/by-id mapping entries in the OS grub config and VPD 0X83 creates conflicts */
#if (!defined(CONFIG_SUSE_KERNEL))

		/*Check to see if it's for VPD 0x83 or 0x80 */
		if(scsicmd->cmnd[2] == 0x83)
		{
			/* vpd page 0x83 - Device Identification Page */
			struct aac_dev * dev;
			int container, i;
			TVPD_Page83 VPDPage83Data;

			dev = (struct aac_dev *)scsicmd->device->host->hostdata;

				memset(((u8 *)&VPDPage83Data), 0, sizeof(VPDPage83Data));


				 VPDPage83Data.DeviceType = 0;			//DIRECT_ACCESS_DEVICE;
                VPDPage83Data.DeviceTypeQualifier = 0;	//DEVICE_CONNECTED;
                VPDPage83Data.PageCode = 0x83;			//VPD_DEVICE_IDENTIFIERS;
                VPDPage83Data.Reserved = 0;
                VPDPage83Data.PageLength = sizeof(VPDPage83Data.IdDescriptorType1) +
                                              sizeof(VPDPage83Data.IdDescriptorType2) +
					      sizeof(VPDPage83Data.IdDescriptorType3);

                // T10 Vendor Identifier Field Format
                VPDPage83Data.IdDescriptorType1.CodeSet = 2;			//VpdCodeSetAscii;
                VPDPage83Data.IdDescriptorType1.IdentifierType = 1;		//VpdIdentifierTypeVendorId;
                VPDPage83Data.IdDescriptorType1.IdentifierLength = sizeof(VPDPage83Data.IdDescriptorType1) - 4;

                memcpy(VPDPage83Data.IdDescriptorType1.VendId, "ADAPTEC ", // "ADAPTEC " for adaptec
                       sizeof(VPDPage83Data.IdDescriptorType1.VendId));
                memcpy(VPDPage83Data.IdDescriptorType1.ProductId, "ARRAY           ",
                       sizeof(VPDPage83Data.IdDescriptorType1.ProductId));

                // Convert to ascii based serial number.
                // The LSB is the the end.

                for (i=0; i < 8; i++) {
                    u8 temp = (u8)((get_serial_reply->uid >> ((7 - i) * 4)) & 0xF);
                    if (temp  > 0x9)
                    {
                        VPDPage83Data.IdDescriptorType1.SerialNumber[i] = 'A' + (temp - 0xA);
                    } else
                    {
                        VPDPage83Data.IdDescriptorType1.SerialNumber[i] = '0' + temp;
                    }
                }

                // EUI-64 Vendor Identifier Field Format, 24 bits for VendId and 40 bits for SN.
                VPDPage83Data.IdDescriptorType2.CodeSet = 1;				//VpdCodeSetBinary;
                VPDPage83Data.IdDescriptorType2.IdentifierType = 2;			//VpdIdentifierTypeEUI64;
                VPDPage83Data.IdDescriptorType2.IdentifierLength = sizeof(VPDPage83Data.IdDescriptorType2) - 4;

                VPDPage83Data.IdDescriptorType2.EU64Id.VendId[0] = 0xD0; // 0x0000055 for IBM, 0x0000D0 for Adaptec.
                VPDPage83Data.IdDescriptorType2.EU64Id.VendId[1] = 0;
                VPDPage83Data.IdDescriptorType2.EU64Id.VendId[2] = 0;

			    VPDPage83Data.IdDescriptorType2.EU64Id.Serial = get_serial_reply->uid;
                VPDPage83Data.IdDescriptorType2.EU64Id.Reserved = 0;

		// VpdIdentifierTypeFCPHName
		VPDPage83Data.IdDescriptorType3.CodeSet = 1;
		VPDPage83Data.IdDescriptorType3.IdentifierType = 3;
		VPDPage83Data.IdDescriptorType3.IdentifierLength = sizeof(VPDPage83Data.IdDescriptorType3) - 4;
		for (container = 0; container < dev->maximum_num_containers; container++) {
			if (scmd_id(scsicmd) == container) {
				for (i = 0; i < 16; i++)
					VPDPage83Data.IdDescriptorType3.Identifier[i] = dev->fsa_dev[container].identifier[i];
				break;
			}
		}

                // Move the inquiry data to the response buffer.
//                memcpy(arr, &VPDPage83Data, sizeof(VPDPage83Data));

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
				aac_internal_transfer(scsicmd, &VPDPage83Data, 0,
				  sizeof(VPDPage83Data));
#else
				scsi_sg_copy_from_buffer(scsicmd, &VPDPage83Data,
							 sizeof(VPDPage83Data));
#endif
		}
		else
#endif
		{
			/* It must be for VPD 0x80 */
			char sp[13];
			/* EVPD bit set */
			sp[0] = INQD_PDT_DA;
			sp[1] = scsicmd->cmnd[2];
			sp[2] = 0;
	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			sp[3] = snprintf(sp+4, sizeof(sp)-4, "%08X",
			  le32_to_cpu(get_serial_reply->uid));
	#else
			sp[3] = sprintf(sp+4, "%08X",
			  le32_to_cpu(get_serial_reply->uid));
	#endif
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			aac_internal_transfer(scsicmd, sp, 0, sizeof(sp));
	#else
			scsi_sg_copy_from_buffer(scsicmd, sp, sizeof(sp));
	#endif
		}
	}

	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;

	aac_fib_complete(fibptr);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	aac_fib_free_tag(fibptr);
#else
	aac_fib_free(fibptr);
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif
}

/**
 *	aac_get_container_serial - get container serial, none blocking.
 */
static int aac_get_container_serial(struct scsi_cmnd * scsicmd)
{
	int status;
	struct aac_get_serial *dinfo;
	struct fib * cmd_fibcontext;
	struct aac_dev * dev;

	dev = (struct aac_dev *)scsicmd->device->host->hostdata;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (!(cmd_fibcontext = aac_fib_alloc_tag(dev, scsicmd)))
#else
	if (!(cmd_fibcontext = aac_fib_alloc(dev)))
#endif
		return -ENOMEM;

	aac_fib_init(cmd_fibcontext);
	dinfo = (struct aac_get_serial *) fib_data(cmd_fibcontext);

	dinfo->command = cpu_to_le32(VM_ContainerConfig);
	dinfo->type = cpu_to_le32(CT_CID_TO_32BITS_UID);
	dinfo->cid = cpu_to_le32(scmd_id(scsicmd));

	status = aac_fib_send(ContainerCommand,
		  cmd_fibcontext,
		  sizeof (struct aac_get_serial_resp),
		  FsaNormal,
		  0, 1,
		  (fib_callback) get_container_serial_callback,
		  (void *) scsicmd);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	printk(KERN_WARNING "aac_get_container_serial: aac_fib_send failed with status: %d.\n", status);
	aac_fib_complete(cmd_fibcontext);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	aac_fib_free_tag(cmd_fibcontext);
#else
	aac_fib_free(cmd_fibcontext);
#endif
	return -1;
}

/* Function: setinqserial
 *
 * Arguments: [1] pointer to void [1] int
 *
 * Purpose: Sets SCSI Unit Serial number.
 *          This is a fake. We should read a proper
 *          serial number from the container. <SuSE>But
 *          without docs it's quite hard to do it :-)
 *          So this will have to do in the meantime.</SuSE>
 */

static int setinqserial(struct aac_dev *dev, void *data, int cid)
{
	/*
	 *	This breaks array migration.
	 */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	return snprintf((char *)(data), sizeof(struct scsi_inq) - 4, "%08X%02X",
			le32_to_cpu(dev->adapter_info.serial[0]), cid);
#else
	return sprintf((char *)(data), "%08X%02X",
			le32_to_cpu(dev->adapter_info.serial[0]), cid);
#endif
}

static inline void set_sense(struct sense_data *sense_data, u8 sense_key,
	u8 sense_code, u8 a_sense_code, u8 bit_pointer, u16 field_pointer)
{
	u8 *sense_buf = (u8 *)sense_data;
	/* Sense data valid, err code 70h */
	sense_buf[0] = 0x70; /* No info field */
	sense_buf[1] = 0;	/* Segment number, always zero */

	sense_buf[2] = sense_key;	/* Sense key */

	sense_buf[12] = sense_code;	/* Additional sense code */
	sense_buf[13] = a_sense_code;	/* Additional sense code qualifier */

	if (sense_key == ILLEGAL_REQUEST) {
		sense_buf[7] = 10;	/* Additional sense length */

		sense_buf[15] = bit_pointer;
		/* Illegal parameter is in the parameter block */
		if (sense_code == SENCODE_INVALID_CDB_FIELD)
			sense_buf[15] |= 0xc0;/* Std sense key specific field */
		/* Illegal parameter is in the CDB block */
		sense_buf[16] = field_pointer >> 8;	/* MSB */
		sense_buf[17] = field_pointer;		/* LSB */
	} else
		sense_buf[7] = 6;	/* Additional sense length */
}

static int aac_bounds_32(struct aac_dev * dev, struct scsi_cmnd * cmd, u64 lba)
{
	if (lba & 0xffffffff00000000LL) {
		int cid = scmd_id(cmd);
		dprintk((KERN_DEBUG "aacraid: Illegal lba\n"));
		cmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
			SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data,
		  HARDWARE_ERROR, SENCODE_INTERNAL_TARGET_FAILURE,
		  ASENCODE_INTERNAL_TARGET_FAILURE, 0, 0);
		memcpy(cmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		__aac_io_done(cmd);
#else
		cmd->scsi_done(cmd);
#endif
		return 1;
	}
	return 0;
}

static int aac_bounds_64(struct aac_dev * dev, struct scsi_cmnd * cmd, u64 lba)
{
	return 0;
}

static void io_callback(void *context, struct fib * fibptr);

static int aac_read_raw_io(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count)
{
	struct aac_dev *dev = fib->dev;
	u16 fibsize, command;
	long ret;

	aac_fib_init(fib);
	if ((dev->comm_interface == AAC_COMM_MESSAGE_TYPE2 ||
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE3) && !dev->sync_mode) {
		struct aac_raw_io2 *readcmd2;
		readcmd2 = (struct aac_raw_io2 *) fib_data(fib);
		memset(readcmd2, 0, sizeof(struct aac_raw_io2));
		readcmd2->blockLow = cpu_to_le32((u32)(lba&0xffffffff));
		readcmd2->blockHigh = cpu_to_le32((u32)((lba&0xffffffff00000000LL)>>32));
		readcmd2->byteCount = cpu_to_le32(count * dev->fsa_dev[scmd_id(cmd)].block_size);
		readcmd2->cid = cpu_to_le16(scmd_id(cmd));
		readcmd2->flags = cpu_to_le16(RIO2_IO_TYPE_READ);
		ret = aac_build_sgraw2(cmd, readcmd2, dev->scsi_host_ptr->sg_tablesize);
		if (ret < 0)
			return ret;
		command = ContainerRawIo2;
		fibsize = sizeof(struct aac_raw_io2) +
			((le32_to_cpu(readcmd2->sgeCnt)-1) * sizeof(struct sge_ieee1212));
	} else {
		struct aac_raw_io *readcmd;
		readcmd = (struct aac_raw_io *) fib_data(fib);
		readcmd->block[0] = cpu_to_le32((u32)(lba&0xffffffff));
		readcmd->block[1] = cpu_to_le32((u32)((lba&0xffffffff00000000LL)>>32));
		readcmd->count = cpu_to_le32(count * dev->fsa_dev[scmd_id(cmd)].block_size);
		readcmd->cid = cpu_to_le16(scmd_id(cmd));
		readcmd->flags = cpu_to_le16(RIO_TYPE_READ);
		readcmd->bpTotal = 0;
		readcmd->bpComplete = 0;
		ret = aac_build_sgraw(cmd, &readcmd->sg);
		if (ret < 0)
			return ret;
		command = ContainerRawIo;
		fibsize = sizeof(struct aac_raw_io) +
			((le32_to_cpu(readcmd->sg.count)-1) * sizeof(struct sgentryraw));
	}

	BUG_ON(fibsize > (fib->dev->max_fib_size - sizeof(struct aac_fibhdr)));
	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(command,
			  fib,
			  fibsize,
			  FsaNormal,
			  0, 1,
			  (fib_callback) io_callback,
			  (void *) cmd);
}

static int aac_read_block64(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count)
{
	u16 fibsize;
	struct aac_read64 *readcmd;
	long ret;

	aac_fib_init(fib);
	readcmd = (struct aac_read64 *) fib_data(fib);
	readcmd->command = cpu_to_le32(VM_CtHostRead64);
	readcmd->cid = cpu_to_le16(scmd_id(cmd));
	readcmd->sector_count = cpu_to_le16(count);
	readcmd->block = cpu_to_le32((u32)(lba&0xffffffff));
	readcmd->pad   = 0;
	readcmd->flags = 0;

	ret = aac_build_sg64(cmd, &readcmd->sg);
	if (ret < 0)
		return ret;
	fibsize = sizeof(struct aac_read64) +
		((le32_to_cpu(readcmd->sg.count) - 1) *
		 sizeof (struct sgentry64));
	BUG_ON (fibsize > (fib->dev->max_fib_size -
				sizeof(struct aac_fibhdr)));
	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(ContainerCommand64,
			  fib,
			  fibsize,
			  FsaNormal,
			  0, 1,
			  (fib_callback) io_callback,
			  (void *) cmd);
}

static int aac_read_block(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count)
{
	u16 fibsize;
	struct aac_read *readcmd;
	struct aac_dev *dev = fib->dev;
	long ret;

	aac_fib_init(fib);
	readcmd = (struct aac_read *) fib_data(fib);
	readcmd->command = cpu_to_le32(VM_CtBlockRead);
	readcmd->cid = cpu_to_le32(scmd_id(cmd));
	readcmd->block = cpu_to_le32((u32)(lba&0xffffffff));
	readcmd->count = cpu_to_le32(count * dev->fsa_dev[scmd_id(cmd)].block_size);

	ret = aac_build_sg(cmd, &readcmd->sg);
	if (ret < 0)
		return ret;
	fibsize = sizeof(struct aac_read) +
			((le32_to_cpu(readcmd->sg.count) - 1) *
			 sizeof (struct sgentry));
	BUG_ON (fibsize > (fib->dev->max_fib_size -
				sizeof(struct aac_fibhdr)));
	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(ContainerCommand,
			  fib,
			  fibsize,
			  FsaNormal,
			  0, 1,
			  (fib_callback) io_callback,
			  (void *) cmd);
}

static int aac_write_raw_io(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count, int fua)
{
	struct aac_dev *dev = fib->dev;
	u16 fibsize, command;
	long ret;

	aac_fib_init(fib);
	if ((dev->comm_interface == AAC_COMM_MESSAGE_TYPE2 ||
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE3) && !dev->sync_mode) {
		struct aac_raw_io2 *writecmd2;
		writecmd2 = (struct aac_raw_io2 *) fib_data(fib);
		memset(writecmd2, 0, sizeof(struct aac_raw_io2));
		writecmd2->blockLow = cpu_to_le32((u32)(lba&0xffffffff));
		writecmd2->blockHigh = cpu_to_le32((u32)((lba&0xffffffff00000000LL)>>32));
		writecmd2->byteCount = cpu_to_le32(count * dev->fsa_dev[scmd_id(cmd)].block_size);
		writecmd2->cid = cpu_to_le16(scmd_id(cmd));
		writecmd2->flags = (fua && ((aac_cache & 5) != 1) &&
						   (((aac_cache & 5) != 5) || !fib->dev->cache_protected)) ?
			cpu_to_le16(RIO2_IO_TYPE_WRITE|RIO2_IO_SUREWRITE) :
			cpu_to_le16(RIO2_IO_TYPE_WRITE);
		ret = aac_build_sgraw2(cmd, writecmd2, dev->scsi_host_ptr->sg_tablesize);
		if (ret < 0)
			return ret;
		command = ContainerRawIo2;
		fibsize = sizeof(struct aac_raw_io2) +
			((le32_to_cpu(writecmd2->sgeCnt)-1) * sizeof(struct sge_ieee1212));
	} else {
		struct aac_raw_io *writecmd;
		writecmd = (struct aac_raw_io *) fib_data(fib);
		writecmd->block[0] = cpu_to_le32((u32)(lba&0xffffffff));
		writecmd->block[1] = cpu_to_le32((u32)((lba&0xffffffff00000000LL)>>32));
		writecmd->count = cpu_to_le32(count * dev->fsa_dev[scmd_id(cmd)].block_size);
		writecmd->cid = cpu_to_le16(scmd_id(cmd));
#if (defined(RIO_SUREWRITE))
		writecmd->flags = (fua && ((aac_cache & 5) != 1) &&
						   (((aac_cache & 5) != 5) || !fib->dev->cache_protected)) ?
			cpu_to_le16(RIO_TYPE_WRITE|RIO_SUREWRITE) :
			cpu_to_le16(RIO_TYPE_WRITE);
#else
		writecmd->flags = cpu_to_le16(RIO_TYPE_WRITE);
#endif
		writecmd->bpTotal = 0;
		writecmd->bpComplete = 0;
		ret = aac_build_sgraw(cmd, &writecmd->sg);
		if (ret < 0)
			return ret;
		command = ContainerRawIo;
		fibsize = sizeof(struct aac_raw_io) +
			((le32_to_cpu(writecmd->sg.count)-1) * sizeof (struct sgentryraw));
	}

	BUG_ON(fibsize > (fib->dev->max_fib_size - sizeof(struct aac_fibhdr)));
	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(command,
			  fib,
			  fibsize,
			  FsaNormal,
			  0, 1,
			  (fib_callback) io_callback,
			  (void *) cmd);
}

static int aac_write_block64(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count, int fua)
{
	u16 fibsize;
	struct aac_write64 *writecmd;
	long ret;

	aac_fib_init(fib);
	writecmd = (struct aac_write64 *) fib_data(fib);
	writecmd->command = cpu_to_le32(VM_CtHostWrite64);
	writecmd->cid = cpu_to_le16(scmd_id(cmd));
	writecmd->sector_count = cpu_to_le16(count);
	writecmd->block = cpu_to_le32((u32)(lba&0xffffffff));
	writecmd->pad	= 0;
	writecmd->flags	= 0;

	ret = aac_build_sg64(cmd, &writecmd->sg);
	if (ret < 0)
		return ret;
	fibsize = sizeof(struct aac_write64) +
		((le32_to_cpu(writecmd->sg.count) - 1) *
		 sizeof (struct sgentry64));
	BUG_ON (fibsize > (fib->dev->max_fib_size -
				sizeof(struct aac_fibhdr)));
	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(ContainerCommand64,
			  fib,
			  fibsize,
			  FsaNormal,
			  0, 1,
			  (fib_callback) io_callback,
			  (void *) cmd);
}

static int aac_write_block(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count, int fua)
{
	u16 fibsize;
	struct aac_write *writecmd;
	struct aac_dev *dev = fib->dev;
	long ret;

	aac_fib_init(fib);
	writecmd = (struct aac_write *) fib_data(fib);
	writecmd->command = cpu_to_le32(VM_CtBlockWrite);
	writecmd->cid = cpu_to_le32(scmd_id(cmd));
	writecmd->block = cpu_to_le32((u32)(lba&0xffffffff));
	writecmd->count = cpu_to_le32(count * dev->fsa_dev[scmd_id(cmd)].block_size);
	writecmd->sg.count = cpu_to_le32(1);
	/* ->stable is not used - it did mean which type of write */

	ret = aac_build_sg(cmd, &writecmd->sg);
	if (ret < 0)
		return ret;
	fibsize = sizeof(struct aac_write) +
		((le32_to_cpu(writecmd->sg.count) - 1) *
		 sizeof (struct sgentry));
	BUG_ON (fibsize > (fib->dev->max_fib_size -
				sizeof(struct aac_fibhdr)));
	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(ContainerCommand,
			  fib,
			  fibsize,
			  FsaNormal,
			  0, 1,
			  (fib_callback) io_callback,
			  (void *) cmd);
}

static struct aac_srb * aac_scsi_common(struct fib * fib, struct scsi_cmnd * cmd)
{
	struct aac_srb * srbcmd;
	u32 flag;
	u32 timeout;

	aac_fib_init(fib);
	switch(cmd->sc_data_direction){
	case DMA_TO_DEVICE:
		flag = SRB_DataOut;
		break;
	case DMA_BIDIRECTIONAL:
		flag = SRB_DataIn | SRB_DataOut;
		break;
	case DMA_FROM_DEVICE:
		flag = SRB_DataIn;
		break;
	case DMA_NONE:
	default:	/* shuts up some versions of gcc */
		flag = SRB_NoDataXfer;
		break;
	}

	srbcmd = (struct aac_srb*) fib_data(fib);
	srbcmd->function = cpu_to_le32(SRBF_ExecuteScsi);
	srbcmd->channel  = cpu_to_le32(aac_logical_to_phys(scmd_channel(cmd)));
	srbcmd->id       = cpu_to_le32(scmd_id(cmd));
	srbcmd->lun      = cpu_to_le32(cmd->device->lun);
	srbcmd->flags    = cpu_to_le32(flag);
#if (1 && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
	timeout = cmd->timeout_per_command/HZ;
#else
	timeout = cmd->request->timeout/HZ;
#endif
	if (timeout == 0)
		timeout = 1;
	srbcmd->timeout  = cpu_to_le32(timeout);  // timeout in seconds
	srbcmd->retry_limit = 0; /* Obsolete parameter */
	srbcmd->cdb_size = cpu_to_le32(cmd->cmd_len);
	return srbcmd;
}

static struct aac_hba_cmd_req * aac_construct_hbacmd(struct fib * fib, struct scsi_cmnd * cmd)
{
	struct aac_hba_cmd_req * hbacmd;
	struct aac_dev *dev;
	int bus, target;
	u64 address;

	dev = (struct aac_dev *)cmd->device->host->hostdata;

	hbacmd = (struct aac_hba_cmd_req *)fib->hw_fib_va;
	memset(hbacmd, 0, 96);	/* sizeof(*hbacmd) is not necessary */
	/* iu_type is a parameter of aac_hba_send */
	switch (cmd->sc_data_direction) {
	case DMA_TO_DEVICE:
		hbacmd->byte1 = 2;
		break;
	case DMA_FROM_DEVICE:
	case DMA_BIDIRECTIONAL:
		hbacmd->byte1 = 1;
		break;
	case DMA_NONE:
	default:
		break;
	}
	hbacmd->lun[1] = cpu_to_le32(cmd->device->lun);

	bus = aac_logical_to_phys(scmd_channel(cmd));
	target = scmd_id(cmd);
	hbacmd->it_nexus = dev->hba_map[bus][target].rmw_nexus;

	/* we fill in reply_qid later in aac_src_deliver_message */
	/* we fill in iu_type, request_id later in aac_hba_send */
	/* we fill in emb_data_desc_count later in aac_build_sghba */

	memcpy(hbacmd->cdb, cmd->cmnd, cmd->cmd_len);
	hbacmd->data_length = cpu_to_le32(scsi_bufflen(cmd));

	address = (u64)fib->hw_error_pa;
	hbacmd->error_ptr_hi = cpu_to_le32((u32)(address >> 32));
	hbacmd->error_ptr_lo = cpu_to_le32((u32)(address & 0xffffffff));
	hbacmd->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);

	return hbacmd;
}

static void aac_srb_callback(void *context, struct fib * fibptr);

static int aac_scsi_64(struct fib * fib, struct scsi_cmnd * cmd)
{
	u16 fibsize;
	struct aac_srb * srbcmd = aac_scsi_common(fib, cmd);
	long ret;

	ret = aac_build_sg64(cmd, (struct sgmap64*) &srbcmd->sg);
	if (ret < 0)
		return ret;
	srbcmd->count = cpu_to_le32(scsi_bufflen(cmd));

	memset(srbcmd->cdb, 0, sizeof(srbcmd->cdb));
	memcpy(srbcmd->cdb, cmd->cmnd, cmd->cmd_len);
	/*
	 *	Build Scatter/Gather list
	 */
	fibsize = sizeof (struct aac_srb) - sizeof (struct sgentry) +
		((le32_to_cpu(srbcmd->sg.count) & 0xff) *
		 sizeof (struct sgentry64));
	BUG_ON (fibsize > (fib->dev->max_fib_size -
				sizeof(struct aac_fibhdr)));

	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(ScsiPortCommand64, fib,
				fibsize, FsaNormal, 0, 1,
				  (fib_callback) aac_srb_callback,
				  (void *) cmd);
}

static int aac_scsi_32(struct fib * fib, struct scsi_cmnd * cmd)
{
	u16 fibsize;
	struct aac_srb * srbcmd = aac_scsi_common(fib, cmd);
	long ret;

	ret = aac_build_sg(cmd, (struct sgmap*)&srbcmd->sg);
	if (ret < 0)
		return ret;
	srbcmd->count = cpu_to_le32(scsi_bufflen(cmd));

	memset(srbcmd->cdb, 0, sizeof(srbcmd->cdb));
	memcpy(srbcmd->cdb, cmd->cmnd, cmd->cmd_len);
	/*
	 *	Build Scatter/Gather list
	 */
	fibsize = sizeof (struct aac_srb) +
		(((le32_to_cpu(srbcmd->sg.count) & 0xff) - 1) *
		 sizeof (struct sgentry));
	BUG_ON (fibsize > (fib->dev->max_fib_size -
				sizeof(struct aac_fibhdr)));

	/*
	 *	Now send the Fib to the adapter
	 */
	return aac_fib_send(ScsiPortCommand, fib, fibsize, FsaNormal, 0, 1,
				  (fib_callback) aac_srb_callback, (void *) cmd);
}

static int aac_scsi_32_64(struct fib * fib, struct scsi_cmnd * cmd)
{
	if ((sizeof(dma_addr_t) > 4) && fib->dev->needs_dac &&
	    (fib->dev->adapter_info.options & AAC_OPT_SGMAP_HOST64))
		return FAILED;
	return aac_scsi_32(fib, cmd);
}

void aac_hba_callback(void *context, struct fib * fibptr);

static int aac_adapter_hba(struct fib * fib, struct scsi_cmnd * cmd)
{
	struct aac_hba_cmd_req * hbacmd = aac_construct_hbacmd(fib, cmd);
	struct aac_dev *dev;
	// u16 fibsize;
	long ret;

	dev = (struct aac_dev *)cmd->device->host->hostdata;

	ret = aac_build_sghba(cmd, hbacmd,
		dev->scsi_host_ptr->sg_tablesize, (u64)fib->hw_sgl_pa);
	if (ret < 0)
		return ret;

	/*
	 *	Now send the HBA command to the adapter
	 */
	fib->hbacmd_size = 64 + le32_to_cpu(hbacmd->emb_data_desc_count) *
		sizeof(struct aac_hba_sgl);

	return aac_hba_send(HBA_IU_TYPE_SCSI_CMD_REQ, fib,
				  (fib_callback) aac_hba_callback,
				  (void *) cmd);
}

int aac_issue_bmic_identify(struct aac_dev* dev, u32 bus, u32 target)
{
	struct fib* fibptr;
	int rcode = -1;
	u16 fibsize, datasize;
	struct aac_srb *srbcmd;
	struct sgmap64 *sg64;
	struct aac_ciss_identify_pd *identify_resp;
	dma_addr_t addr;
	u32 vbus, vid;
	u16 temp;

	if (!(fibptr = aac_fib_alloc(dev)))
		return -ENOMEM;
	
	temp = AAC_MAX_LUN + target;

	fibsize = sizeof (struct aac_srb) -
		sizeof (struct sgentry) + sizeof (struct sgentry64);
	datasize = sizeof (struct aac_ciss_identify_pd);

	identify_resp = (struct aac_ciss_identify_pd *)
		pci_alloc_consistent(dev->pdev, datasize, &addr);

	if (identify_resp != NULL) 
	{
		vbus = (u32)le16_to_cpu(
			dev->supplement_adapter_info.VirtDeviceBus);
		vid = (u32)le16_to_cpu(
			dev->supplement_adapter_info.VirtDeviceTarget);

		aac_fib_init(fibptr);
		srbcmd = (struct aac_srb *) fib_data(fibptr);

		srbcmd->function = cpu_to_le32(SRBF_ExecuteScsi);
		srbcmd->channel  = cpu_to_le32(vbus);
		srbcmd->id       = cpu_to_le32(vid);
		srbcmd->lun      = 0;
		srbcmd->flags    = cpu_to_le32(SRB_DataIn);
		srbcmd->timeout  = cpu_to_le32(10);
		srbcmd->retry_limit = 0;
		srbcmd->cdb_size = cpu_to_le32(12);
		srbcmd->count = cpu_to_le32(datasize);

		memset(srbcmd->cdb, 0, sizeof(srbcmd->cdb));
		srbcmd->cdb[0] = 0x26;
		srbcmd->cdb[2] = (u8)(temp & 0x00FF);

		srbcmd->cdb[6] = CISS_IDENTIFY_PHYSICAL_DEVICE;

		sg64 = (struct sgmap64 *)&srbcmd->sg;
		sg64->count = cpu_to_le32(1);
		sg64->sg[0].addr[1] = cpu_to_le32((u32)(((addr) >> 16) >> 16));
		sg64->sg[0].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
		sg64->sg[0].count = cpu_to_le32(datasize);

		rcode = aac_fib_send(ScsiPortCommand64,
			fibptr, fibsize, FsaNormal, 1, 1, NULL, NULL);

		if( identify_resp->current_queue_depth_limit <= 0 ||
			identify_resp->current_queue_depth_limit > 32 )
			dev->hba_map[bus][target].qd_limit = 32;
		else
			dev->hba_map[bus][target].qd_limit = identify_resp->current_queue_depth_limit;

		pci_free_consistent(dev->pdev, datasize, (void *)identify_resp, addr);

		aac_fib_complete(fibptr);
	}

	aac_fib_free(fibptr);

	return rcode;
}

int aac_get_adapter_info(struct aac_dev* dev)
{
	struct fib* fibptr;
	int rcode, is_StreamLinedFIB;
	u32 tmp, i, bus, target;
	struct aac_adapter_info *info;
	struct aac_bus_info *command;
	struct aac_bus_info_response *bus_info;
	u16 fibsize, datasize;
	struct aac_srb *srbcmd;
	struct sgmap64 *sg64;
	struct aac_ciss_phys_luns_resp *phys_luns;
	dma_addr_t addr;

	is_StreamLinedFIB = 0;

	if (!(fibptr = aac_fib_alloc(dev)))
		return -ENOMEM;

	aac_fib_init(fibptr);
	info = (struct aac_adapter_info *) fib_data(fibptr);
	memset(info,0,sizeof(*info));

	dev->streamlined_fib_support = 0;
	rcode = aac_fib_send(RequestAdapterInfo,
			 fibptr,
			 sizeof(*info),
			 FsaNormal,
			 -1, 1, /* First `interrupt' command uses special wait */
			 NULL,
			 NULL);

	if (rcode < 0) {
		/* FIB should be freed only after
		 * getting the response from the F/W */
		if (rcode != -ERESTARTSYS) {
			aac_fib_complete(fibptr);
			aac_fib_free(fibptr);
		}
		return rcode;
	}
	memcpy(&dev->adapter_info, info, sizeof(*info));

	dev->supplement_adapter_info.VirtDeviceBus = 0xffff;
	if (dev->adapter_info.options & AAC_OPT_SUPPLEMENT_ADAPTER_INFO) {
		struct aac_supplement_adapter_info * sinfo;

		aac_fib_init(fibptr);

		sinfo = (struct aac_supplement_adapter_info *) fib_data(fibptr);

		memset(sinfo,0,sizeof(*sinfo));

		rcode = aac_fib_send(RequestSupplementAdapterInfo,
				 fibptr,
				 sizeof(*sinfo),
				 FsaNormal,
				 1, 1,
				 NULL,
				 NULL);

		if (rcode >= 0)
			memcpy(&dev->supplement_adapter_info, sinfo, sizeof(*sinfo));

		if (rcode == -ERESTARTSYS) {
			fibptr = aac_fib_alloc(dev);
			if (!fibptr)
				return -ENOMEM;
		}
#if (defined(AAC_DEBUG_INSTRUMENT_SLOT))
		if ((le32_to_cpu(dev->supplement_adapter_info.Version)
		      < AAC_SIS_VERSION_V3) ||
		    (dev->supplement_adapter_info.SlotNumber
		      == cpu_to_le32(AAC_SIS_SLOT_UNKNOWN))) {
			dev->supplement_adapter_info.SlotNumber
			  = cpu_to_le32(PCI_SLOT(dev->pdev->devfn));
			(void)aac_adapter_sync_cmd(dev, SEND_SLOT_NUMBER,
			  PCI_SLOT(dev->pdev->devfn),
			  0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);
#endif
	}
	if (dev->supplement_adapter_info.FeatureBits & AAC_FEATURE_STREAMLINED_CONTINUATION)
		dev->streamlined_fib_support = 1;

	if (dev->adapter_info.options & AAC_OPT_2K_FIB_SUPPORT)
		dev->fib_size_supported = FIB_SIZE_2K;
	else
		dev->fib_size_supported = FIB_SIZE_STANDARD;

#if (defined(CODE_STREAM_IDENTIFIER) && !defined(CONFIG_COMMUNITY_KERNEL))
	if (dev->supplement_adapter_info.FeatureBits & AAC_FEATURE_FALCON) {
		char * finfo;

		aac_fib_init(fibptr);

		finfo = (char *) fib_data(fibptr);

		memset(finfo,0,MAX_CODE_STREAM_IDENTIFIER_LENGTH);

		rcode = aac_fib_send(RequestCompatibilityId,
			fibptr,
			MAX_CODE_STREAM_IDENTIFIER_LENGTH,
			FsaNormal,
			1, 1,
			NULL,
			NULL);

		if (rcode >= 0)
			memcpy(dev->code_stream_identifier, finfo,
			  MAX_CODE_STREAM_IDENTIFIER_LENGTH);

		if (dev->code_stream_identifier[0]
		 && strncmp(CODE_STREAM_IDENTIFIER,
		  dev->code_stream_identifier,
		  MAX_CODE_STREAM_IDENTIFIER_LENGTH)) {
			extern char aac_driver_version[];
			printk(KERN_INFO
			  "%s%d: Warning ! ! ! Compatibility Mismatch\n",
			  dev->name, dev->id);
			tmp = le32_to_cpu(dev->adapter_info.kernelrev);
			printk(KERN_INFO
			  "%s%d: Firmware=%d.%d-%d[%d], Device Driver=%s\n",
			  dev->name, dev->id,
			  tmp>>24,(tmp>>16)&0xff,tmp&0xff,
			  le32_to_cpu(dev->adapter_info.kernelbuild),
			  aac_driver_version);
			printk(KERN_INFO
			  "%s%d: These should be a tested set to avoid possible compatibility problems.\n",
			  dev->name, dev->id);
		}
	}
#endif

	/* reset all previous mapped devices (i.e. for init. after IOP_RESET) */
	for (bus = 0; bus < AAC_MAX_BUSES; bus++)
		for (target = 0; target < AAC_MAX_TARGETS; target++) {
			dev->hba_map[bus][target].devtype = 0;
			dev->hba_map[bus][target].qd_limit = 0;
		}

	/*
	 * GetBusInfo
	 */
	aac_fib_init(fibptr);

	bus_info = (struct aac_bus_info_response *) fib_data(fibptr);
	memset(bus_info, 0, sizeof(*bus_info));
	command = (struct aac_bus_info *)bus_info;

	command->Command = cpu_to_le32(VM_Ioctl);
	command->ObjType = cpu_to_le32(FT_DRIVE);
	command->MethodId = cpu_to_le32(1);
	command->CtlCmd = cpu_to_le32(GetBusInfo);

	rcode = aac_fib_send(ContainerCommand,
			 fibptr,
			 sizeof (*bus_info),
			 FsaNormal,
			 1, 1,
			 NULL, NULL);

	/* reasoned default */
	dev->maximum_num_physicals = 16;
	if (rcode >= 0 && le32_to_cpu(bus_info->Status) == ST_OK) {
		dev->maximum_num_physicals = le32_to_cpu(bus_info->TargetsPerBus);
		dev->maximum_num_channels = le32_to_cpu(bus_info->BusCount);
	}

	if (!dev->sync_mode && dev->sa_firmware &&
		dev->supplement_adapter_info.VirtDeviceBus != 0xffff) {
		/* Thor SA Firmware -> CISS_REPORT_PHYSICAL_LUNS */
		fibsize = sizeof (struct aac_srb) -
			sizeof (struct sgentry) + sizeof (struct sgentry64);
		datasize = sizeof (struct aac_ciss_phys_luns_resp) +
			(AAC_MAX_NATIVE_TARGETS-1) * sizeof (struct _ciss_lun);

		phys_luns = (struct aac_ciss_phys_luns_resp *)
			pci_alloc_consistent(dev->pdev, datasize, &addr);
		if (phys_luns != NULL) {
			u32 vbus, vid;
			vbus = (u32)le16_to_cpu(
				dev->supplement_adapter_info.VirtDeviceBus);
			vid = (u32)le16_to_cpu(
				dev->supplement_adapter_info.VirtDeviceTarget);

			aac_fib_init(fibptr);
			srbcmd = (struct aac_srb *) fib_data(fibptr);

			srbcmd->function = cpu_to_le32(SRBF_ExecuteScsi);
			srbcmd->channel  = cpu_to_le32(vbus);
			srbcmd->id       = cpu_to_le32(vid);
			srbcmd->lun      = 0;
			srbcmd->flags    = cpu_to_le32(SRB_DataIn);
			srbcmd->timeout  = cpu_to_le32(10);
			srbcmd->retry_limit = 0;
			srbcmd->cdb_size = cpu_to_le32(12);
			srbcmd->count = cpu_to_le32(datasize);

			memset(srbcmd->cdb, 0, sizeof(srbcmd->cdb));
			srbcmd->cdb[0] = CISS_REPORT_PHYSICAL_LUNS;
			srbcmd->cdb[1] = 2;		/* extended reporting */
			srbcmd->cdb[8] = (u8)(datasize>>8);
			srbcmd->cdb[9] = (u8)(datasize);

			sg64 = (struct sgmap64 *)&srbcmd->sg;
			sg64->count = cpu_to_le32(1);
			sg64->sg[0].addr[1] = cpu_to_le32((u32)(((addr) >> 16) >> 16));
			sg64->sg[0].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
			sg64->sg[0].count = cpu_to_le32(datasize);

			rcode = aac_fib_send(ScsiPortCommand64,
				fibptr, fibsize, FsaNormal, 1, 1, NULL, NULL);

			/* analyse data */
			if (rcode >= 0 && phys_luns->resp_flag == 2) {
				/* ok and extended reporting */
				u32 lun_count, nexus;
				u8 expose_flag, attribs;

				lun_count = ((phys_luns->list_length[0]<<24) +
					(phys_luns->list_length[1]<<16) +
					(phys_luns->list_length[2]<<8) +
					(phys_luns->list_length[3])) / 24;
				for (i = 0; i < lun_count; ++i) {
				 bus = phys_luns->lun[i].level2[1] & 0x3f;
				 target = phys_luns->lun[i].level2[0];
				 expose_flag = phys_luns->lun[i].bus >> 6;
				 attribs = phys_luns->lun[i].node_ident[9];
				 nexus = *((u32 *)&phys_luns->lun[i].node_ident[12]);
				 if (bus < AAC_MAX_BUSES &&
					target < AAC_MAX_TARGETS) {
					  dev->hba_map[bus][target].expose = expose_flag;
					  if (expose_flag == 0) {
					   if (nexus != 0 && (attribs & 8)) {
					    dev->hba_map[bus][target].devtype =
						AAC_DEVTYPE_NATIVE_RAW;
					    dev->hba_map[bus][target].rmw_nexus =
						nexus;
					   } else {
					    dev->hba_map[bus][target].devtype =
						AAC_DEVTYPE_ARC_RAW;
					   }
					  } else {
					   dev->hba_map[bus][target].devtype =
						AAC_DEVTYPE_RAID_MEMBER;
					  }
					  if( dev->hba_map[bus][target].devtype == AAC_DEVTYPE_NATIVE_RAW )
					 	if( aac_issue_bmic_identify(dev, bus, target) < 0 )
							dev->hba_map[bus][target].qd_limit = 32;
					 }
				}
			}
		}
		pci_free_consistent(dev->pdev, datasize,
			(void *)phys_luns, addr);
	}

	if (!dev->in_reset) {
		char buffer[16];
		tmp = le32_to_cpu(dev->adapter_info.kernelrev);
		printk(KERN_INFO "%s%d: kernel %d.%d-%d[%d] %.*s\n",
			dev->name,
			dev->id,
			tmp>>24,
			(tmp>>16)&0xff,
			tmp&0xff,
			le32_to_cpu(dev->adapter_info.kernelbuild),
			(int)sizeof(dev->supplement_adapter_info.BuildDate),
			dev->supplement_adapter_info.BuildDate);
#if (0 && defined(BOOTCD))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "%s%d: kernel %d.%d-%d[%d] %.*s",
		  dev->name, dev->id,
		  tmp>>24, (tmp>>16)&0xff, tmp&0xff,
		  le32_to_cpu(dev->adapter_info.kernelbuild),
		  (int)sizeof(dev->supplement_adapter_info.BuildDate),
		  dev->supplement_adapter_info.BuildDate));
#endif
		tmp = le32_to_cpu(dev->adapter_info.monitorrev);
		printk(KERN_INFO "%s%d: monitor %d.%d-%d[%d]\n",
			dev->name, dev->id,
			tmp>>24,(tmp>>16)&0xff,tmp&0xff,
			le32_to_cpu(dev->adapter_info.monitorbuild));
#if (0 && defined(BOOTCD))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "%s%d: monitor %d.%d-%d[%d]",
		  dev->name, dev->id, tmp>>24,(tmp>>16)&0xff,tmp&0xff,
		  le32_to_cpu(dev->adapter_info.monitorbuild)));
#endif
		tmp = le32_to_cpu(dev->adapter_info.biosrev);
		printk(KERN_INFO "%s%d: bios %d.%d-%d[%d]\n",
			dev->name, dev->id,
			tmp>>24,(tmp>>16)&0xff,tmp&0xff,
			le32_to_cpu(dev->adapter_info.biosbuild));
#if (0 && defined(BOOTCD))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "%s%d: bios %d.%d-%d[%d]",
		  dev->name, dev->id,
		  tmp>>24,(tmp>>16)&0xff,tmp&0xff,
		  le32_to_cpu(dev->adapter_info.biosbuild)));
#endif
		buffer[0] = '\0';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
		if (aac_show_serial_number(
		  shost_to_class(dev->scsi_host_ptr), buffer))
#else
		if (aac_get_serial_number(
		  shost_to_class(dev->scsi_host_ptr), buffer))
#endif
#if (0 && defined(BOOTCD))
		{
#endif
			printk(KERN_INFO "%s%d: serial %s",
			  dev->name, dev->id, buffer);
#if (0 && defined(BOOTCD))
			if (nblank(fwprintf(x))) {
				char * cp = strchr(buffer, '\n');
				if (cp)
					*cp = '\0';
				fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  "%s%d: serial %s",
				  dev->name, dev->id, buffer));
			}
		}
#endif
		if (dev->supplement_adapter_info.VpdInfo.Tsid[0]) {
			printk(KERN_INFO "%s%d: TSID %.*s\n",
			  dev->name, dev->id,
			  (int)sizeof(dev->supplement_adapter_info.VpdInfo.Tsid),
			  dev->supplement_adapter_info.VpdInfo.Tsid);
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "%s%d: TSID %.*s",
			  dev->name, dev->id,
			  (int)sizeof(dev->supplement_adapter_info.VpdInfo.Tsid),
			  dev->supplement_adapter_info.VpdInfo.Tsid));
#endif
		}
		if (!aac_check_reset || ((aac_check_reset == 1) &&
		  (dev->supplement_adapter_info.SupportedOptions2 &
		  AAC_OPTION_IGNORE_RESET))) {
			printk(KERN_INFO "%s%d: Reset Adapter Ignored\n",
			  dev->name, dev->id);
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "%s%d: Reset Adapter Ignored",
			  dev->name, dev->id));
#endif
		}
	}
#if (!defined(CONFIG_COMMUNITY_KERNEL) && !defined(__VMKLNX30__) && !defined(__VMKLNX__) && ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || !defined(HAS_BOOT_CONFIG)))
#if (((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC)) || !defined(MODULE))
	aacraid = kmalloc(COMMAND_LINE_SIZE, GFP_KERNEL);
#else
	aacraid = kzalloc(COMMAND_LINE_SIZE, GFP_KERNEL);
#endif
	if (aacraid) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#if (defined(MODULE))
		extern struct proc_dir_entry proc_root;
		struct proc_dir_entry * entry;

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		memset(aacraid, 0, COMMAND_LINE_SIZE);
#endif
		for (entry = proc_root.subdir;
		  entry != (struct proc_dir_entry *)NULL;
		  entry = entry->next) {
		  adbg_setup(dev,KERN_INFO,"\"%.*s\"[%d]=%x ", entry->namelen,
			  entry->name, entry->namelen, entry->low_ino);
			if ((entry->low_ino != 0)
			 && (entry->namelen == 7)
			 && (memcmp ("cmdline", entry->name, 7) == 0)) {
				adbg_setup(dev,KERN_INFO,"%p->read_proc=%p ", entry, entry->read_proc);
				if (entry->read_proc != (int (*)(char *, char **, off_t, int, int *, void *))NULL) {
					char * start = aacraid;
					int eof;
					mm_segment_t fs;

					fs = get_fs();
					set_fs(get_ds());
					lock_kernel();
					entry->read_proc(aacraid, &start,
					  (off_t)0, COMMAND_LINE_SIZE-1, &eof,
					  NULL);
					unlock_kernel();
					set_fs(fs);
					adbg_setup(dev,KERN_INFO,
					  "cat /proc/cmdline -> \"%s\"\n",
					  aacraid);
#if (defined(AAC_DEBUG_INSTRUMENT_SETUP) || defined(BOOTCD))
					fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
					  "cat /proc/cmdline -> \"%s\"",
					  aacraid));
#endif
				}
				break;
			}
		}
#else
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,20))
		extern char *saved_command_line;
#else
		extern char saved_command_line[];
#endif
		memcpy(aacraid, saved_command_line, COMMAND_LINE_SIZE);
#endif
#endif
	}
	if (aacraid && aacraid[0])
		aacraid_setup(dev,aacraid);
#endif
#if (defined(AAC_EXTENDED_TIMEOUT))
	adbg_setup( dev,KERN_INFO,"nondasd=%d dacmode=%d commit=%d "
	  "coalescethreshold=%d extendedtimeout=%d\n",
	  nondasd, dacmode, aac_commit, coalescethreshold,
	  extendedtimeout);
#else
	adbg_setup( dev,KERN_INFO,"nondasd=%d dacmode=%d commit=%d "
	  "coalescethreshold=%d\n",
	  nondasd, dacmode, aac_commit, coalescethreshold);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SETUP) || (0 && defined(BOOTCD)))
#if (defined(AAC_EXTENDED_TIMEOUT))
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "nondasd=%d dacmode=%d commit=%d "
	  "coalescethreshold=%d extendedtimeout=%d",
	  nondasd, dacmode, aac_commit, coalescethreshold,
	  extendedtimeout));
#else
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  "nondasd=%d dacmode=%d commit=%d "
	  "coalescethreshold=%d",
	  nondasd, dacmode, aac_commit, coalescethreshold));
#endif
#endif

	dev->cache_protected = 0;
	dev->jbod = ((dev->supplement_adapter_info.FeatureBits &
		AAC_FEATURE_JBOD) != 0);
	dev->nondasd_support = 0;
	dev->raid_scsi_mode = 0;
	if(dev->adapter_info.options & AAC_OPT_NONDASD)
		dev->nondasd_support = 1;

	/*
	 * If the firmware supports ROMB RAID/SCSI mode and we are currently
	 * in RAID/SCSI mode, set the flag. For now if in this mode we will
	 * force nondasd support on. If we decide to allow the non-dasd flag
	 * additional changes changes will have to be made to support
	 * RAID/SCSI.  the function aac_scsi_cmd in this module will have to be
	 * changed to support the new dev->raid_scsi_mode flag instead of
	 * leaching off of the dev->nondasd_support flag. Also in linit.c the
	 * function aac_detect will have to be modified where it sets up the
	 * max number of channels based on the aac->nondasd_support flag only.
	 */
	if ((dev->adapter_info.options & AAC_OPT_SCSI_MANAGED) &&
	    (dev->adapter_info.options & AAC_OPT_RAID_SCSI_MODE)) {
		dev->nondasd_support = 1;
		dev->raid_scsi_mode = 1;
	}
	if (dev->raid_scsi_mode != 0)
		printk(KERN_INFO "%s%d: ROMB RAID/SCSI mode enabled\n",
				dev->name, dev->id);
	if (nondasd != -1)
		dev->nondasd_support = (nondasd!=0);
	if (dev->nondasd_support && !dev->in_reset)
#if (0 && defined(BOOTCD))
	{
#endif
		printk(KERN_INFO "%s%d: Non-DASD support enabled.\n",dev->name, dev->id);
#if (0 && defined(BOOTCD))
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "%s%d: Non-DASD support enabled.",dev->name, dev->id));
	}
#endif

#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)) && !defined(__VMKLNX__))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
	if (dma_get_required_mask(&dev->pdev->dev) > DMA_32BIT_MASK)
#else
	if (dma_get_required_mask(&dev->pdev->dev) > DMA_BIT_MASK(32))
#endif
#else
	if (num_physpages > (0xFFFFFFFFULL >> PAGE_SHIFT))
#endif
		dev->needs_dac = 1;
	dev->dac_support = 0;
#if (defined(CONFIG_COMMUNITY_KERNEL))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7))
	if ((sizeof(dma_addr_t) > 4) && dev->needs_dac &&
	    (dev->adapter_info.options & AAC_OPT_SGMAP_HOST64)) {
#else
	if( (sizeof(dma_addr_t) > 4) && (dev->adapter_info.options & AAC_OPT_SGMAP_HOST64)){
#endif
		if (!dev->in_reset)
			printk(KERN_INFO "%s%d: 64bit support enabled.\n",
				dev->name, dev->id);
#else
	/*
	 *	Only enable DAC mode if the dma_addr_t is larger than 32
	 * bit addressing, and we have more than 32 bit addressing worth of
	 * memory and if the controller supports 64 bit scatter gather elements.
	 */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7))
	if ((sizeof(dma_addr_t) > 4) && dev->needs_dac &&
	    (dev->adapter_info.options & AAC_OPT_SGMAP_HOST64)) {
#else
	if( (sizeof(dma_addr_t) > 4) && (num_physpages > (0xFFFFFFFFULL >> PAGE_SHIFT)) && (dev->adapter_info.options & AAC_OPT_SGMAP_HOST64)){
#endif
#endif
		dev->dac_support = 1;
	}

	if(dacmode != -1) {
		dev->dac_support = (dacmode!=0);
	}

	/* avoid problems with AAC_QUIRK_SCSI_32 controllers */
	if (dev->dac_support &&	(aac_get_driver_ident(dev->cardtype)->quirks
		& AAC_QUIRK_SCSI_32)) {
		dev->nondasd_support = 0;
		dev->jbod = 0;
		expose_physicals = 0;
	}

	if(dev->dac_support != 0) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
		if (!pci_set_dma_mask(dev->pdev, DMA_64BIT_MASK) &&
			!pci_set_consistent_dma_mask(dev->pdev, DMA_64BIT_MASK)) {
#else
		if (!pci_set_dma_mask(dev->pdev, DMA_BIT_MASK(64)) &&
			!pci_set_consistent_dma_mask(dev->pdev, DMA_BIT_MASK(64))) {
#endif
			if (!dev->in_reset)
				printk(KERN_INFO"%s%d: 64 Bit DAC enabled\n",
					dev->name, dev->id);
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "%s%d: 64 Bit DAC enabled",
			  dev->name, dev->id));
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
		} else if (!pci_set_dma_mask(dev->pdev, DMA_32BIT_MASK) &&
			!pci_set_consistent_dma_mask(dev->pdev, DMA_32BIT_MASK)) {
#else
		} else if (!pci_set_dma_mask(dev->pdev, DMA_BIT_MASK(32)) &&
			!pci_set_consistent_dma_mask(dev->pdev, DMA_BIT_MASK(32))) {
#endif
			printk(KERN_INFO"%s%d: DMA mask set failed, 64 Bit DAC disabled\n",
				dev->name, dev->id);
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "%s%d: DMA mask set failed, 64 Bit DAC disabled",
			  dev->name, dev->id));
#endif
			dev->dac_support = 0;
		} else {
			printk(KERN_WARNING"%s%d: No suitable DMA available.\n",
				dev->name, dev->id);
#if (0 && defined(BOOTCD))
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "%s%d: No suitable DMA available.",
			  dev->name, dev->id));
#endif
			rcode = -ENOMEM;
		}
	}
	/*
	 * Deal with configuring for the individualized limits of each packet
	 * interface.
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)) && (!defined(__arm__)) && defined(CONFIG_HIGHMEM) && ((LINUX_VERSION_CODE != KERNEL_VERSION(2,4,19)) || defined(CONFIG_HIGHIO))
	dev->scsi_host_ptr->highmem_io = 1;
#endif
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	// WRONG PLACE FOR THIS CHECK TODAY? BUT WE WILL SURVIVE. MGS
	if (dev->comm_interface == AAC_COMM_APRE)
		dev->a_ops.adapter_scsi_cmd = aac_scsi_cmd_apre;
	else {
		dev->a_ops.adapter_scsi_cmd = aac_scsi_cmd;
#endif
	dev->a_ops.adapter_scsi = (dev->dac_support)
	  ? ((aac_get_driver_ident(dev->cardtype)->quirks & AAC_QUIRK_SCSI_32)
				? aac_scsi_32_64
				: aac_scsi_64)
				: aac_scsi_32;
	if (dev->raw_io_interface) {
		dev->a_ops.adapter_bounds = (dev->raw_io_64)
					? aac_bounds_64
					: aac_bounds_32;
		dev->a_ops.adapter_read = aac_read_raw_io;
		dev->a_ops.adapter_write = aac_write_raw_io;
	} else {
		dev->a_ops.adapter_bounds = aac_bounds_32;
		dev->scsi_host_ptr->sg_tablesize = (dev->max_fib_size -
			sizeof(struct aac_fibhdr) -
			sizeof(struct aac_write) + sizeof(struct sgentry)) /
				sizeof(struct sgentry);
		if (dev->dac_support) {
			dev->a_ops.adapter_read = aac_read_block64;
			dev->a_ops.adapter_write = aac_write_block64;
			/*
			 * 38 scatter gather elements
			 */
			dev->scsi_host_ptr->sg_tablesize =
				(dev->max_fib_size -
				sizeof(struct aac_fibhdr) -
				sizeof(struct aac_write64) +
				sizeof(struct sgentry64)) /
					sizeof(struct sgentry64);
		} else {
			dev->a_ops.adapter_read = aac_read_block;
			dev->a_ops.adapter_write = aac_write_block;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)) && (!defined(__arm__)) && defined(CONFIG_HIGHMEM) && ((LINUX_VERSION_CODE != KERNEL_VERSION(2,4,19)) || defined(CONFIG_HIGHIO))
			dev->scsi_host_ptr->highmem_io = 0;
#endif
		}
		dev->scsi_host_ptr->max_sectors = AAC_MAX_32BIT_SGBCOUNT;
		if (!(dev->adapter_info.options & AAC_OPT_NEW_COMM)) {
			/*
			 * Worst case size that could cause sg overflow when
			 * we break up SG elements that are larger than 64KB.
			 * Would be nice if we could tell the SCSI layer what
			 * the maximum SG element size can be. Worst case is
			 * (sg_tablesize-1) 4KB elements with one 64KB
			 * element.
			 *	32bit -> 468 or 238KB	64bit -> 424 or 212KB
			 */
			dev->scsi_host_ptr->max_sectors =
			  (dev->scsi_host_ptr->sg_tablesize * 8) + 112;
		}
	}
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	}
#endif

	if (!dev->sync_mode && dev->sa_firmware &&
		dev->scsi_host_ptr->sg_tablesize > HBA_MAX_SG_SEPARATE)
		dev->scsi_host_ptr->sg_tablesize = dev->sg_tablesize =
			HBA_MAX_SG_SEPARATE;

	/* FIB should be freed only after getting the response from the F/W */
	if (rcode != -ERESTARTSYS) {
		aac_fib_complete(fibptr);
		aac_fib_free(fibptr);
	}

	return rcode;
}


static void io_callback(void *context, struct fib * fibptr)
{
	struct aac_dev *dev;
	struct aac_read_reply *readreply;
	struct scsi_cmnd *scsicmd;
	u32 cid;

	scsicmd = (struct scsi_cmnd *) context;

	if (!aac_valid_context(scsicmd, fibptr))
		return;

	dev = fibptr->dev;
	cid = scmd_id(scsicmd);

	if (nblank(dprintk(x))) {
		u64 lba;
		switch (scsicmd->cmnd[0]) {
		case WRITE_6:
		case READ_6:
			lba = ((scsicmd->cmnd[1] & 0x1F) << 16) |
			    (scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
			break;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(WRITE_16))
		case WRITE_16:
		case READ_16:
			lba = ((u64)scsicmd->cmnd[2] << 56) |
			      ((u64)scsicmd->cmnd[3] << 48) |
			      ((u64)scsicmd->cmnd[4] << 40) |
			      ((u64)scsicmd->cmnd[5] << 32) |
			      ((u64)scsicmd->cmnd[6] << 24) |
			      (scsicmd->cmnd[7] << 16) |
			      (scsicmd->cmnd[8] << 8) | scsicmd->cmnd[9];
			break;
#endif
		case WRITE_12:
		case READ_12:
			lba = ((u64)scsicmd->cmnd[2] << 24) |
			      (scsicmd->cmnd[3] << 16) |
			      (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
			break;
		default:
			lba = ((u64)scsicmd->cmnd[2] << 24) |
			       (scsicmd->cmnd[3] << 16) |
			       (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
			break;
		}
		printk(KERN_DEBUG
		  "io_callback[cpu %d]: lba = %llu, t = %ld.\n",
		  smp_processor_id(), (unsigned long long)lba, jiffies);
	}

	BUG_ON(fibptr == NULL);

#if (!defined(__VMKLNX30__) || defined(__x86_64__))
	scsi_dma_unmap(scsicmd);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	if(!scsi_sg_count(scsicmd) && scsi_bufflen(scsicmd))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		pci_unmap_single(dev->pdev, (dma_addr_t)scsicmd->SCp.dma_handle,
#else
		pci_unmap_single(dev->pdev, scsicmd->SCp.dma_handle,
#endif
				 scsicmd->request_bufflen,
				 scsicmd->sc_data_direction);
#endif

#endif
	readreply = (struct aac_read_reply *)fib_data(fibptr);
	switch (le32_to_cpu(readreply->status)) {
	case ST_OK:
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
			SAM_STAT_GOOD;
		dev->fsa_dev[cid].sense_data.sense_key = NO_SENSE;
		break;
	case ST_NOT_READY:
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
			SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data, NOT_READY,
		  SENCODE_BECOMING_READY, ASENCODE_BECOMING_READY, 0, 0);
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
		break;
	case ST_MEDERR:
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
			SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data, MEDIUM_ERROR,
		  SENCODE_UNRECOVERED_READ_ERROR, ASENCODE_NO_SENSE, 0, 0);
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
		break;
	default:
#ifdef AAC_DETAILED_STATUS_INFO
		printk(KERN_WARNING "io_callback: io failed, status = %d\n",
		  le32_to_cpu(readreply->status));
#endif
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
			SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data,
		  HARDWARE_ERROR, SENCODE_INTERNAL_TARGET_FAILURE,
		  ASENCODE_INTERNAL_TARGET_FAILURE, 0, 0);
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
		break;
	}
	aac_fib_complete(fibptr);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	aac_fib_free_tag(fibptr);
#else
	aac_fib_free(fibptr);
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	if (scsicmd->device->device_blocked) {
		struct scsi_cmnd * cmd;
		cid = 0;

		for (cmd = scsicmd->device->device_queue; cmd; cmd = cmd->next)
			if (cmd->serial_number)
				++cid;
		if (cid < scsicmd->device->queue_depth)
			scsicmd->device->device_blocked = 0;
	}
#endif
}
#if (!defined(CONFIG_COMMUNITY_KERNEL))

static inline void aac_select_queue_depth(
	struct scsi_cmnd * scsicmd,
	u64 lba,
	u32 count)
{
	struct scsi_device *device = scsicmd->device;
	struct aac_dev *dev;
	unsigned depth;
	int cid;

#if (1 || defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	/*-
		this routine is only used to try to back up
		sequential IO into linux's scsi_merge layer
		in an attempt to coalesce them.  vmkernel
		doesn't do this, and in fact changing the
		queue_depth like this seems to tickle a bug
		in our scsi layer where we don't reschedule
		the IOs in a timely fashion.
			-gmccready@vmware.com
	 */
	return;
#endif
	if (coalescethreshold == 0)
		return;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if (!device->tagged_supported)
		return;
#endif
	if (
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)) || defined(SCSI_HAS_SHOST_STATE_ENUM))
	  SHOST_RECOVERY == device->host->shost_state
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
	  test_bit(SHOST_RECOVERY, &device->host->shost_state)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	  test_bit(SHOST_RECOVERY, &device->host->shost_state) ||
	  device->host->eh_active
#else
	  device->host->in_recovery || device->host->eh_active
#endif
	)
		return;
	dev = (struct aac_dev *)device->host->hostdata;
	cid = scmd_id(scsicmd);
	if (dev->fsa_dev[cid].queue_depth <= 2)
		dev->fsa_dev[cid].queue_depth = device->queue_depth;
	if (lba == dev->fsa_dev[cid].last) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		struct scsi_cmnd * cmd;
#endif
		/*
		 * If larger than coalescethreshold in size, coalescing has
		 * less effect on overall performance.  Also, if we are
		 * coalescing right now, leave it alone if above the threshold.
		 */
		if (count > coalescethreshold)
			return;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		depth = 0;

		for (cmd = device->device_queue; cmd; cmd = cmd->next)
			if ((cmd->serial_number)
			 && (cmd != scsicmd)
			 && (++depth > 1)) {
				device->device_blocked = 1;
				break;
			}
#endif
		depth = 2;
	} else {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		device->device_blocked = 0;
#endif
		depth = dev->fsa_dev[cid].queue_depth;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
	scsi_adjust_queue_depth(device, MSG_ORDERED_TAG, depth);
#else
	scsi_change_queue_depth(device, depth);
#endif
#else
	device->queue_depth = depth;
#endif
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	dprintk((KERN_DEBUG "l=%llu %llu[%u] q=%u %lu\n",
	  dev->fsa_dev[cid].last, lba, count, device->queue_depth,
	  (unsigned long)(atomic_read(&dev->queues->queue[
	    (dev->comm_interface == AAC_COMM_APRE)
	      ? ApreCmdQueue
	      : AdapNormCmdQueue
	  ].numpending))));
#else
	dprintk((KERN_DEBUG "l=%llu %llu[%u] q=%u %lu\n",
	  dev->fsa_dev[cid].last, lba, count, device->queue_depth,
	  (unsigned long)(atomic_read(&dev->queues->queue[AdapNormCmdQueue].numpending))));
#endif
	dev->fsa_dev[cid].last = lba + count;
}
#endif

static int aac_read(struct scsi_cmnd * scsicmd)
{
	u64 lba;
	u32 count;
	int status;
	struct aac_dev *dev;
	struct fib * cmd_fibcontext;

	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
	/*
	 *	Get block address and transfer length
	 */
#if (defined(AAC_DEBUG_INSTRUMENT_IO))
	printk(KERN_DEBUG "aac_read: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  scsicmd->cmnd[0],  scsicmd->cmnd[1],  scsicmd->cmnd[2],
	  scsicmd->cmnd[3],  scsicmd->cmnd[4],  scsicmd->cmnd[5],
	  scsicmd->cmnd[6],  scsicmd->cmnd[7],  scsicmd->cmnd[8],
	  scsicmd->cmnd[9],  scsicmd->cmnd[10], scsicmd->cmnd[11],
	  scsicmd->cmnd[12], scsicmd->cmnd[13], scsicmd->cmnd[14],
	  scsicmd->cmnd[15]);
#endif
	switch (scsicmd->cmnd[0]) {
	case READ_6:
		dprintk((KERN_DEBUG "aachba: received a read(6) command on id %d.\n", scmd_id(scsicmd)));

		lba = ((scsicmd->cmnd[1] & 0x1F) << 16) |
			(scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
		count = scsicmd->cmnd[4];

		if (count == 0)
			count = 256;
		break;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(READ_16))
	case READ_16:
		dprintk((KERN_DEBUG "aachba: received a read(16) command on id %d.\n", scmd_id(scsicmd)));

		lba =	((u64)scsicmd->cmnd[2] << 56) |
			((u64)scsicmd->cmnd[3] << 48) |
			((u64)scsicmd->cmnd[4] << 40) |
			((u64)scsicmd->cmnd[5] << 32) |
			((u64)scsicmd->cmnd[6] << 24) |
			(scsicmd->cmnd[7] << 16) |
			(scsicmd->cmnd[8] << 8) | scsicmd->cmnd[9];
		count = (scsicmd->cmnd[10] << 24) |
			(scsicmd->cmnd[11] << 16) |
			(scsicmd->cmnd[12] << 8) | scsicmd->cmnd[13];
		break;
#endif
	case READ_12:
		dprintk((KERN_DEBUG "aachba: received a read(12) command on id %d.\n", scmd_id(scsicmd)));

		lba = ((u64)scsicmd->cmnd[2] << 24) |
			(scsicmd->cmnd[3] << 16) |
			(scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
		count = (scsicmd->cmnd[6] << 24) |
			(scsicmd->cmnd[7] << 16) |
			(scsicmd->cmnd[8] << 8) | scsicmd->cmnd[9];
		break;
	default:
		dprintk((KERN_DEBUG "aachba: received a read(10) command on id %d.\n", scmd_id(scsicmd)));

		lba = ((u64)scsicmd->cmnd[2] << 24) |
			(scsicmd->cmnd[3] << 16) |
			(scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
		count = (scsicmd->cmnd[7] << 8) | scsicmd->cmnd[8];
		break;
	}
/* ADPml11898 SUNMR Spitfire issue
 * FW layer exposes lesser container capacity than the actual one
 * It exposes [Actaul size - Spitfire space(10MB)] to the OS, IO's to the 10MB should be prohibhited from the Linux driver
 * Sensekey sets to HARDWARE_ERROR and sending the notification to the MID layer
 */

if(expose_hidden_space <= 0) {
	if((lba + count) > (dev->fsa_dev[scmd_id(scsicmd)].size)) {
		int cid = scmd_id(scsicmd);
		dprintk((KERN_DEBUG "aacraid: Illegal lba\n"));
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
			SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data,
		  HARDWARE_ERROR, SENCODE_INTERNAL_TARGET_FAILURE,
		  ASENCODE_INTERNAL_TARGET_FAILURE, 0, 0);
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		__aac_io_done(scsicmd);
#else
		scsicmd->scsi_done(scsicmd);
#endif
		return 1;
	}
}

	dprintk((KERN_DEBUG "aac_read[cpu %d]: lba = %llu, t = %ld.\n",
	  smp_processor_id(), (unsigned long long)lba, jiffies));
	if (aac_adapter_bounds(dev,scsicmd,lba))
		return 0;
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	/*
	 *	Are we in a sequential mode?
	 */
	aac_select_queue_depth(scsicmd, lba, count);
#endif
	/*
	 *	Alocate and initialize a Fib
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (!(cmd_fibcontext = aac_fib_alloc_tag(dev, scsicmd))) {
#else
	if (!(cmd_fibcontext = aac_fib_alloc(dev))) {
#endif
		return -1;
	}

	status = aac_adapter_read(cmd_fibcontext, scsicmd, lba, count);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	printk(KERN_WARNING "aac_read: aac_fib_send failed with status: %d.\n", status);
	/*
	 *	For some reason, the Fib didn't queue, return QUEUE_FULL
	 */
	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_TASK_SET_FULL;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif
	aac_fib_complete(cmd_fibcontext);
	aac_fib_free(cmd_fibcontext);
	return 0;
}

static int aac_write(struct scsi_cmnd * scsicmd)
{
	u64 lba;
	u32 count;
	int fua;
	int status;
	struct aac_dev *dev;
	struct fib * cmd_fibcontext;

	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
	/*
	 *	Get block address and transfer length
	 */
#if (defined(AAC_DEBUG_INSTRUMENT_IO))
	printk(KERN_DEBUG "aac_write: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
	  scsicmd->cmnd[0],  scsicmd->cmnd[1],  scsicmd->cmnd[2],
	  scsicmd->cmnd[3],  scsicmd->cmnd[4],  scsicmd->cmnd[5],
	  scsicmd->cmnd[6],  scsicmd->cmnd[7],  scsicmd->cmnd[8],
	  scsicmd->cmnd[9],  scsicmd->cmnd[10], scsicmd->cmnd[11],
	  scsicmd->cmnd[12], scsicmd->cmnd[13], scsicmd->cmnd[14],
	  scsicmd->cmnd[15]);
#endif
	if (scsicmd->cmnd[0] == WRITE_6)	/* 6 byte command */
	{
		lba = ((scsicmd->cmnd[1] & 0x1F) << 16) | (scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
		count = scsicmd->cmnd[4];
		if (count == 0)
			count = 256;
		fua = 0;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(WRITE_16))
	} else if (scsicmd->cmnd[0] == WRITE_16) { /* 16 byte command */
		dprintk((KERN_DEBUG "aachba: received a write(16) command on id %d.\n", scmd_id(scsicmd)));

		lba =	((u64)scsicmd->cmnd[2] << 56) |
			((u64)scsicmd->cmnd[3] << 48) |
			((u64)scsicmd->cmnd[4] << 40) |
			((u64)scsicmd->cmnd[5] << 32) |
			((u64)scsicmd->cmnd[6] << 24) |
			(scsicmd->cmnd[7] << 16) |
			(scsicmd->cmnd[8] << 8) | scsicmd->cmnd[9];
		count = (scsicmd->cmnd[10] << 24) | (scsicmd->cmnd[11] << 16) |
			(scsicmd->cmnd[12] << 8) | scsicmd->cmnd[13];
		fua = scsicmd->cmnd[1] & 0x8;
#endif
	} else if (scsicmd->cmnd[0] == WRITE_12) { /* 12 byte command */
		dprintk((KERN_DEBUG "aachba: received a write(12) command on id %d.\n", scmd_id(scsicmd)));

		lba = ((u64)scsicmd->cmnd[2] << 24) | (scsicmd->cmnd[3] << 16)
		    | (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
		count = (scsicmd->cmnd[6] << 24) | (scsicmd->cmnd[7] << 16)
		      | (scsicmd->cmnd[8] << 8) | scsicmd->cmnd[9];
		fua = scsicmd->cmnd[1] & 0x8;
	} else {
		dprintk((KERN_DEBUG "aachba: received a write(10) command on id %d.\n", scmd_id(scsicmd)));
		lba = ((u64)scsicmd->cmnd[2] << 24) | (scsicmd->cmnd[3] << 16) | (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
		count = (scsicmd->cmnd[7] << 8) | scsicmd->cmnd[8];
		fua = scsicmd->cmnd[1] & 0x8;
	}

/* ADPml11898 SUNMR Spitfire issue
 * FW layer exposes lesser container capacity than the actual one
 * It exposes [Actaul size - Spitfire space(10MB)] to the OS, IO's to the 10MB should be prohibhited from the Linux driver
 * Sensekey sets to HARDWARE_ERROR and sending the notification to the MID layer
 */
if(expose_hidden_space <= 0) {
	if((lba + count) > (dev->fsa_dev[scmd_id(scsicmd)].size)) {
		int cid = scmd_id(scsicmd);
		dprintk((KERN_DEBUG "aacraid: Illegal lba\n"));
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
			SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data,
		  HARDWARE_ERROR, SENCODE_INTERNAL_TARGET_FAILURE,
		  ASENCODE_INTERNAL_TARGET_FAILURE, 0, 0);
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		__aac_io_done(scsicmd);
#else
		scsicmd->scsi_done(scsicmd);
#endif
		return 1;
	}
}
	dprintk((KERN_DEBUG "aac_write[cpu %d]: lba = %llu, t = %ld.\n",
	  smp_processor_id(), (unsigned long long)lba, jiffies));
	if (aac_adapter_bounds(dev,scsicmd,lba))
		return 0;
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	/*
	 *	Are we in a sequential mode?
	 */
	aac_select_queue_depth(scsicmd, lba, count);
#endif
	/*
	 *	Allocate and initialize a Fib then setup a BlockWrite command
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (!(cmd_fibcontext = aac_fib_alloc_tag(dev, scsicmd))) {
#else
	if (!(cmd_fibcontext = aac_fib_alloc(dev))) {
#endif
		return -1;
	}

	status = aac_adapter_write(cmd_fibcontext, scsicmd, lba, count, fua);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	printk(KERN_WARNING "aac_write: aac_fib_send failed with status: %d\n", status);
	/*
	 *	For some reason, the Fib didn't queue, return QUEUE_FULL
	 */
	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_TASK_SET_FULL;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif

	aac_fib_complete(cmd_fibcontext);
	aac_fib_free(cmd_fibcontext);
	return 0;
}

static void synchronize_callback(void *context, struct fib *fibptr)
{
	struct aac_synchronize_reply *synchronizereply;
	struct scsi_cmnd *cmd;

	cmd = context;

	if (!aac_valid_context(cmd, fibptr))
		return;

	dprintk((KERN_DEBUG "synchronize_callback[cpu %d]: t = %ld.\n",
				smp_processor_id(), jiffies));
	BUG_ON(fibptr == NULL);


	synchronizereply = fib_data(fibptr);
	if (le32_to_cpu(synchronizereply->status) == CT_OK)
		cmd->result = DID_OK << 16 |
			COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
	else {
		struct scsi_device *sdev = cmd->device;
		struct aac_dev *dev = fibptr->dev;
		u32 cid = sdev_id(sdev);
		printk(KERN_WARNING
		     "synchronize_callback: synchronize failed, status = %d\n",
		     le32_to_cpu(synchronizereply->status));
		cmd->result = DID_OK << 16 |
			COMMAND_COMPLETE << 8 | SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data,
		  HARDWARE_ERROR, SENCODE_INTERNAL_TARGET_FAILURE,
		  ASENCODE_INTERNAL_TARGET_FAILURE, 0, 0);
		memcpy(cmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
	}

	aac_fib_complete(fibptr);
	aac_fib_free(fibptr);
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(cmd);
#else
	cmd->scsi_done(cmd);
#endif
}

static int aac_synchronize(struct scsi_cmnd *scsicmd)
{
	int status;
	struct fib *cmd_fibcontext;
	struct aac_synchronize *synchronizecmd;
	struct scsi_cmnd *cmd;
	struct scsi_device *sdev = scsicmd->device;
	int active = 0;
	struct aac_dev *aac;
	u64 lba = ((u64)scsicmd->cmnd[2] << 24) | (scsicmd->cmnd[3] << 16) |
		(scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
	u32 count = (scsicmd->cmnd[7] << 8) | scsicmd->cmnd[8];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	unsigned long flags;

#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	printk(KERN_INFO "aac_synchronize(%p={.lba=%llu,.count=%lu})\n",
	    scsicmd, (unsigned long long)lba, (unsigned long)count);
#endif
	/*
	 * Wait for all outstanding queued commands to complete to this
	 * specific target (block).
	 */
	spin_lock_irqsave(&sdev->list_lock, flags);
	list_for_each_entry(cmd, &sdev->cmd_list, list)
#else
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	printk(KERN_INFO "aac_synchronize(%p={.lba=%llu,.count=%lu})\n",
	    scsicmd, (unsigned long long)lba, (unsigned long)count);
#endif
	for(cmd = sdev->device_queue; cmd; cmd = cmd->next)
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	{
		printk(KERN_INFO "%p={.SCp.phase=%x,.cmnd[0]=%u,",
		  cmd, (unsigned)cmd->SCp.phase, cmd->cmnd[0]);
#endif
		if (cmd->SCp.phase == AAC_OWNER_FIRMWARE) {
			u64 cmnd_lba;
			u32 cmnd_count;

			if (cmd->cmnd[0] == WRITE_6) {
				cmnd_lba = ((cmd->cmnd[1] & 0x1F) << 16) |
					(cmd->cmnd[2] << 8) |
					cmd->cmnd[3];
				cmnd_count = cmd->cmnd[4];
				if (cmnd_count == 0)
					cmnd_count = 256;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(WRITE_16))
			} else if (cmd->cmnd[0] == WRITE_16) {
				cmnd_lba = ((u64)cmd->cmnd[2] << 56) |
					((u64)cmd->cmnd[3] << 48) |
					((u64)cmd->cmnd[4] << 40) |
					((u64)cmd->cmnd[5] << 32) |
					((u64)cmd->cmnd[6] << 24) |
					(cmd->cmnd[7] << 16) |
					(cmd->cmnd[8] << 8) |
					cmd->cmnd[9];
				cmnd_count = (cmd->cmnd[10] << 24) |
					(cmd->cmnd[11] << 16) |
					(cmd->cmnd[12] << 8) |
					cmd->cmnd[13];
#endif
			} else if (cmd->cmnd[0] == WRITE_12) {
				cmnd_lba = ((u64)cmd->cmnd[2] << 24) |
					(cmd->cmnd[3] << 16) |
					(cmd->cmnd[4] << 8) |
					cmd->cmnd[5];
				cmnd_count = (cmd->cmnd[6] << 24) |
					(cmd->cmnd[7] << 16) |
					(cmd->cmnd[8] << 8) |
					cmd->cmnd[9];
			} else if (cmd->cmnd[0] == WRITE_10) {
				cmnd_lba = ((u64)cmd->cmnd[2] << 24) |
					(cmd->cmnd[3] << 16) |
					(cmd->cmnd[4] << 8) |
					cmd->cmnd[5];
				cmnd_count = (cmd->cmnd[7] << 8) |
					cmd->cmnd[8];
			} else
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
			{
				printk("}\n");
				continue;
			}
			printk(".lba=%llu,.count=%lu,",
			  (unsigned long long)cmnd_lba,
			  (unsigned long)cmnd_count);
#else
				continue;
#endif
			if (((cmnd_lba + cmnd_count) < lba) ||
			  (count && ((lba + count) < cmnd_lba)))
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
			{
				printk("}\n");
				continue;
			}
			printk(".active}\n");
#else
				continue;
#endif
			++active;
			break;
		}
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
		printk("}\n");
	}
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

	spin_unlock_irqrestore(&sdev->list_lock, flags);
#endif

	/*
	 *	Yield the processor (requeue for later)
	 */
	if (active)
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	{
		printk(KERN_INFO "aac_synchronize ACTIVE!\n");
		return SCSI_MLQUEUE_DEVICE_BUSY;
	}
#else
		return SCSI_MLQUEUE_DEVICE_BUSY;
#endif

	aac = (struct aac_dev *)sdev->host->hostdata;
	if (aac->in_reset)
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	{
		printk(KERN_INFO "aac_synchronize RESET!\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}
#else
		return SCSI_MLQUEUE_HOST_BUSY;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	printk(KERN_INFO "aac_synchronize START\n");
#elif (defined(AAC_DEBUG_INSTRUMENT_IO))
	printk(KERN_DEBUG "aac_synchronize[cpu %d]: t = %ld.\n",
	  smp_processor_id(), jiffies);
#endif
	/*
	 *	Allocate and initialize a Fib
	 */
	if (!(cmd_fibcontext = aac_fib_alloc(aac)))
#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	{
		printk(KERN_INFO "aac_synchronize ALLOC!\n");
		return SCSI_MLQUEUE_HOST_BUSY;
	}
#else
		return SCSI_MLQUEUE_HOST_BUSY;
#endif

	aac_fib_init(cmd_fibcontext);

	synchronizecmd = fib_data(cmd_fibcontext);
	synchronizecmd->command = cpu_to_le32(VM_ContainerConfig);
	synchronizecmd->type = cpu_to_le32(CT_FLUSH_CACHE);
	synchronizecmd->cid = cpu_to_le32(scmd_id(scsicmd));
	synchronizecmd->count =
	     cpu_to_le32(sizeof(((struct aac_synchronize_reply *)NULL)->data));

	/*
	 *	Now send the Fib to the adapter
	 */
	status = aac_fib_send(ContainerCommand,
		  cmd_fibcontext,
		  sizeof(struct aac_synchronize),
		  FsaNormal,
		  0, 1,
		  (fib_callback)synchronize_callback,
		  (void *)scsicmd);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	printk(KERN_WARNING
		"aac_synchronize: aac_fib_send failed with status: %d.\n", status);
	aac_fib_complete(cmd_fibcontext);
	aac_fib_free(cmd_fibcontext);
	return SCSI_MLQUEUE_HOST_BUSY;
}

static void aac_start_stop_callback(void *context, struct fib *fibptr)
{
	struct scsi_cmnd *scsicmd = context;

	if (!aac_valid_context(scsicmd, fibptr))
		return;

	BUG_ON(fibptr == NULL);

	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;

	aac_fib_complete(fibptr);
	aac_fib_free(fibptr);
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif
}

static int aac_start_stop(struct scsi_cmnd *scsicmd)
{
	int status;
	struct fib *cmd_fibcontext;
	struct aac_power_management *pmcmd;
	struct scsi_device *sdev = scsicmd->device;
	struct aac_dev *aac = (struct aac_dev *)sdev->host->hostdata;

	if (!(aac->supplement_adapter_info.SupportedOptions2 &
	      AAC_OPTION_POWER_MANAGEMENT)) {
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
				  SAM_STAT_GOOD;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		__aac_io_done(scsicmd);
#else
		scsicmd->scsi_done(scsicmd);
#endif
		return 0;
	}

	if (aac->in_reset)
		return SCSI_MLQUEUE_HOST_BUSY;

	/*
	 *	Allocate and initialize a Fib
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	cmd_fibcontext = aac_fib_alloc_tag(aac, scsicmd);
#else
	cmd_fibcontext = aac_fib_alloc(aac);
#endif
	if (!cmd_fibcontext)
		return SCSI_MLQUEUE_HOST_BUSY;

	aac_fib_init(cmd_fibcontext);

	pmcmd = fib_data(cmd_fibcontext);
	pmcmd->command = cpu_to_le32(VM_ContainerConfig);
	pmcmd->type = cpu_to_le32(CT_POWER_MANAGEMENT);
	/* Eject bit ignored, not relevant */
	pmcmd->sub = (scsicmd->cmnd[4] & 1) ?
		cpu_to_le32(CT_PM_START_UNIT) : cpu_to_le32(CT_PM_STOP_UNIT);
	pmcmd->cid = cpu_to_le32(sdev_id(sdev));
	pmcmd->parm = (scsicmd->cmnd[1] & 1) ?
		cpu_to_le32(CT_PM_UNIT_IMMEDIATE) : 0;

	/*
	 *	Now send the Fib to the adapter
	 */
	status = aac_fib_send(ContainerCommand,
		  cmd_fibcontext,
		  sizeof(struct aac_power_management),
		  FsaNormal,
		  0, 1,
		  (fib_callback)aac_start_stop_callback,
		  (void *)scsicmd);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	aac_fib_complete(cmd_fibcontext);
	aac_fib_free(cmd_fibcontext);
	return SCSI_MLQUEUE_HOST_BUSY;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))

static inline void get_sd_devname(int disknum, char *buffer)
{
	if (disknum < 0) {
		buffer[0] = '\0';
		return;
	}

	buffer[0] = 's';
	buffer[1] = 'd';
	if (disknum < 26) {
		buffer[2] = 'a' + disknum;
		buffer[3] = '\0';
	} else {
		/*
		 * For larger numbers of disks, we need to go to a new
		 * naming scheme.
		 */
		buffer[2] = 'a' - 1 + (disknum / 26);
		buffer[3] = 'a' + (disknum % 26);
		buffer[4] = '\0';
	}
}

# define strlcpy(s1,s2,n) strncpy(s1,s2,n);s1[n-1]='\0'
#endif

/**
 *	aac_scsi_cmd()		-	Process SCSI command
 *	@scsicmd:		SCSI command block
 *
 *	Emulate a SCSI command and queue the required request for the
 *	aacraid firmware.
 */

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
static
#endif
int aac_scsi_cmd(struct scsi_cmnd * scsicmd)
{
	u32 cid, bus;
	struct Scsi_Host *host = scsicmd->device->host;
	struct aac_dev *dev = (struct aac_dev *)host->hostdata;
	struct fsa_dev_info *fsa_dev_ptr = dev->fsa_dev;

	if (fsa_dev_ptr == NULL)
		return -1;
#if (defined(AAC_DEBUG_INSTRUMENT_2TB))
		printk(KERN_NOTICE "scsicmd->cmnd={%02x %02x %02x %02x %02x "
		  "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x}\n",
		  scsicmd->cmnd[0], scsicmd->cmnd[1], scsicmd->cmnd[2],
		  scsicmd->cmnd[3], scsicmd->cmnd[4], scsicmd->cmnd[5],
		  scsicmd->cmnd[6], scsicmd->cmnd[7], scsicmd->cmnd[8],
		  scsicmd->cmnd[9], scsicmd->cmnd[10], scsicmd->cmnd[11],
		  scsicmd->cmnd[12], scsicmd->cmnd[13], scsicmd->cmnd[14],
		  scsicmd->cmnd[15]);
#endif
	/*
	 *	If the bus, id or lun is out of range, return fail
	 *	Test does not apply to ID 16, the pseudo id for the controller
	 *	itself.
	 */
	cid = scmd_id(scsicmd);
	if (cid != host->this_id) {
		if (scmd_channel(scsicmd) == CONTAINER_CHANNEL) {
			if((cid >= dev->maximum_num_containers) ||
					(scsicmd->device->lun != 0)) {
#if (defined(AAC_DEBUG_INSTRUMENT_2TB))
					printk(KERN_INFO
					  "scsicmd(0:%d:%d:0) No Connect\n",
					  scmd_channel(scsicmd), cid);
#endif
				scsicmd->result = DID_NO_CONNECT << 16;
				goto scsi_done_ret;
			}

			/*
			 *	If the target container doesn't exist, it may have
			 *	been newly created
			 */
			if (((fsa_dev_ptr[cid].valid & 1) == 0) ||
			  (fsa_dev_ptr[cid].sense_data.sense_key ==
			   NOT_READY)) {
				switch (scsicmd->cmnd[0]) {
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(SERVICE_ACTION_IN))
				case SERVICE_ACTION_IN:
					if (!(dev->raw_io_interface) ||
					    !(dev->raw_io_64) ||
					    ((scsicmd->cmnd[1] & 0x1f) != SAI_READ_CAPACITY_16))
						break;
#endif
				case INQUIRY:
				case READ_CAPACITY:
				case TEST_UNIT_READY:
					if (dev->in_reset)
						return -1;
					return _aac_probe_container(scsicmd, 
							aac_probe_container_callback2);
				default:
					break;
				}
			}
		} else {  /* check for physical non-dasd devices */
#if (defined(AAC_DEBUG_INSTRUMENT_2TB))
				printk(KERN_INFO "scsicmd(0:%d:%d:0) Phys\n",
				  scmd_channel(scsicmd), cid);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))
			/* ADPml05517 */
			/*
			 * If this is a test unit ready and there is already
			 * a long command outstanding, we will assume a
			 * sequentially queued device and report back that
			 * this needs a retry.
			 */
			if (scsicmd->cmnd[0] == TEST_UNIT_READY) {
				struct scsi_cmnd * command;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
				unsigned long flags;
				spin_lock_irqsave(&scsicmd->device->list_lock,
				  flags);
				list_for_each_entry(command,
				  &scsicmd->device->cmd_list, list)
#else
				for(command = scsicmd->device->device_queue;
				  command; command = command->next)
#endif
				{
					if (command == scsicmd)
						continue;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12))
					if ((command->state == SCSI_STATE_FINISHED)
					 || (command->state == 0))
						continue;
#endif
#if (1 || (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18)))
					if (command->timeout_per_command
					  <= scsicmd->timeout_per_command)
						continue;
#else
					if (command->request->timeout
					  <= scsicmd->request->timeout)
						continue;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
					spin_unlock_irqrestore(
					  &scsicmd->device->list_lock,
					  flags);
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) || defined(DID_BUS_BUSY))
					scsicmd->result = DID_BUS_BUSY << 16 |
						COMMAND_COMPLETE << 8;
#else
					scsicmd->result = DID_OK << 16
					  | COMMAND_COMPLETE << 8
					  | SAM_STAT_CHECK_CONDITION;
					set_sense(
					  &dev->fsa_dev[cid].sense_data,
					  ABORTED_COMMAND, 0, 0, 0, 0);
					memcpy(scsicmd->sense_buffer,
					  &dev->fsa_dev[cid].sense_data,
					  min_t(size_t, sizeof(
						dev->fsa_dev[cid].sense_data),
						SCSI_SENSE_BUFFERSIZE));
#endif
					goto scsi_done_ret;
				}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
				spin_unlock_irqrestore(
				  &scsicmd->device->list_lock,
				  flags);
#endif
			}
#endif
			bus = aac_logical_to_phys(scmd_channel(scsicmd));
			if( bus < AAC_MAX_BUSES && cid < AAC_MAX_TARGETS &&
				 ( dev->hba_map[bus][cid].expose == AAC_HIDE_DISK)){
				if( scsicmd->cmnd[0] == INQUIRY){
					scsicmd->result = DID_NO_CONNECT << 16;
					goto scsi_done_ret;
				}
			}

			if (bus < AAC_MAX_BUSES && cid < AAC_MAX_TARGETS &&
				dev->hba_map[bus][cid].devtype == AAC_DEVTYPE_NATIVE_RAW) {
				if (dev->in_reset)
					return -1;
				return aac_send_hba_fib(scsicmd);
			} else if (dev->nondasd_support || expose_physicals ||
				dev->jbod) {
#if (!defined(CONFIG_COMMUNITY_KERNEL))
				/*
				 *	Read and Write protect the exposed
				 * physical devices.
				 */
				if (scsicmd->device->no_uld_attach)
				switch (scsicmd->cmnd[0]) {
				/* Filter Format? SMART Verify/Fix? */
				case MODE_SELECT:
				case MODE_SELECT_10:
				case LOG_SELECT:
				case WRITE_LONG:
				case WRITE_SAME:
				case WRITE_VERIFY:
				case WRITE_VERIFY_12:
				case WRITE_6:
				case READ_6:
				case WRITE_10:
				case READ_10:
				case WRITE_12:
				case READ_12:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(WRITE_16))
				case WRITE_16:
				case READ_16:
#endif
					scsicmd->result = DID_OK << 16
					  | COMMAND_COMPLETE << 8
					  | SAM_STAT_CHECK_CONDITION;
					set_sense(
					  &dev->fsa_dev[cid].sense_data,
					  DATA_PROTECT, SENCODE_DATA_PROTECT,
					  ASENCODE_END_OF_DATA, 0, 0);
					memcpy(scsicmd->sense_buffer,
					  &dev->fsa_dev[cid].sense_data,
					  min_t(size_t, sizeof(
						dev->fsa_dev[cid].sense_data),
						SCSI_SENSE_BUFFERSIZE));
					goto scsi_done_ret;
				}
#endif
				if (dev->in_reset)
					return -1;
				return aac_send_srb_fib(scsicmd);
			} else {
				scsicmd->result = DID_NO_CONNECT << 16;
				goto scsi_done_ret;
			}
		}
	}
	/*
	 * else Command for the controller itself
	 */
	else if ((scsicmd->cmnd[0] != INQUIRY) &&	/* only INQUIRY & TUR cmnd supported for controller */
		(scsicmd->cmnd[0] != TEST_UNIT_READY))
	{
		dprintk((KERN_WARNING "Only INQUIRY & TUR command supported for controller, rcvd = 0x%x.\n", scsicmd->cmnd[0]));
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data,
		  ILLEGAL_REQUEST, SENCODE_INVALID_COMMAND,
		  ASENCODE_INVALID_COMMAND, 0, 0);
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
		       min_t(size_t, sizeof(dev->fsa_dev[cid].sense_data),
			     SCSI_SENSE_BUFFERSIZE));
		goto scsi_done_ret;
	}

	switch (scsicmd->cmnd[0]) {
	case READ_6:
	case READ_10:
	case READ_12:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(READ_16))
	case READ_16:
#endif
		if (dev->in_reset)
			return -1;
		/*
		 *	Hack to keep track of ordinal number of the device that
		 *	corresponds to a container. Needed to convert
		 *	containers to /dev/sd device names
		 */
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(AAC_EXTENDED_TIMEOUT))
		if ((scsicmd->eh_state != SCSI_STATE_QUEUED)
			 && (extendedtimeout > 0)) {
				mod_timer(&scsicmd->eh_timeout, jiffies + (extendedtimeout * HZ));
		}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#if (defined(__VMKLNX30__) || defined(__VMKLNX__))
		/* This code is commented out because such a mapping
		* doesn't seem to exist in vmklinux.
		*
		* This information is only used when a QUERY_DISK IOCTL
		* comes down and as far as I can tell there is no
		* equivalent check in vmklinux.
		*/
#endif
#if (!defined(__VMKLNX30__) && !defined(__VMKLNX__))
		if(fsa_dev_ptr[cid].devname[0]=='\0') {
			adbg_ioctl(dev,KERN_INFO,
				"rq_disk=%p disk_name=\"%s\"\n",
				scsicmd->request->rq_disk,
				scsicmd->request->rq_disk
				? scsicmd->request->rq_disk->disk_name
				: "Aiiiii");
		}
		if (scsicmd->request->rq_disk)
			strlcpy(fsa_dev_ptr[cid].devname,
			scsicmd->request->rq_disk->disk_name,
			min(sizeof(fsa_dev_ptr[cid].devname),
			sizeof(scsicmd->request->rq_disk->disk_name) + 1));
#endif
#else
		get_sd_devname(DEVICE_NR(scsicmd->request.rq_dev), fsa_dev_ptr[cid].devname);
#endif
		return aac_read(scsicmd);

	case WRITE_6:
	case WRITE_10:
	case WRITE_12:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(WRITE_16))
	case WRITE_16:
#endif
		if (dev->in_reset)
			return -1;
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && defined(AAC_EXTENDED_TIMEOUT))
		if ((scsicmd->eh_state != SCSI_STATE_QUEUED)
			&& (extendedtimeout > 0)) {
			mod_timer(&scsicmd->eh_timeout, jiffies + (extendedtimeout * HZ));
		}
#endif
		return aac_write(scsicmd);

	case SYNCHRONIZE_CACHE:
		if (((aac_cache & 6) == 6) && dev->cache_protected) {
			scsicmd->result = DID_OK << 16 |
				COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
			goto scsi_done_ret;
		}
		/* Issue FIB to tell Firmware to flush it's cache */
		if ((aac_cache & 6) != 2)
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
			return aac_adapter_synchronize(dev, scsicmd);
#else
			return aac_synchronize(scsicmd);
#endif

	case INQUIRY:
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
		return aac_adapter_inquiry(dev, scsicmd);
#else
	{
		struct inquiry_data inq_data;

		dprintk((KERN_DEBUG "INQUIRY command, ID: %d.\n", cid));
		memset(&inq_data, 0, sizeof (struct inquiry_data));

		if ((scsicmd->cmnd[1] & 0x1) && aac_wwn) {
			char *arr = (char *)&inq_data;

			/* EVPD bit set */
			arr[0] = (scmd_id(scsicmd) == host->this_id) ?
			  INQD_PDT_PROC : INQD_PDT_DA;
			if (scsicmd->cmnd[2] == 0) {
				/* supported vital product data pages */
/* Excluding SUSE as it has issues when inbox driver does not have this support but outbox has it.
  Because SUSE uses /dev/disk/by-id mapping entries in the OS grub config and VPD 0X83 creates conflicts */
#if (!defined(CONFIG_SUSE_KERNEL))
				arr[3] = 3;
				arr[6] = 0x83;
#else
				arr[3] = 2;
#endif
				arr[4] = 0x0;
				arr[5] = 0x80;
				arr[1] = scsicmd->cmnd[2];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
				aac_internal_transfer(scsicmd, &inq_data, 0,
				  sizeof(inq_data));
#else
				scsi_sg_copy_from_buffer(scsicmd, &inq_data,
							 sizeof(inq_data));
#endif
				scsicmd->result = DID_OK << 16 |
				  COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
			} else if (scsicmd->cmnd[2] == 0x80) {
				/* unit serial number page */
				arr[3] = setinqserial(dev, &arr[4],
				  scmd_id(scsicmd));
				arr[1] = scsicmd->cmnd[2];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
				aac_internal_transfer(scsicmd, &inq_data, 0,
				  sizeof(inq_data));
#else
				scsi_sg_copy_from_buffer(scsicmd, &inq_data,
							 sizeof(inq_data));
#endif
				if (aac_wwn != 2)
					return aac_get_container_serial(
						scsicmd);
				/* SLES 10 SP1 special */
				scsicmd->result = DID_OK << 16 |
				  COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
/* Excluding SUSE as it has issues when inbox driver does not have this support but outbox has it.
  Because SUSE uses /dev/disk/by-id mapping entries in the OS grub config and VPD 0X83 creates conflicts */
#if (!defined(CONFIG_SUSE_KERNEL))
			} else if (scsicmd->cmnd[2] == 0x83) {
				/* vpd page 0x83 - Device Identification Page */
				char *sno = (char *)&inq_data;

				sno[3] = setinqserial(dev, &sno[4],
				  scmd_id(scsicmd));

				if (aac_wwn != 2)
					return aac_get_container_serial(
						scsicmd);

				scsicmd->result = DID_OK << 16 |
				  COMMAND_COMPLETE << 8 | SAM_STAT_CHECK_CONDITION;
#endif
			} else {
				/* vpd page not implemented */
				scsicmd->result = DID_OK << 16 |
				  COMMAND_COMPLETE << 8 |
				  SAM_STAT_CHECK_CONDITION;
				set_sense(&dev->fsa_dev[cid].sense_data,
				  ILLEGAL_REQUEST, SENCODE_INVALID_CDB_FIELD,
				  ASENCODE_NO_SENSE, 7, 2);
				memcpy(scsicmd->sense_buffer,
				  &dev->fsa_dev[cid].sense_data,
				  min_t(size_t,
					sizeof(dev->fsa_dev[cid].sense_data),
					SCSI_SENSE_BUFFERSIZE));
			}
			scsicmd->scsi_done(scsicmd);
			return 0;
		}
		inq_data.inqd_ver = 2;	/* claim compliance to SCSI-2 */
		inq_data.inqd_rdf = 2;	/* A response data format value of two indicates that the data shall be in the format specified in SCSI-2 */
		inq_data.inqd_len = 31;
		/*Format for "pad2" is  RelAdr | WBus32 | WBus16 |  Sync  | Linked |Reserved| CmdQue | SftRe */
		inq_data.inqd_pad2= 0x32 ;	 /*WBus16|Sync|CmdQue */
		/*
		 *	Set the Vendor, Product, and Revision Level
		 *	see: <vendor>.c i.e. aac.c
		 */
		if (cid == host->this_id) {
			setinqstr(dev, (void *) (inq_data.inqd_vid), ARRAY_SIZE(container_types));
			inq_data.inqd_pdt = INQD_PDT_PROC;	/* Processor device */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			aac_internal_transfer(scsicmd, &inq_data, 0, sizeof(inq_data));
#else
			scsi_sg_copy_from_buffer(scsicmd, &inq_data,
						 sizeof(inq_data));
#endif
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
			goto scsi_done_ret;
		}
		if (dev->in_reset)
			return -1;
		setinqstr(dev, (void *) (inq_data.inqd_vid), fsa_dev_ptr[cid].type);
		inq_data.inqd_pdt = INQD_PDT_DA;	/* Direct/random access device */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
		aac_internal_transfer(scsicmd, &inq_data, 0, sizeof(inq_data));
#else
		scsi_sg_copy_from_buffer(scsicmd, &inq_data, sizeof(inq_data));
#endif
		return aac_get_container_name(scsicmd);
	}
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(SERVICE_ACTION_IN))
	case SERVICE_ACTION_IN:
#if (defined(AAC_DEBUG_INSTRUMENT_2TB))
		printk(KERN_NOTICE
		  "SERVICE_ACTION_IN, raw_io_interface=%d raw_io_64=%d\n",
		  dev->raw_io_interface, dev->raw_io_64);
#endif
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
		if ((scsicmd->cmnd[1] & 0x1f) != SAI_READ_CAPACITY_16)
			break;
		return aac_adapter_read_capacity_16(dev, scsicmd);
#else
		if (!(dev->raw_io_interface) ||
		    !(dev->raw_io_64) ||
		    ((scsicmd->cmnd[1] & 0x1f) != SAI_READ_CAPACITY_16))
			break;
	{
		u64 capacity;
		char cp[13];
		unsigned int alloc_len;

		dprintk((KERN_DEBUG "READ CAPACITY_16 command.\n"));
		capacity = fsa_dev_ptr[cid].size - 1;
		cp[0] = (capacity >> 56) & 0xff;
		cp[1] = (capacity >> 48) & 0xff;
		cp[2] = (capacity >> 40) & 0xff;
		cp[3] = (capacity >> 32) & 0xff;
		cp[4] = (capacity >> 24) & 0xff;
		cp[5] = (capacity >> 16) & 0xff;
		cp[6] = (capacity >> 8) & 0xff;
		cp[7] = (capacity >> 0) & 0xff;
		cp[8] = (fsa_dev_ptr[cid].block_size >> 24) & 0xff;
		cp[9] = (fsa_dev_ptr[cid].block_size >> 16) & 0xff;
		cp[10] = (fsa_dev_ptr[cid].block_size >> 8) & 0xff;
		cp[11] = (fsa_dev_ptr[cid].block_size) & 0xff;
		cp[12] = 0;
#if (defined(AAC_DEBUG_INSTRUMENT_2TB))
		printk(KERN_INFO "SAI_READ_CAPACITY_16(%d): "
		  "%02x %02x %02x %02x %02x %02x %02x %02x "
		  "%02x %02x %02x %02x %02x\n",
		  scsicmd->cmnd[13],
		  cp[0] & 0xff, cp[1] & 0xff, cp[2] & 0xff, cp[3] & 0xff,
		  cp[4] & 0xff, cp[5] & 0xff, cp[6] & 0xff, cp[7] & 0xff,
		  cp[8] & 0xff, cp[9] & 0xff, cp[10] & 0xff, cp[11] & 0xff,
		  cp[12] & 0xff);
#endif

		alloc_len = ((scsicmd->cmnd[10] << 24)
			     + (scsicmd->cmnd[11] << 16)
			     + (scsicmd->cmnd[12] << 8) + scsicmd->cmnd[13]);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
		aac_internal_transfer(scsicmd, cp, 0,
		  min_t(size_t, alloc_len, sizeof(cp)));
		if (sizeof(cp) < alloc_len) {
			unsigned int len, offset = sizeof(cp);

			memset(cp, 0, offset);
			do {
				len = min_t(size_t, alloc_len - offset,
						sizeof(cp));
				aac_internal_transfer(scsicmd, cp, offset, len);
			} while ((offset += len) < alloc_len);
		}
#else

		alloc_len = min_t(size_t, alloc_len, sizeof(cp));
		scsi_sg_copy_from_buffer(scsicmd, cp, alloc_len);
		if (alloc_len < scsi_bufflen(scsicmd))
			scsi_set_resid(scsicmd,
				       scsi_bufflen(scsicmd) - alloc_len);
#endif

		/* Do not cache partition table for arrays */
		scsicmd->device->removable = 1;

		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
		goto scsi_done_ret;
	}
#endif
#endif

	case READ_CAPACITY:
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
		return aac_adapter_read_capacity(dev, scsicmd);
#else
	{
		u32 capacity;
		char cp[8];

		dprintk((KERN_DEBUG "READ CAPACITY command.\n"));
		if (fsa_dev_ptr[cid].size <= 0x100000000ULL)
			capacity = fsa_dev_ptr[cid].size - 1;
		else
			capacity = (u32)-1;

		cp[0] = (capacity >> 24) & 0xff;
		cp[1] = (capacity >> 16) & 0xff;
		cp[2] = (capacity >> 8) & 0xff;
		cp[3] = (capacity >> 0) & 0xff;
		cp[4] = (fsa_dev_ptr[cid].block_size >> 24) & 0xff;
		cp[5] = (fsa_dev_ptr[cid].block_size >> 16) & 0xff;
		cp[6] = (fsa_dev_ptr[cid].block_size >> 8) & 0xff;
		cp[7] = (fsa_dev_ptr[cid].block_size) & 0xff;
#if (defined(AAC_DEBUG_INSTRUMENT_2TB))
		printk(KERN_INFO "READ_CAPACITY: "
		  "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		  cp[0] & 0xff, cp[1] & 0xff, cp[2] & 0xff, cp[3] & 0xff,
		  cp[4] & 0xff, cp[5] & 0xff, cp[6] & 0xff, cp[7] & 0xff);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
		aac_internal_transfer(scsicmd, cp, 0, sizeof(cp));
#else
		scsi_sg_copy_from_buffer(scsicmd, cp, sizeof(cp));
#endif
		/* Do not cache partition table for arrays */
		scsicmd->device->removable = 1;
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
		  SAM_STAT_GOOD;
		goto scsi_done_ret;
	}
#endif

	case MODE_SENSE:
	{
		int mode_buf_length = 4;
		u32 capacity;
		aac_modep_data mpd;

		if (fsa_dev_ptr[cid].size <= 0x100000000ULL)
			capacity = fsa_dev_ptr[cid].size - 1;
		else
			capacity = (u32)-1;

		dprintk((KERN_DEBUG "MODE SENSE command.\n"));
		memset((char*)&mpd,0,sizeof(aac_modep_data));
		mpd.hd.data_length = sizeof(mpd.hd) - 1;	/* Mode data length */
		mpd.hd.med_type = 0;	/* Medium type - default */
		mpd.hd.dev_par = 0;	/* Device-specific param,
					   bit 8: 0/1 = write enabled/protected
					   bit 4: 0/1 = FUA enabled */

#if (defined(RIO_SUREWRITE))
		if (dev->raw_io_interface && ((aac_cache & 5) != 1))
			mpd.hd.dev_par = 0x10;
#endif
		if (scsicmd->cmnd[1] & 0x8) {
			mpd.hd.bd_length = 0;   /* Block descriptor length */
		} else {
			mpd.hd.bd_length = sizeof(mpd.bd);
			mpd.hd.data_length += mpd.hd.bd_length;
			mpd.bd.block_length[0] = (fsa_dev_ptr[cid].block_size >> 16) & 0xff;
			mpd.bd.block_length[1] = (fsa_dev_ptr[cid].block_size >> 8) &  0xff;
			mpd.bd.block_length[2] = fsa_dev_ptr[cid].block_size  & 0xff;

			mpd.mpc_buf[0] = scsicmd->cmnd[2];
			if(scsicmd->cmnd[2] == 0x1C)
			{
				mpd.mpc_buf[1] = 0xa;	//page length
				mpd.hd.data_length = 23;	/* Mode data length */
			}
			else
				mpd.hd.data_length = 15;	/* Mode data length */

			if (capacity > 0xffffff) {
				mpd.bd.block_count[0] = 0xff;
				mpd.bd.block_count[1] = 0xff;
				mpd.bd.block_count[2] = 0xff;
			} else {
				mpd.bd.block_count[0] = (capacity >> 16) & 0xff;
				mpd.bd.block_count[1] = (capacity >> 8) & 0xff;
				mpd.bd.block_count[2] = capacity  & 0xff;
			}
		}

		if (((scsicmd->cmnd[2] & 0x3f) == 8) ||
		  ((scsicmd->cmnd[2] & 0x3f) == 0x3f)) {
			mpd.hd.data_length += 3;
			mpd.mpc_buf[0] = 8;
			mpd.mpc_buf[1] = 1;
			mpd.mpc_buf[2] = ((aac_cache & 6) == 2)
				  ? 0 : 0x04; /* WCE */
			mode_buf_length = sizeof(mpd);

		}

		if (mode_buf_length > scsicmd->cmnd[4])
			mode_buf_length = scsicmd->cmnd[4];
		else
			mode_buf_length = sizeof(mpd);


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
		aac_internal_transfer(scsicmd, (char*)&mpd, 0, mode_buf_length);
#else
		scsi_sg_copy_from_buffer(scsicmd, (char*)&mpd, mode_buf_length);
#endif
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
		goto scsi_done_ret;
	}

	case MODE_SENSE_10:
	{
		u32 capacity;
		int mode_buf_length = 8;
		aac_modep10_data mpd10;

		if (fsa_dev_ptr[cid].size <= 0x100000000ULL)
			capacity = fsa_dev_ptr[cid].size - 1;
		else
			capacity = (u32)-1;

		dprintk((KERN_DEBUG "MODE SENSE 10 byte command.\n"));
		memset((char*)&mpd10,0,sizeof(aac_modep10_data));
		mpd10.hd.data_length[0] = 0;			/* Mode data length (MSB) */
		mpd10.hd.data_length[1] = sizeof(mpd10.hd) - 1;	/* Mode data length (LSB) */
		mpd10.hd.med_type = 0;	/* Medium type - default */
		mpd10.hd.dev_par = 0;	/* Device-specific param,
					   bit 8: 0/1 = write enabled/protected
					   bit 4: 0/1 = FUA enabled */
#if (defined(RIO_SUREWRITE))
		if (dev->raw_io_interface && ((aac_cache & 5) != 1))
			mpd10.hd.dev_par = 0x10;
#endif
		mpd10.hd.rsrvd[0] = 0;	/* reserved */
		mpd10.hd.rsrvd[1] = 0;  /* reserved */

		if (scsicmd->cmnd[1] & 0x8) {
			mpd10.hd.bd_length[0] = 0;	/* Block descriptor length (MSB) */
			mpd10.hd.bd_length[1] = 0;	/* Block descriptor length (LSB) */
		} else {
			mpd10.hd.bd_length[0] = 0;
			mpd10.hd.bd_length[1] = sizeof(mpd10.bd);

			mpd10.hd.data_length[1] += mpd10.hd.bd_length[1];

			mpd10.bd.block_length[0] = (fsa_dev_ptr[cid].block_size >> 16) & 0xff;
			mpd10.bd.block_length[1] = (fsa_dev_ptr[cid].block_size >> 8) & 0xff;
			mpd10.bd.block_length[2] = fsa_dev_ptr[cid].block_size  & 0xff;

			if (capacity > 0xffffff) {
				mpd10.bd.block_count[0] = 0xff;
				mpd10.bd.block_count[1] = 0xff;
				mpd10.bd.block_count[2] = 0xff;
			} else {
				mpd10.bd.block_count[0] = (capacity >> 16) & 0xff;
				mpd10.bd.block_count[1] = (capacity >> 8) & 0xff;
				mpd10.bd.block_count[2] = capacity  & 0xff;
			}
		}
		if (((scsicmd->cmnd[2] & 0x3f) == 8) ||
		  ((scsicmd->cmnd[2] & 0x3f) == 0x3f)) {

			mpd10.hd.data_length[1] += 3;
			mpd10.mpc_buf[0] = 8;
			mpd10.mpc_buf[1] = 1;
			mpd10.mpc_buf[2] = ((aac_cache & 6) == 2)
				     ? 0 : 0x04; /* WCE */

			mode_buf_length = sizeof(mpd10);

			if (mode_buf_length > scsicmd->cmnd[8])
				mode_buf_length = scsicmd->cmnd[8];
		}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
		aac_internal_transfer(scsicmd, (char*)&mpd10, 0, mode_buf_length);
#else
		scsi_sg_copy_from_buffer(scsicmd, (char*)&mpd10, mode_buf_length);
#endif

		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
		goto scsi_done_ret;
	}

	case REQUEST_SENSE:
		dprintk((KERN_DEBUG "REQUEST SENSE command.\n"));
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data, sizeof (struct sense_data));
		memset(&dev->fsa_dev[cid].sense_data, 0, sizeof (struct sense_data));
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
		goto scsi_done_ret;

	case ALLOW_MEDIUM_REMOVAL:
		dprintk((KERN_DEBUG "LOCK command.\n"));
		if (scsicmd->cmnd[4])
			fsa_dev_ptr[cid].locked = 1;
		else
			fsa_dev_ptr[cid].locked = 0;

		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
		goto scsi_done_ret;

	/*
	 *	These commands are all No-Ops
	 */
	case TEST_UNIT_READY:
		if (fsa_dev_ptr[cid].sense_data.sense_key == NOT_READY) {
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 |
				SAM_STAT_CHECK_CONDITION;
			set_sense(&dev->fsa_dev[cid].sense_data,
				  NOT_READY, SENCODE_BECOMING_READY,
				  ASENCODE_BECOMING_READY, 0, 0);
			memcpy(scsicmd->sense_buffer,
			       &dev->fsa_dev[cid].sense_data,
			       min_t(size_t,
				     sizeof(dev->fsa_dev[cid].sense_data),
				     SCSI_SENSE_BUFFERSIZE));
			goto scsi_done_ret;
		}

	/* FALLTHRU */
	case RESERVE:
	case RELEASE:
	case REZERO_UNIT:
	case REASSIGN_BLOCKS:
	case SEEK_10:
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_GOOD;
		goto scsi_done_ret;

	case START_STOP:
		return aac_start_stop(scsicmd);

	/* FALLTHRU */
	default:
	/*
	*	Unhandled commands
    */
		goto scsi_default;
	}

	scsi_default:
		dprintk((KERN_WARNING "Unhandled SCSI Command: 0x%x.\n", scsicmd->cmnd[0]));
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_CHECK_CONDITION;
		set_sense(&dev->fsa_dev[cid].sense_data,
			ILLEGAL_REQUEST, SENCODE_INVALID_COMMAND,
			ASENCODE_INVALID_COMMAND, 0, 0);
		memcpy(scsicmd->sense_buffer, &dev->fsa_dev[cid].sense_data,
			min_t(size_t,
			sizeof(dev->fsa_dev[cid].sense_data),
			SCSI_SENSE_BUFFERSIZE));

	scsi_done_ret:
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		__aac_io_done(scsicmd);
#else
		scsicmd->scsi_done(scsicmd);
#endif
		return 0;
}
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

static int aac_scsi_cmd_apre(struct scsi_cmnd * cmd)
{
	struct aac_apre_srb * hw_apre = (struct aac_apre_srb *)&fib->hw_apre_va;
	struct aac_dev * dev = fib->dev;
	struct fsa_dev_info * fsa_dev_ptr = &dev->fsa_dev[scmd_id(cmd)];
	int status;
	struct fib * fib;

	/*
	 *	Allocate and initialize a Fib then setup a BlockWrite command
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (!(fib = aac_fib_alloc_tag((struct aac_dev *)cmd->device->host->hostdata, cmd))) {
#else
	if (!(fib = aac_fib_alloc((struct aac_dev *)cmd->device->host->hostdata))) {
#endif

		cmd->result = DID_ERROR << 16;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		aac_io_done(cmd);
#else
		cmd->scsi_done(cmd);
#endif
		return 0;
	}

	fib->Credits = fsa_dev_ptr->Credits;
	hw_apre->DataDir = 0;
	if (cmd->request_bufflen) switch (cmd->sc_data_direction) {
	case DMA_TO_DEVICE:
		hw_apre->DataDir = AAC_DATA_OUT_IOP_PERSPECTIVE;
		break;
	case DMA_BIDIRECTIONAL:
		hw_apre->DataDir = AAC_DATA_OUT_IOP_PERSPECTIVE | AAC_DATA_IN_IOP_PERSPECTIVE;
		break;
	case DMA_FROM_DEVICE:
		hw_apre->DataDir = AAC_DATA_IN_IOP_PERSPECTIVE;
		break;
	}
	hw_apre->header.Operation = AS_REQ_LKP_CODE_EXEC_SCSI_TASK;
	hw_apre->header.DestObjHandle = fsa_dev_ptr->DestObjHandle;
	hw_apre->CDBLength = (cmd->cmd_len - 1);
	memcpy(hw_apre->Cdb, cmd->cmnd, cmd->cmd_len);
	hw_apre->TransferSizeLow = cpu_to_le32(cmd->request_bufflen);
	hw_apre->TransferSizeHigh = cpu_to_le32((u32)((u64)cmd->request_bufflen>>32));
	status = aac_adapter_build_sg(fib, cmd, &hw_apre->Sgl);
	hw_apre->NumEsge = (status >
	  (sizeof(hw_apre->Sgl.nark)/sizeof(hw_apre->Sgl.nark.FirstElement)))
		? 1 : status;
	status = aac_adapter_deliver(fib);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		cmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	printk(KERN_WARNING "aac_scsi_cmd_apre: failed with status: %d\n", status);
	/*
	 *	For some reason, the Fib didn't queue, return QUEUE_FULL
	 */
	cmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_TASK_SET_FULL;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(cmd);
#else
	cmd->scsi_done(cmd);
#endif

	aac_fib_complete(fib);
	aac_fib_free(fib);
	return 0;
}
#endif
#if (!defined(CONFIG_COMMUNITY_KERNEL))

static int busy_disk(struct aac_dev * dev, int cid)
{
	if ((dev != (struct aac_dev *)NULL)
	 && (dev->scsi_host_ptr != (struct Scsi_Host *)NULL)) {
		struct scsi_device *device;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		shost_for_each_device(device, dev->scsi_host_ptr)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		list_for_each_entry(device, &dev->scsi_host_ptr->my_devices, siblings)
#else
		for (device = dev->scsi_host_ptr->host_queue;
		  device != (struct scsi_device *)NULL;
		  device = device->next)
#endif
		{
			if ((device->channel == CONTAINER_TO_CHANNEL(cid))
			 && (device->id == CONTAINER_TO_ID(cid))
			 && (device->lun == CONTAINER_TO_LUN(cid))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
			 && (atomic_read(&device->access_count)
			  || test_bit(SHOST_RECOVERY, &dev->scsi_host_ptr->shost_state)
			  || dev->scsi_host_ptr->eh_active)) {
#elif (defined(RHEL_MAJOR) && RHEL_MAJOR == 7 && RHEL_MINOR >= 2)
			&& (atomic_read(&device->device_busy)
			  || (SHOST_RECOVERY == dev->scsi_host_ptr->shost_state))) {
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0))) || defined(SCSI_HAS_SHOST_STATE_ENUM))
			&& (device->device_busy
			  || (SHOST_RECOVERY == dev->scsi_host_ptr->shost_state))) {
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)) || defined(SCSI_HAS_SHOST_STATE_ENUM))
			&& (atomic_read(&device->device_busy)
			  || (SHOST_RECOVERY == dev->scsi_host_ptr->shost_state))) {
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
			&& (device->device_busy
			  || test_bit(SHOST_RECOVERY, &dev->scsi_host_ptr->shost_state))) {
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
			&& (device->device_busy
			  || test_bit(SHOST_RECOVERY, &dev->scsi_host_ptr->shost_state)
			  || dev->scsi_host_ptr->eh_active)) {
#else
			&& (device->access_count
			  || dev->scsi_host_ptr->in_recovery
			  || dev->scsi_host_ptr->eh_active)) {
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
				scsi_device_put(device);
#endif
				return 1;
			}
		}
	}
	return 0;
}
#endif

static int query_disk(struct aac_dev *dev, void __user *arg)
{
	struct aac_query_disk qd;
	struct fsa_dev_info *fsa_dev_ptr;

	fsa_dev_ptr = dev->fsa_dev;
	if (!fsa_dev_ptr)
		return -EBUSY;
	if (copy_from_user(&qd, arg, sizeof (struct aac_query_disk)))
		return -EFAULT;
	if (qd.cnum == -1)
		qd.cnum = qd.id;
	else if ((qd.bus == -1) && (qd.id == -1) && (qd.lun == -1))
	{
		if (qd.cnum < 0 || qd.cnum >= dev->maximum_num_containers)
			return -EINVAL;
		qd.instance = dev->scsi_host_ptr->host_no;
		qd.bus = 0;
		qd.id = CONTAINER_TO_ID(qd.cnum);
		qd.lun = CONTAINER_TO_LUN(qd.cnum);
	}
	else return -EINVAL;

	qd.valid = fsa_dev_ptr[qd.cnum].valid != 0;
#if (defined(CONFIG_COMMUNITY_KERNEL))
	qd.locked = fsa_dev_ptr[qd.cnum].locked;
#else
	qd.locked = fsa_dev_ptr[qd.cnum].locked || busy_disk(dev, qd.cnum);
#endif
	qd.deleted = fsa_dev_ptr[qd.cnum].deleted;

#if (!defined(__VMKLNX30__) && !defined(__VMKLNX__))
	if (fsa_dev_ptr[qd.cnum].devname[0] == '\0')
		qd.unmapped = 1;
	else
		qd.unmapped = 0;
#else
	qd.unmapped = 0;
#endif

	strlcpy(qd.name, fsa_dev_ptr[qd.cnum].devname,
	  min(sizeof(qd.name), sizeof(fsa_dev_ptr[qd.cnum].devname) + 1));

	if (copy_to_user(arg, &qd, sizeof (struct aac_query_disk)))
		return -EFAULT;
	return 0;
}

static int force_delete_disk(struct aac_dev *dev, void __user *arg)
{
	struct aac_delete_disk dd;
	struct fsa_dev_info *fsa_dev_ptr;

	fsa_dev_ptr = dev->fsa_dev;
	if (!fsa_dev_ptr)
		return -EBUSY;

	if (copy_from_user(&dd, arg, sizeof (struct aac_delete_disk)))
		return -EFAULT;

	if (dd.cnum >= dev->maximum_num_containers)
		return -EINVAL;
	/*
	 *	Mark this container as being deleted.
	 */
	fsa_dev_ptr[dd.cnum].deleted = 1;
	/*
	 *	Mark the container as no longer valid
	 */
	fsa_dev_ptr[dd.cnum].valid = 0;
	return 0;
}

static int delete_disk(struct aac_dev *dev, void __user *arg)
{
	struct aac_delete_disk dd;
	struct fsa_dev_info *fsa_dev_ptr;

	fsa_dev_ptr = dev->fsa_dev;
	if (!fsa_dev_ptr)
		return -EBUSY;

	if (copy_from_user(&dd, arg, sizeof (struct aac_delete_disk)))
		return -EFAULT;

	if (dd.cnum >= dev->maximum_num_containers)
		return -EINVAL;
	/*
	 *	If the container is locked, it can not be deleted by the API.
	 */
#if (defined(CONFIG_COMMUNITY_KERNEL))
	if (fsa_dev_ptr[dd.cnum].locked)
#else
	if (fsa_dev_ptr[dd.cnum].locked || busy_disk(dev, dd.cnum))
#endif
		return -EBUSY;
	else {
		/*
		 *	Mark the container as no longer being valid.
		 */
		fsa_dev_ptr[dd.cnum].valid = 0;
		fsa_dev_ptr[dd.cnum].devname[0] = '\0';
		return 0;
	}
}

#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
static int aac_register_fib_send(struct aac_dev *dev, void __user *arg)
{
	fib_send_t __user callback;

	if (arg == NULL) {
		return -EINVAL;
	}
	callback = *((fib_send_t __user *)arg);
	*((fib_send_t __user *)arg) = (fib_send_t __user)aac_fib_send;
	if (callback == (fib_send_t __user)NULL) {
		aac_fib_send_switch = aac_fib_send;
		return 0;
	}
	if (aac_fib_send_switch != aac_fib_send) {
		return -EBUSY;
	}
	aac_fib_send_switch = (fib_send_t)callback;
	return 0;
}

#endif
int aac_dev_ioctl(struct aac_dev *dev, int cmd, void __user *arg)
{

	int retval;
	if (cmd != FSACTL_GET_NEXT_ADAPTER_FIB){
		adbg_ioctl(dev, KERN_DEBUG, "aac_dev_ioctl(%p,%x,%p)\n", dev, cmd, arg);
	}

	switch (cmd) {
	case FSACTL_QUERY_DISK:
		retval = query_disk(dev, arg);
		adbg_ioctl(dev, KERN_DEBUG, "aac_dev_ioctl returns %d\n", retval);
		return retval;
	case FSACTL_DELETE_DISK:
		retval = delete_disk(dev, arg);
		adbg_ioctl(dev, KERN_DEBUG, "aac_dev_ioctl returns %d\n", retval);
		return retval;
	case FSACTL_FORCE_DELETE_DISK:
		retval = force_delete_disk(dev, arg);
		adbg_ioctl(dev, KERN_DEBUG, "aac_dev_ioctl returns %d\n", retval);
		return retval;
	case FSACTL_GET_CONTAINERS:
		retval = aac_get_containers(dev);
		adbg_ioctl(dev, KERN_DEBUG, "aac_dev_ioctl returns %d\n", retval);
		return retval;
#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
	case FSACTL_REGISTER_FIB_SEND:
		retval = aac_register_fib_send(dev, arg);
		adbg_ioctl(dev, KERN_DEBUG, "aac_dev_ioctl returns %d\n", retval);
		return retval;
#endif
	default:
		adbg_ioctl(dev, KERN_DEBUG,"aac_dev_ioctl returns -ENOTTY\n");
		return -ENOTTY;
	}
}

/**
 *
 * aac_srb_callback
 * @context: the context set in the fib - here it is scsi cmd
 * @fibptr: pointer to the fib
 *
 * Handles the completion of a scsi command to a non dasd device
 *
 */

static void aac_srb_callback(void *context, struct fib * fibptr)
{
	struct aac_dev *dev;
	struct aac_srb_reply *srbreply;
	struct scsi_cmnd *scsicmd;

	scsicmd = (struct scsi_cmnd *) context;

	if (!aac_valid_context(scsicmd, fibptr))
		return;

	BUG_ON(fibptr == NULL);

	dev = fibptr->dev;

	srbreply = (struct aac_srb_reply *) fib_data(fibptr);

	scsicmd->sense_buffer[0] = '\0';  /* Initialize sense valid flag to false */

	if (fibptr->flags & FIB_CONTEXT_FLAG_FASTRESP) {
		/* fast response */
		srbreply->srb_status = cpu_to_le32(SRB_STATUS_SUCCESS);
		srbreply->scsi_status = cpu_to_le32(SAM_STAT_GOOD);
	} else {
		/*
		 *	Calculate resid for sg
		 */
		scsi_set_resid(scsicmd, scsi_bufflen(scsicmd)
				   - le32_to_cpu(srbreply->data_xfer_length));
	}

#if (!defined(__VMKLNX30__) || defined(__x86_64__))
	scsi_dma_unmap(scsicmd);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	if(!scsi_sg_count(scsicmd) && scsi_bufflen(scsicmd))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		pci_unmap_single(dev->pdev, (dma_addr_t)scsicmd->SCp.dma_handle, scsicmd->request_bufflen,
#else
		pci_unmap_single(dev->pdev, scsicmd->SCp.dma_handle, scsicmd->request_bufflen,
#endif
			scsicmd->sc_data_direction);
#endif
#endif
	/* expose physical device if expose_physicald flag is on */
	if(scsicmd->cmnd[0] == INQUIRY && !(scsicmd->cmnd[1] & 0x01) && expose_physicals > 0)
		aac_expose_phy_device(scsicmd);

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
#if defined(__ESX5__)

#define SCSI_VENDOR_ID_OFFSET	8
#define SCSI_VENDOR_ID_LEN	8
#define SCSI_PRODUCT_ID_OFFSET	16
#define SCSI_PRODUCT_ID_LEN	16

	if (scmd_channel(scsicmd) == 3) {
               if (scsicmd->cmnd[0] == INQUIRY && !(scsicmd->cmnd[1] & 0x01)) {
		char inq_data;
		char vi[SCSI_VENDOR_ID_LEN + 1];
		char pi[SCSI_PRODUCT_ID_LEN + 1];
		char resp[32];

                scsi_sg_copy_to_buffer(scsicmd,  &inq_data, sizeof(inq_data));
		scsi_sg_copy_to_buffer(scsicmd,  resp, sizeof(resp));

		memcpy(vi,resp + SCSI_VENDOR_ID_OFFSET, SCSI_VENDOR_ID_LEN);
		vi[SCSI_VENDOR_ID_LEN]='\0';
		memcpy(pi,resp + SCSI_PRODUCT_ID_OFFSET, SCSI_PRODUCT_ID_LEN);
		pi[SCSI_PRODUCT_ID_LEN]='\0';

                if ((inq_data & 0x1F) == TYPE_ENCLOSURE &&
			(strstr(vi,"ADAPTEC") != NULL)  &&
			(strstr(pi,"SGPIO") != NULL)  )
			inq_data |= 0x20;

		scsi_sg_copy_from_buffer(scsicmd, &inq_data, sizeof(inq_data));
	}
       }
#endif
#endif
    	/*
	 * First check the fib status
	 */

	if (le32_to_cpu(srbreply->status) != ST_OK){
		int len;
		printk(KERN_WARNING "aac_srb_callback: srb failed, status = %d\n", le32_to_cpu(srbreply->status));
		len = min_t(u32, le32_to_cpu(srbreply->sense_data_size),
			    SCSI_SENSE_BUFFERSIZE);
		scsicmd->result = DID_ERROR << 16 | COMMAND_COMPLETE << 8 | SAM_STAT_CHECK_CONDITION;
		memcpy(scsicmd->sense_buffer, srbreply->sense_data, len);
	}

	/*
	 * Next check the srb status
	 */
	switch( (le32_to_cpu(srbreply->srb_status))&0x3f){
	case SRB_STATUS_ERROR_RECOVERY:
	case SRB_STATUS_PENDING:
	case SRB_STATUS_SUCCESS:
//#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)) || defined(BLIST_NO_ULD_ATTACH))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
#else
		if ((scsicmd->cmnd[0] == INQUIRY) && (expose_physicals <= 0)) {
			u8 b;
			/* We can't expose disk devices because we can't tell
			 * whether they are the raw container drives or stand
			 * alone drives.  If they have the removable bit set
			 * then we should expose them though.
			 */
			b = *((u8*)scsicmd->request_buffer);
			if (((b & 0x1F) != TYPE_DISK) ||
			  (((u8*)scsicmd->request_buffer)[1] & 0x80) ||
			/*
			 * We will allow disk devices if in RAID/SCSI mode and
			 * the channel is 2
			 */
			  ((dev->raid_scsi_mode) &&
					(scmd_channel(scsicmd) == 2)) ||
			  (dev->jbod && !(b >> 5))) {
				if (dev->jbod && ((b & 0x1F) == TYPE_DISK))
					((u8*)scsicmd->request_buffer)[1] |=
						1 << 7;
				scsicmd->result = DID_OK << 16 |
						COMMAND_COMPLETE << 8;
			} else if (expose_physicals) {
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(BLIST_NO_ULD_ATTACH)))
				scsicmd->device->no_uld_attach = (void *)1;
#else
				scsicmd->device->no_uld_attach = 1;
#endif
//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)))
				/* Insurance */
				(*(u8*)scsicmd->request_buffer) |= 1 << 5;
//#endif
				scsicmd->result = DID_OK << 16 |
						COMMAND_COMPLETE << 8;
			} else
				scsicmd->result = DID_NO_CONNECT << 16 |
						COMMAND_COMPLETE << 8;
		} else
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
#endif
		break;
	case SRB_STATUS_DATA_OVERRUN:
		switch(scsicmd->cmnd[0]){
		case  READ_6:
		case  WRITE_6:
		case  READ_10:
		case  WRITE_10:
		case  READ_12:
		case  WRITE_12:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(READ_16))
		case  READ_16:
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)) || defined(WRITE_16))
		case  WRITE_16:
#endif
			if (le32_to_cpu(srbreply->data_xfer_length) < scsicmd->underflow) {
				printk(KERN_WARNING"aacraid: SCSI CMD underflow\n");
			} else {
				printk(KERN_WARNING"aacraid: SCSI CMD Data Overrun\n");
			}
			scsicmd->result = DID_ERROR << 16 | COMMAND_COMPLETE << 8;
			break;
		case INQUIRY: {
//#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)) || defined(BLIST_NO_ULD_ATTACH))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
#else
			if (expose_physicals <= 0) {
				/*
				 * We can't expose disk devices because we
				 * can't tell whether they are the raw
				 * container drives or stand alone drives
				 */
				u8 b = *((u8*)scsicmd->request_buffer);
				if ((((b & 0x1f) != TYPE_DISK) ||
				  (((u8*)scsicmd->request_buffer)[1] & 0x80)) ||
				/*
				 * We will allow disk devices if in RAID/SCSI
				 * mode and the channel is 2
				 */
				  ((dev->raid_scsi_mode) &&
						(scmd_channel(scsicmd) == 2)) ||
				  (dev->jbod && !(b >> 5))) {
					if (dev->jbod && ((b & 0x1F) == TYPE_DISK))
						((u8*)scsicmd->request_buffer)[1] |=
							1 << 7;
					scsicmd->result = DID_OK << 16 |
						COMMAND_COMPLETE << 8;
				} else if (expose_physicals) {
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(BLIST_NO_ULD_ATTACH)))
					scsicmd->device->no_uld_attach = (void *)1;
#else
					scsicmd->device->no_uld_attach = 1;
#endif
//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)))
					/* Insurance */
					(*(u8*)scsicmd->request_buffer) |= 1 << 5;
//#endif
					scsicmd->result = DID_OK << 16 |
							COMMAND_COMPLETE << 8;
				} else
					scsicmd->result = DID_NO_CONNECT << 16 |
							COMMAND_COMPLETE << 8;
			} else
				scsicmd->result = DID_OK << 16 |
						COMMAND_COMPLETE << 8;
#endif
			break;
		}
		default:
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
			break;
		}
		break;
	case SRB_STATUS_ABORTED:
		scsicmd->result = DID_ABORT << 16 | ABORT << 8;
		break;
	case SRB_STATUS_ABORT_FAILED:
		// Not sure about this one - but assuming the hba was trying to abort for some reason
		scsicmd->result = DID_ERROR << 16 | ABORT << 8;
		break;
	case SRB_STATUS_PARITY_ERROR:
		scsicmd->result = DID_PARITY << 16 | MSG_PARITY_ERROR << 8;
		break;
	case SRB_STATUS_NO_DEVICE:
	case SRB_STATUS_INVALID_PATH_ID:
	case SRB_STATUS_INVALID_TARGET_ID:
	case SRB_STATUS_INVALID_LUN:
	case SRB_STATUS_SELECTION_TIMEOUT:
		scsicmd->result = DID_NO_CONNECT << 16 | COMMAND_COMPLETE << 8;
		break;

	case SRB_STATUS_COMMAND_TIMEOUT:
	case SRB_STATUS_TIMEOUT:
		scsicmd->result = DID_TIME_OUT << 16 | COMMAND_COMPLETE << 8;
		break;

	case SRB_STATUS_BUSY:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) || defined(DID_BUS_BUSY))
		scsicmd->result = DID_BUS_BUSY << 16 | COMMAND_COMPLETE << 8;
#else
		scsicmd->result = DID_NO_CONNECT << 16 | COMMAND_COMPLETE << 8;
#endif
		break;

	case SRB_STATUS_BUS_RESET:
		scsicmd->result = DID_RESET << 16 | COMMAND_COMPLETE << 8;
		break;

	case SRB_STATUS_MESSAGE_REJECTED:
		scsicmd->result = DID_ERROR << 16 | MESSAGE_REJECT << 8;
		break;
	case SRB_STATUS_REQUEST_FLUSHED:
	case SRB_STATUS_ERROR:
	case SRB_STATUS_INVALID_REQUEST:
	case SRB_STATUS_REQUEST_SENSE_FAILED:
	case SRB_STATUS_NO_HBA:
	case SRB_STATUS_UNEXPECTED_BUS_FREE:
	case SRB_STATUS_PHASE_SEQUENCE_FAILURE:
	case SRB_STATUS_BAD_SRB_BLOCK_LENGTH:
	case SRB_STATUS_DELAYED_RETRY:
	case SRB_STATUS_BAD_FUNCTION:
	case SRB_STATUS_NOT_STARTED:
	case SRB_STATUS_NOT_IN_USE:
	case SRB_STATUS_FORCE_ABORT:
	case SRB_STATUS_DOMAIN_VALIDATION_FAIL:
	default:
#ifdef AAC_DETAILED_STATUS_INFO
		printk("aacraid: SRB ERROR(%u) %s scsi cmd 0x%x - scsi status 0x%x\n",
			le32_to_cpu(srbreply->srb_status) & 0x3F,
			aac_get_status_string(
				le32_to_cpu(srbreply->srb_status) & 0x3F),
			scsicmd->cmnd[0],
			le32_to_cpu(srbreply->scsi_status));
#endif
		if((scsicmd->cmnd[0] == ATA_12) || (scsicmd->cmnd[0] == ATA_16)) {

		/*
		 * When the CC bit is SET by the host in ATA pass thru CDB, driver is supposed to return DID_OK
		 * When the CC bit is RESET by the host, driver should return DID_ERROR
		 */
                       if(scsicmd->cmnd[2] & (0x01 << 5)) {
                               scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
                               break;
                       }
                       else {
                               scsicmd->result = DID_ERROR << 16 | COMMAND_COMPLETE << 8;
                               break;
                       }
               }
               else {
                       scsicmd->result = DID_ERROR << 16 | COMMAND_COMPLETE << 8;
                       break;
               }
	}
	if (le32_to_cpu(srbreply->scsi_status) == SAM_STAT_CHECK_CONDITION) {
		int len;
		scsicmd->result |= SAM_STAT_CHECK_CONDITION;
		len = min_t(u32, le32_to_cpu(srbreply->sense_data_size),
			    SCSI_SENSE_BUFFERSIZE);
#ifdef AAC_DETAILED_STATUS_INFO
		printk(KERN_WARNING "aac_srb_callback: check condition, status = %d len=%d\n",
					le32_to_cpu(srbreply->status), len);
#endif
		memcpy(scsicmd->sense_buffer, srbreply->sense_data, len);
	}
	/*
	 * OR in the scsi status (already shifted up a bit)
	 */
	scsicmd->result |= le32_to_cpu(srbreply->scsi_status);

	aac_fib_complete(fibptr);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
    aac_fib_free_tag(fibptr);
#else
	aac_fib_free(fibptr);
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	aac_io_done(scsicmd);
#else
	scsicmd->scsi_done(scsicmd);
#endif
}

/**
 *
 * aac_hba_callback
 * @context: the context set in the fib - here it is scsi cmd
 * @fibptr: pointer to the fib
 *
 * Handles the completion of a native HBA scsi command
 *
 */

void aac_hba_callback(void *context, struct fib * fibptr)
{
	struct aac_dev *dev;
	struct scsi_cmnd *scsicmd;

	scsicmd = (struct scsi_cmnd *) context;

	if (!aac_valid_context(scsicmd, fibptr))
		return;

	BUG_ON(fibptr == NULL);
	dev = fibptr->dev;

	if (!(fibptr->flags & FIB_CONTEXT_FLAG_NATIVE_HBA_TMF)) {
#if (!defined(__VMKLNX30__) || defined(__x86_64__))
		scsi_dma_unmap(scsicmd);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
		if(!scsi_sg_count(scsicmd) && scsi_bufflen(scsicmd))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
			pci_unmap_single(dev->pdev, (dma_addr_t)
			 scsicmd->SCp.dma_handle, scsicmd->request_bufflen,
#else
			pci_unmap_single(dev->pdev,
			 scsicmd->SCp.dma_handle, scsicmd->request_bufflen,
#endif
			 scsicmd->sc_data_direction);
#endif
#endif
	}

	if (fibptr->flags & FIB_CONTEXT_FLAG_FASTRESP) {
		/* fast response */
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
	} else {
		struct aac_hba_resp *err =
			&((struct aac_native_hba *)fibptr->hw_fib_va)->resp.err;

		// BUG_ON(err->iu_type != HBA_IU_TYPE_RESP);
		if (err->service_response == HBA_RESP_SVCRES_TASK_COMPLETE) {
			scsicmd->result = err->status;
			/* set residual count */
			scsi_set_resid(scsicmd,
				le32_to_cpu(err->residual_count));

			switch (err->status) {
			case SAM_STAT_GOOD:
				scsicmd->result |= DID_OK << 16 |
					COMMAND_COMPLETE << 8;
				break;
			case SAM_STAT_CHECK_CONDITION:
			{
				int len;

				len = min_t(u8, err->sense_response_data_len,
					SCSI_SENSE_BUFFERSIZE);
				if (len)
					memcpy(scsicmd->sense_buffer,
						err->sense_response_buf, len);
				scsicmd->result |= DID_OK << 16 |
					COMMAND_COMPLETE << 8;
				break;
			}
			case SAM_STAT_BUSY:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) || defined(DID_BUS_BUSY))
				scsicmd->result |= DID_BUS_BUSY << 16 |
					COMMAND_COMPLETE << 8;
#else
				scsicmd->result |= DID_NO_CONNECT << 16 |
					COMMAND_COMPLETE << 8;
#endif
			case SAM_STAT_TASK_ABORTED:
				scsicmd->result |= DID_ABORT << 16 |
					ABORT << 8;
				break;
			case SAM_STAT_RESERVATION_CONFLICT:
			case SAM_STAT_TASK_SET_FULL:
			default:
				scsicmd->result |= DID_ERROR << 16 |
					COMMAND_COMPLETE << 8;
				break;
			}
		} else if (err->service_response == HBA_RESP_SVCRES_FAILURE) {
			switch (err->status) {
			case HBA_RESP_STAT_HBAMODE_DISABLED:
			{
				u32 bus, cid;

				bus =aac_logical_to_phys(scmd_channel(scsicmd));
				cid =scmd_id(scsicmd);
				if (dev->hba_map[bus][cid].devtype ==
					AAC_DEVTYPE_NATIVE_RAW) {
					dev->hba_map[bus][cid].devtype =
						AAC_DEVTYPE_ARC_RAW;
					dev->hba_map[bus][cid].rmw_nexus =
						0xffffffff;
				}
				scsicmd->result = DID_NO_CONNECT << 16 |
					COMMAND_COMPLETE << 8;
				break;
			}
			case HBA_RESP_STAT_IO_ERROR:
			case HBA_RESP_STAT_NO_PATH_TO_DEVICE:
				scsicmd->result = DID_OK << 16 |
					COMMAND_COMPLETE << 8 | SAM_STAT_BUSY;
				break;
			case HBA_RESP_STAT_IO_ABORTED:
				scsicmd->result = DID_ABORT << 16 |
					ABORT << 8;
				break;
			case HBA_RESP_STAT_INVALID_DEVICE:
				scsicmd->result = DID_NO_CONNECT << 16 |
					COMMAND_COMPLETE << 8;
				break;
			case HBA_RESP_STAT_UNDERRUN:
				/* UNDERRUN is OK */
				scsicmd->result = DID_OK << 16 |
					COMMAND_COMPLETE << 8;
				break;
			case HBA_RESP_STAT_OVERRUN:
			default:
				scsicmd->result = DID_ERROR << 16 |
					COMMAND_COMPLETE << 8;
				break;
			}
		} else if (err->service_response ==
			HBA_RESP_SVCRES_TMF_REJECTED) {
			scsicmd->result =
				DID_ERROR << 16 | MESSAGE_REJECT << 8;
		} else if (err->service_response ==
			HBA_RESP_SVCRES_TMF_LUN_INVALID) {
			scsicmd->result =
				DID_NO_CONNECT << 16 | COMMAND_COMPLETE << 8;
		} else if ((err->service_response ==
			HBA_RESP_SVCRES_TMF_COMPLETE) ||
			(err->service_response ==
			HBA_RESP_SVCRES_TMF_SUCCEEDED)) {
			scsicmd->result =
				DID_OK << 16 | COMMAND_COMPLETE << 8;
		} else {
			scsicmd->result =
				DID_ERROR << 16 | COMMAND_COMPLETE << 8;
		}
	}

	aac_fib_complete(fibptr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	aac_fib_free_tag(fibptr);
#else
	aac_fib_free(fibptr);
#endif

	if (fibptr->flags & FIB_CONTEXT_FLAG_NATIVE_HBA_TMF) {
		scsicmd->SCp.sent_command = 1;
	} else {
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		aac_io_done(scsicmd);
#else
		scsicmd->scsi_done(scsicmd);
#endif
	}
}

/**
 *
 * aac_send_scb_fib
 * @scsicmd: the scsi command block
 *
 * This routine will form a FIB and fill in the aac_srb from the
 * scsicmd passed in.
 */

static int aac_send_srb_fib(struct scsi_cmnd* scsicmd)
{
	struct fib* cmd_fibcontext;
	struct aac_dev* dev;
	int status;

	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
	if (scmd_id(scsicmd) >= dev->maximum_num_physicals ||
			scsicmd->device->lun > AAC_MAX_LUN - 1) {
		scsicmd->result = DID_NO_CONNECT << 16;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		__aac_io_done(scsicmd);
#else
		scsicmd->scsi_done(scsicmd);
#endif
		return 0;
	}

	/*
	 *	Allocate and initialize a Fib then setup a BlockWrite command
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (!(cmd_fibcontext = aac_fib_alloc_tag(dev, scsicmd))) {
#else
	if (!(cmd_fibcontext = aac_fib_alloc(dev))) {
#endif
		return -1;
	}

	status = aac_adapter_scsi(cmd_fibcontext, scsicmd);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	printk(KERN_WARNING "aac_srb: aac_fib_send failed with status: %d\n", status);
	aac_fib_complete(cmd_fibcontext);
	aac_fib_free(cmd_fibcontext);

	return -1;
}

/**
 *
 * aac_send_hba_fib
 * @scsicmd: the scsi command block
 *
 * This routine will form a FIB and fill in the aac_hba_cmd_req from the
 * scsicmd passed in.
 */

static int aac_send_hba_fib(struct scsi_cmnd* scsicmd)
{
	struct fib* cmd_fibcontext;
	struct aac_dev* dev;
	int status;

	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
	if (scmd_id(scsicmd) >= dev->maximum_num_physicals ||
			scsicmd->device->lun > AAC_MAX_LUN - 1) {
		scsicmd->result = DID_NO_CONNECT << 16;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING) || defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		__aac_io_done(scsicmd);
#else
		scsicmd->scsi_done(scsicmd);
#endif
		return 0;
	}

	/*
	 *	Allocate and initialize a Fib then setup a BlockWrite command
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (!(cmd_fibcontext = aac_fib_alloc_tag(dev, scsicmd))) {
#else
	if (!(cmd_fibcontext = aac_fib_alloc(dev))) {
#endif
		return -1;
	}

	status = aac_adapter_hba(cmd_fibcontext, scsicmd);

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) {
		scsicmd->SCp.phase = AAC_OWNER_FIRMWARE;
		return 0;
	}

	printk(KERN_WARNING "aac_hba_cmd_req: aac_fib_send failed with status: %d\n", status);
	aac_fib_complete(cmd_fibcontext);
	aac_fib_free(cmd_fibcontext);

	return -1;
}

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
static int aac_build_sg(struct scsi_cmnd* scsicmd, struct sgmap* psg)
#else
static long aac_build_sg(struct scsi_cmnd* scsicmd, struct sgmap* psg)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	struct Scsi_Host *host = scsicmd->device->host;
#endif
	struct aac_dev *dev;
	unsigned long byte_count = 0;
	int nseg;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	dev = (struct aac_dev *)host->hostdata;
#else
	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
#endif
	// Get rid of old data
	psg->count = 0;
	psg->sg[0].addr = 0;
	psg->sg[0].count = 0;

	nseg = scsi_dma_map(scsicmd);
	if (nseg < 0)
		return nseg;
	if (nseg) {
		struct scatterlist *sg;
		int i;
#if (defined(AAC_DEBUG_INSTRUMENT_SG) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)))
		int nseg_hold = nseg;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		psg->count = cpu_to_le32(nseg);

#endif
		scsi_for_each_sg(scsicmd, sg, nseg, i) {
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(AAC_DEBUG_INSTRUMENT_SG_PROBE) || defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
			int count = sg_dma_len(sg);
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
			u32 addr = sg_dma_address(sg);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
			char c = ((char *)sg->page + sg->offset)[0];
			c = ((char *)sg->page + sg->offset)[count-1];
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__))
			vmk_verify_memory_for_io(addr, count);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
			if (!(dev->adapter_info.options & AAC_OPT_NEW_COMM))
			while (count > 65536) {
				psg->sg[i].addr = cpu_to_le32(addr);
				psg->sg[i].count = cpu_to_le32(65536);
				++i;
				if (++nseg > host->sg_tablesize) {
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
					printk(KERN_INFO
					  "SG List[%d] too large based on original[%d]:\n",
					  nseg, nseg_hold);
					sg = (struct scatterlist *) scsicmd->request_buffer;
					for (i = 0; i < nseg_hold; i++) {
						printk(KERN_INFO "0x%llx[%d] ",
						  (u64)(sg_dma_address(sg)),
						  (int)(sg_dma_len(sg)));
						++sg;
					}
					printk(KERN_INFO "...\n");
#endif
					BUG();
				}
				byte_count += 65536;
				addr += 65536;
				count -= 65536;
			}

			psg->sg[i].addr = cpu_to_le32(addr);
			psg->sg[i].count = cpu_to_le32(count);
			byte_count += count;
#else
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
			psg->sg[i].addr = cpu_to_le32(addr);
#else
			psg->sg[i].addr = cpu_to_le32(sg_dma_address(sg));
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE) || defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
			psg->sg[i].count = cpu_to_le32(count);
#else
			psg->sg[i].count = cpu_to_le32(sg_dma_len(sg));
#endif
			byte_count += sg_dma_len(sg);
#endif
		}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		psg->count = cpu_to_le32(nseg);
#endif
		/* hba wants the size to be exact */
		if (byte_count > scsi_bufflen(scsicmd)) {
			u32 temp = le32_to_cpu(psg->sg[i-1].count) -
				(byte_count - scsi_bufflen(scsicmd));
			psg->sg[i-1].count = cpu_to_le32(temp);
			byte_count = scsi_bufflen(scsicmd);
		}
		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	else if(scsicmd->request_bufflen) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		int i, count;
#endif
		u32 addr;
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
		char c = ((char *)scsicmd->request_buffer)[0];
		c = ((char *)scsicmd->request_buffer)[scsicmd->request_bufflen-1];
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__x86_64__))
		scsicmd->SCp.dma_handle = scsicmd->request_bufferMA;
		vmk_verify_memory_for_io(scsicmd->request_bufferMA, scsicmd->request_bufflen);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		scsicmd->SCp.dma_handle = (char *)(uintptr_t)pci_map_single(dev->pdev,
#else
		scsicmd->SCp.dma_handle = pci_map_single(dev->pdev,
#endif
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsicmd->sc_data_direction);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		addr = (u32)(uintptr_t)scsicmd->SCp.dma_handle;
#else
		addr = scsicmd->SCp.dma_handle;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		count = scsicmd->request_bufflen;
		i = 0;
		if (!(dev->adapter_info.options & AAC_OPT_NEW_COMM))
		while (count > 65536) {
			psg->sg[i].addr = cpu_to_le32(addr);
			psg->sg[i].count = cpu_to_le32(65536);
			if (++i >= host->sg_tablesize) {
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
				printk(KERN_INFO
				  "SG List[%d] too large based on original single element %d in size\n",
				  i, scsicmd->request_bufflen);
#endif
				BUG();
			}
			addr += 65536;
			count -= 65536;
		}
		psg->count = cpu_to_le32(1+i);
		psg->sg[i].addr = cpu_to_le32(addr);
		psg->sg[i].count = cpu_to_le32(count);
#else
		psg->count = cpu_to_le32(1);
		psg->sg[0].addr = cpu_to_le32(addr);
		psg->sg[0].count = cpu_to_le32(scsicmd->request_bufflen);
#endif
		byte_count = scsicmd->request_bufflen;
	}
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
{
	int i, nseg = le32_to_cpu(psg->count);
	printk("aac_build_sg:");
	for (i = 0; i < nseg; i++) {
		int count = le32_to_cpu(psg->sg[i].count);
		u32 addr = le32_to_cpu(psg->sg[i].addr);
		printk(" %x[%d]", addr, count);
	}
	printk ("\n");
}
#endif

        adbg_debug_aac_config(scsicmd, psg->count, byte_count);

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	return le32_to_cpu(psg->count);
#else
	return byte_count;
#endif
}


#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
static int aac_build_sg64(struct scsi_cmnd* scsicmd, struct sgmap64* psg)
#else
static long aac_build_sg64(struct scsi_cmnd* scsicmd, struct sgmap64* psg)
#endif
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	struct Scsi_Host *host = scsicmd->device->host;
#endif
	struct aac_dev *dev;
	unsigned long byte_count = 0;
	u64 addr;
	int nseg;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	dev = (struct aac_dev *)host->hostdata;
#else
	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
#endif
	// Get rid of old data
	psg->count = 0;
	psg->sg[0].addr[0] = 0;
	psg->sg[0].addr[1] = 0;
	psg->sg[0].count = 0;

	nseg = scsi_dma_map(scsicmd);
	if (nseg < 0)
		return nseg;
	if (nseg) {
		struct scatterlist *sg;
		int i;
#if (defined(AAC_DEBUG_INSTRUMENT_SG) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)))
		int nseg_hold = nseg;
#endif

		scsi_for_each_sg(scsicmd, sg, nseg, i) {
			int count = sg_dma_len(sg);
			addr = sg_dma_address(sg);
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
			char c = ((char *)sg->page + sg->offset)[0];
			c = ((char *)sg->page + sg->offset)[count-1];
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__))
			vmk_verify_memory_for_io(addr, count);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
			if (!(dev->adapter_info.options & AAC_OPT_NEW_COMM))
			while (count > 65536) {
				psg->sg[i].addr[1] = cpu_to_le32((u32)(addr>>32));
				psg->sg[i].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
				psg->sg[i].count = cpu_to_le32(65536);
				++i;
				if (++nseg > host->sg_tablesize) {
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
					printk(KERN_INFO
					  "SG List[%d] too large based on original[%d]:\n",
					  nseg, nseg_hold);
					sg = (struct scatterlist *) scsicmd->request_buffer;
					for (i = 0; i < nseg_hold; i++) {
						printk(KERN_INFO "0x%llx[%d] ",
						  (u64)sg_dma_address(sg),
						  (int)sg_dma_len(sg));
						++sg;
					}
					printk(KERN_INFO "...\n");
#endif
					BUG();
				}
				byte_count += 65536;
				addr += 65536;
				count -= 65536;
			}
#endif
			psg->sg[i].addr[0] = cpu_to_le32(addr & 0xffffffff);
			psg->sg[i].addr[1] = cpu_to_le32(addr>>32);
			psg->sg[i].count = cpu_to_le32(count);
			byte_count += count;
		}
		psg->count = cpu_to_le32(nseg);
		/* hba wants the size to be exact */
		if (byte_count > scsi_bufflen(scsicmd)) {
			u32 temp = le32_to_cpu(psg->sg[i-1].count) -
				(byte_count - scsi_bufflen(scsicmd));
			psg->sg[i-1].count = cpu_to_le32(temp);
			byte_count = scsi_bufflen(scsicmd);
		}
		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	else if(scsicmd->request_bufflen) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		int i;
		int count;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
		char c = ((char *)scsicmd->request_buffer)[0];
		c = ((char *)scsicmd->request_buffer)[scsicmd->request_bufflen-1];
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__x86_64__))
		scsicmd->SCp.dma_handle = scsicmd->request_bufferMA;
		vmk_verify_memory_for_io(scsicmd->request_bufferMA, scsicmd->request_bufflen);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		scsicmd->SCp.dma_handle = (char *)(uintptr_t)pci_map_single(dev->pdev,
#else
		scsicmd->SCp.dma_handle = pci_map_single(dev->pdev,
#endif
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsicmd->sc_data_direction);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		addr = (u64)(uintptr_t)scsicmd->SCp.dma_handle;
#else
		addr = scsicmd->SCp.dma_handle;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		count = scsicmd->request_bufflen;
		i = 0;
		if (!(dev->adapter_info.options & AAC_OPT_NEW_COMM))
		while (count > 65536) {
			psg->sg[i].addr[1] = cpu_to_le32((u32)(addr>>32));
			psg->sg[i].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
			psg->sg[i].count = cpu_to_le32(65536);
			if (++i >= host->sg_tablesize) {
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
				printk(KERN_INFO
				  "SG List[%d] too large based on original single element %d in size\n",
				  i, scsicmd->request_bufflen);
#endif
				BUG();
			}
			addr += 65536;
			count -= 65536;
		}
		psg->count = cpu_to_le32(1+i);
		psg->sg[i].addr[0] = cpu_to_le32(addr & 0xffffffff);
		psg->sg[i].addr[1] = cpu_to_le32(addr >> 32);
		psg->sg[i].count = cpu_to_le32(count);
#else
		psg->count = cpu_to_le32(1);
		psg->sg[0].addr[0] = cpu_to_le32(addr & 0xffffffff);
		psg->sg[0].addr[1] = cpu_to_le32(addr >> 32);
		psg->sg[0].count = cpu_to_le32(scsicmd->request_bufflen);
#endif
		byte_count = scsicmd->request_bufflen;
	}
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
{
	int i, nseg = le32_to_cpu(psg->count);
	printk("aac_build_sg64:");
	for (i = 0; i < nseg; i++) {
		int count = le32_to_cpu(psg->sg[i].count);
		u32 addr0 = le32_to_cpu(psg->sg[i].addr[0]);
		u32 addr1 = le32_to_cpu(psg->sg[i].addr[1]);
		if (addr1 == 0)
			printk(" %x[%d]", addr0, count);
		else
			printk(" %x%08x[%d]", addr1, addr0, count);
	}
	printk ("\n");
}
#endif

        adbg_debug_aac_config(scsicmd, psg->count, byte_count);

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	return le32_to_cpu(psg->count);
#else
	return byte_count;
#endif
}

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
static int aac_build_sgraw(struct scsi_cmnd* scsicmd, struct sgmapraw* psg)
#else
static long aac_build_sgraw(struct scsi_cmnd* scsicmd, struct sgmapraw* psg)
#endif
{
	unsigned long byte_count = 0;
	int nseg;

	// Get rid of old data
	psg->count = 0;
	psg->sg[0].next = 0;
	psg->sg[0].prev = 0;
	psg->sg[0].addr[0] = 0;
	psg->sg[0].addr[1] = 0;
	psg->sg[0].count = 0;
	psg->sg[0].flags = 0;

	nseg = scsi_dma_map(scsicmd);
	if (nseg < 0)
		return nseg;
	if (nseg) {
		struct scatterlist *sg;
		int i;

		scsi_for_each_sg(scsicmd, sg, nseg, i) {
			int count = sg_dma_len(sg);
			u64 addr = sg_dma_address(sg);
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
			char c = ((char *)sg->page + sg->offset)[0];
			c = ((char *)sg->page + sg->offset)[count-1];
#endif
			psg->sg[i].next = 0;
			psg->sg[i].prev = 0;
			psg->sg[i].addr[1] = cpu_to_le32((u32)(addr>>32));
			psg->sg[i].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
			psg->sg[i].count = cpu_to_le32(count);
			psg->sg[i].flags = 0;
			byte_count += count;
		}
		psg->count = cpu_to_le32(nseg);
		/* hba wants the size to be exact */
		if (byte_count > scsi_bufflen(scsicmd)) {
			u32 temp = le32_to_cpu(psg->sg[i-1].count) -
				(byte_count - scsi_bufflen(scsicmd));
			psg->sg[i-1].count = cpu_to_le32(temp);
			byte_count = scsi_bufflen(scsicmd);
		}
		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	else if(scsicmd->request_bufflen) {
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
		struct aac_dev *dev = (struct aac_dev *)scsicmd->device->host->hostdata;
#endif
		int count;
		u64 addr;
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
		char c = ((char *)scsicmd->request_buffer)[0];
		c = ((char *)scsicmd->request_buffer)[scsicmd->request_bufflen-1];
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__x86_64__))
		scsicmd->SCp.dma_handle = scsicmd->request_bufferMA;
		vmk_verify_memory_for_io(scsicmd->request_bufferMA, scsicmd->request_bufflen);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		scsicmd->SCp.dma_handle = (char *)(uintptr_t)pci_map_single(dev->pdev,
#else
		scsicmd->SCp.dma_handle = pci_map_single(dev->pdev,
#endif
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsicmd->sc_data_direction);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		addr = (u64)(uintptr_t)scsicmd->SCp.dma_handle;
#else
		addr = scsicmd->SCp.dma_handle;
#endif
		count = scsicmd->request_bufflen;
		psg->count = cpu_to_le32(1);
		psg->sg[0].next = 0;
		psg->sg[0].prev = 0;
		psg->sg[0].addr[1] = cpu_to_le32((u32)(addr>>32));
		psg->sg[0].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
		psg->sg[0].count = cpu_to_le32(count);
		psg->sg[0].flags = 0;
		byte_count = scsicmd->request_bufflen;
	}
#endif

        adbg_debug_aac_config(scsicmd, psg->count, byte_count);

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	return le32_to_cpu(psg->count);
#else
	return byte_count;
#endif
}

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
static int aac_build_sgraw2(struct scsi_cmnd* scsicmd, struct aac_raw_io2* rio2, int sg_max)
#else
static long aac_build_sgraw2(struct scsi_cmnd* scsicmd, struct aac_raw_io2* rio2, int sg_max)
#endif
{
	unsigned long byte_count = 0;
	int nseg;

	nseg = scsi_dma_map(scsicmd);
	if (nseg < 0)
		return nseg;
	if (nseg) {
		struct scatterlist *sg;
		int i, conformable = 0;
		u32 min_size = PAGE_SIZE, cur_size;

		scsi_for_each_sg(scsicmd, sg, nseg, i) {
			int count = sg_dma_len(sg);
			u64 addr = sg_dma_address(sg);
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
			char c = ((char *)sg->page + sg->offset)[0];
			c = ((char *)sg->page + sg->offset)[count-1];
#endif
			BUG_ON(i >= sg_max);
			rio2->sge[i].addrHigh = cpu_to_le32((u32)(addr>>32));
			rio2->sge[i].addrLow = cpu_to_le32((u32)(addr & 0xffffffff));
			cur_size = cpu_to_le32(count);
			rio2->sge[i].length = cur_size;
			rio2->sge[i].flags = 0;
			if (i == 0) {
				conformable = 1;
				rio2->sgeFirstSize = cur_size;
			} else if (i == 1) {
				rio2->sgeNominalSize = cur_size;
				min_size = cur_size;
			} else if ((i+1) < nseg && cur_size != rio2->sgeNominalSize) {
				conformable = 0;
				if (cur_size < min_size)
					min_size = cur_size;
			}
			byte_count += count;
		}

		/* hba wants the size to be exact */
		if (byte_count > scsi_bufflen(scsicmd)) {
			u32 temp = le32_to_cpu(rio2->sge[i-1].length) -
				(byte_count - scsi_bufflen(scsicmd));
			rio2->sge[i-1].length = cpu_to_le32(temp);
			byte_count = scsi_bufflen(scsicmd);
		}

		rio2->sgeCnt = cpu_to_le32(nseg);
		rio2->flags |= cpu_to_le16(RIO2_SG_FORMAT_IEEE1212);
		/* not conformable: evaluate required sg elements */
		if (!conformable) {
			int j, nseg_new = nseg, err_found;
			for (i = min_size / PAGE_SIZE; i >= 1; --i) {
				err_found = 0;
				nseg_new = 2;
				for (j = 1; j < nseg - 1; ++j) {
					if (rio2->sge[j].length % (i*PAGE_SIZE)) {
						err_found = 1;
						break;
					}
					nseg_new += (rio2->sge[j].length / (i*PAGE_SIZE));
				}
				if (!err_found)
					break;
			}
			if (i > 0 && nseg_new <= sg_max)
				aac_convert_sgraw2(rio2, i, nseg, nseg_new);
		} else
			rio2->flags |= cpu_to_le16(RIO2_SGL_CONFORMANT);

		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	else if(scsicmd->request_bufflen) {
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
		struct aac_dev *dev = (struct aac_dev *)scsicmd->device->host->hostdata;
#endif
		int count;
		u64 addr;
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
		char c = ((char *)scsicmd->request_buffer)[0];
		c = ((char *)scsicmd->request_buffer)[scsicmd->request_bufflen-1];
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__x86_64__))
		scsicmd->SCp.dma_handle = scsicmd->request_bufferMA;
		vmk_verify_memory_for_io(scsicmd->request_bufferMA, scsicmd->request_bufflen);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		scsicmd->SCp.dma_handle = (char *)(uintptr_t)pci_map_single(dev->pdev,
#else
		scsicmd->SCp.dma_handle = pci_map_single(dev->pdev,
#endif
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsicmd->sc_data_direction);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		addr = (u64)(uintptr_t)scsicmd->SCp.dma_handle;
#else
		addr = scsicmd->SCp.dma_handle;
#endif
		count = scsicmd->request_bufflen;
		rio2->sgeCnt = cpu_to_le32(1);
		rio2->sge[0].addrHigh = cpu_to_le32((u32)(addr>>32));
		rio2->sge[0].addrLow = cpu_to_le32((u32)(addr & 0xffffffff));
		rio2->sge[0].length = cpu_to_le32(count);
		rio2->sge[0].flags = 0;
		rio2->sgeFirstSize = cpu_to_le32(count);
		rio2->flags |= cpu_to_le16(RIO2_SGL_CONFORMANT|RIO2_SG_FORMAT_IEEE1212);
		byte_count = scsicmd->request_bufflen;
	}
#endif

        adbg_debug_aac_config(scsicmd, rio2->byteCount, byte_count);

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	return le32_to_cpu(psg->count);
#else
	return byte_count;
#endif
}

static int aac_convert_sgraw2(struct aac_raw_io2* rio2, int pages, int nseg, int nseg_new)
{
	struct sge_ieee1212 *sge;
	int i, j, pos;
	u32 addr_low;

	if (aac_convert_sgl == 0)
		return 0;

	sge = kmalloc(nseg_new * sizeof(struct sge_ieee1212), GFP_ATOMIC);
	if (sge == NULL)
		return -1;

	for (i = 1, pos = 1; i < nseg-1; ++i) {
		for (j = 0; j < rio2->sge[i].length / (pages * PAGE_SIZE); ++j) {
			addr_low = rio2->sge[i].addrLow + j * pages * PAGE_SIZE;
			sge[pos].addrLow = addr_low;
			sge[pos].addrHigh = rio2->sge[i].addrHigh;
			if (addr_low < rio2->sge[i].addrLow)
				sge[pos].addrHigh++;
			sge[pos].length = pages * PAGE_SIZE;
			sge[pos].flags = 0;
			pos++;
		}
	}
	sge[pos] = rio2->sge[nseg-1];
	memcpy(&rio2->sge[1], &sge[1], (nseg_new-1)*sizeof(struct sge_ieee1212));

	kfree(sge);
	rio2->sgeCnt = cpu_to_le32(nseg_new);
	rio2->flags |= cpu_to_le16(RIO2_SGL_CONFORMANT);
	rio2->sgeNominalSize = pages * PAGE_SIZE;
	return 0;
}

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
static int aac_build_sghba(struct scsi_cmnd* scsicmd, struct aac_hba_cmd_req * hbacmd, int sg_max, u64 sg_address)
#else
static long aac_build_sghba(struct scsi_cmnd* scsicmd, struct aac_hba_cmd_req * hbacmd, int sg_max, u64 sg_address)
#endif
{
	unsigned long byte_count = 0;
	int nseg;

	nseg = scsi_dma_map(scsicmd);
	if (nseg < 0)
		return nseg;
	if (nseg) {
		struct scatterlist *sg;
		int i;
		u32 cur_size;
		struct aac_hba_sgl *sge;

		if (nseg > HBA_MAX_SG_EMBEDDED)
			sge = &hbacmd->sge[2];
		else
			sge = &hbacmd->sge[0];
		scsi_for_each_sg(scsicmd, sg, nseg, i) {
			int count = sg_dma_len(sg);
			u64 addr = sg_dma_address(sg);
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
			char c = ((char *)sg->page + sg->offset)[0];
			c = ((char *)sg->page + sg->offset)[count-1];
#endif
			BUG_ON(i >= sg_max);
			sge->addr_hi = cpu_to_le32((u32)(addr>>32));
			sge->addr_lo = cpu_to_le32((u32)(addr & 0xffffffff));
			cur_size = cpu_to_le32(count);
			sge->len = cur_size;
			sge->flags = 0;
			byte_count += count;
			sge++;
		}

		sge--;
		/* hba wants the size to be exact */
		if (byte_count > scsi_bufflen(scsicmd)) {
			u32 temp = le32_to_cpu(sge->len) -
				(byte_count - scsi_bufflen(scsicmd));
			sge->len = cpu_to_le32(temp);
			byte_count = scsi_bufflen(scsicmd);
		}

		if (nseg <= HBA_MAX_SG_EMBEDDED) {
			hbacmd->emb_data_desc_count = cpu_to_le32(nseg);
			sge->flags = cpu_to_le32(0x40000000);
		} else {
			/* not embedded */
			hbacmd->sge[0].flags = cpu_to_le32(0x80000000);
			hbacmd->emb_data_desc_count = cpu_to_le32(1);
			hbacmd->sge[0].addr_hi =
				cpu_to_le32((u32)(sg_address >> 32));
			hbacmd->sge[0].addr_lo =
				cpu_to_le32((u32)(sg_address & 0xffffffff));
		}

		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	else if(scsicmd->request_bufflen) {
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
		struct aac_dev *dev = (struct aac_dev *)scsicmd->device->host->hostdata;
#endif
		int count;
		u64 addr;
#if (defined(AAC_DEBUG_INSTRUMENT_SG_PROBE))
		char c = ((char *)scsicmd->request_buffer)[0];
		c = ((char *)scsicmd->request_buffer)[scsicmd->request_bufflen-1];
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__x86_64__))
		scsicmd->SCp.dma_handle = scsicmd->request_bufferMA;
		vmk_verify_memory_for_io(scsicmd->request_bufferMA, scsicmd->request_bufflen);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		scsicmd->SCp.dma_handle = (char *)(uintptr_t)pci_map_single(dev->pdev,
#else
		scsicmd->SCp.dma_handle = pci_map_single(dev->pdev,
#endif
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsicmd->sc_data_direction);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		addr = (u64)(uintptr_t)scsicmd->SCp.dma_handle;
#else
		addr = scsicmd->SCp.dma_handle;
#endif
		count = scsicmd->request_bufflen;
		hbacmd->emb_data_desc_count = cpu_to_le32(1);
		hbacmd->sge[0].addr_hi = cpu_to_le32((u32)(addr>>32));
		hbacmd->sge[0].addr_lo = cpu_to_le32((u32)(addr & 0xffffffff));
		hbacmd->sge[0].len = cpu_to_le32(count);
		hbacmd->sge[0].flags = cpu_to_le32(0x40000000);
		byte_count = scsicmd->request_bufflen;
	}
#endif

        adbg_debug_aac_config(scsicmd, hbacmd->data_length, byte_count);

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	return le32_to_cpu(psg->count);
#else
	return byte_count;
#endif
}

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

int aac_build_sg_nark(struct fib * fib, struct scsi_cmnd* scsicmd, union aac_apre_embedded_sglist* psg)
{
	struct aac_dev *dev = fib->dev;
	struct aac_apre_element_nark * sglist = &(psg->nark.FirstElement);
	unsigned long byte_count = 0;

        int nseg = scsi_dma_map(scsicmd);
	BUG_ON(nseg < 0);

	if (nseg) {
		struct scatterlist *sg;
		int i;

		scsi_for_each_sg(scsicmd, sg, nseg, i) {
			int count = sg_dma_len(sg);
			u64 addr;
			if (i == (sizeof(psg->nark)/sizeof(*sglist))) {
				int Index = fib->hw_fib_va - dev->hw_fib_va;
				struct hw_apre * frame = (struct hw_apre *)&dev->hw_fib_va[Index];
				sglist = (struct aac_apre_element_nark *)frame->sg;
				*((union aac_apre_embedded_sglist *)frame->sg) = *psg;
				addr = fib->hw_fib_pa + offsetof(struct hw_apre, sg[0]);
				psg->nark.FirstElement.physAddrLow = cpu_to_le32((u32)(addr & 0xffffffff));
				psg->nark.FirstElement.physAddrHigh = cpu_to_le32((u32)(addr>>32));
				psg->nark.FirstElement.elementByteCount = cpu_to_le32(sg_count * sizeof(psg->nark.FirstElement));
				psg->nark.FirstElement.elementType = APRE_BUFFER_DESC_POINTER;
			}
			addr = sg_dma_address(sg);
			sglist->physAddrLow = cpu_to_le32((u32)(addr & 0xffffffff));
			sglist->physAddrHigh = cpu_to_le32((u32)(addr>>32));
			sglist->elementByteCount = cpu_to_le32(count);
			sglist->domainSelect = APRE_OFF_CHIP_MEM_DOMAIN;
			sglist->elementType = APRE_BUFFER_DESC_ENTRY;
			byte_count += count;
			sglist++;
		}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	} else if (scsicmd->request_bufflen) {
		int count;
		u64 addr;
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__x86_64__))
		scsicmd->SCp.dma_handle = scsicmd->request_bufferMA;
		vmk_verify_memory_for_io(scsicmd->request_bufferMA, scsicmd->request_bufflen);
#else
		scsicmd->SCp.dma_handle = pci_map_single(dev->pdev,
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsicmd->sc_data_direction);
#endif
		addr = scsicmd->SCp.dma_handle;
		count = scsicmd->request_bufflen;
		byte_count = count;
		sg_count = 1;
		sglist->physAddrLow = cpu_to_le32((u32)(addr & 0xffffffff));
		sglist->physAddrHigh = cpu_to_le32((u32)(addr>>32));
		sglist->elementByteCount = cpu_to_le32(count);
		sglist->domainSelect = APRE_OFF_CHIP_MEM_DOMAIN;
		sglist->elementType = APRE_BUFFER_DESC_ENTRY;
	}
#endif

        adbg_debug_aac_config(scsicmd, sg_count, byte_count);

	return sg_count;
}

int aac_build_sg_rx(struct fib * fib, struct scsi_cmnd* scsicmd, union aac_apre_embedded_sglist* psg)
{
	return 0;
}
#endif
#if (defined(INITFLAGS_BEDROC_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
int aac_build_sg_bedroc(struct fib * fib, struct scsi_cmnd* scsicmd, struct sgmap64* psg)
{
	struct SGformat {
		__le32	TotalSize:12;
		__le32	PageSize:4;  /* Page size in Power of two times 512 */
		__le32	MaximumEntries:8;
		__le32	AddressBE:1;
		__le32	LengthBE:1;
		__le32	EndBitEnable:1;
		__le32	EndBitPre:1; /* place end bit in 2nd to last
					entry for pipelines, rather than
					in last entry */
		__le32	LinkBitEnable:1;
		__le32	LinkBitPre:1; /* place end bit in 2nd to last entry */
		__le32	Version:2;	/* Zero */
		__le32	AddressSize:12;	/* Address SG field size in # bytes */
		__le32	AddressReserved:2;
		__le32	AddressBoundary:1: /* Address must be on page
					      boundary (except first) */
		__le32	AddressPage:1;	/* Address is page number */
		__le32	AddressOffset:16;/* Address SG field offset # bytes */
		__le32	LengthSize:12;	 /* Length SG field size in # bytes */
		__le32	LengthReserved:3;
		__le32	LengthBoundary:1; /* Length must not exceed page size */
		__le32	LengthPage:1;     /* Length is number of pages */
		__le32	LengthOffset:16;  /* Length SG field offset # bytes */
		__le31	EndBitLocationBit:3; /* End bit field with the byte */
		__le32	EndBitLocationOffset:12; /* End bit field offset in
						    # of bytes, must be less
						    less than TotalSize, upper
						    bits above that ignored */
		__le32	EndBitSignedness:1;	/* 0 indicates 1 is active */
		__le31	LinkBitLocationBit:3; /* Link bit field with the byte */
		__le32	LinkBitLocationOffset:12; /* Link bit field offset */
		__le32	LinkBitSignedness:1;	/* 0 indicates 1 is active */
	};

	dev->SGformat
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	struct Scsi_Host *host = scsicmd->device->host;
#endif
	struct aac_dev *dev;
	unsigned long byte_count = 0;
	u64 addr;
	int nseg;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	dev = (struct aac_dev *)host->hostdata;
#else
	dev = (struct aac_dev *)scsicmd->device->host->hostdata;
#endif
	// Get rid of old data
	psg->count = 0;
	memset(&psg->sg[0], 0, dev->SGformat.TotalSize);
	nseg = scsi_dma_map(scsicmd);
	BUG_ON(nseg < 0);
	if (nseg) {
		struct scatterlist *sg;
		int i;
#if (defined(AAC_DEBUG_INSTRUMENT_SG) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)))
		int nseg_hold = nseg;
#endif

		scsi_for_each_sg(scsicmd, sg, nseg, i) {
			int count = sg_dma_len(sg);
			addr = sg_dma_address(sg);
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__))
			vmk_verify_memory_for_io(addr, count);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
			if (!(dev->adapter_info.options & AAC_OPT_NEW_COMM))
			while (count > 65536) {
				psg->sg[i].addr[1] = cpu_to_le32((u32)(addr>>32));
				psg->sg[i].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
				psg->sg[i].count = cpu_to_le32(65536);
				++i;
				if (++nseg > host->sg_tablesize) {
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
					printk(KERN_INFO
					  "SG List[%d] too large based on original[%d]:\n",
					  nseg, nseg_hold);
					sg = (struct scatterlist *) scsicmd->request_buffer;
					for (i = 0; i < nseg_hold; i++) {
						printk(KERN_INFO "0x%llx[%d] ",
						  (u64)sg_dma_address(sg),
						  (int)sg_dma_len(sg));
						++sg;
					}
					printk(KERN_INFO "...\n");
#endif
					BUG();
				}
				byte_count += 65536;
				addr += 65536;
				count -= 65536;
			}
#endif
			psg->sg[i].addr[0] = cpu_to_le32(addr & 0xffffffff);
			psg->sg[i].addr[1] = cpu_to_le32(addr>>32);
			psg->sg[i].count = cpu_to_le32(count);
			byte_count += count;
		}
		psg->count = cpu_to_le32(nseg);
		/* hba wants the size to be exact */
		if(byte_count > scsi_bufflen(scsicmd)) {
			u32 temp = le32_to_cpu(psg->sg[i-1].count) -
				(byte_count - scsi_bufflen(scsicmd));
			psg->sg[i-1].count = cpu_to_le32(temp);
			byte_count = scsi_bufflen(scsicmd);
		}
		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
	else if(scsicmd->request_bufflen) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		int i;
		int count;
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__x86_64__))
		scsicmd->SCp.dma_handle = scsicmd->request_bufferMA;
		vmk_verify_memory_for_io(scsicmd->request_bufferMA, scsicmd->request_bufflen);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		scsicmd->SCp.dma_handle = (char *)(uintptr_t)pci_map_single(dev->pdev,
#else
		scsicmd->SCp.dma_handle = pci_map_single(dev->pdev,
#endif
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsicmd->sc_data_direction);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
		addr = (u64)(uintptr_t)scsicmd->SCp.dma_handle;
#else
		addr = scsicmd->SCp.dma_handle;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		count = scsicmd->request_bufflen;
		i = 0;
		if (!(dev->adapter_info.options & AAC_OPT_NEW_COMM))
		while (count > 65536) {
			psg->sg[i].addr[1] = cpu_to_le32((u32)(addr>>32));
			psg->sg[i].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));
			psg->sg[i].count = cpu_to_le32(65536);
			if (++i >= host->sg_tablesize) {
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
				printk(KERN_INFO
				  "SG List[%d] too large based on original single element %d in size\n",
				  i, scsicmd->request_bufflen);
#endif
				BUG();
			}
			addr += 65536;
			count -= 65536;
		}
		psg->count = cpu_to_le32(1+i);
		psg->sg[i].addr[0] = cpu_to_le32(addr & 0xffffffff);
		psg->sg[i].addr[1] = cpu_to_le32(addr >> 32);
		psg->sg[i].count = cpu_to_le32(count);
#else
		psg->count = cpu_to_le32(1);
		psg->sg[0].addr[0] = cpu_to_le32(addr & 0xffffffff);
		psg->sg[0].addr[1] = cpu_to_le32(addr >> 32);
		psg->sg[0].count = cpu_to_le32(scsicmd->request_bufflen);
#endif
		byte_count = scsicmd->request_bufflen;
	}
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SG))
{
	int i, nseg = le32_to_cpu(psg->count);
	printk("aac_build_sg64:");
	for (i = 0; i < nseg; i++) {
		int count = le32_to_cpu(psg->sg[i].count);
		u32 addr0 = le32_to_cpu(psg->sg[i].addr[0]);
		u32 addr1 = le32_to_cpu(psg->sg[i].addr[1]);
		if (addr1 == 0)
			printk(" %x[%d]", addr0, count);
		else
			printk(" %x%08x[%d]", addr1, addr0, count);
	}
	printk ("\n");
}
#endif

        adbg_debug_aac_config(scsicmd, psg->count, byte_count);

	return le32_to_cpu(psg->count);
	return 0;
}
#endif

#ifdef AAC_DETAILED_STATUS_INFO

struct aac_srb_status_info {
	u32	status;
	char	*str;
};


static struct aac_srb_status_info srb_status_info[] = {
	{ SRB_STATUS_PENDING,		"Pending Status"},
	{ SRB_STATUS_SUCCESS,		"Success"},
	{ SRB_STATUS_ABORTED,		"Aborted Command"},
	{ SRB_STATUS_ABORT_FAILED,	"Abort Failed"},
	{ SRB_STATUS_ERROR,		"Error Event"},
	{ SRB_STATUS_BUSY,		"Device Busy"},
	{ SRB_STATUS_INVALID_REQUEST,	"Invalid Request"},
	{ SRB_STATUS_INVALID_PATH_ID,	"Invalid Path ID"},
	{ SRB_STATUS_NO_DEVICE,		"No Device"},
	{ SRB_STATUS_TIMEOUT,		"Timeout"},
	{ SRB_STATUS_SELECTION_TIMEOUT,	"Selection Timeout"},
	{ SRB_STATUS_COMMAND_TIMEOUT,	"Command Timeout"},
	{ SRB_STATUS_MESSAGE_REJECTED,	"Message Rejected"},
	{ SRB_STATUS_BUS_RESET,		"Bus Reset"},
	{ SRB_STATUS_PARITY_ERROR,	"Parity Error"},
	{ SRB_STATUS_REQUEST_SENSE_FAILED,"Request Sense Failed"},
	{ SRB_STATUS_NO_HBA,		"No HBA"},
	{ SRB_STATUS_DATA_OVERRUN,	"Data Overrun/Data Underrun"},
	{ SRB_STATUS_UNEXPECTED_BUS_FREE,"Unexpected Bus Free"},
	{ SRB_STATUS_PHASE_SEQUENCE_FAILURE,"Phase Error"},
	{ SRB_STATUS_BAD_SRB_BLOCK_LENGTH,"Bad Srb Block Length"},
	{ SRB_STATUS_REQUEST_FLUSHED,	"Request Flushed"},
	{ SRB_STATUS_DELAYED_RETRY,	"Delayed Retry"},
	{ SRB_STATUS_INVALID_LUN,	"Invalid LUN"},
	{ SRB_STATUS_INVALID_TARGET_ID,	"Invalid TARGET ID"},
	{ SRB_STATUS_BAD_FUNCTION,	"Bad Function"},
	{ SRB_STATUS_ERROR_RECOVERY,	"Error Recovery"},
	{ SRB_STATUS_NOT_STARTED,	"Not Started"},
	{ SRB_STATUS_NOT_IN_USE,	"Not In Use"},
	{ SRB_STATUS_FORCE_ABORT,	"Force Abort"},
	{ SRB_STATUS_DOMAIN_VALIDATION_FAIL,"Domain Validation Failure"},
	{ 0xff,				"Unknown Error"}
};

char *aac_get_status_string(u32 status)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(srb_status_info); i++)
		if (srb_status_info[i].status == status)
			return srb_status_info[i].str;

	return "Bad Status Code";
}

#endif

void aac_simulate_scsi_error(struct aac_dev *dev, struct hw_fib *hw_fib)
{
	struct aac_hba_resp *err =
		&((struct aac_native_hba *)hw_fib)->resp.err;

	err->iu_type = HBA_IU_TYPE_RESP;
	err->service_response = HBA_RESP_SVCRES_TASK_COMPLETE;
	err->residual_count = 0;

	if (dev->simulated_scsi_error & 0x01) {
		err->status = SAM_STAT_CHECK_CONDITION;
		err->datapres = 0x02; /* Indicate Sense Data */
		err->sense_response_data_len = 0x08;
		err->sense_response_buf[0] = 0x72; /* Descriptor Sense Data */
		err->sense_response_buf[1] = 0x05; /* Illegal Request */
		err->sense_response_buf[2] = 0x24; /* ASC: Invalid field in the CDB */
		err->sense_response_buf[3] = 0x00; /* ASCQ */
		err->sense_response_buf[4] = 0x00; /* Reserved */
		err->sense_response_buf[5] = 0x00; /* Reserved */
		err->sense_response_buf[6] = 0x00; /* Reserved */
		err->sense_response_buf[7] = 0x00; /* Additional Sense Length */
	} else if (dev->simulated_scsi_error & 0x02) {
		err->status = SAM_STAT_BUSY;
		err->datapres = 0x00; /* No Data */
		err->sense_response_data_len = 0x00;
	} else if (dev->simulated_scsi_error & 0x04) {
		err->status = SAM_STAT_RESERVATION_CONFLICT;
		err->datapres = 0x00; /* No Data */
		err->sense_response_data_len = 0x00;
	} else if (dev->simulated_scsi_error & 0x08) {
		err->status = SAM_STAT_TASK_SET_FULL;
		err->datapres = 0x00; /* No Data */
		err->sense_response_data_len = 0x00;
	} else if (dev->simulated_scsi_error & 0x10) {
		err->status = SAM_STAT_TASK_ABORTED;
		err->datapres = 0x00; /* No Data */
		err->sense_response_data_len = 0x00;
	}
}

void aac_simulate_tgt_failure(struct aac_dev *dev, struct hw_fib *hw_fib)
{
	struct aac_hba_resp *err =
		&((struct aac_native_hba *)hw_fib)->resp.err;

	err->iu_type = HBA_IU_TYPE_RESP;
	err->service_response = HBA_RESP_SVCRES_FAILURE;
	err->datapres = 0;

	if (dev->simulated_tgt_failure & 0x01) {
		err->status = HBA_RESP_STAT_HBAMODE_DISABLED;
	} else if (dev->simulated_tgt_failure & 0x02) {
		err->status = HBA_RESP_STAT_IO_ERROR;
		err->sense_response_data_len = 0;
	} else if (dev->simulated_tgt_failure & 0x04) {
		err->status = HBA_RESP_STAT_IO_ABORTED;
		err->sense_response_data_len = 0;
	} else if (dev->simulated_tgt_failure & 0x08) {
		err->status = HBA_RESP_STAT_NO_PATH_TO_DEVICE;
		err->sense_response_data_len = 0;
	} else if (dev->simulated_tgt_failure & 0x10) {
		err->status = HBA_RESP_STAT_INVALID_DEVICE;
		err->sense_response_data_len = 0;
	} else if (dev->simulated_tgt_failure & 0x20) {
		err->status = HBA_RESP_STAT_UNDERRUN;
		err->residual_count = 1;
		err->sense_response_data_len -= 1;
	} else if (dev->simulated_tgt_failure & 0x40) {
		err->status = HBA_RESP_STAT_OVERRUN;
		err->residual_count = 1;
		err->sense_response_data_len -= 1;
	}
}


