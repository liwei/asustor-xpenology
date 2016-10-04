/*
 *	Adaptec AAC series RAID controller driver
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
 *  frey.c
 *
 * Abstract: Hardware Device Interface for Cardinal Frey
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
#include <linux/delay.h>
#include <linux/version.h>	/* Needed for the following */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/time.h>
#include <linux/interrupt.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23))
#if (!defined(IRQ_NONE))
  typedef void irqreturn_t;
# define IRQ_HANDLED
# define IRQ_NONE
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,25))
#include <asm/semaphore.h>
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#include "scsi.h"
#include "hosts.h"
#else
#include <scsi/scsi_host.h>
#endif

#include "aacraid.h"

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x70)
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
# define AAC_DEBUG_PREAMBLE_SIZE 10
#else
# define AAC_DEBUG_PREAMBLE	
# define AAC_DEBUG_POSTAMBLE
# define AAC_DEBUG_PREAMBLE_SIZE 0
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static irqreturn_t aac_frey_intr_message(int irq, void *dev_id, struct pt_regs *regs)
#elif (defined(HAS_NEW_IRQ_HANDLER_T))
static irqreturn_t aac_frey_intr_message(void *dev_id)
#else
static irqreturn_t aac_frey_intr_message(int irq, void *dev_id)
#endif
{
	struct aac_dev *dev = dev_id;
	u32 Index = frey_readl0(dev, MUnit.OutboundQueue);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30)
	static unsigned empty_count = 0;
	if (nblank(fwprintf(x)) &&
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30) == 0x10)
	  ((Index == 0xFFFFFFFFL) || (!(Index & 0x00000002L) &&
	  (((Index >> 2) >= (dev->scsi_host_ptr->can_queue+AAC_NUM_MGT_FIB)) ||
	  !(dev->fibs[Index >> 2].hw_fib_va->header.XferState &
	  cpu_to_le32(NoResponseExpected | Async))))) &&
#else
	  dev->aif_thread &&
#endif
	  ((Index != 0xFFFFFFFFL) || (++empty_count < 3))) {
		unsigned long DebugFlags = dev->FwDebugFlags;
		dev->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30) == 0x30)
		if (!(Index & 0x00000002L) && ((Index >> 2) >=
		  (dev->scsi_host_ptr->can_queue+AAC_NUM_MGT_FIB))) {
			struct hw_fib * f = dev->fibs[Index >> 2].hw_fib_va;
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B, AAC_DEBUG_PREAMBLE
			  "irq%d Q=0x%X %u+%u+%u\n" AAC_DEBUG_POSTAMBLE,
			  irq, Index, le16_to_cpu(f->header.Command),
			  le32_to_cpu(((struct aac_query_mount *)f->data)->command),
			  le32_to_cpu(((struct aac_query_mount *)f->data)->type)));
		} else
#endif
		{
			fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
			  AAC_DEBUG_PREAMBLE "irq%d Q=0x%X\n"
			  AAC_DEBUG_POSTAMBLE, irq, Index));
		}
		dev->FwDebugFlags = DebugFlags;
	}
#endif
#endif
	if (unlikely(Index == 0xFFFFFFFFL))
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30)
	{
#endif
#endif
		Index = frey_readl0(dev, MUnit.OutboundQueue);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30)
		if (nblank(fwprintf(x))
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30) == 0x10)
		 && (!(Index & 0x00000002L) && (((Index >> 2) >=
		  (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB)) ||
		  !(dev->fibs[Index >> 2].hw_fib_va->header.XferState &
		  cpu_to_le32(NoResponseExpected | Async))))
#else
		 && dev->aif_thread
#endif
		) {
			unsigned long DebugFlags = dev->FwDebugFlags;
			dev->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30) == 0x30)
			if (!(Index & 0x00000002L) && ((Index >> 2) >=
			  (dev->scsi_host_ptr->can_queue+AAC_NUM_MGT_FIB))) {
				struct hw_fib * f = dev->fibs[
				  Index >> 2].hw_fib_va;
				fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  AAC_DEBUG_PREAMBLE " Q=0x%X %u+%u+%u\n"
				  AAC_DEBUG_POSTAMBLE, Index,
				  le16_to_cpu(f->header.Command),
				  le32_to_cpu(((struct aac_query_mount *)
				    f->data)->command),
				  le32_to_cpu(((struct aac_query_mount *)
				    f->data)->type)));
			} else
#endif
			{
				fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
				  AAC_DEBUG_PREAMBLE " Q=0x%X\n"
				  AAC_DEBUG_POSTAMBLE, Index));
			}
			dev->FwDebugFlags = DebugFlags;
		}
	}
#endif
#endif
	if (likely(Index != 0xFFFFFFFFL)) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30)
		if (nblank(fwprintf(x)))
			empty_count = 0;
#endif
#endif
		do {
			if (unlikely(aac_intr_normal(dev, Index))) {
				frey_writel0(dev, MUnit.OutboundQueue, Index);
				frey_writel1(dev, F0_Doorbell, DoorBellAdapterNormRespReady);
			}
			Index = frey_readl0(dev, MUnit.OutboundQueue);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30)
			if (nblank(fwprintf(x))
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30) == 0x10)
			 && !(Index & 0x00000002L) && (((Index >> 2) >=
			  (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB)) ||
			  !(dev->fibs[Index >> 2].hw_fib_va->header.XferState &
			  cpu_to_le32(NoResponseExpected | Async)))
#else
			 && dev->aif_thread
#endif
			) {
				unsigned long DebugFlags = dev->FwDebugFlags;
				dev->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30) == 0x30)
				if (!(Index & 0x00000002L) && ((Index >> 2) >=
				  (dev->scsi_host_ptr->can_queue +
				  AAC_NUM_MGT_FIB))) {
					struct hw_fib * f = dev->fibs[
					  Index >> 2].hw_fib_va;
					fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
					  AAC_DEBUG_PREAMBLE " Q=0x%X %u+%u+%u\n"
					  AAC_DEBUG_POSTAMBLE, Index,
					  le16_to_cpu(f->header.Command),
					  le32_to_cpu(
					    ((struct aac_query_mount *)f->data)
					      ->command),
					  le32_to_cpu(
					    ((struct aac_query_mount *)f->data)
					      ->type)));
				} else
#endif
				{
					fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B,
					  AAC_DEBUG_PREAMBLE " Q=0x%X\n"
					  AAC_DEBUG_POSTAMBLE, Index));
				}
				dev->FwDebugFlags = DebugFlags;
			}
#endif
#endif
		} while (Index != 0xFFFFFFFFL);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

/**
 *	aac_frey_disable_interrupt	-	Disable interrupts
 *	@dev: Adapter
 */

static void aac_frey_disable_interrupt(struct aac_dev *dev)
{
	frey_writel1(dev, PCIeF0_Int_Enable, 0x0000UL);
}

/**
 *	aac_frey_enable_interrupt_message	-	Enable interrupts
 *	@dev: Adapter
 */

static void aac_frey_enable_interrupt_message(struct aac_dev *dev)
{
	frey_writel1(dev, PCIeF0_Int_Enable, 0x1010UL);
}

/**
 *	frey_sync_cmd	-	send a command and wait
 *	@dev: Adapter
 *	@command: Command to execute
 *	@p1: first parameter
 *	@ret: adapter status
 *
 *	This routine will send a synchronous command to the adapter and wait 
 *	for its	completion.
 */

static int frey_sync_cmd(struct aac_dev *dev, u32 command,
	u32 p1, u32 p2, u32 p3, u32 p4, u32 p5, u32 p6,
	u32 *status, u32 * r1, u32 * r2, u32 * r3, u32 * r4)
{
	unsigned long start;
	int ok;
#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
	printk(KERN_INFO "frey_sync_cmd(%p,0x%lx,0x%lx,0x%lx,0x%lx,0x%lx,"
	  "0x%lx,0x%lx,%p,%p,%p,%p,%p)\n",
	  dev, command, p1, p2, p3, p4, p5, p6, status, r1, r2, r3, r4);
#endif
	/*
	 *	Write the command into Mailbox 0
	 */
	frey_writel0(dev, IndexRegs.Mailbox[0], command);
	/*
	 *	Write the parameters into Mailboxes 1 - 6
	 */
	frey_writel0(dev, IndexRegs.Mailbox[1], p1);
	frey_writel0(dev, IndexRegs.Mailbox[2], p2);
	frey_writel0(dev, IndexRegs.Mailbox[3], p3);
	frey_writel0(dev, IndexRegs.Mailbox[4], p4);
#if (defined(AAC_LM_SENSOR) && !defined(CONFIG_COMMUNITY_KERNEL))
	frey_writel0(dev, IndexRegs.Mailbox[5], p5);
	frey_writel0(dev, IndexRegs.Mailbox[6], p6);
#endif
#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
	printk(KERN_INFO "OutMaiboxes=%p="
	  "{0x%lx,0x%lx,0x%lx,0x%lx,0x%lx,0x%lx,0x%lx}\n",
	  &dev->IndexRegs->Mailbox[0],
	  frey_readl0(dev, IndexRegs.Mailbox[0]),
	  frey_readl0(dev, IndexRegs.Mailbox[1]),
	  frey_readl0(dev, IndexRegs.Mailbox[2]),
	  frey_readl0(dev, IndexRegs.Mailbox[3]),
	  frey_readl0(dev, IndexRegs.Mailbox[4]),
	  frey_readl0(dev, IndexRegs.Mailbox[5]),
	  frey_readl0(dev, IndexRegs.Mailbox[6]));
#endif
	/*
	 *	Clear the synch command doorbell to start on a clean slate.
	 */
	frey_writel1(dev, F0_Doorbell, OUTBOUNDDOORBELL_0);
	/*
	 *	Disable doorbell interrupts
	 */
	aac_adapter_disable_int(dev);
	/*
	 *	Force the completion of the mask register write before issuing
	 *	the interrupt.
	 */
	frey_readl1(dev, PCIeF0_Int_Enable);
	/*
	 *	Signal that there is a new synch command
	 */
	frey_writel1(dev, F0_To_CPU_Doorbell, INBOUNDDOORBELL_0);

	ok = 0;
	start = jiffies;

	/*
	 *	Wait up to 30 seconds
	 */
	while (time_before(jiffies, start+30*HZ)) 
	{
		udelay(5);	/* Delay 5 microseconds to let Mon960 get info. */
		/*
		 *	Mon960 will set doorbell0 bit when it has completed the command.
		 */
		if (frey_readl1(dev, F0_Doorbell) & OUTBOUNDDOORBELL_0) {
			/*
			 *	Clear the doorbell.
			 */
			frey_writel1(dev, F0_Doorbell, OUTBOUNDDOORBELL_0);
			ok = 1;
			break;
		}
		/*
		 *	Yield the processor in case we are slow 
		 */
		msleep(1);
	}
	if (unlikely(ok != 1)) {
		/*
		 *	Restore interrupt mask even though we timed out
		 */
		aac_adapter_enable_int(dev);
		return -ETIMEDOUT;
	}
	/*
	 *	Pull the synch status from Mailbox 0.
	 */
	if (status)
		*status = frey_readl0(dev, IndexRegs.Mailbox[0]);
	if (r1)
		*r1 = frey_readl0(dev, IndexRegs.Mailbox[1]);
	if (r2)
		*r2 = frey_readl0(dev, IndexRegs.Mailbox[2]);
	if (r3)
		*r3 = frey_readl0(dev, IndexRegs.Mailbox[3]);
	if (r4)
		*r4 = frey_readl0(dev, IndexRegs.Mailbox[4]);
#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
	printk(KERN_INFO "InMaiboxes={0x%lx,0x%lx,0x%lx,0x%lx,0x%lx}\n",
	  frey_readl0(dev, IndexRegs.Mailbox[0]),
	  frey_readl0(dev, IndexRegs.Mailbox[1]),
	  frey_readl0(dev, IndexRegs.Mailbox[2]),
	  frey_readl0(dev, IndexRegs.Mailbox[3]),
	  frey_readl0(dev, IndexRegs.Mailbox[4]));
#endif
	/*
	 *	Clear the synch command doorbell.
	 */
	frey_writel1(dev, F0_Doorbell, OUTBOUNDDOORBELL_0);
	/*
	 *	Restore interrupt mask
	 */
	aac_adapter_enable_int(dev);
	return 0;

}

/**
 *	aac_frey_interrupt_adapter	-	interrupt adapter
 *	@dev: Adapter
 *
 *	Send an interrupt to the i960 and breakpoint it.
 */

static void aac_frey_interrupt_adapter(struct aac_dev *dev)
{
	frey_sync_cmd(dev, BREAKPOINT_REQUEST, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);
}

/**
 *	aac_frey_notify_adapter		-	send an event to the adapter
 *	@dev: Adapter
 *	@event: Event to send
 *
 *	Notify the i960 that something it probably cares about has
 *	happened.
 */

static void aac_frey_notify_adapter(struct aac_dev *dev, u32 event)
{
	switch (event) {

	case AdapNormCmdQue:
		frey_writel1(dev, F0_To_CPU_Doorbell, INBOUNDDOORBELL_1);
		break;
	case HostNormRespNotFull:
		frey_writel1(dev, F0_To_CPU_Doorbell, INBOUNDDOORBELL_4);
		break;
	case AdapNormRespQue:
		frey_writel1(dev, F0_To_CPU_Doorbell, INBOUNDDOORBELL_2);
		break;
	case HostNormCmdNotFull:
		frey_writel1(dev, F0_To_CPU_Doorbell, INBOUNDDOORBELL_3);
		break;
	case HostShutdown:
//		frey_sync_cmd(dev, HOST_CRASHING, 0, 0, 0, 0, 0, 0,
//		  NULL, NULL, NULL, NULL, NULL);
		break;
	case FastIo:
		frey_writel1(dev, F0_To_CPU_Doorbell, INBOUNDDOORBELL_6);
		break;
	case AdapPrintfDone:
		frey_writel1(dev, F0_To_CPU_Doorbell, INBOUNDDOORBELL_5);
		break;
	default:
		BUG();
		break;
	}
}

/**
 *	aac_frey_start_adapter		-	activate adapter
 *	@dev:	Adapter
 *
 *	Start up processing on an i960 based AAC adapter
 */

static void aac_frey_start_adapter(struct aac_dev *dev)
{
	struct aac_init *init;

	init = dev->init;
	init->HostElapsedSeconds = cpu_to_le32(get_seconds());
	// We can only use a 32 bit address here
	frey_sync_cmd(dev, INIT_STRUCT_BASE_ADDRESS, (u32)(ulong)dev->init_pa,
	  0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL);
	adbg_init(dev,KERN_INFO,"INIT_STRUCT_BASE_ADDRESS=0x%lx\n",
		  	(unsigned long)dev->init_pa);
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	/*
	 * On some cards, without a wait here after the INIT_STRUCT_BASE_ADDRESS
	 * command, the ContainerCommand in AacHba_ProbeContainers() fails to
	 * report the container.
	 * The wait time was determined by trial-and-error.
	 */
	 mdelay(500);
#endif
}

/**
 *	aac_frey_check_health
 *	@dev: device to check if healthy
 *
 *	Will attempt to determine if the specified adapter is alive and
 *	capable of handling requests, returning 0 if alive.
 */
static int aac_frey_check_health(struct aac_dev *dev)
{
	u32 status = frey_readl0(dev, OMR0);

	/*
	 *	Check to see if the board failed any self tests.
	 */
	if (unlikely(status & SELF_TEST_FAILED))
		return -1;
	/*
	 *	Check to see if the board panic'd.
	 */
	if (unlikely(status & KERNEL_PANIC)) {
		char * buffer;
		struct POSTSTATUS {
			__le32 Post_Command;
			__le32 Post_Address;
		} * post;
		dma_addr_t paddr, baddr;
		int ret;

		if (likely((status & 0xFF000000L) == 0xBC000000L))
			return (status >> 16) & 0xFF;
#if (((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + __GNUC_PATCHLEVEL__) <= 400002)
		baddr = 0;
#endif
		buffer = pci_alloc_consistent(dev->pdev, 512, &baddr);
		ret = -2;
		if (unlikely(buffer == NULL))
			return ret;
#if (((__GNUC__ * 10000) + (__GNUC_MINOR__ * 100) + __GNUC_PATCHLEVEL__) <= 400002)
		paddr = 0;
#endif
		post = pci_alloc_consistent(dev->pdev,
		  sizeof(struct POSTSTATUS), &paddr);
		if (unlikely(post == NULL)) {
			pci_free_consistent(dev->pdev, 512, buffer, baddr);
			return ret;
		}
		memset(buffer, 0, 512);
		post->Post_Command = cpu_to_le32(COMMAND_POST_RESULTS);
		post->Post_Address = cpu_to_le32(baddr);
		frey_writel0(dev, IMR0, paddr);
		frey_sync_cmd(dev, COMMAND_POST_RESULTS, baddr, 0, 0, 0, 0, 0,
		  NULL, NULL, NULL, NULL, NULL);
		pci_free_consistent(dev->pdev, sizeof(struct POSTSTATUS),
		  post, paddr);
		if (likely((buffer[0] == '0') && ((buffer[1] == 'x') || (buffer[1] == 'X')))) {
			ret = (buffer[2] <= '9') ? (buffer[2] - '0') : (buffer[2] - 'A' + 10);
			ret <<= 4;
			ret += (buffer[3] <= '9') ? (buffer[3] - '0') : (buffer[3] - 'A' + 10);
		}
		pci_free_consistent(dev->pdev, 512, buffer, baddr);
		return ret;
	}
	/*
	 *	Wait for the adapter to be up and running.
	 */
	if (unlikely(!(status & KERNEL_UP_AND_RUNNING)))
		return -3;
	/*
	 *	Everything is OK
	 */
	return 0;
}

/**
 *	aac_frey_deliver_message
 *	@fib: fib to issue
 *
 *	Will send a fib, returning 0 if successful.
 */
static int aac_frey_deliver_message(struct fib * fib)
{
	struct aac_dev *dev = fib->dev;
	struct aac_queue *q = &dev->queues->queue[AdapNormCmdQueue];
	unsigned long qflags;
	u32 Index;
	u64 addr;
	volatile void __iomem *device;

	unsigned long count = 10000000L; /* 50 seconds */
	spin_lock_irqsave(q->lock, qflags);
	q->numpending++;
	spin_unlock_irqrestore(q->lock, qflags);
	for(;;) {
		Index = frey_readl0(dev, MUnit.InboundQueue);
		if (unlikely(Index == 0xFFFFFFFFL))
			Index = frey_readl0(dev, MUnit.InboundQueue);
		if (likely(Index != 0xFFFFFFFFL))
			break;
		if (--count == 0) {
			spin_lock_irqsave(q->lock, qflags);
			q->numpending--;
			spin_unlock_irqrestore(q->lock, qflags);
#			if (defined(AAC_DEBUG_INSTRUMENT_FIB))
				printk(KERN_INFO "aac_fib_send: message unit full\n");
#			endif
			return -ETIMEDOUT;
		}
		udelay(5);
	}
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x70)
	if (nblank(fwprintf(x))
#if ((AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30) == 0x10)
	 && !(fib->hw_fib_va->header.XferState &
	  cpu_to_le32(NoResponseExpected | Async))
#else
	 && dev->aif_thread
#endif
	) {
		unsigned long DebugFlags = dev->FwDebugFlags;
		dev->FwDebugFlags |= FW_DEBUG_FLAGS_NO_HEADERS_B;
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x30)
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B, AAC_DEBUG_PREAMBLE
		  "send Q=0x%X I=0x%X %u+%u+%u\n" AAC_DEBUG_POSTAMBLE,
		  ((int)(fib - dev->fibs)) << 2, Index,
		  le16_to_cpu(fib->hw_fib_va->header.Command),
		  le32_to_cpu(((struct aac_query_mount *)
		    fib->hw_fib_va->data)->command),
		  le32_to_cpu(((struct aac_query_mount *)
		    fib->hw_fib_va->data)->type)));
#endif
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x40)
		{
			u8 * p = (u8 *)fib->hw_fib_va;
			unsigned len = le16_to_cpu(fib->hw_fib_va->header.Size);
			char buffer[80-AAC_DEBUG_PREAMBLE_SIZE];
			char * cp = buffer;

			strcpy(cp, "FIB=");
			cp += 4;
			while (len > 0) {
				if (cp >= &buffer[sizeof(buffer)-4]) {
					fwprintf((dev,
					  HBA_FLAGS_DBG_FW_PRINT_B,
					  AAC_DEBUG_PREAMBLE "%s\n"
					  AAC_DEBUG_POSTAMBLE,
					  buffer));
					strcpy(cp = buffer, "    ");
					cp += 4;
				}
				sprintf (cp, "%02x ", *(p++));
				cp += strlen(cp);
				--len;
			}
			if (cp > &buffer[4]) {
				fwprintf((dev,
				  HBA_FLAGS_DBG_FW_PRINT_B,
				  AAC_DEBUG_PREAMBLE "%s\n"
				  AAC_DEBUG_POSTAMBLE, buffer));
			}
		}
#endif
		dev->FwDebugFlags = DebugFlags;
	}
#endif
#endif
	device = dev->base + Index;
	addr = fib->hw_fib_pa;
	writel((u32)(addr & 0xffffffff), device);
	device += sizeof(u32);
	writel((u32)(addr >> 32), device);
	device += sizeof(u32);
	writel(le16_to_cpu(fib->hw_fib_va->header.Size), device);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB))
#if (AAC_DEBUG_INSTRUMENT_IOCTL_SENDFIB & 0x70)
//qflags=readl(device);
#endif
#endif
	frey_writel0(dev, MUnit.InboundQueue, Index);
	return 0;
}

/**
 *	aac_frey_ioremap
 *	@size: mapping resize request
 *
 */
static int aac_frey_ioremap(struct aac_dev * dev, u32 size)
{
	if (!size) {
		iounmap(dev->regs.rx);
		dev->regs.rx = NULL;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,9))
		iounmap((void __iomem *)dev->base);
#else
		iounmap(dev->base);
#endif
		dev->base = NULL;
		return 0;
	}
	dev->regs.frey.bar1 = 
        ioremap(pci_resource_start(dev->pdev, 2), AAC_MIN_FREY_BAR1_SIZE);
	dev->base = NULL;
	if (dev->regs.frey.bar1 == NULL)
		return -1;
	dev->base = dev->regs.frey.bar0 = ioremap(dev->scsi_host_ptr->base, size);
	if (dev->base == NULL) {
		iounmap(dev->regs.frey.bar1);
		dev->regs.frey.bar1 = NULL;
		return -1;
	}
    /* IndexRegs not really needed, only for debugging purposes */
	dev->IndexRegs = &((struct frey_registers __iomem *)dev->base)->IndexRegs;
	return 0;
}

static int aac_frey_restart_adapter(struct aac_dev *dev, int bled)
{
	u32 var;

	if (!(dev->supplement_adapter_info.SupportedOptions2 &
	  AAC_OPTION_MU_RESET) || (bled >= 0) || (bled == -2)) {
		if (bled)
			printk(KERN_ERR "%s%d: adapter kernel panic'd %x.\n",
				dev->name, dev->id, bled);
		else {
			bled = aac_adapter_sync_cmd(dev, IOP_RESET_ALWAYS,
			  0, 0, 0, 0, 0, 0, &var, NULL, NULL, NULL, NULL);
			if (!bled && (var != 0x00000001) && (var != 0x3803000F))
				bled = -EINVAL;
		}
		if (bled && (bled != -ETIMEDOUT))
			bled = aac_adapter_sync_cmd(dev, IOP_RESET,
			  0, 0, 0, 0, 0, 0, &var, NULL, NULL, NULL, NULL);

		if (bled && (bled != -ETIMEDOUT))
			return -EINVAL;
	}
	if (frey_readl0(dev, OMR0) & KERNEL_PANIC)
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	{
		if (var == 10)
			printk(KERN_INFO "IOP_RESET disabled\n");
#endif
		return -ENODEV;
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	}
#endif
	if (startup_timeout < 300)
		startup_timeout = 300;
	return 0;
}

/**
 *	aac_frey_select_comm	-	Select communications method
 *	@dev: Adapter
 *	@comm: communications method
 */
int aac_frey_select_comm(struct aac_dev *dev, int comm)
{
	switch (comm) {
	case AAC_COMM_MESSAGE:
		dev->a_ops.adapter_enable_int = aac_frey_enable_interrupt_message;
		dev->a_ops.adapter_intr = aac_frey_intr_message;
		dev->a_ops.adapter_deliver = aac_frey_deliver_message;
		break;
	default:
		return 1;
	}
	return 0;
}

/**
 *  aac_frey_init	-	initialize an Cardinal Frey Bar card
 *  @dev: device to configure
 *
 */

int aac_frey_init(struct aac_dev * dev)
{
	unsigned long start;
	unsigned long status;
	int restart = 0;
	int instance = dev->id;
	const char * name = dev->name;
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)) && !defined(HAS_RESET_DEVICES))
#	define reset_devices aac_reset_devices
#endif

	dev->a_ops.adapter_ioremap = aac_frey_ioremap;
	dev->a_ops.adapter_comm = aac_frey_select_comm;

	dev->base_size = AAC_MIN_FREY_BAR0_SIZE;
	if (aac_adapter_ioremap(dev, dev->base_size)) {
		printk(KERN_WARNING "%s: unable to map adapter.\n", name);
		goto error_iounmap;
	}

	/* Failure to reset here is an option ... */
	dev->a_ops.adapter_sync_cmd = frey_sync_cmd;
	dev->a_ops.adapter_enable_int = aac_frey_disable_interrupt;
	if ((aac_reset_devices || reset_devices) &&
	  !aac_frey_restart_adapter(dev, 0))
		/* Make sure the Hardware FIFO is empty */
		while ((++restart < 512) &&
		  (frey_readl0(dev, MUnit.OutboundQueue) != 0xFFFFFFFFL));
	/*
	 *	Check to see if the board panic'd while booting.
	 */
	status = frey_readl0(dev, OMR0);
	if (status & KERNEL_PANIC) {
		if (aac_frey_restart_adapter(dev, aac_frey_check_health(dev)))
			goto error_iounmap;
		++restart;
	}
	/*
	 *	Check to see if the board failed any self tests.
	 */
	status = frey_readl0(dev, OMR0);
	if (status & SELF_TEST_FAILED) {
		printk(KERN_ERR "%s%d: adapter self-test failed.\n", dev->name, instance);
		goto error_iounmap;
	}
	/*
	 *	Check to see if the monitor panic'd while booting.
	 */
	if (status & MONITOR_PANIC) {
		printk(KERN_ERR "%s%d: adapter monitor panic.\n", dev->name, instance);
		goto error_iounmap;
	}
	start = jiffies;
	/*
	 *	Wait for the adapter to be up and running. Wait up to 3 minutes
	 */
	while (!((status = frey_readl0(dev, OMR0)) & KERNEL_UP_AND_RUNNING))
	{
		if ((restart &&
		  (status & (KERNEL_PANIC|SELF_TEST_FAILED|MONITOR_PANIC))) ||
		  time_after(jiffies, start+HZ*startup_timeout)) {
			printk(KERN_ERR "%s%d: adapter kernel failed to start, init status = %lx.\n", 
					dev->name, instance, status);
			goto error_iounmap;
		}
		if (!restart &&
		  ((status & (KERNEL_PANIC|SELF_TEST_FAILED|MONITOR_PANIC)) ||
		  time_after(jiffies, start + HZ *
		  ((startup_timeout > 60)
		    ? (startup_timeout - 60)
		    : (startup_timeout / 2))))) {
			if (likely(!aac_frey_restart_adapter(dev, aac_frey_check_health(dev))))
				start = jiffies;
			++restart;
		}
		msleep(1);
	}
	if (restart && aac_commit)
		aac_commit = 1;
	/*
	 *	Fill in the common function dispatch table.
	 */
	dev->a_ops.adapter_interrupt = aac_frey_interrupt_adapter;
	dev->a_ops.adapter_disable_int = aac_frey_disable_interrupt;
	dev->a_ops.adapter_notify = aac_frey_notify_adapter;
	dev->a_ops.adapter_sync_cmd = frey_sync_cmd;
	dev->a_ops.adapter_check_health = aac_frey_check_health;
	dev->a_ops.adapter_restart = aac_frey_restart_adapter;

	/*
	 *	First clear out all interrupts.  Then enable the one's that we
	 *	can handle.
	 */
	aac_adapter_comm(dev, AAC_COMM_MESSAGE);
	aac_adapter_disable_int(dev);
	frey_writel1(dev, F0_Doorbell, 0xffffffff);
	aac_adapter_enable_int(dev);

	if (aac_init_adapter(dev) == NULL)
		goto error_iounmap;
	if (dev->comm_interface != AAC_COMM_MESSAGE)
		goto error_iounmap;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI))
	dev->msi = aac_msi && !pci_enable_msi(dev->pdev);
#endif
	if (request_irq(dev->pdev->irq, dev->a_ops.adapter_intr,
			IRQF_SHARED|IRQF_DISABLED, "aacraid", dev) < 0) {
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_DISABLE_MSI))
		if (dev->msi)
			pci_disable_msi(dev->pdev);
#endif
		printk(KERN_ERR "%s%d: Interrupt unavailable.\n",
			name, instance);
		goto error_iounmap;
	}
	aac_adapter_enable_int(dev);
	/*
	 *	Tell the adapter that all is configured, and it can
	 * start accepting requests
	 */
	aac_frey_start_adapter(dev);

	return 0;

error_iounmap:

	return -1;
}
