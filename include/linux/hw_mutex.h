/*
 *  include/linux/hw_mutex.h
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2011-2012 Intel Corporation. All rights reserved.
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
 *
 */
#ifndef LINUX_HW_MUTEX_H
#define LINUX_HW_MUTEX_H


#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <asm/atomic.h>


/* 4 masters */
#define MASTER_TOTAL 			(4)


#define HW_MUTEX_DEV_ID		0x0949


/* HW MUTEX controller working mode */
typedef enum {
	HW_MUTEX_POLLING = 0,
	HW_MUTEX_FIFO_SCHE,
	HW_MUTEX_NULL_SCHE
} hw_mutex_mode_type;
/* Totally four masters are supported */
typedef enum {
	MASTER_ARM11 = 0,
	MASTER_PP =1 ,
	MASTER_ATOM = 2 ,
	MASTER_RESV
} hw_mutex_master_type;

/* 0: SPI_NOR_FLASH, 1: eMMC, 2,3: Mailbox*/
#define HW_MUTEX_TOTAL 			(5)
/* Devices use HW mutex */
typedef enum {
	HW_MUTEX_NOR_SPI = 0,
	HW_MUTEX_EMMC = 1,
	HW_MUTEX_ARM_MBX = 2,
	HW_MUTEX_ATOM_MBX = 3,
	HW_MUTEX_GPIO = 4,
	HW_MUTEX_RESV
} hw_mutex_device_type;


/* Each HW mutex is controlled by a software mutex */
struct hw_mutex {
		hw_mutex_device_type lock_name;
		spinlock_t irq_lock;
		struct mutex lock;
		struct thread_info	*owner; /* Which thread owns the MUTEX */
		atomic_t	status;			/* 1: unlocked, 0: requesting, negative: locked, possible waiters */

#define	HW_MUTEX_LOCKED  	(-1)	/* The MUTEX is locked 							*/
#define	HW_MUTEX_REQUESTING (0)		/* We've requested for the MUTEX, but not get it yet 	*/
#define	HW_MUTEX_UNLOCKED  	(1)		/* The HW mutex is free 							*/

}__attribute__((aligned(4)));

struct hw_master {
	hw_mutex_master_type master;
	hw_mutex_mode_type mode; 		/* polling , fifo_interrupt, null_interrupt */
	uint32_t irq_num;				/* The irq number used for HW mutex */
	void __iomem *reg_base;			/* Mapped io reg base address */
	struct pci_dev *dev;
	struct hw_mutex hw_mutexes[HW_MUTEX_TOTAL];
	struct hw_mutex_operations *ops;
}__attribute__((aligned(4)));

/* Abstraction oprations of a HW mutex */
struct hw_mutex_operations {
	char *name;
	void (*clr_intr)(struct hw_master* pmaster);
	int (*lock)(struct hw_mutex* hmutex, int force);
	void (*unlock)(struct hw_mutex* hmutex);
	uint8_t (*is_locked)(struct hw_mutex* hmutex);
	uint8_t (*is_waiting)(struct hw_mutex* hmutex);
}__attribute__((aligned(4)));


/*
  * hw_mutex_lock - acquire the mutex
  * @mutex: the mutex to be acquired
  *
  * Lock the mutex exclusively for this task. If the mutex is not
  * available right now, it will sleep until we can get it.
  *
  * The function is non interruptible
  */

extern void hw_mutex_lock(uint8_t mutex);

/*
 * hw_mutex_lock_interruptible - acquire the mutex
 * @mutex: the mutex to be acquired
 *
 * Lock the mutex exclusively for this task. If the mutex is not
 * available right now, it will sleep until it can get it.
 * It can be interruptibed by signal, or exit when timeout
 *
 * Returns 0 if success, negative if interrupted or timeout
 */

extern long __must_check hw_mutex_lock_interruptible(uint8_t mutex);


/* hw_mutex_is_locked - check whether the current master owns the mutex or not
 * @mutex: the mutex number to be checked
 *
 * Return 1, if current master owns the mutex
 * Return 0, if not
 * Return Negative for errors
*/

extern int __must_check hw_mutex_is_locked(uint8_t mutex);

/*
 * hw_mutex_unlock - release the mutex
 * @mutex: the mutex to be released
 *
 * release the mutex got previously
 *
 */
extern void hw_mutex_unlock(uint8_t mutex);


//#define HW_MUTEX_DEBUG 1
#ifdef HW_MUTEX_DEBUG
//#   define DEBUG_PRINT(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#define DEBUG_PRINT printk("\nLine %d.%s: ->\n",__LINE__,__FUNCTION__)
#define DEBUG_PRINTK(fmt, args...) printk("%d@%s " fmt,__LINE__,__FUNCTION__ , ## args)

#else
#define DEBUG_PRINT do{} while (0)
#define DEBUG_PRINTK(fmt, args...) do{} while (0)

#endif



#endif
