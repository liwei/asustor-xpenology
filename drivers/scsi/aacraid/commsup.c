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
 *  commsup.c
 *
 * Abstract: Contain all routines that are required for FSA host/adapter
 *    communication.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/version.h>	/* Needed for the following */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/blkdev.h>
#include <linux/delay.h>
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) || defined(HAS_KTHREAD))
#include <linux/kthread.h>
#endif
#include <linux/interrupt.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
#include <linux/semaphore.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include "scsi.h"
#include "hosts.h"
#if (!defined(SCSI_HAS_HOST_LOCK))
#include <linux/blk.h>	/* for io_request_lock definition */
#endif
#else
#if (((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__)
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>
#endif
#include <scsi/scsi.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,1)) && !defined(DID_OK))
#define DID_OK 0x00
#endif
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#include <scsi/scsi_eh.h>
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) && defined(MODULE))
#include <scsi/scsi_driver.h>
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#endif

#include "aacraid.h"
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#include "fwdebug.h"
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32))
struct tm {
	/*
	 * the number of seconds after the minute, normally in the range
	 * 0 to 59, but can be up to 60 to allow for leap seconds
	 */
	int tm_sec;
	/* the number of minutes after the hour, in the range 0 to 59*/
	int tm_min;
	/* the number of hours past midnight, in the range 0 to 23 */
	int tm_hour;
	/* the day of the month, in the range 1 to 31 */
	int tm_mday;
	/* the number of months since January, in the range 0 to 11 */
	int tm_mon;
	/* the number of years since 1900 */
	long tm_year;
	/* the number of days since Sunday, in the range 0 to 6 */
	int tm_wday;
	/* the number of days since January 1, in the range 0 to 365 */
	int tm_yday;
};
/*
 * Nonzero if YEAR is a leap year (every 4 years,
 * except every 100th isn't, and every 400th is).
 */
static int __isleap(long year)
{
	return (year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0);
}

/* do a mathdiv for long type */
static long math_div(long a, long b)
{
	return a / b - (a % b < 0);
}

/* How many leap years between y1 and y2, y1 must less or equal to y2 */
static long leaps_between(long y1, long y2)
{
	long leaps1 = math_div(y1 - 1, 4) - math_div(y1 - 1, 100)
		+ math_div(y1 - 1, 400);
	long leaps2 = math_div(y2 - 1, 4) - math_div(y2 - 1, 100)
		+ math_div(y2 - 1, 400);
	return leaps2 - leaps1;
}

/* How many days come before each month (0-12). */
static const unsigned short __mon_yday[2][13] = {
	/* Normal years. */
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
	/* Leap years. */
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

#define SECS_PER_HOUR	(60 * 60)
#define SECS_PER_DAY	(SECS_PER_HOUR * 24)

/**
 * time_to_tm - converts the calendar time to local broken-down time
 *
 * @totalsecs	the number of seconds elapsed since 00:00:00 on January 1, 1970,
 *		Coordinated Universal Time (UTC).
 * @offset	offset seconds adding to totalsecs.
 * @result	pointer to struct tm variable to receive broken-down time
 */
void time_to_tm(time_t totalsecs, int offset, struct tm *result)
{
	long days, rem, y;
	const unsigned short *ip;

	days = totalsecs / SECS_PER_DAY;
	rem = totalsecs % SECS_PER_DAY;
	rem += offset;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}

	result->tm_hour = rem / SECS_PER_HOUR;
	rem %= SECS_PER_HOUR;
	result->tm_min = rem / 60;
	result->tm_sec = rem % 60;

	/* January 1, 1970 was a Thursday. */
	result->tm_wday = (4 + days) % 7;
	if (result->tm_wday < 0)
		result->tm_wday += 7;

	y = 1970;

	while (days < 0 || days >= (__isleap(y) ? 366 : 365)) {
		/* Guess a corrected year, assuming 365 days per year. */
		long yg = y + math_div(days, 365);

		/* Adjust DAYS and Y to match the guessed year. */
		days -= (yg - y) * 365 + leaps_between(y, yg);
		y = yg;
	}

	result->tm_year = y - 1900;

	result->tm_yday = days;

	ip = __mon_yday[__isleap(y)];
	for (y = 11; days < ip[y]; y--)
		continue;
	days -= ip[y];

	result->tm_mon = y;
	result->tm_mday = days + 1;
}
#endif

/**
 *	fib_map_alloc		-	allocate the fib objects
 *	@dev: Adapter to allocate for
 *
 *	Allocate and map the shared PCI space for the FIB blocks used to
 *	talk to the Adaptec firmware.
 */

static int fib_map_alloc(struct aac_dev *dev)
{
	if (AAC_MAX_NATIVE_SIZE > dev->max_fib_size)
		dev->max_cmd_size = AAC_MAX_NATIVE_SIZE;
	else
		dev->max_cmd_size = dev->max_fib_size;

	dprintk((KERN_INFO
	  "allocate hardware fibs pci_alloc_consistent(%p, %d * (%d + %d), %p)\n",
	  dev->pdev, dev->max_cmd_size, dev->scsi_host_ptr->can_queue,
	  AAC_NUM_MGT_FIB, &dev->hw_fib_pa));
#if 0 && ((defined(CONFIG_X86) || defined(CONFIG_X86_64)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)))
	/* Bug in pci_alloc_consistent dealing with respecting dma map */
	dev->hw_fib_va = kmalloc(
	  dev->max_cmd_size * (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB),
	  GFP_ATOMIC|GFP_KERNEL);
	if (dev->hw_fib_va) {
		dev->hw_fib_pa = pci_map_single(dev->pdev, dev->hw_fib_va,
		  dev->max_fib_size * (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB),
		  DMA_BIDIRECTIONAL);
		if (dev->hw_fib_pa > (0x80000000UL
		  - (dev->max_fib_size
		   * (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB)))) {
			kfree(dev->hw_fib_va);
			dev->hw_fib_va = NULL;
		}
	}
	if (dev->hw_fib_va == NULL)
#endif
	if((dev->hw_fib_va = pci_alloc_consistent(dev->pdev, 
		(dev->max_cmd_size + sizeof(struct aac_fib_xporthdr))
		* (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB) + 31,
		&dev->hw_fib_pa)) == NULL)
		return -ENOMEM;
	return 0;
}

/**
 *	aac_fib_map_free		-	free the fib objects
 *	@dev: Adapter to free
 *
 *	Free the PCI mappings and the memory allocated for FIB blocks
 *	on this adapter.
 */

void aac_fib_map_free(struct aac_dev *dev)
{
	pci_free_consistent(dev->pdev,
		(dev->max_cmd_size *
		dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB),
	  	dev->hw_fib_va, dev->hw_fib_pa);
	dev->hw_fib_va = NULL;
	dev->hw_fib_pa = 0;
}

/**
 *	aac_fib_setup	-	setup the fibs
 *	@dev: Adapter to set up
 *
 *	Allocate the PCI space for the fibs, map it and then intialise the
 *	fib area, the unmapped fib data and also the free list
 */

int aac_fib_setup(struct aac_dev * dev)
{
	struct fib *fibptr;
	struct hw_fib *hw_fib;
	dma_addr_t hw_fib_pa;
	int i;
	u32 max_cmds, vector=1;

	while (((i = fib_map_alloc(dev)) == -ENOMEM)
	 && (dev->scsi_host_ptr->can_queue > (64 - AAC_NUM_MGT_FIB))) {
		max_cmds = (dev->scsi_host_ptr->can_queue+AAC_NUM_MGT_FIB) >> 1;
		dev->scsi_host_ptr->can_queue = max_cmds - AAC_NUM_MGT_FIB;
		if (dev->comm_interface != AAC_COMM_MESSAGE_TYPE3)
			dev->init->r7.MaxIoCommands = cpu_to_le32(max_cmds);
	}
	if (i<0)
		return -ENOMEM;

	/* 32 byte alignment for PMC */
	hw_fib_pa = (dev->hw_fib_pa + 31) & ~31;
	dev->hw_fib_va = (struct hw_fib *)((unsigned char *)dev->hw_fib_va +
		(hw_fib_pa - dev->hw_fib_pa));
	dev->hw_fib_pa = hw_fib_pa;
	memset(dev->hw_fib_va, 0, 
		(dev->max_cmd_size + sizeof(struct aac_fib_xporthdr)) * 
		(dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB));

	/* add Xport header */
	dev->hw_fib_va = (struct hw_fib *)((unsigned char *)dev->hw_fib_va +
		sizeof(struct aac_fib_xporthdr));
	dev->hw_fib_pa += sizeof(struct aac_fib_xporthdr);

	hw_fib = dev->hw_fib_va;
	hw_fib_pa = dev->hw_fib_pa;
	/*
	 *	Initialise the fibs
	 */
	for (i = 0, fibptr = &dev->fibs[i];
		i < (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB);
		i++, fibptr++)
	{
		fibptr->flags = 0;
		fibptr->dev = dev;
		fibptr->hw_fib_va = hw_fib;
		fibptr->data = (void *) fibptr->hw_fib_va->data;
		fibptr->next = fibptr+1;	/* Forward chain the fibs */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
		init_MUTEX_LOCKED(&fibptr->event_wait);
#else
		sema_init(&fibptr->event_wait, 0);
#endif
		spin_lock_init(&fibptr->event_lock);
		hw_fib->header.XferState = cpu_to_le32(0xffffffff);
		hw_fib->header.SenderSize = cpu_to_le16(dev->max_fib_size);
		fibptr->hw_fib_pa = hw_fib_pa;
		fibptr->hw_sgl_pa = hw_fib_pa +
			offsetof(struct aac_hba_cmd_req, sge[2]);
		/* one element is for the ptr to the separate sg list,
		   second element for 32 byte alignment */
		fibptr->hw_error_pa = hw_fib_pa +
			offsetof(struct aac_native_hba, resp.resp_bytes[0]);	
		hw_fib = (struct hw_fib *)((unsigned char *)hw_fib + 
			dev->max_cmd_size + sizeof(struct aac_fib_xporthdr));
		hw_fib_pa = hw_fib_pa + 
			dev->max_cmd_size + sizeof(struct aac_fib_xporthdr);
		DBG_SET_STATE(fibptr, DBG_STATE_FREE);

		if (1 == dev->max_msix ||
			(i > ((dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB -1 ) - dev->vector_cap)))
		{
			fibptr->vector_no = 0;
		}
		else {
			fibptr->vector_no = vector;
			vector++;
			if (vector == dev->max_msix)
				vector  = 1;
		}
	}
	/*
	 *	Add the fib chain to the free list
	 */
	dev->fibs[dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB - 1].next = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))

    /* Reserving the last 8 as management fibs */
	dev->free_fib = &dev->fibs[dev->scsi_host_ptr->can_queue];
#else
	/*
	 *	Enable this to debug out of queue space
	 */
	dev->free_fib = &dev->fibs[0];
#endif
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
/**
 *	aac_fib_alloc_tag	-	allocate a fib
 *	@dev: Adapter to allocate the fib for
 *
 *	Allocate a fib from the adapter fib pool. If the pool is empty we
 *	return NULL.
 */
struct fib *aac_fib_alloc_tag(struct aac_dev *dev, struct scsi_cmnd *scmd)
{
	struct fib * fibptr;

    fibptr = &dev->fibs[scmd->request->tag];    

	/*
	 *	Set the proper node type code and node byte size
	 */
	fibptr->type = FSAFS_NTC_FIB_CONTEXT;
	fibptr->size = sizeof(struct fib);
	/*
	 *	Null out fields that depend on being zero at the start of
	 *	each I/O
	 */
	fibptr->hw_fib_va->header.XferState = 0;
	fibptr->flags = 0;
	fibptr->callback = NULL;
	fibptr->callback_data = NULL;

	DBG_SET_STATE(fibptr, DBG_STATE_ALLOCATED_IO);

    return fibptr;
}

/**
 *	aac_fib_free_tag	-	free a fib
 *	@fibptr: fib to free up
 *
 *	Frees up a fib and places it on the appropriate queue
 */

void aac_fib_free_tag(struct fib *fibptr)
{

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	struct timeval now;
	unsigned int flags;
	do_gettimeofday(&now);
	fibptr->DriverTimeDoneS = now.tv_sec;
	fibptr->DriverTimeDoneuS = now.tv_usec;
	flags = (fibptr->DriverTimeDoneS - fibptr->DriverTimeStartS) * 1000000L
	      + fibptr->DriverTimeDoneuS - fibptr->DriverTimeStartuS;
	if (flags > aac_config.peak_duration) {
		aac_config.peak_duration = flags;
		printk(KERN_INFO "peak_duration %lduseconds\n", flags);
		fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "peak_duration %lduseconds", flags));
	}
#endif
	DBG_SET_STATE(fibptr, DBG_STATE_FREED_IO);
}
#endif

/**
 *	aac_fib_alloc	-	allocate a fib
 *	@dev: Adapter to allocate the fib for
 *
 *	Allocate a fib from the adapter fib pool. If the pool is empty we
 *	return NULL.
 */

struct fib *aac_fib_alloc(struct aac_dev *dev)
{
	struct fib * fibptr;
	unsigned long flags;

	spin_lock_irqsave(&dev->fib_lock, flags);
	fibptr = dev->free_fib;
	if(!fibptr){
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		return fibptr;
	}
	dev->free_fib = fibptr->next;
	spin_unlock_irqrestore(&dev->fib_lock, flags);

	/*
	 *	Set the proper node type code and node byte size
	 */
	fibptr->type = FSAFS_NTC_FIB_CONTEXT;
	fibptr->size = sizeof(struct fib);
	/*
	 *	Null out fields that depend on being zero at the start of
	 *	each I/O
	 */
	fibptr->hw_fib_va->header.XferState = 0;
	fibptr->flags = 0;
	fibptr->callback = NULL;
	fibptr->callback_data = NULL;
#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	{
		struct timeval now;
		do_gettimeofday(&now);
		fibptr->DriverTimeStartS = now.tv_sec;
		fibptr->DriverTimeStartuS = now.tv_usec;
	}
	fibptr->DriverTimeDoneS = 0;
	fibptr->DriverTimeDoneuS = 0;
#endif
	DBG_SET_STATE(fibptr, DBG_STATE_ALLOCATED_AIF);

	return fibptr;
}

/**
 *	aac_fib_free	-	free a fib
 *	@fibptr: fib to free up
 *
 *	Frees up a fib and places it on the appropriate queue
 */

void aac_fib_free(struct fib *fibptr)
{
	unsigned long flags;

#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
    struct aac_dev *dev=fibptr->dev;

    if ( (fibptr - dev->fibs) < dev->scsi_host_ptr->can_queue)
    {
        struct scsi_cmnd *cmd = fibptr->callback_data;

        dump_stack();

        if (cmd)
            printk(" aac free fibs (unexpected) : %lu, 0x%X\n", (fibptr - fibptr->dev->fibs), cmd->cmnd[0]);
        else
            printk(" aac free fibs (unexpected) : %lu\n", (fibptr - fibptr->dev->fibs));
        return;
    }
#endif

	/* fib with status wait timeout? */
	if (fibptr->done == 2) {
		return;
	}

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	struct timeval now;
	do_gettimeofday(&now);
	fibptr->DriverTimeDoneS = now.tv_sec;
	fibptr->DriverTimeDoneuS = now.tv_usec;
	flags = (fibptr->DriverTimeDoneS - fibptr->DriverTimeStartS) * 1000000L
	      + fibptr->DriverTimeDoneuS - fibptr->DriverTimeStartuS;
	if (flags > aac_config.peak_duration) {
		aac_config.peak_duration = flags;
		printk(KERN_INFO "peak_duration %lduseconds\n", flags);
		fwprintf((fibptr->dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "peak_duration %lduseconds", flags));
	}
#endif
	spin_lock_irqsave(&fibptr->dev->fib_lock, flags);
	if (unlikely(fibptr->flags & FIB_CONTEXT_FLAG_TIMED_OUT))
		aac_config.fib_timeouts++;
	if (!(fibptr->flags & FIB_CONTEXT_FLAG_NATIVE_HBA) &&
		fibptr->hw_fib_va->header.XferState != 0) {
		printk(KERN_WARNING "aac_fib_free, XferState != 0, fibptr = 0x%p, XferState = 0x%x\n",
			 (void*)fibptr,
			 le32_to_cpu(fibptr->hw_fib_va->header.XferState));
	}
	fibptr->next = fibptr->dev->free_fib;
	fibptr->dev->free_fib = fibptr;
	spin_unlock_irqrestore(&fibptr->dev->fib_lock, flags);
	DBG_SET_STATE(fibptr, DBG_STATE_FREED_AIF);
}

/**
 *	aac_fib_init	-	initialise a fib
 *	@fibptr: The fib to initialize
 *
 *	Set up the generic fib fields ready for use
 */

void aac_fib_init(struct fib *fibptr)
{
	struct hw_fib *hw_fib = fibptr->hw_fib_va;

	memset(&hw_fib->header, 0, sizeof(struct aac_fibhdr));
	hw_fib->header.StructType = FIB_MAGIC;
	hw_fib->header.Size = cpu_to_le16(fibptr->dev->max_fib_size);
	hw_fib->header.XferState = cpu_to_le32(HostOwned | FibInitialized | FibEmpty | FastResponseCapable);
	hw_fib->header.u.ReceiverFibAddress = cpu_to_le32(fibptr->hw_fib_pa);
	hw_fib->header.SenderSize = cpu_to_le16(fibptr->dev->max_fib_size);
}

/**
 *	fib_deallocate		-	deallocate a fib
 *	@fibptr: fib to deallocate
 *
 *	Will deallocate and return to the free pool the FIB pointed to by the
 *	caller.
 */

static void fib_dealloc(struct fib * fibptr)
{
	struct hw_fib *hw_fib = fibptr->hw_fib_va;
	hw_fib->header.XferState = 0;
}

/*
 *	Commuication primitives define and support the queuing method we use to
 *	support host to adapter commuication. All queue accesses happen through
 *	these routines and are the only routines which have a knowledge of the
 *	 how these queues are implemented.
 */

/**
 *	aac_get_entry		-	get a queue entry
 *	@dev: Adapter
 *	@qid: Queue Number
 *	@entry: Entry return
 *	@index: Index return
 *	@nonotify: notification control
 *
 *	With a priority the routine returns a queue entry if the queue has free entries. If the queue
 *	is full(no free entries) than no entry is returned and the function returns 0 otherwise 1 is
 *	returned.
 */

static int aac_get_entry (struct aac_dev * dev, u32 qid, struct aac_entry **entry, u32 * index, unsigned long *nonotify)
{
	struct aac_queue * q;
	unsigned long idx;

	/*
	 *	All of the queues wrap when they reach the end, so we check
	 *	to see if they have reached the end and if they have we just
	 *	set the index back to zero. This is a wrap. You could or off
	 *	the high bits in all updates but this is a bit faster I think.
	 */

	q = &dev->queues->queue[qid];

	idx = *index = le32_to_cpu(*(q->headers.producer));
	/* Interrupt Moderation, only interrupt for first two entries */
	if (idx != le32_to_cpu(*(q->headers.consumer))) {
		if (--idx == 0) {
			if (qid == AdapNormCmdQueue)
				idx = ADAP_NORM_CMD_ENTRIES;
			else
				idx = ADAP_NORM_RESP_ENTRIES;
		}
		if (idx != le32_to_cpu(*(q->headers.consumer)))
			*nonotify = 1;
	}

	if (qid == AdapNormCmdQueue) {
		if (*index >= ADAP_NORM_CMD_ENTRIES)
			*index = 0; /* Wrap to front of the Producer Queue. */
	} else {
		if (*index >= ADAP_NORM_RESP_ENTRIES)
			*index = 0; /* Wrap to front of the Producer Queue. */
	}

	/* Queue is full */
	if ((*index + 1) == le32_to_cpu(*(q->headers.consumer))) {
		printk(KERN_WARNING "Queue %d full, %u outstanding.\n",
				qid, atomic_read(&q->numpending));
		return 0;
	} else {
		*entry = q->base + *index;
		return 1;
	}
}

/**
 *	aac_queue_get		-	get the next free QE
 *	@dev: Adapter
 *	@index: Returned index
 *	@priority: Priority of fib
 *	@fib: Fib to associate with the queue entry
 *	@wait: Wait if queue full
 *	@fibptr: Driver fib object to go with fib
 *	@nonotify: Don't notify the adapter
 *
 *	Gets the next free QE off the requested priorty adapter command
 *	queue and associates the Fib with the QE. The QE represented by
 *	index is ready to insert on the queue when this routine returns
 *	success.
 */

int aac_queue_get(struct aac_dev * dev, u32 * index, u32 qid, struct hw_fib * hw_fib, int wait, struct fib * fibptr, unsigned long *nonotify)
{
	struct aac_entry * entry = NULL;
	int map = 0;

	if (qid == AdapNormCmdQueue) {
		/*  if no entries wait for some if caller wants to */
		while (!aac_get_entry(dev, qid, &entry, index, nonotify)) {
			printk(KERN_ERR "GetEntries failed\n");
		}
		/*
		 *	Setup queue entry with a command, status and fib mapped
		 */
		entry->size = cpu_to_le32(le16_to_cpu(hw_fib->header.Size));
		map = 1;
	} else {
		while (!aac_get_entry(dev, qid, &entry, index, nonotify)) {
			/* if no entries wait for some if caller wants to */
		}
		/*
		 *	Setup queue entry with command, status and fib mapped
		 */
		entry->size = cpu_to_le32(le16_to_cpu(hw_fib->header.Size));
		entry->addr = hw_fib->header.SenderFibAddress;
			/* Restore adapters pointer to the FIB */
		hw_fib->header.u.ReceiverFibAddress = hw_fib->header.SenderFibAddress;	/* Let the adapter now where to find its data */
		map = 0;
	}
	/*
	 *	If MapFib is true than we need to map the Fib and put pointers
	 *	in the queue entry.
	 */
	if (map)
		entry->addr = cpu_to_le32(fibptr->hw_fib_pa);
	return 0;
}

/*
 *	Define the highest level of host to adapter communication routines.
 *	These routines will support host to adapter FS commuication. These
 *	routines have no knowledge of the commuication method used. This level
 *	sends and receives FIBs. This level has no knowledge of how these FIBs
 *	get passed back and forth.
 */

/**
 *	aac_fib_send	-	send a fib to the adapter
 *	@command: Command to send
 *	@fibptr: The fib
 *	@size: Size of fib data area
 *	@priority: Priority of Fib
 *	@wait: Async/sync select
 *	@reply: True if a reply is wanted
 *	@callback: Called with reply
 *	@callback_data: Passed to callback
 *
 *	Sends the requested FIB to the adapter and optionally will wait for a
 *	response FIB. If the caller does not wish to wait for a response than
 *	an event to wait on must be supplied. This event will be set when a
 *	response FIB is received from the adapter.
 */
#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
#undef aac_fib_send
fib_send_t aac_fib_send_switch = aac_fib_send;
#endif

int aac_fib_send(u16 command, struct fib *fibptr, unsigned long size,
		int priority, int wait, int reply, fib_callback callback,
		void *callback_data)
#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
#define aac_fib_send aac_fib_send_switch
#endif
{
	struct aac_dev * dev = fibptr->dev;
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	unsigned long flags = 0;
	unsigned long mflags = 0;
	unsigned long sflags;

	if (!(hw_fib->header.XferState & cpu_to_le32(HostOwned)))
		return -EBUSY;

	/*
	 *	There are 5 cases with the wait and reponse requested flags.
	 *	The only invalid cases are if the caller requests to wait and
	 *	does not request a response and if the caller does not want a
	 *	response and the Fib is not allocated from pool. If a response
	 *	is not requesed the Fib will just be deallocaed by the DPC
	 *	routine when the response comes back from the adapter. No
	 *	further processing will be done besides deleting the Fib. We
	 *	will have a debug mode where the adapter can notify the host
	 *	it had a problem and the host can log that fact.
	 */
	fibptr->flags = 0;
	if (wait && !reply) {
		return -EINVAL;
	} else if (!wait && reply) {
		hw_fib->header.XferState |= cpu_to_le32(Async | ResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.AsyncSent);
	} else if (!wait && !reply) {
		hw_fib->header.XferState |= cpu_to_le32(NoResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.NoResponseSent);
	} else if (wait && reply) {
		hw_fib->header.XferState |= cpu_to_le32(ResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.NormalSent);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x8)
		if ((wait == 1) && (reply == 1) && !callback &&
		  !callback_data && !list_empty(&dev->entry)) {

			adbg_ioctl(dev,KERN_INFO,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL)\n",
			  command, fibptr, size, priority);

			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL)",
			  command, fibptr, size, priority));
		}
#endif
#endif
#if (defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
		/* If wait >0 and reply != 0 then there's a potential
		 * to land in down() below which will eventually land
		 * us where we'll try to sleep. We can't sleep in this
		 * routine because we're in the issuing path for the
		 * VMkernel. Setting wait <0 causes us to spin for up
		 * to three minutes with down_trylock() instead of down()
		 * so we won't trip the World_AssertIsSafeToBlock().
		 */
		wait = -1;
#endif
	}
	/*
	 *	Map the fib into 32bits by using the fib number
	 */

	hw_fib->header.SenderFibAddress = 
		cpu_to_le32(((u32)(fibptr - dev->fibs)) << 2);
	
	/* use the same shifted value for handle to be compatible
	 * with the new native hba command handle
	 */
	hw_fib->header.Handle = 
		cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);

	/*
	 *	Set FIB state to indicate where it came from and if we want a
	 *	response from the adapter. Also load the command from the
	 *	caller.
	 *
	 *	Map the hw fib pointer as a 32bit value
	 */
	hw_fib->header.Command = cpu_to_le16(command);
	hw_fib->header.XferState |= cpu_to_le32(SentFromHost);
	/*
	 *	Set the size of the Fib we want to send to the adapter
	 */
	hw_fib->header.Size = cpu_to_le16(sizeof(struct aac_fibhdr) + size);
	if (le16_to_cpu(hw_fib->header.Size) > le16_to_cpu(hw_fib->header.SenderSize)) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x8)
		if ((wait == 1) && (reply == 1) && !callback &&
		  !callback_data && !list_empty(&dev->entry)) {
			adbg_ioctl(dev,KERN_INFO,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -EMSGSIZE\n",
			  command, fibptr, size, priority);
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -EMSGSIZE",
			  command, fibptr, size, priority));
		}
#endif
#endif
		return -EMSGSIZE;
	}
	/*
	 *	Get a queue entry connect the FIB to it and send an notify
	 *	the adapter a command is ready.
	 */
	hw_fib->header.XferState |= cpu_to_le32(NormalPriority);

	/*
	 *	Fill in the Callback and CallbackContext if we are not
	 *	going to wait.
	 */
	if (!wait) {
		fibptr->callback = callback;
		fibptr->callback_data = callback_data;
		fibptr->flags = FIB_CONTEXT_FLAG;
	}

	fibptr->done = 0;

#	if (defined(AAC_DEBUG_INSTRUMENT_FIB))
		printk(KERN_INFO "Fib content %p[%d] P=%llx:\n",
		  hw_fib, le16_to_cpu(hw_fib->header.Size), fibptr->hw_fib_pa);
		{
			int size = le16_to_cpu(hw_fib->header.Size)
					/ sizeof(u32);
			char buffer[80];
			u32 * up = (u32 *)hw_fib;

			while (size > 0) {
				sprintf (buffer,
				  "  %08x %08x %08x %08x %08x %08x %08x %08x\n",
				  up[0], up[1], up[2], up[3], up[4], up[5],
				  up[6], up[7]);
				up += 8;
				size -= 8;
				if (size < 0) {
					buffer[73+(size*9)] = '\n';
					buffer[74+(size*9)] = '\0';
				}
				printk(KERN_INFO "%s", buffer);
			}
		}
#	endif
	FIB_COUNTER_INCREMENT(aac_config.FibsSent);

	dprintk((KERN_DEBUG "Fib contents:.\n"));
	dprintk((KERN_DEBUG "  Command =               %d.\n", le32_to_cpu(hw_fib->header.Command)));
	dprintk((KERN_DEBUG "  SubCommand =            %d.\n", le32_to_cpu(((struct aac_query_mount *)fib_data(fibptr))->command)));
	dprintk((KERN_DEBUG "  XferState  =            %x.\n", le32_to_cpu(hw_fib->header.XferState)));
	dprintk((KERN_DEBUG "  hw_fib va being sent=%p\n",fibptr->hw_fib_va));
	dprintk((KERN_DEBUG "  hw_fib pa being sent=%lx\n",(ulong)fibptr->hw_fib_pa));
	dprintk((KERN_DEBUG "  fib being sent=%p\n",fibptr));

	if (!dev->queues)
#if (defined(AAC_DEBUG_INSTRUMENT_FIB) || defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
	{
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x8)
		if ((wait == 1) && (reply == 1) && !callback &&
		  !callback_data && !list_empty(&dev->entry)) {
			adbg_ioctl(dev,KERN_INFO,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -EBUSY\n",
			  command, fibptr, size, priority);
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -EBUSY",
			  command, fibptr, size, priority));
		}
#endif
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
		printk(KERN_INFO "aac_fib_send: dev->queues gone!\n");
#endif
#endif
		return -EBUSY;
#if (defined(AAC_DEBUG_INSTRUMENT_FIB) || defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
	}
#endif

	if(wait) {
		spin_lock_irqsave(&dev->manage_lock, mflags);
		if (dev->management_fib_count >= AAC_NUM_MGT_FIB) {
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
			return -EBUSY;
		}
		dev->management_fib_count++;
		spin_unlock_irqrestore(&dev->manage_lock, mflags);
		spin_lock_irqsave(&fibptr->event_lock, flags);
	}

	if (dev->sync_mode) {
		if (wait)
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
		spin_lock_irqsave(&dev->sync_lock, sflags);
		if (dev->sync_fib) {
			list_add_tail(&fibptr->fiblink, &dev->sync_fib_list);
			spin_unlock_irqrestore(&dev->sync_lock, sflags);
		} else {
			dev->sync_fib = fibptr;
			spin_unlock_irqrestore(&dev->sync_lock, sflags);
			aac_adapter_sync_cmd(dev, SEND_SYNCHRONOUS_FIB, 
				(u32)fibptr->hw_fib_pa, 0, 0, 0, 0, 0, 
				NULL, NULL, NULL, NULL, NULL);
		}
		if (wait) {
			fibptr->flags |= FIB_CONTEXT_FLAG_WAIT;
			if (down_interruptible(&fibptr->event_wait)) {
				fibptr->flags &= ~FIB_CONTEXT_FLAG_WAIT;
				return -EFAULT;
			}
			return 0;
		}
		return -EINPROGRESS;
	}

	DBG_SET_STATE(fibptr, DBG_STATE_INITIALIZED);

	if (aac_adapter_deliver(fibptr) != 0) {
		if (wait) {
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			spin_lock_irqsave(&dev->manage_lock, mflags);
			dev->management_fib_count--;
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
		}
		return -EBUSY;
	}

	/*
	 *	If the caller wanted us to wait for response wait now.
	 */

	if (wait) {
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		/* Only set for first known interruptable command */
		if (wait < 0) {
			/*
			 * *VERY* Dangerous to time out a command, the
			 * assumption is made that we have no hope of
			 * functioning because an interrupt routing or other
			 * hardware failure has occurred.
			 */
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
			unsigned long count = 180000L;  /* 3 minutes */
#else
			unsigned long count = jiffies + (180 * HZ); /* 3 minutes */
#endif
			while (down_trylock(&fibptr->event_wait)) {
				int blink;
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				if (--count == 0) {
#else
				if (time_after(jiffies, count)) {
#endif
					struct aac_queue * q = &dev->queues->queue[AdapNormCmdQueue];
					atomic_dec(&q->numpending);
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: first asynchronous command timed out.\n"
						  "Usually a result of a PCI interrupt routing problem;\n"
						  "update mother board BIOS or consider utilizing one of\n"
						  "the SAFE mode kernel options (acpi, apic etc)\n");
					}
					spin_lock_irqsave(&fibptr->event_lock, flags);
					fibptr->done = 2;
					spin_unlock_irqrestore(&fibptr->event_lock, flags);
					dprintk((KERN_ERR "aacraid: sync. command timed out after 180 seconds\n"));
					return -ETIMEDOUT;
				}
				if ((blink = aac_adapter_check_health(dev)) > 0) {
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: adapter blinkLED 0x%x.\n"
						  "Usually a result of a serious unrecoverable hardware problem\n",
						  blink);
					}
					return -EFAULT;
				}
#if (defined(CONFIG_XEN) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
				udelay(5);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32))
				cpu_relax();
#else
				msleep(1);
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				/**
				* We are waiting for an interrupt but might be in a deadlock here.
				* Since vmkernel is non-preemtible and we are hogging the CPU,
				* in special circumstances the interrupt handler may not get to run.
				* For more details, please see PR 414597.
				*/
				if ((count % 5L) == 0L) {
					// busy waiting for at most 5 ms at a time.
					dprintk((KERN_INFO "aacraid: aac_fib_send: Calling interrupt handler\n"));
					if (!dev->msi_enabled) {
						if (dev->pdev->device != PMC_DEVICE_S6 &&
	    				    	    dev->pdev->device != PMC_DEVICE_S7 &&
	    				    	    dev->pdev->device != PMC_DEVICE_S8 &&
	    				    	    dev->pdev->device != PMC_DEVICE_S9) {
							disable_irq(dev->scsi_host_ptr->irq);
							aac_adapter_intr(dev);
							enable_irq(dev->scsi_host_ptr->irq);
						}
					}
				}
#endif
			}
		} else if (down_interruptible(&fibptr->event_wait)) {
			fibptr->done = 2;
			up(&fibptr->event_wait);
		}
		spin_lock_irqsave(&fibptr->event_lock, flags);
		if ((fibptr->done == 0) || (fibptr->done == 2)) {
			fibptr->done = 2; /* Tell interrupt we aborted */
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x8)
			if ((wait == 1) && (reply == 1) && !callback &&
			  !callback_data && !list_empty(&dev->entry)) {
				adbg_ioctl(dev,KERN_INFO,
				  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -EINTR\n",
				  command, fibptr, size, priority);
				fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -EINTR",
				  command, fibptr, size, priority));
			}
#endif
#endif
			return -ERESTARTSYS;
		}
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		BUG_ON(fibptr->done == 0);

		if(unlikely(fibptr->flags & FIB_CONTEXT_FLAG_TIMED_OUT))
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x8)
		{
			if ((wait == 1) && (reply == 1) && !callback &&
			  !callback_data && !list_empty(&dev->entry)) {
				adbg_ioctl(dev,KERN_INFO,
				  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -ETIMEDOUT\n",
				  command, fibptr, size, priority);
				fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns -ETIMEDOUT",
				  command, fibptr, size, priority));
			}
			return -ETIMEDOUT;
		}
#else
			return -ETIMEDOUT;
#endif
#else
			return -ETIMEDOUT;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x8)
		if ((wait == 1) && (reply == 1) && !callback &&
		  !callback_data && !list_empty(&dev->entry)) {
			adbg_ioctl(dev,KERN_INFO,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns 0\n",
			  command, fibptr, size, priority);
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  "aac_fib_send(%u,%p,%lu,%d,1,1,NULL,NULL) returns 0",
			  command, fibptr, size, priority));
		}
#endif
#endif
		return 0;
	}
	/*
	 *	If the user does not want a response than return success otherwise
	 *	return pending
	 */
	if (reply)
		return -EINPROGRESS;
	else
		return 0;
}

int aac_hba_send(u8 command, struct fib *fibptr, fib_callback callback,
		void *callback_data)
{
	struct aac_dev * dev = fibptr->dev;
	int wait;
	unsigned long flags = 0;
	unsigned long mflags = 0;

	fibptr->flags = (FIB_CONTEXT_FLAG | FIB_CONTEXT_FLAG_NATIVE_HBA);
	if (callback) {
		wait = 0;
		fibptr->callback = callback;
		fibptr->callback_data = callback_data;
	} else {	
#if (defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
		wait = -1;
#else
		wait = 1;
#endif		
	}
	
	if (command == HBA_IU_TYPE_SCSI_CMD_REQ) {
		struct aac_hba_cmd_req *hbacmd = 
			(struct aac_hba_cmd_req *)fibptr->hw_fib_va;
	
		hbacmd->iu_type = command;
		/* bit1 of request_id must be 0 */
		hbacmd->request_id =
			cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);
	} else if (command == HBA_IU_TYPE_SCSI_TM_REQ) {
		struct aac_hba_tm_req *hbacmd = 
			(struct aac_hba_tm_req *)fibptr->hw_fib_va;
	
		hbacmd->iu_type = command;
		/* bit1 of request_id must be 0 */
		hbacmd->request_id =
			cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);
		fibptr->flags |= FIB_CONTEXT_FLAG_NATIVE_HBA_TMF;
	} else if (command == HBA_IU_TYPE_SATA_REQ) {
		struct aac_hba_reset_req *hbacmd = 
			(struct aac_hba_reset_req *)fibptr->hw_fib_va;
	
		hbacmd->iu_type = command;
		/* bit1 of request_id must be 0 */
		hbacmd->request_id =
			cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);
		fibptr->flags |= FIB_CONTEXT_FLAG_NATIVE_HBA_TMF;
	} else {
		return -EINVAL;
	}	

	if (wait) {
		spin_lock_irqsave(&dev->manage_lock, mflags);
		if (dev->management_fib_count >= AAC_NUM_MGT_FIB) {
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
			return -EBUSY;
		}
		dev->management_fib_count++;
		spin_unlock_irqrestore(&dev->manage_lock, mflags);
		spin_lock_irqsave(&fibptr->event_lock, flags);
	}	
	
	if (aac_adapter_deliver(fibptr) != 0) { 
		if (wait) {
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			spin_lock_irqsave(&dev->manage_lock, mflags);
			dev->management_fib_count--;
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
		}
		return -EBUSY;
	}
	FIB_COUNTER_INCREMENT(aac_config.NativeSent);

	if (wait) {
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		/* Only set for first known interruptable command */
		if (wait < 0) {
			/*
			 * *VERY* Dangerous to time out a command, the
			 * assumption is made that we have no hope of
			 * functioning because an interrupt routing or other
			 * hardware failure has occurred.
			 */
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
			unsigned long count = 180000L;  /* 3 minutes */
#else
			unsigned long count = jiffies + (180 * HZ); /* 3 minutes */
#endif
			while (down_trylock(&fibptr->event_wait)) {
				int blink;
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				if (--count == 0) {
#else
				if (time_after(jiffies, count)) {
#endif
					struct aac_queue * q = &dev->queues->queue[AdapNormCmdQueue];
					atomic_dec(&q->numpending);
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: first asynchronous command timed out.\n"
						  "Usually a result of a PCI interrupt routing problem;\n"
						  "update mother board BIOS or consider utilizing one of\n"
						  "the SAFE mode kernel options (acpi, apic etc)\n");
					}
					spin_lock_irqsave(&fibptr->event_lock, flags);
					fibptr->done = 2;
					spin_unlock_irqrestore(&fibptr->event_lock, flags);
					dprintk((KERN_ERR "aacraid: sync. command timed out after 180 seconds\n"));
					return -ETIMEDOUT;
				}
				if ((blink = aac_adapter_check_health(dev)) > 0) {
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: adapter blinkLED 0x%x.\n"
						  "Usually a result of a serious unrecoverable hardware problem\n",
						  blink);
					}
					return -EFAULT;
				}
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				msleep(1);
#else
				cpu_relax();
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				/**
				* We are waiting for an interrupt but might be in a deadlock here.
				* Since vmkernel is non-preemtible and we are hogging the CPU,
				* in special circumstances the interrupt handler may not get to run.
				* For more details, please see PR 414597.
				*/
				if ((count % 5L) == 0L) {
					// busy waiting for at most 5 ms at a time.
					dprintk((KERN_INFO "aacraid: aac_fib_send: Calling interrupt handler\n"));
					if (!dev->msi_enabled) {
						if (dev->pdev->device != PMC_DEVICE_S6 &&
	    				    	    dev->pdev->device != PMC_DEVICE_S7 &&
	    				    	    dev->pdev->device != PMC_DEVICE_S8 &&
	    				    	    dev->pdev->device != PMC_DEVICE_S9) {
							disable_irq(dev->scsi_host_ptr->irq);
							aac_adapter_intr(dev);
							enable_irq(dev->scsi_host_ptr->irq);
						}
					}
				}
#endif
			}
		} else if (down_interruptible(&fibptr->event_wait)) {
			fibptr->done = 2;
			up(&fibptr->event_wait);
		}
		spin_lock_irqsave(&fibptr->event_lock, flags);
		if ((fibptr->done == 0) || (fibptr->done == 2)) {
			fibptr->done = 2; /* Tell interrupt we aborted */
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			return -ERESTARTSYS;
		}
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		BUG_ON(fibptr->done == 0);

		if(unlikely(fibptr->flags & FIB_CONTEXT_FLAG_TIMED_OUT))
			return -ETIMEDOUT;

		return 0;
	}
	
	return -EINPROGRESS;
}

/**
 *	aac_consumer_get	-	get the top of the queue
 *	@dev: Adapter
 *	@q: Queue
 *	@entry: Return entry
 *
 *	Will return a pointer to the entry on the top of the queue requested that
 *	we are a consumer of, and return the address of the queue entry. It does
 *	not change the state of the queue.
 */

int aac_consumer_get(struct aac_dev * dev, struct aac_queue * q, struct aac_entry **entry)
{
	u32 index;
	int status;
	if (le32_to_cpu(*q->headers.producer) == le32_to_cpu(*q->headers.consumer)) {
		status = 0;
	} else {
		/*
		 *	The consumer index must be wrapped if we have reached
		 *	the end of the queue, else we just use the entry
		 *	pointed to by the header index
		 */
		if (le32_to_cpu(*q->headers.consumer) >= q->entries)
			index = 0;
		else
			index = le32_to_cpu(*q->headers.consumer);
		*entry = q->base + index;
		status = 1;
	}
	return(status);
}

/**
 *	aac_consumer_free	-	free consumer entry
 *	@dev: Adapter
 *	@q: Queue
 *	@qid: Queue ident
 *
 *	Frees up the current top of the queue we are a consumer of. If the
 *	queue was full notify the producer that the queue is no longer full.
 */

void aac_consumer_free(struct aac_dev * dev, struct aac_queue *q, u32 qid)
{
	int wasfull = 0;
	u32 notify;

	if ((le32_to_cpu(*q->headers.producer)+1) == le32_to_cpu(*q->headers.consumer))
		wasfull = 1;

	if (le32_to_cpu(*q->headers.consumer) >= q->entries)
		*q->headers.consumer = cpu_to_le32(1);
	else
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,24)) && !defined(HAS_LE32_ADD_CPU))
		*q->headers.consumer = cpu_to_le32(le32_to_cpu(*q->headers.consumer)+1);
#else
		le32_add_cpu(q->headers.consumer, 1);
#endif

	if (wasfull) {
		switch (qid) {

		case HostNormCmdQueue:
			notify = HostNormCmdNotFull;
			break;
		case HostNormRespQueue:
			notify = HostNormRespNotFull;
			break;
		default:
			BUG();
			return;
		}
		aac_adapter_notify(dev, notify);
	}
}

/**
 *	aac_fib_adapter_complete	-	complete adapter issued fib
 *	@fibptr: fib to complete
 *	@size: size of fib
 *
 *	Will do all necessary work to complete a FIB that was sent from
 *	the adapter.
 */

int aac_fib_adapter_complete(struct fib *fibptr, unsigned short size)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	struct aac_dev * dev = fibptr->dev;
	struct aac_queue * q;
	unsigned long nointr = 0;
	unsigned long qflags;

	if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE1 || 
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE2 || 
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE3) {
		kfree (hw_fib);
		return 0;
	}

	if (hw_fib->header.XferState == 0) {
		if (dev->comm_interface == AAC_COMM_MESSAGE)
			kfree (hw_fib);
		return 0;
	}
	/*
	 *	If we plan to do anything check the structure type first.
	 */
	if (hw_fib->header.StructType != FIB_MAGIC &&
		hw_fib->header.StructType != FIB_MAGIC2 &&
		hw_fib->header.StructType != FIB_MAGIC2_64) {
		if (dev->comm_interface == AAC_COMM_MESSAGE)
			kfree (hw_fib);
		return -EINVAL;
	}
	/*
	 *	This block handles the case where the adapter had sent us a
	 *	command and we have finished processing the command. We
	 *	call completeFib when we are done processing the command
	 *	and want to send a response back to the adapter. This will
	 *	send the completed cdb to the adapter.
	 */
	if (hw_fib->header.XferState & cpu_to_le32(SentFromAdapter)) {
		if (dev->comm_interface == AAC_COMM_MESSAGE) {
			kfree (hw_fib);
		} else {
			u32 index;
			hw_fib->header.XferState |= cpu_to_le32(HostProcessed);
			if (size) {
				size += sizeof(struct aac_fibhdr);
				if (size > le16_to_cpu(hw_fib->header.SenderSize))
					return -EMSGSIZE;
				hw_fib->header.Size = cpu_to_le16(size);
			}
			q = &dev->queues->queue[AdapNormRespQueue];
			spin_lock_irqsave(q->lock, qflags);
			aac_queue_get(dev, &index, AdapNormRespQueue, hw_fib, 1, NULL, &nointr);
			*(q->headers.producer) = cpu_to_le32(index + 1);
			spin_unlock_irqrestore(q->lock, qflags);
			if (!(nointr & (int)aac_config.irq_mod))
				aac_adapter_notify(dev, AdapNormRespQueue);
		}
	} else {
		printk(KERN_WARNING "aac_fib_adapter_complete: "
			"Unknown xferstate detected.\n");
		BUG();
	}
	return 0;
}

/**
 *	aac_fib_complete	-	fib completion handler
 *	@fib: FIB to complete
 *
 *	Will do all necessary work to complete a FIB.
 */

int aac_fib_complete(struct fib *fibptr)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;

	/*
	 *	Check for a fib which has already been completed or with a
	 *	status wait timeout
	 */

	if (fibptr->flags & FIB_CONTEXT_FLAG_NATIVE_HBA) {
		fib_dealloc(fibptr);
		return 0;
	}

	if (hw_fib->header.XferState == 0 || fibptr->done == 2)
		return 0;
	/*
	 *	If we plan to do anything check the structure type first.
	 */

	if (hw_fib->header.StructType != FIB_MAGIC &&
		hw_fib->header.StructType != FIB_MAGIC2 &&
		hw_fib->header.StructType != FIB_MAGIC2_64)
		return -EINVAL;
	/*
	 *	This block completes a cdb which orginated on the host and we
	 *	just need to deallocate the cdb or reinit it. At this point the
	 *	command is complete that we had sent to the adapter and this
	 *	cdb could be reused.
	 */

	if((hw_fib->header.XferState & cpu_to_le32(SentFromHost)) &&
		(hw_fib->header.XferState & cpu_to_le32(AdapterProcessed)))
	{
		fib_dealloc(fibptr);
	}
	else if(hw_fib->header.XferState & cpu_to_le32(SentFromHost))
	{
		/*
		 *	This handles the case when the host has aborted the I/O
		 *	to the adapter because the adapter is not responding
		 */
		fib_dealloc(fibptr);
	} else if(hw_fib->header.XferState & cpu_to_le32(HostOwned)) {
		fib_dealloc(fibptr);
	} else {
		BUG();
	}
	return 0;
}

/**
 *	aac_printf	-	handle printf from firmware
 *	@dev: Adapter
 *	@val: Message info
 *
 *	Print a message passed to us by the controller firmware on the
 *	Adaptec board
 */

void aac_printf(struct aac_dev *dev, u32 val)
{
	char *cp = dev->printfbuf;
#if (!defined(AAC_PRINTF_ENABLED))
	if (dev->printf_enabled)
#endif
	{
		int length = val & 0xffff;
		int level = (val >> 16) & 0xffff;

		/*
		 *	The size of the printfbuf is set in port.c
		 *	There is no variable or define for it
		 */
		if (length > 255)
			length = 255;
		if (cp[length] != 0)
			cp[length] = 0;
		if (level == LOG_AAC_HIGH_ERROR)
			printk(KERN_WARNING "%s:%s", dev->name, cp);
		else
			printk(KERN_INFO "%s:%s", dev->name, cp);
	}
	memset(cp, 0, 256);
}

int aac_issue_bmic_identify(struct aac_dev* dev, u32 bus, u32 target);

/**
 *	aac_handle_sa_aif		-	Handle a message from the firmware
 *	@dev: Which adapter this fib is from
 *	@fibptr: Pointer to fibptr from adapter
 *
 *	This routine handles a driver notify fib from the adapter and
 *	dispatches it to the appropriate routine for handling.
 */

static void aac_handle_sa_aif(struct aac_dev * dev, struct fib * fibptr)
{
	int i, bus, target, container, channel, rcode = 0;
	u32 lun_count, nexus, events = 0;
	u16 fibsize, datasize;
	u8 expose_flag, attribs;
	struct fib *fib;
	struct aac_srb *srbcmd;
	struct sgmap64 *sg64;
	struct aac_ciss_phys_luns_resp *phys_luns;
	struct scsi_device *sdev;
	dma_addr_t addr;
	
	if (fibptr->hbacmd_size & SA_AIF_HOTPLUG)
		events = SA_AIF_HOTPLUG;
	else if(fibptr->hbacmd_size & SA_AIF_HARDWARE)
		events = SA_AIF_HARDWARE;
	else if(fibptr->hbacmd_size & SA_AIF_PDEV_CHANGE)
		events = SA_AIF_PDEV_CHANGE;
	else if(fibptr->hbacmd_size & SA_AIF_LDEV_CHANGE)
		events = SA_AIF_LDEV_CHANGE;
	else if(fibptr->hbacmd_size & SA_AIF_BPSTAT_CHANGE)
		events = SA_AIF_BPSTAT_CHANGE;
	else if(fibptr->hbacmd_size & SA_AIF_BPCFG_CHANGE)
		events = SA_AIF_BPCFG_CHANGE;

	switch (events) {
	case SA_AIF_HOTPLUG:
	case SA_AIF_HARDWARE:
	case SA_AIF_PDEV_CHANGE:
	case SA_AIF_LDEV_CHANGE:

		if (!(fib = aac_fib_alloc(dev))) {
			printk(KERN_ERR "aac_handle_sa_aif: out of memory\n");
			return;
		}
		fibsize = sizeof (struct aac_srb) - 
			sizeof (struct sgentry) + sizeof (struct sgentry64);
		datasize = sizeof (struct aac_ciss_phys_luns_resp) +
			(AAC_MAX_NATIVE_TARGETS - 1) * sizeof (struct _ciss_lun);

		phys_luns = (struct aac_ciss_phys_luns_resp *)
			pci_alloc_consistent(dev->pdev, datasize, &addr);
		if (phys_luns != NULL) {
			u32 vbus, vid;
			vbus = (u32)le16_to_cpu(
				dev->supplement_adapter_info.VirtDeviceBus);
			vid = (u32)le16_to_cpu(
				dev->supplement_adapter_info.VirtDeviceTarget);

			aac_fib_init(fib);
			srbcmd = (struct aac_srb *) fib_data(fib);

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
				fib, fibsize, FsaNormal, 1, 1, NULL, NULL);

			/* analyse data */
			if (rcode >= 0 && phys_luns->resp_flag == 2) {
					
				for (bus = 0; bus < AAC_MAX_BUSES; bus++)
					for (target = 0; target < AAC_MAX_TARGETS; target++)
						dev->cur_hba_map[bus][target].devtype = 0;

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
							if (nexus != 0) { 
								dev->cur_hba_map[bus][target].devtype =
									AAC_DEVTYPE_NATIVE_RAW;
								dev->hba_map[bus][target].rmw_nexus =
									nexus;
							} else {
								dev->cur_hba_map[bus][target].devtype =
									AAC_DEVTYPE_ARC_RAW;
							}
						} else {
							dev->cur_hba_map[bus][target].devtype =
								AAC_DEVTYPE_RAID_MEMBER;
					  	}
						if( dev->cur_hba_map[bus][target].devtype == AAC_DEVTYPE_NATIVE_RAW )
							dev->hba_map[bus][target].devtype = AAC_DEVTYPE_NATIVE_RAW;
					 		if( aac_issue_bmic_identify(dev, bus, target) < 0 ){
								dev->hba_map[bus][target].qd_limit = 32;
							}
					 }
				}
				aac_fib_complete(fib);
			}

			if (rcode != -ERESTARTSYS)
				aac_fib_free(fib);

			for (bus = 0; bus < AAC_MAX_BUSES; bus++) {
				for (target = 0; target < AAC_MAX_TARGETS && aac_phys_to_logical(bus) != ENCLOSURE_CHANNEL; target++) {
					if (bus == CONTAINER_CHANNEL)
						channel = CONTAINER_CHANNEL;
					else
						channel = aac_phys_to_logical(bus);
					sdev = scsi_device_lookup(dev->scsi_host_ptr, channel, target, 0);
					if (!sdev && dev->cur_hba_map[bus][target].devtype != 0)
						scsi_add_device(dev->scsi_host_ptr, channel, target, 0);
					else if (sdev && dev->hba_map[bus][target].devtype != dev->cur_hba_map[bus][target].devtype) {
						scsi_remove_device(sdev);
						scsi_device_put(sdev);
					} else if(sdev && dev->hba_map[bus][target].devtype == dev->cur_hba_map[bus][target].devtype) {
						scsi_rescan_device(&sdev->sdev_gendev);
						scsi_device_put(sdev);
					}
					dev->hba_map[bus][target].devtype = dev->cur_hba_map[bus][target].devtype;
				}
			}
			pci_free_consistent(dev->pdev, datasize, (void *)phys_luns, addr);
		}
		if (events == SA_AIF_LDEV_CHANGE) {
			aac_get_containers(dev);
			for (container = 0; container < dev->maximum_num_containers; ++container) {
				sdev = scsi_device_lookup(dev->scsi_host_ptr, CONTAINER_CHANNEL, container, 0);
				if (dev->fsa_dev[container].valid && !sdev) {
					scsi_add_device(dev->scsi_host_ptr, CONTAINER_CHANNEL, container, 0);
				} else if (!dev->fsa_dev[container].valid && sdev) {
					scsi_remove_device(sdev);
					scsi_device_put(sdev);
				} else if (sdev) {
					scsi_rescan_device(&sdev->sdev_gendev);
					scsi_device_put(sdev);
				}
			}
		}
		break;

	case SA_AIF_BPSTAT_CHANGE:
	case SA_AIF_BPCFG_CHANGE:
		/* currently do nothing */
		break;
	}
	
	for (i = 1; i <= 10; ++i) {
		events = src_readl(dev, MUnit.IDR);
		if (events & (1<<23)) {
			printk(KERN_WARNING "aac_handle_sa_aif: AIF not cleared by firmware (attempt %d/%d)\n",
				i, 10);
			ssleep(1);
		}
	}
}

/**
 *	aac_handle_aif		-	Handle a message from the firmware
 *	@dev: Which adapter this fib is from
 *	@fibptr: Pointer to fibptr from adapter
 *
 *	This routine handles a driver notify fib from the adapter and
 *	dispatches it to the appropriate routine for handling.
 */

#define AIF_SNIFF_TIMEOUT	(500*HZ)
static void aac_handle_aif(struct aac_dev * dev, struct fib * fibptr)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	struct aac_aifcmd * aifcmd = (struct aac_aifcmd *)hw_fib->data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))
	int busy;
#endif
	u32 channel, id, lun, container;
	struct scsi_device *device;
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)) && defined(MODULE))
	struct scsi_driver * drv;
#endif
	enum {
		NOTHING,
		DELETE,
		ADD,
		CHANGE
	} device_config_needed = NOTHING;
//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE)))
	extern struct proc_dir_entry * proc_scsi;
#endif

	/* Sniff for container changes */
	adbg_aif(dev,KERN_INFO,
	  "aac_handle_aif: Aif command=%x type=%x container=%d\n",
	  le32_to_cpu(aifcmd->command), le32_to_cpu(*(__le32 *)aifcmd->data),
	  le32_to_cpu(((__le32 *)aifcmd->data)[1]));

	if (!dev || !dev->fsa_dev)
		return;
	container = channel = id = lun = (u32)-1;

	/*
	 *	We have set this up to try and minimize the number of
	 * re-configures that take place. As a result of this when
	 * certain AIF's come in we will set a flag waiting for another
	 * type of AIF before setting the re-config flag.
	 */
	switch (le32_to_cpu(aifcmd->command)) {
	case AifCmdDriverNotify:
		switch (le32_to_cpu(((__le32 *)aifcmd->data)[0])) {
		case AifRawDeviceRemove:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if ((container >> 28)) {
				container = (u32)-1;
				break;
			}
			channel = (container >> 24) & 0xF;
			if (channel >= dev->maximum_num_channels) {
				container = (u32)-1;
				break;
			}
			id = container & 0xFFFF;
			if (id >= dev->maximum_num_physicals) {
				container = (u32)-1;
				break;
			}
			lun = (container >> 16) & 0xFF;
			container = (u32)-1;
			channel = aac_phys_to_logical(channel);
			device_config_needed = DELETE;
			break;

		/*
		 *	Morph or Expand complete
		 */
		case AifDenMorphComplete:
		case AifDenVolumeExtendComplete:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
		    adbg_aif(dev,KERN_INFO,"container=%d(%d,%d,%d,%d)\n",
			  container,
			  (dev && dev->scsi_host_ptr)
			    ? dev->scsi_host_ptr->host_no
			    : -1, CONTAINER_TO_CHANNEL(container),
                                CONTAINER_TO_ID(container),
                                CONTAINER_TO_LUN(container));

			/*
			 *	Find the scsi_device associated with the SCSI
			 * address. Make sure we have the right array, and if
			 * so set the flag to initiate a new re-config once we
			 * see an AifEnConfigChange AIF come through.
			 */

			if ((dev != NULL) && (dev->scsi_host_ptr != NULL)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
				device = scsi_device_lookup(dev->scsi_host_ptr,
					CONTAINER_TO_CHANNEL(container),
					CONTAINER_TO_ID(container),
					CONTAINER_TO_LUN(container));
				if (device) {
					dev->fsa_dev[container].config_needed = CHANGE;
					dev->fsa_dev[container].config_waiting_on = AifEnConfigChange;
					dev->fsa_dev[container].config_waiting_stamp = jiffies;
					scsi_device_put(device);
				}
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
				shost_for_each_device(device,
					dev->scsi_host_ptr)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
				list_for_each_entry(device,
					&dev->scsi_host_ptr->my_devices,
					siblings)
#else
				for (device = dev->scsi_host_ptr->host_queue;
				  device != (struct scsi_device *)NULL;
				  device = device->next)
#endif
					if ((device->channel ==
						CONTAINER_TO_CHANNEL(container))
					 && (device->id ==
						CONTAINER_TO_ID(container))
					 && (device->lun ==
						CONTAINER_TO_LUN(container))) {
						dev->fsa_dev[container].config_needed = CHANGE;
						dev->fsa_dev[container].config_waiting_on = AifEnConfigChange;
						dev->fsa_dev[container].config_waiting_stamp = jiffies;
						break;
					}
#endif
			}
		}

		/*
		 *	If we are waiting on something and this happens to be
		 * that thing then set the re-configure flag.
		 */
		if (container != (u32)-1) {
			if (container >= dev->maximum_num_containers)
				break;
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		} else for (container = 0;
		    container < dev->maximum_num_containers; ++container) {
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		}
		break;

	case AifCmdEventNotify:
		switch (le32_to_cpu(((__le32 *)aifcmd->data)[0])) {
		case AifEnBatteryEvent:
			dev->cache_protected =
				(((__le32 *)aifcmd->data)[1] == cpu_to_le32(3));
			break;
		/*
		 *	Add an Array.
		 */
		case AifEnAddContainer:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			dev->fsa_dev[container].config_needed = ADD;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		/*
		 *	Delete an Array.
		 */
		case AifEnDeleteContainer:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			dev->fsa_dev[container].config_needed = DELETE;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		/*
		 *	Container change detected. If we currently are not
		 * waiting on something else, setup to wait on a Config Change.
		 */
		case AifEnContainerChange:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			if (dev->fsa_dev[container].config_waiting_on &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				break;
			dev->fsa_dev[container].config_needed = CHANGE;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		case AifEnConfigChange:
			break;

		case AifEnAddJBOD:
		case AifEnDeleteJBOD:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if ((container >> 28)) {
				container = (u32)-1;
				break;
			}
			channel = (container >> 24) & 0xF;
			if (channel >= dev->maximum_num_channels) {
				container = (u32)-1;
				break;
			}
			id = container & 0xFFFF;
			if (id >= dev->maximum_num_physicals) {
				container = (u32)-1;
				break;
			}
			lun = (container >> 16) & 0xFF;
			container = (u32)-1;
			channel = aac_phys_to_logical(channel);
			device_config_needed =
			  (((__le32 *)aifcmd->data)[0] ==
			    cpu_to_le32(AifEnAddJBOD)) ? ADD : DELETE;

			/*	ADPml11423: JBOD created in Redhat 5.3 OS are not available until System Reboot
			 *  After JBOD creation, Dynamic updation of scsi_device table was not handled earlier
			 *  Below code is to reinitialize scsi device, After creation of JBOD
			 *  Device structure is freshly initialized and discovered the same, 'fdisk -l' lists JBOD
			 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3)) 
			if (device_config_needed == ADD) {
				device = scsi_device_lookup(dev->scsi_host_ptr, channel, id, lun);
				if (device) {
					scsi_remove_device(device);
					scsi_device_put(device);
				}
			}
#endif	
			break;

		case AifEnEnclosureManagement:

			switch (le32_to_cpu(((__le32 *)aifcmd->data)[3])) {
			case EM_DRIVE_INSERTION:
			case EM_DRIVE_REMOVAL:
			case EM_SES_DRIVE_INSERTION:
			case EM_SES_DRIVE_REMOVAL:
				container = le32_to_cpu(
					((__le32 *)aifcmd->data)[2]);
				adbg_aif(dev,KERN_INFO,"dev_t=%x(%d,%d,%d,%d)\n",
				  container,
				  (dev && dev->scsi_host_ptr)
				    ? dev->scsi_host_ptr->host_no
				    : -1,
				  aac_phys_to_logical((container >> 24) & 0xF),
				  container & 0xFFF,
				  (container >> 16) & 0xFF);
				if ((container >> 28)) {
					container = (u32)-1;
					break;
				}
				channel = (container >> 24) & 0xF;
				if (channel >= dev->maximum_num_channels) {
					container = (u32)-1;
					break;
				}
				id = container & 0xFFFF;
				lun = (container >> 16) & 0xFF;
				container = (u32)-1;
				if (id >= dev->maximum_num_physicals) {
					/* legacy dev_t ? */
					if ((0x2000 <= id) || lun || channel ||
					  ((channel = (id >> 7) & 0x3F) >=
					  dev->maximum_num_channels))
						break;
					lun = (id >> 4) & 7;
					id &= 0xF;
				}
				channel = aac_phys_to_logical(channel);
				device_config_needed =
				  ((((__le32 *)aifcmd->data)[3]
				    == cpu_to_le32(EM_DRIVE_INSERTION)) ||
				    (((__le32 *)aifcmd->data)[3]
				    == cpu_to_le32(EM_SES_DRIVE_INSERTION))) ?
				    ADD : DELETE;
				break;
			}
			break;
		}

		/*
		 *	If we are waiting on something and this happens to be
		 * that thing then set the re-configure flag.
		 */
		if (container != (u32)-1) {
			if (container >= dev->maximum_num_containers)
				break;
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		} else for (container = 0;
		    container < dev->maximum_num_containers; ++container) {
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		}
		break;

	case AifCmdJobProgress:
		/*
		 *	These are job progress AIF's. When a Clear is being
		 * done on a container it is initially created then hidden from
		 * the OS. When the clear completes we don't get a config
		 * change so we monitor the job status complete on a clear then
		 * wait for a container change.
		 */
		adbg_aif(dev,KERN_INFO,
		  "aac_handle_aif: Aif command=AifCmdJobProgress job=%x [4]=%x [5]=%x [6]=%x\n",
		  le32_to_cpu(((__le32 *)aifcmd->data)[1]),
		  le32_to_cpu(((__le32 *)aifcmd->data)[4]),
		  le32_to_cpu(((__le32 *)aifcmd->data)[5]),
		  le32_to_cpu(((__le32 *)aifcmd->data)[6]));

		if (((__le32 *)aifcmd->data)[1] == cpu_to_le32(AifJobCtrZero) &&
		    (((__le32 *)aifcmd->data)[6] == ((__le32 *)aifcmd->data)[5] ||
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsSuccess) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsAborted) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsFailed) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsPreempted) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsPended) )) {
			for (container = 0;
			    container < dev->maximum_num_containers;
			    ++container) {
				/*
				 * Stomp on all config sequencing for all
				 * containers?
				 */
				dev->fsa_dev[container].config_waiting_on =
					AifEnContainerChange;
				dev->fsa_dev[container].config_needed = ADD;
				dev->fsa_dev[container].config_waiting_stamp =
					jiffies;
			}
			adbg_aif(dev,KERN_INFO,
			  "aac_handle_aif: Wait=AifEnContainerChange ADD\n");
		}
		if (((__le32 *)aifcmd->data)[1] == cpu_to_le32(AifJobCtrZero) &&
		    ((__le32 *)aifcmd->data)[6] == 0 &&
		    ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsRunning)) {
			for (container = 0;
			    container < dev->maximum_num_containers;
			    ++container) {
				/*
				 * Stomp on all config sequencing for all
				 * containers?
				 */
				dev->fsa_dev[container].config_waiting_on =
					AifEnContainerChange;
				dev->fsa_dev[container].config_needed = DELETE;
				dev->fsa_dev[container].config_waiting_stamp =
					jiffies;
			}
			adbg_aif(dev,KERN_INFO,
			  "aac_handle_aif: Wait=AifEnContainerChange DELETE\n");
		}
		break;
	}

//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE)))
	container = 0;
//#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
retry_next_on_busy:
//#endif
	if (device_config_needed == NOTHING)
	for (; container < dev->maximum_num_containers; ++container) {
#else
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))
	if (device_config_needed == NOTHING)
	for (container = 0; container < dev->maximum_num_containers;
	    ++container) {
#else
	container = 0;
retry_next:
	if (device_config_needed == NOTHING)
	for (; container < dev->maximum_num_containers; ++container) {
#endif
#endif
	
		if ((dev->fsa_dev[container].config_waiting_on == 0) &&
			(dev->fsa_dev[container].config_needed != NOTHING) &&
			time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT)) {
			device_config_needed =
				dev->fsa_dev[container].config_needed;
			dev->fsa_dev[container].config_needed = NOTHING;
			channel = CONTAINER_TO_CHANNEL(container);
			id = CONTAINER_TO_ID(container);
			lun = CONTAINER_TO_LUN(container);
			break;
		}
	}
	if (device_config_needed == NOTHING)
		return;

	/*
	 *	If we decided that a re-configuration needs to be done,
	 * schedule it here on the way out the door, please close the door
	 * behind you.
	 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))

	busy = 0;

#endif
	adbg_aif(dev,KERN_INFO,"id=(%d,%d,%d,%d)\n", (dev && dev->scsi_host_ptr) ?
	  dev->scsi_host_ptr->host_no : -1,
	  channel, id, lun);

	/*
	 *	Find the scsi_device associated with the SCSI address,
	 * and mark it as changed, invalidating the cache. This deals
	 * with changes to existing device IDs.
	 */

	if (!dev || !dev->scsi_host_ptr)
		return;
	/*
	 * force reload of disk info via aac_probe_container
	 */
	if ((channel == CONTAINER_CHANNEL) &&
	  (device_config_needed != NOTHING)) {
		if (dev->fsa_dev[container].valid == 1)
			dev->fsa_dev[container].valid = 2;
		aac_probe_container(dev, container);
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
	device = scsi_device_lookup(dev->scsi_host_ptr, channel, id, lun);
        adbg_aif(dev,KERN_INFO,"scsi_device_lookup(%p,%d,%d,%d)=%p %s %s\n",
	  dev->scsi_host_ptr, channel, id, lun, device,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
	  ((busy) ? "BUSY" : "AVAILABLE"),
#else
	  "",
#endif
	  (device_config_needed == NOTHING)
	   ? "NOTHING"
	   : (device_config_needed == DELETE)
	     ? "DELETE"
	     : (device_config_needed == ADD)
	       ? "ADD"
	       : (device_config_needed == CHANGE)
		 ? "CHANGE"
		 : "UNKNOWN");
	if (device) {
		switch (device_config_needed) {
		case DELETE:
			if (aac_remove_devnodes > 0) {
				/* Bug in sysfs removing then adding devices quickly */
				scsi_remove_device(device);
			} else {
				if (scsi_device_online(device)) {
					scsi_device_set_state(device, SDEV_OFFLINE);
					sdev_printk(KERN_INFO, device,
						"Device offlined - %s\n",
						(channel == CONTAINER_CHANNEL) ?
							"array deleted" :
							"enclosure services event");
				}
			}
			break;
		case ADD:
			if (!scsi_device_online(device)) {
				sdev_printk(KERN_INFO, device,
					"Device online - %s\n",
					(channel == CONTAINER_CHANNEL) ?
						"array created" :
						"enclosure services event");
				scsi_device_set_state(device, SDEV_RUNNING);
			}
			/* FALLTHRU */
		case CHANGE:
			if ((channel == CONTAINER_CHANNEL)
			 && (!dev->fsa_dev[container].valid)) {
				if (aac_remove_devnodes > 0)
					scsi_remove_device(device);
				else {
					if (!scsi_device_online(device))
						break;
					scsi_device_set_state(device, SDEV_OFFLINE);
					sdev_printk(KERN_INFO, device,
						"Device offlined - %s\n",
						"array failed");
				}
				break;
			}
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) || !defined(MODULE))
			scsi_rescan_device(&device->sdev_gendev);
#else
			if (!device->sdev_gendev.driver)
				break;
			drv = to_scsi_driver(
				device->sdev_gendev.driver);
			if (!try_module_get(drv->owner))
				break;
			/* scsi_rescan_device code fragment */
			if(drv->rescan)
				drv->rescan(&device->sdev_gendev);
			module_put(drv->owner);
#endif

		default:
			break;
		}
		scsi_device_put(device);
		device_config_needed = NOTHING;
	}
#else
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
	        adbg_aif(dev,KERN_INFO, "aifd: device (%d,%d,%d,%d)?\n",
		        dev->scsi_host_ptr->host_no, device->channel, device->id,
		        device->lun);
	        if ((device->channel == channel)
	                && (device->id == id)
	                && (device->lun == lun)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
			busy |= atomic_read(&device->access_count) ||
				test_bit(SHOST_RECOVERY,
				(const unsigned long*)&dev->scsi_host_ptr->shost_state) ||
				dev->scsi_host_ptr->eh_active;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)) || defined(SCSI_HAS_SHOST_STATE_ENUM))
			busy |= device->device_busy ||
				(SHOST_RECOVERY == device->host->shost_state);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
			busy |= device->device_busy ||
				test_bit(SHOST_RECOVERY,
				(const unsigned long*)&dev->scsi_host_ptr->shost_state);
#else
			busy |= device->device_busy ||
				test_bit(SHOST_RECOVERY,
				(const unsigned long*)&dev->scsi_host_ptr->shost_state) ||
				dev->scsi_host_ptr->eh_active;
#endif
#else
			busy |= device->access_count ||
				dev->scsi_host_ptr->in_recovery ||
				dev->scsi_host_ptr->eh_active;
#endif
		    adbg_aif(dev,KERN_INFO, " %s %s\n",
			  ((busy) ? "BUSY" : "AVAILABLE"),
			  (device_config_needed == NOTHING)
			   ? "NOTHING"
			   : (device_config_needed == DELETE)
			     ? "DELETE"
			     : (device_config_needed == ADD)
			       ? "ADD"
			       : (device_config_needed == CHANGE)
				 ? "CHANGE"
				 : "UNKNOWN");
			if (busy == 0) {
				device->removable = 1;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
				switch (device_config_needed) {
#if 0
				case ADD:
					/*
					 *	No need to call
					 * scsi_scan_single_target
					 */
					device_config_needed = CHANGE;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
					adbg_aif(dev,KERN_INFO
					  "scsi_add_device(%p{%d}, %d, %d, %d)\n",
					  dev->scsi_host_ptr,
					  dev->scsi_host_ptr->host_no,
					  device->channel, device->id,
					  device->lun);
					scsi_add_device(dev->scsi_host_ptr,
					  device->channel, device->id,
					  device->lun);
					break;
#endif
#endif
				case DELETE:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
				    adbg_aif(dev,KERN_INFO,
					  "scsi_remove_device(%p{%d:%d:%d:%d})\n",
					  device, device->host->host_no,
					  device->channel, device->id,
					  device->lun);
					scsi_remove_device(device);
					break;
#endif
				case CHANGE:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
					if ((channel == CONTAINER_CHANNEL) &&
					  !dev->fsa_dev[container].valid) {
						adbg_aif(dev,KERN_INFO,
						  "scsi_remove_device(%p{%d:%d:%d:%d})\n",
						  device,
						  device->host->host_no,
						  device->channel, device->id,
						  device->lun);
						scsi_remove_device(device);
						break;
					}
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) || !defined(MODULE))
					adbg_aif(dev,KERN_INFO,
					  "scsi_rescan_device(&%p{%d:%d:%d:%d}->sdev_gendev)\n",
					  device, device->host->host_no,
					  device->channel, device->id,
					  device->lun);
					scsi_rescan_device(&device->sdev_gendev);
#else
					if (!device->sdev_gendev.driver)
						break;
					drv = to_scsi_driver(
						device->sdev_gendev.driver);
					if (!try_module_get(drv->owner))
						break;
					adbg_aif(dev,KERN_INFO,
					  "drv->rescan{%p}(&%p{%d:%d:%d:%d}->sdev_gendev)\n",
					  drv->rescan, device,
					  device->host->host_no,
					  device->channel, device->id,
					  device->lun);
					/* scsi_rescan_device code fragment */
					if(drv->rescan)
						drv->rescan(&device->sdev_gendev);
					module_put(drv->owner);
#endif

				default:
					break;
				}
#endif
			}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
			scsi_device_put(device);
#endif
			break;
		}
	}
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
	if (device_config_needed == ADD)
	{
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
		adbg_aif(dev,KERN_INFO,
		  "scsi_add_device(%p{%d}, %d, %d, %d)\n",
		  dev->scsi_host_ptr, dev->scsi_host_ptr->host_no,
		  channel, id, lun);
		scsi_add_device(dev->scsi_host_ptr, channel, id, lun);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
		adbg_aif(dev,KERN_INFO,
		  "scsi_scan_single_target(%p{%d}, %d, %d)\n",
		  dev->scsi_host_ptr, dev->scsi_host_ptr->host_no,
		  channel, id);
		scsi_scan_single_target(dev->scsi_host_ptr, channel, id);
#elif (!defined(MODULE))
		adbg_aif(dev,KERN_INFO,
		  "scsi_scan_host_selected(%p{%d}, %d, %d, %d, 0)\n",
		  dev->scsi_host_ptr, dev->scsi_host_ptr->host_no,
		  channel, id, lun);
		scsi_scan_host_selected(dev->scsi_host_ptr, channel, id, lun, 0);
#else
		;
#endif
    }
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))
    adbg_aif(dev,KERN_INFO ,"busy=%d\n", busy);
#endif

//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE)))
	/*
	 * if (busy == 0) {
	 *	scan_scsis(dev->scsi_host_ptr, 1,
	 *	  CONTAINER_TO_CHANNEL(container),
	 *	  CONTAINER_TO_ID(container),
	 *	  CONTAINER_TO_LUN(container));
	 * }
	 * is not exported as accessible, so we need to go around it
	 * another way. So, we look for the "proc/scsi/scsi" entry in
	 * the proc filesystem (using proc_scsi as a shortcut) and send
	 * it a message. This deals with new devices that have
	 * appeared. If the device has gone offline, scan_scsis will
	 * also discover this, but we do not want the device to
	 * go away. We need to check the access_count for the
	 * device since we are not wanting the devices to go away.
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if (device_config_needed != NOTHING)
#endif
	if ((channel == CONTAINER_CHANNEL) && busy) {
		dev->fsa_dev[container].config_waiting_on = 0;
		dev->fsa_dev[container].config_needed = device_config_needed;
		/*
		 * Jump back and check if any other containers are ready for
		 * processing.
		 */
		container++;
		device_config_needed = NOTHING;
		goto retry_next_on_busy;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if (device_config_needed != NOTHING)
#endif
	if (proc_scsi != (struct proc_dir_entry *)NULL) {
		struct proc_dir_entry * entry;

		adbg_aif(dev,KERN_INFO, "proc_scsi=%p ", proc_scsi);
		for (entry = proc_scsi->subdir;
		  entry != (struct proc_dir_entry *)NULL;
		  entry = entry->next) {
			adbg_aif(dev,KERN_INFO,"\"%.*s\"[%d]=%x ", entry->namelen,
			  entry->name, entry->namelen, entry->low_ino);
			if ((entry->low_ino != 0)
			 && (entry->namelen == 4)
			 && (memcmp ("scsi", entry->name, 4) == 0)) {
				adbg_aif(dev,KERN_INFO,"%p->write_proc=%p ", entry, entry->write_proc);
				if (entry->write_proc != (int (*)(struct file *, const char *, unsigned long, void *))NULL) {
					char buffer[80];
					int length;
					mm_segment_t fs;

					sprintf (buffer,
					  "scsi %s-single-device %d %d %d %d\n",
					  ((device_config_needed == DELETE)
					   ? "remove"
					   : "add"),
					  dev->scsi_host_ptr->host_no,
					  channel, id, lun);
					length = strlen (buffer);
					adbg_aif(dev,KERN_INFO,
					  "echo %.*s > /proc/scsi/scsi\n",
					  length-1, buffer);
					fs = get_fs();
					set_fs(get_ds());
					lock_kernel();
					length = entry->write_proc(
					  NULL, buffer, length, NULL);
					unlock_kernel();
					set_fs(fs);
				        adbg_aif(dev,KERN_INFO "returns %d\n",
					  length);
				}
				break;
			}
		}
	}
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
	if (channel == CONTAINER_CHANNEL) {
		container++;
		device_config_needed = NOTHING;
		goto retry_next;
	}
#endif
}

static int _aac_reset_adapter(struct aac_dev *aac, int forced)
{
	int index, quirks;
	int retval;
	struct Scsi_Host *host;
	struct scsi_device *dev;
	struct scsi_cmnd *command;
	struct scsi_cmnd *command_list;
	int jafo = 0;

	/*
	 * Assumptions:
	 *	- host is locked, unless called by the aacraid thread.
	 *	  (a matter of convenience, due to legacy issues surrounding
	 *	  eh_host_adapter_reset).
	 *	- in_reset is asserted, so no new i/o is getting to the
	 *	  card.
	 *	- The card is dead, or will be very shortly ;-/ so no new
	 *	  commands are completing in the interrupt service.
	 */
	host = aac->scsi_host_ptr;
	scsi_block_requests(host);
	aac_adapter_disable_int(aac);
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	if (aac->thread_pid != current->pid) {
		kill_proc(aac->thread_pid, SIGKILL, 0);
		/* Chance of sleeping in this context, must unlock */
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_unlock_irq(host->host_lock);
#else
		spin_unlock_irq(host->lock);
#endif
#else
		spin_unlock_irq(&io_request_lock);
#endif
		wait_for_completion(&aac->aif_completion);
		jafo = 1;
	}
#else
	if (aac->thread->pid != current->pid) {
		spin_unlock_irq(host->host_lock);
		kthread_stop(aac->thread);
		jafo = 1;
	}
#endif

	/*
	 *	If a positive health, means in a known DEAD PANIC
	 * state and the adapter could be reset to `try again'.
	 */
	retval = aac_adapter_restart(aac, forced ? 0 : aac_adapter_check_health(aac));
	
	if (retval)
		goto out;
	
	/*
	 *	Loop through the fibs, close the synchronous FIBS
	 */
	for (retval = 1, index = 0; index < (aac->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB); index++) {
		struct fib *fib = &aac->fibs[index];
		if (!(fib->hw_fib_va->header.XferState & cpu_to_le32(NoResponseExpected | Async)) &&
		  (fib->hw_fib_va->header.XferState & cpu_to_le32(ResponseExpected))) {
			unsigned long flagv;
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
			printk(KERN_INFO "returning FIB %p undone\n", fib);
#endif
			spin_lock_irqsave(&fib->event_lock, flagv);
			up(&fib->event_wait);
			spin_unlock_irqrestore(&fib->event_lock, flagv);
			schedule();
			retval = 0;
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
			printk(KERN_INFO "returned FIB %p undone\n", fib);
#endif
		}
	}
	/* Give some extra time for ioctls to complete. */
	if (retval == 0)
		ssleep(2);
	index = aac->cardtype;

	/*
	 * Re-initialize the adapter, first free resources, then carefully
	 * apply the initialization sequence to come back again. Only risk
	 * is a change in Firmware dropping cache, it is assumed the caller
	 * will ensure that i/o is queisced and the card is flushed in that
	 * case.
	 */
	aac_fib_map_free(aac);
	pci_free_consistent(aac->pdev, aac->comm_size, aac->comm_addr, aac->comm_phys);
	aac->comm_addr = NULL;
	aac->comm_phys = 0;
	kfree(aac->queues);
	aac->queues = NULL;
	aac_free_irq(aac);
	kfree(aac->fsa_dev);
	aac->fsa_dev = NULL;
	quirks = aac_get_driver_ident(index)->quirks;
	if (quirks & AAC_QUIRK_31BIT) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
		if (((retval = pci_set_dma_mask(aac->pdev, DMA_31BIT_MASK))) ||
		  ((retval = pci_set_consistent_dma_mask(aac->pdev, DMA_31BIT_MASK))))
#else
		if (((retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(31)))) ||
		  ((retval = pci_set_consistent_dma_mask(aac->pdev, DMA_BIT_MASK(31)))))
#endif
			goto out;
	} else {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
		if (((retval = pci_set_dma_mask(aac->pdev, DMA_32BIT_MASK))) ||
		  ((retval = pci_set_consistent_dma_mask(aac->pdev, DMA_32BIT_MASK))))
#else
		if (((retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(32)))) ||
		  ((retval = pci_set_consistent_dma_mask(aac->pdev, DMA_BIT_MASK(32)))))
#endif
			goto out;
	}
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Calling adapter init\n");
#endif
	if ((retval = (*(aac_get_driver_ident(index)->init))(aac)))
		goto out;
	if (quirks & AAC_QUIRK_31BIT)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
		if ((retval = pci_set_dma_mask(aac->pdev, DMA_32BIT_MASK)))
#else
		if ((retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(32))))
#endif
			goto out;
	if (jafo) {
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
		aac->thread_pid = retval = kernel_thread(
		  (int (*)(void *))aac_command_thread, aac, 0);
		if (retval < 0)
			goto out;
#else
		aac->thread = kthread_run(aac_command_thread, aac, aac->name);
		if (IS_ERR(aac->thread)) {
			retval = PTR_ERR(aac->thread);
			goto out;
		}
#endif
	}
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Acquiring adapter information\n");
#endif
	(void)aac_get_adapter_info(aac);
	if ((quirks & AAC_QUIRK_34SG) && (host->sg_tablesize > 34)) {
		host->sg_tablesize = 34;
		host->max_sectors = (host->sg_tablesize * 8) + 112;
	}
	if ((quirks & AAC_QUIRK_17SG) && (host->sg_tablesize > 17)) {
		host->sg_tablesize = 17;
		host->max_sectors = (host->sg_tablesize * 8) + 112;
	}
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Determine the configuration status\n");
#endif
	aac_get_config_status(aac, 1);
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Probing all arrays to confirm status\n");
#endif
	aac_get_containers(aac);
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Completing all outstanding driver commands as BUSY\n");
#endif
	/*
	 * This is where the assumption that the Adapter is quiesced
	 * is important.
	 */
	command_list = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	__shost_for_each_device(dev, host) {
		unsigned long flags;
		spin_lock_irqsave(&dev->list_lock, flags);
		list_for_each_entry(command, &dev->cmd_list, list)
			if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
				command->SCp.buffer = (struct scatterlist *)command_list;
				command_list = command;
			}
		spin_unlock_irqrestore(&dev->list_lock, flags);
	}
#else
#ifndef SAM_STAT_TASK_SET_FULL
# define SAM_STAT_TASK_SET_FULL (QUEUE_FULL << 1)
#endif
	for (dev = host->host_queue; dev != (struct scsi_device *)NULL; dev = dev->next)
		for(command = dev->device_queue; command; command = command->next)
			if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
				command->SCp.buffer = (struct scatterlist *)command_list;
				command_list = command;
			}
#endif
	while ((command = command_list)) {
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
		printk(KERN_INFO "returning %p for retry\n", command);
#endif
		command_list = (struct scsi_cmnd *)command->SCp.buffer;
		command->SCp.buffer = NULL;
		command->result = DID_OK << 16
		  | COMMAND_COMPLETE << 8
		  | SAM_STAT_TASK_SET_FULL;
		command->SCp.phase = AAC_OWNER_ERROR_HANDLER;
		command->scsi_done(command);
	}
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Continue where we left off\n");
#endif
	retval = 0;

out:
	aac->in_reset = 0;
	scsi_unblock_requests(host);
	if (jafo) {
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_lock_irq(host->host_lock);
#else
		spin_lock_irq(host->lock);
#endif
#else
		spin_lock_irq(&io_request_lock);
#endif
	}
	return retval;
}

int aac_reset_adapter(struct aac_dev * aac, int forced)
{
	unsigned long flagv = 0;
	int retval;
	struct Scsi_Host * host;

	if (spin_trylock_irqsave(&aac->fib_lock, flagv) == 0)
		return -EBUSY;

	if (aac->in_reset) {
		spin_unlock_irqrestore(&aac->fib_lock, flagv);
		return -EBUSY;
	}
	aac->in_reset = 1;
	spin_unlock_irqrestore(&aac->fib_lock, flagv);

	/*
	 * Wait for all commands to complete to this specific
	 * target (block maximum 60 seconds). Although not necessary,
	 * it does make us a good storage citizen.
	 */
	host = aac->scsi_host_ptr;
	scsi_block_requests(host);
	if (forced < 2) for (retval = 60; retval; --retval) {
		struct scsi_device * dev;
		struct scsi_cmnd * command;
		int active = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		__shost_for_each_device(dev, host) {
			spin_lock_irqsave(&dev->list_lock, flagv);
			list_for_each_entry(command, &dev->cmd_list, list) {
				if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
					active++;
					break;
				}
			}
			spin_unlock_irqrestore(&dev->list_lock, flagv);
			if (active)
				break;

		}
#else
		for (dev = host->host_queue; dev != (struct scsi_device *)NULL; dev = dev->next) {
			for(command = dev->device_queue; command; command = command->next) {
				if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
					++active;
					break;
				}
			}
		}
#endif
		/*
		 * We can exit If all the commands are complete
		 */
		if (active == 0)
			break;
		ssleep(1);
	}

	/* Quiesce build, flush cache, write through mode */
	if (forced < 2)
		aac_send_shutdown(aac);
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
	spin_lock_irqsave(host->host_lock, flagv);
#else
	spin_lock_irqsave(host->lock, flagv);
#endif
#else
	spin_lock_irqsave(&io_request_lock, flagv);
#endif
	retval = _aac_reset_adapter(aac, forced ? forced : ((aac_check_reset != 0) && (aac_check_reset != 1)));
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
	spin_unlock_irqrestore(host->host_lock, flagv);
#else
	spin_unlock_irqrestore(host->lock, flagv);
#endif
#else
	spin_unlock_irqrestore(&io_request_lock, flagv);
#endif

	if ((forced < 2) && (retval == -ENODEV)) {
		/* Unwind aac_send_shutdown() IOP_RESET unsupported/disabled */
		struct fib * fibctx = aac_fib_alloc(aac);
		if (fibctx) {
			struct aac_pause *cmd;
			int status;

			aac_fib_init(fibctx);

			cmd = (struct aac_pause *) fib_data(fibctx);

			cmd->command = cpu_to_le32(VM_ContainerConfig);
			cmd->type = cpu_to_le32(CT_PAUSE_IO);
			cmd->timeout = cpu_to_le32(1);
			cmd->min = cpu_to_le32(1);
			cmd->noRescan = cpu_to_le32(1);
			cmd->count = cpu_to_le32(0);

			status = aac_fib_send(ContainerCommand,
			  fibctx,
			  sizeof(struct aac_pause),
			  FsaNormal,
			  -2 /* Timeout silently */, 1,
			  NULL, NULL);

			if (status >= 0)
				aac_fib_complete(fibctx);
			/* FIB should be freed only after getting
			 * the response from the F/W */
			if (status != -ERESTARTSYS)
				aac_fib_free(fibctx);
		}
	}

	return retval;
}

int aac_check_health(struct aac_dev * aac)
{
	int BlinkLED;
#if (!defined(HAS_BOOT_CONFIG))
	unsigned long time_now, flagv = 0;
	struct list_head * entry;
#else
	unsigned long flagv = 0;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))
	struct Scsi_Host * host;
#endif

	/* Extending the scope of fib_lock slightly to protect aac->in_reset */
	if (spin_trylock_irqsave(&aac->fib_lock, flagv) == 0)
		return 0;

	if (aac->in_reset || !(BlinkLED = aac_adapter_check_health(aac))) {
		spin_unlock_irqrestore(&aac->fib_lock, flagv);
		return 0; /* OK */
	}

	aac->in_reset = 1;

#if (!defined(HAS_BOOT_CONFIG))
	/* Fake up an AIF:
	 *	aac_aifcmd.command = AifCmdEventNotify = 1
	 *	aac_aifcmd.seqnum = 0xFFFFFFFF
	 *	aac_aifcmd.data[0] = AifEnExpEvent = 23
	 *	aac_aifcmd.data[1] = AifExeFirmwarePanic = 3
	 *	aac.aifcmd.data[2] = AifHighPriority = 3
	 *	aac.aifcmd.data[3] = BlinkLED
	 */

	time_now = jiffies/HZ;
	entry = aac->fib_list.next;

	/*
	 * For each Context that is on the
	 * fibctxList, make a copy of the
	 * fib, and then set the event to wake up the
	 * thread that is waiting for it.
	 */
	while (entry != &aac->fib_list) {
		/*
		 * Extract the fibctx
		 */
		struct aac_fib_context *fibctx = list_entry(entry, struct aac_fib_context, next);
		struct hw_fib * hw_fib;
		struct fib * fib;
		/*
		 * Check if the queue is getting
		 * backlogged
		 */
		if (fibctx->count > 20) {
			/*
			 * It's *not* jiffies folks,
			 * but jiffies / HZ, so do not
			 * panic ...
			 */
			u32 time_last = fibctx->jiffies;
			/*
			 * Has it been > 2 minutes
			 * since the last read off
			 * the queue?
			 */
			if ((time_now - time_last) > aif_timeout) {
				entry = entry->next;
				aac_close_fib_context(aac, fibctx);
				continue;
			}
		}
		/*
		 * Warning: no sleep allowed while
		 * holding spinlock
		 */
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		hw_fib = kmalloc(sizeof(struct hw_fib), GFP_ATOMIC);
		fib = kmalloc(sizeof(struct fib), GFP_ATOMIC);
#else
		hw_fib = kzalloc(sizeof(struct hw_fib), GFP_ATOMIC);
		fib = kzalloc(sizeof(struct fib), GFP_ATOMIC);
#endif
		if (fib && hw_fib) {
			struct aac_aifcmd * aif;

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
			memset(hw_fib, 0, sizeof(struct hw_fib));
			memset(fib, 0, sizeof(struct fib));
#endif
			fib->hw_fib_va = hw_fib;
			fib->dev = aac;
			aac_fib_init(fib);
			fib->type = FSAFS_NTC_FIB_CONTEXT;
			fib->size = sizeof (struct fib);
			fib->data = hw_fib->data;
			aif = (struct aac_aifcmd *)hw_fib->data;
			aif->command = cpu_to_le32(AifCmdEventNotify);
			aif->seqnum = cpu_to_le32(0xFFFFFFFF);
			((__le32 *)aif->data)[0] = cpu_to_le32(AifEnExpEvent);
			((__le32 *)aif->data)[1] = cpu_to_le32(AifExeFirmwarePanic);
			((__le32 *)aif->data)[2] = cpu_to_le32(AifHighPriority);
			((__le32 *)aif->data)[3] = cpu_to_le32(BlinkLED);

			/*
			 * Put the FIB onto the
			 * fibctx's fibs
			 */
			list_add_tail(&fib->fiblink, &fibctx->fib_list);
			fibctx->count++;
			/*
			 * Set the event to wake up the
			 * thread that will waiting.
			 */
			up(&fibctx->wait_sem);
		} else {
			printk(KERN_WARNING "aifd: didn't allocate NewFib.\n");
			kfree(fib);
			kfree(hw_fib);
		}
		entry = entry->next;
	}
#endif

	spin_unlock_irqrestore(&aac->fib_lock, flagv);

	if (BlinkLED < 0) {
		printk(KERN_ERR "%s: Host adapter dead %d\n", aac->name, BlinkLED);
		goto out;
	}

	printk(KERN_ERR "%s: Host adapter BLINK LED 0x%x\n", aac->name, BlinkLED);

	if (!aac_check_reset || ((aac_check_reset == 1) &&
		(aac->supplement_adapter_info.SupportedOptions2 &
			AAC_OPTION_IGNORE_RESET)))
		goto out;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))
	host = aac->scsi_host_ptr;
	if (aac->thread->pid != current->pid)
		spin_lock_irqsave(host->host_lock, flagv);
	BlinkLED = _aac_reset_adapter(aac, aac_check_reset != 1);
	if (aac->thread->pid != current->pid)
		spin_unlock_irqrestore(host->host_lock, flagv);
	return BlinkLED;
#else
	return _aac_reset_adapter(aac, aac_check_reset != 1);
#endif

out:
	aac->in_reset = 0;
	return BlinkLED;
}

char DecimalToBcd (char decimal)
{
	return (char) ((decimal / 10)*16)+(decimal % 10);
}

/**
 *	aac_command_thread	-	command processing thread
 *	@dev: Adapter to monitor
 *
 *	Waits on the commandready event in it's queue. When the event gets set
 *	it will pull FIBs off it's queue. It will continue to pull FIBs off
 *	until the queue is empty. When the queue is empty it will wait for
 *	more FIBs.
 */

#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
int aac_command_thread(struct aac_dev * dev)
#else
int aac_command_thread(void *data)
#endif
{
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) || defined(HAS_KTHREAD))
	struct aac_dev *dev = data;
#endif
#if (!defined(HAS_BOOT_CONFIG))
	struct hw_fib *hw_fib, *hw_newfib;
	struct fib *fib, *newfib;
	struct aac_fib_context *fibctx;
#else
	struct hw_fib *hw_fib;
	struct fib *fib;
#endif
	unsigned long flags;
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
	DECLARE_WAITQUEUE(wait, current);
#endif
	unsigned long next_jiffies = jiffies + HZ;
	unsigned long next_check_jiffies = next_jiffies;
	long difference = HZ;

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	if (dev->comm_interface == AAC_COMM_APRE)
		return 0;
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_ERR "update_interval=%d:%02d check_interval=%ds\n",
	  update_interval / 60, update_interval % 60, check_interval);
#endif
	/*
	 *	We can only have one thread per adapter for AIF's.
	 */
	if (dev->aif_thread)
		return -EINVAL;
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	/*
	 *	Set up the name that will appear in 'ps'
	 *	stored in  task_struct.comm[16].
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	daemonize(dev->name);
	allow_signal(SIGKILL);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	snprintf(current->comm, sizeof(current->comm), dev->name);
	daemonize();
#else
	sprintf(current->comm, dev->name);
	daemonize();
#endif
#endif
#else

#endif
	/*
	 *	Let the DPC know it has a place to send the AIF's to.
	 */
	dev->aif_thread = 1;
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
	add_wait_queue(&dev->queues->queue[HostNormCmdQueue].cmdready, &wait);
#endif
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	// warning: value computed is not used
#endif
	set_current_state(TASK_INTERRUPTIBLE);
	dprintk ((KERN_INFO "aac_command_thread start\n"));
	while (1) {
#if (!defined(CONFIG_COMMUNITY_KERNEL))
		fwprintf((NULL, HBA_FLAGS_DBG_FW_PRINT_B, ""));
#endif
		spin_lock_irqsave(dev->queues->queue[HostNormCmdQueue].lock, flags);
		while(!list_empty(&(dev->queues->queue[HostNormCmdQueue].cmdq))) {
			struct list_head *entry;
			struct aac_aifcmd * aifcmd;

			set_current_state(TASK_RUNNING);

			entry = dev->queues->queue[HostNormCmdQueue].cmdq.next;
			list_del(entry);

			spin_unlock_irqrestore(dev->queues->queue[HostNormCmdQueue].lock, flags);
			fib = list_entry(entry, struct fib, fiblink);
			hw_fib = fib->hw_fib_va;
			if (dev->sa_firmware) {
				/* Thor AIF */
				aac_handle_sa_aif(dev, fib);
				aac_fib_adapter_complete(fib, (u16)sizeof(u32));
			} else {
			/*
			 *	We will process the FIB here or pass it to a
			 *	worker thread that is TBD. We Really can't
			 *	do anything at this point since we don't have
			 *	anything defined for this thread to do.
			 */
			memset(fib, 0, sizeof(struct fib));
			fib->type = FSAFS_NTC_FIB_CONTEXT;
			fib->size = sizeof(struct fib);
			fib->hw_fib_va = hw_fib;
			fib->data = hw_fib->data;
			fib->dev = dev;
			/*
			 *	We only handle AifRequest fibs from the adapter.
			 */
			 
			 aifcmd = (struct aac_aifcmd *) hw_fib->data;
			 if (aifcmd->command==cpu_to_le32(AifCmdDriverNotify)) {
				/* Handle Driver Notify Events */
				aac_handle_aif(dev, fib);
				*(__le32 *)hw_fib->data = cpu_to_le32(ST_OK);
				aac_fib_adapter_complete(fib, (u16)sizeof(u32));
			 } else {
#if (!defined(HAS_BOOT_CONFIG))
//				struct list_head *entry;
				/* The u32 here is important and intended. We are using
				   32bit wrapping time to fit the adapter field */

				u32 time_now, time_last;
				unsigned long flagv;
				unsigned num;
				struct hw_fib ** hw_fib_pool, ** hw_fib_p;
				struct fib ** fib_pool, ** fib_p;
#endif

				/* Sniff events */
				if ((aifcmd->command ==
				     cpu_to_le32(AifCmdEventNotify)) ||
				    (aifcmd->command ==
				     cpu_to_le32(AifCmdJobProgress))) {
					aac_handle_aif(dev, fib);
				}

#if (!defined(HAS_BOOT_CONFIG))
				time_now = jiffies/HZ;

				/*
				 * Warning: no sleep allowed while
				 * holding spinlock. We take the estimate
				 * and pre-allocate a set of fibs outside the
				 * lock.
				 */
				num = le32_to_cpu(dev->init->r7.AdapterFibsSize)
				    / sizeof(struct hw_fib); /* some extra */
				spin_lock_irqsave(&dev->fib_lock, flagv);
				entry = dev->fib_list.next;
				while (entry != &dev->fib_list) {
					entry = entry->next;
					++num;
				}
				spin_unlock_irqrestore(&dev->fib_lock, flagv);
				hw_fib_pool = NULL;
				fib_pool = NULL;
				if (num
				 && ((hw_fib_pool = kmalloc(sizeof(struct hw_fib *) * num, GFP_KERNEL)))
				 && ((fib_pool = kmalloc(sizeof(struct fib *) * num, GFP_KERNEL)))) {
					hw_fib_p = hw_fib_pool;
					fib_p = fib_pool;
					while (hw_fib_p < &hw_fib_pool[num]) {
						if (!(*(hw_fib_p++) = kmalloc(sizeof(struct hw_fib), GFP_KERNEL))) {
							--hw_fib_p;
							break;
						}
						if (!(*(fib_p++) = kmalloc(sizeof(struct fib), GFP_KERNEL))) {
							kfree(*(--hw_fib_p));
							break;
						}
					}
					if ((num = hw_fib_p - hw_fib_pool) == 0) {
						kfree(fib_pool);
						fib_pool = NULL;
						kfree(hw_fib_pool);
						hw_fib_pool = NULL;
					}
				} else {
					kfree(hw_fib_pool);
					hw_fib_pool = NULL;
				}
				spin_lock_irqsave(&dev->fib_lock, flagv);
				entry = dev->fib_list.next;
				/*
				 * For each Context that is on the
				 * fibctxList, make a copy of the
				 * fib, and then set the event to wake up the
				 * thread that is waiting for it.
				 */
				hw_fib_p = hw_fib_pool;
				fib_p = fib_pool;
				while (entry != &dev->fib_list) {
					/*
					 * Extract the fibctx
					 */
					fibctx = list_entry(entry, struct aac_fib_context, next);
					/*
					 * Check if the queue is getting
					 * backlogged
					 */
					if (fibctx->count > 20)
					{
						/*
						 * It's *not* jiffies folks,
						 * but jiffies / HZ so do not
						 * panic ...
						 */
						time_last = fibctx->jiffies;
						/*
						 * Has it been > 2 minutes
						 * since the last read off
						 * the queue?
						 */
						if ((time_now - time_last) > aif_timeout) {
							entry = entry->next;
							aac_close_fib_context(dev, fibctx);
							continue;
						}
					}
					/*
					 * Warning: no sleep allowed while
					 * holding spinlock
					 */
					if (hw_fib_p < &hw_fib_pool[num]) {
						hw_newfib = *hw_fib_p;
						*(hw_fib_p++) = NULL;
						newfib = *fib_p;
						*(fib_p++) = NULL;
						/*
						 * Make the copy of the FIB
						 */
						memcpy(hw_newfib, hw_fib, sizeof(struct hw_fib));
						memcpy(newfib, fib, sizeof(struct fib));
						newfib->hw_fib_va = hw_newfib;
						/*
						 * Put the FIB onto the
						 * fibctx's fibs
						 */
						list_add_tail(&newfib->fiblink, &fibctx->fib_list);
						fibctx->count++;
						/*
						 * Set the event to wake up the
						 * thread that is waiting.
						 */
						up(&fibctx->wait_sem);
					} else {
						printk(KERN_WARNING "aifd: didn't allocate NewFib.\n");
					}
					entry = entry->next;
				}
#endif
				/*
				 *	Set the status of this FIB
				 */
				*(__le32 *)hw_fib->data = cpu_to_le32(ST_OK);
				aac_fib_adapter_complete(fib, sizeof(u32));
#if (!defined(HAS_BOOT_CONFIG))
				spin_unlock_irqrestore(&dev->fib_lock, flagv);
				/* Free up the remaining resources */
				hw_fib_p = hw_fib_pool;
				fib_p = fib_pool;
				while (hw_fib_p < &hw_fib_pool[num]) {
					kfree(*hw_fib_p);
					kfree(*fib_p);
					++fib_p;
					++hw_fib_p;
				}
				kfree(hw_fib_pool);
				kfree(fib_pool);
#endif
			 }
			}
			kfree(fib);
			spin_lock_irqsave(dev->queues->queue[HostNormCmdQueue].lock, flags);
		}
		/*
		 *	There are no more AIF's
		 */
		spin_unlock_irqrestore(dev->queues->queue[HostNormCmdQueue].lock, flags);

#if (!defined(CONFIG_COMMUNITY_KERNEL))
		fwprintf((NULL, HBA_FLAGS_DBG_FW_PRINT_B, ""));
#endif
		/*
		 *	Background activity
		 */
		if ((time_before(next_check_jiffies,next_jiffies))
		 && ((difference = next_check_jiffies - jiffies) <= 0)) {
			next_check_jiffies = next_jiffies;
			if (aac_check_health(dev) == 0) {
				difference = ((long)(unsigned)check_interval)
					   * HZ;
#if (!defined(CONFIG_COMMUNITY_KERNEL))
				fwprintf((NULL, HBA_FLAGS_DBG_FW_PRINT_B, ""));
#endif
				next_check_jiffies = jiffies + difference;
			} else if (!dev->queues)
				break;
		}
		if (!time_before(next_check_jiffies,next_jiffies)
		 && ((difference = next_jiffies - jiffies) <= 0)) {
		 struct timeval now;
		 int ret;

		 /* Don't even try to talk to adapter if its sick */
		 ret = aac_check_health(dev);
		 if (!ret && !dev->queues)
		  break;
#if (!defined(CONFIG_COMMUNITY_KERNEL))
		 fwprintf((NULL, HBA_FLAGS_DBG_FW_PRINT_B, ""));
#endif
		 next_check_jiffies = jiffies
		  + ((long)(unsigned)check_interval) * HZ;
		 do_gettimeofday(&now);

		 /* Synchronize our watches */
		 if (((1000000 - (1000000 / HZ)) > now.tv_usec)
		  && (now.tv_usec > (1000000 / HZ)))
		  difference = (((1000000 - now.tv_usec) * HZ)
		   + 500000) / 1000000;
		 else if (ret == 0) {
		  struct fib *fibptr;

		  if (now.tv_usec > 500000)
		   ++now.tv_sec;

		  if ((fibptr = aac_fib_alloc(dev))) {
		   aac_fib_init(fibptr);

		   if (dev->sa_firmware) {
		    struct aac_srb *srbcmd;
		    struct sgmap64 *sg64;
		    dma_addr_t addr;
		    struct tm cur_tm; 	
		    char wellness_str[] = 
		     "<HW>TD\010\0\0\0\0\0\0\0\0\0DW\0\0ZZ";
		    u32 datasize = sizeof(wellness_str);	
		    char *dma_buf;
#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
		    unsigned long local_time;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
		    extern struct timezone sys_tz;
#endif

#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
		    local_time = (u32)(now.tv_sec - (sys_tz.tz_minuteswest * 60));
		    time_to_tm(local_time, 0, &cur_tm);
#else
		    time_to_tm(now.tv_sec, 0, &cur_tm);
#endif
		    cur_tm.tm_mon += 1;
		    cur_tm.tm_year += 1900;
		    wellness_str[8] = DecimalToBcd((char)cur_tm.tm_hour);
		    wellness_str[9] = DecimalToBcd((char)cur_tm.tm_min);
		    wellness_str[10] = DecimalToBcd((char)cur_tm.tm_sec);
		    wellness_str[12] = DecimalToBcd((char)cur_tm.tm_mon);
		    wellness_str[13] = DecimalToBcd((char)cur_tm.tm_mday);
		    wellness_str[14] = DecimalToBcd((char)(cur_tm.tm_year/100));
		    wellness_str[15] = DecimalToBcd((char)(cur_tm.tm_year%100));
			
		    dma_buf = pci_alloc_consistent(dev->pdev, datasize, &addr);
		    if (dma_buf != NULL) {
		     u32 vbus, vid;
		     vbus = (u32)le16_to_cpu(
		      dev->supplement_adapter_info.VirtDeviceBus);
		     vid = (u32)le16_to_cpu(
		      dev->supplement_adapter_info.VirtDeviceTarget);

		     srbcmd = (struct aac_srb *) fib_data(fibptr);

		     srbcmd->function = cpu_to_le32(SRBF_ExecuteScsi);
		     srbcmd->channel  = cpu_to_le32(vbus);
		     srbcmd->id       = cpu_to_le32(vid);
		     srbcmd->lun      = 0;
		     srbcmd->flags    = cpu_to_le32(SRB_DataOut);
		     srbcmd->timeout  = cpu_to_le32(10);
		     srbcmd->retry_limit = 0;
		     srbcmd->cdb_size = cpu_to_le32(12);
		     srbcmd->count = cpu_to_le32(datasize);
				
		     memset(srbcmd->cdb, 0, sizeof(srbcmd->cdb));
		     srbcmd->cdb[0] = 0x27;
		     srbcmd->cdb[6] = WRITE_HOST_WELLNESS;
		     memcpy(dma_buf, (char *)wellness_str, datasize);
	
	 	     sg64 = (struct sgmap64 *)&srbcmd->sg;
		     sg64->count = cpu_to_le32(1);
		     sg64->sg[0].addr[1] = cpu_to_le32((u32)(((addr) >> 16) >> 16));
		     sg64->sg[0].addr[0] = cpu_to_le32((u32)(addr& 0xffffffff));
		     sg64->sg[0].count = cpu_to_le32(datasize);

		     ret = aac_fib_send(ScsiPortCommand64, fibptr,
		      sizeof(struct aac_srb), FsaNormal, 1, 1, NULL, NULL);
		    }
		    pci_free_consistent(dev->pdev, datasize, 
		     (void *)dma_buf, addr);

		   } else {
		    __le32 *info = (__le32 *)fib_data(fibptr);
		    *info = cpu_to_le32(now.tv_sec);

		    ret = aac_fib_send(SendHostTime, fibptr,
		     sizeof(*info), FsaNormal, 1, 1, NULL, NULL);
		   }

		   /* Do not set XferState to zero unless
		    * receives a response from F/W */
		   if (ret >= 0)
		    aac_fib_complete(fibptr);

		   /* FIB should be freed only after
		    * getting the response from the F/W */
		   if (ret != -ERESTARTSYS)
		    aac_fib_free(fibptr);
		  }
		  difference = (long)(unsigned)update_interval*HZ;
	 	 } else {
		  /* retry shortly */
		  difference = 10 * HZ;
		 }
		 next_jiffies = jiffies + difference;
		 if (time_before(next_check_jiffies,next_jiffies))
		  difference = next_check_jiffies - jiffies;
		}
		if (difference <= 0)
			difference = 1;
#if (!defined(CONFIG_COMMUNITY_KERNEL))
		if (nblank(fwprintf(x)) && (difference > HZ))
			difference = HZ;
#endif
		set_current_state(TASK_INTERRUPTIBLE);
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
		if (dev->thread_die)
			break;

		down(&dev->queues->queue[HostNormCmdQueue].cmdready);
#else
		schedule_timeout(difference);

#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
		if(signal_pending(current))
#else
		if (kthread_should_stop())
#endif
			break;
#endif
	}
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
	if (dev->queues)
		remove_wait_queue(&dev->queues->queue[HostNormCmdQueue].cmdready, &wait);
#endif
	dev->aif_thread = 0;
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	complete_and_exit(&dev->aif_completion, 0);
#endif
	return 0;
}

int aac_acquire_irq(struct aac_dev *dev)
{
	
	int i;
	int j;
	int ret = 0;
#if ((defined(RHEL_MAJOR) && RHEL_MAJOR == 6 && RHEL_MINOR >= 2)\
	  || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)))
	int cpu;
	cpu = cpumask_first(cpu_online_mask);
#endif
	if (!dev->sync_mode && dev->msi_enabled && dev->max_msix > 1) {
		for (i = 0; i < dev->max_msix; i++) {
			dev->aac_msix[i].vector_no = i;
			dev->aac_msix[i].dev = dev;
			if (request_irq(dev->msixentry[i].vector, dev->a_ops.adapter_intr,
					0,"aacraid", &(dev->aac_msix[i]))) {
				printk(KERN_ERR "%s%d: Failed to register IRQ for vector %d.\n",
						dev->name, dev->id, i);
				for (j = 0 ; j < i ; j++)
					free_irq(dev->msixentry[j].vector,
						 &(dev->aac_msix[j]));
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_DISABLE_MSI))
				pci_disable_msix(dev->pdev);
#endif
				ret = -1;
			}
#if ((defined(RHEL_MAJOR) && RHEL_MAJOR == 6 && RHEL_MINOR >= 2)\
	  || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)))
                        dev->msix_vector_tbl[cpu] = i;
                        if(irq_set_affinity_hint(dev->msixentry[i].vector, get_cpu_mask(cpu))) {
				printk(KERN_ERR "%s%d: Failed to set IRQ affinity for cpu %d\n",
					    dev->name, dev->id, cpu);
			}
			cpu = cpumask_next(cpu, cpu_online_mask);
#endif
		}
	} else {
		dev->aac_msix[0].vector_no = 0;
		dev->aac_msix[0].dev = dev;

		if (request_irq(dev->pdev->irq, dev->a_ops.adapter_intr,
			IRQF_SHARED|IRQF_DISABLED, "aacraid", &(dev->aac_msix[0])) < 0) {
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_DISABLE_MSI))
			if (dev->msi)
				pci_disable_msi(dev->pdev);
#endif
			printk(KERN_ERR "%s%d: Interrupt unavailable.\n",
					dev->name, dev->id);
			ret = -1;
		}
	}
	return ret;
}

void aac_free_irq(struct aac_dev *dev)
{
	int i;
#if ((defined(RHEL_MAJOR) && RHEL_MAJOR == 6 && RHEL_MINOR >= 2)\
	  || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)))
	int cpu;
	cpu = cpumask_first(cpu_online_mask);
#endif
	if (dev->pdev->device == PMC_DEVICE_S6 ||
	    dev->pdev->device == PMC_DEVICE_S7 ||
	    dev->pdev->device == PMC_DEVICE_S8 ||
	    dev->pdev->device == PMC_DEVICE_S9) {
		if (dev->max_msix > 1) {
			for (i = 0; i < dev->max_msix; i++){
#if ((defined(RHEL_MAJOR) && RHEL_MAJOR == 6 && RHEL_MINOR >= 2)\
	  || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)))
				if(irq_set_affinity_hint(dev->msixentry[i].vector, NULL)) {
				printk(KERN_ERR "%s%d: Failed to reset IRQ affinity for cpu %d\n",
					    dev->name, dev->id, cpu);
				}
				cpu = cpumask_next(cpu, cpu_online_mask);
#endif
				free_irq(dev->msixentry[i].vector, &(dev->aac_msix[i]));
			}
		} else {
			free_irq(dev->pdev->irq, &(dev->aac_msix[0]));
		}
	} else {
		free_irq(dev->pdev->irq, dev);
	}
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_DISABLE_MSI))
	if (dev->msi)
		pci_disable_msi(dev->pdev);
	else if (dev->max_msix > 1)
		pci_disable_msix(dev->pdev);
#endif
}
