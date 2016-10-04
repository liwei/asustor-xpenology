
/*
 * Adaptec AAC series RAID controller driver
 * (c) Copyright 2001 Red Hat Inc.<alan@redhat.com>
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
 *
 * Copyright (c) 2000-2007 Adaptec, Inc. (aacraid@adaptec.com)
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
 *  adbg.c
 *
 * Abstract: Contains debug routines
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/blkdev.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#include "adbg.h"

#ifdef AAC_DEBUG_INSTRUMENT_RESET
void dump_pending_fibs(struct aac_dev *aac, struct scsi_cmnd *cmd)
{
	int i;
	u32 handle=0, isFastResponse=0;
	char	strstate[9][20] = {	"FREE",
					"FREED_AIF",
					"FREED_IO",
					"ALLOCATED_AIF",
					"ALLOCATED_IO",
					"INITIALIZED",
					"IO_SUBMITTED",
					"PRE_INT",
					"POST_INT"
				};

	adbg_reset(aac, KERN_ERR, " \n********* Pending FIBs ********* \n");
	for (i=0; i<1024; i++)
	{
		if ((atomic_read(&aac->fibs[i].state) > DBG_STATE_FREED_IO)
			&& (atomic_read(&aac->fibs[i].state) != DBG_STATE_POST_INT))
		{
			u32 state = atomic_read(&aac->fibs[i].state);
			if ((cmd != NULL) && (aac->fibs[i].callback_data == cmd))
				adbg_reset(aac, KERN_ERR, "* (s)pending fib: %04u - %s",
					i, strstate[state]);
			else if (aac->fibs[i].callback_data)
				adbg_reset(aac, KERN_ERR, "  (s)pending fib: %04u - %s",
					i, strstate[state]);
			else
				adbg_reset(aac, KERN_ERR, "  ( )pending fib: %04u - %s",
					i, strstate[state]);

		}
	}

	adbg_reset(aac, KERN_ERR, " \n********* Host RRQ Pending handles ********* \n");
	for (i=0; i<1024; i++)
	{

		if (aac->host_rrq[i])
		handle = le32_to_cpu((aac->host_rrq[i])
				& 0x7fffffff);
		/* check fast response bits (30, 1) */
		if (handle & 0x40000000)
			isFastResponse = 1;
		handle &= 0x0000ffff;

		if (handle)
		{
			handle >>= 2;
			adbg_reset(aac, KERN_ERR, " HOST_RRQ[%04u] : %04u, Vec: %02u, FastResponse: %u, FIB Status: %s\n", i,
					handle,
					i/aac->vector_cap,
					isFastResponse,
					strstate[atomic_read(&aac->fibs[handle].state)]);
		}
	}

	adbg_reset(aac, KERN_ERR, " \n\n********* Host RRQ Indexs ********* \n");
	for (i=0; i<aac->max_msix; i++)
	{
		adbg_reset(aac, KERN_ERR, "  host_rrq index vec[%02u]: %04u\n",i, aac->host_rrq_idx[i]);
	}

	adbg_reset(aac, KERN_ERR, "\n\n");

	return;
}
#endif


#if 0
void test(){
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
}
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_RESET) || (0 && defined(BOOTCD)))
int dump_command_queue(struct scsi_cmnd* cmd)
{
	struct scsi_device * dev = cmd->device;
	struct Scsi_Host * host = dev->host;
	struct aac_dev * aac = (struct aac_dev *)host->hostdata;
	struct scsi_cmnd * command;
	int active=0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		unsigned long flags;
		spin_lock_irqsave(&dev->list_lock, flags);
		list_for_each_entry(command, &dev->cmd_list, list)
#else
		for(command = dev->device_queue;command;command = command->next)
#endif
		{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12))
			if ((command->state == SCSI_STATE_FINISHED)
		 		|| (command->state == 0))
				continue;
#endif
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
		 "%4d %c%c %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		  	 active,
		  	 (command->SCp.phase == AAC_OWNER_FIRMWARE) ? 'A' : 'C',
		  	 (cmd == command) ? '*' : ' ',
		  	 command->cmnd[0], command->cmnd[1], command->cmnd[2],
		  	 command->cmnd[3], command->cmnd[4], command->cmnd[5],
		  	 command->cmnd[6], command->cmnd[7], command->cmnd[8],
		  	 command->cmnd[9]));
			printk(KERN_ERR
		 "%4d %c%c %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		  	 active,
		  	 (command->SCp.phase == AAC_OWNER_FIRMWARE) ? 'A' : 'C',
		  	 (cmd == command) ? '*' : ' ',
		  	 command->cmnd[0], command->cmnd[1], command->cmnd[2],
		  	 command->cmnd[3], command->cmnd[4], command->cmnd[5],
		  	 command->cmnd[6], command->cmnd[7], command->cmnd[8],
		  	 command->cmnd[9]);
			++active;
		}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		spin_unlock_irqrestore(&dev->list_lock, flags);
#endif
		return active;
}
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
void queuecmd_debug_timing(struct scsi_cmnd *cmd)
{
	u64 lba;
	u32 count = 0;
	struct timeval now;
	struct Scsi_Host *host = cmd->device->host;
	struct aac_dev *dev = (struct aac_dev *)host->hostdata;
	do_gettimeofday(&now);
	if ((cmd->cmnd[0] == WRITE_6)	/* 6 byte command */
	 || (cmd->cmnd[0] == READ_6)) {
		lba = ((cmd->cmnd[1] & 0x1F) << 16)
		    | (cmd->cmnd[2] << 8) | cmd->cmnd[3];
		count = cmd->cmnd[4];
		if (count == 0)
			count = 256;

#if (defined(WRITE_16))
	} else if ((cmd->cmnd[0] == WRITE_16) /* 16 byte command */
	 || (cmd->cmnd[0] == READ_16)) {
		lba = ((u64)cmd->cmnd[2] << 56)
		    | ((u64)cmd->cmnd[3] << 48)
		    | ((u64)cmd->cmnd[4] << 40)
		    | ((u64)cmd->cmnd[9] << 32)
		    | (cmd->cmnd[6] << 24)
		    | (cmd->cmnd[7] << 16)
		    | (cmd->cmnd[8] << 8) | cmd->cmnd[9];
		count = (cmd->cmnd[10] << 24)
		      | (cmd->cmnd[11] << 16)
		      | (cmd->cmnd[12] << 8) | cmd->cmnd[13];
#endif

	} else if ((cmd->cmnd[0] == WRITE_12) /* 12 byte command */
	 || (cmd->cmnd[0] == READ_12)) {
		lba = (cmd->cmnd[2] << 24)
		    | (cmd->cmnd[3] << 16)
		    | (cmd->cmnd[4] << 8) | cmd->cmnd[5];
		count = (cmd->cmnd[6] << 24)
		      | (cmd->cmnd[7] << 16)
		      | (cmd->cmnd[8] << 8) | cmd->cmnd[9];
	} else if ((cmd->cmnd[0] == WRITE_10) /* 10 byte command */
	 || (cmd->cmnd[0] == READ_10)) {
		lba = (cmd->cmnd[2] << 24)
		    | (cmd->cmnd[3] << 16)
		    | (cmd->cmnd[4] << 8) | cmd->cmnd[5];
		count = (cmd->cmnd[7] << 8) | cmd->cmnd[8];
	} else
		lba = (u64)(uintptr_t)cmd;
	printk(((count)
	  ? KERN_DEBUG "%lu.%06lu q%lu %llu[%u]\n"
	  : KERN_DEBUG "%lu.%06lu q%lu 0x%llx\n"),
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)(atomic_read(&dev->queues->queue[
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	    (dev->comm_interface == AAC_COMM_APRE)
	      ? ApreCmdQueue
	      : AdapNormCmdQueue
#else
	    AdapNormCmdQueue
#endif
	  ].numpending)), (unsigned long long)lba, count);
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  ((count)
	    ? "%lu.%06lu q%lu %llu[%u]"
	    : "%lu.%06lu q%lu 0x%llx"),
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)(atomic_read(&dev->queues->queue[
	    (dev->comm_interface == AAC_COMM_APRE)
	      ? ApreCmdQueue
	      : AdapNormCmdQueue
	  ].numpending)), (unsigned long long)lba, count));
#else
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  ((count)
	    ? "%lu.%06lu q%lu %llu[%u]"
	    : "%lu.%06lu q%lu 0x%llx"),
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)(atomic_read(&dev->queues->queue[
	    AdapNormCmdQueue
	  ].numpending)), (unsigned long long)lba, count));
#endif
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  ((count)
	    ? "%lu.%06lu q%lu %llu[%u]"
	    : "%lu.%06lu q%lu 0x%llx"),
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)(atomic_read(&dev->queues->queue[
	    (dev->comm_interface == AAC_COMM_APRE)
	      ? ApreCmdQueue
	      : AdapNormCmdQueue
	  ].numpending)), (unsigned long long)lba, count));
#else
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  ((count)
	    ? "%lu.%06lu q%lu %llu[%u]"
	    : "%lu.%06lu q%lu 0x%llx"),
	  now.tv_sec % 100, now.tv_usec,
	  (unsigned long)(atomic_read(&dev->queues->queue[
	    AdapNormCmdQueue
	  ].numpending)), (unsigned long long)lba, count));
#endif
}
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_AAC_CONFIG))
void debug_aac_config(struct scsi_cmnd* scsicmd, __le32 count, unsigned long byte_count)
{
	struct aac_dev *dev = (struct aac_dev *)scsicmd->device->host->hostdata;

        if (le32_to_cpu(count) > aac_config.peak_sg) {
                aac_config.peak_sg  = le32_to_cpu(count);
                adbg_conf(dev,KERN_INFO, "peak_sg=%u\n", aac_config.peak_sg);
        }

        if (byte_count > aac_config.peak_size) {
                aac_config.peak_size = byte_count;
                adbg_conf(dev, KERN_INFO, "peak_size=%u\n", aac_config.peak_size);
        }

}
#endif


