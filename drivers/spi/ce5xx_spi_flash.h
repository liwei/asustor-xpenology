/*
 * GPL LICENSE SUMMARY
 * Copyright (c) 2011, Intel Corporation and its suppliers.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef CE5XX_SPI_FLASH_H
#define CE5XX_SPI_FLASH_H

/* Set controller working in boot mode, using direct memory window for device read */
#define CE5XX_BOOT_MODE_ENABLE					1

#define INTEL_VENDOR_ID 						0x8086
#define CERSV1_DEVID 							0x0DC0
#define CERSV2_DEVID 							0x0932

#define CE5XX_SPI_FLASH_CONTROLLER_ID 			0x08A0
#define CE5300_SPI_FLASH_REVISION_ID			0x00
#define CE2600_SPI_FLASH_REVISION_ID			0x02


/* Controller Register Set */
#define MODE_CONTL_REG       					0

#define MODE_CONTL_CLK_RATIOR_SHIFT				0
#define MODE_CONTL_BOOT_MODE_ENABLE				(1<<4)
#define MODE_CONTL_BOOT_MODE_DISABLE			(~(1<<4))
#define MODE_CONTL_SPI_UNIT_EN					(1<<5)
#define MODE_CONTL_SS1_EN						(1<<6)

#define MODE_CONTL_CMD_WIDTH_EQUAL_TO_DATA		(0<<9)
#define MODE_CONTL_CMD_WIDTH_1_DATA_LINE		(1<<9)

#define MODE_CONTL_SPI_WIDTH_1_BIT				(1<<10)
#define MODE_CONTL_SPI_WIDTH_2_BIT				(2<<10)
#define MODE_CONTL_SPI_WIDTH_4_BIT				(3<<10)
#define MODE_CONTL_SPI_WIDTH_BIT_MASK			(~(3<<10))

#define MODE_CONTL_N_ADDR_2_BYTES				(2<<12)
#define MODE_CONTL_N_ADDR_3_BYTES				(3<<12)
#define MODE_CONTL_N_ADDR_4_BYTES				(4<<12)
#define MODE_CONTL_N_ADDR_BYTES_MASK			(~(0xf<<12))


#define MODE_CONTL_CS0_MODE_ENABLE				(1<<16)
#define MODE_CONTL_CS0_WP						(1<<18)
#define MODE_CONTL_CS0_MODE_DISABLE_MASK		(~(3<<16))

#define MODE_CONTL_CS1_MODE_ENABLE				(1<<20)
#define MODE_CONTL_CS1_WP						(1<<22)
#define MODE_CONTL_CS1_MODE_DISABLE_MASK		(~(3<<20))

#define MODE_CONTL_CS_TAR_SHIFT					24


#define ADDR_SPLIT_REG                  		0x04
#define CS0_CMP                                 0
#define CS0_MASK                                4
#define CS1_CMP                                 8
#define CS1_MASK                                12

#define CURRENT_ADDR_REG               	 		0x08
#define DATA_COMMAND_REG                		0x0C
#define DCR_CS_HOLD                             26
#define DCR_NBYTES                              24
#define DCR_NBYTES_MAX                          3
#define DCR_DATA_FIRST_BYTE                     16
#define DCR_DATA_SECOND_BYTE            		8
#define DCR_DATA_THIRD_BYTE                     0
#define DCR_DATA                                0
#define INTERFACE_CONFIG_REG            		0x10

#define HIGH_EFFICY_CMD_DATA_REG        		0x20
#define HECDR_FIRST_BYTE                        24
#define HECDR_SECOND_BYTE                       16
#define HECDR_THIRD_BYTE                        8
#define HECDR_FORTH_BYTE                        0


#define HIGH_EFFICY_TRS_PAR_REG         		0x24
#define HETPR_DUMMY_CYCLE               		28
#define HETPR_CS_HOLD                   		27
#define HETPR_NBYTES_WR                 		24
#define HETPR_NBYTES_WR_MAX     				4
#define HETPR_NBYTES_RD                 		0
#define HETPR_NBYTES_RD_MAX             		0xFFFF
#define HIGH_EFFICY_OPCODE_REG          		0x28

enum {
	SPI_FLASH_REMOVE_BIT = 1,
	SPI_FLASH_SUSPEND_BIT,
};

#define SPI_FLASH_REMOVE	(1 << SPI_FLASH_REMOVE_BIT)
#define SPI_FLASH_SUSPEND	(1 << SPI_FLASH_SUSPEND_BIT)

struct ce5xx_sflash {
	struct work_struct 			work;
	struct workqueue_struct		*workqueue;
	spinlock_t 					lock;				/* protect 'queue' */
	struct list_head 			queue;

	void __iomem 				*mem_base;
	void __iomem 				*regs_base;

	struct pci_dev 				*pdev;
	struct spi_master 			*master;

	uint16_t					mode;
	struct flash_cs_info 		*cntl_data;			/* Device size info in each chip select */
	struct ce5xx_address_split * addr_split_methd;	/* Address Split register value for the current flash layout */
	struct notifier_block 		*reboot_notifier;	/* Reboot notification */
	int  status;

	uint32_t (*transmiter)	(struct ce5xx_sflash *dev, void *buf, size_t len);
	uint32_t (*dma_receiver)(struct ce5xx_sflash *dev, void *to, uint32_t offset, size_t len);
	uint32_t (*csr_receiver)(struct ce5xx_sflash *dev, void *buf, size_t len);

	/* S2Ram stored register */
	uint32_t mode_contrl; /* mode control */
	uint32_t addr_split; /* address split */
	uint32_t cur_addr; /* current address */
	uint32_t command; /* command/data */
	uint32_t inf_conf; /* interface configuration */
	uint32_t hecd;  /* high efficiency command/data */
	uint32_t hetp;	/* high efficiency transaction parameters */
	uint32_t heop; /* high efficiency opcode */

};
struct ce5xx_address_split {
	uint32_t 	cs0_size;
	uint32_t	cs1_size;

	uint16_t	cs0_cmp:4;
	uint16_t	cs0_mask:4;
	uint16_t	cs1_cmp:4;
	uint16_t	cs1_mask:4;
}__attribute__((aligned(4)));


#endif
