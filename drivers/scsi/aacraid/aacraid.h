#undef AAC_DRIVER_BRANCH
#define AAC_DRIVER_BRANCH "custom"
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
 *  aacraid.h
 *
 * Abstract: Contains all routines for control of the aacraid driver
 *
 */

#ifndef _AACRAID_H_
#define _AACRAID_H_

#define fwprintf(x) aac_fw_printf x
//#define dprintk(x) printk x
#if (!defined(dprintk))
# define dprintk(x)
#endif
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#if (defined(BOOTCD))
#define fwprintf(x) aac_fw_printf x
#endif
#if (!defined(fwprintf) || defined(HAS_BOOT_CONFIG))
# undef fwprintf
# define fwprintf(x)
#endif
#endif
//#define AAC_DETAILED_STATUS_INFO
//#define AAC_DEBUG_INSTRUMENT_INIT
//#define AAC_DEBUG_INSTRUMENT_SETUP
//#define AAC_DEBUG_INSTRUMENT_TIMING
//#define AAC_DEBUG_INSTRUMENT_AIF
//#define AAC_DEBUG_INSTRUMENT_IOCTL
//#define AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB 0x0030
//#define AAC_DEBUG_INSTRUMENT_AAC_CONFIG
//#define AAC_DEBUG_INSTRUMENT_RESET
//#define AAC_DEBUG_INSTRUMENT_FIB
//#define AAC_DEBUG_INSTRUMENT_CONTEXT
//#define AAC_DEBUG_INSTRUMENT_2TB
//#define AAC_DEBUG_INSTRUMENT_SENDFIB
//#define AAC_DEBUG_INSTRUMENT_IO
//#define AAC_DEBUG_INSTRUMENT_PENDING
//#define AAC_DEBUG_INSTRUMENT_SG
//#define AAC_DEBUG_INSTRUMENT_SG_PROBE
//#define AAC_DEBUG_INSTRUMENT_VM_NAMESERVE
//#define AAC_DEBUG_INSTRUMENT_SERIAL
//#define AAC_DEBUG_INSTRUMENT_SYNCHRONIZE
//#define AAC_DEBUG_INSTRUMENT_SHUTDOWN
//#define AAC_DEBUG_INSTRUMENT_MSIX

#if ( ((defined(RHEL_MAJOR) && RHEL_MAJOR == 7 && RHEL_MINOR >= 2)) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)))
#define SERVICE_ACTION_IN SERVICE_ACTION_IN_16
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
#define strnicmp(x,y,z)  strncasecmp(x,y,z)
#endif

/* eg: if (nblank(dprintk(x))) */
#define _nblank(x) #x
#define nblank(x) _nblank(x)[0]

/* preprocessor defines for ESX35/ESX40 because
 * the check is no longer in the Makefile 
 */
#if defined(__VMKLNX__)
#define PCI_HAS_SHUTDOWN
#define PCI_HAS_ENABLE_MSI
#define PCI_HAS_DISABLE_MSI
#define SCSI_HAS_SSLEEP
#define SCSI_HAS_SCSI_DEVICE_ONLINE
#define HAS_NO_SETUP
#define SCSI_HAS_SHOST_STATE_ENUM
#define HAS_BITWISE_TYPE
#define HAS_SECTOR_T
#define HAS_MSLEEP
#elif (defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__))
#define HAS_NO_SETUP
#endif

/* changes for Smart HBA */
#define	AAC_MAX_NATIVE_TARGETS		1024
/* Thor: 5 phys. buses: #0: empty, 1-4: 256 targets each */
#define AAC_MAX_BUSES			5
#define AAC_MAX_TARGETS			256
#define AAC_MAX_NATIVE_SIZE		2048
#define FW_ERROR_BUFFER_SIZE		512

/* Thor AIF events */
#define SA_AIF_HOTPLUG			(1<<1)
#define SA_AIF_HARDWARE			(1<<2)
#define SA_AIF_PDEV_CHANGE		(1<<4)
#define SA_AIF_LDEV_CHANGE		(1<<5)
#define SA_AIF_BPSTAT_CHANGE		(1<<30)
#define SA_AIF_BPCFG_CHANGE		(1<<31)

#define	HBA_MAX_SG_EMBEDDED		28
#define HBA_MAX_SG_SEPARATE		90
#define	HBA_SENSE_DATA_LEN_MAX		32
#define	HBA_REQUEST_TAG_ERROR_FLAG	0x00000002
#define	HBA_SGL_FLAGS_EXT		0x80000000UL

enum {
	DBG_STATE_FREE			= 0,
	DBG_STATE_FREED_AIF		= 1,
	DBG_STATE_FREED_IO		= 2,
	DBG_STATE_ALLOCATED_AIF		= 3,
	DBG_STATE_ALLOCATED_IO		= 4,
	DBG_STATE_INITIALIZED		= 5,
	DBG_STATE_IO_SUBMITTED		= 6,
	DBG_STATE_PRE_INT		= 7,
	DBG_STATE_POST_INT		= 8
};


struct aac_hba_sgl {
	u32		addr_lo; /* Lower 32-bits of SGL element address */
	u32		addr_hi; /* Upper 32-bits of SGL element address */
	u32		len;	/* Length of SGL element in bytes */
	u32		flags;	/* SGL element flags */
};

enum {
	HBA_IU_TYPE_SCSI_CMD_REQ		= 0x40,
	HBA_IU_TYPE_SCSI_TM_REQ			= 0x41,
	HBA_IU_TYPE_SATA_REQ			= 0x42,
	HBA_IU_TYPE_RESP			= 0x60,
	HBA_IU_TYPE_COALESCED_RESP		= 0x61,
	HBA_IU_TYPE_INT_COALESCING_CFG_REQ	= 0x70
};

enum
{
	HBA_CMD_BYTE1_DATA_DIR_IN	= 0x1,
	HBA_CMD_BYTE1_DATA_DIR_OUT	= 0x2,
	HBA_CMD_BYTE1_DATA_TYPE_DDR	= 0x4,
	HBA_CMD_BYTE1_CRYPTO_ENABLE	= 0x8
};

enum
{
	HBA_CMD_BYTE1_BITOFF_DATA_DIR_IN	= 0,
	HBA_CMD_BYTE1_BITOFF_DATA_DIR_OUT,
	HBA_CMD_BYTE1_BITOFF_DATA_TYPE_DDR,
	HBA_CMD_BYTE1_BITOFF_CRYPTO_ENABLE
};

enum
{
	HBA_RESP_DATAPRES_NO_DATA	= 0x0,
	HBA_RESP_DATAPRES_RESPONSE_DATA,
	HBA_RESP_DATAPRES_SENSE_DATA
};

enum
{
	HBA_RESP_SVCRES_TASK_COMPLETE	= 0x0,
	HBA_RESP_SVCRES_FAILURE,
	HBA_RESP_SVCRES_TMF_COMPLETE,
	HBA_RESP_SVCRES_TMF_SUCCEEDED,
	HBA_RESP_SVCRES_TMF_REJECTED,
	HBA_RESP_SVCRES_TMF_LUN_INVALID
};

enum
{
	HBA_RESP_STAT_IO_ERROR = 0x1,
	HBA_RESP_STAT_IO_ABORTED,
	HBA_RESP_STAT_NO_PATH_TO_DEVICE,
	HBA_RESP_STAT_INVALID_DEVICE,
	HBA_RESP_STAT_HBAMODE_DISABLED	= 0xE,
	HBA_RESP_STAT_UNDERRUN = 0x51,
	HBA_RESP_STAT_OVERRUN = 0x75
};

struct aac_hba_cmd_req {
	u8	iu_type;	/* HBA information unit type */
	/*
	 ** byte1:
	 ** [1:0] DIR - 0=No data, 0x1 = IN, 0x2 = OUT
	 ** [2]   TYPE - 0=PCI, 1=DDR
	 ** [3]   CRYPTO_ENABLE - 0=Crypto disabled, 1=Crypto enabled
	 */
	u8	byte1;
	u8	reply_qid;	/* Host reply queue to post response to */
	u8	reserved1;
	__le32	it_nexus;	/* Device handle for the request */
	__le32	request_id;	/* Sender context */
	__le32	tweak_value_lo;	/* Lower 32-bits of tweak value for crypto enabled IOs */
	u8	cdb[16];	/* SCSI CDB of the command */
	u8	lun[8];		/* SCSI LUN of the command */
	__le32	data_length;	/* Total data length in bytes to be read/written (if any) */

	u8	attr_prio;	/* [2:0] Task Attribute, [6:3] Command Priority */
	u8	emb_data_desc_count;	/* Number of SGL elements embedded in the HBA req */
	__le16	dek_index;	/* DEK index for crypto enabled IOs */
	__le32	error_ptr_lo;	/* Lower 32-bits of reserved error data target location on the host */
	__le32	error_ptr_hi;	/* Upper 32-bits of reserved error data target location on the host */
	__le32	error_length;	/* Length of reserved error data area on the host in bytes */
	__le32	tweak_value_hi;	/* Upper 32-bits of tweak value for crypto enabled IOs */

	struct aac_hba_sgl sge[HBA_MAX_SG_SEPARATE+2]; /* SG list space */
	/* structure must not exceed AAC_MAX_NATIVE_SIZE-FW_ERROR_BUFFER_SIZE */
};

/* Task Management Functions (TMF) */
#define HBA_TMF_ABORT_TASK	0x01
#define HBA_TMF_LUN_RESET	0x08

struct aac_hba_tm_req {
	u8	iu_type;	/* HBA information unit type */
	u8	reply_qid;	/* Host reply queue to post response to */
	u8	tmf;		/* Task management function */
	u8	reserved1;

	__le32	it_nexus;	/* Device handle for the command */

	u8	lun[8];		/* SCSI LUN */

	/* Used to hold sender context. */
	__le32	request_id;	/* Sender context */
	__le32	reserved2;

	/* Request identifier of managed task */
	__le32	managed_request_id;	/* Sender context being managed */ 
	__le32	reserved3;

	__le32	error_ptr_lo;	/* Lower 32-bits of reserved error data target location on the host */
	__le32	error_ptr_hi;	/* Upper 32-bits of reserved error data target location on the host */
	__le32	error_length;	/* Length of reserved error data area on the host in bytes */
};

struct aac_hba_reset_req {
	u8	iu_type;	/* HBA information unit type */
	u8	reset_type;	/* 0 - reset specified device, 1 - reset all devices */
	u8	reply_qid;	/* Host reply queue to post response to */
	u8	reserved1;

	__le32	it_nexus;	/* Device handle for the command */
	__le32	request_id;	/* Sender context */

	__le32	error_ptr_lo;	/* Lower 32-bits of reserved error data target location on the host */
	__le32	error_ptr_hi;	/* Upper 32-bits of reserved error data target location on the host */
	__le32	error_length;	/* Length of reserved error data area on the host in bytes */
};

struct aac_hba_resp {
	u8	iu_type;	/* HBA information unit type */
	u8	reserved1[3];
	__le32	request_identifier;	/* sender context */
	__le32	reserved2;
	u8	service_response;	/* SCSI service response */
	u8	status;	/* SCSI status */
	u8	datapres;	/* [1:0] - data present, [7:2] - reserved */
	u8	sense_response_data_len;	/* Sense/response data length */
	__le32	residual_count;	/* Residual data length in bytes */
	u8	sense_response_buf[HBA_SENSE_DATA_LEN_MAX];	/* Sense/response data */
};

struct aac_native_hba {
	union {
		struct aac_hba_cmd_req cmd; 
		struct aac_hba_tm_req tmr;
		u8 cmd_bytes[AAC_MAX_NATIVE_SIZE-FW_ERROR_BUFFER_SIZE];
	} cmd;
	union {
		struct aac_hba_resp err;
		u8 resp_bytes[FW_ERROR_BUFFER_SIZE];
	} resp;
};

#define CISS_REPORT_PHYSICAL_LUNS	0xc3
struct aac_ciss_phys_luns_resp {
	u8	list_length[4];		/* LUN list length (N-7, big endian) */
	u8	resp_flag;		/* extended response_flag */
	u8	reserved[3];
	struct _ciss_lun {
		u8	tid[3];		/* Target ID */
		u8	bus;		/* Bus, flag (bits 6,7) */
		u8	level3[2];
		u8	level2[2];
		u8	node_ident[16];	/* phys. node identifier */
	} lun[1];			/* List of phys. devices */
};

#define WRITE_HOST_WELLNESS		0xa5

#define CISS_IDENTIFY_PHYSICAL_DEVICE 	0x15

#pragma pack(1)

struct aac_ciss_identify_pd {
        u8 scsi_bus;          /* SCSI Bus number on controller */
        u8 scsi_id;           /* SCSI ID on this bus */
        u16 block_size;      /* sector size in bytes */
        u32 total_blocks;            /* number for sectors on drive */
        u32 reserved_blocks;   /* controller reserved (RIS) */
        u8 model[40];         /* Physical Drive Model */
        u8 serial_number[40]; /* Drive Serial Number */
        u8 firmware_revision[8]; /* drive firmware revision */
        u8 scsi_inquiry_bits; /* inquiry byte 7 bits */
        u8 compaq_drive_stamp; /* 0 means drive not stamped */
        u8 last_failure_reason;

        u8  flags;
        u8  more_flags;
        u8  scsi_lun;          /* SCSI LUN for phys drive */
        u8  yet_more_flags;
        u8  even_more_flags;
        u32 spi_speed_rules;/* SPI Speed data:Ultra disable diagnose */
        u8  phys_connector[2];         /* connector number on controller */
        u8  phys_box_on_bus;  /* phys enclosure this drive resides */
        u8  phys_bay_in_box;  /* phys drv bay this drive resides */
        u32 rpm;              /* Drive rotational speed in rpm */
        u8  device_type;       /* type of drive */
        u8  sata_version;     /* only valid when drive_type is SATA */
        u64 big_total_block_count;
        u64 ris_starting_lba;
        u32 ris_size;
        u8  wwid[20];
        u8  controller_phy_map[32];
        u16 phy_count;
        u8  phy_connected_dev_type[256];
        u8  phy_to_drive_bay_num[256];
        u16 phy_to_attached_dev_index[256];
        u8  box_index;
        u8  spitfire_support;
        u16 extra_physical_drive_flags;
        u8  negotiated_link_rate[256];
        u8  phy_to_phy_map[256];
        u8  redundant_path_present_map;
        u8  redundant_path_failure_map;
        u8  active_path_number;
        u16 alternate_paths_phys_connector[8];
        u8  alternate_paths_phys_box_on_port[8];
        u8  multi_lun_device_lun_count;
        u8  minimum_good_fw_revision[8];
        u8  unique_inquiry_bytes[20];
        u8  current_temperature_degreesC;
        u8  temperature_threshold_degreesC;
        u8  max_temperature_degreesC;
        u8  logical_blocks_per_phys_block_exp; /* phyblocksize = 512 * 2^exp */
        u16 current_queue_depth_limit;
        u8  switch_name[10];
        u16 switch_port;
        u8  alternate_paths_switch_name[40];
        u8  alternate_paths_switch_port[8];
        u16 power_on_hours; /* valid only if gas gauge supported */
        u16 percent_endurance_used; /* valid only if gas gauge supported. */
        u8  drive_authentication;
        u8  smart_carrier_authentication;
        u8  smart_carrier_app_fw_version;
        u8  smart_carrier_bootloader_fw_version;
        u8  SanitizeSecureEraseSupport;
        u8  DriveKeyFlags;
        u8  encryption_key_name[64];
        u32 misc_drive_flags;
        u16 dek_index;
        u16 drive_encryption_flags;   
        u8  sanitize_maximum_time[6];
        u8  connector_info_mode;    
        u8  connector_info_number[4];
        u8  long_connector_name[64]; 
        u8  device_unique_identifier[16]; 
        u8  padto_2K[17];              
};

#pragma pack()

/*
 * Interrupts
 */
#define AAC_MAX_MSIX		32	/* vectors */
#define AAC_MAX_HRRQ		64

#define AAC_PCI_MSI_ENABLE	0x8000

enum {
	AAC_ENABLE_INTERRUPT	= 0x0,
	AAC_DISABLE_INTERRUPT,
	AAC_ENABLE_MSIX,
	AAC_DISABLE_MSIX,
	AAC_CLEAR_AIF_BIT,
	AAC_CLEAR_SYNC_BIT,
	AAC_ENABLE_INTX
};

#define AAC_INT_MODE_INTX		(1<<0)
#define AAC_INT_MODE_MSI		(1<<1)
#define AAC_INT_MODE_AIF		(1<<2)
#define AAC_INT_MODE_SYNC		(1<<3)

#define AAC_INT_ENABLE_TYPE1_INTX	0xfffffffb
#define AAC_INT_ENABLE_TYPE1_MSIX	0xfffffffa
#define AAC_INT_DISABLE_ALL		0xffffffff

/* Bit definitions in IOA->Host Interrupt Register */
#define PMC_TRANSITION_TO_OPERATIONAL	(0x80000000 >> 0)
#define PMC_IOARCB_TRANSFER_FAILED	(0x80000000 >> 3)
#define PMC_IOA_UNIT_CHECK		(0x80000000 >> 4)
#define PMC_NO_HOST_RRQ_FOR_CMD_RESPONSE (0x80000000 >> 5)
#define PMC_CRITICAL_IOA_OP_IN_PROGRESS	(0x80000000 >> 6)
#define PMC_IOARRIN_LOST		(0x80000000 >> 27)
#define PMC_SYSTEM_BUS_MMIO_ERROR	(0x80000000 >> 28)
#define PMC_IOA_PROCESSOR_IN_ERROR_STATE (0x80000000 >> 29)
#define PMC_HOST_RRQ_VALID		(0x80000000 >> 30)
#define PMC_OPERATIONAL_STATUS		(0x80000000 >> 0)
#define PMC_ALLOW_MSIX_VECTOR0		(0x80000000 >> 31)

#define PMC_IOA_ERROR_INTERRUPTS	(PMC_IOARCB_TRANSFER_FAILED | \
					 PMC_IOA_UNIT_CHECK | \
					 PMC_NO_HOST_RRQ_FOR_CMD_RESPONSE | \
					 PMC_IOARRIN_LOST | \
					 PMC_SYSTEM_BUS_MMIO_ERROR | \
					 PMC_IOA_PROCESSOR_IN_ERROR_STATE)

#define PMC_ALL_INTERRUPT_BITS		(PMC_IOA_ERROR_INTERRUPTS | \
					 PMC_HOST_RRQ_VALID | \
					 PMC_TRANSITION_TO_OPERATIONAL | \
					 PMC_ALLOW_MSIX_VECTOR0)

#define PMC_GLOBAL_INT_BIT2		0x00000004
#define PMC_GLOBAL_INT_BIT0		0x00000001

// in linit.c too
#if (!defined(CONFIG_COMMUNITY_KERNEL) && !defined(CONFIG_DISKDUMP) && !defined(CONFIG_DISKDUMP_MODULE) && !defined(CONFIG_CRASH_DUMP) && !defined(CONFIG_CRASH_DUMP_MODULE))
#undef SCSI_HAS_DUMP
#endif
#if (defined(HAS_KDUMP_CONFIG))
#undef SCSI_HAS_DUMP
#endif
//

#include <linux/interrupt.h>
#include <linux/pci.h>
#include "compat.h"
#include "adbg.h"
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23))
#if (!defined(IRQ_NONE))
  typedef void irqreturn_t;
# define IRQ_HANDLED
# define IRQ_NONE
#endif
#endif
#if (defined(SCSI_HAS_DUMP))
#if (defined(HAS_DISKDUMP_H))
#include <linux/diskdump.h>
#endif
#if (defined(lkcd_dump_mode) && !defined(crashdump_mode))
# define crashdump_mode() lkcd_dump_mode()
#endif
#if ((defined(HAS_DISKDUMPLIB_H) || defined(crashdump_mode)) && (defined(CONFIG_DISKDUMP) || defined(CONFIG_DISKDUMP_MODULE) || defined(CONFIG_CRASH_DUMP) || defined(CONFIG_CRASH_DUMP_MODULE)))
#undef ssleep
#undef msleep
#if (!defined(HAS_DUMP_SSLEEP))
 void aac_diskdump_ssleep(unsigned int seconds);
 void aac_diskdump_msleep(unsigned int msecs);
#define ssleep(seconds)	aac_diskdump_ssleep(seconds)
#define msleep(msec)	aac_diskdump_msleep(msec)
#else
#define ssleep(seconds)	diskdump_ssleep(seconds)
#define msleep(msec)	diskdump_msleep(msec)
#endif
#if (defined(HAS_DUMP_MDELAY))
 void aac_diskdump_mdelay(unsigned int msecs);
#undef mdelay
#define mdelay(msec)	aac_diskdump_mdelay(msec)
#endif
#endif
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,9)) && !defined(HAS_BOOT_CONFIG) && !defined(CONFIG_EXTERNAL) && !defined(CONFIG_COMMUNITY_KERNEL) && !defined(__arm__) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
//#define AAC_CSMI
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
#include "scsi.h"
#if (!defined (__VMKLNX__))
#include "hosts.h"
#include "vmklinux_dist.h"
#endif
#define num_physpages AAC_MAX_HOSTPHYSMEMPAGES /* vmkernel does not support 64-bit card access */
#endif

/*------------------------------------------------------------------------------
 *              D E F I N E S
 *----------------------------------------------------------------------------*/

//#define AAC_EXTENDED_TIMEOUT	120
//
#ifndef AAC_DRIVER_BUILD
# define AAC_DRIVER_BUILD 50639
#if (defined(CONFIG_COMMUNITY_KERNEL))
# define AAC_DRIVER_BRANCH "-ms"
#endif
#endif
#define MAXIMUM_NUM_CONTAINERS	32

#define AAC_NUM_MGT_FIB         8
#define AAC_NUM_IO_FIB		(1024 - AAC_NUM_MGT_FIB)
#define AAC_NUM_FIB		(AAC_NUM_IO_FIB + AAC_NUM_MGT_FIB)

#define AAC_MAX_LUN		256

#define AAC_MAX_HOSTPHYSMEMPAGES (0xfffff)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18))
#define AAC_MAX_32BIT_SGBCOUNT	((unsigned short)128)
#else
#define AAC_MAX_32BIT_SGBCOUNT	((unsigned short)256)
#endif

/*
 * These macros convert from physical channels to virtual channels
 */
#define CONTAINER_CHANNEL		(0)
#define CONTAINER_TO_CHANNEL(cont)	(CONTAINER_CHANNEL)
#define CONTAINER_TO_ID(cont)		(cont)
#define CONTAINER_TO_LUN(cont)		(0)
#define ENCLOSURE_CHANNEL		(3)

#define	PMC_DEVICE_S6	0x28b
#define	PMC_DEVICE_S7	0x28c
#define	PMC_DEVICE_S8	0x28d
#define	PMC_DEVICE_S9	0x28f

#define aac_phys_to_logical(x)  ((x)+1)
#define aac_logical_to_phys(x)  ((x)?(x)-1:0)

/* #define AAC_DETAILED_STATUS_INFO */
#if (defined(__arm__) && !defined(CONFIG_COMMUNITY_KERNEL))
#define AAC_LM_SENSOR
#endif
#if (defined(__VMKLNX__))
#define spin_lock_destroy(lock)
#define vmk_verify_memory_for_io(a,b)
#endif  

struct diskparm
{
	int heads;
	int sectors;
	int cylinders;
};


/*
 *	Firmware constants
 */

#define		CT_NONE			0
#define		CT_CONTINUATION_STOP_OK	198
#define		CT_CONTINUATION_ERROR	199
#define		CT_CONTINUATION_OK	200
#define		CT_CONTINUATION_DATA	201
#define		CT_OK			218
#define		CT_ID_CONFLICT		282
#define		FT_FILESYS	8	/* ADAPTEC's "FSA"(tm) filesystem */
#define		FT_DRIVE	9	/* physical disk - addressable in scsi by bus/id/lun */

/* continuation */
#define STREAMLINED_CONTINUATION    0x80000000
#define CT_RESPONSE_DATA_PACKET_SIZE( RESPONSE_SIZE )   \
                                                (RESPONSE_SIZE - (sizeof(u32)*8))
#define		FIB_DATA_SIZE_IN_BYTES_VAR(FIB_SIZE)	(FIB_SIZE - sizeof(struct aac_fibhdr))
#define		CT_PACKET_SIZE_VAR_FIB(FIB_SIZE)  CT_RESPONSE_DATA_PACKET_SIZE(FIB_DATA_SIZE_IN_BYTES_VAR (FIB_SIZE))
#define		CT_CONTINUE_DATA		83
#define		CT_STOP_DATA		84

/*
 *	Host side memory scatter gather list
 *	Used by the adapter for read, write, and readdirplus operations
 *	We have separate 32 and 64 bit version because even
 *	on 64 bit systems not all cards support the 64 bit version
 */
struct sgentry {
	__le32	addr;	/* 32-bit address. */
	__le32	count;	/* Length. */
};

struct user_sgentry {
	u32	addr;	/* 32-bit address. */
	u32	count;	/* Length. */
};

struct sgentry64 {
	__le32	addr[2];	/* 64-bit addr. 2 pieces for data alignment */
	__le32	count;	/* Length. */
};

struct user_sgentry64 {
	u32	addr[2];	/* 64-bit addr. 2 pieces for data alignment */
	u32	count;	/* Length. */
};

struct sgentryraw {
	__le32		next;	/* reserved for F/W use */
	__le32		prev;	/* reserved for F/W use */
	__le32		addr[2];
	__le32		count;
	__le32		flags;	/* reserved for F/W use */
};

struct user_sgentryraw {
	u32		next;	/* reserved for F/W use */
	u32		prev;	/* reserved for F/W use */
	u32		addr[2];
	u32		count;
	u32		flags;	/* reserved for F/W use */
};

struct sge_ieee1212 {
	u32		addrLow;
	u32		addrHigh;
	u32		length;
	u32		flags;
};

/*
 *	SGMAP
 *
 *	This is the SGMAP structure for all commands that use
 *	32-bit addressing.
 */

struct sgmap {
	__le32		count;
	struct sgentry	sg[1];
};

struct user_sgmap {
	u32		count;
	struct user_sgentry	sg[1];
};

struct sgmap64 {
	__le32		count;
	struct sgentry64 sg[1];
};

struct user_sgmap64 {
	u32		count;
	struct user_sgentry64 sg[1];
};

struct sgmapraw {
	__le32		  count;
	struct sgentryraw sg[1];
};

struct user_sgmapraw {
	u32		  count;
	struct user_sgentryraw sg[1];
};

struct creation_info
{
	u8		buildnum;		/* e.g., 588 */
	u8		usec;			/* e.g., 588 */
	u8		via;			/* e.g., 1 = FSU,
						 *	 2 = API
						 */
	u8		year;			/* e.g., 1997 = 97 */
	__le32		date;			/*
						 * unsigned	Month		:4;	// 1 - 12
						 * unsigned	Day		:6;	// 1 - 32
						 * unsigned	Hour		:6;	// 0 - 23
						 * unsigned	Minute		:6;	// 0 - 60
						 * unsigned	Second		:6;	// 0 - 60
						 */
	__le32		serial[2];			/* e.g., 0x1DEADB0BFAFAF001 */
};


/*
 *	Define all the constants needed for the communication interface
 */

/*
 *	Define how many queue entries each queue will have and the total
 *	number of entries for the entire communication interface. Also define
 *	how many queues we support.
 *
 *	This has to match the controller
 */

#define NUMBER_OF_COMM_QUEUES  8   // 4 command; 4 response
#define HOST_HIGH_CMD_ENTRIES  4
#define HOST_NORM_CMD_ENTRIES  8
#define ADAP_HIGH_CMD_ENTRIES  4
#define ADAP_NORM_CMD_ENTRIES  512
#define HOST_HIGH_RESP_ENTRIES 4
#define HOST_NORM_RESP_ENTRIES 512
#define ADAP_HIGH_RESP_ENTRIES 4
#define ADAP_NORM_RESP_ENTRIES 8

#define TOTAL_QUEUE_ENTRIES  \
    (HOST_NORM_CMD_ENTRIES + HOST_HIGH_CMD_ENTRIES + ADAP_NORM_CMD_ENTRIES + ADAP_HIGH_CMD_ENTRIES + \
	    HOST_NORM_RESP_ENTRIES + HOST_HIGH_RESP_ENTRIES + ADAP_NORM_RESP_ENTRIES + ADAP_HIGH_RESP_ENTRIES)


/*
 *	Set the queues on a 16 byte alignment
 */

#define QUEUE_ALIGNMENT		16

/*
 *	The queue headers define the Communication Region queues. These
 *	are physically contiguous and accessible by both the adapter and the
 *	host. Even though all queue headers are in the same contiguous block
 *	they will be represented as individual units in the data structures.
 */

struct aac_entry {
	__le32 size; /* Size in bytes of Fib which this QE points to */
	__le32 addr; /* Receiver address of the FIB */
};

/*
 *	The adapter assumes the ProducerIndex and ConsumerIndex are grouped
 *	adjacently and in that order.
 */

struct aac_qhdr {
	__le64 header_addr;/* Address to hand the adapter to access
			      to this queue head */
	__le32 *producer; /* The producer index for this queue (host address) */
	__le32 *consumer; /* The consumer index for this queue (host address) */
};

/*
 *	Define all the events which the adapter would like to notify
 *	the host of.
 */

#define		HostNormCmdQue		1	/* Change in host normal priority command queue */
#define		HostHighCmdQue		2	/* Change in host high priority command queue */
#define		HostNormRespQue		3	/* Change in host normal priority response queue */
#define		HostHighRespQue		4	/* Change in host high priority response queue */
#define		AdapNormRespNotFull	5
#define		AdapHighRespNotFull	6
#define		AdapNormCmdNotFull	7
#define		AdapHighCmdNotFull	8
#define		SynchCommandComplete	9
#define		AdapInternalError	0xfe    /* The adapter detected an internal error shutting down */

/*
 *	Define all the events the host wishes to notify the
 *	adapter of. The first four values much match the Qid the
 *	corresponding queue.
 */

#define		AdapNormCmdQue		2
#define		AdapHighCmdQue		3
#define		AdapNormRespQue		6
#define		AdapHighRespQue		7
#define		HostShutdown		8
#define		HostPowerFail		9
#define		FatalCommError		10
#define		HostNormRespNotFull	11
#define		HostHighRespNotFull	12
#define		HostNormCmdNotFull	13
#define		HostHighCmdNotFull	14
#define		FastIo			15
#define		AdapPrintfDone		16

/*
 *	Define all the queues that the adapter and host use to communicate
 *	Number them to match the physical queue layout.
 */

enum aac_queue_types {
        HostNormCmdQueue = 0,	/* Adapter to host normal priority command traffic */
        HostHighCmdQueue,	/* Adapter to host high priority command traffic */
        AdapNormCmdQueue,	/* Host to adapter normal priority command traffic */
        AdapHighCmdQueue,	/* Host to adapter high priority command traffic */
        HostNormRespQueue,	/* Adapter to host normal priority response traffic */
        HostHighRespQueue,	/* Adapter to host high priority response traffic */
        AdapNormRespQueue,	/* Host to adapter normal priority response traffic */
        AdapHighRespQueue	/* Host to adapter high priority response traffic */
};

/*
 *	Assign type values to the FSA communication data structures
 */

#define		FIB_MAGIC		0x0001
#define		FIB_MAGIC2		0x0004
#define		FIB_MAGIC2_64	0x0005

/*
 *	Define the priority levels the FSA communication routines support.
 */

#define		FsaNormal	1

/* transport FIB header (PMC) */
struct aac_fib_xporthdr {
	__le64	HostAddress;	/* FIB host address w/o xport header */
	__le32	Size;		/* FIB size excluding xport header */
	__le32	Handle;		/* driver handle to reference the FIB */
	__le64	Reserved[2];
};

/*
 * Define the FIB. The FIB is the where all the requested data and
 * command information are put to the application on the FSA adapter.
 */
#define	FIB_SIZE_STANDARD			512	/* for all legacy FIBs that has header+payload <= 512 bytes */
#define	FIB_SIZE_2K				2048	/* AAC_OPT_2K_FIB_SUPPORT: Support for larger payloads */
#define	FIB_DATA_SIZE_IN_BYTES_VAR(FIB_SIZE)	(FIB_SIZE - sizeof(struct aac_fibhdr))
#define	FIB_DATA_SIZE_IN_BYTES			FIB_DATA_SIZE_IN_BYTES_VAR(FIB_SIZE_STANDARD)
struct aac_fibhdr {
	__le32 XferState;	/* Current transfer state for this CCB */
	__le16 Command;		/* Routing information for the destination */
	u8 StructType;		/* Type FIB */
	u8 Unused;		/* Unused */
	__le16 Size;		/* Size of this FIB in bytes */
	__le16 SenderSize;	/* Size of the FIB in the sender
				   (for response sizing) */
	__le32 SenderFibAddress;  /* Host defined data in the FIB */
	union {
		__le32 ReceiverFibAddress;/* Logical address of this FIB for
				     the adapter (old) */
		__le32 SenderFibAddressHigh;/* upper 32bit of phys. FIB address */
		__le32 TimeStamp;	/* otherwise timestamp for FW internal use */
	} u;
	__le32 Handle;		/* FIB handle used for MSGU commnunication */
	u32 Previous;		/* FW internal use */
	u32 Next;		/* FW internal use */
};

struct hw_fib {
	struct aac_fibhdr header;
	u8 data[FIB_DATA_SIZE_IN_BYTES];	// Command specific data
};
//#define INITFLAGS_APRE_SUPPORTED
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

struct aac_apre_hdr {
	__le32	nextaddr_lower;
	__le32	nextaddr_upper;
	__le16	Operation;
	__le16	reserved0;
	u16	tcNumber;
	__le16	reserved1;
	u32	DestObjHandle;
	u8	hSourceObj[4];
	u8	hDesContext[4];
};

struct hw_apre {
	struct aac_apre_hdr header;
	u8 data[128-sizeof(struct aac_apre_hdr)];
	u32 sg[17*4];
};

union hw_cmnd {
	struct hw_fib fib;
	struct hw_apre apre;
};

union hw_cmnd_ptr {
	struct hw_fib * fib;
	struct hw_apre * apre;
};

/* APRE 8 byte Done List Entry definition */

struct donelist_entry
{
	u8	status0;
	u8	status1;
	u16	tcNumber;
        __le16	consumerIndex;
        u8	resv0;
        u8	DLIndicator;
};
#endif

/*
 *	FIB commands
 */

#define		TestCommandResponse		1
#define		TestAdapterCommand		2
/*
 *	Lowlevel and comm commands
 */
#define		LastTestCommand			100
#define		ReinitHostNormCommandQueue	101
#define		ReinitHostHighCommandQueue	102
#define		ReinitHostHighRespQueue		103
#define		ReinitHostNormRespQueue		104
#define		ReinitAdapNormCommandQueue	105
#define		ReinitAdapHighCommandQueue	107
#define		ReinitAdapHighRespQueue		108
#define		ReinitAdapNormRespQueue		109
#define		InterfaceShutdown		110
#define		DmaCommandFib			120
#define		StartProfile			121
#define		TermProfile			122
#define		SpeedTest			123
#define		TakeABreakPt			124
#define		RequestPerfData			125
#define		SetInterruptDefTimer		126
#define		SetInterruptDefCount		127
#define		GetInterruptDefStatus		128
#define		LastCommCommand			129
/*
 *	Filesystem commands
 */
#define		NuFileSystem			300
#define		UFS				301
#define		HostFileSystem			302
#define		LastFileSystemCommand		303
/*
 *	Container Commands
 */
#define		ContainerCommand		500
#define		ContainerCommand64		501
#define		ContainerRawIo			502
#define		ContainerRawIo2			503
/*
 *	Scsi Port commands (scsi passthrough)
 */
#define		ScsiPortCommand			600
#define		ScsiPortCommand64		601
/*
 *	Misc house keeping and generic adapter initiated commands
 */
#define		AifRequest			700
#define		CheckRevision			701
#define		FsaHostShutdown			702
#define		RequestAdapterInfo		703
#define		IsAdapterPaused			704
#define		SendHostTime			705
#define		RequestSupplementAdapterInfo	706
#define		LastMiscCommand			707
#if (!defined(CONFIG_COMMUNITY_KERNEL))

#define		RequestCompatibilityId		802
#endif

/*
 * Commands that will target the failover level on the FSA adapter
 */

enum fib_xfer_state {
	HostOwned			= (1<<0),
	AdapterOwned			= (1<<1),
	FibInitialized			= (1<<2),
	FibEmpty			= (1<<3),
	AllocatedFromPool		= (1<<4),
	SentFromHost			= (1<<5),
	SentFromAdapter			= (1<<6),
	ResponseExpected		= (1<<7),
	NoResponseExpected		= (1<<8),
	AdapterProcessed		= (1<<9),
	HostProcessed			= (1<<10),
	HighPriority			= (1<<11),
	NormalPriority			= (1<<12),
	Async				= (1<<13),
	AsyncIo				= (1<<13),	// rpbfix: remove with new regime
	PageFileIo			= (1<<14),	// rpbfix: remove with new regime
	ShutdownRequest			= (1<<15),
	LazyWrite			= (1<<16),	// rpbfix: remove with new regime
	AdapterMicroFib			= (1<<17),
	BIOSFibPath			= (1<<18),
	FastResponseCapable		= (1<<19),
	ApiFib				= (1<<20),	// Its an API Fib.
	NoMoreAifDataAvailable		= (1<<21)	// PMC NEW COMM: There is no more AIF data pending
};

/*
 *	The following defines needs to be updated any time there is an
 *	incompatible change made to the aac_init structure.
 */

#define ADAPTER_INIT_STRUCT_REVISION		3
#define ADAPTER_INIT_STRUCT_REVISION_4		4 // rocket science
#define ADAPTER_INIT_STRUCT_REVISION_6		6 // Tupelo
#define ADAPTER_INIT_STRUCT_REVISION_7		7 // Denali
#define ADAPTER_INIT_STRUCT_REVISION_8		8 // Thor

union aac_init
{
	struct _r7 {
		__le32	InitStructRevision;
		__le32	NoOfMSIXVectors;
		__le32	fsrev;
		__le32	CommHeaderAddress;
		__le32	FastIoCommAreaAddress;
		__le32	AdapterFibsPhysicalAddress;
		__le32	AdapterFibsVirtualAddress;
		__le32	AdapterFibsSize;
		__le32	AdapterFibAlign;
		__le32	printfbuf;
		__le32	printfbufsiz;
		__le32	HostPhysMemPages;   /* number of 4k pages of host phys. mem. */
		__le32	HostElapsedSeconds; /* number of seconds since 1970. */
		/* ADAPTER_INIT_STRUCT_REVISION_4 begins here */
		__le32	InitFlags;	/* flags for supported features */
#define INITFLAGS_NEW_COMM_SUPPORTED	0x00000001
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
#undef INITFLAGS_APRE_SUPPORTED
#define INITFLAGS_APRE_SUPPORTED	0x00000004
#endif
#define INITFLAGS_DRIVER_USES_UTC_TIME	0x00000010
#define INITFLAGS_DRIVER_SUPPORTS_PM	0x00000020
#define INITFLAGS_NEW_COMM_TYPE1_SUPPORTED  0x00000040
#define INITFLAGS_FAST_JBOD_SUPPORTED  0x00000080
#define INITFLAGS_NEW_COMM_TYPE2_SUPPORTED  0x00000100
#define INITFLAGS_DRIVER_SUPPORTS_HBA_MODE  0x00000400
		__le32	MaxIoCommands;	/* max outstanding commands */
		__le32	MaxIoSize;	/* largest I/O command */
		__le32	MaxFibSize;	/* largest FIB to adapter */
		/* ADAPTER_INIT_STRUCT_REVISION_5 begins here */
		__le32	MaxNumAif;	/* max number of aif */ 
		/* ADAPTER_INIT_STRUCT_REVISION_6 begins here */
		__le32	HostRRQ_AddrLow;
		__le32	HostRRQ_AddrHigh;		/* Host RRQ (response queue) for SRC */
	} r7;
	struct _r8 {
		/* ADAPTER_INIT_STRUCT_REVISION_8 */
		__le32	InitStructRevision;
		__le32	RRQueueCount;
		__le32	HostElapsedSeconds; /* number of seconds since 1970. */
		__le32	InitFlags;
		__le32	MaxIoSize;	/* largest I/O command */		
		__le32	MaxNumAif;	/* max number of aif */
		__le32	Reserved1;
		__le32	Reserved2;
		struct _rrq {
			__le32	Host_AddrLow;
			__le32	Host_AddrHigh;
			__le16	MSIX_Id;
			__le16	ElementCount;
			__le16	Comp_Thresh;
			__le16	Unused;
		} rrq[1];		/* up to 64 RRQ addresses */
	} r8;
};
	
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

#define A_OP_INIT_STRUCT 0xC0
struct aac_apre_init {
	struct aac_apre_hdr header;
	__le32	inboundMsgBaseLower;
	__le32	inboundMsgBaseUpper;
	__le16	NumMsgEnvelopes;
	u8	reserved1[2];
	__le32	DoneListBaseAddrLower;
	__le32	DoneListBaseAddreUpper;
	__le16	NumDoneListEntries;
	u8	reserved2[76];
};
#endif

enum aac_log_level {
	LOG_AAC_INIT			= 10,
	LOG_AAC_INFORMATIONAL		= 20,
	LOG_AAC_WARNING			= 30,
	LOG_AAC_LOW_ERROR		= 40,
	LOG_AAC_MEDIUM_ERROR		= 50,
	LOG_AAC_HIGH_ERROR		= 60,
	LOG_AAC_PANIC			= 70,
	LOG_AAC_DEBUG			= 80,
	LOG_AAC_WINDBG_PRINT		= 90
};

#define FSAFS_NTC_GET_ADAPTER_FIB_CONTEXT	0x030b
#define FSAFS_NTC_FIB_CONTEXT			0x030c

struct aac_dev;
struct fib;
struct scsi_cmnd;

struct adapter_ops
{
	/* Low level operations */
	void (*adapter_interrupt)(struct aac_dev *dev);
	void (*adapter_notify)(struct aac_dev *dev, u32 event);
	void (*adapter_disable_int)(struct aac_dev *dev);
	void (*adapter_enable_int)(struct aac_dev *dev);
	int  (*adapter_sync_cmd)(struct aac_dev *dev, u32 command, u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6, u32 *status, u32 *r1, u32 *r2, u32 *r3, u32 *r4);
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	int  (*adapter_sync_apre)(struct aac_dev *dev, struct hw_apre * command, struct hw_apre * response);
#endif
	int  (*adapter_check_health)(struct aac_dev *dev);
	int  (*adapter_restart)(struct aac_dev *dev, int bled);
	void (*adapter_start)(struct aac_dev *dev);
	/* Transport operations */
	int  (*adapter_ioremap)(struct aac_dev * dev, u32 size);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
	irqreturn_t (*adapter_intr)(int irq, void *dev_id, struct pt_regs *regs);
#if defined(__ESX5__)
	irqreturn_t (*adapter_intr_poll)(int irq, void *dev_id, struct pt_regs *regs);
#endif
#else
	irq_handler_t adapter_intr;
#if defined(__ESX5__)
	irq_handler_t adapter_intr_poll;
#endif
#endif
	/* Packet operations */
	int  (*adapter_deliver)(struct fib * fib);
	int  (*adapter_bounds)(struct aac_dev * dev, struct scsi_cmnd * cmd, u64 lba);
	int  (*adapter_read)(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count);
	int  (*adapter_write)(struct fib * fib, struct scsi_cmnd * cmd, u64 lba, u32 count, int fua);
	int  (*adapter_scsi)(struct fib * fib, struct scsi_cmnd * cmd);
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	int  (*adapter_scsi_cmd)(struct scsi_cmnd * cmd);
	int  (*adapter_build_sg)(struct fib * fib, struct scsi_cmnd * cmd, void * psg);
#endif
	/* Administrative operations */
	int  (*adapter_comm)(struct aac_dev * dev, int comm);
};

/*
 *	Define which interrupt handler needs to be installed
 */

struct aac_driver_ident
{
	int	(*init)(struct aac_dev *dev);
	char *	name;
	char *	vname;
	char *	model;
	u16	channels;
	int	quirks;
};
/*
 * Some adapter firmware needs communication memory
 * below 2gig. This tells the init function to set the
 * dma mask such that fib memory will be allocated where the
 * adapter firmware can get to it.
 */
#define AAC_QUIRK_31BIT	0x0001

/*
 * Some adapter firmware, when the raid card's cache is turned off, can not
 * split up scatter gathers in order to deal with the limits of the
 * underlying CHIM. This limit is 34 scatter gather elements.
 */
#define AAC_QUIRK_34SG	0x0002

/*
 * This adapter is a slave (no Firmware)
 */
#define AAC_QUIRK_SLAVE 0x0004

/*
 * This adapter is a master.
 */
#define AAC_QUIRK_MASTER 0x0008

/*
 * Some adapter firmware perform poorly when it must split up scatter gathers
 * in order to deal with the limits of the underlying CHIM. This limit in this
 * class of adapters is 17 scatter gather elements.
 */
#define AAC_QUIRK_17SG	0x0010

/*
 *	Some adapter firmware does not support 64 bit scsi passthrough
 * commands.
 */
#define AAC_QUIRK_SCSI_32	0x0020

/*
 *	The adapter interface specs all queues to be located in the same
 *	physically contigous block. The host structure that defines the
 *	commuication queues will assume they are each a separate physically
 *	contigous memory region that will support them all being one big
 *	contigous block.
 *	There is a command and response queue for each level and direction of
 *	commuication. These regions are accessed by both the host and adapter.
 */

struct aac_queue {
	u64			logical;	/*address we give the adapter */
	struct aac_entry	*base;		/*system virtual address */
	struct aac_qhdr		headers;	/*producer,consumer q headers*/
	u32			entries;	/*Number of queue entries */
	wait_queue_head_t	qfull;		/*Event to wait on if q full */
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
	struct semaphore	cmdready;		/* Indicates there is a Command ready from the adapter. */
#else
	wait_queue_head_t	cmdready;	/*Cmd ready from the adapter */
#endif
		/* This is only valid for adapter to host command queues. */
	spinlock_t		*lock;		/* Spinlock for this queue must take this lock before accessing the lock */
	spinlock_t		lockdata;	/* Actual lock (used only on one side of the lock) */
	struct list_head	cmdq;		/* A queue of FIBs which need to be prcessed by the FS thread. This is */
						/* only valid for command queues which receive entries from the adapter. */
	atomic_t			numpending;	/* Number of entries on outstanding queue. */
	struct aac_dev *	dev;		/* Back pointer to adapter structure */
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	struct donelist_entry *	DoneListPool;
	struct donelist_entry *	NxtDoneListEntry;
	u32			Credits;
#define APRE_MAX_CREDITS 254
	u8			DLIndicator;
#define ApreCmdQueue HostNormCmdQueue
#endif
};

/*
 *	Message queues. The order here is important, see also the
 *	queue type ordering
 */

struct aac_queue_block
{
	struct aac_queue queue[8];
};

/*
 *	SaP1 Message Unit Registers
 */

struct sa_drawbridge_CSR {
				/*	Offset	|  Name */
	__le32	reserved[10];	/*	00h-27h |  Reserved */
	u8	LUT_Offset;	/*	28h	|  Lookup Table Offset */
	u8	reserved1[3];	/*	29h-2bh	|  Reserved */
	__le32	LUT_Data;	/*	2ch	|  Looup Table Data */
	__le32	reserved2[26];	/*	30h-97h	|  Reserved */
	__le16	PRICLEARIRQ;	/*	98h	|  Primary Clear Irq */
	__le16	SECCLEARIRQ;	/*	9ah	|  Secondary Clear Irq */
	__le16	PRISETIRQ;	/*	9ch	|  Primary Set Irq */
	__le16	SECSETIRQ;	/*	9eh	|  Secondary Set Irq */
	__le16	PRICLEARIRQMASK;/*	a0h	|  Primary Clear Irq Mask */
	__le16	SECCLEARIRQMASK;/*	a2h	|  Secondary Clear Irq Mask */
	__le16	PRISETIRQMASK;	/*	a4h	|  Primary Set Irq Mask */
	__le16	SECSETIRQMASK;	/*	a6h	|  Secondary Set Irq Mask */
	__le32	MAILBOX0;	/*	a8h	|  Scratchpad 0 */
	__le32	MAILBOX1;	/*	ach	|  Scratchpad 1 */
	__le32	MAILBOX2;	/*	b0h	|  Scratchpad 2 */
	__le32	MAILBOX3;	/*	b4h	|  Scratchpad 3 */
	__le32	MAILBOX4;	/*	b8h	|  Scratchpad 4 */
	__le32	MAILBOX5;	/*	bch	|  Scratchpad 5 */
	__le32	MAILBOX6;	/*	c0h	|  Scratchpad 6 */
	__le32	MAILBOX7;	/*	c4h	|  Scratchpad 7 */
	__le32	ROM_Setup_Data;	/*	c8h	|  Rom Setup and Data */
	__le32	ROM_Control_Addr;/*	cch	|  Rom Control and Address */
	__le32	reserved3[12];	/*	d0h-ffh	|  reserved */
	__le32	LUT[64];	/*    100h-1ffh	|  Lookup Table Entries */
};

#define Mailbox0	SaDbCSR.MAILBOX0
#define Mailbox1	SaDbCSR.MAILBOX1
#define Mailbox2	SaDbCSR.MAILBOX2
#define Mailbox3	SaDbCSR.MAILBOX3
#define Mailbox4	SaDbCSR.MAILBOX4
#define Mailbox5	SaDbCSR.MAILBOX5
#define Mailbox6	SaDbCSR.MAILBOX6
#define Mailbox7	SaDbCSR.MAILBOX7

#define DoorbellReg_p SaDbCSR.PRISETIRQ
#define DoorbellReg_s SaDbCSR.SECSETIRQ
#define DoorbellClrReg_p SaDbCSR.PRICLEARIRQ


#define	DOORBELL_0	0x0001
#define DOORBELL_1	0x0002
#define DOORBELL_2	0x0004
#define DOORBELL_3	0x0008
#define DOORBELL_4	0x0010
#define DOORBELL_5	0x0020
#define DOORBELL_6	0x0040


#define PrintfReady	DOORBELL_5
#define PrintfDone	DOORBELL_5

struct sa_registers {
	struct sa_drawbridge_CSR	SaDbCSR;			/* 98h - c4h */
};


#define Sa_MINIPORT_REVISION			1

#define sa_readw(AEP, CSR)		readl(&((AEP)->regs.sa->CSR))
#define sa_readl(AEP, CSR)		readl(&((AEP)->regs.sa->CSR))
#define sa_writew(AEP, CSR, value)	writew(value, &((AEP)->regs.sa->CSR))
#define sa_writel(AEP, CSR, value)	writel(value, &((AEP)->regs.sa->CSR))

#if defined(writeq)
#define	sa_writeq(AEP, CSR, value) writeq(value, &((AEP)->regs.sa->CSR))
#endif

/*
 *	Rx Message Unit Registers
 */

struct rx_mu_registers {
			    /*	Local  | PCI*| Name */
	__le32	ARSR;	    /*	1300h  | 00h | APIC Register Select Register */
	__le32	reserved0;  /*	1304h  | 04h | Reserved */
	__le32	AWR;	    /*	1308h  | 08h | APIC Window Register */
	__le32	reserved1;  /*	130Ch  | 0Ch | Reserved */
	__le32	IMRx[2];    /*	1310h  | 10h | Inbound Message Registers */
	__le32	OMRx[2];    /*	1318h  | 18h | Outbound Message Registers */
	__le32	IDR;	    /*	1320h  | 20h | Inbound Doorbell Register */
	__le32	IISR;	    /*	1324h  | 24h | Inbound Interrupt
						Status Register */
	__le32	IIMR;	    /*	1328h  | 28h | Inbound Interrupt
						Mask Register */
	__le32	ODR;	    /*	132Ch  | 2Ch | Outbound Doorbell Register */
	__le32	OISR;	    /*	1330h  | 30h | Outbound Interrupt
						Status Register */
	__le32	OIMR;	    /*	1334h  | 34h | Outbound Interrupt
						Mask Register */
	__le32	reserved2;  /*	1338h  | 38h | Reserved */
	__le32	reserved3;  /*	133Ch  | 3Ch | Reserved */
	__le32	InboundQueue;/*	1340h  | 40h | Inbound Queue Port relative to firmware */
	__le32	OutboundQueue;/*1344h  | 44h | Outbound Queue Port relative to firmware */
			    /* * Must access through ATU Inbound
				 Translation Window */
};

struct rx_inbound {
	__le32	Mailbox[8];
};

#define	INBOUNDDOORBELL_0	0x00000001
#define INBOUNDDOORBELL_1	0x00000002
#define INBOUNDDOORBELL_2	0x00000004
#define INBOUNDDOORBELL_3	0x00000008
#define INBOUNDDOORBELL_4	0x00000010
#define INBOUNDDOORBELL_5	0x00000020
#define INBOUNDDOORBELL_6	0x00000040
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
#define INBOUNDDOORBELL_8	0x00000100
#define INBOUNDDOORBELL_9	0x00000200
#endif

#define	OUTBOUNDDOORBELL_0	0x00000001
#define OUTBOUNDDOORBELL_1	0x00000002
#define OUTBOUNDDOORBELL_2	0x00000004
#define OUTBOUNDDOORBELL_3	0x00000008
#define OUTBOUNDDOORBELL_4	0x00000010
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
#define OUTBOUNDDOORBELL_8	0x00000100
#define OUTBOUNDDOORBELL_9	0x00000200
#endif

#define InboundDoorbellReg	MUnit.IDR
#define OutboundDoorbellReg	MUnit.ODR

struct rx_registers {
	struct rx_mu_registers		MUnit;		/* 1300h - 1347h */
	__le32				reserved1[2];	/* 1348h - 134ch */
	struct rx_inbound		IndexRegs;
};

#define rx_readb(AEP, CSR)		readb(&((AEP)->regs.rx->CSR))
#define rx_readl(AEP, CSR)		readl(&((AEP)->regs.rx->CSR))
#define rx_writeb(AEP, CSR, value)	writeb(value, &((AEP)->regs.rx->CSR))
#define rx_writel(AEP, CSR, value)	writel(value, &((AEP)->regs.rx->CSR))

/*
 *	Rkt Message Unit Registers (same as Rx, except a larger reserve region)
 */

#define rkt_mu_registers rx_mu_registers
#define rkt_inbound rx_inbound

struct rkt_registers {
	struct rkt_mu_registers		MUnit;		 /* 1300h - 1347h */
	__le32				reserved1[1006]; /* 1348h - 22fch */
	struct rkt_inbound		IndexRegs;	 /* 2300h - */
};

#define rkt_readb(AEP, CSR)		readb(&((AEP)->regs.rkt->CSR))
#define rkt_readl(AEP, CSR)		readl(&((AEP)->regs.rkt->CSR))
#define rkt_writeb(AEP, CSR, value)	writeb(value, &((AEP)->regs.rkt->CSR))
#define rkt_writel(AEP, CSR, value)	writel(value, &((AEP)->regs.rkt->CSR))

/* 
 * PMC SRC message unit registers
 */ 

#define src_inbound rx_inbound

struct src_mu_registers {
				/*  PCI*| Name */
	__le32	reserved0[6];	/*  00h | Reserved */
	__le32	IOAR[2];	/*  18h | IOA->host interrupt register */
	__le32	IDR;		/*  20h | Inbound Doorbell Register */
	__le32	IISR;	    	/*  24h | Inbound Int. Status Register */
	__le32	reserved1[3];	/*  28h | Reserved */
	__le32	OIMR;	    	/*  34h | Outbound Int. Mask Register */
	__le32	reserved2[25];  /*  38h | Reserved */
	__le32	ODR_R;  	/*  9ch | Outbound Doorbell Read */
	__le32	ODR_C;  	/*  a0h | Outbound Doorbell Clear */
	__le32	reserved3[3];	/*  a4h | Reserved */
	__le32	SCR0;		/*  b0h | Scratchpad 0 */
	__le32	reserved4[2];	/*  b4h | Reserved */
	__le32	OMR;  		/*  bch | Outbound Message Register */
	__le32	IQ_L;		/*  c0h | Inbound Queue (Low address) */
	__le32	IQ_H;		/*  c4h | Inbound Queue (High address) */
	__le32	ODR_MSI;	/*  c8h | MSI register for sync./AIF */
	__le32  reserved5;	/*  cch | Reserved */
	__le32	IQN_L;		/*  d0h | Inbound (native cmd) low  */
	__le32	IQN_H;		/*  d4h | Inbound (native cmd) high */
};

struct src_registers {
	struct src_mu_registers MUnit;			/* 00h - d7h */
	union {
		struct {
			__le32 reserved1[130786];	/* d8h - 7fc5fh */
			struct src_inbound IndexRegs;	/* 7fc60h */
		} tupelo;
		struct {
			__le32 reserved1[970];		/* d8h - fffh */
			struct src_inbound IndexRegs;	/* 1000h */
		} denali;
	} u;
};

#define src_readb(AEP, CSR)		readb(&((AEP)->regs.src.bar0->CSR))
#define src_readl(AEP, CSR)		readl(&((AEP)->regs.src.bar0->CSR))
#define src_writeb(AEP, CSR, value)	writeb(value, &((AEP)->regs.src.bar0->CSR))
#define src_writel(AEP, CSR, value)	writel(value, &((AEP)->regs.src.bar0->CSR))

#if defined(writeq)
#define	src_writeq(AEP, CSR, value) writeq(value, &((AEP)->regs.src.bar0->CSR))
#endif

#define SRC_ODR_SHIFT 		12
#define SRC_IDR_SHIFT 		9

typedef void (*fib_callback)(void *ctxt, struct fib *fibctx);

struct aac_fib_context {
	s16			type;		// used for verification of structure
	s16			size;
	u32			unique;		// unique value representing this context
	ulong			jiffies;	// used for cleanup - dmb changed to ulong
	struct list_head	next;		// used to link context's into a linked list
	struct semaphore	wait_sem;	// this is used to wait for the next fib to arrive.
	int			wait;		// Set to true when thread is in WaitForSingleObject
	unsigned long		count;		// total number of FIBs on FibList
	struct list_head	fib_list;	// this holds fibs and their attachd hw_fibs
};

struct sense_data {
	u8 error_code;		/* 70h (current errors), 71h(deferred errors) */
	u8 valid:1;		/* A valid bit of one indicates that the information  */
				/* field contains valid information as defined in the
				 * SCSI-2 Standard.
				 */
	u8 segment_number;	/* Only used for COPY, COMPARE, or COPY AND VERIFY Commands */
	u8 sense_key:4;		/* Sense Key */
	u8 reserved:1;
	u8 ILI:1;		/* Incorrect Length Indicator */
	u8 EOM:1;		/* End Of Medium - reserved for random access devices */
	u8 filemark:1;		/* Filemark - reserved for random access devices */

	u8 information[4];	/* for direct-access devices, contains the unsigned
				 * logical block address or residue associated with
				 * the sense key
				 */
	u8 add_sense_len;	/* number of additional sense bytes to follow this field */
	u8 cmnd_info[4];	/* not used */
	u8 ASC;			/* Additional Sense Code */
	u8 ASCQ;		/* Additional Sense Code Qualifier */
	u8 FRUC;		/* Field Replaceable Unit Code - not used */
	u8 bit_ptr:3;		/* indicates which byte of the CDB or parameter data
				 * was in error
				 */
	u8 BPV:1;		/* bit pointer valid (BPV): 1- indicates that
				 * the bit_ptr field has valid value
				 */
	u8 reserved2:2;
	u8 CD:1;		/* command data bit: 1- illegal parameter in CDB.
				 * 0- illegal parameter in data.
				 */
	u8 SKSV:1;
	u8 field_ptr[2];	/* byte of the CDB or parameter data in error */
};

struct fsa_dev_info {
	u64		last;
	u64		size;
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	u32		Credits;
	u32		DestObjHandle;
#endif
	u32		type;
	u32		config_waiting_on;
	unsigned long	config_waiting_stamp;
	u16		queue_depth;
	u8		config_needed;
	u8		valid;
	u8		ro;
	u8		locked;
	u8		deleted;
	char		devname[8];
	struct sense_data sense_data;
	u32		block_size;
	u8		identifier[16];
};

struct fib {
	void			*next;	/* this is used by the allocator */
	s16			type;
	s16			size;
	/*
	 *	The Adapter that this I/O is destined for.
	 */
	struct aac_dev		*dev;
	/*
	 *	This is the event the sendfib routine will wait on if the
	 *	caller did not pass one and this is synch io.
	 */
	struct semaphore	event_wait;
	spinlock_t		event_lock;

	u32			done;	/* gets set to 1 when fib is complete */
	fib_callback		callback;
	void			*callback_data;
	u32			flags; // u32 dmb was ulong
	/*
	 *	And for the internal issue/reply queues (we may be able
	 *	to merge these two)
	 */
	struct list_head	fiblink;
	void			*data;
	u32			vector_no;

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	union hw_cmnd_ptr	hw_cmnd_va;
#	define hw_fib_va	hw_cmnd_va.fib
#	define hw_apre_va	hw_cmnd_va.apre
#else
	struct hw_fib		*hw_fib_va;	/* also used for native */
#endif
	dma_addr_t		hw_fib_pa;	/* physical address of hw_fib*/
	dma_addr_t		hw_sgl_pa;	/* extra sgl for native */
	dma_addr_t		hw_error_pa;	/* error buffer for native */
	u32			hbacmd_size;	/* cmd size for native */
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	u32			Credits;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	unsigned long		DriverTimeStartS;
	unsigned long		DriverTimeStartuS;
	unsigned long		DriverTimeDoneS;
	unsigned long		DriverTimeDoneuS;
#endif

#ifdef AAC_DEBUG_INSTRUMENT_RESET
	atomic_t		state;
#endif
};

#define AAC_DEVTYPE_RAID_MEMBER	1
#define AAC_DEVTYPE_ARC_RAW	2
#define AAC_DEVTYPE_NATIVE_RAW	3
#define AAC_EXPOSE_DISK		0
#define AAC_HIDE_DISK		3
struct aac_hba_map_info {
	__le32	rmw_nexus;		/* nexus for native HBA devices */
	u8		devtype;		/* device type */
	u8		reset_state;	/* 0 - no reset, 1..x - after xth TM LUN reset */
	u16		qd_limit;
	u8		expose;		/*checks if to expose or not*/
};

/*
 *	Adapter Information Block
 *
 *	This is returned by the RequestAdapterInfo block
 */

struct aac_adapter_info
{
	__le32	platform;
	__le32	cpu;
	__le32	subcpu;
	__le32	clock;
	__le32	execmem;
	__le32	buffermem;
	__le32	totalmem;
	__le32	kernelrev;
	__le32	kernelbuild;
	__le32	monitorrev;
	__le32	monitorbuild;
	__le32	hwrev;
	__le32	hwbuild;
	__le32	biosrev;
	__le32	biosbuild;
	__le32	cluster;
	__le32	clusterchannelmask;
	__le32	serial[2];
	__le32	battery;
	__le32	options;
	__le32	OEM;
};

struct aac_supplement_adapter_info
{
	u8	AdapterTypeText[17+1];
	u8	Pad[2];
	__le32	FlashMemoryByteSize;
	__le32	FlashImageId;
	__le32	MaxNumberPorts;
	__le32	Version;
	__le32	FeatureBits;
	u8	SlotNumber;
	u8	ReservedPad0[3];
	u8	BuildDate[12];
	__le32	CurrentNumberPorts;
	struct {
		u8	AssemblyPn[8];
		u8	FruPn[8];
		u8	BatteryFruPn[8];
		u8	EcVersionString[8];
		u8	Tsid[12];
	}	VpdInfo;
	__le32	FlashFirmwareRevision;
	__le32	FlashFirmwareBuild;
	__le32	RaidTypeMorphOptions;
	__le32	FlashFirmwareBootRevision;
	__le32	FlashFirmwareBootBuild;
	u8	MfgPcbaSerialNo[12];
	u8	MfgWWNName[8];
	__le32	SupportedOptions2;
	__le32	StructExpansion;
	/* StructExpansion == 1 */
	__le32	FeatureBits3;
	__le32	SupportedPerformanceModes;
	u8	HostBusType;	    /* uses HOST_BUS_TYPE_xxx defines */
	u8	HostBusWidth;	    /* actual width in bits or links */
	u16	HostBusSpeed;	    /* actual bus speed / link xfer rate in MHz */
	u8	MaxRRCDrives;		/* The max. number of ITP-RRC drives per RRC pool */
	u8	MaxDiskXtasks;		/* The max. possible number of DiskX Tasks */
	
	u8	CpldVerLoaded;
	u8	CpldVerInFlash;

	__le64	MaxRRCCapacity;	/* The max capacity to MaxIQ pool in blocks */
	__le32	CompiledMaxHistLogLevel;
	u8	CustomBoardName[12]; /* Will be used to identify customer board */
	u16	SupportedCntlrMode; /* to identify supported controller modes */
	u16	ReservedForFuture16;
	__le32	SupportedOptions3; /* reserved for future options */
	__le16	VirtDeviceBus;		/* virt. SCSI device for Thor */
	__le16	VirtDeviceTarget;
	__le16	VirtDeviceLUN;
	__le16	Unused;
	__le32	ReservedForFutureGrowth[68];
};
#define AAC_FEATURE_FALCON			cpu_to_le32(0x00000010)
#define AAC_FEATURE_JBOD			cpu_to_le32(0x08000000)
#define AAC_FEATURE_STREAMLINED_CONTINUATION	cpu_to_le32(0x00002000)
/* SupportedOptions2 */
#define AAC_OPTION_MU_RESET		cpu_to_le32(0x00000001)
#define AAC_OPTION_IGNORE_RESET		cpu_to_le32(0x00000002)
#define AAC_OPTION_POWER_MANAGEMENT	cpu_to_le32(0x00000004)
#define AAC_OPTION_DOORBELL_RESET	cpu_to_le32(0x00004000)
#define AAC_OPTION_VARIABLE_BLOCK_SIZE	cpu_to_le32(0x00040000) /* 4KB sector size */
#define AAC_OPTION_SUPPORTED_240_VOLUMES cpu_to_le32(0x10000000)
#define AAC_SIS_VERSION_V3	3
#define AAC_SIS_VERSION_V15	15
#define AAC_SIS_SLOT_UNKNOWN	0xFF

#define GetBusInfo 0x00000009
struct aac_bus_info {
	__le32	Command;	/* VM_Ioctl */
	__le32	ObjType;	/* FT_DRIVE */
	__le32	MethodId;	/* 1 = SCSI Layer */
	__le32	ObjectId;	/* Handle */
	__le32	CtlCmd;		/* GetBusInfo */
};

struct aac_bus_info_response {
	__le32	Status;		/* ST_OK */
	__le32	ObjType;
	__le32	MethodId;	/* unused */
	__le32	ObjectId;	/* unused */
	__le32	CtlCmd;		/* unused */
	__le32	ProbeComplete;
	__le32	BusCount;
	__le32	TargetsPerBus;
	u8	InitiatorBusId[10];
	u8	BusValid[10];
};

/*
 * Battery platforms
 */
#define AAC_BAT_REQ_PRESENT	(1)
#define AAC_BAT_REQ_NOTPRESENT	(2)
#define AAC_BAT_OPT_PRESENT	(3)
#define AAC_BAT_OPT_NOTPRESENT	(4)
#define AAC_BAT_NOT_SUPPORTED	(5)
/*
 * cpu types
 */
#define AAC_CPU_SIMULATOR	(1)
#define AAC_CPU_I960		(2)
#define AAC_CPU_STRONGARM	(3)

/*
 * Supported Options
 */
#define AAC_OPT_SNAPSHOT		cpu_to_le32(1)
#define AAC_OPT_CLUSTERS		cpu_to_le32(1<<1)
#define AAC_OPT_WRITE_CACHE		cpu_to_le32(1<<2)
#define AAC_OPT_64BIT_DATA		cpu_to_le32(1<<3)
#define AAC_OPT_HOST_TIME_FIB		cpu_to_le32(1<<4)
#define AAC_OPT_RAID50			cpu_to_le32(1<<5)
#define AAC_OPT_4GB_WINDOW		cpu_to_le32(1<<6)
#define AAC_OPT_SCSI_UPGRADEABLE	cpu_to_le32(1<<7)
#define AAC_OPT_SOFT_ERR_REPORT		cpu_to_le32(1<<8)
#define AAC_OPT_SUPPORTED_RECONDITION	cpu_to_le32(1<<9)
#define AAC_OPT_SGMAP_HOST64		cpu_to_le32(1<<10)
#define AAC_OPT_ALARM			cpu_to_le32(1<<11)
#define AAC_OPT_NONDASD			cpu_to_le32(1<<12)
#define AAC_OPT_SCSI_MANAGED		cpu_to_le32(1<<13)
#define AAC_OPT_RAID_SCSI_MODE		cpu_to_le32(1<<14)
#define AAC_OPT_2K_FIB_SUPPORT  cpu_to_le32(1<<15)
#define AAC_OPT_SUPPLEMENT_ADAPTER_INFO	cpu_to_le32(1<<16)
#define AAC_OPT_NEW_COMM		cpu_to_le32(1<<17)
#define AAC_OPT_NEW_COMM_64		cpu_to_le32(1<<18)
#define AAC_OPT_EXTENDED		cpu_to_le32(1<<23)
#define AAC_OPT_NATIVE_HBA		cpu_to_le32(1<<25)
#define AAC_OPT_NEW_COMM_TYPE1	cpu_to_le32(1<<28)
#define AAC_OPT_NEW_COMM_TYPE2	cpu_to_le32(1<<29)
#define AAC_OPT_NEW_COMM_TYPE3	cpu_to_le32(1<<30)
#define AAC_OPT_NEW_COMM_TYPE4	cpu_to_le32(1<<31)

#define	AAC_EXTOPT_FIB_64BIT	cpu_to_le32(1)
#define AAC_EXTOPT_SA_FIRMWARE	cpu_to_le32(1<<1)

/* MSIX context */
struct aac_msix_ctx {
	int		vector_no;
	struct aac_dev	*dev;
};

struct aac_dev
{
	struct list_head	entry;
	const char		*name;
	int			id;

	/*
	 *	negotiated FIB settings
	 */
	unsigned		max_fib_size;
	unsigned		sg_tablesize;
	unsigned		max_num_aif;

	unsigned		max_cmd_size;	/* max_fib_size or MAX_NATIVE */

	/*
	 *	Map for 128 fib objects (64k)
	 */
	dma_addr_t		hw_fib_pa;	/* also used for native cmd */
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	union hw_cmnd_ptr	hw_cmnd_va;
#else
	struct hw_fib		*hw_fib_va;	/* also used for native cmd */
#endif
	struct hw_fib		*aif_base_va;
	/*
	 *	Fib Headers
	 */
	struct fib              *fibs;

	struct fib		*free_fib;
	spinlock_t		fib_lock;

	struct aac_queue_block *queues;

	/*
	 *	The user API will use an IOCTL to register itself to receive
	 *	FIBs from the adapter.  The following list is used to keep
	 *	track of all the threads that have requested these FIBs.  The
	 *	mutex is used to synchronize access to all data associated
	 *	with the adapter fibs.
	 */
	struct list_head	fib_list;

	struct adapter_ops	a_ops;
	unsigned long		fsrev;		/* Main driver's revision number */

	unsigned long		dbg_base;	/* address of UART debug buffer */
	unsigned		base_size, dbg_size;	/* Size of mapped in region */
	union aac_init		*init;		/* Holds initialization info to communicate with adapter */
	dma_addr_t		init_pa;	/* Holds physical address of the init struct */

	__le32 *		host_rrq;	/* response queue (if AAC_COMM_MESSAGE_TYPE1) */
	dma_addr_t		host_rrq_pa;	/* phys. address */
	u32			host_rrq_idx[AAC_MAX_MSIX];
	atomic_t		rrq_outstanding[AAC_MAX_MSIX];
	u32*			msix_vector_tbl;
	u32			fibs_pushed_no;
	struct pci_dev		*pdev;		/* Our PCI interface */
	void *			printfbuf;	/* pointer to buffer used for printf's from the adapter */
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	spinlock_t		PrintQueueLock;
	struct list_head	PrintQueue;
	u32			DebugFlags;	/* Debug print flags bitmap */
	volatile u8 __iomem *	FwDebugBuffer_P;/* Addr FW Debug Buffer */
	volatile __le32 __iomem * FwDebugFlags_P;/* Addr FW Debug Flags */
	u32			FwDebugFlags;	/* FW Debug Flags */
	volatile __le32 __iomem * FwDebugStrLength_P;/* Addr FW Debug String Length */
	volatile u8 __iomem *	FwDebugBLEDflag_P;/* Addr FW Debug BLED */
	volatile u8 __iomem *	FwDebugBLEDvalue_P;/* Addr FW Debug BLED */
	u32			FwDebugBufferSize;/* FW Debug Buffer Size in Bytes */
#endif
	void *			comm_addr;	/* Base address of Comm area */
	dma_addr_t		comm_phys;	/* Physical Address of Comm area */
	size_t			comm_size;

	struct Scsi_Host	*scsi_host_ptr;
	int			maximum_num_containers;
	int			maximum_num_physicals;
	int			maximum_num_channels;
	struct fsa_dev_info	*fsa_dev;
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	pid_t			thread_pid;
#else
	struct task_struct	*thread;
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
	int			thread_die;
#endif
	int			cardtype;

	int 		logging_level;
	/*
	 *	This lock will protect the two 32-bit
	 *  writes to the Inbound Queue
	 */
	spinlock_t iq_lock;

	/*
	 *	The following is the device specific extension.
	 */
#if (!defined(AAC_MIN_FOOTPRINT_SIZE))
#define AAC_MIN_FOOTPRINT_SIZE	8192
#define AAC_MIN_SRC_BAR0_SIZE	0x400000
#define AAC_MIN_SRC_BAR1_SIZE	0x800
#define AAC_MIN_SRCV_BAR0_SIZE	0x100000
#define AAC_MIN_SRCV_BAR1_SIZE	0x400
#endif
	union
	{
		struct sa_registers __iomem *sa;
		struct rx_registers __iomem *rx;
		struct rkt_registers __iomem *rkt;
		struct {
			struct src_registers __iomem *bar0;
			char __iomem *bar1;
		} src;
	} regs;
	volatile void __iomem *base, *dbg_base_mapped;
#if (!defined(CONFIG_COMMUNITY_KERNEL) && defined(CONFIG_IA64))
	struct rx_inbound __iomem *IndexRegs;
#else
	volatile struct rx_inbound __iomem *IndexRegs;
#endif
	u32			OIMR; /* Mask Register Cache */
	/*
	 *	AIF thread states
	 */
	u32			aif_thread;
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	struct completion	aif_completion;
#endif
	struct aac_adapter_info adapter_info;
	struct aac_supplement_adapter_info supplement_adapter_info;
#if (defined(CODE_STREAM_IDENTIFIER) && !defined(CONFIG_COMMUNITY_KERNEL))
# if (!defined(MAX_CODE_STREAM_IDENTIFIER_LENGTH))
#  define MAX_CODE_STREAM_IDENTIFIER_LENGTH 64
# endif
	char			code_stream_identifier[MAX_CODE_STREAM_IDENTIFIER_LENGTH];
#endif
	/* These are in adapter info but they are in the io flow so
	 * lets break them out so we don't have to do an AND to check them
	 */
	u8			nondasd_support;
	u8			jbod;
	u8			cache_protected;
	u8			dac_support;
	u8			needs_dac;
	u8			raid_scsi_mode;
	u8			comm_interface;
#	define AAC_COMM_PRODUCER 0
#	define AAC_COMM_MESSAGE  1
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
#	define AAC_COMM_APRE	2
#endif
#	define AAC_COMM_MESSAGE_TYPE1	3    
#	define AAC_COMM_MESSAGE_TYPE2	4
#	define AAC_COMM_MESSAGE_TYPE3	5    
	u8			raw_io_interface;
	u8			raw_io_64;
	u8			printf_enabled;
	u8			in_reset;
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN) || ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN)))))
	/* Used to indicate the adapter is shut down */
	u8			shutdown;
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
	u8			msi;
#endif
	u8			simulated_scsi_error;
	u8			simulated_tgt_failure;
	u8			sa_firmware;
#if (defined(__ESXi4__))
	int			major_number; //ESXi4 support
#endif
	int			management_fib_count;
	spinlock_t		manage_lock;
	spinlock_t		sync_lock;
	int			sync_mode;
	struct fib		*sync_fib;
	struct list_head	sync_fib_list;
	u32			doorbell_mask;	/* from GET_ADAPTER_PROP */
	u32			max_msix;	/* max. MSI-X vectors */
	u32			vector_cap;	/* MSI-X vector capab.*/
	int			msi_enabled;	/* MSI/MSI-X enabled */
	atomic_t		msix_counter;
	int			streamlined_fib_support;
	int			fib_size_supported;
	struct msix_entry	msixentry[AAC_MAX_MSIX];
	struct aac_msix_ctx	aac_msix[AAC_MAX_MSIX]; /* context */
	struct aac_hba_map_info	hba_map[AAC_MAX_BUSES][AAC_MAX_TARGETS];
	struct aac_hba_map_info cur_hba_map[AAC_MAX_BUSES][AAC_MAX_TARGETS];
	u8			adapter_shutdown;
	u32			handle_pci_error;
};

#define aac_adapter_interrupt(dev) \
	(dev)->a_ops.adapter_interrupt(dev)

#define aac_adapter_notify(dev, event) \
	(dev)->a_ops.adapter_notify(dev, event)

#define aac_adapter_disable_int(dev) \
	(dev)->a_ops.adapter_disable_int(dev)

#define aac_adapter_enable_int(dev) \
	(dev)->a_ops.adapter_enable_int(dev)

#define aac_adapter_sync_cmd(dev, command, p1, p2, p3, p4, p5, p6, status, r1, r2, r3, r4) \
	(dev)->a_ops.adapter_sync_cmd(dev, command, p1, p2, p3, p4, p5, p6, status, r1, r2, r3, r4)
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

#define aac_adapter_sync_apre(dev, command, response) \
	(dev)->a_ops.adapter_sync_apre(dev, command, response)
#endif

#define aac_adapter_check_health(dev) \
	(dev)->a_ops.adapter_check_health(dev)

#define aac_adapter_restart(dev,bled) \
	(dev)->a_ops.adapter_restart(dev,bled)

#define aac_adapter_start(dev) \
	(dev)->a_ops.adapter_start(dev)

#define aac_adapter_ioremap(dev, size) \
	(dev)->a_ops.adapter_ioremap(dev, size)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
#define aac_adapter_intr(dev) \
	(dev)->a_ops.adapter_intr(dev->pdev->irq, (void *)dev, (struct pt_regs *)NULL)
#elif (defined(HAS_NEW_IRQ_HANDLER_T))
#define aac_adapter_intr(dev) (dev)->a_ops.adapter_intr((void *)dev)
#else
#define aac_adapter_intr(dev) \
	(dev)->a_ops.adapter_intr(dev->pdev->irq, (void *)dev)
#endif

#if defined(__ESX5__)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
#define aac_adapter_intr_poll(dev) \
	(dev)->a_ops.adapter_intr_poll(dev->pdev->irq, (void *)dev, (struct pt_regs *)NULL)
#elif (defined(HAS_NEW_IRQ_HANDLER_T))
#define aac_adapter_intr_poll(dev) (dev)->a_ops.adapter_intr_poll((void *)dev)
#else
#define aac_adapter_intr_poll(dev) \
	(dev)->a_ops.adapter_intr_poll(dev->pdev->irq, (void *)dev)
#endif
#endif

#define aac_adapter_deliver(fib) \
	((fib)->dev)->a_ops.adapter_deliver(fib)

#define aac_adapter_bounds(dev,cmd,lba) \
	dev->a_ops.adapter_bounds(dev,cmd,lba)

#define aac_adapter_read(fib,cmd,lba,count) \
	((fib)->dev)->a_ops.adapter_read(fib,cmd,lba,count)

#define aac_adapter_write(fib,cmd,lba,count,fua) \
	((fib)->dev)->a_ops.adapter_write(fib,cmd,lba,count,fua)

#define aac_adapter_scsi(fib,cmd) \
	((fib)->dev)->a_ops.adapter_scsi(fib,cmd)
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

#define aac_adapter_scsi_cmd(dev,cmd) \
	dev->a_ops.adapter_scsi_cmd(cmd)

#define aac_adapter_build_sg(fib,cmd,psg) \
	((fib)->dev)->a_ops.adapter_build_sg(fib,cmd,psg)
#endif

#define aac_adapter_comm(dev,comm) \
	(dev)->a_ops.adapter_comm(dev, comm)

#define FIB_CONTEXT_FLAG_TIMED_OUT		(0x00000001)
#define FIB_CONTEXT_FLAG			(0x00000002)
#define FIB_CONTEXT_FLAG_WAIT			(0x00000004)
#define FIB_CONTEXT_FLAG_FASTRESP		(0x00000008)
#define FIB_CONTEXT_FLAG_NATIVE_HBA		(0x00000010)
#define FIB_CONTEXT_FLAG_NATIVE_HBA_TMF		(0x00000020)

/*
 *	Define the command values
 */

#define		Null			0
#define		GetAttributes		1
#define		SetAttributes		2
#define		Lookup			3
#define		ReadLink		4
#define		Read			5
#define		Write			6
#define		Create			7
#define		MakeDirectory		8
#define		SymbolicLink		9
#define		MakeNode		10
#define		Removex			11
#define		RemoveDirectoryx	12
#define		Rename			13
#define		Link			14
#define		ReadDirectory		15
#define		ReadDirectoryPlus	16
#define		FileSystemStatus	17
#define		FileSystemInfo		18
#define		PathConfigure		19
#define		Commit			20
#define		Mount			21
#define		UnMount			22
#define		Newfs			23
#define		FsCheck			24
#define		FsSync			25
#define		SimReadWrite		26
#define		SetFileSystemStatus	27
#define		BlockRead		28
#define		BlockWrite		29
#define		NvramIoctl		30
#define		FsSyncWait		31
#define		ClearArchiveBit		32
#define		SetAcl			33
#define		GetAcl			34
#define		AssignAcl		35
#define		FaultInsertion		36	/* Fault Insertion Command */
#define		CrazyCache		37	/* Crazycache */

#define		MAX_FSACOMMAND_NUM	38


/*
 *	Define the status returns. These are very unixlike although
 *	most are not in fact used
 */

#define		ST_OK		0
#define		ST_PERM		1
#define		ST_NOENT	2
#define		ST_IO		5
#define		ST_NXIO		6
#define		ST_E2BIG	7
#define		ST_MEDERR	8
#define		ST_ACCES	13
#define		ST_EXIST	17
#define		ST_XDEV		18
#define		ST_NODEV	19
#define		ST_NOTDIR	20
#define		ST_ISDIR	21
#define		ST_INVAL	22
#define		ST_FBIG		27
#define		ST_NOSPC	28
#define		ST_ROFS		30
#define		ST_MLINK	31
#define		ST_WOULDBLOCK	35
#define		ST_NAMETOOLONG	63
#define		ST_NOTEMPTY	66
#define		ST_DQUOT	69
#define		ST_STALE	70
#define		ST_REMOTE	71
#define		ST_NOT_READY	72
#define		ST_BADHANDLE	10001
#define		ST_NOT_SYNC	10002
#define		ST_BAD_COOKIE	10003
#define		ST_NOTSUPP	10004
#define		ST_TOOSMALL	10005
#define		ST_SERVERFAULT	10006
#define		ST_BADTYPE	10007
#define		ST_JUKEBOX	10008
#define		ST_NOTMOUNTED	10009
#define		ST_MAINTMODE	10010
#define		ST_STALEACL	10011

/*
 *	On writes how does the client want the data written.
 */

#define	CACHE_CSTABLE		1
#define CACHE_UNSTABLE		2

/*
 *	Lets the client know at which level the data was commited on
 *	a write request
 */

#define	CMFILE_SYNCH_NVRAM	1
#define	CMDATA_SYNCH_NVRAM	2
#define	CMFILE_SYNCH		3
#define CMDATA_SYNCH		4
#define CMUNSTABLE		5


#define	RIO_TYPE_WRITE 			0x0000
#define	RIO_TYPE_READ			0x0001
#define	RIO_SUREWRITE			0x0008

#define RIO2_IO_TYPE			0x0003
#define RIO2_IO_TYPE_WRITE		0x0000
#define RIO2_IO_TYPE_READ		0x0001
#define RIO2_IO_TYPE_VERIFY		0x0002
#define RIO2_IO_ERROR			0x0004
#define RIO2_IO_SUREWRITE		0x0008  
#define RIO2_SGL_CONFORMANT		0x0010  
#define RIO2_SG_FORMAT			0xF000
#define RIO2_SG_FORMAT_ARC		0x0000
#define RIO2_SG_FORMAT_SRL		0x1000
#define RIO2_SG_FORMAT_IEEE1212	0x2000

struct aac_read
{
	__le32		command;
	__le32		cid;
	__le32		block;
	__le32		count;
	struct sgmap	sg;	// Must be last in struct because it is variable
};

struct aac_read64
{
	__le32		command;
	__le16		cid;
	__le16		sector_count;
	__le32		block;
	__le16		pad;
	__le16		flags;
	struct sgmap64	sg;	// Must be last in struct because it is variable
};

struct aac_read_reply
{
	__le32		status;
	__le32		count;
};

struct aac_write
{
	__le32		command;
	__le32		cid;
	__le32		block;
	__le32		count;
	__le32		stable;	// Not used
	struct sgmap	sg;	// Must be last in struct because it is variable
};

struct aac_write64
{
	__le32		command;
	__le16		cid;
	__le16		sector_count;
	__le32		block;
	__le16		pad;
	__le16		flags;
	struct sgmap64	sg;	// Must be last in struct because it is variable
};
struct aac_write_reply
{
	__le32		status;
	__le32		count;
	__le32		committed;
};

struct aac_raw_io
{
	__le32		block[2];
	__le32		count;
	__le16		cid;
	__le16		flags;		/* RIO flags */
	__le16		bpTotal;	/* reserved for F/W use */
	__le16		bpComplete;	/* reserved for F/W use */
	struct sgmapraw	sg;
};

struct aac_raw_io2
{
	__le32		blockLow;
	__le32		blockHigh;
	__le32		byteCount;
	__le16		cid;
	__le16		flags;		/* RIO2 flags */
	__le32		sgeFirstSize;	/* size of first sge el. */
	__le32		sgeNominalSize;	/* size of 2nd sge el. (if conformant) */
	u8		sgeCnt;		/* only 8 bits required */
	u8		bpTotal;	/* reserved for F/W use */
	u8		bpComplete;	/* reserved for F/W use */
	u8		sgeFirstIndex;	/* reserved for F/W use */
	u8		unused[4];
	struct sge_ieee1212	sge[1];
};

#define CT_FLUSH_CACHE 129
struct aac_synchronize {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_FLUSH_CACHE */
	__le32		cid;
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		count;	/* sizeof(((struct aac_synchronize_reply *)NULL)->data) */
};

struct aac_synchronize_reply {
	__le32		dummy0;
	__le32		dummy1;
	__le32		status;	/* CT_OK */
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	u8		data[16];
};

#define CT_POWER_MANAGEMENT	245
#define CT_PM_START_UNIT	2
#define CT_PM_STOP_UNIT		3
#define CT_PM_UNIT_IMMEDIATE	1
struct aac_power_management {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_POWER_MANAGEMENT */
	__le32		sub;		/* CT_PM_* */
	__le32		cid;
	__le32		parm;		/* CT_PM_sub_* */
};

#define CT_PAUSE_IO    65
#define CT_RELEASE_IO  66
struct aac_pause {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_PAUSE_IO */
	__le32		timeout;	/* 10ms ticks */
	__le32		min;
	__le32		noRescan;
	__le32		parm3;
	__le32		parm4;
	__le32		count;	/* sizeof(((struct aac_pause_reply *)NULL)->data) */
};

struct aac_srb
{
	__le32		function;
	__le32		channel;
	__le32		id;
	__le32		lun;
	__le32		timeout;
	__le32		flags;
	__le32		count;		// Data xfer size
	__le32		retry_limit;
	__le32		cdb_size;
	u8		cdb[16];
	struct	sgmap	sg;
};

/*
 * This and associated data structs are used by the
 * ioctl caller and are in cpu order.
 */
struct user_aac_srb
{
	u32		function;
	u32		channel;
	u32		id;
	u32		lun;
	u32		timeout;
	u32		flags;
	u32		count;		// Data xfer size
	u32		retry_limit;
	u32		cdb_size;
	u8		cdb[16];
	struct	user_sgmap	sg;
};

#define		AAC_SENSE_BUFFERSIZE	 30

struct aac_srb_reply
{
	__le32		status;
	__le32		srb_status;
	__le32		scsi_status;
	__le32		data_xfer_length;
	__le32		sense_data_size;
	u8		sense_data[AAC_SENSE_BUFFERSIZE]; // Can this be SCSI_SENSE_BUFFERSIZE
};
/*
 * SRB Flags
 */
#define		SRB_NoDataXfer		 0x0000
#define		SRB_DisableDisconnect	 0x0004
#define		SRB_DisableSynchTransfer 0x0008
#define		SRB_BypassFrozenQueue	 0x0010
#define		SRB_DisableAutosense	 0x0020
#define		SRB_DataIn		 0x0040
#define		SRB_DataOut		 0x0080

/*
 * SRB Functions - set in aac_srb->function
 */
#define	SRBF_ExecuteScsi	0x0000
#define	SRBF_ClaimDevice	0x0001
#define	SRBF_IO_Control		0x0002
#define	SRBF_ReceiveEvent	0x0003
#define	SRBF_ReleaseQueue	0x0004
#define	SRBF_AttachDevice	0x0005
#define	SRBF_ReleaseDevice	0x0006
#define	SRBF_Shutdown		0x0007
#define	SRBF_Flush		0x0008
#define	SRBF_AbortCommand	0x0010
#define	SRBF_ReleaseRecovery	0x0011
#define	SRBF_ResetBus		0x0012
#define	SRBF_ResetDevice	0x0013
#define	SRBF_TerminateIO	0x0014
#define	SRBF_FlushQueue		0x0015
#define	SRBF_RemoveDevice	0x0016
#define	SRBF_DomainValidation	0x0017

/*
 * SRB SCSI Status - set in aac_srb->scsi_status
 */
#define SRB_STATUS_PENDING                  0x00
#define SRB_STATUS_SUCCESS                  0x01
#define SRB_STATUS_ABORTED                  0x02
#define SRB_STATUS_ABORT_FAILED             0x03
#define SRB_STATUS_ERROR                    0x04
#define SRB_STATUS_BUSY                     0x05
#define SRB_STATUS_INVALID_REQUEST          0x06
#define SRB_STATUS_INVALID_PATH_ID          0x07
#define SRB_STATUS_NO_DEVICE                0x08
#define SRB_STATUS_TIMEOUT                  0x09
#define SRB_STATUS_SELECTION_TIMEOUT        0x0A
#define SRB_STATUS_COMMAND_TIMEOUT          0x0B
#define SRB_STATUS_MESSAGE_REJECTED         0x0D
#define SRB_STATUS_BUS_RESET                0x0E
#define SRB_STATUS_PARITY_ERROR             0x0F
#define SRB_STATUS_REQUEST_SENSE_FAILED     0x10
#define SRB_STATUS_NO_HBA                   0x11
#define SRB_STATUS_DATA_OVERRUN             0x12
#define SRB_STATUS_UNEXPECTED_BUS_FREE      0x13
#define SRB_STATUS_PHASE_SEQUENCE_FAILURE   0x14
#define SRB_STATUS_BAD_SRB_BLOCK_LENGTH     0x15
#define SRB_STATUS_REQUEST_FLUSHED          0x16
#define SRB_STATUS_DELAYED_RETRY	    0x17
#define SRB_STATUS_INVALID_LUN              0x20
#define SRB_STATUS_INVALID_TARGET_ID        0x21
#define SRB_STATUS_BAD_FUNCTION             0x22
#define SRB_STATUS_ERROR_RECOVERY           0x23
#define SRB_STATUS_NOT_STARTED		    0x24
#define SRB_STATUS_NOT_IN_USE		    0x30
#define SRB_STATUS_FORCE_ABORT		    0x31
#define SRB_STATUS_DOMAIN_VALIDATION_FAIL   0x32
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

struct aac_apre_element_rx {
	__le32	physAddrLow;
	__le32	physAddrHigh;
	__le32	elementByteCount;
	u8	reserver0[3];
	u8	reserved1:4;
	u8	domainSelect:2;
	u8	endOfList:1;
	u8	endOfSubList:1;
};

struct aac_apre_element_link {
	__le32	physAddrLow;
	__le32	physAddrHigh;
	__le32	reserved0[2];
};

struct aac_apre_element_nark {
	__le32	elementByteCount:24;
	__le32	domainSelect:4;
#define APRE_OFF_CHIP_MEM_DOMAIN 0x0
	__le32	elementType:4;
#define APRE_BUFFER_DESC_ENTRY   0x0
#define APRE_BUFFER_DESC_POINTER 0x4
	__le32	physAddrLow;
	__le32	physAddrHigh;
};

union aac_apre_embedded_sglist {
	struct {
		struct aac_apre_element_rx	FirstElement;
		struct aac_apre_element_rx	SecondElement;
		struct aac_apre_element_link	ThirdElement;
	} rx;
	struct {
		struct aac_apre_element_nark	FirstElement;
		struct aac_apre_element_nark	SecondElement;
		struct aac_apre_element_nark	ThirdElement;
	} nark;
};

#define AS_REQ_LKP_CODE_EXEC_SCSI_TASK ((u16)0x600)

struct aac_apre_srb {
	struct aac_apre_hdr header;
#define AAC_DATA_IN_IOP_PERSPECTIVE  0x02
#define AAC_DATA_OUT_IOP_PERSPECTIVE 0x01
	u8	DataDir:2;
	u8	NumEsge:2;
	u8	CDBLength:4;
	u8	Ind_Cdb:1;
	u8	Reserved0:3;
	u8	Task_Attr:3;
	u8	Reserved1:1;
	u8	Task_Pri:4;
	u8	Reserved2:4;
	u8	Reserved3;
	u8	Cdb[16];
	__le32	TransferSizeLow;
	__le32	TransferSizeHigh;
	u8	Pad[24];
	union aac_apre_embedded_sglist Sgl;
};
#endif

/*
 * Object-Server / Volume-Manager Dispatch Classes
 */

#define		VM_Null			0
#define		VM_NameServe		1
#define		VM_ContainerConfig	2
#define		VM_Ioctl		3
#define		VM_FilesystemIoctl	4
#define		VM_CloseAll		5
#define		VM_CtBlockRead		6
#define		VM_CtBlockWrite		7
#define		VM_SliceBlockRead	8	/* raw access to configured "storage objects" */
#define		VM_SliceBlockWrite	9
#define		VM_DriveBlockRead	10	/* raw access to physical devices */
#define		VM_DriveBlockWrite	11
#define		VM_EnclosureMgt		12	/* enclosure management */
#define		VM_Unused		13	/* used to be diskset management */
#define		VM_CtBlockVerify	14
#define		VM_CtPerf		15	/* performance test */
#define		VM_CtBlockRead64	16
#define		VM_CtBlockWrite64	17
#define		VM_CtBlockVerify64	18
#define		VM_CtHostRead64		19
#define		VM_CtHostWrite64	20
#define		VM_DrvErrTblLog		21
#define		VM_NameServe64		22
#define		VM_GetDynAdapProps	25
#define		VM_SetDynAdapProps	26
#define		VM_NameServeAllBlk	30

#define		MAX_VMCOMMAND_NUM	23	/* used for sizing stats array - leave last */

/*
 *	Descriptive information (eg, vital stats)
 *	that a content manager might report.  The
 *	FileArray filesystem component is one example
 *	of a content manager.  Raw mode might be
 *	another.
 */

struct aac_fsinfo {
	__le32  fsTotalSize;	/* Consumed by fs, incl. metadata */
	__le32  fsBlockSize;
	__le32  fsFragSize;
	__le32  fsMaxExtendSize;
	__le32  fsSpaceUnits;
	__le32  fsMaxNumFiles;
	__le32  fsNumFreeFiles;
	__le32  fsInodeDensity;
};	/* valid iff ObjType == FT_FILESYS && !(ContentState & FSCS_NOTCLEAN) */

struct  aac_blockdevinfo {
	__le32	block_size;
	__le32	logical_phys_map;
	u8	identifier[16];
};

union aac_contentinfo {
	struct	aac_fsinfo		filesys;	/* valid iff ObjType == FT_FILESYS && !(ContentState & FSCS_NOTCLEAN) */
	struct	aac_blockdevinfo	bdevinfo;
};

/*
 *	Query for Container Configuration Status
 */

#define CT_GET_CONFIG_STATUS 147
struct aac_get_config_status {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_GET_CONFIG_STATUS */
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	__le32		count;	/* sizeof(((struct aac_get_config_status_resp *)NULL)->data) */
};

#define CFACT_CONTINUE 0
#define CFACT_PAUSE    1
#define CFACT_ABORT    2
struct aac_get_config_status_resp {
	__le32		response; /* ST_OK */
	__le32		dummy0;
	__le32		status;	/* CT_OK */
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	struct {
		__le32	action; /* CFACT_CONTINUE, CFACT_PAUSE or CFACT_ABORT */
		__le16	flags;
		__le16	count;
	}		data;
};

/*
 *	Accept the configuration as-is
 */

#define CT_COMMIT_CONFIG 152
struct aac_commit_config {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_COMMIT_CONFIG */
};

/*
 *	Query for Container Configuration Status
 */

#define CT_GET_CONTAINER_COUNT 4
struct aac_get_container_count {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_GET_CONTAINER_COUNT */
};

struct aac_get_container_count_resp {
	__le32		response; /* ST_OK */
	__le32		dummy0;
	__le32		MaxContainers;
	__le32		ContainerSwitchEntries;
	__le32		MaxPartitions;
	__le32		MaxSimpleVolumes;
};


/*
 *	Query for "mountable" objects, ie, objects that are typically
 *	associated with a drive letter on the client (host) side.
 */

struct aac_mntent {
	__le32			oid;
	u8			name[16];	/* if applicable */
	struct creation_info	create_info;	/* if applicable */
	__le32			capacity;
	__le32			vol;		/* substrate structure */
	__le32			obj;		/* FT_FILESYS, etc. */
	__le32			state;		/* unready for mounting,
						   readonly, etc. */
	union aac_contentinfo	fileinfo;	/* Info specific to content
						   manager (eg, filesystem) */
	__le32			altoid;		/* != oid <==> snapshot or
						   broken mirror exists */
	__le32			capacityhigh;
};

#define FSCS_NOTCLEAN	0x0001  /* fsck is necessary before mounting */
#define FSCS_READONLY	0x0002	/* possible result of broken mirror */
#define FSCS_HIDDEN	0x0004	/* should be ignored - set during a clear */
#define FSCS_NOT_READY	0x0008	/* Array spinning up to fulfil request */

struct aac_query_mount {
	__le32		command;
	__le32		type;
	__le32		count;
};

struct aac_mount {
	__le32		status;
	__le32		type;           /* should be same as that requested */
	__le32		count;
	struct aac_mntent mnt[1];
};

#define CT_READ_NAME 130
struct aac_get_name {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_READ_NAME */
	__le32		cid;
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		count;	/* sizeof(((struct aac_get_name_resp *)NULL)->data) */
};

struct aac_get_name_resp {
	__le32		dummy0;
	__le32		dummy1;
	__le32		status;	/* CT_OK */
	__le32		parm1;
	__le32		parm2;
	__le32		parm3;
	__le32		parm4;
	__le32		parm5;
	u8		data[16];
};

#define CT_CID_TO_32BITS_UID 165
struct aac_get_serial {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_CID_TO_32BITS_UID */
	__le32		cid;
};

struct aac_get_serial_resp {
	__le32		dummy0;
	__le32		dummy1;
	__le32		status;	/* CT_OK */
	__le32		uid;
};

struct aac_container_cmd {
	__le32		command;	/* VM_ContainerConfig */
	__le32		type;		/* CT_xxx */
	__le32		handle;		/* cid, pid, ... */
	__le32		parm1;
	__le32		parm2;
	__le32		cnt_id;
	__le32		cnt_offset;
	__le32		cnt_size;	/* total expected size */
	u8              data[CT_PACKET_SIZE_VAR_FIB(FIB_SIZE_STANDARD)];
};

struct aac_container_resp {
	__le32		response;	/* ST_OK etc. */
	__le32		type;		/* usually echos command "type" */
	__le32		status;		/* CT_OK etc. */
	__le32		cnt_left;	/* bytes left to be read */
	__le32		dummy1;		/* echos "status" */
	__le32		cnt_id;		/* continuation ID */
	__le32		dummy2;		/* echos "cnt_offset" */
	__le32		dummy3;		/* echos "cnt_size" */
	u8		data[CT_PACKET_SIZE_VAR_FIB(FIB_SIZE_STANDARD)];
};

#define CT_GET_PHYDEV_LIST	247
#define CT_GET_PHYDEV_INFO	248
struct aac_phydev_list_resp {
	__le32	numPhyDev; /* Number of physical devices returned in this resp. */
	__le32	phyDev[1]; /* Physical device identifiers */
};

struct aac_standard_inq	{
	u8	devtype;
	u8	rmb;
	u8	version;
	u8	data_format;
	u8	additionalLength;
	u8	bit_field1;
	u8	bit_field2;
	u8	bit_field3;
	u8	vendorID[8];
	u8	productID[16];
	u8	revision[4];
	u8	vendorSpecific[20];
	u8	bit_field4;
	u8	reserved4;
	__le16	versionDescriptor[8];
	u8	reserved5[22];	
};	/* 96 bytes */

struct aac_sas_phy_info	{
	u8	reserved0;
	u8	phyIdentifier;
	u8	phyOperation;
	u8	outstandingIos;
	u8	attachedDeviceType;
	u8	negotiatedLinkRate;
	u8	attachedInitiatorProtocols;
	u8	attachedTargetProtocols;
	u8	sasAddress[8];
	u8	attachedSASAddress[8];
	u8	attachedPhyIdentifier;
	u8	reserved25[7];
	u8	minimumLinkRate;
	u8	maximumLinkRate;
	u8	reserved34[8];
	u8	vendorSpecific[2];
	u8	it_nexus[4];	/* Native HBA device handle */
};	/* 48 bytes */

struct aac_smart_info {
	u8	smartSupported;		/* 1 == SMART is supported, 0 = SMART is not supported */
	u8	smartEnabled;		/* 1 == SMART is enabled, 0 = SMART is not enabled */

	/* --- Mode page 0x1C start --- */
	u8	bit_field1;
	u8	bit_field2;
	__le32	intervalTimer;
	__le32	reportCount;
	/* --- Mode page 0x1C end --- */

	__le32	errorCount;
};	/* 16 bytes */

struct aac_phydev_info_resp {
	__le32	supportedOptions1;		/* Expansion bits: Add support for for all future expansion of structure. */
	__le32	supportedOptions2;		/* Expansion bits (future expansion) */
	__le32	handle;					/* handle to device */
	__le32	bus;					/* Bus */
	__le32	target;					/* Target */
	__le32	slice;					/* Slice */
	__le64	lun;					/* Lun */
	__le32	negotiatedSpeedMBSec;	/* True neg. speed. ScsiRate does not account for wide and is only 8 bits. */
	u8		negotiatedBusType;		/* Negotiated Bus Type (ARCIO_PHYDEV_BUS_TYPE_T) */
	u8		deviceType;				/* Device type (ARCIO_PHYDEV_DEV_TYPE_T) */
	u8		wrCacheSetting;			/* Write cache setting (ARCIO_PHYDEV_WCACHE_STATE_T) */
	u8		enclNvsramIndex;		/* in case of SEP device */
	__le64	totalNumBlocks;			/* The actual capacity of the physical device */
	__le64	usableBlocks;			/* Subtracts drive coercing and metadata */ 
	__le64	maxUsableBlocks;		/* Subtracts metadata */
	__le32	blockSize;			/* Block size */

	__le32	bDead     :1;			/* Device is dead */
	__le32	bBlinking    :1;		/* Device is blinking */
	__le32	bGlobalHS    :1;		/* Device is global hot spare */
	__le32	bDedicatedHS :1;		/* Device is a dedicated hot spare */
	__le32	bAacManaged  :1;		/* Set to 1 if AAC managed (should always be set) */
	__le32	bSataNCQ     :1;		/* SATA device supports NCQ */
	__le32	bFsaInitialized 	:1;	/* Device has been initialized for use */
	__le32	bNonRotatingMedia	:1;	/* Device is a Non Spinning Device */
	__le32	bItpRrcCapable		:1;	/* This Drive can be used for ITP features */
	__le32	bItpRrcAssigned		:1;	/* This Drive is Assigned to an ITP-RRC pool */
	__le32	bCopybackSource		:1;	/* This is Copyback Source drive */
	__le32	bCopybackTarget		:1;	/* This is Copyback Target drive */
	__le32	bATAsecuritySupport 	:1; 	/* Device Supports ATA SECURITY operations */
	__le32	bATAsecurityLock	:1;	/* ATA device is Security Locked */
	__le32	bOsPartitionPresent 	:1;	/* OS partition is present. Used in Denali */     
	__le32	bNativeHbaCapable	:1;	/* device is connected to Native HBA port */
	__le32	bNativeHbaEnabled	:1;	/* Device operation mode (0=ARC IO, 1=Native HBA IO) */
	__le32	reservedBool 		:15;	/* Reserved boolean bits for growth */

	__le64	fingerPrint;			/* ARC specific unique fingerprint */

	struct aac_standard_inq	inquiry;	/* SCSCI inquiry page */
	struct aac_sas_phy_info	sasPhy[2];	/* SAS PHY details */
	struct aac_smart_info	smart;		/* SMART details */

	u8		wwn[16];		/* Inquiry VPD page 0x83: 8, 12, or 16 byte WWN */
	u8		serialNum[20];		/* Unit serial # */
	u8		sataExtendedRevision[4]; /* Additional 4 Bytes for SATA drive firmware */

	__le32	bit_field2;

	__le32	unsupportedDeviceReason;	/* Set to a non zero value for devices that are not supported. */

	__le32	taskType;			/* Type of task running on this device (ARCIO_PHYDEV_TASK_TYPE_T) */
	__le32	taskProgress;			/* Progress of the active task (0 - 100) */
	__le32	taskState;			/* Task state (ARCIO_PHYDEV_TASK_STATE_T) */
	__le32	taskPriority;			/* Task priority */

	__le32	pwrMgtState;			/* Power management state (ARCIO_PHYDEV_PWR_STATE_T) */
	__le32	pwrMgtSupportOpts;		/* Power management support options (ARCIO_BF_PWR_*) */

	__le32	sataPort;
	__le32	sataPortMultiplier;

	__le32	beginMDBlockCount;		/* Metadata block count beginning of drive */
	__le32	endMDBlockCount;		/* Metadata block count end of drive */

	__le32	hostBusPhy0;			/* Host Bus Id(phy0) */
	__le32	hostTargetPhy0;			/* Host Target Id(phy0) */
	__le32	hostLunPhy0;			/* Host Lun Id(phy0) */

	__le32	hostBusPhy1;			/* Host Bus Id(phy1) */
	__le32	hostTargetPhy1;			/* Host Target Id(phy1) */
	__le32	hostLunPhy1;			/* Host Lun Id(phy1) */

	__le32	connMask;			/* controller connection */

	__le32 fill[5];		/* Reserved space fill (pads to 432 bytes) */
};

/*
 *  Dynamic adapter properties returned from GetDynAdapPropsFIB or
 *  used in SetDynAdapPropsFIB 
 */
#define DYNPROPS_FIB_DATA_SIZE_IN_BYTES 480
struct aac_dyn_adap_props_data {
	__le32	task_priority;
	__le32	property_bits;

	u8		deviceCachePolicy;	/* adapter wide device cache policy */
	u8		rrcDrivesInUse;		/* Number of ITP-RRC drive */
	__le16	MaxCacheFetchFlushRate;      

	__le16	batteryTemperature;	/* PICBAT_STATUS_IOCTL.temperature */

	u8		rBalanceFactor;		/* read Balancefactor for rrc cache strat. */
	u8		wBalanceFactor;		/* write Balancefactor for rrc cache strat. */

	__le32	batteryStatusBits;			/* PICIOCTL_STAT_* */
	__le32	batteryCapabilitiesBits;	/* PICIOCTL_CAPA_* */
	__le16	batteryFullCapacity;		/* PICBAT_STATUS_IOCTL.full_capacity */
	__le16	batteryRemainingCapacity;	/* PICBAT_STATUS_IOCTL.remaining_capacity */
	__le16	batteryDRAMCurrentDraw;		/* PICBAT_STATUS_IOCTL.DRAM_current_draw */
	__le16	dataScrubRate;				/* _E_BG_VERIFY_RATE or in days */

	__le16	heatSensorTemp;				/* ADP_ENV_INFO.temperature */
	__le16	heatSensorThresholdLo;		/* ADP_ENV_INFO.thresholdLo */

	__le16	heatSensorThresholdHi;		/* ADP_ENV_INFO.thresholdHi */
    u8		biosHaltOnMissingDriveCount;  /* MSFT */
	u8		reserved4;

	__le32	keyedOptions;			/* Keyable_Features_Response_FIB.presentlyEnabledKeyableOptions */
	__le32	performanceMode;		/* Active performance mode */

	__le32	pm_bits;
	__le32	pmStayAwakeStart;			/* Power management daily stay awake start time (in minutes since midnight) */
	__le32	pmStayAwakeEnd;				/* Power management daily stay awake end time (in minutes since midnight) */
	__le32	pmSpinupLimitInternal;		/* Maximum number of internal drives to spin up simultaneously */
	__le32	pmSpinupLimitExternal;		/* Maximum number of external drives to spin up simultaneously */
	__le32	pmUTCToLocalTimeDiff;		/* Time zone value used in FW and set by the BIOS and OS utilities (in minutes) */
	__le32	AdapterTime;				/* Time (local) in secs since 00:00:00 1 Jan 1970 (Unix Epoch) */
	u8		MaxCacheDirtyThreshold;		/* Number of ITP-RRC drive currently configured into the RRC-Pool */
	u8		ControllerFunctionMode;	    /* See ARCIO_CONTROLLER_FUNCTION_MODE_T for values */
	u8		reserved6 [2];
	u8		OsBootDevId[8];

    /* backup unit parameters */
	__le32	bu_bits;
	/* Temperature */
	__le16	buFuelPresentTemperature;		/* Current temperature in C */
	__le16	buFuelTemperatureThreshold;  	/* User threshold for Temperature in C */
	__le16	buFuelLifeTimeMaxTemperature;	/* Life-time max temperature recorded */
	__le16	buFuelLifeTimeMinTemperature;	/* Life-time min temperature recorded */
												
	/* voltage */
	__le16	buFuelPresentVoltage;		/* supercap's Present voltage in mV */
	__le16	buFuelMaxVoltage;			/* Maximum voltage supercap may have in mV */
	__le16	buFuelLifeTimeMaxVoltage;	/* Life-time max voltage recorded */

	/* current */
	__le16	buFuelCurrentDraw;		/* Current draw in mA */
	__le16	buFuelMaxCurrentDraw;	/* Max Current draw limit in mA */ 

	/* misc */
    __le16	buHealth;				/* health level in % */
    __le16	buErrorCode;			/* General error code */
	__le16	buHwType;				/* HW type */

	__le16	buFuelChargeLevel; 		/* Current level of energy in % */
	__le32	buFuelLifeTimeEstimate;	/* Supercap life-time estimation in days */
	u8		buFuelSerialNumber[16];	/* AFM board serial number in ascii */
	
	u8		Rsvd[2];		/* For AFM future use */
	u8		maxSasPhyLinkRate;	/* max phy link rate on S8 cards (2014.1) */
	u8		severityHistLog;	/* severity for smart history logging (2014.1) */
		
	__le32	cable_bits;								
	__le16	rescan_status;	/* 1 - running, 0 - completed, any other value - not running. */

	/* Smart HBA: connector settings */
	__le32	connHBAmask;	/* 0xf: supports native HBA dev., 0 otherwise */
	__le32	connRAIDmask;	/* 0xf: support RAID dev., 0 otherwise */
	__le32	connMode;	/* 0xf: RAID use, 0: native HBA use */
	__le32	connChangeMask;
};

/*
 * Index(bit position) into validFieldsBitMap used to indicate which fields are valid
 */
enum aac_dyn_adap_valid_type
{
	ARCIO_DYN_ADAP_DEFAULT_CTR_TASK_PRIORITY_VALID,		/* bit 0 (byte 0) */
	ARCIO_DYN_ADAP_ALARM_STATE_VALID,
	ARCIO_DYN_ADAP_AUTO_FAILOVER_ENABLED_VALID,
	ARCIO_DYN_ADAP_DEVICE_CACHE_POLICY_VALID,
	ARCIO_DYN_ADAP_BATTERY_DATA_VALID,
	ARCIO_DYN_ADAP_DATA_SCRUBBING_ACTIVE_VALID,
	ARCIO_DYN_ADAP_DATA_SCRUB_RATE_VALID,
	ARCIO_DYN_ADAP_COPYBACK_ENABLED_VALID,				/* bit 7 (byte 0) */

	ARCIO_DYN_ADAP_HEAT_SENSOR_DATA_VALID,				/* bit 8 (byte 1) */
	ARCIO_DYN_ADAP_KEYED_OPTIONS_VALID,
	ARCIO_DYN_ADAP_SELECTABLE_PERF_MODE_VALID,
	ARCIO_DYN_ADAP_STATISTIC_DATA_COLLECTION_VALID,

	ARCIO_DYN_ADAP_PM_ACTIVE_FLAGS_VALID,		/* pmEnabled, pmActive, pmStayAwakeActive */
	ARCIO_DYN_ADAP_PM_QUALIFIER_FLAGS_VALID,	/* pmTimeQualifierUTC, pmTimeQualifierLocal */
	ARCIO_DYN_ADAP_PM_STAY_AWAKE_VALID,			/* pmStayAwakeStart and pmStayAwakeEnd */
	ARCIO_DYN_ADAP_PM_SPINUP_VALID,				/* pmSpinupLimitInternal and pmSpinupLimitExternal */
	ARCIO_DYN_ADAP_PM_TIME_DIFF_VALID,			/* pmUTCToLocalTimeDiff */
	
	ARCIO_DYN_ADAP_LOCAL_TIME_VALID,			/* localTime */

	ARCIO_DYN_ADAP_PM_STAY_AWAKE_DOW_VALID,		/* pmStayAwakeStart and pmStayAwakeEnd
												   pmStayAwakeMainEnable, pmStayAwakeDaysOfWeek */

	ARCIO_DYN_ADAP_SATA_NCQ_ENABLED_VALID,		/* SataNCQEnabled */

	ARCIO_DYN_ADAP_RRCCNT_VALID,				/* rrcDrivesInUse */

	ARCIO_DYN_ADAP_MAXCACHE_FETCH_FLUSH_RATE_VALID,		/* Fetch rate option enabled */

	ARCIO_DYN_ADAP_RW_BALANCE_FACTORS_VALID,		/* RW Balance Factors */

    ARCIO_DYN_ADAP_MAXCACHE_DIRTYTHRESHOLD_VALID,		/* MaxcacheDirtyThreshold enabled */

	ARCIO_DYN_ADAP_MAXCACHE_NONREDUNDANT_WC,       /* Write-Cache support with non-redundant Cache pools - (0/1-Disabled/Enabled) */

    ARCIO_DYN_ADAP_CONTROLLER_FUNCTION_MODE_VALID,   /* HBA mode */

    ARCIO_DYN_ADAP_OS_BOOT_DEV_ID_VALID,

	ARCIO_DYN_ADAP_BACKUP_MANAGER_VALID,				/* bit 27 - Backup Unit Manager supported */
	ARCIO_DYN_ADAP_BACKUP_UNIT_STATUS_VALID,
    ARCIO_DYN_ADAP_BACKUP_MANAGER_TEMPERATURE_THRESHOLD_VALID,
    ARCIO_DYN_ADAP_BACKUP_HEALTH_ALERT_VALID,
	ARCIO_DYN_ADAP_BACKUP_TEMPREATURE_ALERT_VALID,

	ARCIO_DYN_ADAP_MAXCACHE_COHERENCY_CHECK_VALID,		/* max cache background coherency check */

	ARCIO_DYN_ADAP_BACKUP_LTE, 	/* bit 33 - Backup unit manager Life-time estimation support */
	
	ARCIO_DYN_ADAP_RESCAN_STATUS_VALID,

	ARCIO_DYN_ADAP_DDR_CACHE_PRESERVATION_STATUS_VALID,

	ARCIO_DYN_ADAP_CONTROLLER_STATUS_VALID,

	ARCIO_DYN_ADAP_CABLE_TYPE_SETTING,

	ARCIO_DYN_ADAP_BIOS_HALT_MISSING_DRIVE_COUNT_VALID,

	ARCIO_DYN_ADAP_HISTORY_LOG_SEVERITY_SETTING_VALID,	/* severity setting for smart history logging (2014.1) */

	ARCIO_DYN_ADAP_MAX_SAS_PHY_LINK_RATE_VALID,			/* max phy link rate on S8 cards (2014.1) */

	ARCIO_DYN_ADAP_SHBA_CTRL_CONN_MODE_VALID,			/* Connector mode config for Smart-HBA */

	ARCIO_DYN_ADAP_LAST	/* Do not remove!  Insert new property indexes above this line */
};

#define VALID_FIELDS_BITMAP_COUNT 32
#define VALID_FIELDS_BITMAP_SIZE_IN_BYTES (sizeof(u8) * VALID_FIELDS_BITMAP_COUNT)

struct aac_dyn_adap_props
{
	__le32	vmCommand;	/* VM_GetDynAdapProps/VM_SetDynAdapProps */
	u8	validFieldsBitMap[VALID_FIELDS_BITMAP_COUNT];	
	/* Bit map (up to 32*8 = 256 fields) indicating which properties
	(See ARCIO_DYN_ADAP_PROPS_VALID_T) are valid within the 'data' struct */
	enum aac_dyn_adap_valid_type	fieldUsedForSet; /* When host calls with SetDynAdapPropsFIB, indicates field/property to set */
	__le32	returnStatus;	/* Usually a _E_CT_GENERAL_STATUS, but can be other return status (ex: ALARM_STATUS) */

	struct aac_dyn_adap_props_data	data;	/* actual dynamic data */
	/* Pad used to guarantee interface struct fits within a FIB. */
	u8	pad[DYNPROPS_FIB_DATA_SIZE_IN_BYTES - sizeof(__le32) - VALID_FIELDS_BITMAP_SIZE_IN_BYTES - sizeof(enum aac_dyn_adap_valid_type) - sizeof(__le32) - sizeof(struct aac_dyn_adap_props_data)];
};

/* Convenience macros to set/get valid properties
 * Takes a pointer to a ARCIO_DYN_ADAP_PROPS_INTERFACE_T object and a property index (ARCIO_DYN_ADAP_PROPS_VALID_T) and sets the
 * corresponding bit in the validFieldsBitMap
 */ 
#define ARCIO_DYN_ADAP_PROPS_SET_VALID(pInterface, propIndex) \
	(((propIndex)/8 < 32 && (propIndex) < ARCIO_DYN_ADAP_LAST) ? (*(pInterface)).validFieldsBitMap[(propIndex)/8] |= (1 << ((propIndex) % 8)) : 0)

/* Takes a pointer to a ARCIO_DYN_ADAP_PROPS_INTERFACE_T object and a property index (ARCIO_DYN_ADAP_PROPS_VALID_T) and returns
 * non-zero if property is set in the validFieldsBitMap
 */ 
#define ARCIO_DYN_ADAP_PROPS_GET_VALID(pInterface, propIndex) \
	(((propIndex)/8 < 32 && (propIndex) < ARCIO_DYN_ADAP_LAST) ? (*(pInterface)).validFieldsBitMap[(propIndex)/8] & (1 << ((propIndex) % 8)) : 0)

#define ARCIO_CONTROLLER_FUNCTION_RAID		 	  	(0x00)
#define ARCIO_CONTROLLER_FUNCTION_HBA		  		(0x01)
#define ARCIO_CONTROLLER_FUNCTION_EXPOSE_RAW_DEVICES		(0x02)
#define ARCIO_CONTROLLER_FUNCTION_AUTO_VOLUME	  		(0x04)
#define ARCIO_CONTROLLER_FUNCTION_SIMPLE_VOLUME_240	  	(0x08)
#define ARCIO_CONTROLLER_FUNCTION_NATIVE_HBA	  		(0x10)

enum aac_controller_function_mode {
 ARCIO_CONTROLLER_RAID_MODE_HIDE_RAW_DEVICES = (ARCIO_CONTROLLER_FUNCTION_RAID),
 ARCIO_CONTROLLER_RAID_MODE_EXPOSE_RAW_DEVICES = 
  (ARCIO_CONTROLLER_FUNCTION_EXPOSE_RAW_DEVICES|ARCIO_CONTROLLER_FUNCTION_RAID),
 ARCIO_CONTROLLER_AUTO_VOLUME_MODE = 
  (ARCIO_CONTROLLER_FUNCTION_EXPOSE_RAW_DEVICES|ARCIO_CONTROLLER_FUNCTION_AUTO_VOLUME),
 ARCIO_CONTROLLER_HBA_MODE = 
  (ARCIO_CONTROLLER_FUNCTION_EXPOSE_RAW_DEVICES|ARCIO_CONTROLLER_FUNCTION_HBA),
 ARCIO_CONTROLLER_SIMPLE_VOLUME_240_MODE = 
  (ARCIO_CONTROLLER_FUNCTION_EXPOSE_RAW_DEVICES|ARCIO_CONTROLLER_FUNCTION_SIMPLE_VOLUME_240),
 ARCIO_CONTROLLER_SMART_MODE = 
  (ARCIO_CONTROLLER_FUNCTION_NATIVE_HBA | ARCIO_CONTROLLER_FUNCTION_EXPOSE_RAW_DEVICES | ARCIO_CONTROLLER_FUNCTION_RAID)
};


/*
 * The following command is sent to shut down each container.
 */

struct aac_close {
	__le32	command;
	__le32	cid;
};

struct aac_query_disk
{
	s32	cnum;
	s32	bus;
	s32	id;
	s32	lun;
	u32	valid;
	u32	locked;
	u32	deleted;
	s32	instance;
	s8	name[10];
	u32	unmapped;
};

struct aac_delete_disk {
	u32	disknum;
	u32	cnum;
};

struct fib_ioctl
{
	u32	fibctx;
	s32	wait;
	char	__user *fib;
};

struct revision
{
	u32 compat;
	__le32 version;
	__le32 build;
};
#if (defined(CODE_STREAM_IDENTIFIER) && !defined(CONFIG_COMMUNITY_KERNEL))

#define VERSION_MATCH_SUCCESS		1
#define VERSION_MATCH_FAILED		2
#define VERSION_MATCH_UNSUPPORTED	3
struct VersionMatch {
	u32 status;
	char driver[MAX_CODE_STREAM_IDENTIFIER_LENGTH];
	char firmware[MAX_CODE_STREAM_IDENTIFIER_LENGTH];
};
#endif


/*
 *	Ugly - non Linux like ioctl coding for back compat.
 */

#define CTL_CODE(function, method) (                 \
    (4<< 16) | ((function) << 2) | (method) \
)

/*
 *	Define the method codes for how buffers are passed for I/O and FS
 *	controls
 */

#define METHOD_BUFFERED                 0
#define METHOD_NEITHER                  3

/*
 *	Filesystem ioctls
 */

#define FSACTL_SENDFIB				CTL_CODE(2050, METHOD_BUFFERED)
#define FSACTL_SEND_RAW_SRB			CTL_CODE(2067, METHOD_BUFFERED)
#define FSACTL_DELETE_DISK			0x163
#define FSACTL_QUERY_DISK			0x173
#define FSACTL_OPEN_GET_ADAPTER_FIB		CTL_CODE(2100, METHOD_BUFFERED)
#define FSACTL_GET_NEXT_ADAPTER_FIB		CTL_CODE(2101, METHOD_BUFFERED)
#define FSACTL_CLOSE_GET_ADAPTER_FIB		CTL_CODE(2102, METHOD_BUFFERED)
#define FSACTL_MINIPORT_REV_CHECK               CTL_CODE(2107, METHOD_BUFFERED)
#define FSACTL_GET_PCI_INFO			CTL_CODE(2119, METHOD_BUFFERED)
#define FSACTL_FORCE_DELETE_DISK		CTL_CODE(2120, METHOD_NEITHER)
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#define FSACTL_REGISTER_FIB_SEND		CTL_CODE(2136, METHOD_BUFFERED)
#endif
#define FSACTL_GET_CONTAINERS			2131
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#define FSACTL_GET_VERSION_MATCHING		CTL_CODE(2137, METHOD_BUFFERED)
#endif
#define FSACTL_SEND_LARGE_FIB			CTL_CODE(2138, METHOD_BUFFERED)

#define FSACTL_GET_HBA_INFO			CTL_CODE(2150, METHOD_BUFFERED)


#define FSACTL_ERROR_INJECT			CTL_CODE(9000, METHOD_BUFFERED)
struct aac_error_inject_str {
	u8	type;
	u8	value;
};

struct aac_common
{
	/*
	 *	If this value is set to 1 then interrupt moderation will occur
	 *	in the base commuication support.
	 */
	u32 irq_mod;
	u32 peak_fibs;
	u32 zero_fibs;
	u32 fib_timeouts;
#if (defined(AAC_DEBUG_INSTRUMENT_AAC_CONFIG))
	u32 peak_size;
	u32 peak_sg;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	u32 peak_duration;
#endif
	/*
	 *	Statistical counters in debug mode
	 */
#ifdef DBG
	u32 FibsSent;
	u32 FibRecved;
	u32 NativeSent;
	u32 NativeRecved;
	u32 NoResponseSent;
	u32 NoResponseRecved;
	u32 AsyncSent;
	u32 AsyncRecved;
	u32 NormalSent;
	u32 NormalRecved;
#endif
};

extern struct aac_common aac_config;


/*
 * This is for management ioctl purpose only.
*/
struct aac_hba_info {

	u8	DriverName[50];
	u8	AdapterNumber;
	u8	SystemIoBusNumber;
	u8	DeviceNumber;
	u32	FunctionNumber;
	u32	VendorID;
	u32	DeviceID;
	u32	SubVendorID;
	u32	SubSystemID; 
	u32	MappedBaseAddressSize;
	u32	BasePhysicalAddress_HighPart;
	u32	BasePhysicalAddress_LowPart;
    
	u32	MaxCommandSize;
	u32	MaxFibSize;
	u32	MaxScatterGatherFromOs;
	u32	MaxScatterGatherToFw;
	u32	MaxOutstandingFibs;

	u32	QueueStartThreshold;
	u32	QueueDumpThreshold;
	u32	MaxIoSizeQueued;
	u32	OutstandingIO;

	u32	FirmwareBuildNumber;
	u32	BIOSBuildNumber;
	u32	DriverBuildNumber;
	u32	SerialNumber_HighPart;
	u32	SerialNumber_LowPart;
	u32	SupportedOptions;
	u32	FeatureBits;
	u32	currentnumberPorts;

	u8	NewCommInterface:1;
	u8	NewCommandsSupported:1;
	u8	DisablePassthrough:1;
	u8	ExposeNonDasd:1;
	u8	QueueAllowed:1;
	u8	BLEDCheckEnabled:1;
	u8	reserved1:1;
	u8	reserted2:1;

	u32	reserved3[10];

};


/*
 *	The following macro is used when sending and receiving FIBs. It is
 *	only used for debugging.
 */

#ifdef DBG
#define	FIB_COUNTER_INCREMENT(counter)		(counter)++
#else
#define	FIB_COUNTER_INCREMENT(counter)
#endif

/*
 *	Adapter direct commands
 *	Monitor/Kernel API
 */

#define	BREAKPOINT_REQUEST		0x00000004
#define	INIT_STRUCT_BASE_ADDRESS	0x00000005
#define READ_PERMANENT_PARAMETERS	0x0000000a
#define WRITE_PERMANENT_PARAMETERS	0x0000000b
#define HOST_CRASHING			0x0000000d
#define	SEND_SYNCHRONOUS_FIB		0x0000000c
#define COMMAND_POST_RESULTS		0x00000014
#define GET_ADAPTER_PROPERTIES		0x00000019
#define GET_DRIVER_BUFFER_PROPERTIES	0x00000023
#if (defined(AAC_DEBUG_INSTRUMENT_SLOT))
#define SEND_SLOT_NUMBER		0x00000024
#endif
#define RCV_TEMP_READINGS		0x00000025
#define GET_COMM_PREFERRED_SETTINGS	0x00000026
#define IOP_RESET			0x00001000
#define IOP_RESET_ALWAYS		0x00001001
#define RE_INIT_ADAPTER			0x000000ee

/*
 *	Adapter Status Register
 *
 *  Phase Staus mailbox is 32bits:
 *	<31:16> = Phase Status
 *	<15:0>  = Phase
 *
 *	The adapter reports is present state through the phase.  Only
 *	a single phase should be ever be set.  Each phase can have multiple
 *	phase status bits to provide more detailed information about the
 *	state of the board.  Care should be taken to ensure that any phase
 *	status bits that are set when changing the phase are also valid
 *	for the new phase or be cleared out.  Adapter software (monitor,
 *	iflash, kernel) is responsible for properly maintining the phase
 *	status mailbox when it is running.
 *
 *	MONKER_API Phases
 *
 *	Phases are bit oriented.  It is NOT valid  to have multiple bits set
 */

#define	SELF_TEST_FAILED		0x00000004
#define	MONITOR_PANIC			0x00000020
#define	KERNEL_UP_AND_RUNNING		0x00000080
#define	KERNEL_PANIC			0x00000100

/*
 * Dual firmware image support
 */
#define	FLASH_UPD_PENDING		0x00002000
#define	FLASH_UPD_SUCCESS		0x00004000
#define	FLASH_UPD_FAILED		0x00008000

/*
 * We wait this many seconds for the adapter to come ready  
 * after flash update
 */
#define FWUPD_TIMEOUT	(5 * 60)

/*
 *	Doorbell bit defines
 */

#define DoorBellSyncCmdAvailable	(1<<0)	/* Host -> Adapter */
#define DoorBellPrintfDone		(1<<5)	/* Host -> Adapter */
#define DoorBellAdapterNormCmdReady	(1<<1)	/* Adapter -> Host */
#define DoorBellAdapterNormRespReady	(1<<2)	/* Adapter -> Host */
#define DoorBellAdapterNormCmdNotFull	(1<<3)	/* Adapter -> Host */
#define DoorBellAdapterNormRespNotFull	(1<<4)	/* Adapter -> Host */
#define DoorBellPrintfReady		(1<<5)	/* Adapter -> Host */
#define DoorBellAifPending		(1<<6)	/* Adapter -> Host */
/* PMC specific outbound doorbell bits */
#define PmDoorBellResponseSent			(1<<1)	// Adapter -> Host


/*
 *	For FIB communication, we need all of the following things
 *	to send back to the user.
 */

#define			AifCmdEventNotify	1	/* Notify of event */
#define			AifEnConfigChange	3	/* Adapter configuration change */
#define			AifEnContainerChange	4	/* Container configuration change */
#define			AifEnDeviceFailure	5	/* SCSI device failed */
#define			AifEnEnclosureManagement 13	/* EM_DRIVE_* */
#define			EM_DRIVE_INSERTION	31
#define			EM_DRIVE_REMOVAL	32
#define			EM_SES_DRIVE_INSERTION	33
#define			EM_SES_DRIVE_REMOVAL	26
#define			AifEnBatteryEvent	14	/* Change in Battery State */
#define			AifEnAddContainer	15	/* A new array was created */
#define			AifEnDeleteContainer	16	/* A container was deleted */
#define			AifEnExpEvent		23	/* Firmware Event Log */
#define			AifExeFirmwarePanic	3	/* Firmware Event Panic */
#define			AifHighPriority		3	/* Highest Priority Event */
#define			AifEnAddJBOD		30	/* JBOD created */
#define			AifEnDeleteJBOD		31	/* JBOD deleted */

#define		AifCmdJobProgress	2	/* Progress report */
#define			AifJobCtrZero	101	/* Array Zero progress */
#define			AifJobStsSuccess 1	/* Job completes */
#define		AifJobStsAborted 3	/* Job aborted */
#define		AifJobStsFailed 4	/* Job failed */
#define		AifJobStsPreempted 5	/* Job preempted */
#define		AifJobStsPended 6	/* Job suspended */
#define			AifJobStsRunning 102	/* Job running */
#define		AifCmdAPIReport		3	/* Report from other user of API */
#define		AifCmdDriverNotify	4	/* Notify host driver of event */
#define			AifDenMorphComplete 200	/* A morph operation completed */
#define			AifDenVolumeExtendComplete 201 /* A volume extend completed */
#define		AifReqJobList		100	/* Gets back complete job list */
#define		AifReqJobsForCtr	101	/* Gets back jobs for specific container */
#define		AifReqJobsForScsi	102	/* Gets back jobs for specific SCSI device */
#define		AifReqJobReport		103	/* Gets back a specific job report or list of them */
#define		AifReqTerminateJob	104	/* Terminates job */
#define		AifReqSuspendJob	105	/* Suspends a job */
#define		AifReqResumeJob		106	/* Resumes a job */
#define		AifReqSendAPIReport	107	/* API generic report requests */
#define		AifReqAPIJobStart	108	/* Start a job from the API */
#define		AifReqAPIJobUpdate	109	/* Update a job report from the API */
#define		AifReqAPIJobFinish	110	/* Finish a job from the API */
#define		AifReqEvent		200	/* PMC NEW COMM: Request the event data */
#define		AifRawDeviceRemove	203	/* RAW device deleted */
#define		AifNativeDeviceAdd	204	/* native HBA device added */
#define		AifNativeDeviceRemove	205	/* native HBA device removed */

/*
 *	Adapter Initiated FIB command structures. Start with the adapter
 *	initiated FIBs that really come from the adapter, and get responded
 *	to by the host.
 */

struct aac_aifcmd {
	__le32 command;		/* Tell host what type of notify this is */
	__le32 seqnum;		/* To allow ordering of reports (if necessary) */
	u8 data[1];		/* Undefined length (from kernel viewpoint) */
};

/**
 *	Convert capacity to cylinders
 *	accounting for the fact capacity could be a 64 bit value
 *
 */
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) && !defined(HAS_SECTOR_T))
typedef unsigned long sector_t;

#endif
static inline unsigned int cap_to_cyls(sector_t capacity, unsigned divisor)
{
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) || defined(sector_div))
	sector_div(capacity, divisor);
#else
	capacity /= divisor;
#endif
#if 0
	if ((sizeof(sector_t) > sizeof(int)) &&
	  ((unsigned long long)capacity > ((1LL << (8 * sizeof(int))) - 1)))
		capacity = ((1LL << (8 * sizeof(int))) - 1LL);
#endif
	return capacity;
}

/* SCp.phase values */
#define AAC_OWNER_MIDLEVEL	0x101
#define AAC_OWNER_LOWLEVEL	0x102
#define AAC_OWNER_ERROR_HANDLER	0x103
#define AAC_OWNER_FIRMWARE	0x106

int aac_acquire_irq(struct aac_dev *dev);
void aac_free_irq(struct aac_dev *dev);
const char *aac_driverinfo(struct Scsi_Host *);
struct fib *aac_fib_alloc(struct aac_dev *dev);
int aac_fib_setup(struct aac_dev *dev);
void aac_fib_map_free(struct aac_dev *dev);
void aac_fib_free(struct fib * context);
void aac_fib_init(struct fib * context);
void aac_printf(struct aac_dev *dev, u32 val);
int aac_fib_send(u16 command, struct fib * context, unsigned long size, int priority, int wait, int reply, fib_callback callback, void *ctxt);
int aac_hba_send(u8 command, struct fib * context, fib_callback callback, void *ctxt);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
struct fib *aac_fib_alloc_tag(struct aac_dev *dev, struct scsi_cmnd *scmd);
void aac_fib_free_tag(struct fib * context);
#endif

#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
typedef int (*fib_send_t)(u16 command, struct fib * context, unsigned long size, int priority, int wait, int reply, fib_callback callback, void *ctxt);
extern fib_send_t aac_fib_send_switch;
#define aac_fib_send aac_fib_send_switch
#endif
int aac_consumer_get(struct aac_dev * dev, struct aac_queue * q, struct aac_entry **entry);
void aac_consumer_free(struct aac_dev * dev, struct aac_queue * q, u32 qnum);
int aac_fib_complete(struct fib * context);
#define fib_data(fibctx) ((void *)(fibctx)->hw_fib_va->data)
struct aac_dev *aac_init_adapter(struct aac_dev *dev);
void aac_src_access_devreg(struct aac_dev *dev, int mode);
int aac_get_config_status(struct aac_dev *dev, int commit_flag);
int aac_get_containers(struct aac_dev *dev);
#if (!defined(INITFLAGS_APRE_SUPPORTED) || defined(CONFIG_COMMUNITY_KERNEL))
int aac_scsi_cmd(struct scsi_cmnd *cmd);
#endif
int aac_dev_ioctl(struct aac_dev *dev, int cmd, void __user *arg);
#if (defined(AAC_CSMI))
int aac_csmi_ioctl(struct aac_dev *dev, int cmd, void __user *arg);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT))
void aac_csmi_register_ioctl32_conversion(void);
void aac_csmi_unregister_ioctl32_conversion(void);
#endif
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
#define class_device Scsi_Host
#define class_to_shost(class_dev) class_dev
#define shost_to_class(shost) shost
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#ifndef shost_to_class
#define shost_to_class(shost) &shost->shost_classdev
#endif
#else
#ifndef shost_to_class
#define shost_to_class(shost) &shost->shost_dev
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
ssize_t aac_show_serial_number(struct class_device *class_dev, char *buf);
#else
ssize_t aac_get_serial_number(struct device *dev, char *buf);
#endif
int aac_do_ioctl(struct aac_dev * dev, int cmd, void __user *arg);
int aac_rx_init(struct aac_dev *dev);
int aac_rkt_init(struct aac_dev *dev);
int aac_nark_init(struct aac_dev *dev);
int aac_sa_init(struct aac_dev *dev);
int aac_src_init(struct aac_dev *dev);
int aac_srcv_init(struct aac_dev *dev);
int aac_queue_get(struct aac_dev * dev, u32 * index, u32 qid, struct hw_fib * hw_fib, int wait, struct fib * fibptr, unsigned long *nonotify);
unsigned int aac_response_normal(struct aac_queue * q);
unsigned int aac_command_normal(struct aac_queue * q);
void aac_simulate_scsi_error(struct aac_dev *dev, struct hw_fib *hw_fib);
void aac_simulate_tgt_failure(struct aac_dev *dev, struct hw_fib *hw_fib);
unsigned int aac_intr_normal(struct aac_dev * dev, u32 Index, int isAif,
	int isFastResponse, struct hw_fib *aif_fib);
int aac_reset_adapter(struct aac_dev * dev, int forced);
int aac_check_health(struct aac_dev * dev);
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
int aac_command_thread(struct aac_dev * dev);
#else
int aac_command_thread(void *data);
#endif
int aac_close_fib_context(struct aac_dev * dev, struct aac_fib_context *fibctx);
int aac_fib_adapter_complete(struct fib * fibptr, unsigned short size);
struct aac_driver_ident* aac_get_driver_ident(int devtype);
#if (!defined(CONFIG_COMMUNITY_KERNEL))
/* Local Structure to set SCSI inquiry data strings */
struct scsi_inq {
	char vid[8];         /* Vendor ID */
	char pid[16];        /* Product ID */
	char prl[4];         /* Product Revision Level */
};
void setinqstr(struct aac_dev *dev, void *data, int tindex);
#endif
int aac_get_adapter_info(struct aac_dev* dev);
int aac_send_shutdown(struct aac_dev *dev);
int aac_probe_container(struct aac_dev *dev, int cid);
int _aac_rx_init(struct aac_dev *dev);
int aac_rx_select_comm(struct aac_dev *dev, int comm);
int aac_rx_deliver_producer(struct fib * fib);
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
int aac_build_sg_rx(struct fib * fib, struct scsi_cmnd * scsicmd, void * psg);
int aac_build_sg_nark(struct fib * fib, struct scsi_cmnd * scsicmd, void * psg);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
char * get_container_type(unsigned type);
#endif
extern int numacb;
extern char aac_driver_version[];
extern int startup_timeout;
extern int aif_timeout;
extern int expose_physicals;
extern int aac_remove_devnodes;
extern int aac_reset_devices;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,4)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
extern int aac_msi;
#endif
extern int aac_commit;
extern int update_interval;
extern int check_interval;
extern int aac_check_reset;
#endif
