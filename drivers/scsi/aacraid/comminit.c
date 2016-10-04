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
 *  comminit.c
 *
 * Abstract: This supports the initialization of the host adapter commuication interface.
 *    This is a platform dependent module.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
//#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/version.h>	/* Needed for the following */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/mm.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#include <scsi/scsi_host.h>
#else
#include "scsi.h"
#include "hosts.h"
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#endif

#include "aacraid.h"

void aac_define_int_mode(struct aac_dev *dev);

struct aac_common aac_config = {
	.irq_mod = 1
};

static int aac_alloc_comm(struct aac_dev *dev, void **commaddr, unsigned long commsize, unsigned long commalign)
{
	unsigned char *base;
	unsigned long size, align;
	const unsigned long fibsize = dev->max_fib_size;
	const unsigned long printfbufsiz = 256;
	unsigned long host_rrq_size, aac_init_size;
	union aac_init *init;
	extern int aac_hba_mode;

#if (((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + __GNUC_PATCHLEVEL__) > 400002)
	dma_addr_t phys;
#else
	dma_addr_t phys = 0;
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)) || !defined(CONFIG_GART_IOMMU))
	unsigned long aac_max_hostphysmempages;
#endif

	if ((dev->comm_interface == AAC_COMM_MESSAGE_TYPE1) ||
		(dev->comm_interface == AAC_COMM_MESSAGE_TYPE2) ||
		(dev->comm_interface == AAC_COMM_MESSAGE_TYPE3 && !dev->sa_firmware)) {
		host_rrq_size = 
			(dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB) * sizeof(u32);
		aac_init_size = sizeof(union aac_init);
	} else if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE3 && dev->sa_firmware) {
		host_rrq_size = 
			(dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB) * sizeof(u32) *AAC_MAX_MSIX;
		aac_init_size = sizeof(union aac_init) + 
	                (AAC_MAX_HRRQ - 1) * sizeof(struct _rrq);
	} else {
		host_rrq_size = 0;
		aac_init_size = sizeof(union aac_init);
	}
	size = fibsize + aac_init_size + commsize + commalign + printfbufsiz + host_rrq_size;

#if 0 && ((defined(CONFIG_X86) || defined(CONFIG_X86_64)) && (KERNEL_VERSION_CODE < KERNEL_VERSION(2,5,0)))
	base = kmalloc(size, GFP_ATOMIC|GFP_KERNEL);
	if (base) {
		phys = pci_map_single(dev->pdev, base, size, DMA_BIDIRECTIONAL);
		if (phys > (0x80000000UL - size)) {
			kfree(base);
			base = NULL;
		}
	}
	if (base == NULL)
#endif
 
	base = pci_alloc_consistent(dev->pdev, size, &phys);
	adbg_init(dev,KERN_INFO,"pci_alloc_consistent(%p,%lu,%p={0x%lx))=%p\n",
		   dev->pdev, size, &phys, (unsigned long)phys, base);


	if(base == NULL)
	{
		printk(KERN_ERR "aacraid: unable to create mapping.\n");
		return 0;
	}
	dev->comm_addr = (void *)base;
	dev->comm_phys = phys;
	dev->comm_size = size;

	if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE1 ||
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE2 ||
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE3) {
		dev->host_rrq = (u32 *)(base + fibsize);
		dev->host_rrq_pa = phys + fibsize;
		memset(dev->host_rrq, 0, host_rrq_size);
	}

	dev->init = (union aac_init *)(base + fibsize + host_rrq_size);
	dev->init_pa = phys + fibsize + host_rrq_size;
	adbg_init(dev,KERN_INFO,"aac->init=%p aac->init_pa=0x%lx\n",
		  dev->init, (unsigned long)dev->init_pa);


	init = dev->init;

	if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE3) {
		int i;
		u64 addr;
		
		init->r8.InitStructRevision = 
			cpu_to_le32(ADAPTER_INIT_STRUCT_REVISION_8);
		init->r8.InitFlags = cpu_to_le32(INITFLAGS_NEW_COMM_SUPPORTED |
			INITFLAGS_DRIVER_USES_UTC_TIME |
			INITFLAGS_DRIVER_SUPPORTS_PM);
		if (aac_hba_mode) {
			printk(KERN_INFO "aacraid: HBA mode enabled\n");
			init->r8.InitFlags |= 
				cpu_to_le32(INITFLAGS_DRIVER_SUPPORTS_HBA_MODE);
		} else {
			printk(KERN_INFO "aacraid: HBA mode NOT enabled\n");
		}
		init->r8.RRQueueCount = cpu_to_le32(dev->max_msix);
		init->r8.MaxIoSize =
			cpu_to_le32(dev->scsi_host_ptr->max_sectors << 9);
		init->r8.MaxNumAif=init->r8.Reserved1=init->r8.Reserved2 = 0;
		for (i = 0; i < dev->max_msix; i++) {
			addr = (u64)dev->host_rrq_pa + dev->vector_cap * i * sizeof(u32);
			init->r8.rrq[i].Host_AddrHigh = cpu_to_le32(addr >> 32);
			init->r8.rrq[i].Host_AddrLow = cpu_to_le32(addr & 0xffffffff);
			init->r8.rrq[i].MSIX_Id = i;
			init->r8.rrq[i].ElementCount = cpu_to_le16((u16)dev->vector_cap);
			init->r8.rrq[i].Comp_Thresh = init->r8.rrq[i].Unused = 0;
		}	
		dprintk((KERN_WARNING"aacraid: New Comm Interface type3 enabled\n"));
	} else {
		init->r7.InitStructRevision = cpu_to_le32(ADAPTER_INIT_STRUCT_REVISION);
		if (dev->max_fib_size != sizeof(struct hw_fib))
			init->r7.InitStructRevision = cpu_to_le32(ADAPTER_INIT_STRUCT_REVISION_4);
		init->r7.NoOfMSIXVectors = cpu_to_le32(Sa_MINIPORT_REVISION);
		init->r7.fsrev = cpu_to_le32(dev->fsrev);

		/*
		 *	Adapter Fibs are the first thing allocated so that they
		 *	start page aligned
		 */
		dev->aif_base_va = (struct hw_fib *)base;
	
		init->r7.AdapterFibsVirtualAddress = 0;
		init->r7.AdapterFibsPhysicalAddress = cpu_to_le32((u32)phys);
		init->r7.AdapterFibsSize = cpu_to_le32(fibsize);
		init->r7.AdapterFibAlign = cpu_to_le32(sizeof(struct hw_fib));
		/*
		 * number of 4k pages of host physical memory. The aacraid fw needs
		 * this number to be less than 4gb worth of pages. New firmware doesn't
		 * have any issues with the mapping system, but older Firmware did, and
		 * had *troubles* dealing with the math overloading past 32 bits, thus
		 * we must limit this field.
		 */
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)) && !defined(__VMKLNX__))
		aac_max_hostphysmempages = dma_get_required_mask(&dev->pdev->dev) >> 12;
		if (aac_max_hostphysmempages < AAC_MAX_HOSTPHYSMEMPAGES)
			init->r7.HostPhysMemPages = cpu_to_le32(aac_max_hostphysmempages);
		else
			init->r7.HostPhysMemPages = cpu_to_le32(AAC_MAX_HOSTPHYSMEMPAGES);
#elif (defined(CONFIG_GART_IOMMU))
		/*
		 * This assumes the memory is mapped zero->n, which isn't
		 * always true on real computers. It also has some slight problems
		 * with the GART on x86-64. I've btw never tried DMA from PCI space
		 * on this platform but don't be surprised if its problematic.
		 */
		init->r7.HostPhysMemPages = cpu_to_le32(AAC_MAX_HOSTPHYSMEMPAGES);
#else
		/* num_physpages is in system page units. */
		aac_max_hostphysmempages = num_physpages << (PAGE_SHIFT - 12);
		if (aac_max_hostphysmempages < AAC_MAX_HOSTPHYSMEMPAGES)
			init->r7.HostPhysMemPages = cpu_to_le32(aac_max_hostphysmempages);
		else 
			init->r7.HostPhysMemPages = cpu_to_le32(AAC_MAX_HOSTPHYSMEMPAGES);
#endif

		init->r7.InitFlags = cpu_to_le32(INITFLAGS_DRIVER_USES_UTC_TIME | 
			INITFLAGS_DRIVER_SUPPORTS_PM);
		init->r7.MaxIoCommands = 
			cpu_to_le32(dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB);
		init->r7.MaxIoSize = cpu_to_le32(dev->scsi_host_ptr->max_sectors << 9);
		init->r7.MaxFibSize = cpu_to_le32(dev->max_fib_size);
		init->r7.MaxNumAif = cpu_to_le32(dev->max_num_aif);

		if (dev->comm_interface == AAC_COMM_MESSAGE) {
			init->r7.InitFlags |= cpu_to_le32(INITFLAGS_NEW_COMM_SUPPORTED);
			dprintk((KERN_WARNING"aacraid: New Comm Interface enabled\n"));
		} else if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE1) {
			init->r7.InitStructRevision = cpu_to_le32(ADAPTER_INIT_STRUCT_REVISION_6);
			init->r7.InitFlags |= cpu_to_le32(INITFLAGS_NEW_COMM_SUPPORTED | 
				INITFLAGS_NEW_COMM_TYPE1_SUPPORTED | INITFLAGS_FAST_JBOD_SUPPORTED);
			init->r7.HostRRQ_AddrHigh = cpu_to_le32((u64)dev->host_rrq_pa >> 32);
			init->r7.HostRRQ_AddrLow = cpu_to_le32(dev->host_rrq_pa & 0xffffffff);
			dprintk((KERN_WARNING"aacraid: New Comm Interface type1 enabled\n"));
		} else if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE2) {
			init->r7.InitStructRevision = cpu_to_le32(ADAPTER_INIT_STRUCT_REVISION_7);
			init->r7.InitFlags |= cpu_to_le32(INITFLAGS_NEW_COMM_SUPPORTED | 
				INITFLAGS_NEW_COMM_TYPE2_SUPPORTED | INITFLAGS_FAST_JBOD_SUPPORTED);
			init->r7.HostRRQ_AddrHigh = cpu_to_le32((u64)dev->host_rrq_pa >> 32);
			init->r7.HostRRQ_AddrLow = cpu_to_le32(dev->host_rrq_pa & 0xffffffff);
			init->r7.NoOfMSIXVectors = cpu_to_le32(dev->max_msix);		/* number of MSI-X */
			/* must be the COMM_PREFERRED_SETTINGS values */
#if 0
			init->r7.MaxIoSize = cpu_to_le32(256L * 1024L);	/* always 256KB */
			init->r7.MaxFibSize = cpu_to_le32(2L * 1024L);		/* always 2KB */
#endif
			printk((KERN_WARNING"aacraid: New Comm Interface type2 enabled\n"));
		}
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
		else if (dev->comm_interface == AAC_COMM_APRE) {
			init->r7.InitFlags |= cpu_to_le32(INITFLAGS_APRE_SUPPORTED);
			dprintk((KERN_WARNING"aacraid: APRE Interface enabled\n"));
		}
#endif
	}

	/*
	 * Increment the base address by the amount already used
	 */
	base = base + fibsize + host_rrq_size + aac_init_size;
	phys = (dma_addr_t)((ulong)phys + fibsize + host_rrq_size + aac_init_size);
	/*
	 *	Align the beginning of Headers to commalign
	 */
	align = (commalign - ((uintptr_t)(base) & (commalign - 1)));
	base = base + align;
	phys = phys + align;
	/*
	 *	Fill in addresses of the Comm Area Headers and Queues
	 */
	*commaddr = base;
	if (dev->comm_interface != AAC_COMM_MESSAGE_TYPE3)
		init->r7.CommHeaderAddress = cpu_to_le32((u32)phys);
	/*
	 *	Increment the base address by the size of the CommArea
	 */
	base = base + commsize;
	phys = phys + commsize;
	/*
	 *	 Place the Printf buffer area after the Fast I/O comm area.
	 */
	dev->printfbuf = (void *)base;
	if (dev->comm_interface != AAC_COMM_MESSAGE_TYPE3) {
		init->r7.printfbuf = cpu_to_le32(phys);
		init->r7.printfbufsiz = cpu_to_le32(printfbufsiz);
	}
	memset(base, 0, printfbufsiz);
	return 1;
}
    
static void aac_queue_init(struct aac_dev * dev, struct aac_queue * q, u32 *mem, int qsize)
{
	atomic_set(&q->numpending, 0);
	q->dev = dev;
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
	q->cmdready = (struct semaphore) __SEMAPHORE_INITIALIZER(q->cmdready, 0);
#else
	init_waitqueue_head(&q->cmdready);
#endif
	INIT_LIST_HEAD(&q->cmdq);
	init_waitqueue_head(&q->qfull);
	spin_lock_init(&q->lockdata);
	q->lock = &q->lockdata;
	q->headers.producer = (__le32 *)mem;
	q->headers.consumer = (__le32 *)(mem+1);
	*(q->headers.producer) = cpu_to_le32(qsize);
	*(q->headers.consumer) = cpu_to_le32(qsize);
	q->entries = qsize;
}

/**
 *	aac_send_shutdown		-	shutdown an adapter
 *	@dev: Adapter to shutdown
 *
 *	This routine will send a VM_CloseAll (shutdown) request to the adapter.
 */

int aac_send_shutdown(struct aac_dev * dev)
{
	struct fib * fibctx;
	struct aac_close *cmd;
	int status;

	fibctx = aac_fib_alloc(dev);
	if (!fibctx)
		return -ENOMEM;
	aac_fib_init(fibctx);

	cmd = (struct aac_close *) fib_data(fibctx);

	cmd->command = cpu_to_le32(VM_CloseAll);
	cmd->cid = cpu_to_le32(0xfffffffe);

	status = aac_fib_send(ContainerCommand,
			  fibctx,
			  sizeof(struct aac_close),
			  FsaNormal,
			  -2 /* Timeout silently */, 1,
			  NULL, NULL);

	if (status >= 0)
		aac_fib_complete(fibctx);
	/* FIB should be freed only after getting the response from the F/W */
	if (status != -ERESTARTSYS)
		aac_fib_free(fibctx);
	dev->adapter_shutdown = 1;
	if ((dev->pdev->device == PMC_DEVICE_S7 ||
	     dev->pdev->device == PMC_DEVICE_S8 ||
	     dev->pdev->device == PMC_DEVICE_S9) &&
	     dev->msi_enabled)
		aac_src_access_devreg(dev, AAC_ENABLE_INTX);

	return status;
}

/**
 *	aac_comm_init	-	Initialise FSA data structures
 *	@dev:	Adapter to initialise
 *
 *	Initializes the data structures that are required for the FSA commuication
 *	interface to operate. 
 *	Returns
 *		1 - if we were able to init the commuication interface.
 *		0 - If there were errors initing. This is a fatal error.
 */
 
static int aac_comm_init(struct aac_dev * dev)
{
	unsigned long hdrsize = (sizeof(u32) * NUMBER_OF_COMM_QUEUES) * 2;
	unsigned long queuesize = sizeof(struct aac_entry) * TOTAL_QUEUE_ENTRIES;
	u32 *headers;
	struct aac_entry * queues;
	unsigned long size;
	struct aac_queue_block * comm = dev->queues;
	/*
	 *	Now allocate and initialize the zone structures used as our 
	 *	pool of FIB context records.  The size of the zone is based
	 *	on the system memory size.  We also initialize the mutex used
	 *	to protect the zone.
	 */
#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	spin_lock_init(&dev->fib_lock);
#endif

	/*
	 *	Allocate the physically contigous space for the commuication
	 *	queue headers. 
	 */
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	if (dev->comm_interface == AAC_COMM_APRE) {
		hdrsize = sizeof(u32) * 2;
		queuesize = (128 + sizeof(struct donelist_entry))
		          * (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB)
		          + (sizeof(struct donelist_entry) * 20);
	}
#endif

	size = hdrsize + queuesize;

	if (!aac_alloc_comm(dev, (void * *)&headers, size, QUEUE_ALIGNMENT))
		return -ENOMEM;

	queues = (struct aac_entry *)(((ulong)headers) + hdrsize);

	/* Adapter to Host normal priority Command queue */ 
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	if (dev->comm_interface == AAC_COMM_APRE) {
		comm->queue[ApreCmdQueue].base = queues;
		aac_queue_init(dev, &comm->queue[ApreCmdQueue], headers,
		  dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB);
		comm->queue[ApreCmdQueue].NxtDoneListEntry =
		    comm->queue[ApreCmdQueue].DoneListPool = (void *)queues
		        + (128
		        * (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB));
		comm->queue[ApreCmdQueue].Credits = APRE_MAX_CREDITS;
		return 0;
	}
#endif
	comm->queue[HostNormCmdQueue].base = queues;
	aac_queue_init(dev, &comm->queue[HostNormCmdQueue], headers, HOST_NORM_CMD_ENTRIES);
	queues += HOST_NORM_CMD_ENTRIES;
	headers += 2;

	/* Adapter to Host high priority command queue */
	comm->queue[HostHighCmdQueue].base = queues;
	aac_queue_init(dev, &comm->queue[HostHighCmdQueue], headers, HOST_HIGH_CMD_ENTRIES);
    
	queues += HOST_HIGH_CMD_ENTRIES;
	headers +=2;

	/* Host to adapter normal priority command queue */
	comm->queue[AdapNormCmdQueue].base = queues;
	aac_queue_init(dev, &comm->queue[AdapNormCmdQueue], headers, ADAP_NORM_CMD_ENTRIES);
    
	queues += ADAP_NORM_CMD_ENTRIES;
	headers += 2;

	/* host to adapter high priority command queue */
	comm->queue[AdapHighCmdQueue].base = queues;
	aac_queue_init(dev, &comm->queue[AdapHighCmdQueue], headers, ADAP_HIGH_CMD_ENTRIES);
    
	queues += ADAP_HIGH_CMD_ENTRIES;
	headers += 2;

	/* adapter to host normal priority response queue */
	comm->queue[HostNormRespQueue].base = queues;
	aac_queue_init(dev, &comm->queue[HostNormRespQueue], headers, HOST_NORM_RESP_ENTRIES);
	queues += HOST_NORM_RESP_ENTRIES;
	headers += 2;

	/* adapter to host high priority response queue */
	comm->queue[HostHighRespQueue].base = queues;
	aac_queue_init(dev, &comm->queue[HostHighRespQueue], headers, HOST_HIGH_RESP_ENTRIES);
   
	queues += HOST_HIGH_RESP_ENTRIES;
	headers += 2;

	/* host to adapter normal priority response queue */
	comm->queue[AdapNormRespQueue].base = queues;
	aac_queue_init(dev, &comm->queue[AdapNormRespQueue], headers, ADAP_NORM_RESP_ENTRIES);

	queues += ADAP_NORM_RESP_ENTRIES;
	headers += 2;
	
	/* host to adapter high priority response queue */ 
	comm->queue[AdapHighRespQueue].base = queues;
	aac_queue_init(dev, &comm->queue[AdapHighRespQueue], headers, ADAP_HIGH_RESP_ENTRIES);

	comm->queue[AdapNormCmdQueue].lock = comm->queue[HostNormRespQueue].lock;
	comm->queue[AdapHighCmdQueue].lock = comm->queue[HostHighRespQueue].lock;
	comm->queue[AdapNormRespQueue].lock = comm->queue[HostNormCmdQueue].lock;
	comm->queue[AdapHighRespQueue].lock = comm->queue[HostHighCmdQueue].lock;

	return 0;
}

struct aac_dev *aac_init_adapter(struct aac_dev *dev)
{
	u32 status[5];
	struct Scsi_Host * host = dev->scsi_host_ptr;
	extern int aac_sync_mode;

	/*
	 *	Check the preferred comm settings, defaults from template.
	 */
	dev->management_fib_count = 0;
	spin_lock_init(&dev->manage_lock);
	spin_lock_init(&dev->sync_lock);
	spin_lock_init(&dev->iq_lock);
	dev->max_fib_size = sizeof(struct hw_fib);
	dev->sg_tablesize = host->sg_tablesize = (dev->max_fib_size
		- sizeof(struct aac_fibhdr)
		- sizeof(struct aac_write) + sizeof(struct sgentry))
			/ sizeof(struct sgentry);
	dev->comm_interface = AAC_COMM_PRODUCER;
	dev->raw_io_interface = dev->raw_io_64 = 0;
#	if (defined(AAC_DEBUG_INSTRUMENT_INIT))
		status[0] = 0xFFFFFFFF;
#	endif
	if ((!aac_adapter_sync_cmd(dev, GET_ADAPTER_PROPERTIES,
		0,0,0,0,0,0, status+0, status+1, status+2, status+3, status+4)) &&
	 		(status[0] == 0x00000001)) {
	    	adbg_init(dev,KERN_INFO,"GET_ADAPTER_PROPERTIES: 0x%x 0x%x 0x%x 0x%x\n",
			  status[1], status[2], status[3], status[4]);
		dev->doorbell_mask = status[3];
		if (status[1] & le32_to_cpu(AAC_OPT_NEW_COMM_64))
			dev->raw_io_64 = 1;
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
		if (dev->a_ops.adapter_comm) {
			if (status[1] & le32_to_cpu(AAC_OPT_NEW_COMM)) {
				dev->comm_interface = AAC_COMM_MESSAGE;
				dev_>raw_io_interface = 1;	
			} else if (status[1] & le32_to_cpu(AAC_OPT_APRE)) {
				dev->comm_interface = AAC_COMM_APRE;
				dev->raw_io_interface = dev->raw_io_64 = 1;
				host->sg_tablesize = dev->sg_tablesize = 17;
				if (host->can_queue > 128)
					host->can_queue = 128;
			}
		}
#else
		dev->sync_mode = aac_sync_mode;
		if (dev->a_ops.adapter_comm &&
			(status[1] & le32_to_cpu(AAC_OPT_NEW_COMM))) {
			dev->comm_interface = AAC_COMM_MESSAGE;
			dev->raw_io_interface = 1;
			if ((status[1] & le32_to_cpu(AAC_OPT_NEW_COMM_TYPE1))) {
				/* driver supports TYPE1 (Tupelo) */
				dev->comm_interface = AAC_COMM_MESSAGE_TYPE1;
			} else if ((status[1] & le32_to_cpu(AAC_OPT_NEW_COMM_TYPE2))) {
				/* driver supports TYPE2 (Denali, Yosemite) */
				dev->comm_interface = AAC_COMM_MESSAGE_TYPE2;
			} else if ((status[1] & le32_to_cpu(AAC_OPT_NEW_COMM_TYPE3))) {
				/* driver supports TYPE3 (Thor) */
				dev->comm_interface = AAC_COMM_MESSAGE_TYPE3;
			} else if ((status[1] & le32_to_cpu(AAC_OPT_NEW_COMM_TYPE4))) {
				/* not supported TYPE - switch to sync. mode */
				dev->comm_interface = AAC_COMM_MESSAGE_TYPE2;
				dev->sync_mode = 1;
			}
		}
		if ((status[1] & le32_to_cpu(AAC_OPT_EXTENDED)) &&
			(status[4] & le32_to_cpu(AAC_EXTOPT_SA_FIRMWARE)))
			dev->sa_firmware = 1;
		else
			dev->sa_firmware = 0;
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
		/*
		 * serveraid 8i returns 0x800000 (8MB) for this,
		 * which works fine in Linux and the COS, but
		 * fails in the VMkernel which imposes a max
		 * size of 1MB.  as such, clamp anything over
		 * 1MB down to AAC_MIN_FOOTPRINT_SIZE and use
		 * the old communications interface.
		 *	-gmccready@vmware.com
		 */
#		define VMK_MAX_MAP_SIZE 0x100000

		if (status[2] > VMK_MAX_MAP_SIZE) {
			printk("aacraid: card asked for %d (0x%x), we truncated to %d (0x%x)\n", status[2], status[2], AAC_MIN_FOOTPRINT_SIZE, AAC_MIN_FOOTPRINT_SIZE);
			dev->comm_interface = AAC_COMM_PRODUCER;
		}
#endif
		if ((dev->comm_interface == AAC_COMM_MESSAGE) &&
		    (status[2] > dev->base_size)) {
			aac_adapter_ioremap(dev, 0);
			dev->base_size = status[2];
			if (aac_adapter_ioremap(dev, status[2])) {
				/* remap failed, go back ... */
				dev->comm_interface = AAC_COMM_PRODUCER;
				if (aac_adapter_ioremap(dev, AAC_MIN_FOOTPRINT_SIZE)) {
					printk(KERN_WARNING
					  "aacraid: unable to map adapter.\n");
					return NULL;
				}
			}
		}
	}
#	if (defined(AAC_DEBUG_INSTRUMENT_INIT))
		else
			printk(KERN_INFO
			  "GET_ADAPTER_PROPERTIES 0x%lx failed\n",
			  (unsigned long)status[0]);
		status[0] = 0xFFFFFFFF;
#	endif
	dev->max_msix = 0;
	dev->msi_enabled = 0;
	dev->adapter_shutdown = 0;
	if ((!aac_adapter_sync_cmd(dev, GET_COMM_PREFERRED_SETTINGS,
	  0, 0, 0, 0, 0, 0,
	  status+0, status+1, status+2, status+3, status+4))
	 && (status[0] == 0x00000001)) {
		/*
		 *	status[1] >> 16		maximum command size in KB
		 *	status[1] & 0xFFFF	maximum FIB size
		 *	status[2] >> 16		maximum SG elements to driver
		 *	status[2] & 0xFFFF	maximum SG elements from driver
		 *	status[3] & 0xFFFF	maximum number FIBs outstanding
		 */
	    	adbg_init(dev,KERN_INFO,"GET_COMM_PREFERRED_SETTINGS: %uKB %uB sg<%u sg<%u queue<%u\n",
			  status[1] >> 16, status[1] & 0xFFFF,
			  status[2] >> 16, status[2] & 0xFFFF,
			  status[3] & 0xFFFF);
		host->max_sectors = (status[1] >> 16) << 1;
		dev->max_fib_size = status[1] & 0xFFE0;	/* multiple of 32 for PMC */
		host->sg_tablesize = status[2] >> 16;
		dev->sg_tablesize = status[2] & 0xFFFF;
		if (dev->pdev->device == PMC_DEVICE_S7 ||
		    dev->pdev->device == PMC_DEVICE_S8 ||
		    dev->pdev->device == PMC_DEVICE_S9) {
#if (defined(AAC_CITRIX))
			if (host->can_queue > 248)
				host->can_queue = 248;
#elif (defined(CONFIG_XEN) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
			if (host->can_queue > 128)
				host->can_queue = 128;
#else
			if (host->can_queue > (status[3] >> 16) - AAC_NUM_MGT_FIB)
				host->can_queue = (status[3] >> 16) - AAC_NUM_MGT_FIB;
#endif
		} else if (host->can_queue > (status[3] & 0xFFFF) - AAC_NUM_MGT_FIB)
			host->can_queue = (status[3] & 0xFFFF) - AAC_NUM_MGT_FIB;
		dev->max_num_aif = status[4] & 0xFFFF;
	}
#	if (defined(AAC_DEBUG_INSTRUMENT_INIT))
		else
			printk(KERN_INFO
			  "GET_COMM_PREFERRED_SETTINGS 0x%lx failed\n",
			  (unsigned long)status[0]);
#	endif
	{

		if (numacb > 0) {
			if (numacb < host->can_queue)
				host->can_queue = numacb;
			else
				printk("numacb=%d ignored\n", numacb);
		}
	}

	if (dev->pdev->device == PMC_DEVICE_S6 ||
	    dev->pdev->device == PMC_DEVICE_S7 ||
	    dev->pdev->device == PMC_DEVICE_S8 ||
	    dev->pdev->device == PMC_DEVICE_S9)
		aac_define_int_mode(dev);
		
	/*
	 *	Ok now init the communication subsystem
	 */

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
	dev->queues = kmalloc(sizeof(struct aac_queue_block), GFP_KERNEL);
#else
	dev->queues = kzalloc(sizeof(struct aac_queue_block), GFP_KERNEL);
#endif
	if (dev->queues == NULL) {
		printk(KERN_ERR "Error could not allocate comm region.\n");
		return NULL;
	}
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
	memset(dev->queues, 0, sizeof(struct aac_queue_block));
#endif

	if (aac_comm_init(dev)<0){
		kfree(dev->queues);
		return NULL;
	}
	/*
	 *	Initialize the list of fibs
	 */
	if (aac_fib_setup(dev) < 0) {
		kfree(dev->queues);
		return NULL;
	}
		
	INIT_LIST_HEAD(&dev->fib_list);
	INIT_LIST_HEAD(&dev->sync_fib_list);
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	init_completion(&dev->aif_completion);
#endif

	return dev;
}


void aac_define_int_mode(struct aac_dev *dev)
{
	int i, msi_count;
	msi_count = i = 0;

	/* max. vectors from GET_COMM_PREFERRED_SETTINGS */
	if (dev->max_msix == 0 || dev->pdev->device == PMC_DEVICE_S6 || dev->sync_mode) {
		dev->max_msix = 1;
		dev->vector_cap = dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB;
#if (defined(AAC_DEBUG_INSTRUMENT_MSIX))
		printk(KERN_INFO "aacraid: dev->msi_enabled %d dev->msi %d dev->vector_cap %d dev->max_msix %d dev->scsi_host_ptr->can_queue %d\n",
			  dev->msi_enabled, dev->msi, dev->vector_cap, dev->max_msix, dev->scsi_host_ptr->can_queue);
#endif
		return;
	}

#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI))
#if ((!defined(CONFIG_XEN) || LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)) || \
     defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	/* Don't bother allocating more MSI-X vectors than cpus */
	msi_count = min(dev->max_msix,
		(unsigned int)num_online_cpus());

	dev->max_msix = msi_count;

	if (msi_count > AAC_MAX_MSIX)
		msi_count = AAC_MAX_MSIX;

	for (i = 0; i < msi_count; i++)
		dev->msixentry[i].entry = i;

	if (msi_count > 1 && pci_find_capability(dev->pdev, PCI_CAP_ID_MSIX)) {

		i = pci_enable_msix(dev->pdev, dev->msixentry, msi_count);
		/*
		 * Check how many MSIX vectors are allocated 
		 */
		if (i >= 0) {
			dev->msi_enabled = 1;
			if (i) {
				msi_count = i;
				if (pci_enable_msix(dev->pdev, dev->msixentry, msi_count)) {
					dev->msi_enabled = 0;
					printk(KERN_ERR "%s%d: MSIX not supported!! Will try MSI 0x%x.\n",
							dev->name, dev->id, i);
				}
			}
		} else {
			dev->msi_enabled = 0;
			printk(KERN_ERR "%s%d: MSIX not supported!! Will try MSI 0x%x.\n",
					dev->name, dev->id, i);
		}
	}

	if (!dev->msi_enabled) {
		msi_count = 1;
		i = !pci_enable_msi(dev->pdev);

		if (i) {
			dev->msi_enabled = 1;
			dev->msi = 1;
		}
		else
			printk(KERN_ERR "%s%d: MSI not supported!! Will try INTx 0x%x.\n",
					dev->name, dev->id, i);
	}
#endif
#endif

	if (!dev->msi_enabled)
		dev->max_msix = msi_count = 1;
	else {
		if (dev->max_msix > msi_count)
			dev->max_msix = msi_count;
	}
	if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE3 && dev->sa_firmware)
                dev->vector_cap = (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB);
	else
                dev->vector_cap = (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB) / msi_count;
        
#if (defined(AAC_DEBUG_INSTRUMENT_MSIX))
		printk(KERN_INFO "aacraid: dev->msi_enabled %d dev->msi %d dev->vector_cap %d dev->max_msix %d dev->scsi_host_ptr->can_queue %d\n",
			  dev->msi_enabled, dev->msi, dev->vector_cap, dev->max_msix, dev->scsi_host_ptr->can_queue);
#endif
}
