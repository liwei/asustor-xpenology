/*
 * SRIOV support and configfs interface support for
 * (Message Passing Technology) based controllers
 *
 * This code is based on drivers/scsi/mpt3sas/mpt3sas_configfs.c
 * Copyright (C) 2014  LSI Corporation
 * Copyright (C) 2014  Avago Technologies
 *  (mailto:MPT-FusionLinux.pdl@avagotech.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * NO WARRANTY
 * THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
 * LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
 * solely responsible for determining the appropriateness of using and
 * distributing the Program and assumes all risks associated with its
 * exercise of rights under this Agreement, including but not limited to
 * the risks and costs of program errors, damage to or loss of data,
 * programs or equipment, and unavailability or interruption of operations.

 * DISCLAIMER OF LIABILITY
 * NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
 * HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/configfs.h>
#include <linux/delay.h>
#include "mpt3sas_base.h"

#if defined(CONFIG_SRIOV_SUPPORT)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27))
/* -------------------------------------------------------------------------- */
/* Macro definitions from linux/configfs.h included here for compatibility    */
/* with older kernels                                                         */
/* -------------------------------------------------------------------------- */

/*
 * Users often need to create attribute structures for their configurable
 * attributes, containing a configfs_attribute member and function pointers
 * for the show() and store() operations on that attribute. If they don't
 * need anything else on the extended attribute structure, they can use
 * this macro to define it  The argument _item is the name of the
 * config_item structure.
 */
#define CONFIGFS_ATTR_STRUCT(_item)					\
struct _item##_attribute {						\
	struct configfs_attribute attr;					\
	ssize_t (*show)(struct _item *, char *);			\
	ssize_t (*store)(struct _item *, const char *, size_t);		\
}

/*
 * With the extended attribute structure, users can use this macro
 * (similar to sysfs' __ATTR) to make defining attributes easier.
 * An example:
 * #define MYITEM_ATTR(_name, _mode, _show, _store)     \
 * struct myitem_attribute childless_attr_##_name =     \
 *         __CONFIGFS_ATTR(_name, _mode, _show, _store)
 */

#define __CONFIGFS_ATTR(_name, _mode, _show, _store)			\
{									\
	.attr   = {							\
			.ca_name = __stringify(_name),			\
			.ca_mode = _mode,				\
			.ca_owner = THIS_MODULE,			\
	},								\
	.show   = _show,						\
	.store  = _store,						\
}
/* Here is a readonly version, only requiring a show() operation */
#define __CONFIGFS_ATTR_RO(_name, _show)				\
{									\
	.attr   = {							\
			.ca_name = __stringify(_name),			\
			.ca_mode = 0444,				\
			.ca_owner = THIS_MODULE,			\
	},								\
	.show   = _show,						\
}

/*
 * With these extended attributes, the simple show_attribute() and
 * store_attribute() operations need to call the show() and store() of the
 * attributes.  This is a common pattern, so we provide a macro to define
 * them.  The argument _item is the name of the config_item structure.
 * This macro expects the attributes to be named "struct <name>_attribute"
 * and the function to_<name>() to exist;
 */

#define CONFIGFS_ATTR_OPS(_item)					\
static ssize_t _item##_attr_show(struct config_item *item,		\
				 struct configfs_attribute *attr,	\
				 char *page)				\
{									\
	struct _item *_item = to_##_item(item);				\
	struct _item##_attribute *_item##_attr =			\
		container_of(attr, struct _item##_attribute, attr);	\
	ssize_t ret = 0;						\
	if (_item##_attr->show)						\
		ret = _item##_attr->show(_item, page);			\
	return ret;							\
}									\
static ssize_t _item##_attr_store(struct config_item *item,		\
				struct configfs_attribute *attr,	\
				const char *page, size_t count)		\
{									\
	struct _item *_item = to_##_item(item);				\
	struct _item##_attribute *_item##_attr =			\
		container_of(attr, struct _item##_attribute, attr);	\
	ssize_t ret = -EINVAL;						\
									\
	if (_item##_attr->store)					\
		ret = _item##_attr->store(_item, page, count);		\
	return ret;							\
}

#endif
/* -------------------------------------------------------------------------- */
/* visibility link list and helper functions                                  */
/* -------------------------------------------------------------------------- */

#define MAPPING_MODE_ENCLOSURE_SLOT			1
#define MAPPING_MODE_DEVICE_PERSISTENCE		2

#define DEVICE_PRESENCE_OFF				0
#define DEVICE_PRESENCE_ON				1

#define MPI2_SAS_IOC_PARAM_TYPE_VISIBILITY		0x80
/**
 * struct visibility_info - SRIOV visibility
 * @device_presence: device is present in topology
 * @WWID: address  to enclosure or device
 * @slot: slot number
 * @mapping_mode: enclosure/slot or device persistency
 * @serial_number: Inquiry VPD Page 0x80
 * @vfid_mask: VF_ID mask
 */
struct visibility_info {
	struct list_head list;
	u8	device_presence;
	u64	WWID;
	u16	slot;
	u8	mapping_mode;
	u8	*serial_number;
	u32	vfid_mask;
};
/**
 * _configfs_get_ioc - get the adapter object
 * @ioc_number: adapter number
 * Returns adapter object
 *
 */
static struct MPT3SAS_ADAPTER *
_configfs_get_ioc(int ioc_number)
{
	struct MPT3SAS_ADAPTER *ioc;

	list_for_each_entry(ioc, &mpt3sas_ioc_list, list) {
		if (ioc->id == ioc_number)
			return ioc;
	}

	return NULL;
}

/**
 * _configfs_find_by_sas_address - sas device search
 * @ioc: per adapter object
 * @sas_address: sas address of the device to be searched
 * Context: Calling function should acquire iocc->sas_device_lock
 *
 * This searches for sas_device based on sas_address, then return sas_device
 * object.
 */
static struct _sas_device *
_configfs_find_by_sas_address(struct MPT3SAS_ADAPTER *ioc,
	u64 sas_address)
{
	struct _sas_device *sas_device, *rc = NULL;
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_for_each_entry(sas_device, &ioc->sas_device_list, list) {
		if (sas_device->sas_address == sas_address) {
			rc = sas_device;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	return rc;
}

/**
 * _configfs_find_by_serial_number - sas device search
 * @ioc: per adapter object
 * @vi:  visibility_info object
 * Context: This function acquires ioc->sas_device_lock and the callers are
 * the function should be capable of letting this function to acquire lock
 *
 * This searches for sas_device based on serial_number, then return
 * sas_device object.
 */
static struct _sas_device *
_configfs_find_by_serial_number(struct MPT3SAS_ADAPTER *ioc,
	U8 *serial_number)
{
	struct _sas_device *sas_device, *rc = NULL;
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_for_each_entry(sas_device, &ioc->sas_device_list, list) {
		if (serial_number && sas_device->serial_number &&
		     !strcmp(serial_number, sas_device->serial_number)) {
			rc = sas_device;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	return rc;
}

/**
 * _configfs_find_by_enclosure_slot - sas device search
 * @ioc: per adapter object
 * @enclosure_logical_id: enclosure logical id of the device to be searched
 * @slot : slot number of the device to be searched
 * Context: Calling function should acquire ioc->sas_device_lock
 *
 * This searches for sas_device based on enclosure and slot number ,
 *  then return sas_device object.
 */
static struct _sas_device *
_configfs_find_by_enclosure_slot(struct MPT3SAS_ADAPTER *ioc,
	u64 enclosure_logical_id, u16 slot)
{
	struct _sas_device *sas_device, *rc = NULL;
	unsigned long flags;

	spin_lock_irqsave(&ioc->sas_device_lock, flags);
	list_for_each_entry(sas_device, &ioc->sas_device_list, list) {
		if (sas_device->enclosure_logical_id == enclosure_logical_id
		    && sas_device->slot == slot) {
			rc = sas_device;
			goto out;
		}
	}
 out:
	spin_unlock_irqrestore(&ioc->sas_device_lock, flags);
	return rc;
}

/* -------------------------------------------------------------------------- */
/* mpt3sas driver group and some macros                                       */
/* -------------------------------------------------------------------------- */

struct mpt3sas_driver {
	struct configfs_subsystem subsys;
};

/**
 * to_mpt3sas_driver - return the mpt3sas_driver group object
 * @item - pointer to config item
 *
 * Returns the mpt3sas_driver object
 */
static inline struct mpt3sas_driver *
to_mpt3sas_driver(struct config_item *item)
{
	return item ? container_of(to_configfs_subsystem(to_config_group(item)),
	    struct mpt3sas_driver, subsys) : NULL;
}

CONFIGFS_ATTR_STRUCT(mpt3sas_driver);

#define MPT3SAS_DRIVER_ATTR(_name, _mode, _show, _store) \
struct mpt3sas_driver_attribute mpt3sas_driver_attr_##_name = \
__CONFIGFS_ATTR(_name, _mode, _show, _store)

#define MPT3SAS_DRIVER_ATTR_RO(_name, _show) \
struct mpt3sas_driver_attribute mpt3sas_driver_attr_##_name = \
__CONFIGFS_ATTR_RO(_name, _show);

/* -------------------------------------------------------------------------- */
/* HBA group and some macros                                                  */
/* -------------------------------------------------------------------------- */

struct mpt3sas_hba {
	struct config_group group;
	u16 ioc_number;
	u8 mapping_mode;
};

/**
 * to_mpt3sas_hba - return the mpt3sas_hba object
 * @item - pointer to config item
 *
 * Returns the mpt3sas_hba object
 */
static inline struct mpt3sas_hba *
to_mpt3sas_hba(struct config_item *item)
{
	return item ? container_of(to_config_group(item),
				   struct mpt3sas_hba, group) : NULL;
}

CONFIGFS_ATTR_STRUCT(mpt3sas_hba);

#define MPT3SAS_HBA_ATTR(_name, _mode, _show, _store) \
struct mpt3sas_hba_attribute mpt3sas_hba_attr_##_name = \
__CONFIGFS_ATTR(_name, _mode, _show, _store)

#define MPT3SAS_HBA_ATTR_RO(_name, _show) \
struct mpt3sas_hba_attribute mpt3sas_hba_attr_##_name = \
__CONFIGFS_ATTR_RO(_name, _show);


/* -------------------------------------------------------------------------- */
/* Device group and some macros                                               */
/* -------------------------------------------------------------------------- */

struct mpt3sas_device {
	struct config_group group;
	struct mpt3sas_hba *hba;
	u64 WWID;
	u16 slot;
	u32 vfid_mask;
	u8  *serial_number;
};

/**
 * to_mpt3sas_device - return the mpt3sas_device object
 * @item - pointer to config item
 *
 * Returns the mpt3sas_device object
 */
static inline struct mpt3sas_device *
to_mpt3sas_device(struct config_item *item)
{
	return item ? container_of(to_config_group(item),
				   struct mpt3sas_device, group) : NULL;
}

CONFIGFS_ATTR_STRUCT(mpt3sas_device);

#define MPT3SAS_DEVICE_ATTR(_name, _mode, _show, _store) \
struct mpt3sas_device_attribute mpt3sas_device_attr_##_name = \
__CONFIGFS_ATTR(_name, _mode, _show, _store)

#define MPT3SAS_DEVICE_ATTR_RO(_name, _show) \
struct mpt3sas_device_attribute mpt3sas_device_attr_##_name = \
__CONFIGFS_ATTR_RO(_name, _show);

/* -------------------------------------------------------------------------- */
/* Logical ID                                                                 */
/* -------------------------------------------------------------------------- */

/* attribute = WWID */

/**
 * mpt3sas_device_WWID_read - get WWID number
 * @device: mpt3sas device object
 * @page: input pointer to fill the WWID number
 *
 */
static ssize_t
mpt3sas_device_WWID_read(struct mpt3sas_device *device,
					char *page)
{
	struct visibility_info *vi = NULL;
	struct MPT3SAS_ADAPTER *ioc = NULL;

	if (device->hba->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE) {

		ioc = _configfs_get_ioc(device->hba->ioc_number);
		
		if (!ioc) {
			printk(KERN_INFO "%s: Inavlid HBA\n", __func__);
			return 0;
		}
	        
		list_for_each_entry(vi, &ioc->visibility_list, list) {
			if (vi->serial_number && device->serial_number &&
 			    !strcmp(vi->serial_number, device->serial_number)) {
				if (vi->WWID != device->WWID)
					device->WWID = vi->WWID;
			}
		}
	}
	return sprintf(page, "(0x%016llx)\n", device->WWID);
}

/**
 * mpt3sas_device_WWID_write - set WWID number
 * @device: mpt3sas device object
 * @page: input pointer from where the WWID number has to be copied
 * count : number of bytes in @page
 *
 */
static ssize_t
mpt3sas_device_WWID_write(struct mpt3sas_device *device,
	const char *page, size_t count)
{
	u64 tmp;
	char *p = (char *) page;

	/* WWID length should be 16 or 18 (with 0x) + carriage return,
	hence check before proceeding */

	if ((strlen(p) == 17) || (strlen(p) == 19)) {
		tmp = simple_strtoull(p, &p, 16);
		if (!p || (*p && (*p != '\n')))
			return -EINVAL;
		printk(KERN_INFO "%s: setting WWID to (0x%016llx)\n",
		       __func__, tmp);
		device->WWID = tmp;
	} else {
		printk(KERN_INFO "%s: WWID entered %s is invalid.\n",
		       __func__, p);
		return -EINVAL;
	}

	return count;
}
MPT3SAS_DEVICE_ATTR(WWID, S_IRUGO | S_IWUSR, mpt3sas_device_WWID_read,
	mpt3sas_device_WWID_write);

/* attribute = slot */
/**
 * mpt3sas_device_slot_read - get device slot number
 * @device: mpt3sas device object
 * @page: input pointer to fill the slot number
 *
 */
static ssize_t
mpt3sas_device_slot_read(struct mpt3sas_device *device,
	char *page)
{
	return sprintf(page, "%d\n", device->slot);
}

/**
 * mpt3sas_device_slot_write - set slot number
 * @device: mpt3sas device object
 * @page: input pointer from where the slot number to be copied
 * count : number of bytes in @page
 *
 */
static ssize_t 
mpt3sas_device_slot_write(struct mpt3sas_device *device,
	const char *page, size_t count)
{
	unsigned long tmp;
	char *p = (char *) page;

	tmp = simple_strtoul(p, &p, 10);
	if (!p || (*p && (*p != '\n')))
		return -EINVAL;

	if (tmp > INT_MAX)
		return -ERANGE;

	if (device->hba->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT)
		device->slot = tmp;
	else {
		printk(KERN_INFO "%s: unable to set slot\n", __func__);
		return -EPERM;
	}
	return count;
}
MPT3SAS_DEVICE_ATTR(slot, S_IRUGO | S_IWUSR, mpt3sas_device_slot_read,
    mpt3sas_device_slot_write);

/**
 * mpt3sas_device_serial_number_read - get serial number
 * @device: mpt3sas device object
 * @page: input pointer from where the serial number to be copied
 *
 */
static ssize_t
mpt3sas_device_serial_number_read(struct mpt3sas_device *device,
	char *page)
{
	return sprintf(page, "%s\n", device->serial_number);
}

/**
 * mpt3sas_device_serial_number_write - set serial number
 * @device: mpt3sas device object
 * @page: input pointer from where the serial number to be copied
 * count : number of bytes in @page
 *
 */
static ssize_t 
mpt3sas_device_serial_number_write(struct mpt3sas_device *device,
	const char *page, size_t count)
{
	char *p = (char *) page;
	int string_len;
	if (!p)
		return -EINVAL;

	if (device->hba->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE) {
		string_len = strlen(p);
		if (string_len > count)
			return -EINVAL;
		device->serial_number = kzalloc(string_len, GFP_KERNEL);
		if (!device->serial_number) {
			printk(KERN_INFO
			       "%s: memory allocation failure\n",
							__func__);
			return -ENOMEM;
		}
		strlcpy(device->serial_number, p, string_len);
	}
	else {
		printk(KERN_INFO "%s: unable to set serial number\n", __func__);
		return -EPERM;
	}
	return count;
}
MPT3SAS_DEVICE_ATTR(serial_number, S_IRUGO | S_IWUSR, mpt3sas_device_serial_number_read,
    mpt3sas_device_serial_number_write);
/**
 * mpt3sas_set_visibility_info - set VF visibility info
 * @ioc: per adapter object
 * @visibility_flag: visibility flag
 * @vf_id: virtual function id
 * @handle: device handle
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
mpt3sas_set_visibility_info(struct MPT3SAS_ADAPTER *ioc,
	u8 visibility_flag, u8 vf_id, u16 handle)
{
	Mpi2SasIoUnitControlReply_t mpi_reply;
	Mpi2SasIoUnitControlRequest_t mpi_request;

	memset(&mpi_request, 0, sizeof(Mpi2SasIoUnitControlRequest_t));
	mpi_request.Function = MPI2_FUNCTION_SAS_IO_UNIT_CONTROL;
	mpi_request.Operation = MPI2_SAS_OP_SET_IOC_PARAMETER;
	mpi_request.IOCParameter = MPI2_SAS_IOC_PARAM_TYPE_VISIBILITY;
	mpi_request.DevHandle = cpu_to_le16(handle);
	mpi_request.IOCParameterValue = cpu_to_le32((vf_id<<8) | visibility_flag);

	printk(MPT3SAS_INFO_FMT "SET_VISIBILITY INFO :"
			"(0x%04x), (0x%08x)\n", ioc->name,
			mpi_request.DevHandle,
			mpi_request.IOCParameterValue);

	if ((mpt3sas_base_sas_iounit_control(ioc, &mpi_reply, &mpi_request))) {
		printk(MPT3SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			ioc->name, __FILE__, __LINE__, __func__);
		return -ENXIO;
	}

	if (mpi_reply.IOCStatus || mpi_reply.IOCLogInfo)
		printk(MPT3SAS_INFO_FMT "ioc_status"
			"(0x%04x), loginfo(0x%08x)\n", ioc->name,
			le16_to_cpu(mpi_reply.IOCStatus),
			le32_to_cpu(mpi_reply.IOCLogInfo));
	return 0;
}

/**
 * mpt3sas_done_device_visibility_settings - notify FW that PF driver has done
 * 		 			      with device visibility settings
 * @ioc: per adapter object
 *
 * Returns 0 for success, non-zero for failure.
 */
int
mpt3sas_done_device_visibility_settings(struct MPT3SAS_ADAPTER *ioc)
{
	Mpi2SasIoUnitControlReply_t mpi_reply;
	Mpi2SasIoUnitControlRequest_t mpi_request;

	memset(&mpi_request, 0, sizeof(Mpi2SasIoUnitControlRequest_t));
	mpi_request.Function = MPI2_FUNCTION_SAS_IO_UNIT_CONTROL;
	mpi_request.Operation = SAS_OP_SRIOV_VF_PERMS_SET;

	printk(MPT3SAS_INFO_FMT "DONE with visibility settings\n",
		ioc->name);

	if ((mpt3sas_base_sas_iounit_control(ioc, &mpi_reply, &mpi_request))) {
		printk(MPT3SAS_ERR_FMT "failure at %s:%d/%s()!\n",
			ioc->name, __FILE__, __LINE__, __func__);
		return -ENXIO;
	}

	if (mpi_reply.IOCStatus || mpi_reply.IOCLogInfo)
		printk(MPT3SAS_INFO_FMT "ioc_status"
			"(0x%04x), loginfo(0x%08x)\n", ioc->name,
			le16_to_cpu(mpi_reply.IOCStatus),
			le32_to_cpu(mpi_reply.IOCLogInfo));
	return 0;
}

/**
 * mpt3sas_traverse_vfid_bitmask_and_update - update the visibility flag
 *	for those VF_ids whose visibility is currently set/ cleared
 * @ioc: per adapter object
 * @visibility_flag: visibility flag
 * @vf_id: virtual function id
 * @handle: device handle
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
mpt3sas_traverse_vfid_bitmask_and_update(struct MPT3SAS_ADAPTER *ioc,
	struct visibility_info *vi, struct mpt3sas_device *device, u16 handle)
{
	u8 i;
	u8 visibility_flag;
	u32 mask;
	int ret;
	int no_of_bits = sizeof(device->vfid_mask) * 8;

/* First clear the visibility flag for those VF_ids whose visiblity flag cleared recently
	and it's visibility flag is still set in the link list */
	for (i = 0; i < no_of_bits; i++) {
		mask = 1<<i;
		if ((vi->vfid_mask & mask) != (device->vfid_mask & mask)) {
			if ((vi->vfid_mask & mask) > 0 && (device->vfid_mask & mask) == 0) {
				visibility_flag =
					(device->vfid_mask & mask) > 0 ? 1 : 0;
				ret = mpt3sas_set_visibility_info(ioc,
						visibility_flag, i, handle);
				if (ret != 0)
					return ret;
			}
		}
	}

/* Now set the visibility flag for those VF_ids whose visibility is currently set
	and it's visibility flag is still in cleared state in the link list */
	for (i = 0; i < no_of_bits; i++) {
		mask = 1<<i;
		if ((vi->vfid_mask & mask) != (device->vfid_mask & mask)) {
			if ((vi->vfid_mask & mask) == 0 && (device->vfid_mask & mask) > 0) {
				visibility_flag =
					(device->vfid_mask & mask) > 0 ? 1 : 0;
				ret = mpt3sas_set_visibility_info(ioc,
						visibility_flag, i, handle);
				if (ret != 0)
					return ret;
			}
		}
	}
	return 0;
}

/**
 * mpt3sas_traverse_vfid_bitmask_and_set - set the visibility information for those
 * 					   VF_ids whose visibility is currently set
 * @ioc: per adapter object
 * @vi: visibility info
 * @handle: device handle
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
mpt3sas_traverse_vfid_bitmask_and_set(struct MPT3SAS_ADAPTER *ioc,
	struct visibility_info *vi, u16 handle)
{
	u8 i;
	u8 visibility_flag = 1;
	u32 mask;
	int ret;
	int no_of_bits = sizeof(vi->vfid_mask) * 8;
	for (i = 0; i < no_of_bits; i++) {
		mask = 1<<i;
		if (vi->vfid_mask & mask) {
			ret = mpt3sas_set_visibility_info(ioc,
				visibility_flag, i, handle);
			if (ret != 0)
				return ret;
		}
	}
	return 0;
}

/**
 * mpt3sas_update_visibility_info - update the visibility information for the
 * 				    input mpt3sas_device object 
 * @ioc: per adapter object
 * @vi: visibility info
 * @device: mpt3sas_device object
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
mpt3sas_update_visibility_info(struct MPT3SAS_ADAPTER *ioc,
    struct visibility_info *vi, struct mpt3sas_device *device)
{
	struct mpt3sas_hba *hba = device->hba;
	static struct _sas_device *sas_device;
	struct MPT3SAS_DEVICE *sas_device_priv_data;
	struct scsi_device *sdev;
	size_t string_len;

	if(ioc->shost_recovery || ioc->pci_error_recovery) {
		printk(MPT3SAS_INFO_FMT "%s: host reset in progress!\n",
			__func__, ioc->name);
		return -EAGAIN;
	}

	if (hba->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT)
		sas_device = _configfs_find_by_enclosure_slot(ioc,
					 device->WWID, device->slot);
	else if (hba->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE) {
		if (device->serial_number)
			sas_device = _configfs_find_by_serial_number(ioc,
						   device->serial_number);
		else
			sas_device = _configfs_find_by_sas_address(ioc,
								 device->WWID);
	} else {
		printk(MPT3SAS_INFO_FMT "Mapping mode information is not"
		  " provided, So please set the mapping mode and try again\n",
								    ioc->name);
		sas_device = NULL;
		return -EAGAIN;
	}

	if (!sas_device) {
		vi->device_presence = DEVICE_PRESENCE_OFF;
		vi->vfid_mask = device->vfid_mask;
		/* Copy serial number which is read from the XML file
		 * in Device Persistence mapping mode */
		if (!vi->serial_number && device->serial_number) {
			string_len = strlen(device->serial_number);
			vi->serial_number = kzalloc(string_len, GFP_KERNEL);
			if (!vi->serial_number) {
				printk(MPT3SAS_INFO_FMT
				       "%s: memory allocation failure\n",
				       ioc->name, __func__);
				return -ENOMEM;
			}
			strncpy(vi->serial_number, device->serial_number,
								string_len);
		}
		return -ENODEV;
	}
	
	shost_for_each_device(sdev, ioc->shost)	{
		sas_device_priv_data = sdev->hostdata;
		if (!sas_device_priv_data)
			continue;
		if (sas_device_priv_data->sas_target->handle !=
				sas_device->handle)
			continue;
		if (sas_device_priv_data->block) {
			printk(MPT3SAS_INFO_FMT
			  "This device having sas address as (0x%016llx)"
			  " is currently in blocked state", ioc->name,
			  sas_device->sas_address);
			return -EAGAIN;
		}
		if (sas_device_priv_data->sas_target->tm_busy) {
			printk(MPT3SAS_INFO_FMT
			  "This device having sas address as (0x%016llx)"
			  " is busy with TMs", ioc->name,
			  sas_device->sas_address);
			return -EAGAIN;
		}

	}

	if (!vi->serial_number && sas_device->serial_number &&
	    hba->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE) {
		string_len = strlen(sas_device->serial_number);
		vi->serial_number = kzalloc(string_len, GFP_KERNEL);
		if (!vi->serial_number) {
			printk(MPT3SAS_INFO_FMT
			       "%s: memory allocation failure\n",
			       ioc->name, __func__);
			return -ENOMEM;
		}
		strncpy(vi->serial_number, sas_device->serial_number,
							string_len);
		vi->WWID = device->WWID = sas_device->sas_address;
	}

	vi->device_presence = DEVICE_PRESENCE_ON;

	if (vi->vfid_mask && (vi->vfid_mask != device->vfid_mask)) {
		if(hba->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE) {
			dewtprintk(ioc, printk(MPT3SAS_INFO_FMT 
			   "The vfid_mask for the device having sas address "
			   "(0x%016llx) is already set to (0x%0x)\n", ioc->name,
			   (unsigned long long)sas_device->sas_address,
			    vi->vfid_mask));
		}
		else if(hba->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT) {
			dewtprintk(ioc, printk(MPT3SAS_INFO_FMT 
				"The vfid_mask for the drive located at"
				"enclosure logical (0x%016llx), slot(%d) "
				"is already set to (0x%0x)\n", ioc->name,
				(unsigned long long)
				sas_device->enclosure_logical_id,
                	        sas_device->slot, vi->vfid_mask));
		}

		dewtprintk(ioc, printk(MPT3SAS_INFO_FMT "If you want to "
			"change the visibility of this drive to different VFs"
			" then delete the existing visibility and create the new"
			" visibility for this drive\n", ioc->name));
		return -EINVAL;
	}
	else if(vi->vfid_mask != device->vfid_mask) {
		mpt3sas_traverse_vfid_bitmask_and_update(ioc, vi, device,
							 sas_device->handle);
		vi->vfid_mask = device->vfid_mask;
	}
	return 0;
}

/**
 * mpt3sas_add_visibility_info - add the visibility information for the
 * 				  input mpt3sas_device object 
 * @ioc: per adapter object
 * @device: mpt3sas_device object
 *
 * Returns 0 for success, non-zero for failure.
 */
static int 
mpt3sas_add_visibility_info(struct MPT3SAS_ADAPTER *ioc,
	struct mpt3sas_device *device)
{
	struct visibility_info *vi;

	vi = kzalloc(sizeof(struct visibility_info), GFP_KERNEL);
	if (!vi) {
		printk(MPT3SAS_INFO_FMT "%s: memory allocation failure\n",
		       ioc->name, __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&vi->list);
	printk(MPT3SAS_INFO_FMT "%s: adding entry to link list: %p\n",
	       ioc->name, __func__, vi);
	list_add_tail(&vi->list, &ioc->visibility_list);
	vi->mapping_mode = device->hba->mapping_mode;
	if (device->hba->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT)
		vi->slot = device->slot;
	vi->WWID = device->WWID;
	mpt3sas_update_visibility_info(ioc, vi, device);
	return 0;
}

/**
 * mpt3sas_set_device_presence - set the device presence in proper mapping mode
 *				  DEVICE_PERSISTENCE_MODE/ ENCLOSURE_SLOT
 * @ioc: per adapter object
 * @sas_device: sas_device object
 *
 */
void 
mpt3sas_set_device_presence(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_device *sas_device)
{
	struct visibility_info *vi;

	if (!ioc->visibility_count)
		return;

	/* see if the entry is already  entered */
	list_for_each_entry(vi, &ioc->visibility_list, list) {
		if (vi->mapping_mode ==
		    MAPPING_MODE_DEVICE_PERSISTENCE) {
			if ((sas_device->device_info &
			     MPI2_SAS_DEVICE_INFO_SATA_DEVICE)) {
			  	if (vi->serial_number &&
				    sas_device->serial_number &&
			  	    !strcmp(vi->serial_number,
					    sas_device->serial_number)) {
					vi->device_presence =
						 DEVICE_PRESENCE_ON;
					mpt3sas_traverse_vfid_bitmask_and_set(
					 	 ioc, vi, sas_device->handle);
					vi->WWID = sas_device->sas_address;
				}
			} else if (vi->WWID == sas_device->sas_address) {
				vi->device_presence = DEVICE_PRESENCE_ON;
				mpt3sas_traverse_vfid_bitmask_and_set(ioc, vi,
							sas_device->handle);
			}
		} else if (vi->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT) {
			if ((vi->WWID == sas_device->enclosure_logical_id) &&
			    (vi->slot == sas_device->slot)) {
				vi->device_presence = DEVICE_PRESENCE_ON;
				mpt3sas_traverse_vfid_bitmask_and_set(ioc, vi,
							sas_device->handle);
			}
		}
	}
}

/**
 * mpt3sas_clear_device_presence - clear the device presence
 * 
 * @ioc: per adapter object
 * @sas_device: sas_device object
 *
 */
void 
mpt3sas_clear_device_presence(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_device *sas_device)
{
	struct visibility_info *vi;

	if (!ioc->visibility_count)
		return;
	
	/* see if the entry is already  entered */
	list_for_each_entry(vi, &ioc->visibility_list, list) {
		if (vi->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE) {
			if (vi->WWID == sas_device->sas_address) { 
				vi->device_presence = DEVICE_PRESENCE_OFF;
				return;
			}
		} else if (vi->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT) {
			if ((vi->WWID == sas_device->enclosure_logical_id) &&
			    (vi->slot == sas_device->slot)) {
				vi->device_presence = DEVICE_PRESENCE_OFF;
				return;
			}
		}
	}
}

/* attribute = vfid_mask */
/**
 * mpt3sas_device_vfid_mask_read - get device vfid mask
 * @device: mpt3sas device object
 * @page: input pointer to fill the vfid mask
 *
 */
static ssize_t 
mpt3sas_device_vfid_mask_read(struct mpt3sas_device *device,
	char *page)
{
	return sprintf(page, "0x%0x\n", device->vfid_mask);
}

/**
 * mpt3sas_device_vfid_mask_write - set vfid mask to the device
 * @device: mpt3sas device object
 * @page: input pointer from where the vfid mask number to be copied
 * count : number of bytes in @page
 *
 */
static ssize_t
mpt3sas_device_vfid_mask_write(struct mpt3sas_device *device,
	const char *page, size_t count)
{
	struct MPT3SAS_ADAPTER *ioc;
	struct visibility_info *vi;
	unsigned long tmp;
	char *p = (char *) page;
	struct _sas_device *sas_device = NULL;
	size_t string_len;

	tmp = simple_strtoul(p, &p, 16);
	if (!p || (*p && (*p != '\n')))
		return -EINVAL;

	if (tmp > UINT_MAX)
		return -ERANGE;

	device->vfid_mask = tmp;

	ioc = _configfs_get_ioc(device->hba->ioc_number);
	if (!ioc) {
		printk(MPT3SAS_INFO_FMT "%s: ioc is no longer present\n",
		       ioc->name, __func__);
		count = -EPERM;
		goto out;
	}
	if(ioc->shost_recovery || ioc->pci_error_recovery
	    || ioc->remove_host) {
		printk(MPT3SAS_INFO_FMT "%s: host reset in progress!\n",
			__func__, ioc->name);
		count = -EAGAIN;
		goto out;
	}

	if (!device->serial_number &&
	     device->hba->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE) {
		sas_device = _configfs_find_by_sas_address(ioc, device->WWID);
		if (sas_device && sas_device->serial_number) {
			string_len = strlen(sas_device->serial_number);
			device->serial_number = kzalloc(string_len,
							 GFP_KERNEL);
			if (!device->serial_number) {
				printk(MPT3SAS_INFO_FMT
				       "%s: memory allocation failure\n",
				       ioc->name, __func__);
				count = -ENOMEM;
				goto out;
			}
			strncpy(device->serial_number,
				 sas_device->serial_number, string_len);
		}
	}

	/* see if the entry is already  entered */
	list_for_each_entry(vi, &ioc->visibility_list, list) {
		if (vi->mapping_mode ==
			MAPPING_MODE_DEVICE_PERSISTENCE) {
			if (device->serial_number && vi->serial_number &&
			   !strcmp(vi->serial_number, device->serial_number)) {
				mpt3sas_update_visibility_info(ioc,
							       vi, device);
				goto out;
			}
		} else if (vi->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT) {
			if ((vi->WWID == device->WWID) &&
			    (vi->slot == device->slot)) {
				mpt3sas_update_visibility_info(ioc,
				    vi, device);
				goto out;
			}
		}
	}

	mpt3sas_add_visibility_info(ioc, device);
 out:

	return count;
}
MPT3SAS_DEVICE_ATTR(vfid_mask, S_IRUGO | S_IWUSR,
		    mpt3sas_device_vfid_mask_read,
		    mpt3sas_device_vfid_mask_write);

/* attribute = description */
/**
 * mpt3sas_device_description_read - get device description
 * @device: mpt3sas device object
 * @page: input pointer to fill the description 
 *
 */
static ssize_t
mpt3sas_device_description_read(struct mpt3sas_device *device,
	char *page)
{
	return sprintf(page,
"Disk Attributes\n\n"
"This level defines the attributes of the disk. In case of Persistant device\n"
"mapping WWID acts as the Unique ID and in case of Enclosure device mapping\n"
"the enclosure logical ID(written into WWID file) along with the slot number\n"
"acts as the unique ID. The vfid_mask contains the information on which of\n"
"the VMs this disk should be visible. For eg, If the value of vfid_mask is 6,\n"
"then the VMs with the VFs mapping to 1 and 2 would be able to see this\n"
"disk\n");
}
MPT3SAS_DEVICE_ATTR_RO(description, mpt3sas_device_description_read);

static struct configfs_attribute *mpt3sas_device_attrs[] = {
	&mpt3sas_device_attr_WWID.attr,
	&mpt3sas_device_attr_slot.attr,
	&mpt3sas_device_attr_serial_number.attr,
	&mpt3sas_device_attr_vfid_mask.attr,
	&mpt3sas_device_attr_description.attr,
	NULL,
};

/**
 * mpt3sas_device_release -  releasing mpt3sas driver in configfs sas device object
 * @item: configfs config_item object
 *
 * Called while releasing device object the configfs 
 *
 */
static void 
mpt3sas_device_release(struct config_item *item)
{
	struct MPT3SAS_ADAPTER *ioc;
	struct visibility_info *vi;
	struct _sas_device *sas_device;
	struct mpt3sas_device *device = to_mpt3sas_device(item);

	if (!device)
		return;
	else if (device->vfid_mask == 0)
		goto out;

	ioc = _configfs_get_ioc(device->hba->ioc_number);
	if (!ioc) {
		printk(MPT3SAS_INFO_FMT "%s: invalid ioc_number\n",
		       ioc->name, __func__);
		goto out;
	}
	if(ioc->shost_recovery || ioc->pci_error_recovery
	    || ioc->remove_host) {
		printk(MPT3SAS_INFO_FMT "%s: host reset in progress!\n",
			__func__, ioc->name);
		goto out;
	}

	list_for_each_entry(vi, &ioc->visibility_list, list) {
		if (device->hba->mapping_mode ==
			MAPPING_MODE_DEVICE_PERSISTENCE) {
			if (device->serial_number && vi->serial_number &&
			    !strcmp(vi->serial_number, device->serial_number))
				goto found_vi;
		} else if (device->hba->mapping_mode ==
				MAPPING_MODE_ENCLOSURE_SLOT) {
			if ((vi->WWID == device->WWID) &&
			    (vi->slot == device->slot))
				goto found_vi;
		}
	}
	goto out;

found_vi:
	if (device->hba->mapping_mode == MAPPING_MODE_ENCLOSURE_SLOT)
		sas_device = _configfs_find_by_enclosure_slot(ioc, device->WWID,
						     device->slot);
	else if (device->hba->mapping_mode == MAPPING_MODE_DEVICE_PERSISTENCE)
		sas_device = _configfs_find_by_serial_number(ioc,
						     device->serial_number);
	else
		sas_device = NULL;

	if (!sas_device) {
		vi->device_presence = DEVICE_PRESENCE_OFF;
		vi->vfid_mask = 0;
		goto out;
	}

	device->vfid_mask = 0;
	mpt3sas_traverse_vfid_bitmask_and_update(ioc, vi, device,
						 sas_device->handle);
	vi->vfid_mask = 0;
        if (device->serial_number)
		kfree(device->serial_number);
out:
	kfree(device);
}

CONFIGFS_ATTR_OPS(mpt3sas_device);
static struct configfs_item_operations mpt3sas_device_item_ops = {
	.show_attribute = mpt3sas_device_attr_show,
	.store_attribute = mpt3sas_device_attr_store,
	.release	= mpt3sas_device_release,
};

static struct config_item_type mpt3sas_device_type = {
	.ct_item_ops	= &mpt3sas_device_item_ops,
	.ct_attrs	= mpt3sas_device_attrs,
	.ct_owner	= THIS_MODULE,
};

/* -------------------------------------------------------------------------- */
/* HBA                                                                        */
/* -------------------------------------------------------------------------- */
/**
 * mpt3sas_hba_make_group -  add hba as part of configfs group object
 * @group: configfs config_group object
 * @name: name to be assigned to group
 *
 */
static struct config_group *
mpt3sas_hba_make_group(struct config_group *group,
    const char *name)
{
	struct mpt3sas_device *device;
	struct mpt3sas_hba *hba;
	struct MPT3SAS_ADAPTER *ioc;
	struct config_group *rc;

	hba = container_of(group, struct mpt3sas_hba, group);

	if ((hba->mapping_mode != MAPPING_MODE_ENCLOSURE_SLOT) &&
	    (hba->mapping_mode != MAPPING_MODE_DEVICE_PERSISTENCE)) {
		printk(KERN_INFO "%s: mapping mode not set\n",
		       __func__);
		rc =  ERR_PTR(-EPERM);
		goto out;
	}

	ioc = _configfs_get_ioc(hba->ioc_number);
	if (!ioc) {
		printk(KERN_INFO "%s: invalid ioc_number\n",
		       __func__);
		rc = ERR_PTR(-ENODEV);
		goto out;
	}

	device = kzalloc(sizeof(struct mpt3sas_device), GFP_KERNEL);
	if (!device) {
		printk(KERN_INFO "%s: memory allocation failure\n",
		       __func__);
		rc = ERR_PTR(-ENOMEM);
		goto out;
	}

	device->hba = hba;
	if (!ioc->visibility_count++)
		INIT_LIST_HEAD(&ioc->visibility_list);
	config_group_init_type_name(&device->group, name, &mpt3sas_device_type);
	return &device->group;

 out:
	/* Insufficient error checking in configfs filesystem code
	 * requires passing NULL to any kernel prior to 2.6.27
	 * to avoid Oops
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27))
	return rc;
#else
	return NULL;
#endif
}

/* attribute = ioc_number */
/**
 * mpt3sas_hba_ioc_number_read - get hba number
 * @hba: mpt3sas_hba object
 * @page: input pointer to fill the hba number
 *
 */
static ssize_t 
mpt3sas_hba_ioc_number_read(struct mpt3sas_hba *hba, char *page)
{
	return sprintf(page, "%d\n", hba->ioc_number);
}

/**
 * mpt3sas_hba_ioc_number_write - set hba number
 * @hba: mpt3sas hba object
 * @page: input pointer from where the hba number has to be copied
 * count : number of bytes in @page
 *
 */
 static ssize_t 
 mpt3sas_hba_ioc_number_write(struct mpt3sas_hba *hba,
    const char *page, size_t count)
{
	struct MPT3SAS_ADAPTER *ioc;
	unsigned long tmp;
	char *p = (char *) page;

	tmp = simple_strtoul(p, &p, 10);
	if (!p || (*p && (*p != '\n')))
		return -EINVAL;

	if (tmp > INT_MAX)
		return -ERANGE;

	ioc = _configfs_get_ioc(tmp);
	if (ioc) {
		if (!ioc->visibility_count) {
			hba->ioc_number = tmp;
			printk(MPT3SAS_INFO_FMT "%s: setting ioc_number\n",
			       ioc->name, __func__);
		} else {
			printk(MPT3SAS_INFO_FMT
			      "%s: unable to change ioc_number\n",
			       ioc->name, __func__);
			return -ENODEV;
		}
	} else {
		printk(MPT3SAS_INFO_FMT "%s: invalid ioc_number\n",
		       ioc->name, __func__);
		return -EINVAL;
	}

	return count;
}
MPT3SAS_HBA_ATTR(ioc_number, S_IRUGO | S_IWUSR, mpt3sas_hba_ioc_number_read,
	mpt3sas_hba_ioc_number_write);

/* attribute = mapping_mode */
/**
 * mpt3sas_hba_mapping_mode_read - get hba mapping mode
 * @hba: mpt3sas_hba object
 * @page: input pointer to fill the mapping mode
 *
 */
static ssize_t 
mpt3sas_hba_mapping_mode_read(struct mpt3sas_hba *hba,
    char *page)
{
	return sprintf(page, "%d\n", hba->mapping_mode);
}

/**
 * mpt3sas_hba_mapping_mode_write - set hba mapping mode
 * @hba: mpt3sas hba object
 * @page: input pointer from where the hba mapping mode has to be copied
 * count : number of bytes in @page
 *
 */
static ssize_t 
mpt3sas_hba_mapping_mode_write(struct mpt3sas_hba *hba,
    const char *page, size_t count)
{
	struct MPT3SAS_ADAPTER *ioc;
	unsigned long tmp;
	char *p = (char *) page;

	tmp = simple_strtoul(p, &p, 10);
	if (!p || (*p && (*p != '\n')))
		return -EINVAL;

	if (tmp > INT_MAX)
		return -ERANGE;
	mdelay(2000);

	if (!(tmp == MAPPING_MODE_ENCLOSURE_SLOT ||
	    tmp == MAPPING_MODE_DEVICE_PERSISTENCE)) {
		printk(KERN_INFO "%s: mapping mode invalid\n",
		       __func__);
		return -EINVAL;
	} else {
		ioc = _configfs_get_ioc(hba->ioc_number);
		if (ioc && !ioc->visibility_count) {
			hba->mapping_mode = tmp;
			printk(KERN_INFO "%s: setting mapping_mode\n",
			       __func__);
		} else {
			printk(KERN_INFO
			"%s: unable to change mapping_mode\n",
			__func__);
			return -ENODEV;
		}
	}
	return count;
}
MPT3SAS_HBA_ATTR(mapping_mode, S_IRUGO | S_IWUSR, mpt3sas_hba_mapping_mode_read,
    mpt3sas_hba_mapping_mode_write);

/* attribute = description */
/**
 * mpt3sas_hba_description_read - get hba description
 * @device: mpt3sas hba object
 * @page: input pointer to fill the description 
 *
 */
static ssize_t 
mpt3sas_hba_description_read(struct mpt3sas_hba *hba, char *page)
{
	return sprintf(page,
"HBA\n\n"
"This level contains the attributes of the hba, that have to be set by\n"
"the user. The ioc_number refers to the unique ID of the HBA and the\n"
"mapping mode refers to either Enclosure Device Mapping(1) or Persistant\n"
"device mapping(2)\n");
}
MPT3SAS_HBA_ATTR_RO(description, mpt3sas_hba_description_read);

static struct configfs_attribute *mpt3sas_hba_attrs[] = {
	&mpt3sas_hba_attr_ioc_number.attr,
	&mpt3sas_hba_attr_mapping_mode.attr,
	&mpt3sas_hba_attr_description.attr,
	NULL,
};

/**
 * mpt3sas_hba_release -  releasing mpt3sas hba in configfs sas device object
 * @item: configfs config_item object
 *
 * Called while releasing hba object in the configfs
 *
 */
static void 
mpt3sas_hba_release(struct config_item *item)
{
	struct mpt3sas_hba *hba = to_mpt3sas_hba(item);
	struct MPT3SAS_ADAPTER *ioc;
	struct visibility_info *vi, *vi_next;

	if (!hba)
		return;

	ioc = _configfs_get_ioc(hba->ioc_number);
	if (ioc && ioc->visibility_count) {
		list_for_each_entry_safe(vi, vi_next,
		   &ioc->visibility_list, list) {
			printk(MPT3SAS_INFO_FMT
			       "%s: removing entry from link list: %p\n",
			       ioc->name, __func__, vi);
			kfree(vi->serial_number);
			list_del(&vi->list);
			kfree(vi);
		}
		ioc->visibility_count = 0;
		kfree(hba);
	}
}

CONFIGFS_ATTR_OPS(mpt3sas_hba);
static struct configfs_item_operations mpt3sas_hba_item_ops = {
	.show_attribute = mpt3sas_hba_attr_show,
	.store_attribute = mpt3sas_hba_attr_store,
	.release	= mpt3sas_hba_release,
};

static struct configfs_group_operations mpt3sas_hba_group_ops = {
	.make_group	= mpt3sas_hba_make_group,
};

static struct config_item_type mpt3sas_hba_type = {
	.ct_item_ops	= &mpt3sas_hba_item_ops,
	.ct_group_ops	= &mpt3sas_hba_group_ops,
	.ct_attrs	= mpt3sas_hba_attrs,
	.ct_owner	= THIS_MODULE,
};

/* ------------------------------------------------------------------------*/
/* Top Level                                                               */
/* ------------------------------------------------------------------------*/
/**
 * mpt3sas_driver_make_group -  configfs make group entry point
 * @group: configfs config_group object
 * @name: name to be assigned to group
 *
 * Called while creating hba group object 
 *
 */
static struct config_group *
mpt3sas_driver_make_group (struct config_group *group,
		const char *name)
{
	struct mpt3sas_hba *hba;

	hba = kzalloc(sizeof(struct mpt3sas_hba), GFP_KERNEL);
	if (!hba) {
		printk(KERN_INFO
		       "%s: memory allocation failure\n",
		       __func__);
		return ERR_PTR(-ENOMEM);
	}
	config_group_init_type_name(&hba->group, name, &mpt3sas_hba_type);
	return &hba->group;
}

/* attribute = description */
/**
 * mpt3sas_driver_description_read - get driver description
 * @device: mpt3sas driver object
 * @page: input pointer to fill the description 
 *
 */
static ssize_t 
mpt3sas_driver_description_read (struct mpt3sas_driver *mpt3sas_driver,
	char *page)
{
	return sprintf(page,
"mpt3sas\n\n"
"This is the top level of configfs tree. All the SAS Gen3 cards handled by\n"
"the mpt3sas driver could be found here\n");
}
MPT3SAS_DRIVER_ATTR_RO(description, mpt3sas_driver_description_read);

static struct configfs_attribute *mpt3sas_driver_attrs[] = {
	&mpt3sas_driver_attr_description.attr,
	NULL,
};

/**
 * mpt3sas_driver_release -  releasing mpt3sas driver in configfs
 * @item: configfs config_item object
 *
 * Called while releasing driver object in the configfs
 *
 */
 static void mpt3sas_driver_release(struct config_item *item)
{
	kfree(to_mpt3sas_driver(item));
}

CONFIGFS_ATTR_OPS(mpt3sas_driver);
static struct configfs_item_operations mpt3sas_driver_item_ops = {
	.show_attribute = mpt3sas_driver_attr_show,
	.store_attribute = mpt3sas_driver_attr_store,
	.release	= mpt3sas_driver_release,
};

static struct configfs_group_operations mpt3sas_driver_group_ops = {
	.make_group	= mpt3sas_driver_make_group,
};

static struct config_item_type mpt3sas_driver_type = {
	.ct_item_ops	= &mpt3sas_driver_item_ops,
	.ct_group_ops	= &mpt3sas_driver_group_ops,
	.ct_attrs	= mpt3sas_driver_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct configfs_subsystem mpt3sas_config_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "mpt3sas",
			.ci_type = &mpt3sas_driver_type,
		},
	},
};

/**
 * mpt3sas_configfs_reset_handler - ioc reset controller in configfs
 * @ioc: per adapter object
 * @reset_phase: phase
 *
 * The handler for doing any required cleanup or initialization.
 *
 */
void
mpt3sas_configfs_reset_handler(struct MPT3SAS_ADAPTER *ioc, int reset_phase)
{
	struct visibility_info *vi;
	struct _sas_device *sas_device;

	switch (reset_phase) {
	case MPT3_IOC_DONE_RESET:
		if (!ioc->visibility_count) {
			mpt3sas_done_device_visibility_settings(ioc);
			return;
		}

		/* see if the entry is already  entered */
		list_for_each_entry(vi, &ioc->visibility_list, list) {
			if (vi->device_presence == DEVICE_PRESENCE_ON) {
				if (vi->mapping_mode ==
					MAPPING_MODE_ENCLOSURE_SLOT)
					sas_device =
					 _configfs_find_by_enclosure_slot(ioc,
							   vi->WWID, vi->slot);
				else if (vi->mapping_mode ==
					MAPPING_MODE_DEVICE_PERSISTENCE)
					sas_device =
					 _configfs_find_by_serial_number(ioc,
							   vi->serial_number);
				else
					sas_device = NULL;

				if (!sas_device) {
					vi->device_presence =
						DEVICE_PRESENCE_OFF;
					vi->vfid_mask = 0;
				} else {
					mpt3sas_traverse_vfid_bitmask_and_set(
					     ioc, vi, sas_device->handle);
				}
			}
		}
		mpt3sas_done_device_visibility_settings(ioc);
		break;
	default:
		break;
	}
}

/* -------------------------------------------------------------------------- */
/* main entry points                                                          */
/* -------------------------------------------------------------------------- */
static u8 mpt3sas_configfs_initialized = 0;

/**
 * mpt3sas_configfs_init - main entry point for regiteringto configfs
 *
 */
void
mpt3sas_configfs_init(void)
{
	int ret;

	config_group_init(&mpt3sas_config_subsys.su_group);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20))
	mutex_init(&mpt3sas_config_subsys.su_mutex);
#else
	init_MUTEX(&mpt3sas_config_subsys.su_sem);
#endif
	ret = configfs_register_subsystem(&mpt3sas_config_subsys);
	if (!ret)
		mpt3sas_configfs_initialized = 1;
	else
		printk(KERN_ERR "Error %d while registering subsystem %s\n",
			ret, mpt3sas_config_subsys.su_group.cg_item.ci_namebuf);
}

/**
 * mpt3sas_configfs_exit - exit point for unregistering configfs
 *
 */
void
mpt3sas_configfs_exit(void)
{
	if (mpt3sas_configfs_initialized)
		configfs_unregister_subsystem(&mpt3sas_config_subsys);
}

#endif /* CONFIG_CONFIGFS_FS_MODULE */
