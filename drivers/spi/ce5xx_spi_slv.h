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
 * File Name: ce5xx_spi_slv.h
 * Driver for  SPI Slave controller
 *------------------------------------------------------------------------------
 */

#ifndef __LINUX_CE5XX_SPI_SLV_H
#define __LINUX_CE5XX_SPI_SLV_H

#define SPI_SLV_SSCR0  0x0  /* SSP control Register 0 */
#define SPI_SLV_SSCR1  0x4  /* SSP control Register 1 */
#define SPI_SLV_SSSR   0x8  /* SSP status Register*/
#define SPI_SLV_SSDR   0x10  /* SSP Dta Read/Write Register*/

#define  SPI_SLV_FIFO_DEEP   32  /* SPI SLAVE FIFO deep 32*/
#define  SPI_SLV_FIFO_WIDTH  32 /* SPI SLAVE FIFO width  32bits */

#define  SPI_SLV_SSE_OFFSET  7  /* SSP enable bit's offset in SSCR0 */
#define  SPI_SLV_SSE   (1 << SPI_SLV_SSE_OFFSET)   /* SSP enable bit in SSCR0 */

#define  SPI_SLV_FRF_OFFSET  5  /* Frame Format control bits' offset in SSCR0 */
#define  SPI_SLV_FRF_MSK     (0x3 << SPI_SLV_FRF_OFFSET) /*Frame Format control bits mask */

#define  SPI_SLV_DSS_OFFSET     0   /*FIFO width select bits' offset in SSCR0*/
#define  SPI_SLV_DSS_WIDTH      5   /*FIFO width select bits' length */
#define  SPI_SLV_DSS_MSK     ( (( 1 << (SPI_SLV_DSS_WIDTH)) -1) << SPI_SLV_DSS_OFFSET)

#define  SPI_SLV_DSS_8         0x7  /* FIFO width 8 bit */
#define  SPI_SLV_DSS_16        0xF  /* FIFO width 16 bit */
#define  SPI_SLV_DSS_32        0x1F  /* FIFO width 32 bit */

#define  SPI_SLV_STRF_OFFSET    17   /*Select FIFO for EFWR's bit offset in SSCR1 */
#define  SPI_SLV_STRF        (1 << SPI_SLV_STRF_OFFSET)  /*Select FIFO for EFWR*/

#define  SPI_SLV_EFWR_OFFSET    16   /* Enable EFWR bit offset in SSCR1 */
#define  SPI_SLV_EFWR        (1 << SPI_SLV_EFWR_OFFSET)  /*Enable EFWR*/

#define  SPI_SLV_RFT_OFFSET   11 /* Receiver FIFO threshold in SSCR1 */
#define  SPI_SLV_RFT_DEEP     SPI_SLV_FIFO_DEEP /* threshold deep*/
#define  SPI_SLV_RFT_MSK      ((SPI_SLV_FIFO_DEEP -1) << SPI_SLV_RFT_OFFSET) /*RFT mask*/


#define  SPI_SLV_TFT_OFFSET   6 /* Transmitter FIFO threshold in SSCR1 */
#define  SPI_SLV_TFT_DEEP     SPI_SLV_FIFO_DEEP /* threshold deep*/
#define  SPI_SLV_TFT_MSK      ((SPI_SLV_FIFO_DEEP -1) << SPI_SLV_TFT_OFFSET) /*RFT mask*/


#define  SPI_SLV_TIE_OFFSET   1 /* Transmitter FIFO interrupt bit 's offset  in SSCR1 */
#define  SPI_SLV_TIE         (1 << SPI_SLV_TIE_OFFSET)


#define  SPI_SLV_RIE_OFFSET   0 /* Receiver FIFO interrupt in SSCR1 */
#define  SPI_SLV_RIE          (1 << SPI_SLV_RIE_OFFSET)

#define  SPI_SLV_RFL_OFFSET   13  /* Entries in Receiver FIFO bits's offset in SSSR*/
#define  SPI_SLV_RFL_MSK         ((SPI_SLV_FIFO_DEEP - 1) << SPI_SLV_RFL_OFFSET) /*Enties in Reciever FIFO MSK*/

#define  SPI_SLV_TFL_OFFSET   8  /* Entries in Transmitter FIFO bits's offset in SSSR*/
#define  SPI_SLV_TFL_MSK         ((SPI_SLV_FIFO_DEEP - 1) << SPI_SLV_TFL_OFFSET) /*Enties in Transmitter FIFO MSK*/

#define  SPI_SLV_ROR_OFFSET   7  /* SPI SLV overrun bit 's offset in SSSR */
#define  SPI_SLV_ROR         (1 << SPI_SLV_ROR_OFFSET)

#define  SPI_SLV_RFS_OFFSET  6 /* SPI SLV Reciever FIFO Interrupt bit offset in SSSR */
#define  SPI_SLV_RFS         (1 << SPI_SLV_RFS_OFFSET)

#define  SPI_SLV_TFS_OFFSET  5 /*SPI SLV Transmiter FIFO interrupt bit offset in SSSR */
#define  SPI_SLV_TFS         (1 << SPI_SLV_TFS_OFFSET)

#define  SPI_SLV_BSY_OFFSET 4 /* SPI SLV BUSY bit offset in SSSR */
#define  SPI_SLV_BSY    (1 << SPI_SLV_BSY_OFFSET)

#define  SPI_SLV_RNE_OFFSET 3 /* SPI SLV Receiver FIFO  is not empty bit offset in SSSR */
#define  SPI_SLV_RNE        (1 << SPI_SLV_RNE_OFFSET)

#define  SPI_SLV_TNF_OFFSET        2 /* SPI SLV Transmiter FIFO is not full bit offset in SSSR */
#define  SPI_SLV_TNF           (1 << SPI_SLV_TNF_OFFSET)

#endif //__LINUX_CE5XX_SPI_SLV_H
