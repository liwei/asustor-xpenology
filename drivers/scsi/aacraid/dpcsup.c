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
 *  dpcsup.c
 *
 * Abstract: All DPC processing routines occur here.
 *
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
//#include <linux/sched.h>
//#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/blkdev.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif

#include "aacraid.h"
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#include "fwdebug.h"
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#include <scsi/scsi_host.h>
#else
#include "scsi.h"
#include "hosts.h"
#endif
#endif

/**
 *	aac_response_normal	-	Handle command replies
 *	@q: Queue to read from
 *
 *	This DPC routine will be run when the adapter interrupts us to let us
 *	know there is a response on our normal priority queue. We will pull off
 *	all QE there are and wake up all the waiters before exiting. We will
 *	take a spinlock out on the queue before operating on it.
 */

unsigned int aac_response_normal(struct aac_queue * q)
{
	struct aac_dev * dev = q->dev;
	struct aac_entry *entry;
	struct hw_fib * hwfib;
	struct fib * fib;
	int consumed = 0;
	unsigned long flags, mflags;

	spin_lock_irqsave(q->lock, flags);	
	/*
	 *	Keep pulling response QEs off the response queue and waking
	 *	up the waiters until there are no more QEs. We then return
	 *	back to the system. If no response was requesed we just
	 *	deallocate the Fib here and continue.
	 */
	while(aac_consumer_get(dev, q, &entry))
	{
		int fast;
		u32 index = le32_to_cpu(entry->addr);
		fast = index & 0x01;
		fib = &dev->fibs[index >> 2];
		hwfib = fib->hw_fib_va;
		
		aac_consumer_free(dev, q, HostNormRespQueue);
		/*
		 *	Remove this fib from the Outstanding I/O queue.
		 *	But only if it has not already been timed out.
		 *
		 *	If the fib has been timed out already, then just 
		 *	continue. The caller has already been notified that
		 *	the fib timed out.
		 */
		atomic_dec(&dev->queues->queue[AdapNormCmdQueue].numpending);

		if (unlikely(fib->flags & FIB_CONTEXT_FLAG_TIMED_OUT)) {
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
			printk(KERN_WARNING "aacraid: FIB timeout (%x).\n", fib->flags);
			printk(KERN_DEBUG"aacraid: hwfib=%p fib index=%i fib=%p\n",hwfib, hwfib->header.Handle,fib);
#endif
			spin_unlock_irqrestore(q->lock, flags);
			aac_fib_complete(fib);
			aac_fib_free(fib);
			spin_lock_irqsave(q->lock, flags);
			continue;
		}
		spin_unlock_irqrestore(q->lock, flags);

		if (fast) {
			/*
			 *	Doctor the fib
			 */
			*(__le32 *)hwfib->data = cpu_to_le32(ST_OK);
			hwfib->header.XferState |= cpu_to_le32(AdapterProcessed);
			fib->flags |= FIB_CONTEXT_FLAG_FASTRESP;
		}

		FIB_COUNTER_INCREMENT(aac_config.FibRecved);

		if (hwfib->header.Command == cpu_to_le16(NuFileSystem))
		{
			__le32 *pstatus = (__le32 *)hwfib->data;
			if (*pstatus & cpu_to_le32(0xffff0000))
				*pstatus = cpu_to_le32(ST_OK);
		}
		if (hwfib->header.XferState & cpu_to_le32(NoResponseExpected | Async)) 
		{
	        	if (hwfib->header.XferState & cpu_to_le32(NoResponseExpected))
				FIB_COUNTER_INCREMENT(aac_config.NoResponseRecved);
			else 
				FIB_COUNTER_INCREMENT(aac_config.AsyncRecved);
			/*
			 *	NOTE:  we cannot touch the fib after this
			 *	    call, because it may have been deallocated.
			 */
			fib->callback(fib->callback_data, fib);
		} else {
			unsigned long flagv;
			int complete = 0;
			spin_lock_irqsave(&fib->event_lock, flagv);
			if (fib->done == 2) {
				dprintk((KERN_INFO "complete fib with pending wait status\n"));
				fib->done = 1;
				complete = 1;
			} else {
				fib->done = 1;
				up(&fib->event_wait);
			}
			spin_unlock_irqrestore(&fib->event_lock, flagv);

			spin_lock_irqsave(&dev->manage_lock, mflags);
			dev->management_fib_count--;
			spin_unlock_irqrestore(&dev->manage_lock, mflags);

			FIB_COUNTER_INCREMENT(aac_config.NormalRecved);
			if (complete) {
				aac_fib_complete(fib);
				aac_fib_free(fib);
			}
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x8)
			if ((hwfib->header.XferState &
			  cpu_to_le32(ResponseExpected)) &&
			  !fib->callback && !fib->callback_data &&
			  !list_empty(&dev->entry)) {
				adbg_ioctl(dev,KERN_INFO,
				  "aac_send_fib(%u,%p,%u,?,?,1,NULL,NULL) wakeup done=%d\n",
				  le16_to_cpu(hwfib->header.Command),
				  fib, (unsigned)(
				  le16_to_cpu(hwfib->header.Size) -
				  sizeof(struct aac_fibhdr)), fib->done);
				fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  "aac_send_fib(%u,%p,%u,?,?,1,NULL,NULL) wakeup done=%d",
				  le16_to_cpu(hwfib->header.Command),
				  fib, (unsigned)(
				  le16_to_cpu(hwfib->header.Size) -
				  sizeof(struct aac_fibhdr)), fib->done));
			}
#endif
#endif
		}
		consumed++;
		spin_lock_irqsave(q->lock, flags);
	}

	if (consumed > aac_config.peak_fibs)
	{
		aac_config.peak_fibs = consumed;
		adbg_conf(dev, KERN_INFO, "peak_fibs=%d\n", aac_config.peak_fibs);
	}
	if (consumed == 0) 
		aac_config.zero_fibs++;

	spin_unlock_irqrestore(q->lock, flags);
	return 0;
}


/**
 *	aac_command_normal	-	handle commands
 *	@q: queue to process
 *
 *	This DPC routine will be queued when the adapter interrupts us to 
 *	let us know there is a command on our normal priority queue. We will 
 *	pull off all QE there are and wake up all the waiters before exiting.
 *	We will take a spinlock out on the queue before operating on it.
 */
 
unsigned int aac_command_normal(struct aac_queue *q)
{
	struct aac_dev * dev = q->dev;
	struct aac_entry *entry;
	unsigned long flags;

	spin_lock_irqsave(q->lock, flags);

	/*
	 *	Keep pulling response QEs off the response queue and waking
	 *	up the waiters until there are no more QEs. We then return
	 *	back to the system.
	 */
	while(aac_consumer_get(dev, q, &entry))
	{
		struct fib fibctx;
		struct hw_fib * hw_fib;
		u32 index;
		struct fib *fib = &fibctx;
		
		index = le32_to_cpu(entry->addr) / sizeof(struct hw_fib);
#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
		printk(KERN_INFO "index=%d or %d\n", index,
		  le32_to_cpu(entry->addr / sizeof(struct hw_fib)));
#endif
		hw_fib = &dev->aif_base_va[index];
		
		/*
		 *	Allocate a FIB at all costs. For non queued stuff
		 *	we can just use the stack so we are happy. We need
		 *	a fib object in order to manage the linked lists
		 */
		if (dev->aif_thread)
			if((fib = kmalloc(sizeof(struct fib), GFP_ATOMIC)) == NULL)
				fib = &fibctx;
		
		memset(fib, 0, sizeof(struct fib));
		INIT_LIST_HEAD(&fib->fiblink);
		fib->type = FSAFS_NTC_FIB_CONTEXT;
		fib->size = sizeof(struct fib);
		fib->hw_fib_va = hw_fib;
		fib->data = hw_fib->data;
		fib->dev = dev;
		
				
		if (dev->aif_thread && fib != &fibctx) {
		        list_add_tail(&fib->fiblink, &q->cmdq);
	 	        aac_consumer_free(dev, q, HostNormCmdQueue);
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
			up(&q->cmdready);
#else
		        wake_up_interruptible(&q->cmdready);
#endif
		} else {
	 	        aac_consumer_free(dev, q, HostNormCmdQueue);
			spin_unlock_irqrestore(q->lock, flags);
			/*
			 *	Set the status of this FIB
			 */
			*(__le32 *)hw_fib->data = cpu_to_le32(ST_OK);
			aac_fib_adapter_complete(fib, sizeof(u32));
			spin_lock_irqsave(q->lock, flags);
		}		
	}
	spin_unlock_irqrestore(q->lock, flags);
	return 0;
}


/**
 *
 * aac_aif_callback
 * @context: the context set in the fib - here it is scsi cmd
 * @fibptr: pointer to the fib
 *
 * Handles the AIFs - new method (SRC)
 *
 */

static void aac_aif_callback(void *context, struct fib * fibptr)
{
	struct fib * fibctx;
	struct aac_dev *dev;
	struct aac_aifcmd *cmd;
	int status;

	fibctx = (struct fib *)context;
	BUG_ON(fibptr == NULL);
	dev = fibptr->dev;

	if (fibptr->hw_fib_va->header.XferState & 
		cpu_to_le32(NoMoreAifDataAvailable)) {
		aac_fib_complete(fibptr);
		aac_fib_free(fibptr);
		return;
	}

	aac_intr_normal(dev, 0, 1, 0, fibptr->hw_fib_va);

	aac_fib_init(fibctx);
	cmd = (struct aac_aifcmd *) fib_data(fibctx);
	cmd->command = cpu_to_le32(AifReqEvent);

	status = aac_fib_send(AifRequest,
		fibctx,
		sizeof(struct hw_fib)-sizeof(struct aac_fibhdr),
		FsaNormal,
		0, 1,
		(fib_callback)aac_aif_callback, fibctx);
}

/**
 *	aac_intr_normal	-	Handle command replies
 *	@dev: Device
 *	@index: completion reference
 *
 *	This DPC routine will be run when the adapter interrupts us to let us
 *	know there is a response on our normal priority queue. We will pull off
 *	all QE there are and wake up all the waiters before exiting.
 */

unsigned int aac_intr_normal(struct aac_dev * dev, u32 index, int isAif,
	int isFastResponse, struct hw_fib *aif_fib)
{
	unsigned long mflags;
	dprintk((KERN_INFO "aac_intr_normal(%p,%x)\n", dev, index));
	if (isAif == 1) {		/* AIF - common */
		struct hw_fib * hw_fib;
		struct fib * fib;
		struct aac_queue *q = &dev->queues->queue[HostNormCmdQueue];
		unsigned long flags;

		/*
		 *	Allocate a FIB. For non queued stuff we can just use
		 * the stack so we are happy. We need a fib object in order to
		 * manage the linked lists.
		 */
		if ((!dev->aif_thread)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		 || (!(fib = kmalloc(sizeof(struct fib),GFP_ATOMIC))))
#else
		 || (!(fib = kzalloc(sizeof(struct fib),GFP_ATOMIC))))
#endif
			return 1;
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		if (!(hw_fib = kmalloc(sizeof(struct hw_fib),GFP_ATOMIC))) {
#else
		if (!(hw_fib = kzalloc(sizeof(struct hw_fib),GFP_ATOMIC))) {
#endif
			kfree (fib);
			return 1;
		}
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		memset(hw_fib, 0, sizeof(struct hw_fib));
#endif
		if (dev->sa_firmware) {
			fib->hbacmd_size = index;	/* store event type */
		} else if (aif_fib != NULL) {
			memcpy(hw_fib, aif_fib, sizeof(struct hw_fib));
		} else {
			memcpy(hw_fib, (struct hw_fib *)(((uintptr_t)(dev->regs.sa)) +
				index), sizeof(struct hw_fib));
		}
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		memset(fib, 0, sizeof(struct fib));
#endif
		INIT_LIST_HEAD(&fib->fiblink);
		fib->type = FSAFS_NTC_FIB_CONTEXT;
		fib->size = sizeof(struct fib);
		fib->hw_fib_va = hw_fib;
		fib->data = hw_fib->data;
		fib->dev = dev;
	
		spin_lock_irqsave(q->lock, flags);
		list_add_tail(&fib->fiblink, &q->cmdq);
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
		up(&q->cmdready);
#else
	        wake_up_interruptible(&q->cmdready);
#endif
		spin_unlock_irqrestore(q->lock, flags);
		return 1;
	} else if (isAif == 2) {		/* AIF - new (SRC) */
		struct fib * fibctx;
		struct aac_aifcmd *cmd;

		fibctx = aac_fib_alloc(dev);
		if (!fibctx)
			return 1;
		aac_fib_init(fibctx);

		cmd = (struct aac_aifcmd *) fib_data(fibctx);
		cmd->command = cpu_to_le32(AifReqEvent);

		return aac_fib_send(AifRequest,
			fibctx,
			sizeof(struct hw_fib)-sizeof(struct aac_fibhdr),
			FsaNormal,
			0, 1,
			(fib_callback)aac_aif_callback, fibctx);
	} else {
		struct fib * fib = &dev->fibs[index];
		int start_callback = 0;

#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
		BUG_ON(index >= (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB));
		BUG_ON(fib->type != FSAFS_NTC_FIB_CONTEXT);
#endif
		/*
		 *	Remove this fib from the Outstanding I/O queue.
		 *	But only if it has not already been timed out.
		 *
		 *	If the fib has been timed out already, then just 
		 *	continue. The caller has already been notified that
		 *	the fib timed out.
		 */
		atomic_dec(&dev->queues->queue[AdapNormCmdQueue].numpending);

		if (unlikely(fib->flags & FIB_CONTEXT_FLAG_TIMED_OUT)) {
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
			printk(KERN_WARNING "aacraid: FIB timeout (%x).\n", fib->flags);
			printk(KERN_DEBUG
			  "aacraid: hwfib=%p index=%i fib[%d]=%p\n",
			  hwfib, index, fib);
#endif
			aac_fib_complete(fib);
			aac_fib_free(fib);
			return 0;
		}

		FIB_COUNTER_INCREMENT(aac_config.FibRecved);

		if (fib->flags & FIB_CONTEXT_FLAG_NATIVE_HBA) {
			if (dev->simulated_scsi_error) {
				aac_simulate_scsi_error(dev, fib->hw_fib_va);
				dev->simulated_scsi_error = 0;
			} else if (dev->simulated_tgt_failure) {
				aac_simulate_tgt_failure(dev, fib->hw_fib_va);
				dev->simulated_tgt_failure = 0;
			} else if (isFastResponse) {
				fib->flags |= FIB_CONTEXT_FLAG_FASTRESP;
			}
			if (fib->callback) {
				start_callback = 1;
			} else {
				unsigned long flagv;
				int complete = 0;
	  			dprintk((KERN_INFO "event_wait up\n"));
				spin_lock_irqsave(&fib->event_lock, flagv);
				if (fib->done == 2) {
					fib->done = 1;
					complete = 1;
				} else {
					fib->done = 1;
					up(&fib->event_wait);
				}
				spin_unlock_irqrestore(&fib->event_lock, flagv);

				spin_lock_irqsave(&dev->manage_lock, mflags);
				dev->management_fib_count--;
				spin_unlock_irqrestore(&dev->manage_lock, 
					mflags);

				FIB_COUNTER_INCREMENT(aac_config.NativeRecved);
				if (complete) {
					aac_fib_complete(fib);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
					aac_fib_free_tag(fib);
#else
					aac_fib_free(fib);
#endif
				} 
			}
		} else {
			struct hw_fib * hwfib = fib->hw_fib_va;
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
			BUG_ON(hwfib->header.XferState == 0);
#endif
			if (isFastResponse) {
				/* Doctor the fib */
				*(__le32 *)hwfib->data = cpu_to_le32(ST_OK);
				hwfib->header.XferState |= 
					cpu_to_le32(AdapterProcessed);
				fib->flags |= FIB_CONTEXT_FLAG_FASTRESP;
			}

			if (hwfib->header.Command == cpu_to_le16(NuFileSystem))
			{
				__le32 *pstatus = (__le32 *)hwfib->data;
				if (*pstatus & cpu_to_le32(0xffff0000))
					*pstatus = cpu_to_le32(ST_OK);
			}
			if (hwfib->header.XferState & cpu_to_le32(
				NoResponseExpected | Async)) 
			{
	        		if (hwfib->header.XferState & cpu_to_le32(
					NoResponseExpected))
					FIB_COUNTER_INCREMENT(
						aac_config.NoResponseRecved);
				else 
					FIB_COUNTER_INCREMENT(
						aac_config.AsyncRecved);
				start_callback = 1;
			} else {
				unsigned long flagv;
				int complete = 0;
	  			dprintk((KERN_INFO "event_wait up\n"));
				spin_lock_irqsave(&fib->event_lock, flagv);
				if (fib->done == 2) {
					fib->done = 1;
					complete = 1;
				} else {
					fib->done = 1;
					up(&fib->event_wait);
				}
				spin_unlock_irqrestore(&fib->event_lock, flagv);

				spin_lock_irqsave(&dev->manage_lock, mflags);
				dev->management_fib_count--;
				spin_unlock_irqrestore(&dev->manage_lock, 
					mflags);

				FIB_COUNTER_INCREMENT(aac_config.NormalRecved);
				if (complete) {
					aac_fib_complete(fib);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
					aac_fib_free_tag(fib);
#else
					aac_fib_free(fib);
#endif				
				} 
			}
		}

		if (start_callback) {
			/*
		 	 * NOTE:  we cannot touch the fib after this
		 	 *  call, because it may have been deallocated.
		 	 */
#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
			BUG_ON((fib->callback == NULL) || (fib->callback_data == NULL));
			BUG_ON((fib->callback == (void *)(uintptr_t)0x6b6b6b6b6b6b6b6bLL) || (fib->callback_data == (void *)(uintptr_t)0x6b6b6b6b6b6b6b6bLL));
			BUG_ON((fib->callback < (fib_callback)aac_get_driver_ident));
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
			if (likely(fib->callback && fib->callback_data)) {
				fib->callback(fib->callback_data, fib);
			} else
				printk(KERN_INFO
				  "Invalid callback fib[%d>>2] (*%p)(%p)\n",
				  index, fib->callback, fib->callback_data);
#else
			if (likely(fib->callback && fib->callback_data)) {
				fib->callback(fib->callback_data, fib);
			} else {
				aac_fib_complete(fib);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
				aac_fib_free_tag(fib);
#else
				aac_fib_free(fib);
#endif
			}
#endif
		}
	}
	return 0;
}

#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
/**
 *	aac_command_apre	-	Handle command replies
 *	@dev: Device
 *
 *	This DPC routine will be run when the adapter interrupts us to let us
 *	know there is a response on our apre queue. We will pull off
 *	all QE there are and wake up all the waiters before exiting.
 */

unsigned int aac_command_apre(struct aac_dev * dev)
{
	struct aac_queue *q = &dev->queues->queue[ApreCmdQueue];

	for (;;) {
		struct donelist_entry *d;
		unsigned long qflags;
		struct fib * fib;

		spin_lock_irqsave(q->lock, qflags);
		d = q->NxtDoneListEntry;
		if (d->DLIndicator == q->DLIndicator) {
			spin_unlock_irqrestore(q->lock, qflags);
			break;
		}
		fib = &dev->fibs[d->tcNumber >> 2];
		q->Credits += fib->Credits;
		if (++d >= &q->DoneListPool[q->entries]) {
			d = q->DoneListPool;
			q->DLIndicator = (q->DLIndicator == 0);
		}
		q->NxtDoneListEntry = d;
		spin_unlock_irqrestore(q->lock, qflags);

		fib->callback(fib->callback_data, fib);
	}
	return 0;
}
#endif
