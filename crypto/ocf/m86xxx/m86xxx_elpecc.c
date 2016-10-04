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
// 	ECC HARDWARE MODULE INTERFACE
//
//-----------------------------------------------------------------------
//
// Language:         C
//
//
// Filename:         $Source: /prj/msacvs/enterprise/drivers/elp1800_ocf/m86xxx_elpecc,v $
// Current Revision: $Revision: %
// Last Updated:     $Date: 2009-09-30 15:19:26 $
// Current Tag:      $Name: EPN1802_MINDSPEED_27SEP2010 $
//
//
//-----------------------------------------------------------------------*/

#ifdef DO_ECC
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

U32 _mmap;

#define CURVE_160_DATA_SIZE 20
#define CURVE_160_ORDER_FULL_SIZE 32
#define CURVE_192_DATA_SIZE 24
#define CURVE_192_ORDER_FULL_SIZE 32
#define CURVE_224_DATA_SIZE 28
#define CURVE_224_ORDER_FULL_SIZE 32
#define CURVE_256_DATA_SIZE 32
#define CURVE_256_ORDER_FULL_SIZE 32
#define CURVE_384_DATA_SIZE 48
#define CURVE_384_ORDER_FULL_SIZE 64

#define CURVE_MAX_DATA_SIZE	256

typedef struct ec_curve_data_
{
  S16 size;
  U8 rr[CURVE_MAX_DATA_SIZE];
  U8 m[CURVE_MAX_DATA_SIZE];    //modulus m
  U8 mp[CURVE_MAX_DATA_SIZE];   // modular inverse of m (mod R)
  U8 x[CURVE_MAX_DATA_SIZE];    // Q_x  the x coordinate of the generator 
  U8 y[CURVE_MAX_DATA_SIZE];    // Q_y  the y coordinate of the generator 
  U8 z[CURVE_MAX_DATA_SIZE];    // Q_z   the z coordinate of the generator 
  U8 r[CURVE_MAX_DATA_SIZE];    // R^2 mod m - used to convert an integer to an M-residue
  U8 a[CURVE_MAX_DATA_SIZE];    // domain parameter 'a'
  U8 b[CURVE_MAX_DATA_SIZE];    // domain parameter 'b'
  U8 order[CURVE_MAX_DATA_SIZE + 1];   // incase 160+1
  C8 *comment;                  // a short description of the curve
  U8 nr[CURVE_MAX_DATA_SIZE];   // R^2 mod n
  U8 np[CURVE_MAX_DATA_SIZE];   // inverse of n mod R
  U8 n[CURVE_MAX_DATA_SIZE];    // order of the curve
} EC_CURVE_DATA;

#define CLUE_MAX_CURVES 16

#if defined (DO_PKA_EPN0202)
#include "SECP_192.inc"
#include "SECP_224.inc"
#include "SECP_256.inc"
#include "SECP_384.inc"
#elif defined (DO_CLUE_EDN0232)
#include "SECP_160.inc"
#include "SECP_384.inc"
#include "SECP_192.inc"
#include "SECP_224.inc"
#include "SECP_256.inc"
#elif defined (DO_CLUE_EDN160)
#include "SECP_160.inc"
#include "SECP_384.inc"
#include "SECP_192.inc"
#include "SECP_224.inc"
#include "SECP_256.inc"
#elif defined (DO_CLUE_EDN135)
#include "SECP_384.inc"
#elif defined (DO_PKA_EPN1200) 
#include "SECP_160.inc"
#include "SECP_192.inc"
#include "SECP_224.inc"
#include "SECP_256.inc"
#elif defined (DO_PKA_EPN0205) 
#include "SECP_160.inc"
#include "SECP_192.inc"
#include "SECP_224.inc"
#include "SECP_256.inc"
#include "SECP_384.inc"
#elif defined (DO_PKA_EPN0203) 
#include "SECP_160.inc"
#include "SECP_192.inc"
#include "SECP_224.inc"
#include "SECP_256.inc"
#include "SECP_384.inc"
#elif defined (DO_CLUE_ALL) 
#include "SECP_160.inc"
#include "SECP_192.inc"
#include "SECP_224.inc"
#include "SECP_256.inc"
#include "SECP_384.inc"
#include "SECP_512.inc"
#include "SECP_521.inc"
#else
#error "Should define Clue load"
#endif

static EC_CURVE_DATA *curve_data[CLUE_MAX_CURVES];

void clue_ec_init (U32 mmap)
{
  int i;

  for (i = 0; i < CLUE_MAX_CURVES; i++)
    curve_data[i] = 0;

#if defined (DO_PKA_EPN0202)
  curve_data[4] = (EC_CURVE_DATA *) &SEC192R1;
  curve_data[6] = (EC_CURVE_DATA *) &SEC224R1;
  curve_data[7] = (EC_CURVE_DATA *) &SEC224K1;
  curve_data[8] = (EC_CURVE_DATA *) &SEC256R1;
  curve_data[9] = (EC_CURVE_DATA *) &SEC256K1;
  curve_data[10] = (EC_CURVE_DATA *) &SEC384R1;
#elif defined (DO_CLUE_EDN0232)
  curve_data[3] = (EC_CURVE_DATA *) & SEC384R1;
  curve_data[4] = (EC_CURVE_DATA *) & SEC192R1;
  curve_data[5] = (EC_CURVE_DATA *) & SEC224R1;
  curve_data[6] = (EC_CURVE_DATA *) & SEC256R1;
#elif defined (DO_CLUE_EDN160)
  curve_data[0] = (EC_CURVE_DATA *) & SEC160R1;
  curve_data[1] = (EC_CURVE_DATA *) & SEC160R2;
  curve_data[2] = (EC_CURVE_DATA *) & SEC160K1;
  curve_data[3] = (EC_CURVE_DATA *) & SEC384R1;
  curve_data[4] = (EC_CURVE_DATA *) & SEC192R1;
  curve_data[5] = (EC_CURVE_DATA *) & SEC224R1;
  curve_data[6] = (EC_CURVE_DATA *) & SEC256R1;
#elif defined (DO_CLUE_EDN135)
  curve_data[3] = (EC_CURVE_DATA *) & SEC384R1;
#elif defined (DO_PKA_EPN1200)
  curve_data[0] = (EC_CURVE_DATA *) &SEC160R1;
  curve_data[1] = (EC_CURVE_DATA *) &SEC160R2;
  curve_data[2] = (EC_CURVE_DATA *) &SEC160K1;
  curve_data[3] = (EC_CURVE_DATA *) &SEC160WTLS9;
  curve_data[4] = (EC_CURVE_DATA *) &SEC192R1;
  curve_data[5] = (EC_CURVE_DATA *) &SEC192K1;
  curve_data[6] = (EC_CURVE_DATA *) &SEC224R1;
  curve_data[7] = (EC_CURVE_DATA *) &SEC224K1;
  curve_data[8] = (EC_CURVE_DATA *) &SEC256R1;
  curve_data[9] = (EC_CURVE_DATA *) &SEC256K1;
#elif defined (DO_PKA_EPN0205)
  curve_data[0] = (EC_CURVE_DATA *) &SEC160R1;
  curve_data[1] = (EC_CURVE_DATA *) &SEC160R2;
  curve_data[2] = (EC_CURVE_DATA *) &SEC160K1;
  curve_data[3] = (EC_CURVE_DATA *) &SEC160WTLS9;
  curve_data[4] = (EC_CURVE_DATA *) &SEC192R1;
  curve_data[5] = (EC_CURVE_DATA *) &SEC192K1;
  curve_data[6] = (EC_CURVE_DATA *) &SEC224R1;
  curve_data[7] = (EC_CURVE_DATA *) &SEC224K1;
  curve_data[8] = (EC_CURVE_DATA *) &SEC256R1;
  curve_data[9] = (EC_CURVE_DATA *) &SEC256K1;
  curve_data[10] = (EC_CURVE_DATA *) &SEC384R1;
#elif defined (DO_PKA_EPN0203)
  curve_data[0] = (EC_CURVE_DATA *) &SEC160R1;
  curve_data[1] = (EC_CURVE_DATA *) &SEC160R2;
  curve_data[2] = (EC_CURVE_DATA *) &SEC160K1;
  curve_data[3] = (EC_CURVE_DATA *) &SEC160WTLS9;
  curve_data[4] = (EC_CURVE_DATA *) &SEC192R1;
  curve_data[5] = (EC_CURVE_DATA *) &SEC192K1;
  curve_data[6] = (EC_CURVE_DATA *) &SEC224R1;
  curve_data[7] = (EC_CURVE_DATA *) &SEC224K1;
  curve_data[8] = (EC_CURVE_DATA *) &SEC256R1;
  curve_data[9] = (EC_CURVE_DATA *) &SEC256K1;
  curve_data[10] = (EC_CURVE_DATA *) &SEC384R1;
#elif defined (DO_CLUE_ALL)
  curve_data[0] = (EC_CURVE_DATA *) &SEC160R1;
  curve_data[1] = (EC_CURVE_DATA *) &SEC160R2;
  curve_data[2] = (EC_CURVE_DATA *) &SEC160K1;
  curve_data[3] = (EC_CURVE_DATA *) &SEC160WTLS9;
  curve_data[4] = (EC_CURVE_DATA *) &SEC192R1;
  curve_data[5] = (EC_CURVE_DATA *) &SEC192K1;
  curve_data[6] = (EC_CURVE_DATA *) &SEC224R1;
  curve_data[7] = (EC_CURVE_DATA *) &SEC224K1;
  curve_data[8] = (EC_CURVE_DATA *) &SEC256R1;
  curve_data[9] = (EC_CURVE_DATA *) &SEC256K1;
  curve_data[10] = (EC_CURVE_DATA *) &SEC384R1;
  curve_data[11] = (EC_CURVE_DATA *) &SEC512R1;
  curve_data[12] = (EC_CURVE_DATA *) &SEC521R1;
#else
#error "Should define Clue load"
#endif

  _mmap = mmap;
}

int clue_ec_find_curve (int size)
{
  int i;
  for (i = 0; i < CLUE_MAX_CURVES; i++) {
    if (curve_data[i] && curve_data[i]->size == size) {
      return i;
    }
  }
  return -1;
}

S16 clue_ec_load_curve_pmult (U16 curve, S16 loadbase)
{
  U32 *p;
  S16 size;
  U8 *x, *y, *a, *b;
  EC_CURVE_DATA *pc = (EC_CURVE_DATA *) curve_data[curve];

  if ((curve > CLUE_MAX_CURVES) || (pc == 0))
    return SPACC_CRYPTO_INVALID_ARGUMENT;
 
	size = pc->size;
  x = (U8 *) pc->x;
  y = (U8 *) pc->y;
  a = (U8 *) pc->a;
	b = (U8 *) pc->b;

  if (loadbase == 1) {
    //A2 <- x
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) x, size >> 2);
    //B2 <- y
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) y, size >> 2);
  }
  
	//A6  <- a, (curve)
  p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 6);
  MEMCPY32EX_R (p, (U32 *) a, size >> 2);
  //PDUMPWORD (EDDUMP, p, size, "//a", 1);

  return SPACC_CRYPTO_OK;
}

S16 clue_ec_load_curve_pdbl (U16 curve)
{
  U32 *p;
  S16 size;
  U8 *a;
  EC_CURVE_DATA *pc = (EC_CURVE_DATA *) curve_data[curve];
  U8 k[CURVE_MAX_DATA_SIZE];

  if ((curve > CLUE_MAX_CURVES) || (pc == 0))
    return SPACC_CRYPTO_INVALID_ARGUMENT;

  size = pc->size;
  a = (U8 *) pc->a;

  MEMSET (k, 0, sizeof (k));
  k[size - 1] = 1;              // z=1
  //C3  <- z
  p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 3);
  MEMCPY32EX_R (p, (U32 *) k, size >> 2);
  k[size - 1] = 0;
  k[clue_page_size (size) - 1] = 2; // K=2
  //D7  <- k, key (2<key<order of base point)
  p = CLUE_REGION_D_PAGE (_mmap, clue_page_size (size), 7);
  MEMCPY32EX_R (p, (U32 *) k, (clue_page_size (size)) >> 2);

  //A6  <- a, (curve)
  p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 6);
  MEMCPY32EX_R (p, (U32 *) a, size >> 2);
  //PDUMPWORD (EDDUMP, p, size, "//a", 1);

  return SPACC_CRYPTO_OK;
}

S16 clue_ec_load_curve_padd (U16 curve)
{
  U32 *p;
  S16 size;
  U8 *a;
  EC_CURVE_DATA *pc = (EC_CURVE_DATA *) curve_data[curve];

  if ((curve > CLUE_MAX_CURVES) || (pc == 0))
    return SPACC_CRYPTO_INVALID_ARGUMENT;

  size = pc->size;
  a = (U8 *) pc->a;

  //A6  <- a, (curve)
  p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 6);
  MEMCPY32EX_R (p, (U32 *) a, size >> 2);

  return SPACC_CRYPTO_OK;
}

S16 clue_ec_load_curve_pver (U16 curve)
{
  U32 *p;
  S16 size;
  U8 *a, *b;
  EC_CURVE_DATA *pc = (EC_CURVE_DATA *) curve_data[curve];

  if ((curve > CLUE_MAX_CURVES) || (pc == 0))
    return SPACC_CRYPTO_INVALID_ARGUMENT;
  size = pc->size;
  a = (U8 *) pc->a;
  b = (U8 *) pc->b;

  //A6  <- a, (curve)
  p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 6);
  MEMCPY32EX_R (p, (U32 *) a, size >> 2);
  //PDUMPWORD (EDDUMP, p, size, "//a", 1);
  //A7  <- b, (curve)
  p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 7);
  MEMCPY32EX_R (p, (U32 *) b, size >> 2);
  //PDUMPWORD (EDDUMP, p, size, "//b", 1);

  return SPACC_CRYPTO_OK;
}

S16 clue_ec_load_curve_data (U16 curve)
{
  U32 *p;
  S16 size;
  U8 *m, *mp, *r;
  EC_CURVE_DATA *pc = (EC_CURVE_DATA *) curve_data[curve];

  if ((curve > CLUE_MAX_CURVES) || (pc == 0))
    return SPACC_CRYPTO_INVALID_ARGUMENT;
  size = pc->size;
  m = (U8 *) pc->m;
  mp = (U8 *) pc->mp;
  r = (U8 *) pc->r;

  //D0  <- modulus m (p)
  p = CLUE_REGION_D_PAGE (_mmap, clue_page_size (size), 0);
  MEMCPY32EX_R (p, (U32 *) m, size >> 2);
  PDUMPWORD (EDDUMP, p, size, "//m, --- p (modulus)", 1);
  //D1  <- m' ,modular inverse of m (mod R)
  p = CLUE_REGION_D_PAGE (_mmap, clue_page_size (size), 1);
  MEMCPY32EX_R (p, (U32 *) mp, size >> 2);
  PDUMPWORD (EDDUMP, p, size, "//m', --- (mp)", 1);
  //D3  <- r_sqr_mod_m
  p = CLUE_REGION_D_PAGE (_mmap, clue_page_size (size), 3);
  MEMCPY32EX_R (p, (U32 *) r, size >> 2);
  PDUMPWORD (EDDUMP, p, size, "//r_sqr_mod_m", 1);

  return SPACC_CRYPTO_OK;
}

S16 clue_ec_load_curve_modulus (U16 curve)
{
  U32 *p;
  S16 size;
  U8 *m;
  EC_CURVE_DATA *pc = (EC_CURVE_DATA *) curve_data[curve];

  if ((curve > CLUE_MAX_CURVES) || (pc == 0))
    return SPACC_CRYPTO_INVALID_ARGUMENT;
  size = pc->size;
  m = (U8 *) pc->m;

  //D0  <- modulus m (p)
  p = CLUE_REGION_D_PAGE (_mmap, clue_page_size (size), 0);
  MEMCPY32EX_R (p, (U32 *) m, size >> 2);
  PDUMPWORD (EDDUMP, p, size, "//m, --- p (modulus)", 1);

  return SPACC_CRYPTO_OK;
}

#if defined(CLUE_POINT_MULT) && defined(CLUE_ECDSA_PT2)
S16 pars_clue_ec_dsa_sign(void *p1, void *p2, U8 *e, U16 size, U16 ksize, U8 *r, U8 *s)
{
   S16 failed, ret;
   U8 ptx[CURVE_MAX_DATA_SIZE], pty[CURVE_MAX_DATA_SIZE];
   U32 *p, curwd;

/* first we do the point mult to generate kG */
   failed = SPACC_CRYPTO_OK;
   PARS_SRC_HANDLE_I_SET_VAL(_mmap, p1);
   ret = clue_ec_point_mult_base(NULL, ptx, pty, size, ksize);

   if (ret != SPACC_CRYPTO_OK) {
      // set watchdog low 
      curwd = CLUE_READ_UINT(_mmap + CLUE_WATCHDOG);
      CLUE_WRITE_UINT(_mmap + CLUE_WATCHDOG, 1);
      failed = ret;
   }

/* now we must do the ECDSA_PT2 bit 
   B0 = ptx
   A6 = e
  
   s = C0
   r = C1
*/
   PARS_SRC_HANDLE_I_SET_VAL(_mmap, p2);
   
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) ptx, size >> 2);
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 6);
    MEMCPY32EX_R (p, (U32 *) e, size >> 2);

    CLUE_ECDSA_PT2 (_mmap);
    if ((ret = clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_dsa_sign PT2: FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap),(unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    if (failed != SPACC_CRYPTO_OK) {
       // reset WD
       CLUE_WRITE_UINT(_mmap + CLUE_WATCHDOG, curwd);
       return failed;
    }

    //output s <- dst I handle
    p = PARS_DST_HANDLE_I_PTR_X (_mmap);
    MEMCPY32EX_R ((U32 *) s, p, size >> 2);
    //output r <- dst I handle
    PDUMPWORD (EDDUMP, p, size, "RX", 1);
    p = PARS_DST_HANDLE_I_PTR_Y (_mmap,size*8);
    MEMCPY32EX_R ((U32 *) r, p, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "RY", 1);

    return ret;
}
#endif

/////////////////////////////////////////////// clue ecc
//point multiplication: k*P(x,y)
//intputs:      k, key
//            x, x of P(x,y)  point on the curve
//    y, y of P(x,y)
//output:   rx
//        ry
#ifdef CLUE_POINT_MULT
S16 clue_ec_point_mult (U8 * k, U8 * x, U8 * y, U8 * rx, U8 * ry, U16 size, U16 ksize)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;
  U8 kbuf[CURVE_MAX_DATA_SIZE];

  if ((k == 0) || (rx == 0) || (ry == 0) || (ksize < 2) || (ksize > CURVE_MAX_DATA_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else if ((x == 0) || (y == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {

    //A2,A3 <- x
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) x, size >> 2);
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 3);
    MEMCPY32EX_R (p, (U32 *) x, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//x", 1);
    //B2,B3 <- y
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) y, size >> 2);
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 3);
    MEMCPY32EX_R (p, (U32 *) y, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//y", 1);

    //load data

    //D7  <- k, key (2<key<order of base point), must full size, 48+16=64 bytes
    if (k) {
       p = CLUE_REGION_D_PAGE (_mmap, clue_page_size (size), 7);
       MEMSET (kbuf, 0, sizeof (kbuf));
       MEMCPY32EX_R (kbuf, k, (ksize >> 2));
       MEMCPY32 (p, (U32 *) kbuf, (clue_page_size (size)) >> 2);
    }
    
		//PDUMPWORD (EDDUMP, k, ksize, "//k", 1);
    //PDUMPWORD (EDDUMP, kbuf, clue_page_size(size), "//k", 1);
    //PDUMPWORD (EDDUMP, p, size+CURVE_384_ORDER_FULL_SIZE-CURVE_384_DATA_SIZE, "//k", 1);

    //clean C1
    //p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 1);
    //MEMSET32 (p, 0, clue_page_size (size) >> 2);

    //do operation
    CLUE_POINT_MULT (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_point_mult: FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap),(unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }
    //get result
    //output rx <- A2
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) rx, p, size >> 2);
    //output ry <- B2
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) ry, p, size >> 2);
  }
  return ret;
}
#endif

/////////////////////////////////////////////// clue ecc
//point multiplication: k*G(x,y)
//intputs:      k, key
//            x, x of G(x,y)  base point
//    y, y of G(x,y)
//output:   rx
//        ry
#ifdef CLUE_POINT_MULT
S16 clue_ec_point_mult_base (U8 * k, U8 * rx, U8 * ry, U16 size, U16 ksize)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;
  U8 kbuf[CURVE_MAX_DATA_SIZE];

  if ((rx == 0) || (ry == 0) || (ksize < 2) || (ksize > CURVE_MAX_DATA_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {

    //load data

    //D7  <- k, key (2<key<order of base point), must full size, 48+16=64 bytes
    if (k) {
       p = CLUE_REGION_D_PAGE (_mmap, clue_page_size (size), 7);
       MEMSET (kbuf, 0, sizeof (kbuf));
       MEMCPY32EX_R (kbuf, k, (ksize >> 2));
       MEMCPY32 (p, (U32 *) kbuf, (clue_page_size (size)) >> 2);
    }

    //clean C1
    //p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 1);
    //MEMSET32 (p, 0, clue_page_size (size) >> 2);

    //do operation
    CLUE_POINT_MULT (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_point_mult_base: FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }
    //get result
    //output rx <- A2
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) rx, p, size >> 2);
    //output ry <- B2
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) ry, p, size >> 2);
  }
  return ret;
}
#endif

//point double: 2*P(x,y), where k=2
//intputs:
//          x, x of P(x,y)
//    y, y of P(x,y)
//output: rx
//    ry
#ifdef CLUE_POINT_DOUBLE
S16 clue_ec_point_double (U8 * x, U8 * y, U8 * rx, U8 * ry, U16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((x == 0) || (y == 0) || (rx == 0) || (ry == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {

    //load data

    //A3 <- x
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 3);
    MEMCPY32EX_R (p, (U32 *) x, size >> 2);
    //B3 <- y
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 3);
    MEMCPY32EX_R (p, (U32 *) y, size >> 2);

    //clean C1
    //p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 1);
    //MEMSET32 (p, 0, clue_page_size (size) >> 2);

    //do operation
    CLUE_POINT_DOUBLE (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR,"clue_ec_point_double:FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    //get result
    //output rx <- A2
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) rx, p, size >> 2);
    //output ry <- B2
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) ry, p, size >> 2);
  }
  return ret;
}
#endif

//point addition: // P(x1,y1)+Q(x2,y2), where k=1
//intputs:
//          x1, x1 of P(x1,y1)
//    y1, y1 of P(x1,y1)
//          x2, x2 of Q(x2,y2)
//    y2, y2 of Q(x2,y2)
//output: rx
//    ry
#ifdef CLUE_POINT_ADD
S16 clue_ec_point_add (U8 * x1, U8 * y1, U8 * x2, U8 * y2, U8 * rx, U8 * ry,
                       U16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((x1 == 0) || (y1 == 0) || (x2 == 0) || (y2 == 0) || (rx == 0)
      || (ry == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {

    //load data

    //A2 <- x1
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) x1, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//x1", 1);
    //B2 <- y1
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) y1, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//y1", 1);
    //A3 <- x2
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 3);
    MEMCPY32EX_R (p, (U32 *) x2, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//x2", 1);
    //B3 <- y2
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 3);
    MEMCPY32EX_R (p, (U32 *) y2, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//y2", 1);

    //clean C1
    //p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 1);
    //MEMSET32 (p, 0, clue_page_size (size) >> 2);

    //do operation
    CLUE_POINT_ADD (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_point_add:FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    //get result
    //output rx <- A2
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) rx, p, size >> 2);
    //output ry <- B2
    PDUMPWORD (EDDUMP, p, size, "RX", 1);
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R ((U32 *) ry, p, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "RY", 1);
  }
  return ret;
}
#endif

//point verification: verify if P(x,y) is on EC y^2=x^3 + ax + b
//intputs:
//          x, x of P(x,y)
//    y, y of P(x,y)
//output: ture of false
#ifdef CLUE_POINT_VERIFY
S16 clue_ec_point_verify (U8 * x, U8 * y, U16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((x == 0) || (y == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {

    //load data

    //A2 <- x
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) x, size >> 2);
    //B2 <- y
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 2);
    MEMCPY32EX_R (p, (U32 *) y, size >> 2);

    //clean C0
    p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 0);
    MEMSET32 (p, 0, clue_page_size (size) >> 2);

    //do operation
    CLUE_POINT_VERIFY (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_point_verify: FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    //get result
    if ((CLUE_READ_STATUS (_mmap) & CLUE_STATUS_ZERO) == CLUE_STATUS_ZERO)
      ret = SPACC_CRYPTO_OK;
    else
      ret = SPACC_CRYPTO_FAILED;

  }
  return ret;
}
#endif

//modular multiplication: c=a*b mod m
//intputs:      a
//          b
//output: c
#ifdef CLUE_MOD_MULT
S16 clue_ec_mod_mult (U8 * a, U8 * b, U8 * c, S16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((a == 0) || (b == 0) || (c == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {

    //load data

    //A0 <- a
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) a, size >> 2);
    //B0 <- b
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) b, size >> 2);

    //do operation
    CLUE_MOD_MULT (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF (ELP_ERR, "clue_ec_mod_mult: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
             (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
             (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    //get result
    //output c <- A0 
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R ((U32 *) c, p, size >> 2);
  }
  return ret;
}
#endif

//modular inversion: c=1/b mod m
//intputs:  a=1
//    b
//output: c
//    curve data: m (modulus)
#ifdef CLUE_MOD_INV
S16 clue_ec_mod_inv (U8 * b, U8 * c, S16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((b == 0) || (c == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {
    //load data
    {
      U8 a[BN_RSA_BASE_RADIX_SIZE];
      MEMSET (a, 0, sizeof (a));
      a[size - 1] = 1;          // a=1
      //C0  <- a = 1
      p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 0);
      MEMCPY32EX_R (p, (U32 *) a, size >> 2);
      //PDUMPWORD (EDDUMP, p, size, "//a", 1);
    }

    //A0 <- b
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) b, size >> 2);

    //clean C1
    //p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 1);
    //MEMSET32 (p, 0, clue_page_size (size) >> 2);

    //do operation
    CLUE_MOD_INV (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_mod_inv: FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }
    //get result, output c <- C0
    p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R ((U32 *) c, p, size >> 2);
  }
  return ret;
}
#endif

//modular division: c=a/b mod m
//intputs:  a
//    b
//output: c
#ifdef CLUE_MOD_DIV
S16 clue_ec_mod_div (U8 * a, U8 * b, U8 * c, S16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((a == 0) || (b == 0) || (c == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {
    //load data
    //C0 <- a
    p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) a, size >> 2);
    //A0 <- b
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) b, size >> 2);

    //clean C1
    //p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 1);
    //MEMSET32 (p, 0, clue_page_size (size) >> 2);

    //do operation
    CLUE_MOD_DIV (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_mod_div: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
             (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
             (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }
    //get result, output c <- C0
    p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R ((U32 *) c, p, size >> 2);
  }
  return ret;
}
#endif

//modular addition:   c=a+b mod m
//intputs:      
//    a
//          b
//output:    
//    c
#ifdef CLUE_MOD_ADD
S16 clue_ec_mod_add (U8 * a, U8 * b, U8 * c, S16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((a == 0) || (b == 0) || (c == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {
    //A0 <- a
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) a, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//a", 1);
    //B0 <- b
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) b, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//b", 1);

    //do operation
    CLUE_MOD_ADD (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_mod_add: FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    //get result, output c <- A0 
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R ((U32 *) c, p, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//c", 1);
  }
  return ret;
}
#endif

//modular subtraction:    c=a-b mod m
//intputs:      a
//          b
//output: c
#ifdef CLUE_MOD_SUB
S16 clue_ec_mod_sub (U8 * a, U8 * b, U8 * c, S16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((a == 0) || (b == 0) || (c == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {
    //A0 <- a
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) a, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//a", 1);
    //B0 <- b
    p = CLUE_REGION_B_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R (p, (U32 *) b, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//b", 1);

    //do operation
    CLUE_MOD_SUB (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_mod_sub:FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    //get result, output c <- A0 
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    MEMCPY32EX_R ((U32 *) c, p, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//c", 1);
  }
  return ret;
}
#endif

//modular reduction:    c=a mod m
//intputs:      a
//output: c
//we test hardware endian swap working here
#ifdef CLUE_MOD_RED
S16 clue_ec_mod_red (U8 * a, U8 * c, S16 size)
{
  S16 ret = SPACC_CRYPTO_OK;
  U32 *p;

  if ((a == 0) || (c == 0)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else if ((size == 0) || (size > BN_RSA_BASE_RADIX_SIZE)) {
    ret = SPACC_CRYPTO_INVALID_ARGUMENT;
  } else {
    //hardware endian swap set
    //CLUE_SET_ENDIAN_SWAP(_mmap);

    //C0 <- a
    p = CLUE_REGION_C_PAGE (_mmap, clue_page_size (size), 0);
    //for no endian swap
    MEMCPY32EX_R (p, (U32 *) a, size >> 2);
    //for hardware do endian swap
    //MEMCPY32HTONL_R (p, (U32 *) a, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//a", 1);

    //carry must be clear before calling
    //CLUE_CLEAR_CARRY(_mmap);
    //printf("flags=%x, page=%d\n", CLUE_READ_FLAGS(_mmap), clue_page_size(size));

    //do operation
    CLUE_MOD_RED (_mmap);
    if ((clue_start_engine (size)) != SPACC_CRYPTO_OK) {
      DPRINTF(ELP_ERR, "clue_ec_mod_red:FAILED: STATUS=0x%X, OPERATION=0x%X, CONTROL=0x%X\n",
         (unsigned int) CLUE_READ_STATUS (_mmap), (unsigned int)CLUE_READ_ENTRY (_mmap),
         (unsigned int) CLUE_READ_CTRL (_mmap));
      ret = SPACC_CRYPTO_FAILED;
    }

    //get result, output c <- A0 
    p = CLUE_REGION_A_PAGE (_mmap, clue_page_size (size), 0);
    //don't care the endian swap
    MEMCPY32EX_R ((U32 *) c, p, size >> 2);
    PDUMPWORD (EDDUMP, p, size, "//c(A0)", 1);

  }
  return ret;
}
#endif

#endif /* DO_ECC */
