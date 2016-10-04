/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2004-2007 Adaptec, Inc
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
 *   csmi.c
 *
 * Abstract: All CSMI IOCTL processing is handled here.
 */

/*
 * Include Files
 */

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/version.h> /* For the following test */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <asm/uaccess.h> /* For copy_from_user()/copy_to_user() definitions */
#include <linux/slab.h> /* For kmalloc()/kfree() definitions */
#include "aacraid.h"
#include "fwdebug.h"
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# include "scsi.h"
# include "hosts.h"
#else
# include <scsi/scsi.h>
# include <scsi/scsi_host.h>
//# include <linux/pci.h>
# include <linux/dma-mapping.h>
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
#include <linux/syscalls.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
#include <linux/ioctl32.h>
#endif
#endif
#if ((KERNEL_VERSION(2,4,19) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,21)))
# include <asm-x86_64/ioctl32.h>
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# include <asm/ioctl32.h>
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
# include <linux/ioctl32.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# include <asm/uaccess.h>
#endif
#endif

#if (defined(AAC_CSMI))

#include "csmi.h"


/*
 * Routine Description:
 *	This routine will verify that the *ppHeader is big enough
 *	for the expected CSMI IOCTL buffer.
 * Return Value:
 *	ppHeader
 *	0 - Success ppHeader set up with successful completion code
 *	!0 - CSMI_SAS_STATUS_INVALID_PARAMETER as the ReturnCode.
 */
static int
aac_VerifyCSMIBuffer(
	struct aac_dev ** pDev,
	void __user * arg,
	unsigned long csmiBufferSizeToVerify,
	PIOCTL_HEADER * ppHeader)
{
	u32 Length;
	int Rtnval;
	struct aac_dev * dev = *pDev;
	extern struct list_head aac_devices; /* in linit.c */

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev, HBA_FLAGS_DBG_FUNCTION_ENTRY_B,
	  "aac_VerifyCSMIBuffer: Enter"));
#endif

	*ppHeader = (PIOCTL_HEADER)NULL;

	if (copy_from_user((void *)&Length,
	  (void __user *)&((PIOCTL_HEADER)arg)->Length, sizeof(u32))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev, HBA_FLAGS_DBG_ERROR_B,
		  "aac_VerifyCSMIBuffer: Acquire Length Failure"));
#endif
		Length = CSMI_SAS_STATUS_INVALID_PARAMETER;
		/* Will msot probably fail */
		Rtnval = copy_to_user(
		  (void __user *)&((PIOCTL_HEADER)arg)->ReturnCode,
		  (void *)&Length, sizeof(u32));
		Rtnval = -EFAULT;
	} else if ((Length < sizeof(IOCTL_HEADER))
	 || (Length < csmiBufferSizeToVerify)
	 || (csmiBufferSizeToVerify < sizeof(IOCTL_HEADER))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev, HBA_FLAGS_DBG_ERROR_B,
		  "aac_VerifyCSMIBuffer:"
		  " sizeof(IOCTL_HEADER)=%u, Length=%u, MinPacketLength=%u",
		  sizeof(IOCTL_HEADER), Length, csmiBufferSizeToVerify));
#endif
		Length = CSMI_SAS_STATUS_INVALID_PARAMETER;
		if (copy_to_user(
		  (void __user *)&((PIOCTL_HEADER)arg)->ReturnCode,
		  (void *)&Length, sizeof(u32)))
			Rtnval = -EFAULT;
		else
			Rtnval = -EINVAL;
	} else if (!(*ppHeader = kmalloc(Length, GFP_KERNEL))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev, HBA_FLAGS_DBG_ERROR_B,
		  "aac_VerifyCSMIBuffer: Acquire Memory %u Failure",
		  Length));
#endif
		Length = CSMI_SAS_STATUS_INVALID_PARAMETER;
		if (copy_to_user(
		  (void __user *)&((PIOCTL_HEADER)arg)->ReturnCode,
		  (void *)&Length, sizeof(u32)))
			Rtnval = -EFAULT;
		else
			Rtnval = -ENOMEM;
	} else if (copy_from_user((void *)*ppHeader, arg, Length)) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev, HBA_FLAGS_DBG_ERROR_B,
		  "aac_VerifyCSMIBuffer: Acquire Content Failure"));
#endif
		kfree(*ppHeader);
		*ppHeader = NULL;
		Length = CSMI_SAS_STATUS_INVALID_PARAMETER;
		/* Will most probably fail */
		Rtnval = copy_to_user(
		  (void __user *)&((PIOCTL_HEADER)arg)->ReturnCode,
		  (void *)&Length, sizeof(u32));
		Rtnval = -EFAULT;
	} else {
		struct aac_dev * found = (struct aac_dev *)NULL;
		list_for_each_entry(dev, &aac_devices, entry)
			if (dev->id == (*ppHeader)->IOControllerNumber) {
				found = dev;
				break;
			}
		dev = found;
		if (dev == (struct aac_dev *)NULL) {
			dev = *pDev; /* Return to original host */
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
			fwprintf((dev, HBA_FLAGS_DBG_ERROR_B,
			  "aac_VerifyCSMIBuffer: Acquire %d Indexed Controller Failure",
			  (*ppHeader)->IOControllerNumber));
#endif
			kfree(*ppHeader);
			*ppHeader = NULL;
			Length = CSMI_SAS_STATUS_INVALID_PARAMETER;
			if (copy_to_user(
			  (void __user *)&((PIOCTL_HEADER)arg)->ReturnCode,
			  (void *)&Length, sizeof(u32)))
				Rtnval = -EFAULT;
			else
				Rtnval = -EINVAL;
		} else {
			(*ppHeader)->ReturnCode = CSMI_SAS_STATUS_SUCCESS;
			*pDev = dev;
			Rtnval = 0;
		}
	}

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev, HBA_FLAGS_DBG_FUNCTION_EXIT_B,
	  "aac_VerifyCSMIBuffer: Exit, ReturnValue=%d",Rtnval));
#endif

	return Rtnval;

}


/*
 * Routine Description:
 *	This routine will close the *ppHeader.
 * Return Value:
 *	0 - Success
 *	!0 - Failure
 */
static inline int
aac_CloseCSMIBuffer(
	struct aac_dev * dev,
	void __user * arg,
	PIOCTL_HEADER pHeader)
{
	int Rtnval = 0;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev, HBA_FLAGS_DBG_FUNCTION_ENTRY_B,
	  "aac_CloseCSMIBuffer: Enter"));
#endif

	if (pHeader) {
		if (copy_to_user(arg, (void *)pHeader, pHeader->Length))
			Rtnval = -EFAULT;
		kfree (pHeader);
	}

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev, HBA_FLAGS_DBG_FUNCTION_EXIT_B,
	  "aac_CloseCSMIBuffer: Exit, ReturnValue=%d",Rtnval));
#endif

	return Rtnval;

}

typedef struct aac_bus_info DIOCTL;
typedef DIOCTL * PDIOCTL;
/* IOCTL Functions */
#define CsmiGetPhyInfo		0x0070
#define CsmiSataSignature	0x0071

typedef struct {
	u32	Status;		/* ST_OK */
	u32	ObjType;	
	u32	MethodId;	/* unused */
	u32	ObjectId;	/* unused */
	u32	CtlCmd;		/* unused */
} DIOCTLRESPONSE;
typedef DIOCTLRESPONSE * PDIOCTLRESPONSE;

#define EnhancedGetBusInfo	0x0000000C
#define SCSI_MAX_PORTS	10
#define CSS_BUS_TYPE_SATA 11
#define CSS_BUS_TYPE_SAS	12
typedef struct aac_enhanced_bus_info_response {
	struct aac_bus_info_response BusInfo;
	/* Enhancements */
	u32	Version;
	u32	BusType[SCSI_MAX_PORTS];
	u8	NumPortsMapped[SCSI_MAX_PORTS];
	u8	ReservedPad0[2];
	u32	Reserved[17];
} ENHANCED_GBI_CSS;

/*
 * Routine Description:
 *	This routine is called to request the version information for the
 *	hardware, firmware, and boot BIOS associated with a storage controller.
 *
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMIGetControllerConfig(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_CNTLR_CONFIG_BUFFER pControllerConfigBuffer;
	PDIOCTL pIoctlInfo;
	ENHANCED_GBI_CSS * EnhancedBusInfo;
	struct fib * fibptr;
	int status;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetControllerConfig: Enter"));
#endif

	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_CNTLR_CONFIG_BUFFER),
	  (PIOCTL_HEADER *)&pControllerConfigBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMISTPPassThru: Exit, ReturnValue = %d",
		  Rtnval));
#endif
		return Rtnval;
	}

	pControllerConfigBuffer->Configuration.uBaseIoAddress = 0;
	pControllerConfigBuffer->Configuration.BaseMemoryAddress.uHighPart
	  = ((u64)dev->scsi_host_ptr->base) >> 32;
	pControllerConfigBuffer->Configuration.BaseMemoryAddress.uLowPart
	  = dev->scsi_host_ptr->base & 0xffffffff;
	pControllerConfigBuffer->Configuration.uBoardID
	  = (dev->pdev->subsystem_device << 16)
	  + dev->pdev->subsystem_vendor;
	/*
	 * Slot number can be pulled from
	 * dev->supplement_adapter_info->SlotNumber in later versions of
	 * the firmware else we could choose to take Linux PCI device slot
	 * number PCI_SLOT(dev->pdev->devfn) instead?
	 */
	if ((dev->supplement_adapter_info.Version < AAC_SIS_VERSION_V3)
	 || (dev->supplement_adapter_info.SlotNumber == AAC_SIS_SLOT_UNKNOWN)) {
		pControllerConfigBuffer->Configuration.usSlotNumber
		  = SLOT_NUMBER_UNKNOWN;
	} else {
		pControllerConfigBuffer->Configuration.usSlotNumber
		 = dev->supplement_adapter_info.SlotNumber;
	}
	pControllerConfigBuffer->Configuration.bControllerClass
	  = CSMI_SAS_CNTLR_CLASS_HBA;
	pControllerConfigBuffer->Configuration.bIoBusType
	  = CSMI_SAS_BUS_TYPE_PCI;
	pControllerConfigBuffer->Configuration.BusAddress.PciAddress.bBusNumber
	  = dev->pdev->bus->number;
	pControllerConfigBuffer->Configuration.BusAddress.PciAddress.bDeviceNumber
	  = PCI_SLOT(dev->pdev->devfn);
	pControllerConfigBuffer->Configuration.BusAddress.PciAddress.bFunctionNumber
	  = PCI_FUNC(dev->pdev->devfn);
	pControllerConfigBuffer->Configuration.szSerialNumber[0] = '\0';
	(void)aac_show_serial_number(shost_to_class(dev->scsi_host_ptr),
	  pControllerConfigBuffer->Configuration.szSerialNumber);
	{
		char * cp = strchr(pControllerConfigBuffer->Configuration.szSerialNumber, '\n');
		if (cp)
			*cp = '\0';
	}
	/* Get Bus Type */
	fibptr = aac_fib_alloc(dev);
	if (fibptr == NULL) {
		pControllerConfigBuffer->Configuration.uControllerFlags
		  = CSMI_SAS_CNTLR_SATA_RAID;
	} else {
		aac_fib_init(fibptr);

		pIoctlInfo = (PDIOCTL) fib_data(fibptr);
		pIoctlInfo->Command = cpu_to_le32(VM_Ioctl);
		pIoctlInfo->ObjType = cpu_to_le32(FT_DRIVE);
		pIoctlInfo->MethodId = cpu_to_le32(1);
		pIoctlInfo->ObjectId = 0;
		pIoctlInfo->CtlCmd = cpu_to_le32(EnhancedGetBusInfo);

		status = aac_fib_send(ContainerCommand, fibptr,
		  sizeof(*EnhancedBusInfo),
		  FsaNormal, 1, 1, NULL, NULL);
	
		aac_fib_complete(fibptr);
	
		EnhancedBusInfo = (struct aac_enhanced_bus_info_response *) pIoctlInfo;
	
		if (status < 0) {
			pControllerConfigBuffer->Configuration.uControllerFlags
			  = CSMI_SAS_CNTLR_SATA_RAID;
		} else switch (EnhancedBusInfo->BusType[0]) {
		case CSS_BUS_TYPE_SATA:
			pControllerConfigBuffer->Configuration.uControllerFlags
			  = CSMI_SAS_CNTLR_SATA_RAID;
			break;
		case CSS_BUS_TYPE_SAS:
			pControllerConfigBuffer->Configuration.uControllerFlags
			  = CSMI_SAS_CNTLR_SAS_RAID;
			break;
		default:
			pControllerConfigBuffer->Configuration.uControllerFlags
			  = 0;
			break;
		}
		aac_fib_free(fibptr);
	}

	pControllerConfigBuffer->Configuration.usBIOSBuildRevision
	  = cpu_to_le16(le32_to_cpu(dev->adapter_info.biosbuild));
	pControllerConfigBuffer->Configuration.usBIOSMajorRevision
	  = cpu_to_le16(le32_to_cpu(dev->adapter_info.biosrev) >> 24);
	pControllerConfigBuffer->Configuration.usBIOSMinorRevision
	  = cpu_to_le16((le32_to_cpu(dev->adapter_info.biosrev) >> 16) & 0xff);
	pControllerConfigBuffer->Configuration.usBIOSReleaseRevision
	  = cpu_to_le16(le32_to_cpu(dev->adapter_info.biosrev) & 0xff);
	pControllerConfigBuffer->Configuration.usBuildRevision
	  = cpu_to_le16(le32_to_cpu(dev->adapter_info.kernelbuild));
	pControllerConfigBuffer->Configuration.usMajorRevision
	  = cpu_to_le16(le32_to_cpu(dev->adapter_info.kernelrev) >> 24);
	pControllerConfigBuffer->Configuration.usMinorRevision
	  = cpu_to_le16((le32_to_cpu(dev->adapter_info.kernelrev) >> 16) & 0xff);
	pControllerConfigBuffer->Configuration.usReleaseRevision
	  = cpu_to_le16(le32_to_cpu(dev->adapter_info.kernelrev) & 0xff);
	pControllerConfigBuffer->Configuration.usRromBIOSBuildRevision = 0;
	pControllerConfigBuffer->Configuration.usRromBIOSMajorRevision = 0;
	pControllerConfigBuffer->Configuration.usRromBIOSMinorRevision = 0;
	pControllerConfigBuffer->Configuration.usRromBIOSReleaseRevision = 0;
	pControllerConfigBuffer->Configuration.usRromBuildRevision = 0;
	pControllerConfigBuffer->Configuration.usRromMajorRevision = 0;
	pControllerConfigBuffer->Configuration.usRromMinorRevision = 0;
	pControllerConfigBuffer->Configuration.usRromReleaseRevision = 0;

	Rtnval = aac_CloseCSMIBuffer(dev, arg,
	  (PIOCTL_HEADER)pControllerConfigBuffer);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetControllerConfig: Exit, ReturnValue=%d, ReturnCode=%x",
	  Rtnval, pControllerConfigBuffer->IoctlHeader.ReturnCode));
#endif

	return Rtnval;

}


/*
 * Routine Description:
 *	This routine is called to request the current status of the controller.
 *
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS.
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMIGetControllerStatus(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_CNTLR_STATUS_BUFFER pStatusBuffer;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetControllerStatus: Enter"));
#endif

	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_CNTLR_STATUS_BUFFER),
	  (PIOCTL_HEADER *)&pStatusBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetControllerStatus: Exit, ReturnValue=%d",
		  Rtnval));
#endif
		return Rtnval;
	}

	/*
	 * Determine and set adapter state
	 */
	switch (aac_adapter_check_health(dev)) {
	case 0:
		pStatusBuffer->Status.uStatus = CSMI_SAS_CNTLR_STATUS_GOOD;
		break;
	case -1:
	case -2:
	case -3:
		pStatusBuffer->Status.uStatus = CSMI_SAS_CNTLR_STATUS_FAILED;
		break;
	default:
		pStatusBuffer->Status.uStatus = CSMI_SAS_CNTLR_STATUS_OFFLINE;
		pStatusBuffer->Status.uOfflineReason
		  = CSMI_SAS_OFFLINE_REASON_NO_REASON;
	}

	Rtnval = aac_CloseCSMIBuffer(dev, arg, (PIOCTL_HEADER)pStatusBuffer);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetControllerStatus: Exit, ReturnValue=%d, ReturnCode=%x",
	  Rtnval, pStatusBuffer->IoctlHeader.ReturnCode));
#endif

	return Rtnval;

}


/*
 * Routine Description:
 *	This routine is called to request information for a specified RAID set
 *	on a controller that supports RAID.
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMIGetRAIDConfig(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_RAID_CONFIG_BUFFER pRaidConfigBuffer;
	typedef struct {
		u32	command;
		u32	type;
		u32	cid;
		u32	parm1;
		u32	parm2;
		u32	uid;
		u32	offset;
		u32	parm5;
	} CONTAINER;
	CONTAINER * ct;
#	define CT_PACKET_SIZE (sizeof(((struct hw_fib *)NULL)->data)-(sizeof(u32)*12))
#	define CT_CONTINUE_DATA		83
#	define CT_STOP_DATA		84
#	define CT_GET_RAID_CONFIG	215
	typedef struct {
		u32	response;
		u32	type;
		u32	status;
		u32	count;
		u32	parm2;
		u32	uid;
		u32	parm4;
		u32	parm5;
		u32	data[1];
	} CONTAINERRESPONSE;
	CONTAINERRESPONSE * ctr;
#	define CT_CONTINUATION_ERROR 199
	u16 bufferOffset = 0;
	u16 LoopCount = 0;
	unsigned long uniqueID = 0, sizeLeft = 0;
	unsigned char *DestinationBuffer;
	struct fib * fibptr;
	int status;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetRAIDConfig: Enter"));
#endif

	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_RAID_CONFIG_BUFFER),
	  (PIOCTL_HEADER *)&pRaidConfigBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetRAIDConfig: Exit, ReturnValue = %d",
		  Rtnval));
#endif
		return Rtnval;
	}

	/*
	 * Make sure the requested container number exists
	 */
	if ((pRaidConfigBuffer->Configuration.uRaidSetIndex == 0)
	 || (pRaidConfigBuffer->Configuration.uRaidSetIndex
	  > dev->maximum_num_containers)
	 || !dev->fsa_dev
	 || (!dev->
	  fsa_dev[pRaidConfigBuffer->Configuration.uRaidSetIndex-1].valid)) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev, HBA_FLAGS_DBG_ERROR_B,
		  ((pRaidConfigBuffer->Configuration.uRaidSetIndex
		    >= dev->maximum_num_containers)
		      ? "aac_CSMIGetRAIDConfig: RaidIndex=%d > Maximum=%d"
		      : "aac_CSMIGetRAIDConfig: RaidIndex=%d invalid"),
		  pRaidConfigBuffer->Configuration.uRaidSetIndex,
		  dev->maximum_num_containers));
#endif

		/*
		 * Indicate the RaidSetIndex is invalid
		 */
		pRaidConfigBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_RAID_SET_OUT_OF_RANGE;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pRaidConfigBuffer);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetRAIDConfig: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_RAID_SET_OUT_OF_RANGE",
		  Rtnval));
#endif
		return Rtnval;
	}

	fibptr = aac_fib_alloc(dev);
	if (fibptr == NULL) {
		pRaidConfigBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pRaidConfigBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetRAIDConfig: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}
	aac_fib_init (fibptr);
	fibptr->hw_fib_va->header.SenderSize = cpu_to_le16(sizeof(struct hw_fib));

	/*
	 * Setup and send CT_GET_RAID_CONFIG command to FW to
	 * fill in IOCTL buffer
	 */
	ct = (CONTAINER *) fib_data(fibptr);
	ct->command = cpu_to_le32(VM_ContainerConfig);
	ct->type = cpu_to_le32(CT_GET_RAID_CONFIG);
 	/* Container number */
	ct->cid = cpu_to_le32(pRaidConfigBuffer->Configuration.uRaidSetIndex-1);

	status = aac_fib_send(ContainerCommand, fibptr, sizeof(CONTAINER),
	  FsaNormal, 1, 1, NULL, NULL);
	aac_fib_complete(fibptr);

	if (status < 0) {
		aac_fib_free(fibptr);
		pRaidConfigBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pRaidConfigBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetRAIDConfig: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}

	ctr = (CONTAINERRESPONSE *) ct;
	/*
	 * Check for error conditions
	 */
	if (ctr->status == cpu_to_le32(CT_CONTINUATION_ERROR)) {
		aac_fib_free(fibptr);
		/*
		 * Indicate failure for this IOCTL
		 */
		pRaidConfigBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pRaidConfigBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetRAIDConfig: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}

	/*
	 * Grab the total size of data to be returned so we can loop through
	 * and get it all
	 */
	sizeLeft = le32_to_cpu(ctr->count);

	/*
	 * Get Unique ID for this continuation session
	 */
	uniqueID = ctr->uid;

	/*
	 * If there is more data, continue looping until we're done
	 */
	DestinationBuffer = (unsigned char *)(&pRaidConfigBuffer->Configuration);

	while (sizeLeft) {
		aac_fib_init (fibptr);
		fibptr->hw_fib_va->header.SenderSize
		  = cpu_to_le16(sizeof(struct hw_fib));

		ct->command = cpu_to_le32(VM_ContainerConfig);
		ct->type = cpu_to_le32(CT_CONTINUE_DATA);
		ct->uid = uniqueID;
		ct->offset = cpu_to_le32(LoopCount);

		status = aac_fib_send(ContainerCommand, fibptr, sizeof(CONTAINER),
		  FsaNormal, 1, 1, NULL, NULL);
		aac_fib_complete(fibptr);

		if (status < 0) {
			/*
			 * Indicate failure for this IOCTL
			 */
			pRaidConfigBuffer->IoctlHeader.ReturnCode
			  = CSMI_SAS_STATUS_FAILED;
			break;
		}

		/*
		 * Check for error conditions
		 */
		if (ctr->status == cpu_to_le32(CT_CONTINUATION_ERROR)) {
			/*
			 * Indicate failure for this IOCTL
			 */
			pRaidConfigBuffer->IoctlHeader.ReturnCode
			  = CSMI_SAS_STATUS_FAILED;
			break;
		}

		/*
		 * No error so copy the remaining data
		 */
		/*
		 * Move the full packet size and update for the next loop
		 */
		if (sizeLeft >= CT_PACKET_SIZE) {
			memcpy(DestinationBuffer, ctr->data, CT_PACKET_SIZE);

			/*
			 * Set current offset in buffer, so we can continue
			 * copying data.
			 */
			bufferOffset += CT_PACKET_SIZE;
			DestinationBuffer += CT_PACKET_SIZE;
			sizeLeft -= CT_PACKET_SIZE;
			++LoopCount;
		}

		/*
		 * last transfer; is less than CT_PACKET_SIZE, so just use
		 * sizeLeft
		 */
		else {
			memcpy(DestinationBuffer, ctr->data, sizeLeft);
			sizeLeft = 0;
		}
	}

	/*
	 * At this point, we have copied back
	 * all of the data. Send a STOP command
	 * to finish things off.
	 */
	aac_fib_init (fibptr);

	ct->command = cpu_to_le32(VM_ContainerConfig);
	ct->type = cpu_to_le32(CT_STOP_DATA);
	ct->uid = uniqueID;

	aac_fib_send(ContainerCommand, fibptr, sizeof(CONTAINER),
	  FsaNormal, 1, 1, NULL, NULL);
	aac_fib_complete(fibptr);
	aac_fib_free(fibptr);

	Rtnval = aac_CloseCSMIBuffer(dev, arg, (PIOCTL_HEADER)pRaidConfigBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetRAIDConfig: Exit, ReturnValue=%d, ReturnCode=%x",
	  Rtnval, pRaidConfigBuffer->IoctlHeader.ReturnCode));
#endif

	return Rtnval;

}


/*
 * Routine Description:
 *	This routine is called to request information on the number of RAID
 *	volumes and number of physical drives on a controller.
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS.
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMIGetRAIDInfo(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_RAID_INFO_BUFFER pRaidInfoBuffer;
	u16 NumRaidSets = 0;
	int lcv;
	PDIOCTL pIoctlInfo;
	ENHANCED_GBI_CSS * EnhancedBusInfo;
	struct fib * fibptr;
	int status;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetRAIDInfo: Enter"));
#endif

	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_RAID_INFO_BUFFER),
	  (PIOCTL_HEADER *)&pRaidInfoBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetRAIDInfo: Exit, ReturnValue=%d",
		  Rtnval));
#endif
		return Rtnval;
	}

	/*
	 * Traverse the container list and count all containers
	 */
	if (dev->fsa_dev)
	for (lcv = 0; lcv < dev->maximum_num_containers; lcv++)
		if (dev->fsa_dev[lcv].valid)
			NumRaidSets++;
	pRaidInfoBuffer->Information.uNumRaidSets = NumRaidSets;

	/*
	 * Find the absolute maximum number of physical drives that can make
	 * up a container. It's pretty ambiquous so we'll default it to the
	 * Falcon maximum number of drives supported and then try to figure
	 * out from firmware the max number of drives we can attach to this
	 * controller.
	 */
	pRaidInfoBuffer->Information.uMaxDrivesPerSet = 128;
	fibptr = aac_fib_alloc(dev);
	if (fibptr) {
		aac_fib_init(fibptr);

		pIoctlInfo = (PDIOCTL) fib_data(fibptr);
		pIoctlInfo->Command = cpu_to_le32(VM_Ioctl);
		pIoctlInfo->ObjType = cpu_to_le32(FT_DRIVE);
		pIoctlInfo->MethodId = cpu_to_le32(1);
		pIoctlInfo->ObjectId = 0;
		pIoctlInfo->CtlCmd = cpu_to_le32(EnhancedGetBusInfo);

		status = aac_fib_send(ContainerCommand, fibptr,
		  sizeof(*EnhancedBusInfo),
		  FsaNormal, 1, 1, NULL, NULL);
	
		aac_fib_complete(fibptr);
	
		EnhancedBusInfo = (struct aac_enhanced_bus_info_response *) pIoctlInfo;
	
		if (status >= 0) switch (EnhancedBusInfo->BusType[0]) {
		case CSS_BUS_TYPE_SATA:
			pRaidInfoBuffer->Information.uMaxDrivesPerSet
			  = le32_to_cpu(dev->supplement_adapter_info.MaxNumberPorts);
			break;
		case CSS_BUS_TYPE_SAS:
			pRaidInfoBuffer->Information.uMaxDrivesPerSet = 128;
			break;
		default:
			pRaidInfoBuffer->Information.uMaxDrivesPerSet
			  = dev->maximum_num_physicals
			  * dev->maximum_num_channels;
			break;
		}
		aac_fib_free(fibptr);
	}

	Rtnval = aac_CloseCSMIBuffer(dev, arg, (PIOCTL_HEADER)pRaidInfoBuffer);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetRAIDInfo: Exit, ReturnValue=%d, ReturnCode=%x",
	  Rtnval, pRaidInfoBuffer->IoctlHeader.ReturnCode));
#endif

	return Rtnval;
}


/*
 * Routine Description:
 *	This routine is called to request information about physical
 *	characteristics and interconnect to the SATA or SAS domain.
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS.
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMIGetPhyInfo(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_PHY_INFO_BUFFER pPhyInfoBuffer;
	PDIOCTL pIoctlInfo;
	PDIOCTLRESPONSE pIoctlResp;
	struct fib * fibptr;
	int status;
	u32 Length;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetPhyInfo: Enter scsi%d",
	  dev->scsi_host_ptr->host_no));
#endif

#if 0
	/* Command can not be issued to the adapter */
	if (!(dev->supplement_adapter_info.FeatureBits
	 & le32_to_cpu(AAC_FEATURE_FALCON))) {
		Rtnval = -ENOENT;
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetPhyInfo: Exit, ReturnValue=-ENOENT",
		  Rtnval));
#endif
		return Rtnval;
	}
#endif
	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_PHY_INFO_BUFFER),
	  (PIOCTL_HEADER *)&pPhyInfoBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetPhyInfo: Exit, ReturnValue=%d",
		  Rtnval));
#endif
		return Rtnval;
	}

	/* TODO : Figure out the correct size to send or do a continue fib */

	fibptr = aac_fib_alloc(dev);
	if (fibptr == NULL) {
		pPhyInfoBuffer->IoctlHeader.ReturnCode = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pPhyInfoBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetPhyInfo: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}
	aac_fib_init(fibptr);
	fibptr->hw_fib_va->header.SenderSize = cpu_to_le16(sizeof(struct hw_fib));

	pIoctlInfo = (PDIOCTL) fib_data(fibptr);
	pIoctlInfo->Command = cpu_to_le32(VM_Ioctl);
	pIoctlInfo->ObjType = cpu_to_le32(FT_DRIVE);
	pIoctlInfo->MethodId = cpu_to_le32(1);
	pIoctlInfo->ObjectId = 0;
	pIoctlInfo->CtlCmd = cpu_to_le32(CsmiGetPhyInfo);
	Length = pPhyInfoBuffer->IoctlHeader.Length;
	/* Issue a Larger FIB? */
	if (Length > (sizeof(struct hw_fib) - sizeof(struct aac_fibhdr)
					- sizeof(*pIoctlInfo))) {
		Length = sizeof(struct hw_fib) - sizeof(struct aac_fibhdr)
		       - sizeof(*pIoctlInfo);
		pPhyInfoBuffer->IoctlHeader.Length = Length;
	}
	memcpy(((char *)pIoctlInfo) + sizeof(*pIoctlInfo),
	  pPhyInfoBuffer, Length);

	status = aac_fib_send(ContainerCommand, fibptr,
	  Length + sizeof(*pIoctlInfo),
	  FsaNormal, 1, 1, NULL, NULL);

	aac_fib_complete(fibptr);

	if (status < 0) {
		aac_fib_free(fibptr);
		pPhyInfoBuffer->IoctlHeader.ReturnCode = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pPhyInfoBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetPhyInfo: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}

	pIoctlResp = (PDIOCTLRESPONSE) pIoctlInfo;

	/*
	 * Copy back the filled out buffer to complete the
	 * request
	 */
	memcpy(pPhyInfoBuffer, ((char *)pIoctlResp) + sizeof(*pIoctlResp),
	  Length);

	aac_fib_free(fibptr);

	Rtnval = aac_CloseCSMIBuffer(dev, arg, (PIOCTL_HEADER)pPhyInfoBuffer);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetPhyInfo: Exit, Rtnval, ReturnCode=%x",
	  Rtnval, pPhyInfoBuffer->IoctlHeader.ReturnCode));
#endif

	return Rtnval;

}


/*
 * Routine Description:
 *	This routine is called to obtain the initial SATA signature (the
 *	initial Register Device to the Host FIS) from a directly attached SATA
 *	device.
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS.
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMIGetSATASignature(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_SATA_SIGNATURE_BUFFER pSataSignatureBuffer;
	PDIOCTL pIoctlInfo;
	PDIOCTLRESPONSE pIoctlResp;
	struct fib * fibptr;
	int status;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetSATASignature: Enter scsi%d",
	  dev->scsi_host_ptr->host_no));
#endif

	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_SATA_SIGNATURE_BUFFER),
	  (PIOCTL_HEADER *)&pSataSignatureBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetSATASignature: Exit, ReturnValue=%d",
		  Rtnval));
#endif
		return Rtnval;
	}

	pSataSignatureBuffer->IoctlHeader.ReturnCode = CSMI_SAS_NO_SATA_DEVICE;

	fibptr = aac_fib_alloc(dev);
	if (fibptr == NULL) {
		pSataSignatureBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pSataSignatureBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetSATASignature: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}
	aac_fib_init(fibptr);

	pIoctlInfo = (PDIOCTL) fib_data(fibptr);
	pIoctlInfo->Command = cpu_to_le32(VM_Ioctl);
	pIoctlInfo->ObjType = cpu_to_le32(FT_DRIVE);
	pIoctlInfo->MethodId = cpu_to_le32(1);
	pIoctlInfo->ObjectId = 0;
	pIoctlInfo->CtlCmd = cpu_to_le32(CsmiSataSignature);
	memcpy(((char *)pIoctlInfo) + sizeof(*pIoctlInfo),
	  pSataSignatureBuffer,sizeof(CSMI_SAS_SATA_SIGNATURE_BUFFER));

	status = aac_fib_send(ContainerCommand, fibptr, sizeof(*pIoctlInfo)
	  - sizeof(u32) + sizeof(CSMI_SAS_SATA_SIGNATURE_BUFFER), FsaNormal,
	  1, 1, NULL, NULL);

	aac_fib_complete(fibptr);

	if (status < 0) {
		aac_fib_free(fibptr);
		pSataSignatureBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pSataSignatureBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetSATASignature: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}

	pIoctlResp = (PDIOCTLRESPONSE) pIoctlInfo;

	/*
	 * Copy back the filled out buffer to complete the
	 * request
	 */
	memcpy(pSataSignatureBuffer,
	  ((char *)pIoctlResp) + sizeof(*pIoctlResp),
	  sizeof(CSMI_SAS_SATA_SIGNATURE_BUFFER));

	aac_fib_free(fibptr);

	/*
	 * Indicate success for this IOCTL
	 *	pSataSignatureBuffer->IoctlHeader.ReturnCode
	 *		= CSMI_SAS_STATUS_SUCCESS;
	 * is set by the Firmware Response.
	 */
	Rtnval = aac_CloseCSMIBuffer(dev, arg,
	  (PIOCTL_HEADER)pSataSignatureBuffer);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetSATASignature: Exit, ReturnValue=%d,"
	  " ReturnCode=CSMI_SAS_STATUS_SUCCESS",
	  Rtnval));
#endif

	return Rtnval;

}


/*
 * Routine Description:
 *	This routine is called to return the driver information.
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS.
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMIGetDriverInfo(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_DRIVER_INFO_BUFFER pDriverInfoBuffer;
	char * driver_version = aac_driver_version;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetDriverInfo: Enter"));
#endif

	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_DRIVER_INFO_BUFFER),
	  (PIOCTL_HEADER *)&pDriverInfoBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetDriverInfo: Exit, ReturnValue=%d",
		  Rtnval));
#endif
		return Rtnval;
	}

	/*
	 * Fill in the information member of the pDriverInfoBuffer
	 * structure.
	 */

	/*
	 * Driver name
	 */
	strncpy(pDriverInfoBuffer->Information.szName,
	  (dev->scsi_host_ptr->hostt->info
	    ? dev->scsi_host_ptr->hostt->info(dev->scsi_host_ptr)
	    : dev->scsi_host_ptr->hostt->name),
	  sizeof(pDriverInfoBuffer->Information.szName));

	/*
	 * Driver Description
	 */
	sprintf(pDriverInfoBuffer->Information.szDescription,
	  "Adaptec %s driver",
	  dev->scsi_host_ptr->hostt->name);

	/*
	 * Set version number information
	 */
	pDriverInfoBuffer->Information.usMajorRevision
	  = cpu_to_le16(simple_strtol(driver_version, &driver_version, 10));
	pDriverInfoBuffer->Information.usMinorRevision
	  = cpu_to_le16(simple_strtol(driver_version + 1, &driver_version, 10));
#if (defined(AAC_DRIVER_BUILD))
	pDriverInfoBuffer->Information.usBuildRevision = cpu_to_le16(AAC_DRIVER_BUILD);
#else
	pDriverInfoBuffer->Information.usBuildRevision = cpu_to_le16(9999);
#endif
	pDriverInfoBuffer->Information.usReleaseRevision
	  = cpu_to_le16(simple_strtol(driver_version + 1, NULL, 10));
	pDriverInfoBuffer->Information.usCSMIMajorRevision
	  = cpu_to_le16(CSMI_MAJOR_REVISION);
	pDriverInfoBuffer->Information.usCSMIMinorRevision
	  = cpu_to_le16(CSMI_MINOR_REVISION);

	Rtnval = aac_CloseCSMIBuffer(dev, arg, (PIOCTL_HEADER)pDriverInfoBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMIGetDriverInfo: Exit, ReturnValue=%d, ReturnCode=%x",
	  Rtnval, pDriverInfoBuffer->IoctlHeader.ReturnCode));
#endif

	return Rtnval;

}



/*
 * Routine Description:
 *	This routine is called to change the physical characteristics of a phy.
 *	We currently do not support this functionality, and are not required to
 *	in order to support CSMI.
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS.
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMISetPhyInfo(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	u32 ReturnCode;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMISetPhyInfo: Enter scsi%d",
	  dev->scsi_host_ptr->host_no));
#endif

	ReturnCode = CSMI_SAS_PHY_INFO_NOT_CHANGEABLE;
	Rtnval = 0;
	if (copy_to_user((void __user *)&((PIOCTL_HEADER)arg)->ReturnCode,
	  (void *)&ReturnCode, sizeof(u32)))
		Rtnval = -EFAULT;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMISetPhyInfo: Exit, ReturnValue=%d,"
	  " ReturnCode=CSMI_SAS_PHY_INFO_NOT_CHANGEABLE",
	  Rtnval));
#endif

	return Rtnval;

}


/*
 * Routine Description:
 *	This routine is called to send generic STP or SATA commands to a
 *	specific SAS address.
 * Return Value:
 *	Status value, to be returned by aac_HandleCSMI, and returned to the OS.
 *	--> Must set CSMI status value in pHeader->ReturnCode.
 */
int
aac_CSMISTPPassThru(
	struct aac_dev * dev,
	void __user * arg)
{
	int Rtnval;
	PCSMI_SAS_STP_PASSTHRU_BUFFER pPassThruBuffer;
	unsigned bytesLeft = 0;
	u8 * pDataPointer = NULL;
	/* function */
#	define SATAPASSTHROUGH_REGISTER		0x00000000
#	define SATAPASSTHROUGH_SOFTRESET	0x00000001
	typedef struct {
		u32	function;
		u32	bus;
		u32	targetId;
		u32	lun;
		u32	timeOutValue;
		u32	srbFlags;
#			define	HOSTSRB_FLAGS_NO_DATA_TRANSFER	0x00000000
#			define	HOSTSRB_FLAGS_DATA_IN		0x00000040
#			define	HOSTSRB_FLAGS_DATA_OUT		0x00000080
		u32	dataTransferLength;
		u32	retryLimit;
		u32	cdbLength;
		u8	command;
		u8	features;
		u8	sectorNumber;
		u8	cylinderLow;
		u8	cylinderHigh;
		u8	deviceHead;
		u8	sectorNumber_Exp;
		u8	cylinderLow_Exp;
		u8	cylinderHigh_Exp;
		u8	features_Exp;
		u8	sectorCount;
		u8	sectorCount_Exp;
		u8	reserved;
		u8	control;
		u8	reserved1[2];
		u32	reserved2[4];
		struct sgmap64	sgMap;
	} HOST_SATA_REQUEST_BLOCK;
	typedef HOST_SATA_REQUEST_BLOCK * PHOST_SATA_REQUEST_BLOCK;
	PHOST_SATA_REQUEST_BLOCK pSataRequest;
	typedef struct {
		u32	status;
		u32	srbStatus;
		u32	scsiStatus;
		u32	dataTransferLength;
		u32	senseInfoBufferLength;
		u8	statusReg;
		u8	error;
		u8	sectorNumber;
		u8	cylinderLow;
		u8	cylinderHigh;
		u8	deviceHead;
		u8	sectorNumber_Exp;
		u8	cylinderLow_Exp;
		u8	cylinderHigh_Exp;
		u8	deviceRegister_Exp;
		u8	features;
		u8	featuers_Exp;
		u8	reserved1[4];
	} HOST_SATA_REQUEST_BLOCK_RESULT;
	typedef HOST_SATA_REQUEST_BLOCK_RESULT * PHOST_SATA_REQUEST_BLOCK_RESULT;
	PHOST_SATA_REQUEST_BLOCK_RESULT pSataResponse;
	struct sgmap64 * pSgMap;
	struct fib * fibptr;
	int status;
	dma_addr_t addr;
	void * p = NULL;
#	define SataPortCommandU64	602

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMISTPPassThru: Enter scsi%d",
	  dev->scsi_host_ptr->host_no));
#endif

	/*
	 * Verify buffer size. If buffer is too small, the error status will
	 * be set for pHeader->ReturnCode in aac_VerifyCSMIBuffer.
	 */
	if ((Rtnval = aac_VerifyCSMIBuffer(&dev, arg,
	  sizeof(CSMI_SAS_STP_PASSTHRU_BUFFER),
	  (PIOCTL_HEADER *)&pPassThruBuffer))) {
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMISTPPassThru: Exit, ReturnValue=%d",
		  Rtnval));
#endif
		return Rtnval;
	}
	/*
	 * Weed out the flags we don't support
	 */
	if ((pPassThruBuffer->Parameters.uFlags & CSMI_SAS_STP_DMA)
	 || (pPassThruBuffer->Parameters.uFlags & CSMI_SAS_STP_DMA_QUEUED)) {
		/*
		 * Indicate failure for this IOCTL
		 */
		pPassThruBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pPassThruBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMISTPPassThru: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}

	fibptr = aac_fib_alloc(dev);
	if (fibptr == NULL) {
		pPassThruBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pPassThruBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMISTPPassThru: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}
	aac_fib_init(fibptr);

	pSataRequest = (PHOST_SATA_REQUEST_BLOCK) fib_data(fibptr);
	pSgMap = &pSataRequest->sgMap;
	pSataResponse = (PHOST_SATA_REQUEST_BLOCK_RESULT) pSataRequest;

	/*
	 * Setup HOST_SATA_REQUEST_BLOCK structure
	 */
	memset(pSataRequest,0,sizeof(*pSataRequest));
	memset(pSataResponse,0,sizeof(HOST_SATA_REQUEST_BLOCK_RESULT));
	if (pPassThruBuffer->Parameters.uFlags & CSMI_SAS_STP_RESET_DEVICE)
		pSataRequest->function = SATAPASSTHROUGH_SOFTRESET;
	else
		pSataRequest->function = SATAPASSTHROUGH_REGISTER;

	/*
	 * Pull relevant data from header.
	 */
	if (pPassThruBuffer->Parameters.uFlags & CSMI_SAS_STP_READ)
		pSataRequest->srbFlags = HOSTSRB_FLAGS_DATA_IN;
	else
		pSataRequest->srbFlags = HOSTSRB_FLAGS_DATA_OUT;
	pSataRequest->timeOutValue = pPassThruBuffer->IoctlHeader.Timeout;

	/*
	 * Obsolete parameter - adapter firmware ignores this
	 */
	pSataRequest->retryLimit = 0;
	pSataRequest->cdbLength = 14;

	/*
	 * Fill in remaining data from IOCTL Parameters
	 */
	/* Someday will be: SAS_ADDR_TO_BUS((*((u64*)pPassThruBuffer->Parameters.bDestinationSASAddress))); */
	pSataRequest->bus = 0;
	/* Someday will be: SAS_ADDR_TO_TARGET((*((u64*)pPassThruBuffer->Parameters.bDestinationSASAddress))); */
	pSataRequest->targetId = pPassThruBuffer->Parameters.bPhyIdentifier;
	/* Someday will be: SAS_ADDR_TO_LUN((*((u64*)pPassThruBuffer->Parameters.bDestinationSASAddress))); */
	pSataRequest->lun = 0;
	pSataRequest->dataTransferLength
	  = pPassThruBuffer->Parameters.uDataLength;

	/*
	 * SATA Task Set Register Listing
	 */
	pSataRequest->command = pPassThruBuffer->Parameters.bCommandFIS[2];
	pSataRequest->features = pPassThruBuffer->Parameters.bCommandFIS[3];
	pSataRequest->sectorNumber = pPassThruBuffer->Parameters.bCommandFIS[4];
	pSataRequest->cylinderLow = pPassThruBuffer->Parameters.bCommandFIS[5];
	pSataRequest->cylinderHigh = pPassThruBuffer->Parameters.bCommandFIS[6];
	pSataRequest->deviceHead = pPassThruBuffer->Parameters.bCommandFIS[7];
	pSataRequest->sectorNumber_Exp
	  = pPassThruBuffer->Parameters.bCommandFIS[8];
	pSataRequest->cylinderLow_Exp
	  = pPassThruBuffer->Parameters.bCommandFIS[9];
	pSataRequest->cylinderHigh_Exp
	  = pPassThruBuffer->Parameters.bCommandFIS[10];
	pSataRequest->features_Exp
	  = pPassThruBuffer->Parameters.bCommandFIS[11];
	pSataRequest->sectorCount
	  = pPassThruBuffer->Parameters.bCommandFIS[12];
	pSataRequest->sectorCount_Exp
	  = pPassThruBuffer->Parameters.bCommandFIS[13];
	pSataRequest->control = pPassThruBuffer->Parameters.bCommandFIS[15];

	/*
	 * Build SGMAP
	 */
	if (pPassThruBuffer->Parameters.uDataLength) {

		pDataPointer = &pPassThruBuffer->bDataBuffer[0];
		bytesLeft = pPassThruBuffer->Parameters.uDataLength;

		/*
		 * Get physical address and length of
		 * contiguous physical buffer
		 */
		p = pci_alloc_consistent(dev->pdev, bytesLeft, &addr);
		if(p == 0) {
			aac_fib_free(fibptr);
			pPassThruBuffer->IoctlHeader.ReturnCode
			  = CSMI_SAS_STATUS_FAILED;
			Rtnval = aac_CloseCSMIBuffer(dev, arg,
			  (PIOCTL_HEADER)pPassThruBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
			fwprintf((dev, HBA_FLAGS_DBG_FUNCTION_EXIT_B
			   | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
			  "aac_CSMISTPPassThru: Exit, ReturnValue=%d,"
			  " ReturnCode=CSMI_SAS_STATUS_FAILED",
			  Rtnval));
#endif
			return Rtnval;
		}
		memcpy(p, pDataPointer, bytesLeft);

		pSgMap->sg[0].addr[1] = cpu_to_le32((u32)((u64)addr>>32));
		pSgMap->sg[0].addr[0] = cpu_to_le32((u32)(addr & 0xffffffff));

		/*
		 * Store the length for this entry
		 */
		pSgMap->sg[0].count = bytesLeft;

		/*
		 * Store final count of entries
		 */
		pSgMap->count = 1;

	} else
		pSataRequest->srbFlags = HOSTSRB_FLAGS_NO_DATA_TRANSFER;

	/*
	 * Send FIB
	 */
	status = aac_fib_send(SataPortCommandU64, fibptr, sizeof(*pSataRequest),
	  FsaNormal, 1, 1, NULL, NULL);

	aac_fib_complete(fibptr);

	if (status < 0) {
		if (pPassThruBuffer->Parameters.uDataLength)
			pci_free_consistent(dev->pdev, bytesLeft,p, addr);
		aac_fib_free(fibptr);
		pPassThruBuffer->IoctlHeader.ReturnCode
		  = CSMI_SAS_STATUS_FAILED;
		Rtnval = aac_CloseCSMIBuffer(dev, arg,
		  (PIOCTL_HEADER)pPassThruBuffer);
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev,
		  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_CSMIGetSATASignature: Exit, ReturnValue=%d,"
		  " ReturnCode=CSMI_SAS_STATUS_FAILED",
		  Rtnval));
#endif
		return Rtnval;
	}

	if (pPassThruBuffer->Parameters.uDataLength) {
		memcpy(pDataPointer, p, bytesLeft);
		pci_free_consistent(dev->pdev, bytesLeft,p, addr);
	}

	/*
	 * pull response data and respond to IOCTL
	 */

	/*
	 * Return relevant data
	 */
	pPassThruBuffer->Status.bConnectionStatus = CSMI_SAS_OPEN_ACCEPT;
	pPassThruBuffer->Status.bStatusFIS[2] = pSataResponse->statusReg;

	/*
	 * pPassThruBuffer->Status.uSCR = ??;
	 */
	pPassThruBuffer->Status.uDataBytes = pSataResponse->dataTransferLength;

	aac_fib_free(fibptr);

	/*
	 * Indicate success for this IOCTL
	 */
	pPassThruBuffer->IoctlHeader.ReturnCode = CSMI_SAS_STATUS_SUCCESS;
	Rtnval = aac_CloseCSMIBuffer(dev, arg, (PIOCTL_HEADER)pPassThruBuffer);

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_CSMISTPPassThru: Exit, ReturnValue=%d,"
	  " ReturnCode=CSMI_SAS_STATUS_SUCCESS",
	  Rtnval));
#endif

	return Rtnval;

}


/*
 *
 * Routine Description:
 *
 *   This routine is the main entry point for all CSMI function calls.
 *
 */
int aac_csmi_ioctl(
	struct aac_dev * dev,
	int cmd,
	void __user * arg)
{
	int returnStatus = -ENOTTY;

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_ENTRY_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_HandleCSMI: Enter, (scsi%d) ControlCode = %x",
	  dev->scsi_host_ptr->host_no, cmd));
#endif

	/*
	 * Handle the supported CSMI commands
	 */
	switch (cmd) {
	case CC_CSMI_SAS_GET_DRIVER_INFO:
		returnStatus = aac_CSMIGetDriverInfo(dev, arg);
		break;

	case CC_CSMI_SAS_GET_CNTLR_CONFIG:
		returnStatus = aac_CSMIGetControllerConfig(dev, arg);
		break;

	case CC_CSMI_SAS_GET_CNTLR_STATUS:
		returnStatus = aac_CSMIGetControllerStatus(dev, arg);
		break;

	case CC_CSMI_SAS_GET_RAID_INFO:
		returnStatus = aac_CSMIGetRAIDInfo(dev, arg);
		break;

	case CC_CSMI_SAS_GET_PHY_INFO:
		returnStatus = aac_CSMIGetPhyInfo(dev, arg);
		break;

	case CC_CSMI_SAS_SET_PHY_INFO:
		returnStatus = aac_CSMISetPhyInfo(dev, arg);
		break;

	case CC_CSMI_SAS_GET_SATA_SIGNATURE:
		returnStatus = aac_CSMIGetSATASignature(dev, arg);
		break;

	case CC_CSMI_SAS_GET_RAID_CONFIG:
		returnStatus = aac_CSMIGetRAIDConfig(dev, arg);
		break;

	case CC_CSMI_SAS_STP_PASSTHRU:
		returnStatus = aac_CSMISTPPassThru(dev, arg);
		break;

	/*
	 * Unsupported CSMI control code
	 */
	case CC_CSMI_SAS_FIRMWARE_DOWNLOAD:
	case CC_CSMI_SAS_GET_SCSI_ADDRESS:
	case CC_CSMI_SAS_GET_DEVICE_ADDRESS:
	case CC_CSMI_SAS_SMP_PASSTHRU:
	case CC_CSMI_SAS_SSP_PASSTHRU:
	case CC_CSMI_SAS_GET_LINK_ERRORS:
	case CC_CSMI_SAS_TASK_MANAGEMENT:
	case CC_CSMI_SAS_GET_CONNECTOR_INFO:
	case CC_CSMI_SAS_PHY_CONTROL:
	{
		PIOCTL_HEADER pHeader;

		/*
		 * Verify buffer size. If buffer is too small, the error
		 * status will be set for pHeader->ReturnCode in
		 * aac_VerifyCSMIBuffer.
		 */
		if (!(returnStatus = aac_VerifyCSMIBuffer(&dev, arg,
		  sizeof(PIOCTL_HEADER), &pHeader))) {
			pHeader->ReturnCode = CSMI_SAS_STATUS_BAD_CNTL_CODE;
			if (!(returnStatus = aac_CloseCSMIBuffer(dev, arg,
			  pHeader)))
				returnStatus = -EINVAL;
		}
#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
		fwprintf((dev, HBA_FLAGS_DBG_CSMI_COMMANDS_B,
		  "aac_HandleCSMI: Unsupported ControlCode=%x",
		  cmd));
#endif
		break;
	}
	}

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	fwprintf((dev,
	  HBA_FLAGS_DBG_FUNCTION_EXIT_B | HBA_FLAGS_DBG_CSMI_COMMANDS_B,
	  "aac_HandleCSMI: Exit, ReturnCode=%d", returnStatus));
#endif

	return(returnStatus);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT))
void aac_csmi_register_ioctl32_conversion(void)
{
	register_ioctl32_conversion(CC_CSMI_SAS_GET_DRIVER_INFO,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_CNTLR_CONFIG,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_CNTLR_STATUS,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_RAID_INFO,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_PHY_INFO,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_SET_PHY_INFO,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_SATA_SIGNATURE,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_RAID_CONFIG,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_STP_PASSTHRU,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_FIRMWARE_DOWNLOAD,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_SCSI_ADDRESS,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_DEVICE_ADDRESS,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_SMP_PASSTHRU,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_SSP_PASSTHRU,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_LINK_ERRORS,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_TASK_MANAGEMENT,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_GET_CONNECTOR_INFO,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
	register_ioctl32_conversion(CC_CSMI_SAS_PHY_CONTROL,
	  (int(*)(unsigned int,unsigned int,unsigned long,struct file*))sys_ioctl);
}

void aac_csmi_unregister_ioctl32_conversion(void)
{
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_DRIVER_INFO);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_CNTLR_CONFIG);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_CNTLR_STATUS);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_RAID_INFO);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_PHY_INFO);
	unregister_ioctl32_conversion(CC_CSMI_SAS_SET_PHY_INFO);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_SATA_SIGNATURE);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_RAID_CONFIG);
	unregister_ioctl32_conversion(CC_CSMI_SAS_STP_PASSTHRU);
	unregister_ioctl32_conversion(CC_CSMI_SAS_FIRMWARE_DOWNLOAD);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_SCSI_ADDRESS);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_DEVICE_ADDRESS);
	unregister_ioctl32_conversion(CC_CSMI_SAS_SMP_PASSTHRU);
	unregister_ioctl32_conversion(CC_CSMI_SAS_SSP_PASSTHRU);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_LINK_ERRORS);
	unregister_ioctl32_conversion(CC_CSMI_SAS_TASK_MANAGEMENT);
	unregister_ioctl32_conversion(CC_CSMI_SAS_GET_CONNECTOR_INFO);
	unregister_ioctl32_conversion(CC_CSMI_SAS_PHY_CONTROL);
}
#endif
#endif

#endif
