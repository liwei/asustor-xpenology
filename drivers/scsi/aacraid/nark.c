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
 *  nark.c
 *
 * Abstract: Hardware Device Interface for NEMER/ARK
 *
 */

#include <linux/pci.h>
#include <linux/blkdev.h>
#include <linux/version.h>	/* Needed for the following */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#include "scsi.h"
#include "hosts.h"
#else
#include <scsi/scsi_host.h>
#endif

#include "aacraid.h"

/**
 *	aac_nark_ioremap
 *	@size: mapping resize request
 *
 */
static int aac_nark_ioremap(struct aac_dev * dev, u32 size)
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
	dev->scsi_host_ptr->base = pci_resource_start(dev->pdev, 2);
	dev->regs.rx = ioremap((u64)pci_resource_start(dev->pdev, 0) |
	  ((u64)pci_resource_start(dev->pdev, 1) << 32),
	  sizeof(struct rx_registers) - sizeof(struct rx_inbound));
	dev->base = NULL;
	if (dev->regs.rx == NULL)
		return -1;
	dev->base = ioremap(dev->scsi_host_ptr->base, size);
	if (dev->base == NULL) {
		iounmap(dev->regs.rx);
		dev->regs.rx = NULL;
		return -1;
	}
	dev->IndexRegs = &((struct rx_registers __iomem *)dev->base)->IndexRegs;
	return 0;
}
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))

/**
 *	aac_nark_select_comm	-	Select communications method
 *	@dev: Adapter
 *	@comm: communications method
 */
int aac_nark_select_comm(struct aac_dev *dev, int comm)
{
	int ret = aac_rx_select_comm(dev, comm);
	dev->a_ops.adapter_build_sg = aac_build_sg_nark;
	return ret;
}
#endif

/**
 *	aac_nark_init	-	initialize an NEMER/ARK Split Bar card
 *	@dev: device to configure
 *
 */

int aac_nark_init(struct aac_dev * dev)
{
	/*
	 *	Fill in the function dispatch table.
	 */
	dev->a_ops.adapter_ioremap = aac_nark_ioremap;
#if (defined(INITFLAGS_APRE_SUPPORTED) && !defined(CONFIG_COMMUNITY_KERNEL))
	dev->a_ops.adapter_comm = aac_nark_select_comm;
#else
	dev->a_ops.adapter_comm = aac_rx_select_comm;
#endif

	return _aac_rx_init(dev);
}
