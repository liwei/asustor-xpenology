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
 *  adbg.h
 *
 * Abstract: Contains all routines for control of the debug data.
 *
 */

#ifndef _ADBG_H_
#define _ADBG_H_

/*Used to suppress unused variable warning*/
#define UNUSED(x)       (void)(x)

#define AAC_STATUS_INFO                 0x00000001
#define AAC_DEBUG_INIT                  0x00000002
#define AAC_DEBUG_SETUP                 0x00000004
#define AAC_DEBUG_TIMING                0x00000008
#define AAC_DEBUG_AIF                   0x00000010
#define AAC_DEBUG_IOCTL                 0x00000020
#define AAC_DEBUG_IOCTL_SENDFIB         0x00000040
#define AAC_DEBUG_AAC_CONFIG            0x00000080
#define AAC_DEBUG_RESET                 0x00000100
#define AAC_DEBUG_FIB                   0x00000200
#define AAC_DEBUG_CONTEXT               0x00000400
#define AAC_DEBUG_2TB                   0x00000800
#define AAC_DEBUG_SENDFIB               0x00001000
#define AAC_DEBUG_IO                    0x00002000
#define AAC_DEBUG_PENDING               0x00004000
#define AAC_DEBUG_SG                    0x00008000
#define AAC_DEBUG_SG_PROBE              0x00010000
#define AAC_DEBUG_VM_NAMESERVE          0x00020000
#define AAC_DEBUG_SERIAL                0x00040000
#define AAC_DEBUG_SYNCHRONIZE           0x00080000
#define AAC_DEBUG_SHUTDOWN              0x00100000
#define AAC_DEBUG_MSIX                  0x00200000

#define LOG_SETUP  (AAC_DEBUG_INIT| AAC_DEBUG_IOCTL)

//#define CONFIG_SCSI_AACRAID_LOGGING
#define CONFIG_SCSI_AACRAID_LOGGING_PRINTK

#ifdef CONFIG_SCSI_AACRAID_LOGGING
#define AAC_CHECK_LOGGING(DEV, BITS, LVL, TEST, ...)   \
({                                          \
    if(DEV->logging_level & BITS)           \
    {                                       \
      printk(LVL "%s%u: " TEST,DEV->name,DEV->id, ##__VA_ARGS__);      \
     fwprintf((DEV, HBA_FLAGS_DBG_FW_PRINT_B, ##__VA_ARGS__));\
    }\
})
#elif  defined(CONFIG_SCSI_AACRAID_LOGGING_PRINTK)
#define AAC_CHECK_LOGGING(DEV, BITS, LVL, TEST, ...) \
({                                              \
        printk(LVL "%s%u: " TEST,DEV->name,DEV->id,##__VA_ARGS__);        \
})
#else
#define AAC_CHECK_LOGGING(DEV, BITS, CMD, TEST,...)
#endif

#define adbg_dev() \
    printk(KERN_ERR,"Line-%d,Function-%s,File-%s",__LINE__,__FUNCTION__,__FILE__);

#define adbg(DEV, BITS, LVL, TEST,...) \
    AAC_CHECK_LOGGING(DEV, BITS, LVL, TEST, ##__VA_ARGS__)

#if defined(AAC_DETAILED_STATUS_INFO)
#define adbg_info(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_STATUS_INFO, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_info(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_INIT)
#define adbg_init(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_INIT,  LVL, TEST,##__VA_ARGS__)
#else
#define     adbg_init(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SETUP)
#define adbg_setup(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SETUP, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_setup(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_TIMING)
#define adbg_time(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_TIMING,  LVL, TEST,##__VA_ARGS__)
#else
#define adbg_time(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_AIF)
#define adbg_aif(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_AIF, LVL ,TEST,##__VA_ARGS__)
#else
#define adbg_aif(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IOCTL)
#define adbg_ioctl(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_IOCTL, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_ioctl(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB)
#define adbg_ioctl_sendfib(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_IOCTL_SENDFIB, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_sendfib(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_AAC_CONFIG)
#define adbg_conf(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_AAC_CONFIG, LVL, TEST,  ##__VA_ARGS__)
#else
#define adbg_conf(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_RESET)
#define adbg_reset(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_RESET, LVL, TEST,  ##__VA_ARGS__)
#else
#define adbg_reset(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_FIB)
#define adbg_fib(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_FIB, LVL, TEST,  ##__VA_ARGS__)
#else
#define adbg_fib(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_CONTEXT)
#define adbg_context(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_CONTEXT, LVL, TEST,  ##__VA_ARGS__)
#else
#define adbg_context(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_2TB)
#define adbg_2tb(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_2TB,  LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_2tb(DEV, LVL, TEST, ...)
#endif


#if defined(AAC_DEBUG_INSTRUMENT_SENDFIB)
#define adbg_sendfib(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_SENDFIB, LVL, TEST,  ##__VA_ARGS__)
#else
#define adbg_sendfib(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IO)
#define adbg_io(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_IO, LVL, TEST,  ##__VA_ARGS__)
#else
#define adbg_io(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_PENDING)
#define adbg_pending(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_PENDING, LVL, TEST,  ##__VA_ARGS__)
#else
#define adbg_pending(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SG)
#define adbg_sg(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_SG,  LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_sg(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SG_PROBE)
#define adbg_sg_probe(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_SG_PROBE,  LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_sg_probe(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE)
#define adbg_vm(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_VM_NAMESERVE, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_vm(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SERIAL)
#define adbg_serial(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV,AAC_DEBUG_SERIAL, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_serial(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE)
#define adbg_sync(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SYNCHRONIZE, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_sync(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN)
#define adbg_shut(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SHUTDOWN, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_shut(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_MSIX)
#define adbg_msix(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_MSIX, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_msix(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_FIB) || defined(AAC_DEBUG_INSTRUMENT_MSIX)
#define adbg_fib_or_msix(DEV, LVL,TEST, ...) \
    AAC_CHECK_LOGGING(DEV, (AAC_DEBUG_FIB|AAC_DEBUG_MSIX), LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_fib_or_msix(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF)
#define adbg_ioctl_and_aif(DEV, LVL, TEST, ...)\
    AAC_CHECK_LOGGING(DEV, (AAC_DEBUG_IOCTL|AAC_DEBUG_AIF), LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_ioctl_and_aif(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_INIT) && defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE)
#define adbg_init_or_vm(DEV, LVL, TEST, ...)\
    AAC_CHECK_LOGGING(DEV, (AAC_DEBUG_INIT|AAC_DEBUG_VM), LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_init_or_vm(DEV, LVL, TEST, ...)
#endif


/********** Below contains struct definitions  ***********/
struct scsi_cmnd;
struct aac_dev;

/********** Use below section for adding debug routines & macros ***********/
#if defined(AAC_DEBUG_INSTRUMENT_RESET)

void dump_pending_fibs(struct aac_dev *aac, struct scsi_cmnd *cmd);
int dump_command_queue(struct scsi_cmnd* cmd);

#define DBG_OVERFLOW_CHK(dev, vno) \
{\
	if(atomic_read(&dev->rrq_outstanding[vno]) > dev->vector_cap)\
		adbg_reset(dev, KERN_ERR, \
		"%s:%d, Host RRQ overloaded vec: %u, Outstanding IOs: %u\n",\
		__FUNCTION__,__LINE__, \
		vno,\
		atomic_read(&dev->rrq_outstanding[vno])); \
}
#define DBG_SET_STATE(FIB, S)	atomic_set(&FIB->state, S);
#define adbg_dump_pending_fibs(AAC, CMD) dump_pending_fibs(AAC, CMD)
#define adbg_dump_command_queue(CMD) dump_command_queue(CMD)
#else
#define DBG_SET_STATE(FIB, S)
#define DBG_OVERFLOW_CHK(dev, vno)
#define adbg_dump_pending_fibs(AAC, CMD)
#define adbg_dump_command_queue(CMD) (0)
#endif


#if defined(AAC_DEBUG_INSTRUMENT_TIMING)

void queuecmd_debug_timing(struct scsi_cmnd *cmd);

#define adbg_queuecmd_debug_timing(CMD) queuecmd_debug_timing(CMD)
#else
#define adbg_queuecmd_debug_timing(CMD)
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_AAC_CONFIG))

void debug_aac_config(struct scsi_cmnd* scsicmd, __le32 count,
                      unsigned long byte_count);

#define adbg_debug_aac_config(CMD,COUNT,BYTE_COUNT) \
            debug_aac_config(CMD,COUNT,BYTE_COUNT)
#else
#define adbg_debug_aac_config(CMD,COUNT,BYTE_COUNT)
#endif

#endif


