/*
 * kernel/hw_mutex.c
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2011 Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *    Intel Corporation
 *    2200 Mission College Blvd.
 *    Santa Clara, CA  97052
 *
 */


#include <linux/init.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/module.h>

#include <linux/hw_mutex.h>
#include "hw_mutex_lld.h"

int  hw_mutex_register (struct hw_master *pmaster);
void hw_mutex_unregister (struct hw_master *pmaster);


static const struct pci_device_id hw_mutex_pci_tbl[] = {
        { PCI_DEVICE( 0x8086, HW_MUTEX_DEV_ID), .driver_data = 0 },
        {0},
};
MODULE_DEVICE_TABLE(pci, hw_mutex_pci_tbl);


static inline struct hw_master*hw_mutex_to_master(struct hw_mutex *hmutex)
{

	return container_of(hmutex, struct hw_master, hw_mutexes[hmutex->lock_name]);
}
/* __hw_mutex_clear_interrupt_status
 * Write 1 to clear the interrupts
 */
static inline void __hw_mutex_clear_interrupt_status(struct hw_master * pmaster)
{
	hw_mutex_set_reg(pmaster->reg_base + HW_MUTEX_INTR,HW_MUTEX_INTR_IC_BIT(pmaster->master));
}

/* __hw_mutex_is_waiting - check whether we're waiting for the HW mutex
 *
 * Return 1, if current master waiting on the mutex
 * Return 0, if not
*/
static inline uint8_t  __hw_mutex_is_waiting(struct hw_mutex *hmutex)
{
	struct hw_master * pmaster = hw_mutex_to_master(hmutex);

	return hw_mutex_read_and_test_bits(pmaster->reg_base + hw_mutex_waits[pmaster->master],BIT(hmutex->lock_name));
}

/* __hw_mutex_is_locked
 *
 * Return 1, if current master owns the mutex
 * Return 0, if not
*/
static inline uint8_t __hw_mutex_is_locked(struct hw_mutex *hmutex)
{
	struct hw_master * pmaster = hw_mutex_to_master(hmutex);
	return hw_mutex_read_and_test_bits(pmaster->reg_base + hw_mutex_owns[pmaster->master], BIT(hmutex->lock_name));
}


/*
  * __lock_hw_mutex - low level function to lock HW mutex
  *
  * check HW mutex status first
  *  When force == 0, check whether a new locking request is needed before we make the request
  *  When force ===1, make a new reqeust forceblly
  * Return 1: locked, 0: unlocked
 */
static inline int __lock_hw_mutex(struct hw_mutex *hmutex, int force)
{
	struct hw_master * pmaster = hw_mutex_to_master(hmutex);
	int retval = 0;
	if (!force){
		if (unlikely(__hw_mutex_is_waiting(hmutex))) return 0;
		else if (unlikely(__hw_mutex_is_locked(hmutex))) return 1;
	}
	/* Make sure we're doing a new request */
	retval = hw_mutex_read_and_test_bits(pmaster->reg_base + hw_mutex_locks[pmaster->master] + (hmutex->lock_name<<2),HW_MUTEX_MTX_UNLOCK_BIT);

	if (!retval)
		atomic_set(&hmutex->status,HW_MUTEX_REQUESTING);
	else
		atomic_set(&hmutex->status,HW_MUTEX_LOCKED);
	return retval;
}

/*
 * __unlock_hw_mutex - check HW mutex status, and unlock the mutex if we own it
 */
static inline void __unlock_hw_mutex(struct hw_mutex *hmutex)
{
	struct hw_master * pmaster = hw_mutex_to_master(hmutex);
	if (unlikely(!__hw_mutex_is_locked(hmutex))) return ;
	hw_mutex_set_reg(pmaster->reg_base + hw_mutex_locks[pmaster->master] + (hmutex->lock_name<<2),HW_MUTEX_MTX_UNLOCK_BIT);
	atomic_set(&hmutex->status,HW_MUTEX_UNLOCKED);
	return ;
}
/*
 * hw_mutex_unlock_all - unlock all of the HW mutexes at start-up
 */
static void hw_mutex_unlock_all(struct hw_master * pmaster)
{
	int i = 0;
	/* Initialize critical structures */
	for (i = 0; i< HW_MUTEX_TOTAL; i++){
		__unlock_hw_mutex(&pmaster->hw_mutexes[i]);
	}
	return;
}
/*
 * __set_hw_mutex - Set the maser working in FIFO/NULL or polling mode
 */
static void __set_hw_mutex(struct hw_master * pmaster,hw_mutex_mode_type mode)
{
	switch(mode){
		case HW_MUTEX_POLLING:
			hw_mutex_read_and_set_bits(pmaster->reg_base + HW_MUTEX_CFG,HW_MUTEX_CFG_IP_BIT);
			break;
		case HW_MUTEX_NULL_SCHE:
			hw_mutex_read_and_set_bits(pmaster->reg_base + HW_MUTEX_CFG, HW_MUTEX_CFG_IP_BIT);
			hw_mutex_read_and_set_bits(pmaster->reg_base + hw_mutex_cntls[pmaster->master],HW_MUTEX_CNTL_NF_BIT);
			break;
		case HW_MUTEX_FIFO_SCHE:
			hw_mutex_read_and_set_bits(pmaster->reg_base + HW_MUTEX_CFG, HW_MUTEX_CFG_IP_BIT);
			hw_mutex_read_and_set_bits(pmaster->reg_base + hw_mutex_cntls[pmaster->master],HW_MUTEX_CNTL_NF_BIT);
			break;
		default:
			printk(KERN_ERR "error mutex working mode\n");
	}
	pmaster->mode	= mode;
	return;
}
static struct hw_mutex_operations hw_mutex_ops = {
	.name			= "hw-mutex-ops",
	.lock			= __lock_hw_mutex,
	.unlock			= __unlock_hw_mutex,
	.is_locked		= __hw_mutex_is_locked,
	.is_waiting 	= __hw_mutex_is_waiting,
	.clr_intr		= __hw_mutex_clear_interrupt_status,
};

#ifdef CONFIG_PM
int hw_mutex_device_suspend(struct device *dev)
{
//	struct pci_dev *pdev = to_pci_dev(dev);
	int ret = 0;

	return ret;
}

int hw_mutex_device_resume(struct device *dev)
{
//	struct pci_dev *pdev = to_pci_dev(dev);
	int ret = 0;

	/*pci device restore*/
	return ret;
}
#endif
/*
 * driver entry point
 */
static int hw_mutex_probe(struct pci_dev *pdev,
                                const struct pci_device_id *id)
{
	int i, ret = -ENODEV;
	resource_size_t mem_iobase;
	unsigned long mem_iosize;
	struct hw_master *pmaster;
	DEBUG_PRINT;

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_enable_device failed.\n");
		return ret;
	}

	mem_iobase = pci_resource_start(pdev,0);
	mem_iosize = pci_resource_len(pdev,0);
	printk(KERN_INFO "mem_iobase = 0x%x, mem_iosize = 0x%x\n",(unsigned int)mem_iobase,(unsigned int)mem_iosize);

	if (pci_request_regions(pdev, "hw-mutex")){
		dev_err(&pdev->dev, "Cannot obtain PCI resources\n");
		ret = -EBUSY;
		goto free_dev;
	}

	pmaster = kzalloc(sizeof(struct hw_master), GFP_KERNEL);
	if (!pmaster){
		dev_err(&pdev->dev, "Cannot allocate memory\n");
		ret = -ENOMEM;
		goto free_resource;
	}
	/* Initialize critical structures */
	for (i = 0; i< HW_MUTEX_TOTAL; i++){
			pmaster->hw_mutexes[i].lock_name = i;
			spin_lock_init(&pmaster->hw_mutexes[i].irq_lock);
			mutex_init(&pmaster->hw_mutexes[i].lock);
			pmaster->hw_mutexes[i].owner = NULL;
			atomic_set(&pmaster->hw_mutexes[i].status,HW_MUTEX_UNLOCKED);
	}

	pmaster->reg_base = (void __iomem *)ioremap_nocache(mem_iobase,mem_iosize);
	if (!pmaster->reg_base) {
		dev_err( &pdev->dev, "error, failed to ioremap mutex registers\n");
		ret = -ENOMEM;
		goto free_mem;
	}

	/* We're running in ATOM */
	pmaster->master = MASTER_ATOM;
	pmaster->irq_num = pdev->irq;
	pmaster->dev = pdev;
	pmaster->ops = &hw_mutex_ops;

	pci_set_drvdata(pmaster->dev,pmaster);

	printk(KERN_INFO "pmaster 0x%x mem_base 0x%x, io_size 0x%x,irq_num %d, reg_base 0x%x\n",(uint32_t)pmaster, (uint32_t)mem_iobase,(uint32_t)mem_iosize,pmaster->irq_num,(uint32_t)pmaster->reg_base);

	/* HW mutex is configured to be fifo scheduler mode by default */
	/* Do not config the settings since BIOS already do that */
#if defined(CONFIG_MUTEX_FIFO)
	pmaster->mode = HW_MUTEX_FIFO_SCHE;
#elif defined(CONFIG_MUTEX_NULL)
	pmaster->mode = HW_MUTEX_NULL_SCHE;
#else
	pmaster->mode = HW_MUTEX_POLLING;
#endif

	if (hw_mutex_register(pmaster)){
		ret = -EINTR;
		goto free_iomem;
	}
	printk(KERN_INFO "Intel(R) HW MUTEX driver built on %s @ %s\n", __DATE__, __TIME__);
	return 0;
	pci_set_drvdata(pmaster->dev,NULL);
free_iomem:
	iounmap(pmaster->reg_base);
free_mem:
	kfree(pmaster);
free_resource:
	pci_release_regions(pdev);
free_dev:
	pci_disable_device(pdev);

	return ret;
}
/*
 * driver exit point
 */
static void hw_mutex_remove(struct pci_dev *pdev)
{
	struct hw_master * pmaster = pci_get_drvdata(pdev);
	if (!pmaster)
		return;
	hw_mutex_unregister(pmaster);
	/* Unlock all mutexes when driver exit */
	pci_set_drvdata(pmaster->dev,NULL);
	hw_mutex_unlock_all(pmaster);
	iounmap(pmaster->reg_base);
	kfree(pmaster);
	pci_release_regions(pmaster->dev);
	pci_disable_device(pmaster->dev);
	DEBUG_PRINT;
	printk(KERN_INFO "hw-mutex : device exit \n");

	return;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops hw_mutex_pm_ops = {
	.suspend	= hw_mutex_device_suspend,
	.resume		= hw_mutex_device_resume,
};
#endif

static struct pci_driver hw_mutex_driver = {
        .name           = "ce-hw-mutex",
        .id_table       = hw_mutex_pci_tbl,
        .probe          = hw_mutex_probe,
        .remove			= hw_mutex_remove,
#ifdef CONFIG_PM
		.driver.pm 		= &hw_mutex_pm_ops,
#endif
};
static int __init hw_mutex_lld_init (void)
{

	return pci_register_driver(&hw_mutex_driver);

}
static void __exit hw_mutex_lld_exit(void)
{
	pci_unregister_driver(&hw_mutex_driver);
}

subsys_initcall(hw_mutex_lld_init);
module_exit(hw_mutex_lld_exit);

MODULE_DESCRIPTION("Intel(R) HW MUTEX DEVICE Driver");
MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");
