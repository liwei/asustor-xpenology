/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Semiconductor
// Copyright (C) 2002-2009, all rights reserved
// Elliptic Semiconductor, Inc.
//
// As part of our confidentiality  agreement, Elliptic Semiconductor and
// the Company, as  a  Receiving Party, of  this  information  agrees to
// keep strictly  confidential  all Proprietary Information  so received
// from Elliptic Semiconductor. Such Proprietary Information can be used
// solely for  the  purpose  of evaluating  and/or conducting a proposed
// business  relationship  or  transaction  between  the  parties.  Each
// Party  agrees  that  any  and  all  Proprietary  Information  is  and
// shall remain confidential and the property of Elliptic Semiconductor.
// The  Company  may  not  use  any of  the  Proprietary  Information of
// Elliptic Semiconductor for any purpose other  than  the  above-stated
// purpose  without the prior written consent of Elliptic Semiconductor.
//
//-----------------------------------------------------------------------
//
// Project:
//
//   ELP - SPACC
//
// Description:
//
//
// This file maps defines a system abstraction layer for some generic
// functions and data structures
//
//
//-----------------------------------------------------------------------
//
// Language:         C
//
//
// Filename:         $Source: /home/repository/cvsrep/common_sdk/include/elptypes.h,v $
// Current Revision: $Revision: 1.19 $
// Last Updated:     $Date: 2009-08-28 17:00:47 $
// Current Tag:      $Name: EPN1802_MINDSPEED_27SEP2010 $
//
//
//-----------------------------------------------------------------------*/

#ifndef _ELPTYPES_H_
#define _ELPTYPES_H_

#include <linux/kernel.h>       /* printk() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/delay.h>        /* udelay */
#include <linux/sched.h>
#include <linux/kdev_t.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>         /* wait queue */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#ifndef NO_PCI_H
#include <linux/pci.h>
#endif
#include <asm/system.h>         /* cli(), *_flags, mb() */
#include <asm/uaccess.h>        /* copy_*_user */
#include <asm/io.h>             /* memcpy_fromio */

#define MEM_FREE_MAP(d,s,v,r)	dma_free_coherent(d,s,v,(dma_addr_t)(r))
#define MEM_ALLOC_MAP(d,s,r)	dma_alloc_coherent(d,s,(dma_addr_t *)(r), GFP_KERNEL)
#define MEM_UNMAP(d,m,s)	dma_unmap_single(d,(dma_addr_t)(m),(size_t)(s),DMA_BIDIRECTIONAL)
#define MEM_MAP(d,m,s)		(U32*)dma_map_single(d,m,(size_t)(s),DMA_BIDIRECTIONAL)

#define USE_IT(x) ((x)=(x))

typedef unsigned int   UINT;
typedef char C8;                /* just normal char for compatability with sysytem functions */
typedef unsigned char U8;       /* unsigned 8-bit  integer */
typedef unsigned short U16;     /* unsigned 16-bit integer */
typedef unsigned int U32;       /* unsigned 32-bit integer */
typedef unsigned long long U64; /* unsigned 64-bit integer */
typedef signed char S8;         /* 8-bit  signed integer */
typedef signed short S16;       /* 16-bit signed integer */
typedef signed int S32;         /* 32-bit signed integer */
typedef signed long long S64;   /* 64 bit signed integer */

//#define ELP_BOARD_VENDOR_ID   0xE117

typedef struct elp_id_s
{

  U16 vendor_id;
  U16 device_id;
  struct pci_dev *data;

} elp_id;

// This structure defines control and access registers and memory regions
// mapped into host virtual and physical memory
// it also holds a bus control and config information
typedef struct elphw_if_s
{
  // Virtual address mapping
  volatile void *pmem;          //  Bar0
  volatile void *preg;          //  Bar1
  volatile char *preg2;         //  Bar1
  volatile char *pbar2;         //  Bar2
  volatile char *pbar3;         //  Bar3
  volatile char *pbar4;         //  Bar4

} elphw_if;

void elppci_get_vendor(elphw_if *tif);
void elppci_set_little_endian(elphw_if *tif);
void elppci_interrupt_enabled(elphw_if *tif);
void elppci_set_little_endian(elphw_if *tif);
void elppci_set_big_endian(elphw_if *tif);
void elppci_reset (elphw_if * tif);
int elppci_init (elp_id * tid, elphw_if * tif, U32 flags);
void elppci_cleanup (elphw_if * tif);
void elppci_info (elphw_if * tif);

#endif
