/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Technologies
// Copyright (C) 2002-2009, all rights reserved
// Elliptic Technologies, Inc.
//
// As part of our confidentiality  agreement, Elliptic Technologies and
// the Company, as  a  Receiving Party, of  this  information  agrees to
// keep strictly  confidential  all Proprietary Information  so received
// from Elliptic Technologies. Such Proprietary Information can be used
// solely for  the  purpose  of evaluating  and/or conducting a proposed
// business  relationship  or  transaction  between  the  parties.  Each
// Party  agrees  that  any  and  all  Proprietary  Information  is  and
// shall remain confidential and the property of Elliptic Technologies.
// The  Company  may  not  use  any of  the  Proprietary  Information of
// Elliptic Technologies for any purpose other  than  the  above-stated
// purpose  without the prior written consent of Elliptic Technologies.
//
//-----------------------------------------------------------------------
//
// Project:
//
//   ESM
//
// Description:
//
// 	CLUE BN HARDWARE MODULE INTERFACE
//
//-----------------------------------------------------------------------
//
// Language:         C
//
//
// Filename:         $Source: /home/repository/cvsrep/clue_esm/crypto/elprsa.c,v $
// Current Revision: $Revision: %
// Last Updated:     $Date: 2009-07-29 14:56:39 $
// Current Tag:      $Name: EPN1802_MINDSPEED_27SEP2010 $
//
//
//-----------------------------------------------------------------------*/

#include <generated/autoconf.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/mount.h>
#include <linux/miscdevice.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <asm/scatterlist.h>
#include <linux/dma-mapping.h>  
#include <linux/uio.h>
#include <linux/dmapool.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
#include <linux/platform_device.h>
#endif

#include <asm/io.h>
#include "uio.h"

#include "m86xxx_spacc.h"

//PKA errors code
char *pka_errmsgs[] = {
  "Normal",
  "Invalid Opcode",
  "Fstack underflow",
  "FStack overflow",
  "Watchdog",
  "Stop Request",
  "Pstack underflow",
  "Pstack overflow",
  "Memport collision"
};

/*micro code to buffer */
U32 clue_code [] = {
#include "epn1802_microcode.h"
};
U32 rom_code [sizeof(clue_code)/sizeof(U32)];

extern struct elp_spacc_device _spacc_dev;
extern elp_pka _pka;

//load 32bit microcode
int clue_load_microcode (U32 * code, int size)
{
	U32 *clue = CLUE_CODE_MEMORY_START (_spacc_dev.reg.regmap);

        if (_spacc_dev.reg.regmap == 0) {
		DPRINTF(ELP_ERR, "%s: %d SPACC_CRYPTO_FAILED \n",__FUNCTION__,__LINE__);
        	return SPACC_CRYPTO_FAILED;
	}

        MEMCPY32 (clue, code, size);
        //dumpword (clue, size, "//microcode");
        return SPACC_CRYPTO_OK;
}

int clue_get_microcode (U32 * code, int size)
{
	U32 *clue = CLUE_CODE_MEMORY_START (_spacc_dev.reg.regmap);
	MEMCPY32 (code, clue, size);
	//dumpword (code, size, "//microcode");
	return SPACC_CRYPTO_OK;
}

//clean hardware data memory
void clue_clean_data_memory (U32 mmap)
{
  U32 *p;

  //clean memory
  p = CLUE_REGION_A_START (mmap);
  MEMSET32 (p, 0, (CLUE_A_PAGE_SIZE >> 2));
  p = CLUE_REGION_B_START (mmap);
  MEMSET32 (p, 0, (CLUE_B_PAGE_SIZE >> 2));
  p = CLUE_REGION_C_START (mmap);
  MEMSET32 (p, 0, (CLUE_C_PAGE_SIZE >> 2));
  p = CLUE_REGION_D_START (mmap);
  MEMSET32 (p, 0, (CLUE_D_PAGE_SIZE >> 2));
}

/*********************** PKA *************************************************/

static void outdiff (char *s, unsigned char *buf1, unsigned char *buf2, int len)
{
  int x, y;
  printk ("Comparing [%s]\n", s);
  printk ("\texpect\t -result\n");
  for (x = 0; x < len; x += 4) {
    printk ("%04x   ", x);
    for (y = 0; y < 4; y++)
      printk ("%02lx", (unsigned long) buf1[y] & 255UL);
    printk (" - ");
    for (y = 0; y < 4; y++)
      printk ("%02lx", (unsigned long) buf2[y] & 255UL);
    printk ("\n");
    buf1 += 4;
    buf2 += 4;
  }

}

int pka_init (U32 mmap)
{
	int ret;
	DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);

	// Initialize PKA (a.k.a CLUE) module context
	_pka.active = 1;
	_pka.ctrl = (U32 *) (mmap + CLUE_CTRL);
	_pka.entry = (U32 *) (mmap + CLUE_ENTRY);
	_pka.addr = (U32 *) (mmap + CLUE_ADDR);
	_pka.opcode = (U32 *) (mmap + CLUE_OPCODE);
	_pka.stack = (U32 *) (mmap + CLUE_STACK);
	_pka.conf_reg = (U32 *) (mmap + CLUE_CONF_REG);
	_pka.status = (U32 *) (mmap + CLUE_STATUS);
	_pka.flags = (U32 *) (mmap + CLUE_FLAGS);
	_pka.watchdog = (U32 *) (mmap + CLUE_WATCHDOG);
	_pka.cycle_since_go = (U32 *) (mmap + CLUE_CYCLE_SINCE_GO);
	_pka.inst_since_go = (U32 *) (mmap + CLUE_INST_SINCE_GO);
	_pka.index_i = (U32 *) (mmap + CLUE_INDEX_I);
	_pka.index_j = (U32 *) (mmap + CLUE_INDEX_J);
	_pka.index_k = (U32 *) (mmap + CLUE_INDEX_K);
	_pka.index_l = (U32 *) (mmap + CLUE_INDEX_L);
	_pka.int_en = (U32 *) (mmap + CLUE_INT_EN);	
	_pka.jump_prob = (U32 *) (mmap + CLUE_JUMP_PROB);	
	_pka.lfsr_seed = (U32 *) (mmap + CLUE_LFSR_SEED);

  	DPRINTF (ELP_DBG, "PKA MAPPING status @ %p, entry @ %p, control @ %p\n", _pka.status, _pka.entry, _pka.ctrl);	

	pka_dump_registers();

#ifdef CLUE_ENDIAN_BIG
	CLUE_SET_ENDIAN_SWAP (mmap);
#endif

	DPRINTF(ELP_DBG, "Initializing clue_ec_init\n");
	clue_ec_init (mmap);
	
	clue_clean_data_memory (mmap);

	ret = clue_load_microcode (clue_code, sizeof(clue_code)/sizeof(U32));
	if (ret != SPACC_CRYPTO_OK) {
	    printk ("Failed to load micro code %d\n",ret);
	    clue_get_microcode (clue_code, 32);
	    //clue_get_microcode (clue_code, 32);
  	}
#if 0
	clue_get_microcode (rom_code, sizeof(clue_code)/sizeof(U32));

	if (memcmp (rom_code, clue_code, (sizeof(clue_code)/sizeof(U32)) *4)) {
		outdiff ("ROM: FILE", rom_code, clue_code, sizeof(clue_code)/sizeof(U32));
		printk ("PKA ROM doesn't match microcode \n");
	}
	else
	{
			printk ("PKA ROM matches microcode \n");
	}
#endif
	return SPACC_CRYPTO_OK;
}

void pka_enable_int (void)
{
	ELP_WRITE_UINT(_pka.int_en,  CLUE_CTRL_BIT_IRQ_EN);
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, ELP_READ_UINT( _spacc_dev.reg.irq_enable) | SPACC_IRQ_PKA_EN);
}

void pka_disable_int (void)
{
	ELP_WRITE_UINT(_pka.int_en, 0);
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, ELP_READ_UINT(_spacc_dev.reg.irq_enable) & ~SPACC_IRQ_PKA_EN);
}

//get current register value for s/w DEBUG
void pka_dump_registers (void)
{
  DPRINTF (ELP_DBG, "Cntl             [0x00]: %08X, NxtIdxFetch      [0x04]: %08X, NxtAddrFetch     [0x08]: %08X\n",
          CLUE_READ_UINT (_pka.ctrl),
          CLUE_READ_UINT (_pka.entry),
          CLUE_READ_UINT (_pka.addr));

  DPRINTF (ELP_DBG, "StackPntr        [0x10]: %08X, Stat             [0x1c]: %08X, Flags            [0x20]: %08X\n",
          CLUE_READ_UINT (_pka.stack),
          CLUE_READ_UINT (_pka.status),
          CLUE_READ_UINT (_pka.flags));

  DPRINTF (ELP_DBG, "WatchDog         [0x28]: %X, Cycles           [0x2c]: %X, Index_I          [0x30]: %X\n",
          CLUE_READ_UINT (_pka.watchdog),
          CLUE_READ_UINT (_pka.cycle_since_go),
          CLUE_READ_UINT (_pka.index_i));

  DPRINTF (ELP_DBG, "Index_J          [0x34]: %X, Index_K          [0x38]: %X, Index_L          [0x3c]: %X\n",
          CLUE_READ_UINT (_pka.index_j),
          CLUE_READ_UINT (_pka.index_k),
          CLUE_READ_UINT (_pka.index_l));
}

void pka_halt_engine()
{
	DPRINTF(ELP_DBG, "CLUE HALT (Before)  STATUS=0x%X CTRL=0x%X\n",
		(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_CTRL (_spacc_dev.reg.regmap));
	CLUE_STOP_REQUEST (_spacc_dev.reg.regmap);
	//CLUE_INT_ACK (_spacc_dev.reg.regmap);
	DPRINTF(ELP_DBG, "CLUE HALT STATUS=0x%X CTRL=0x%X\n", (U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap),
		CLUE_READ_CTRL (_spacc_dev.reg.regmap));
}

//software gcd check 
static int
clue_bn_denominator_gcd_check (U8 * denominator, U8 * modulus, int size)
{
	return SPACC_CRYPTO_OK;
}

int clue_start_engine (int size)
{
	int ret = SPACC_CRYPTO_OK;
	U32 wait_count = CLUE_LOOP_WAIT, reason;
	unsigned long flags;

	DPRINTF(ELP_DBG, "%s: %d\n", __FUNCTION__, size);

	if (size < 0) {
		size = -size;
	} else {
		CLUE_CLEAR_FLAG (_spacc_dev.reg.regmap);
		CLUE_CLEAR_STACK (_spacc_dev.reg.regmap);
	}

	//go
	CLUE_START_OPERATION (_spacc_dev.reg.regmap, (clue_base_and_partial_radix (size)));

	while ((wait_count > 0) && ((CLUE_READ_STATUS (_spacc_dev.reg.regmap) & CLUE_STATUS_IRQ) == 0))
	{
		wait_count--;
		DPRINTF(ELP_DBG, "%s: wait_count %d\n", __FUNCTION__, wait_count);
		DPRINTF(ELP_DBG, "%s: clue_status  %d\n", __FUNCTION__, CLUE_READ_STATUS (_spacc_dev.reg.regmap));
	}

	CLUE_INT_ACK (_spacc_dev.reg.regmap);

	if (wait_count == 0) {
		DPRINTF(ELP_DBG, "wait_count %d\n", wait_count);
		DPRINTF(ELP_DBG, "clue_start_engine: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X CYCLES=%X INSTS=%X FSTACK=%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap), (U32)CLUE_READ_CYCLES(_spacc_dev.reg.regmap), (U32)CLUE_READ_INSTS(_spacc_dev.reg.regmap), (U32)CLUE_READ_STACK(_spacc_dev.reg.regmap));

		pka_halt_engine();
		ret = SPACC_CRYPTO_FAILED;
	}

	DPRINTF (ELP_DBG, "clue_start_engine: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X CYCLES=%X INSTS=%X FSTACK=%X\n",
		(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
		(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap), (U32)CLUE_READ_CYCLES(_spacc_dev.reg.regmap),
		(U32)CLUE_READ_INSTS(_spacc_dev.reg.regmap), (U32)CLUE_READ_STACK(_spacc_dev.reg.regmap));

	reason = CLUE_READ_REASON (_spacc_dev.reg.regmap);
	if (reason > 0) {
		DPRINTF(ELP_ERR, "%s: %d SPACC_CRYPTO_FAILED \n", __FUNCTION__, __LINE__);
		ret = SPACC_CRYPTO_FAILED;
	}
	switch (reason) {
		case CLUE_REASON_NORMAL:
			break;
		case CLUE_REASON_STOP_RQST:
			DPRINTF(ELP_DBG, "CLUE STOP: Reason %X [Stop Request]\n",reason);
			ret = SPACC_CRYPTO_HALTED;
			CLUE_CLEAR_STACK (_spacc_dev.reg.regmap);
			break;
		case CLUE_REASON_INVALID_OP:
			DPRINTF(ELP_DBG, "CLUE STOP: Reason %X [Invalid Opcode]\n",reason);
			break;
		case CLUE_REASON_FSTACK_UFLOW:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [Fstack underflow]\n",reason);
			break;
		case CLUE_REASON_FSTACK_OFLOW:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [FStack overflow]\n",reason);
			break;
		case CLUE_REASON_WATCHDOG:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [Watchdog]\n",reason);
			break;
		case CLUE_REASON_PSTACK_UFLOW:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [Pstack underflow]\n",reason);
			break;
		case CLUE_REASON_PSTACK_OFLOW:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [Pstack overflow]\n",reason);
			break;
		case CLUE_REASON_MEM_PORT_COL:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [Memport collision]\n",reason);
			break;
		case CLUE_REASON_INVALID_PNT_FAULT:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [Invalid memory pointer]\n",reason);
			break;
		case CLUE_REASON_INVALID_KEY_FAULT:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [Invalid key type]\n",reason);
			break;
		case CLUE_REASON_NO_KEY_FAULT:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [No key]\n",reason);
			break;
		case CLUE_REASON_NO_EXPONENT_FAULT:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [No exponent]\n",reason);
			break;
		default:
			DPRINTF(ELP_DBG,"CLUE STOP: Reason %X [UNKNOWN]\n",reason);
			break;
	}

	return ret;
}

//get right page size, such as 384 use page 64 not 48
//input:  size  (0,256)
//output: page_size (0, 256)
//        <0 is invalid
int clue_page_size (int size)
{
	int page_size;
	
	if (size < 0) {
		page_size = SPACC_CRYPTO_INVALID_SIZE;
	} else if (size <= 16) {
		page_size = 16;
	} else if (size <= 32) {
		page_size = 32;
	} else if (size <= 64) {
		page_size = 64;
	} else if (size <= 128) {
		page_size = 128;
	} else if (size <= 256) {
		page_size = 256;
	} else {
		page_size = SPACC_CRYPTO_INVALID_SIZE;
	}
	
	return page_size;
}

//for set base adn partial radix bit in CTRL register
unsigned clue_base_and_partial_radix (int size)
{
	unsigned radix;
	
	if (size == 65) { 
		return (4U<<8)|17|(9U<<16);
	}
	
	if (size < 0) {
		radix = SPACC_CRYPTO_INVALID_SIZE;
	} else if (size <= 16) {
		radix = 1;
	} else if (size <= 32) {
		radix = 2;
	} else if (size <= 64) {
		radix = 3;
	} else if (size <= 128) {
		radix = 4;
	} else if (size <= 256) {
		radix = 5;
	} else if (size <= 512) {
		radix = 6;
	} else if (size <= 1024) {
		radix = 7;
	} else {
		radix = SPACC_CRYPTO_INVALID_SIZE;
	}
	
	if ((size == 16) || (size == 32) || (size == 64) || (size == 128) || (size == 256) || (size == 512) || (size == 1024)) {
		radix = (radix << 8);
	} else {
		//base | partial = size*8/32
		radix = (radix << 8) | (size >> 2);
	}
	
	return radix;
}

//precompute
// input: n, size (modulus and size)
// output:
// load mp, rr to hardware

//      mp (m_prime = (r*r^-1)/m )
//      rr (r_sqr_mod_m = r*r mod m)

//static void dump_val(char *name, unsigned char *ptr, unsigned size)
//{
//	unsigned char buf[512];
//	unsigned n;
//	MEMCPY32EX_R(buf, ptr, size/4);
//	DPRINTF(ELP_DBG,"%s:\n", name);
//	for (n = 0; n < size; ++n) {
//		DPRINTF(ELP_DBG,"%02X", ptr[n]);
//	}
//	DPRINTF(ELP_DBG,"%s","\n");
//}

int clue_bn_precompute (U8 * n, int size)
{
	int err = SPACC_CRYPTO_OK;
	U32 *p;
	char val[512];

	// load modulus into D0
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
	MEMCPY32EX_R (p, (U32 *) n, size >> 2);

	// call CALC_R_INV
	//dump_val("modulus", CLUE_REGION_D_PAGE(_spacc_dev.reg.regmap, clue_page_size(size), 0), size);
	CLUE_R_INV (_spacc_dev.reg.regmap);
	if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
		DPRINTF (ELP_DBG,"CLUE R_INV FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
		err = SPACC_CRYPTO_FAILED;
	}
	//dump_val("r_invres", CLUE_REGION_C_PAGE(_spacc_dev.reg.regmap, clue_page_size(size), 0), size);

	// save it 
	MEMCPY32EX_R (p, (U32 *) n, size >> 2);
	MEMCPY32EX_R (val, CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0), size >> 2);

	// call CALC_MP
	MEMCPY32EX_R (CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 2), val, size >> 2);
	//dump_val("calc_mp_input", CLUE_REGION_D_PAGE(_spacc_dev.reg.regmap, clue_page_size(size), 2), size);

	CLUE_CALC_MP (_spacc_dev.reg.regmap);
	if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
		DPRINTF (ELP_DBG,"CLUE R_MP FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
			(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
			(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
		err = SPACC_CRYPTO_FAILED;
	}
	
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
	MEMCPY32EX_R (p, (U32 *) n, size >> 2);
	// call CALC_SQR
	MEMCPY32EX_R (CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0), val, size >> 2);
  
	CLUE_R_SQR (_spacc_dev.reg.regmap);
	if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
		DPRINTF (ELP_DBG,"CLUE R_SQR FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
			(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
			(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
		err = SPACC_CRYPTO_FAILED;
	}
 
	return err;
}

int clue_bn_r_inv (U8 * m, U8 * c,int size)
{
	int err = SPACC_CRYPTO_OK;
	U32 *p;

	// load modulus into D0
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
	MEMCPY32EX_R (p, (U32 *) m, size >> 2);

	// call CALC_R_INV
	CLUE_R_INV (_spacc_dev.reg.regmap);
	if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
		DPRINTF (ELP_DBG,"CLUE R_INV FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
			(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
			(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
		err = SPACC_CRYPTO_FAILED;
	}

	//c <- C0
	p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
	MEMCPY32EX_R (c, p, size >> 2);
	//PDUMPWORD (EDDUMP, p, size, "Precompute: r_inv", 1);

	return err;
}

int clue_bn_mp (U8 * a, U8 *m, U8 * c,int size)
{
	int err = SPACC_CRYPTO_OK;
	U32 *p;

	// load modulus into D0
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
	MEMCPY32EX_R (p, (U32 *) m, size >> 2);

	// load r-1 mod m into D2
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 2);
	MEMCPY32EX_R (p, (U32 *) a, size >> 2);

	// call CALC_MP
	CLUE_CALC_MP (_spacc_dev.reg.regmap);
	if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
		DPRINTF (ELP_DBG,"CLUE R_MP FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
			(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
			(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
		err = SPACC_CRYPTO_FAILED;
	}
	
	//c <- D1
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 1);
	MEMCPY32EX_R (c, p, size >> 2);
	//PDUMPWORD (EDDUMP, p, size, "Precompute: mp", 1);

	return err;
}

int clue_bn_r_sqr (U8 * a, U8 *b, U8 *m, U8 * c,int size)
{
	int err = SPACC_CRYPTO_OK;
	U32 *p;
	//char val[512];

	// load modulus into D0
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
	MEMCPY32EX_R (p, (U32 *) m, size >> 2);

	// load r-1 mod m into C0
	p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
	MEMCPY32EX_R (p, (U32 *) a, size >> 2);
  
	// load mp into D1
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 1);
	MEMCPY32EX_R (p, (U32 *) b, size >> 2);
  
	CLUE_R_SQR (_spacc_dev.reg.regmap);
	if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
		DPRINTF (ELP_DBG,"CLUE R_SQR FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
			(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
			(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
		err = SPACC_CRYPTO_FAILED;
	}
 
	//c <- D3
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 3);
	MEMCPY32EX_R (c, p, size >> 2);
	//PDUMPWORD (EDDUMP, p, size, "Precompute: R_SQR", 1);

	return err;
}

int clue_bn_load_precompute (U8 * mp, U8 * rr, int size)
{
	int err = SPACC_CRYPTO_OK;
	U32 *p;
	//D3  <- rr
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 3);
	MEMCPY32EX_R (p, (U32 *) rr, size >> 2);
	//PDUMPWORD (EDDUMP, p, size, "clue_bn_load_precompute: RR", 1);

	//D1  <- mp
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 1);
	MEMCPY32EX_R (p, (U32 *) mp, size >> 2);
	//PDUMPWORD (EDDUMP, p, size, "clue_bn_load_precompute: MP", 1);

	return err;
}

int clue_bn_precompute_ex (U8 * m, U8 * c, int size)
{
	U32 *p;
	if (clue_bn_precompute (m, size) != SPACC_CRYPTO_OK)
		return SPACC_CRYPTO_FAILED;

	//get result output c <- D3 
	p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 3);
	MEMCPY32EX_R (c, p, size >> 2);
	//PDUMPWORD (EDDUMP, p, size, "Precompute", 1);

	return SPACC_CRYPTO_OK;
}

//mod exp:  c=a^e mod m
//intputs:  a (base)
//          e (exponent applied to base)
//          m (modulus)
//output: c

//precompute
//          mp (m_prime = (r*r^-1)/m )
//          rr (r_sqr_mod_m = r*r mod m)
//algorithm:
//      mp (m_prime = (r*r^-1)/m )
//      rr (r_sqr_mod_m = r*r mod m)
//  mp----m'   m_prime = ((r * r_inv) - 1)/m;
//  calculate: r_inv = inv( r, m ); m_prime = ((r * r_inv) - 1)/m;
//  rr----r_sqr_mod_m     r_sqr_mod_m = mod( r^2, m );
//  calculate: r = 2^(ff1( m ) + 1); r_sqr_mod_m = mod( r^2, m );
//  mod_exp  = mpower( base, exp, m );
// The precomp parameter forces precompute values to be loaded in the
// CLUE hardware. This whould be done only one time when a new key is used.
       
// a -base
// b - m'
// d - r2 mod m
// e - exponent

int clue_bn_modexp_base (U8 * a, U8 * b,U8 * d, U8 * e, U8 * m, U8 * c, int size)
{
	int ret = SPACC_CRYPTO_OK;
	U32 *p;

	if ((a == 0) || (e == 0) || (c == 0) || (d == 0)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {
		//load data
		//A0  <- a // base
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		//dumpword ( p, size, "//m");
		//D2  <- e // exponent
		p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 2);
		MEMCPY32EX_R (p, (U32 *) e, size >> 2);
		//dumpword ( p, size, "//E");
		//D1 <-b  // m'
		p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 1);
		MEMCPY32EX_R (p, (U32 *) b, size >> 2);
		//dumpword ( p, size, "//mp");

		//D3 <-d
		p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 3);
		MEMCPY32EX_R (p, (U32 *) d, size >> 2);
		//dumpword ( p, size, "//r^2");

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
			//dumpword ( p, size, "//m");
		}

		//do operation
		CLUE_MOD_EXP (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG,"BN_MODEXP FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}
		//get result output c <- A0 
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (c, p, size >> 2);
		//dumpword ( p, size, "//result c=a^e mod m");
	}
	return ret;
}

int clue_bn_modexp (U8 * a, U8 * e, U8 * m, U8 * c, int size, int precomp)
{
	int ret = SPACC_CRYPTO_OK;
	U32 *p;

	if ((a == 0) || (e == 0) || (c == 0) || (m == 0)) {
		DPRINTF(ELP_ERR,"%s: %d SPACC_CRYPTO_INVALID_ARGUMENT\n",__FUNCTION__,__LINE__);
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		DPRINTF(ELP_ERR,"%s: %d SPACC_CRYPTO_INVALID_ARGUMENT\n",__FUNCTION__,__LINE__);
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {
		if (precomp == 1) {
			if (clue_bn_precompute (m, size) != SPACC_CRYPTO_OK)
				ret = SPACC_CRYPTO_FAILED;
		}
		//load data
		//A0  <- a
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		//dumpword (p, size, "//a");
		//D2  <- e
		p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 2);
		MEMCPY32EX_R (p, (U32 *) e, size >> 2);
		//dumpword ( p, size, "//e");

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
			//dumpword ( p, size, "//m");
		}

		p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 1);
		//dumpword ( p, size, "//mp");

		p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 3);
		//dumpword ( p, size, "//r^2");

		//do operation
		CLUE_MOD_EXP (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG, "BN_MODEXP FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}
		//get result output c <- A0 
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);

		//MEMCPY32EX_R (c, p, size >> 2);
		MEMCPY32 (c, p, size >> 2);
		//dumpword ( p, size, "//result c=a^e mod m");
	}
	return ret;
}

//modular multiplication: c=a*b mod m
//intputs:      a
//          b
//    m (modulus)
//output: c

//precompute
//          mp (m_prime = (r*r^-1)/m )
//          rr (r_sqr_mod_m = r*r mod m)
// The precomp parameter forces precompute values to be loaded in the
// CLUE hardware. This whould be done only one time when a new key is used.
#ifdef CLUE_MOD_MULT
int clue_bn_modmult (U8 * a, U8 * b, U8 * m, U8 * c, int size, int precomp)
{
	int ret = SPACC_CRYPTO_OK;
	U32 *p;
	if ((a == 0) || (b == 0) || (c == 0) || (m == 0)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {

		if (precomp == 1) {
			if (clue_bn_precompute (m, size) != SPACC_CRYPTO_OK)
				ret = SPACC_CRYPTO_FAILED;
		}
		//load data

		//A0 <- a
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		dumpword ( p, size, "//a");
		//B0 <- b
		p = CLUE_REGION_B_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) b, size >> 2);
		dumpword (p, size, "//b");

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
			dumpword (p, size, "//m");
		}

		//do operation
		CLUE_MOD_MULT (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG, "BN_MODMULT FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}

		//get result
		//output c <- A0 
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R ((U32 *) c, p, size >> 2);
		dumpword (p, size, "//result c=a*b mod m");
	}
	return ret;
}
#endif

//modular division: c=a/b mod m
//intputs:  a
//    b
//    m (modulus)
//output: c
#ifdef CLUE_MOD_DIV
int clue_bn_moddiv (U8 * a, U8 * b, U8 * m, U8 * c, int size)
{
	int ret = SPACC_CRYPTO_OK;
	U32 *p;

	if ((a == 0) || (b == 0) || (c == 0) || (m == 0)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if (clue_bn_denominator_gcd_check (b, m, size) != SPACC_CRYPTO_OK) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {
		//load data
		//C0 <- a
		p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		//A0 <- b
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) b, size >> 2);

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
		}

		//clean C1
		//p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 1);
		//MEMSET32 (p, 0, clue_page_size (size) >> 2);

		//do operation
		CLUE_MOD_DIV (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG, "BN_MODINV FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}
		//get result, output c <- C0
		p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R ((U32 *) c, p, size >> 2);
	}
	return ret;
}
#endif

//modular inversion: c=1/b mod m
//intputs:  a=1
//    b
//    m (modulus)
//output: c
#ifdef CLUE_MOD_INV
int clue_bn_modinv (U8 * b, U8 * m, U8 * c, int size)
{
	U8 a[BN_RSA_BASE_RADIX_SIZE];
	int ret = SPACC_CRYPTO_OK;
	U32 *p;

	if ((b == 0) || (c == 0) || (m == 0)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if (clue_bn_denominator_gcd_check (b, m, size) != SPACC_CRYPTO_OK) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {
		MEMSET (a, 0, sizeof (a));
		a[size - 1] = 1;            // a=1
		//C0  <- a = 1
		p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		//PDUMPWORD (EDDUMP, p, size, "//a", 1);

		//A0 <- b
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) b, size >> 2);

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
		}

		//clean C1
		//p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 1);
		//MEMSET32 (p, 0, clue_page_size (size) >> 2);

		//do operation
		CLUE_MOD_INV (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG, "BN_MODINV FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}
		//get result, output c <- C0
		p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R ((U32 *) c, p, size >> 2);
	}
	return ret;
}
#endif

//modular addition:   c=a+b mod m
//intputs:      
//    a
//          b
//          m (modulus)
//output:    
//    c
#ifdef CLUE_MOD_ADD
int clue_bn_modadd (U8 * a, U8 * b, U8 * m, U8 * c, int size)
{
	int ret = SPACC_CRYPTO_OK;
	U32 *p;

	if ((a == 0) || (b == 0) || (c == 0) || (m == 0)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {
		//A0 <- a
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		//dumpword (p, size, "//a");
		//B0 <- b
		p = CLUE_REGION_B_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) b, size >> 2);
		//dumpword (p, size, "//b");

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
			//dumpword ( p, size, "//m");
		}
		//do operation
		CLUE_MOD_ADD (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG, "BN_MODSUB FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}

		//get result, output c <- A0 
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R ((U32 *) c, p, size >> 2);
		//dumpword (p, size, "//c");
	}
	return ret;
}
#endif

//modular subtraction:    c=a-b mod m
//intputs:      a
//          b
//          m (modulus)
//output: c
#ifdef CLUE_MOD_SUB
int clue_bn_modsub (U8 * a, U8 * b, U8 * m, U8 * c, int size)
{
	int ret = SPACC_CRYPTO_OK;
	U32 *p;

	if ((a == 0) || (b == 0) || (c == 0) || (m == 0)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {
		//A0 <- a
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		//dumpword (p, size, "//a");
		//B0 <- b
		p = CLUE_REGION_B_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) b, size >> 2);
		//dumpword (p, size, "//b");

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
			//dumpword (p, size, "//m");
		}

		//do operation
		CLUE_MOD_SUB (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG, "BN_MODSUB FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}

		//get result, output c <- A0 
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R ((U32 *) c, p, size >> 2);
		//dumpword (p, size, "//c");
	}
	return ret;
}
#endif

//modular reduction:    c=a mod m
//intputs:      a
//          m (modulus)
//output: c
#ifdef CLUE_MOD_RED
int clue_bn_modred (U8 * a, U8 * m, U8 * c, int size)
{
	int ret = SPACC_CRYPTO_OK;
	U32 *p;

	if ((a == 0) || (c == 0) || (m == 0)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
		ret = SPACC_CRYPTO_INVALID_ARGUMENT;
	} else {

		//C0 <- a
		p = CLUE_REGION_C_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R (p, (U32 *) a, size >> 2);
		//dumpword (p, size, "//a");

		if (m != 0) {
			//D0  <- m
			p = CLUE_REGION_D_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
			MEMCPY32EX_R (p, (U32 *) m, size >> 2);
			//dumpword (p, size, "//m");
		}
		//do operation
		CLUE_MOD_RED (_spacc_dev.reg.regmap);
		if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_DBG, "BN_MODREDUCTION: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
				(U32) CLUE_READ_STATUS (_spacc_dev.reg.regmap), CLUE_READ_ENTRY (_spacc_dev.reg.regmap),
				(U32) CLUE_READ_CTRL (_spacc_dev.reg.regmap));
			ret = SPACC_CRYPTO_FAILED;
		}

		//get result, output c <- A0 
		p = CLUE_REGION_A_PAGE (_spacc_dev.reg.regmap, clue_page_size (size), 0);
		MEMCPY32EX_R ((U32 *) c, p, size >> 2);
		//dumpword (p, size, "//c(A0)");
	}
	return ret;
}
#endif
