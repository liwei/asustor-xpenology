/*
#
#  This file is provided under a dual BSD/GPLv2 license.  When using or
#  redistributing this file, you may do so under either license.
#
#  GPL LICENSE SUMMARY
#
#  Copyright(c) 2011-2012 Intel Corporation. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#  The full GNU General Public License is included in this distribution
#  in the file called LICENSE.GPL.
#
#  Contact Information:
#  intel.com
#  Intel Corporation
#  2200 Mission College Blvd.
#  Santa Clara, CA  95052
#  USA
#  (408) 765-8080
#
#*/
/*------------------------------------------------------------------------------
 * File Name: ce5xx_spi_slv.c
 * Driver for  SPI Slave controller
 *------------------------------------------------------------------------------
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/pci.h>
#include <linux/circ_buf.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "ce5xx_spi_slv.h"

#ifdef DEBUG
#define spi_slv_dbg(fmt, args...) do \
    { \
	            printk(KERN_INFO fmt, ##args); \
	    } while (0)
#else
#define spi_slv_dbg(fmt, arg...) do { } while (0)
#endif

struct spi_slv{
     /* lock to protect the critical area */
	 spinlock_t lock;
	 /* Driver model hookup */
	 struct pci_dev *pdev;
	 struct spi_master *master;

	 /* SSP register addresses */
	 void __iomem *ioaddr;
	 u32 phy_base;
	 u32 phy_len;
	 s32 irq;


#define SPI_SLV_CIRC_SIZE    (4*1024)
 /* tx/rv circ buf */
	struct circ_buf tx;
	struct circ_buf rx;

	u32 shift_per_word;

    int waiting_len;
	 /* Driver message queue and workqueue */
	 struct workqueue_struct *workqueue;
	 struct work_struct work;
	 struct list_head queue;

	 struct completion done;


	 int (*transmitter)(struct spi_slv *slv);
	 int (*receiver)(struct spi_slv *slv);

 };

static int buffer_size = SPI_SLV_CIRC_SIZE;
module_param(buffer_size, int, S_IRUGO);

static inline void  slv_circs_clear(struct spi_slv *slv)
{
		 slv->tx.head = slv->tx.tail = 0;
		 slv->rx.head = slv->rx.tail = 0;
}
 static inline  int slv_circ_cnt(struct circ_buf *circ)
 {

		return CIRC_CNT(circ->head, circ->tail, buffer_size);
 }

 static inline int slv_circ_space(struct circ_buf *circ)
 {

		return CIRC_SPACE(circ->head, circ->tail, buffer_size);
 }

 static inline bool slv_circ_full(struct circ_buf *circ)
 {

		return !CIRC_SPACE(circ->head, circ->tail, buffer_size);
 }

 static inline bool slv_circ_empty(struct circ_buf *circ)
 {
		return (circ->head == circ->tail);
 }

 static inline void __slv_write32(struct spi_slv *slv, u32 reg_offset, u32 value)
 {
	 void __iomem *addr = (void __iomem *)slv->ioaddr;
	 __raw_writel(value, addr + reg_offset);

 }
 static inline u32  __slv_read32(struct spi_slv *slv, u32 reg_offset)
 {
	 void __iomem *addr = (void __iomem *)slv->ioaddr;
	 return __raw_readl(addr + reg_offset);
 }

 static int slv_u8_transmitter(struct spi_slv *slv)
 {
	struct circ_buf *circ = &slv->tx;
	int count = 0;

	while ((__slv_read32(slv,SPI_SLV_SSSR) & SPI_SLV_TNF)
			&& (slv_circ_cnt(circ) > 0)) {
			__slv_write32(slv, SPI_SLV_SSDR, *(u8 *)(circ->buf + circ->tail));
			circ->tail = ((circ->tail + 1) & (buffer_size - 1));
		   count++;
	}
	return count;
 }

 static int slv_u8_receiver(struct spi_slv *slv)
 {
	struct circ_buf *circ = &slv->rx;
	u8 ch;
	int count = 0;
	 while (__slv_read32(slv,SPI_SLV_SSSR) & SPI_SLV_RNE) {
		 ch = __slv_read32(slv,SPI_SLV_SSDR);
		 if (slv_circ_space(circ) > 0) {
			*(u8 *)(circ->buf + circ->head) = ch;
			circ->head = ((circ->head + 1) & (buffer_size - 1));
			count++;
		 }
	 }
	return count;
  }

 static int slv_u16_transmitter(struct spi_slv *slv)
 {
	 struct circ_buf *circ = &slv->tx;
	 int count = 0;

	 while ((__slv_read32(slv,SPI_SLV_SSSR) & SPI_SLV_TNF)
			 && (slv_circ_cnt(circ) >= 2 )) {
			 __slv_write32(slv, SPI_SLV_SSDR, *(u16 *)(circ->buf + circ->tail));
			 circ->tail = ((circ->tail + 2) & (buffer_size - 1));
			count += 2;
	 }

	return count;

 }

 static int slv_u16_receiver(struct spi_slv *slv)
 {
	 struct circ_buf *circ = &slv->rx;
	 u16 ch;
	 int count = 0;
	 while (__slv_read32(slv,SPI_SLV_SSSR) & SPI_SLV_RNE) {
		  ch = __slv_read32(slv,SPI_SLV_SSDR);
		  if (slv_circ_space(circ) >= 2) {
			 *(u16 *)(circ->buf + circ->head) = ch;
			 circ->head = ((circ->head + 2) & (buffer_size - 1));
			 count += 2;
		  }
	 }
	 return count;
 }

 static int slv_u32_transmitter(struct spi_slv *slv)
 {
	struct circ_buf *circ = &slv->tx;
	int count = 0;

	while ((__slv_read32(slv,SPI_SLV_SSSR) & SPI_SLV_TNF)
		 && (slv_circ_cnt(circ) >= 4 )) {
		 __slv_write32(slv, SPI_SLV_SSDR, *(u32 *)(circ->buf + circ->tail));
		 circ->tail = ((circ->tail + 4) & (buffer_size - 1));
		count++;
	 }

	return count;

 }

 static int slv_u32_receiver(struct spi_slv *slv)
 {
	 struct circ_buf *circ = &slv->rx;
	 u32 ch;
	 int count = 0;
	 while (__slv_read32(slv,SPI_SLV_SSSR) & SPI_SLV_RNE) {
		  ch = __slv_read32(slv,SPI_SLV_SSDR);
		  if (slv_circ_space(circ) >= 4) {
			 *(u32 *)(circ->buf + circ->head) = ch;
			 circ->head = ((circ->head + 4) & (buffer_size - 1));
			 count += 4;
		  }
	 }
	 return count;

}

/*slv is configureed and enabled*/
static int spi_slv_setup(struct spi_device *spi)
{
	struct spi_slv *slv = spi_master_get_devdata(spi->master);
	int ret = 0;
	u32 dds = 0, value = 0;

	if (list_empty(&slv->queue)) {
		switch (spi->bits_per_word) {
			case 8:
				slv->shift_per_word = 0;
				slv->transmitter = slv_u8_transmitter;
				slv->receiver = slv_u8_receiver;
				dds = SPI_SLV_DSS_8;
				break;

			case 16:
				slv->shift_per_word = 1;
				slv->transmitter = slv_u16_transmitter;
				slv->receiver = slv_u16_receiver;
				dds = SPI_SLV_DSS_16;
				break;

			case 32:
				slv->shift_per_word = 2;
				slv->transmitter = slv_u32_transmitter;
				slv->receiver = slv_u32_receiver;
				dds = SPI_SLV_DSS_32;
				break;

			default:  /* default use 8 bits */
			  spi->bits_per_word = 8;
				break;
		}
	} else {
		ret = -EBUSY;
	}
	if (!ret) {
		value = __slv_read32(slv, SPI_SLV_SSCR0);
		__slv_write32(slv, SPI_SLV_SSCR0, value & ~SPI_SLV_SSE);
		mdelay(10);

		__slv_write32(slv, SPI_SLV_SSCR0, SPI_SLV_SSE | dds | (0x1 << 5));
		mdelay(10);
		__slv_write32(slv, SPI_SLV_SSCR1, SPI_SLV_RIE ); /*fix me  define the macro for it */
		slv_circs_clear(slv);

	}

	return ret;

}

static void spi_slv_cleanup(struct spi_device *spi)
{
	struct spi_slv *slv = spi_master_get_devdata(spi->master);
	u32 value;

	value = __slv_read32(slv, SPI_SLV_SSCR0);
	__slv_write32(slv, SPI_SLV_SSCR0, value & ~SPI_SLV_SSE);
	slv_circs_clear(slv);

}

static int spi_slv_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct spi_slv *slv = spi_master_get_devdata(spi->master);
	int ret = 0;

	m->actual_length = 0;
	m->status = -EINPROGRESS;
	spin_lock(&slv->lock);
	list_add_tail(&m->queue, &slv->queue);
	queue_work(slv->workqueue, &slv->work);
	spin_unlock(&slv->lock);
	return ret;
}
static void spi_slv_pump_messages(struct work_struct *work)
{
	struct spi_slv *slv;

	unsigned int bytes_per_word = 0;
	unsigned int mask = 0;

	slv = container_of(work, struct spi_slv, work);
	bytes_per_word = 1 << slv->shift_per_word;
	mask = ~(bytes_per_word - 1);

	spin_lock(&slv->lock);
	while(!list_empty(&slv->queue)) {
		struct spi_message *m;
		struct spi_transfer *t;

		m = container_of(slv->queue.next, struct spi_message, queue);
		list_del_init(&m->queue);
		spin_unlock(&slv->lock);

		list_for_each_entry(t, &m->transfers, transfer_list) {
			int count = 0, c = 0;

			slv->waiting_len = 0;
			t->len = t->len & mask;
			if (t->tx_buf) {
				struct circ_buf *circ= &slv->tx;
				count = t->len;

					while(1) {
						if (0 == count) break;
						c = CIRC_SPACE_TO_END(circ->head, circ->tail, buffer_size);
						c &= mask;
						if (count < c)
							c = count;
						if (c <= 0) break;
						memcpy(circ->buf + circ->head, t->tx_buf, c);
						circ->head = (circ->head + c) & (buffer_size - 1);
						t->tx_buf += c;
						count -= c;
					}
					t->len -= count;
					disable_irq(slv->irq);
					slv->transmitter(slv);
					enable_irq(slv->irq);
			}
			if (t->rx_buf) {
				struct circ_buf *circ= &slv->rx;

				count = t->len;

					while(1) {
						if (0 == count) break;
						c = CIRC_CNT_TO_END(circ->head, circ->tail, buffer_size);
						if (count < c)
							c = count;
						if (c <= 0) {
							slv->waiting_len = count;
							wait_for_completion(&slv->done);
							continue;
						}
						memcpy(t->rx_buf, circ->buf + circ->tail, c);
						circ->tail = (circ->tail + c) & (buffer_size - 1);
						t->rx_buf += c;
						count -= c;

					}

			}
			m->actual_length += t->len;

		}
		m->status = 0;
		m->complete(m->context); /* the mesage has been handled */

		spin_lock(&slv->lock);
	}
	spin_unlock(&slv->lock);

}

 static irqreturn_t spi_slv_int(int irq, void *dev_id)
 {
	 struct spi_slv *slv = dev_id;
	 u32 mask = (SPI_SLV_ROR | SPI_SLV_TFS | SPI_SLV_RFS);
	 u32 status;

	 status = __slv_read32(slv, SPI_SLV_SSSR);

	 if (!(status & mask)) /* no interrupt comes*/
		 return IRQ_NONE;

	 if (status & SPI_SLV_ROR) {
		__slv_write32(slv, SPI_SLV_SSSR, status);
		printk(KERN_ERR "Error:SPI SLAVE overrun!!\n");

	 }

	slv->receiver(slv);

	slv->transmitter(slv);

	 if (slv->waiting_len > 0) {
		int left;

		left = slv_circ_cnt(&slv->rx) - slv->waiting_len;

		if (left >= 0){
		    slv->waiting_len = 0;
			complete(&slv->done);
		}
	 }
	 return IRQ_HANDLED;

 }


static struct spi_board_info info[1];

static int __devinit spi_slv_probe(struct pci_dev *pdev,
		 const struct pci_device_id *id)
 {
	int ret = 0;
	struct pci_bus *pbus;
	struct pci_dev *root;
	struct spi_master *master;
	struct spi_slv *slv;

    pbus = pci_find_bus(0, 0); /*get the bus 0 in domain 0*/
	root = pci_get_slot(pbus, 0); /*get pci root above bus 0 D0F0*/

	switch (root->device) {

		case  0x0c40:
		   spi_slv_dbg("CE5300 board,SLV is found!\n");/*only CE5300 have spl slave controller*/
		   break;
		default:
		   return ret = -ENODEV;
	}
    pci_dev_put(root);

	ret = pci_enable_device(pdev);
	if (ret)
		 return ret;

	 /* Allocate master with space for slv	*/
	master = spi_alloc_master(&pdev->dev, 0);
	if (!master) {
		 dev_err(&pdev->dev, "can't alloc spi_master\n");
		 return -ENOMEM;
	 }
	 /* the spi->mode bits understood by this driver: */
	 master->bus_num = id->driver_data;
	 master->num_chipselect = 1;
	 master->using_slave = 1;
	 master->setup = spi_slv_setup;
	 master->transfer = spi_slv_transfer;
	 master->cleanup = spi_slv_cleanup;

	 strcpy(info[0].modalias, "spidev");
	 info[0].irq =  pdev->irq;
	 info[0].bus_num =   master->bus_num;
	 info[0].chip_select = 0;
	 info[0].mode  = 0;
	 info[0].max_speed_hz =  0;
	 spi_register_board_info(info, 1);

	 slv = kmalloc(sizeof(struct spi_slv), GFP_KERNEL);
     if (!slv){
            dev_err(&pdev->dev, "can't alloc memory for spi_slv\n");
            goto out_unregister_board_info;
     }
     spi_master_set_devdata(master, slv); /*set master privdate data*/
	 slv->master = master;
	 slv->pdev = pdev;

	 spin_lock_init(&slv->lock);
	 init_completion(&slv->done);
	 INIT_WORK(&slv->work, spi_slv_pump_messages);
	 INIT_LIST_HEAD(&slv->queue);

	 slv->tx.buf = kmalloc(buffer_size*2, GFP_KERNEL);
	 if (!slv->tx.buf) {
		printk(KERN_ERR "circ buffer alloclation fails!\n");
		goto out_free_slv;
	 }
	 slv->rx.buf = slv->tx.buf + buffer_size;
	 slv->tx.head = slv->tx.tail = 0;
	 slv->rx.head = slv->rx.tail = 0;

	 slv->phy_base = pci_resource_start(pdev, 0);
	 slv->phy_len  = pci_resource_len(pdev, 0);
	 slv->irq = pdev->irq;

	 spi_slv_dbg("slv phy is %x, len is %x\n", slv->phy_base, slv->phy_len);
	 pci_request_region(pdev, 0, "slv");

	 slv->ioaddr = ioremap_nocache(slv->phy_base, slv->phy_len);
	 if (!slv->ioaddr) {
		 dev_err(&pdev->dev, "failed to ioremap() registers\n");
		 ret = -EIO;
		 goto out_release_regions;
	 }

	 ret = request_irq(slv->irq, spi_slv_int, IRQF_SHARED, "spi slave int", slv);
	 if (ret < 0) {
			 dev_err(&pdev->dev, "register irq %d handler failed!\n", slv->irq);
			 goto out_iounmap;
	 }


		 /* alloc work queue */
	 slv->workqueue = create_singlethread_workqueue("spi_slave");

	 if (!slv->workqueue) {
		 dev_err(&pdev->dev, "Create work queue failed!\n");
		 ret = -EBUSY;
		 goto out_free_irq;
	 }

	 /* Register with the SPI framework */

	 ret = spi_register_master(master);
	 if (ret != 0) {
		 dev_err(&pdev->dev, "problem registering spi master\n");
		 goto out_destroy_queue;
	 }

	 pci_set_drvdata(pdev, slv);
	 return ret;

out_destroy_queue:
	 destroy_workqueue(slv->workqueue);


out_free_irq:
	 free_irq(slv->irq, slv);

out_iounmap:
	 iounmap(slv->ioaddr);

out_release_regions:
	 pci_release_region(pdev, 0);
	 kfree(slv->tx.buf);

out_free_slv:
     kfree(slv);

out_unregister_board_info:
	 spi_unregister_board_info(info, 1);
     spi_master_put(master);
	 pci_disable_device(pdev);

	 return ret;
 }

static void __devexit spi_slv_remove(struct pci_dev *pdev)
 {
	 struct spi_slv *slv;
	 struct spi_message *m;
	 struct list_head *l;
	 u32 value;

	 slv = pci_get_drvdata(pdev);

	 value = __slv_read32(slv, SPI_SLV_SSCR0); /*disable hardware*/
	 __slv_write32(slv, SPI_SLV_SSCR0, value & ~SPI_SLV_SSE);

	 list_for_each(l, &slv->queue) {
		m = list_entry(l, struct spi_message, queue);
		m->status = -ESHUTDOWN;
		m->complete(m->context); /* the mesage has been handled */
	 }
	 spi_unregister_master(slv->master);
	 destroy_workqueue(slv->workqueue);

	 /* Disconnect from the SPI framework */
	 free_irq(slv->irq, slv);
	 iounmap(slv->ioaddr);
	 pci_release_region(pdev, 0);
     kfree(slv->tx.buf);
     kfree(slv);
	 spi_unregister_board_info(info, 1);
	 pci_set_drvdata(pdev, NULL);
	 pci_disable_device(pdev);

 }

#ifdef CONFIG_PM
 static int spi_slv_suspend(struct pci_dev *pdev, pm_message_t state)
 {
		 pci_save_state(pdev);
		 pci_set_power_state(pdev, pci_choose_state(pdev, state));
		 return 0;
 }

 static int spi_slv_resume(struct pci_dev *pdev)
 {
		 pci_set_power_state(pdev, PCI_D0);
		 pci_restore_state(pdev);
		 return 0;
 }
#else
#define   spi_slv_suspend NULL
#define   spi_slv_resume  NULL
#endif

 static struct pci_device_id spi_slv_id_tables[] __devinitdata = {

	 { PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x707), .driver_data = 32 },
	 {0 },
 };
 MODULE_DEVICE_TABLE(pci, spi_slv_id_tables);

 static struct pci_driver spi_slv_driver = {
	 .name			 = "ce5xxx-spi_slv",
	 .id_table		 = spi_slv_id_tables,
	 .probe 		 = spi_slv_probe,
	 .remove		 = __devexit_p(spi_slv_remove),
	 .suspend		 = spi_slv_suspend,
	 .resume		 = spi_slv_resume,

 };

 static int __init spi_slv_init(void)
 {
	 return pci_register_driver(&spi_slv_driver);
 }


 static void __exit spi_slv_exit(void)
 {
	 pci_unregister_driver(&spi_slv_driver);
 }

 module_init(spi_slv_init);
 module_exit(spi_slv_exit);

 MODULE_DESCRIPTION("CE5xxx SPI Slave code");
 MODULE_LICENSE("GPL v2");
 MODULE_AUTHOR("cxu19 <chaoxu@intel.com>");
