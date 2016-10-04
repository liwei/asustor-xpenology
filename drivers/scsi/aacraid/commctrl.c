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
 *  commctrl.c
 *
 * Abstract: Contains all routines for control of the AFA comm layer
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
//#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/version.h> /* for the following test */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#include <linux/dma-mapping.h>
#endif
#include <linux/blkdev.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)) || defined(SCSI_HAS_SSLEEP)
#include <linux/delay.h> /* ssleep prototype */
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) || defined(HAS_KTHREAD))
#include <linux/kthread.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif
#include <asm/uaccess.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#include <scsi/scsi_cmnd.h>
#endif
#include <scsi/scsi_host.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#endif
#else
#include "scsi.h"
#include "hosts.h"
#endif

#include "aacraid.h"
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#include "fwdebug.h"
#endif
#if (defined(AAC_CSMI))
# include "csmi.h"
#endif

#if (!defined(HAS_BOOT_CONFIG))
/**
 *	ioctl_send_fib	-	send a FIB from userspace
 *	@dev:	adapter is being processed
 *	@arg:	arguments to the ioctl call
 *
 *	This routine sends a fib to the adapter on behalf of a user level
 *	program.
 */
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0xF)
static char * aac_debug_timestamp(void)
{
	unsigned long seconds = get_seconds();
	static char buffer[80];
	sprintf(buffer, "%02u:%02u:%02u: ",
	  (int)((seconds / 3600) % 24),
	  (int)((seconds / 60) % 60),
	  (int)(seconds % 60));
	return buffer;
}
# define AAC_DEBUG_PREAMBLE	"%s"
# define AAC_DEBUG_POSTAMBLE	,aac_debug_timestamp()
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) || defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB7))
# define AAC_DEBUG_PREAMBLE_SIZE 10
#endif
#else
# define AAC_DEBUG_PREAMBLE	KERN_INFO
# define AAC_DEBUG_POSTAMBLE
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) || defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB7))
# define AAC_DEBUG_PREAMBLE_SIZE 0
#endif
#endif
#else
# define AAC_DEBUG_PREAMBLE	KERN_INFO
# define AAC_DEBUG_POSTAMBLE
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) || defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB7))
# define AAC_DEBUG_PREAMBLE_SIZE 0
#endif
#endif

static int ioctl_send_fib(struct aac_dev * dev, void __user *arg)
{
	struct hw_fib * kfib;
	struct fib *fibptr;
	struct hw_fib * hw_fib = (struct hw_fib *)0;
	dma_addr_t hw_fib_pa = (dma_addr_t)0LL;
	unsigned size;
	int retval;

	adbg_ioctl(dev,KERN_DEBUG, AAC_DEBUG_PREAMBLE "ioctl_send_fib(%p,%p)\n" AAC_DEBUG_POSTAMBLE,
	  dev, arg);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x2)
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  AAC_DEBUG_PREAMBLE "ioctl_send_fib(%p,%p)" AAC_DEBUG_POSTAMBLE,
	  dev, arg));
#endif
#endif
	if (dev->in_reset) {
		adbg_ioctl(dev, KERN_DEBUG, AAC_DEBUG_PREAMBLE
		  "ioctl_send_fib(%p,%p) returns -EBUSY\n"
		  AAC_DEBUG_POSTAMBLE, dev, arg);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x2)
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  AAC_DEBUG_PREAMBLE "ioctl_send_fib(%p,%p) returns -EBUSY"
		  AAC_DEBUG_POSTAMBLE, dev, arg));
#endif
#endif
		return -EBUSY;
	}
	fibptr = aac_fib_alloc(dev);
	if(fibptr == NULL) {
		adbg_ioctl(dev,KERN_DEBUG, AAC_DEBUG_PREAMBLE
		  "ioctl_send_fib(%p,%p) returns -ENOMEM\n"
		  AAC_DEBUG_POSTAMBLE, dev, arg);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x2)
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  AAC_DEBUG_PREAMBLE "ioctl_send_fib(%p,%p) returns -ENOMEM"
		  AAC_DEBUG_POSTAMBLE, dev, arg));
#endif
#endif
		return -ENOMEM;
	}

	kfib = fibptr->hw_fib_va;
	/*
	 *	First copy in the header so that we can check the size field.
	 */
	if (copy_from_user((void *)kfib, arg, sizeof(struct aac_fibhdr))) {
		aac_fib_free(fibptr);
		adbg_ioctl(dev, KERN_DEBUG, AAC_DEBUG_PREAMBLE
		  "ioctl_send_fib(%p,%p) returns -EFAULT\n"
		  AAC_DEBUG_POSTAMBLE, dev, arg);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x2)
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  AAC_DEBUG_PREAMBLE "ioctl_send_fib(%p,%p) returns -EFAULT"
		  AAC_DEBUG_POSTAMBLE, dev, arg));
#endif
#endif
		return -EFAULT;
	}
	/*
	 *	Since we copy based on the fib header size, make sure that we
	 *	will not overrun the buffer when we copy the memory. Return
	 *	an error if we would.
	 */
	size = le16_to_cpu(kfib->header.Size) + sizeof(struct aac_fibhdr);
	if (size < le16_to_cpu(kfib->header.SenderSize))
		size = le16_to_cpu(kfib->header.SenderSize);
	if (size > dev->max_fib_size) {
		dma_addr_t daddr;

		if (size > 2048) {
			retval = -EINVAL;
			goto cleanup;
		}

		kfib = pci_alloc_consistent(dev->pdev, size, &daddr);
		if (!kfib) {
			retval = -ENOMEM;
			goto cleanup;
		}

		/* Highjack the hw_fib */
		hw_fib = fibptr->hw_fib_va;
		hw_fib_pa = fibptr->hw_fib_pa;
		fibptr->hw_fib_va = kfib;
		fibptr->hw_fib_pa = daddr;
		memset(((char *)kfib) + dev->max_fib_size, 0, size - dev->max_fib_size);
		memcpy(kfib, hw_fib, dev->max_fib_size);
	}
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
	if (kfib->header.XferState & cpu_to_le32(Async | ResponseExpected | NoResponseExpected)) {
		retval = -EINVAL;
		goto cleanup;
	}
#endif

	if (copy_from_user(kfib, arg, size)) {
		retval = -EFAULT;
		goto cleanup;
	}

	if (kfib->header.Command == cpu_to_le16(TakeABreakPt)) {
		aac_adapter_interrupt(dev);
		/*
		 * Since we didn't really send a fib, zero out the state to allow
		 * cleanup code not to assert.
		 */
		kfib->header.XferState = 0;
	} else {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x7)
#define AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB7
#endif
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) || defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB7))
		{
			u8 * fib = (u8 *)kfib;
			unsigned len = le16_to_cpu(kfib->header.Size);
			char buffer[80-AAC_DEBUG_PREAMBLE_SIZE];
			char * cp = buffer;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB7))
			unsigned long DebugFlags;
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x4) == 0)
			unsigned header_length;
			sprintf(cp, "FIB=%p=", fib);
			header_length = strlen(cp);
#else
#			define header_length 4
			strcpy(cp, "FIB=");
#endif
			if (nblank(fwprintf(x))) {
				DebugFlags = dev->FwDebugFlags;
				dev->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
			}
#else
#			define header_length 4
			strcpy(cp, "FIB=");
#endif
			cp += header_length;
			while (len > 0) {
				if (cp >= &buffer[sizeof(buffer)-4]) {
					adbg_ioctl(dev, KERN_DEBUG,
					  AAC_DEBUG_PREAMBLE "%s\n"
					  AAC_DEBUG_POSTAMBLE,
					  buffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x1)
					fwprintf((dev,
					  HBA_FLAGS_DBG_FW_PRINT_B,
					  AAC_DEBUG_PREAMBLE "%s\n"
					  AAC_DEBUG_POSTAMBLE,
					  buffer));
#endif
#endif
					sprintf(cp = buffer, "%*s", header_length, "");
					cp += header_length;
				}
				sprintf (cp, "%02x ", *(fib++));
				cp += strlen(cp);
				--len;
			}
			if (cp > &buffer[header_length]) {
				adbg_ioctl(dev, KERN_DEBUG,
				  AAC_DEBUG_PREAMBLE "%s\n"
				  AAC_DEBUG_POSTAMBLE, buffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x1)
				fwprintf((dev,
				  HBA_FLAGS_DBG_FW_PRINT_B,
				  AAC_DEBUG_PREAMBLE "%s\n"
				  AAC_DEBUG_POSTAMBLE, buffer));
#endif
#endif
			}
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB7))
			if (nblank(fwprintf(x)))
				dev->FwDebugFlags = DebugFlags;
#endif
		}
		adbg_ioctl(dev,KERN_DEBUG, AAC_DEBUG_PREAMBLE
		  "aac_fib_send(%x,,%d,...)\n"
		  AAC_DEBUG_POSTAMBLE, kfib->header.Command,
		  le16_to_cpu(kfib->header.Size));
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x4)
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  AAC_DEBUG_PREAMBLE "aac_fib_send(%x,,%d,...)"
		  AAC_DEBUG_POSTAMBLE, kfib->header.Command,
		  le16_to_cpu(kfib->header.Size)));
#endif
#endif
#endif
		retval = aac_fib_send(le16_to_cpu(kfib->header.Command), fibptr,
				le16_to_cpu(kfib->header.Size) , FsaNormal,
				1, 1, NULL, NULL);

			adbg_ioctl(dev,KERN_DEBUG, AAC_DEBUG_PREAMBLE
			  "aac_fib_send(%x,,%d,...) returns %d\n"
			  AAC_DEBUG_POSTAMBLE, kfib->header.Command,
			  le16_to_cpu(kfib->header.Size), retval);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x4)
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  AAC_DEBUG_PREAMBLE "aac_fib_send(%x,,%d,...) returns %d"
			  AAC_DEBUG_POSTAMBLE, kfib->header.Command,
			  le16_to_cpu(kfib->header.Size), retval));
#endif
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x5) == 0x1)
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  AAC_DEBUG_PREAMBLE "FIB=%p returns %d"
			  AAC_DEBUG_POSTAMBLE, kfib, retval));
#endif
#endif
		if (aac_fib_complete(fibptr) != 0) {
			if (!retval)
				retval = -EINVAL;
			goto cleanup;
		}
		if (retval) {
			goto cleanup;
		}
	}
	/*
	 *	Make sure that the size returned by the adapter (which includes
	 *	the header) is less than or equal to the size of a fib, so we
	 *	don't corrupt application data. Then copy that size to the user
	 *	buffer. (Don't try to add the header information again, since it
	 *	was already included by the adapter.)
	 */

	retval = 0;
	if (copy_to_user(arg, (void *)kfib, size))
		retval = -EFAULT;
cleanup:
	if (hw_fib) {
		pci_free_consistent(dev->pdev, size, kfib, fibptr->hw_fib_pa);
		fibptr->hw_fib_pa = hw_fib_pa;
		fibptr->hw_fib_va = hw_fib;
	}
	if (retval != -ERESTARTSYS)
		aac_fib_free(fibptr);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) || defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
	else {

		adbg_ioctl(dev,KERN_DEBUG, AAC_DEBUG_PREAMBLE
		  "ioctl_send_fib(%p,%p) returns -ERESTARTSYS\n"
		  AAC_DEBUG_POSTAMBLE, dev, arg);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x2)
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
		  "ioctl_send_fib(%p,%p) returns -ERESTARTSYS"
		  AAC_DEBUG_POSTAMBLE, dev, arg));
#endif
#endif
		return retval;
	}
#endif

	adbg_ioctl(dev,KERN_DEBUG, AAC_DEBUG_PREAMBLE
	  "ioctl_send_fib(%p,%p) returns %d\n"
	  AAC_DEBUG_POSTAMBLE, dev, arg, retval);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x2)
	fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
	  AAC_DEBUG_PREAMBLE "ioctl_send_fib(%p,%p) returns %d"
	  AAC_DEBUG_POSTAMBLE, dev, arg, retval));
#endif
#endif
	return retval;
}

/**
 *	open_getadapter_fib	-	Get the next fib
 *
 *	This routine will get the next Fib, if available, from the AdapterFibContext
 *	passed in from the user.
 */

static int open_getadapter_fib(struct aac_dev * dev, void __user *arg)
{
	struct aac_fib_context * fibctx;
	int status;

	fibctx = kmalloc(sizeof(struct aac_fib_context), GFP_KERNEL);
	if (fibctx == NULL) {
		status = -ENOMEM;
	} else {
		unsigned long flags;
		struct list_head * entry;
		struct aac_fib_context * context;

		fibctx->type = FSAFS_NTC_GET_ADAPTER_FIB_CONTEXT;
		fibctx->size = sizeof(struct aac_fib_context);
		/*
		 *	Yes yes, I know this could be an index, but we have a
		 * better guarantee of uniqueness for the locked loop below.
		 * Without the aid of a persistent history, this also helps
		 * reduce the chance that the opaque context would be reused.
		 */
		fibctx->unique = (u32)((ulong)fibctx & 0xFFFFFFFF);
		/*
		 *	Initialize the mutex used to wait for the next AIF.
		 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
		init_MUTEX_LOCKED(&fibctx->wait_sem);
#else
		sema_init(&fibctx->wait_sem, 0);
#endif

		fibctx->wait = 0;
		/*
		 *	Initialize the fibs and set the count of fibs on
		 *	the list to 0.
		 */
		fibctx->count = 0;
		INIT_LIST_HEAD(&fibctx->fib_list);
		fibctx->jiffies = jiffies/HZ;
		/*
		 *	Now add this context onto the adapter's
		 *	AdapterFibContext list.
		 */
		spin_lock_irqsave(&dev->fib_lock, flags);
		/* Ensure that we have a unique identifier */
		entry = dev->fib_list.next;
		while (entry != &dev->fib_list) {
			context = list_entry(entry, struct aac_fib_context, next);
			if (context->unique == fibctx->unique) {
				/* Not unique (32 bits) */
				fibctx->unique++;
				entry = dev->fib_list.next;
			} else {
				entry = entry->next;
			}
		}
		list_add_tail(&fibctx->next, &dev->fib_list);
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		if (copy_to_user(arg, &fibctx->unique,
						sizeof(fibctx->unique))) {
			status = -EFAULT;
		} else {
			status = 0;
		}
	}
	return status;
}

/**
 *	next_getadapter_fib	-	get the next fib
 *	@dev: adapter to use
 *	@arg: ioctl argument
 *
 *	This routine will get the next Fib, if available, from the AdapterFibContext
 *	passed in from the user.
 */

static int next_getadapter_fib(struct aac_dev * dev, void __user *arg)
{
	struct fib_ioctl f;
	struct fib *fib;
	struct aac_fib_context *fibctx;
	int status;
	struct list_head * entry;
	unsigned long flags;

	if(copy_from_user((void *)&f, arg, sizeof(struct fib_ioctl)))
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
	{
		printk(KERN_INFO"next_getadapter_fib(%p,%p)=-EFAULT\n", dev, arg);
#endif
		return -EFAULT;
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
	}
#endif
	/*
	 *	Verify that the HANDLE passed in was a valid AdapterFibContext
	 *
	 *	Search the list of AdapterFibContext addresses on the adapter
	 *	to be sure this is a valid address
	 */
	spin_lock_irqsave(&dev->fib_lock, flags);
	entry = dev->fib_list.next;
	fibctx = NULL;

	while (entry != &dev->fib_list) {
		fibctx = list_entry(entry, struct aac_fib_context, next);
		/*
		 *	Extract the AdapterFibContext from the Input parameters.
		 */
		if (fibctx->unique == f.fibctx) { /* We found a winner */
			break;
		}
		entry = entry->next;
		fibctx = NULL;
	}
	if (!fibctx) {
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		dprintk ((KERN_INFO "Fib Context not found\n"));
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
		printk(KERN_INFO"next_getadapter_fib(%p,%p)=-EINVAL no fib context\n", dev, arg);
#endif
		return -EINVAL;
	}

	if((fibctx->type != FSAFS_NTC_GET_ADAPTER_FIB_CONTEXT) ||
		 (fibctx->size != sizeof(struct aac_fib_context))) {
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		dprintk ((KERN_INFO "Fib Context corrupt?\n"));
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
		printk(KERN_INFO"next_getadapter_fib(%p,%p)=-EINVAL fib context corrupt\n", dev, arg);
#endif
		return -EINVAL;
	}
	status = 0;
	/*
	 *	If there are no fibs to send back, then either wait or return
	 *	-EAGAIN
	 */
return_fib:
	if (!list_empty(&fibctx->fib_list)) {
//		struct list_head * entry;
		/*
		 *	Pull the next fib from the fibs
		 */
		entry = fibctx->fib_list.next;
		list_del(entry);

		fib = list_entry(entry, struct fib, fiblink);
		fibctx->count--;
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		if (copy_to_user(f.fib, fib->hw_fib_va, sizeof(struct hw_fib))) {
			kfree(fib->hw_fib_va);
			kfree(fib);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
			printk(KERN_INFO
			  "next_getadapter_fib(%p,%p)=-EFAULT copy_to_user(%p,%p,%u)\n",
			  dev, arg, f.fib, fib->hw_fib_va,
			  (unsigned)sizeof(struct hw_fib));
#endif
			return -EFAULT;
		}
		/*
		 *	Free the space occupied by this copy of the fib.
		 */
		kfree(fib->hw_fib_va);
		kfree(fib);
		status = 0;
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
		printk(KERN_INFO"next_getadapter_fib(%p,%p)=0\n", dev, arg);
#endif
	} else {
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		/* If someone killed the AIF aacraid thread, restart it */
		status = !dev->aif_thread;
#if (defined(HAS_FIND_TASK_BY_PID))
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,16))
		if (!status) { /* Insurance */
			read_lock(&tasklist_lock);
#else
		if (!status) /* Insurance */
#endif
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
			status = !find_task_by_pid(dev->thread_pid);
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			status = !find_task_by_pid(dev->thread->pid);
#else
			status = !find_task_by_vpid(dev->thread->pid);
#endif
#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,16))
			read_unlock(&tasklist_lock);
		}
#endif
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN) || ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN)))))
		if (!dev->shutdown)
#endif
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD) && (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)))
		if (!dev->thread_die)
#endif
		if (status && !dev->in_reset && dev->queues && dev->fsa_dev) {
			/* Be paranoid, be very paranoid! */
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
			kill_proc(dev->thread_pid, SIGKILL, 0);
#else
			kthread_stop(dev->thread);
#endif
			ssleep(1);
			dev->aif_thread = 0;
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
			dev->thread_pid = kernel_thread(
			  (int (*)(void *))aac_command_thread, dev, 0);
#else
			dev->thread = kthread_run(aac_command_thread, dev, dev->name);
#endif
			ssleep(1);
		}
		if (f.wait) {
			if(down_interruptible(&fibctx->wait_sem) < 0) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
				printk(KERN_INFO"next_getadapter_fib(%p,%p)=-EINTR\n", dev, arg);
#endif
				status = -ERESTARTSYS;
			} else {
				/* Lock again and retry */
				spin_lock_irqsave(&dev->fib_lock, flags);
				goto return_fib;
			}
		} else {
			status = -EAGAIN;
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF))
			printk(KERN_INFO"next_getadapter_fib(%p,%p)=-EAGAIN\n", dev, arg);
#endif
		}
	}
	fibctx->jiffies = jiffies/HZ;
	return status;
}

int aac_close_fib_context(struct aac_dev * dev, struct aac_fib_context * fibctx)
{
	struct fib *fib;

	/*
	 *	First free any FIBs that have not been consumed.
	 */
	while (!list_empty(&fibctx->fib_list)) {
		struct list_head * entry;
		/*
		 *	Pull the next fib from the fibs
		 */
		entry = fibctx->fib_list.next;
		list_del(entry);
		fib = list_entry(entry, struct fib, fiblink);
		fibctx->count--;
		/*
		 *	Free the space occupied by this copy of the fib.
		 */
		kfree(fib->hw_fib_va);
		kfree(fib);
	}
	/*
	 *	Remove the Context from the AdapterFibContext List
	 */
	list_del(&fibctx->next);
	/*
	 *	Invalidate context
	 */
	fibctx->type = 0;
	/*
	 *	Free the space occupied by the Context
	 */
	kfree(fibctx);
	return 0;
}

/**
 *	close_getadapter_fib	-	close down user fib context
 *	@dev: adapter
 *	@arg: ioctl arguments
 *
 *	This routine will close down the fibctx passed in from the user.
 */

static int close_getadapter_fib(struct aac_dev * dev, void __user *arg)
{
	struct aac_fib_context *fibctx;
	int status;
	unsigned long flags;
	struct list_head * entry;

	/*
	 *	Verify that the HANDLE passed in was a valid AdapterFibContext
	 *
	 *	Search the list of AdapterFibContext addresses on the adapter
	 *	to be sure this is a valid address
	 */

	entry = dev->fib_list.next;
	fibctx = NULL;

	while(entry != &dev->fib_list) {
		fibctx = list_entry(entry, struct aac_fib_context, next);
		/*
		 *	Extract the fibctx from the input parameters
		 */
		if (fibctx->unique == (u32)(uintptr_t)arg) /* We found a winner */
			break;
		entry = entry->next;
		fibctx = NULL;
	}

	if (!fibctx)
		return 0; /* Already gone */

	if((fibctx->type != FSAFS_NTC_GET_ADAPTER_FIB_CONTEXT) ||
		 (fibctx->size != sizeof(struct aac_fib_context)))
		return -EINVAL;
	spin_lock_irqsave(&dev->fib_lock, flags);
	status = aac_close_fib_context(dev, fibctx);
	spin_unlock_irqrestore(&dev->fib_lock, flags);
	return status;
}

/**
 *	check_revision	-	close down user fib context
 *	@dev: adapter
 *	@arg: ioctl arguments
 *
 *	This routine returns the driver version.
 *	Under Linux, there have been no version incompatibilities, so this is
 *	simple!
 */

static int check_revision(struct aac_dev *dev, void __user *arg)
{
	struct revision response;
	char *driver_version = aac_driver_version;
	u32 version;

	response.compat = 1;
	version = (simple_strtol(driver_version,
				&driver_version, 10) << 24) | 0x00000400;
	version += simple_strtol(driver_version + 1, &driver_version, 10) << 16;
	version += simple_strtol(driver_version + 1, NULL, 10);
	response.version = cpu_to_le32(version);
#	if (defined(AAC_DRIVER_BUILD))
		response.build = cpu_to_le32(AAC_DRIVER_BUILD);
#	else
		response.build = cpu_to_le32(9999);
#	endif

	if (copy_to_user(arg, &response, sizeof(response)))
		return -EFAULT;
	return 0;
}

#if (defined(CODE_STREAM_IDENTIFIER) && !defined(CONFIG_COMMUNITY_KERNEL))
/**
 *	check_code_stream	-	close down user fib context
 *	@dev: adapter
 *	@arg: ioctl arguments
 *
 *	This routine returns the driver code stream identifier
 */

static int check_code_stream_identifier(struct aac_dev *dev, void __user *arg)
{
	struct VersionMatch response;

	memset (&response, 0, sizeof(response));
	strncpy (response.driver, CODE_STREAM_IDENTIFIER,
	  MAX_CODE_STREAM_IDENTIFIER_LENGTH);
	strncpy (response.firmware, dev->code_stream_identifier,
	  MAX_CODE_STREAM_IDENTIFIER_LENGTH);
	if (response.firmware[0] == '\0')
		response.status = VERSION_MATCH_UNSUPPORTED;
	else if (strncmp(response.driver, response.firmware,
	  MAX_CODE_STREAM_IDENTIFIER_LENGTH))
		response.status = VERSION_MATCH_FAILED;
	else
		response.status = VERSION_MATCH_SUCCESS;

	if (copy_to_user(arg, &response, sizeof(response)))
		return -EFAULT;
	return 0;
}
#endif

static int aac_error_inject(struct aac_dev *dev, void __user *arg)
{
	struct aac_error_inject_str inj;
	struct fsa_dev_info *fsa_dev_ptr;

	fsa_dev_ptr = dev->fsa_dev;
	if (!fsa_dev_ptr)
		return -EBUSY;

	if (copy_from_user(&inj, arg, sizeof(struct aac_error_inject_str)))
		return -EFAULT;

	if (inj.type == 1)
		dev->simulated_scsi_error = (1 << inj.value);
	else if (inj.type == 2)
		dev->simulated_tgt_failure = (1 << inj.value);
	return 0;
}	

/**
 *
 * aac_send_raw_scb
 *
 */

static int aac_send_raw_srb(struct aac_dev* dev, void __user * arg)
{
	struct fib* srbfib;
	int status;
	struct aac_srb *srbcmd = NULL;
	struct aac_hba_cmd_req *hbacmd = NULL;
	struct user_aac_srb *user_srbcmd = NULL;
	struct user_aac_srb __user *user_srb = arg;
	struct aac_srb_reply __user *user_reply;
	u32 chn, fibsize = 0, flags = 0;
	s32 rcode = 0;
	u32 data_dir;
	void __user *sg_user[HBA_MAX_SG_EMBEDDED];
	void *sg_list[HBA_MAX_SG_EMBEDDED];
	u32 sg_count[HBA_MAX_SG_EMBEDDED];
	u32 sg_indx = 0;
	u32 byte_count = 0;
	u32 actual_fibsize64, actual_fibsize = 0;
	int i, is_native_device;
	u64 address;


	if (dev->in_reset) {
		dprintk((KERN_DEBUG"aacraid: send raw srb -EBUSY\n"));
		return -EBUSY;
	}
	if (!capable(CAP_SYS_ADMIN)){
		dprintk((KERN_DEBUG"aacraid: No permission to send raw srb\n"));
		return -EPERM;
	}
	/*
	 *	Allocate and initialize a Fib then setup a SRB command
	 */
	if (!(srbfib = aac_fib_alloc(dev))) {
		return -ENOMEM;
	}

	memset(sg_list, 0, sizeof(sg_list)); /* cleanup may take issue */
	if(copy_from_user(&fibsize, &user_srb->count,sizeof(u32))){
		dprintk((KERN_DEBUG"aacraid: Could not copy data size from user\n"));
		rcode = -EFAULT;
		goto cleanup;
	}

	if (fibsize > (dev->max_fib_size - sizeof(struct aac_fibhdr))) {
		rcode = -EINVAL;
		goto cleanup;
	}

	user_srbcmd = kmalloc(fibsize, GFP_KERNEL);
	if (!user_srbcmd) {
		dprintk((KERN_DEBUG"aacraid: Could not make a copy of the srb\n"));
		rcode = -ENOMEM;
		goto cleanup;
	}
	if(copy_from_user(user_srbcmd, user_srb,fibsize)){
		dprintk((KERN_DEBUG"aacraid: Could not copy srb from user\n"));
		rcode = -EFAULT;
		goto cleanup;
	}
#	if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		{
			u8 * srb = (u8 *)user_srbcmd;
			unsigned len = fibsize;
			char buffer[80];
			char * cp = buffer;

			strcpy(cp, "SRB=");
			cp += 4;
			while (len > 0) {
				if (cp >= &buffer[sizeof(buffer)-4]) {
					printk (KERN_INFO "%s\n", buffer);
					strcpy(cp = buffer, "    ");
					cp += 4;
				}
				sprintf (cp, "%02x ", *(srb++));
				cp += strlen(cp);
				--len;
			}
			if (cp > &buffer[4])
				printk (KERN_INFO "%s\n", buffer);
		}
#	endif

	flags = user_srbcmd->flags; /* from user in cpu order */
	switch (flags & (SRB_DataIn | SRB_DataOut)) {
	case SRB_DataOut:
		data_dir = DMA_TO_DEVICE;
		break;
	case (SRB_DataIn | SRB_DataOut):
		data_dir = DMA_BIDIRECTIONAL;
		break;
	case SRB_DataIn:
		data_dir = DMA_FROM_DEVICE;
		break;
	default:
		data_dir = DMA_NONE;
	}
	if (user_srbcmd->sg.count > ARRAY_SIZE(sg_list)) {
		dprintk((KERN_DEBUG"aacraid: too many sg entries %d\n", user_srbcmd->sg.count));
		rcode = -EINVAL;
		goto cleanup;
	}
	if ((data_dir == DMA_NONE) && user_srbcmd->sg.count) {
		dprintk((KERN_DEBUG"aacraid: SG with no direction specified in Raw SRB command\n"));
		rcode = -EINVAL;
		goto cleanup;
	}
	actual_fibsize = sizeof(struct aac_srb) - sizeof(struct sgentry) +
		((user_srbcmd->sg.count & 0xff) * sizeof(struct sgentry));
	actual_fibsize64 = actual_fibsize + (user_srbcmd->sg.count & 0xff) *
	  (sizeof(struct sgentry64) - sizeof(struct sgentry));
	/* User made a mistake - should not continue */
	if ((actual_fibsize != fibsize) && (actual_fibsize64 != fibsize)) {
		dprintk((KERN_DEBUG"aacraid: Bad Size specified in "
		  "Raw SRB command calculated fibsize=%lu;%lu "
		  "user_srbcmd->sg.count=%d aac_srb=%lu sgentry=%lu;%lu "
		  "issued fibsize=%d\n",
		  actual_fibsize, actual_fibsize64, user_srbcmd->sg.count,
		  sizeof(struct aac_srb), sizeof(struct sgentry),
		  sizeof(struct sgentry64), fibsize));
		rcode = -EINVAL;
		goto cleanup;
	}
	
	chn = aac_logical_to_phys(user_srbcmd->channel);
	if (chn < AAC_MAX_BUSES && user_srbcmd->id < AAC_MAX_TARGETS && 
		dev->hba_map[chn][user_srbcmd->id].devtype == AAC_DEVTYPE_NATIVE_RAW) {
		is_native_device = 1;
		hbacmd = (struct aac_hba_cmd_req *)srbfib->hw_fib_va;
		memset(hbacmd, 0, 96);	/* sizeof(*hbacmd) is not necessary */
	
		/* iu_type is a parameter of aac_hba_send */
		switch (data_dir) {
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
		hbacmd->lun[1] = cpu_to_le32(user_srbcmd->lun);
		hbacmd->it_nexus = dev->hba_map[chn][user_srbcmd->id].rmw_nexus;

		/* we fill in reply_qid later in aac_src_deliver_message */
		/* we fill in iu_type, request_id later in aac_hba_send */
		/* we fill in emb_data_desc_count, data_length later in sg list build */
	
		memcpy(hbacmd->cdb, user_srbcmd->cdb, sizeof(hbacmd->cdb));

		address = (u64)srbfib->hw_error_pa;
		hbacmd->error_ptr_hi = cpu_to_le32((u32)(address >> 32));
		hbacmd->error_ptr_lo = cpu_to_le32((u32)(address & 0xffffffff));
		hbacmd->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);
		hbacmd->emb_data_desc_count = cpu_to_le32(user_srbcmd->sg.count);
		srbfib->hbacmd_size = 64 + 
			user_srbcmd->sg.count * sizeof(struct aac_hba_sgl);
		
	} else {
		is_native_device = 0;
		aac_fib_init(srbfib);
		
		/* raw_srb FIB is not FastResponseCapable */
		srbfib->hw_fib_va->header.XferState &= ~cpu_to_le32(FastResponseCapable);

		srbcmd = (struct aac_srb*) fib_data(srbfib);
	
		// Fix up srb for endian and force some values

		srbcmd->function = cpu_to_le32(SRBF_ExecuteScsi);	// Force this
		srbcmd->channel	 = cpu_to_le32(user_srbcmd->channel);
		srbcmd->id	 = cpu_to_le32(user_srbcmd->id);
		srbcmd->lun	 = cpu_to_le32(user_srbcmd->lun);
		srbcmd->timeout	 = cpu_to_le32(user_srbcmd->timeout);
		srbcmd->flags	 = cpu_to_le32(flags);
		srbcmd->retry_limit = 0; // Obsolete parameter
		srbcmd->cdb_size = cpu_to_le32(user_srbcmd->cdb_size);
		memcpy(srbcmd->cdb, user_srbcmd->cdb, sizeof(srbcmd->cdb));
	}

	byte_count = 0;
	if (is_native_device) {
		struct user_sgmap* usg32 = &user_srbcmd->sg;
		struct user_sgmap64* usg64 = (struct user_sgmap64*)&user_srbcmd->sg;
		for (i = 0; i < usg32->count; i++) {
			void *p;
			u64 addr;
			
			sg_count[i] = (actual_fibsize64 == fibsize) ?
				usg64->sg[i].count : usg32->sg[i].count;
			if (sg_count[i] > (dev->scsi_host_ptr->max_sectors << 9)) {

					adbg_ioctl(dev, KERN_INFO, "aacraid: upsg->sg[%d].count=%u>%u\n", 
						i, sg_count[i], dev->scsi_host_ptr->max_sectors << 9);

				rcode = -EINVAL;
				goto cleanup;
			}
			p = kmalloc(sg_count[i], GFP_KERNEL|__GFP_DMA);
			if (!p) {
				dprintk((KERN_DEBUG"aacraid: Could not allocate SG buffer - size = %d buffer number %d of %d\n",
					  sg_count[i], i, user_srbcmd->sg.count));
				rcode = -ENOMEM;
				goto cleanup;
			}
			if (actual_fibsize64 == fibsize) {
				addr = (u64)usg64->sg[i].addr[0];
				addr += ((u64)usg64->sg[i].addr[1]) << 32;
			} else {
				addr = (u64)usg32->sg[i].addr;
			}
			sg_user[i] = (void __user *)(uintptr_t)addr;
			sg_list[i] = p; // save so we can clean up later
			sg_indx = i;

			if (flags & SRB_DataOut) {
				if (copy_from_user(p, sg_user[i], sg_count[i])) {
					dprintk((KERN_DEBUG"aacraid: Could not copy sg data from user\n"));
					rcode = -EFAULT;
					goto cleanup;
				}
			}
			addr = pci_map_single(dev->pdev, p, sg_count[i], data_dir);
			hbacmd->sge[i].addr_hi = cpu_to_le32((u32)(addr>>32));
			hbacmd->sge[i].addr_lo = cpu_to_le32((u32)(addr & 0xffffffff));
			hbacmd->sge[i].len = cpu_to_le32(sg_count[i]);
			hbacmd->sge[i].flags = 0;
			byte_count += sg_count[i];
		}
		if (usg32->count > 0)	/* embedded sglist */
			hbacmd->sge[usg32->count-1].flags = cpu_to_le32(0x40000000);
		hbacmd->data_length = cpu_to_le32(byte_count);
		status = aac_hba_send(HBA_IU_TYPE_SCSI_CMD_REQ, srbfib, NULL, NULL);
		
	} else if (dev->adapter_info.options & AAC_OPT_SGMAP_HOST64) {
		struct user_sgmap64* upsg = (struct user_sgmap64*)&user_srbcmd->sg;
		struct sgmap64* psg = (struct sgmap64*)&srbcmd->sg;

		/*
		 * This should also catch if user used the 32 bit sgmap
		 */
		if (actual_fibsize64 == fibsize) {
			actual_fibsize = actual_fibsize64;
			for (i = 0; i < upsg->count; i++) {
				u64 addr;
				void* p;
				
				sg_count[i] = upsg->sg[i].count;
				if (sg_count[i] >
				    ((dev->adapter_info.options &
				     AAC_OPT_NEW_COMM) ?
				      (dev->scsi_host_ptr->max_sectors << 9) :
				      65536)) {
						adbg_ioctl(dev,KERN_INFO, "aacraid: upsg->sg[%d].count=%u>%u\n", i, sg_count[i], (dev->adapter_info.options & AAC_OPT_NEW_COMM) ? (dev->scsi_host_ptr->max_sectors << 9) : 65536);
					rcode = -EINVAL;
					goto cleanup;
				}
				/* Does this really need to be GFP_DMA? */
				p = kmalloc(sg_count[i], GFP_KERNEL|__GFP_DMA);
				if(!p) {
					dprintk((KERN_DEBUG"aacraid: Could not allocate SG buffer - size = %d buffer number %d of %d\n",
					  sg_count[i], i, upsg->count));
					rcode = -ENOMEM;
					goto cleanup;
				}
				addr = (u64)upsg->sg[i].addr[0];
				addr += ((u64)upsg->sg[i].addr[1]) << 32;
				sg_user[i] = (void __user *)(uintptr_t)addr;
				sg_list[i] = p; // save so we can clean up later
				sg_indx = i;

				if (flags & SRB_DataOut) {
					if(copy_from_user(p, sg_user[i], sg_count[i])){
						dprintk((KERN_DEBUG"aacraid: Could not copy sg data from user\n"));
						rcode = -EFAULT;
						goto cleanup;
					}
				}
				addr = pci_map_single(dev->pdev, p, sg_count[i], data_dir);

				psg->sg[i].addr[0] = cpu_to_le32(addr & 0xffffffff);
				psg->sg[i].addr[1] = cpu_to_le32(addr>>32);
				byte_count += sg_count[i];
				psg->sg[i].count = cpu_to_le32(sg_count[i]);
			}
		} else {
			struct user_sgmap* usg;
			usg = kmalloc(actual_fibsize - sizeof(struct aac_srb)
			  + sizeof(struct sgmap), GFP_KERNEL);
			if (!usg) {
				dprintk((KERN_DEBUG"aacraid: Allocation error in Raw SRB command\n"));
				rcode = -ENOMEM;
				goto cleanup;
			}
			memcpy (usg, upsg, actual_fibsize - sizeof(struct aac_srb)
			  + sizeof(struct sgmap));
			actual_fibsize = actual_fibsize64;

			for (i = 0; i < usg->count; i++) {
				u64 addr;
				void* p;
				
				sg_count[i] = usg->sg[i].count;
				if (sg_count[i] >
				    ((dev->adapter_info.options &
				     AAC_OPT_NEW_COMM) ?
				      (dev->scsi_host_ptr->max_sectors << 9) :
				      65536)) {
						adbg_ioctl(dev,KERN_INFO, "aacraid: usg->sg[%d].count=%u>%u\n", i, sg_count[i], (dev->adapter_info.options & AAC_OPT_NEW_COMM) ? (dev->scsi_host_ptr->max_sectors << 9) : 65536);
					rcode = -EINVAL;
					goto cleanup;
				}
				/* Does this really need to be GFP_DMA? */
				p = kmalloc(sg_count[i], GFP_KERNEL|__GFP_DMA);
				if(!p) {
					kfree (usg);
					dprintk((KERN_DEBUG"aacraid: Could not allocate SG buffer - size = %d buffer number %d of %d\n",
					  sg_count[i], i, usg->count));
					rcode = -ENOMEM;
					goto cleanup;
				}
				sg_user[i] = (void __user *)(uintptr_t)usg->sg[i].addr;
				sg_list[i] = p; // save so we can clean up later
				sg_indx = i;

				if (flags & SRB_DataOut) {
					if(copy_from_user(p, sg_user[i], sg_count[i])){
						kfree (usg);
						dprintk((KERN_DEBUG"aacraid: Could not copy sg data from user\n"));
						rcode = -EFAULT;
						goto cleanup;
					}
				}
				addr = pci_map_single(dev->pdev, p, sg_count[i], data_dir);

				psg->sg[i].addr[0] = cpu_to_le32(addr & 0xffffffff);
				psg->sg[i].addr[1] = cpu_to_le32(addr>>32);
				byte_count += sg_count[i];
				psg->sg[i].count = cpu_to_le32(sg_count[i]);
			}
			kfree (usg);
		}
		srbcmd->count = cpu_to_le32(byte_count);
		if(user_srbcmd->sg.count)
			psg->count = cpu_to_le32(sg_indx+1);
		else
			psg->count = 0;
#		if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
			{
				u8 * srb = (u8 *)srbfib->hw_fib_va;
				unsigned len = actual_fibsize + sizeof(struct aac_fibhdr);
				char buffer[80];
				char * cp = buffer;

				strcpy(cp, "FIB=");
				cp += 4;
				while (len > 0) {
					if (cp >= &buffer[sizeof(buffer)-4]) {
						printk (KERN_INFO "%s\n", buffer);
						strcpy(cp = buffer, "    ");
						cp += 4;
					}
					sprintf (cp, "%02x ", *(srb++));
					cp += strlen(cp);
					--len;
				}
				if (cp > &buffer[4])
					printk (KERN_INFO "%s\n", buffer);
			}
			printk(KERN_INFO
			  "aac_fib_send(ScsiPortCommand64,,%d,...)\n",
			  actual_fibsize);
#		endif
		status = aac_fib_send(ScsiPortCommand64, srbfib, actual_fibsize, FsaNormal, 1, 1,NULL,NULL);
	} else {
		struct user_sgmap* upsg = &user_srbcmd->sg;
		struct sgmap* psg = &srbcmd->sg;

		if (actual_fibsize64 == fibsize) {
			struct user_sgmap64* usg = (struct user_sgmap64 *)upsg;
			for (i = 0; i < upsg->count; i++) {
				uintptr_t addr;
				void* p;
				
				sg_count[i] = usg->sg[i].count;
				if (usg->sg[i].count >
				    ((dev->adapter_info.options &
				     AAC_OPT_NEW_COMM) ?
				      (dev->scsi_host_ptr->max_sectors << 9) :
				      65536)) {
						adbg_ioctl(dev, KERN_INFO, "aacraid: usg->sg[%d].count=%u>%u\n", i, sg_count[i], (dev->adapter_info.options & AAC_OPT_NEW_COMM) ? (dev->scsi_host_ptr->max_sectors << 9) : 65536);
					rcode = -EINVAL;
					goto cleanup;
				}
				/* Does this really need to be GFP_DMA? */
				p = kmalloc(sg_count[i], GFP_KERNEL|__GFP_DMA);
				if(!p) {
					dprintk((KERN_DEBUG"aacraid: Could not allocate SG buffer - size = %d buffer number %d of %d\n",
					  sg_count[i], i, usg->count));
					rcode = -ENOMEM;
					goto cleanup;
				}
				addr = (u64)usg->sg[i].addr[0];
				addr += ((u64)usg->sg[i].addr[1]) << 32;
				sg_user[i] = (void __user *)addr;
				sg_list[i] = p; // save so we can clean up later
				sg_indx = i;

				if (flags & SRB_DataOut) {
					if(copy_from_user(p, sg_user[i], sg_count[i])){
						dprintk((KERN_DEBUG"aacraid: Could not copy sg data from user\n"));
						rcode = -EFAULT;
						goto cleanup;
					}
				}
				addr = pci_map_single(dev->pdev, p, sg_count[i], data_dir);

				psg->sg[i].addr = cpu_to_le32(addr & 0xffffffff);
				byte_count += sg_count[i];
				psg->sg[i].count = cpu_to_le32(sg_count[i]);
			}
		} else {
			for (i = 0; i < upsg->count; i++) {
				dma_addr_t addr;
				void* p;
				
				sg_count[i] = upsg->sg[i].count;
				if (sg_count[i] >
				    ((dev->adapter_info.options &
				     AAC_OPT_NEW_COMM) ?
				      (dev->scsi_host_ptr->max_sectors << 9) :
				      65536)) {
						adbg_ioctl(dev, KERN_INFO, "aacraid: upsg->sg[%d].count=%u>%u\n", i, sg_count[i], (dev->adapter_info.options & AAC_OPT_NEW_COMM) ? (dev->scsi_host_ptr->max_sectors << 9) : 65536);
					rcode = -EINVAL;
					goto cleanup;
				}
				p = kmalloc(sg_count[i], GFP_KERNEL);
				if (!p) {
					dprintk((KERN_DEBUG"aacraid: Could not allocate SG buffer - size = %d buffer number %d of %d\n",
					  sg_count[i], i, upsg->count));
					rcode = -ENOMEM;
					goto cleanup;
				}
				sg_user[i] = (void __user *)(uintptr_t)upsg->sg[i].addr;
				sg_list[i] = p; // save so we can clean up later
				sg_indx = i;

				if (flags & SRB_DataOut) {
					if(copy_from_user(p, sg_user[i], sg_count[i])) {
						dprintk((KERN_DEBUG"aacraid: Could not copy sg data from user\n"));
						rcode = -EFAULT;
						goto cleanup;
					}
				}
				addr = pci_map_single(dev->pdev, p,
					sg_count[i], data_dir);

				psg->sg[i].addr = cpu_to_le32(addr);
				byte_count += sg_count[i];
				psg->sg[i].count = cpu_to_le32(sg_count[i]);
			}
		}
		srbcmd->count = cpu_to_le32(byte_count);
		if (user_srbcmd->sg.count)
			psg->count = cpu_to_le32(sg_indx+1);
		else
			psg->count = 0;
#		if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
			{
				u8 * srb = (u8 *)srbfib->hw_fib_va;
				unsigned len = actual_fibsize + sizeof(struct aac_fibhdr);
				char buffer[80];
				char * cp = buffer;

				strcpy(cp, "FIB=");
				cp += 4;
				while (len > 0) {
					if (cp >= &buffer[sizeof(buffer)-4]) {
						printk (KERN_INFO "%s\n", buffer);
						strcpy(cp = buffer, "    ");
						cp += 4;
					}
					sprintf (cp, "%02x ", *(srb++));
					cp += strlen(cp);
					--len;
				}
				if (cp > &buffer[4])
					printk (KERN_INFO "%s\n", buffer);
			}
			printk(KERN_INFO
			  "aac_fib_send(ScsiPortCommand,,%d,...)\n",
			  actual_fibsize);
#		endif
		status = aac_fib_send(ScsiPortCommand, srbfib, actual_fibsize, FsaNormal, 1, 1, NULL, NULL);
	}
	
	if (status == -ERESTARTSYS) {
		rcode = -ERESTARTSYS;
		goto cleanup;
	}

	if (status != 0) {
		dprintk((KERN_DEBUG"aacraid: Could not send raw srb fib to hba\n"));
		rcode = -ENXIO;
		goto cleanup;
	}

	if (flags & SRB_DataIn) {
		for(i = 0 ; i <= sg_indx; i++){
			if (copy_to_user(sg_user[i], sg_list[i], sg_count[i])) {
				dprintk((KERN_DEBUG"aacraid: Could not copy sg data to user\n"));
				rcode = -EFAULT;
				goto cleanup;

			}
		}
	}

	user_reply = arg + fibsize;
	if (is_native_device) {
		struct aac_hba_resp *err = 
			&((struct aac_native_hba *)srbfib->hw_fib_va)->resp.err;
		struct aac_srb_reply reply;
			
		reply.status = ST_OK;	
		if (srbfib->flags & FIB_CONTEXT_FLAG_FASTRESP) {
			/* fast response */
			reply.srb_status = SRB_STATUS_SUCCESS;
			reply.scsi_status = 0;
			reply.data_xfer_length = byte_count;
		} else {
			reply.srb_status = err->service_response;
			reply.scsi_status = err->status;
			reply.data_xfer_length = byte_count - 
				le32_to_cpu(err->residual_count);
			reply.sense_data_size = err->sense_response_data_len;
			memcpy(reply.sense_data, err->sense_response_buf, 
				AAC_SENSE_BUFFERSIZE);
		}
		if (copy_to_user(user_reply, &reply, sizeof(struct aac_srb_reply))) {
			dprintk((KERN_DEBUG"aacraid: Could not copy reply to user\n"));
			rcode = -EFAULT;
			goto cleanup;
		}
	} else {
		struct aac_srb_reply *reply;

		reply = (struct aac_srb_reply *) fib_data(srbfib);
		if (copy_to_user(user_reply, reply, sizeof(struct aac_srb_reply))) {
			dprintk((KERN_DEBUG"aacraid: Could not copy reply to user\n"));
			rcode = -EFAULT;
			goto cleanup;
		}
	}

cleanup:
	kfree(user_srbcmd);
	if (rcode != -ERESTARTSYS) {
		for(i=0; i <= sg_indx; i++){
			kfree(sg_list[i]);
		}
		aac_fib_complete(srbfib);
		aac_fib_free(srbfib);
	}

	return rcode;
}

struct aac_pci_info {
	u32 bus;
	u32 slot;
};


static int aac_get_pci_info(struct aac_dev* dev, void __user *arg)
{
	struct aac_pci_info pci_info;

	pci_info.bus = dev->pdev->bus->number;
	pci_info.slot = PCI_SLOT(dev->pdev->devfn);

	if (copy_to_user(arg, &pci_info, sizeof(struct aac_pci_info))) {
		dprintk((KERN_DEBUG "aacraid: Could not copy pci info\n"));
		return -EFAULT;
	}
	return 0;
}
		
static int aac_get_hba_info(struct aac_dev* dev, void __user *arg)
{
	struct aac_hba_info hbainfo;

	hbainfo.AdapterNumber 		= (u8) dev->id;
	hbainfo.SystemIoBusNumber	= dev->pdev->bus->number;
	hbainfo.DeviceNumber		= (dev->pdev->devfn >> 3);
	hbainfo.FunctionNumber		= (dev->pdev->devfn & 0x0007);

	hbainfo.VendorID		= dev->pdev->vendor;
	hbainfo.DeviceID		= dev->pdev->device;
	hbainfo.SubVendorID		= dev->pdev->subsystem_vendor;
	hbainfo.SubSystemID		= dev->pdev->subsystem_device;

	if (copy_to_user(arg, &hbainfo, sizeof(struct aac_hba_info))) {
		dprintk((KERN_DEBUG "aacraid: Could not copy hba info\n"));
		return -EFAULT;
	}

	return 0;
}

int aac_do_ioctl(struct aac_dev * dev, int cmd, void __user *arg)
{
	int status;


	if (cmd != FSACTL_GET_NEXT_ADAPTER_FIB)
		adbg_ioctl(dev, KERN_DEBUG, "aac_do_ioctl(%p,%x,%p)\n", dev, cmd, arg);


	/*
	 *	HBA gets first crack
	 */

	status = aac_dev_ioctl(dev, cmd, arg);
	if(status != -ENOTTY) {
		adbg_ioctl(dev, KERN_DEBUG, "aac_do_ioctl returns %d\n", status);
		return status;
	}


#if (defined(AAC_CSMI))
	/*
	 *	HP gets second crack
	 */
	 
	status = aac_csmi_ioctl(dev, cmd, arg);
	if (status != -ENOTTY) {
		adbg_ioctl(dev, KERN_DEBUG, "aac_do_ioctl returns %d\n", status)
		return status;
	}

#endif
	switch (cmd) {
	case FSACTL_MINIPORT_REV_CHECK:
		status = check_revision(dev, arg);
		break;
	case FSACTL_SEND_LARGE_FIB:
	case FSACTL_SENDFIB:
		status = ioctl_send_fib(dev, arg);
		break;
	case FSACTL_OPEN_GET_ADAPTER_FIB:
		status = open_getadapter_fib(dev, arg);
		break;
	case FSACTL_GET_NEXT_ADAPTER_FIB:
		status = next_getadapter_fib(dev, arg);
		break;
	case FSACTL_CLOSE_GET_ADAPTER_FIB:
		status = close_getadapter_fib(dev, arg);
		break;
	case FSACTL_SEND_RAW_SRB:
		status = aac_send_raw_srb(dev,arg);
		break;
	case FSACTL_GET_PCI_INFO:
		status = aac_get_pci_info(dev,arg);
		break;
	case FSACTL_GET_HBA_INFO:
		status = aac_get_hba_info(dev,arg);
		break;

#if (defined(CODE_STREAM_IDENTIFIER) && !defined(CONFIG_COMMUNITY_KERNEL))
	case FSACTL_GET_VERSION_MATCHING:
		status = check_code_stream_identifier(dev,arg);
		break;
#endif
	case FSACTL_ERROR_INJECT:
		status = aac_error_inject(dev, arg);
		break;
	default:
		status = -ENOTTY;
		break;
	}

	if (cmd != FSACTL_GET_NEXT_ADAPTER_FIB) {
		adbg_ioctl(dev, KERN_DEBUG, "aac_do_ioctl returns %d\n", status);
	}

	return status;
}
#endif

