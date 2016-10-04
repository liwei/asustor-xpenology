/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Semiconductor
// Copyright (C) 2002-2006, all rights reserved
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
//   ELP - TOE
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
// Filename:         $Source: /home/repository/cvsrep/spacc/include/elpspacchw.h,v $
// Current Revision: $Revision: 1.53 $
// Last Updated:     $Date: 2010-09-17 16:02:28 $
// Current Tag:      $Name: EPN1802_MINDSPEED_27SEP2010 $
//
//
//-----------------------------------------------------------------------*/

#ifndef _ELPSPACCHW_
#define _ELPSPACCHW_
#include "m86xxx_types.h"

#define SPACC_ALG_DES
#define SPACC_ALG_AES
#define SPACC_ALG_RC4

#define SPACC_ALG_MD5
#define SPACC_ALG_SHA1
#ifdef ELP_SHA2
   #ifndef SPACC_ALG_SHA224
      #define SPACC_ALG_SHA224
   #endif
   #ifndef SPACC_ALG_SHA256
      #define SPACC_ALG_SHA256
   #endif
   #ifndef SPACC_ALG_SHA384
      #define SPACC_ALG_SHA384
   #endif
   #ifndef SPACC_ALG_SHA512
      #define SPACC_ALG_SHA512
   #endif
#endif

/* number of DDT entries (gets x2 for in/out), totsize = X * 2 * 8 bytes */
#define SINGLE_DDT_SIZE      (10*4)

/* max DDT particle size */
#ifndef SPACC_MAX_PARTICLE_SIZE
#define SPACC_MAX_PARTICLE_SIZE     65536
#endif

#define SPACC_LOOP_WAIT  1000000

/* Avoid using NULL addresses, which indicate the end of a DDT table. */
#define SPACC_MASTER_OFFSET 128

/********* Register Offsets **********/
#define SPACC_REG_IRQ_EN	0x00000L
#define SPACC_REG_IRQ_STAT	0x00004L
#define SPACC_REG_SDMA_BRST_SZ	0x00010L
#define SPACC_CRYPTO_BASE	0x40000
#define SPACC_REG_IRQ_CTRL	SPACC_CRYPTO_BASE + 0x00008L
#define SPACC_REG_FIFO_STAT	SPACC_CRYPTO_BASE + 0x0000CL

#define SPACC_REG_SRC_PTR	SPACC_CRYPTO_BASE + 0x00020L
#define SPACC_REG_DST_PTR	SPACC_CRYPTO_BASE + 0x00024L
#define SPACC_REG_OFFSET	SPACC_CRYPTO_BASE + 0x00028L
#define SPACC_REG_PRE_AAD_LEN	SPACC_CRYPTO_BASE + 0x0002CL // new 
#define SPACC_REG_POST_AAD_LEN	SPACC_CRYPTO_BASE + 0x00030L // new, also no REG_AAD_LEN anymore

#define SPACC_REG_PROC_LEN	SPACC_CRYPTO_BASE + 0x00034L
#define SPACC_REG_ICV_LEN	SPACC_CRYPTO_BASE + 0x00038L
#define SPACC_REG_ICV_OFFSET	SPACC_CRYPTO_BASE + 0x0003CL
#define SPACC_REG_IV_OFFSET	SPACC_CRYPTO_BASE + 0x00040L

#define SPACC_REG_SW_ID		SPACC_CRYPTO_BASE + 0x00044L
#define SPACC_REG_AUX_INFO	SPACC_CRYPTO_BASE + 0x00048L
#define SPACC_REG_CTRL		SPACC_CRYPTO_BASE + 0x0004CL

#define SPACC_REG_STAT_POP	SPACC_CRYPTO_BASE + 0x00050L
#define SPACC_REG_STATUS	SPACC_CRYPTO_BASE + 0x00054L

#define SPACC_REG_KEY_SZ	SPACC_CRYPTO_BASE + 0x00100L

#define SPACC_REG_VIRTUAL_RQST  0x00140L
#define SPACC_REG_VIRTUAL_ALLOC 0x00144L
#define SPACC_REG_VIRTUAL_PRIO  0x00148L
#define SPACC_REG_VIRTUAL_RC4_KEY_RQST 0x00150L
#define SPACC_REG_VIRTUAL_RC4_KEY_GNT  0x00154L

#define SPACC_REG_ID            0x00180L

#define SPACC_ID_MINOR(x)   ((x)         & 0x0F)
#define SPACC_ID_MAJOR(x)   (((x) >>  4) & 0x0F)
#define SPACC_ID_QOS(x)     (((x) >>  8) & 0x01)
#define SPACC_ID_PDU(x)     (((x) >>  9) & 0x01)
#define SPACC_ID_AUX(x)     (((x) >> 10) & 0x01)
#define SPACC_ID_VIDX(x)    (((x) >> 12) & 0x07)
#define SPACC_ID_PROJECT(x) ((x)>>16)

/********** Context Offsets **********/

#define SPACC_CTX_CIPH_KEY     SPACC_CRYPTO_BASE + 0x04000L
#define SPACC_CTX_HASH_KEY     SPACC_CRYPTO_BASE + 0x08000L
#define SPACC_CTX_RC4_CTX      SPACC_CRYPTO_BASE + 0x20000L

/******** Sub-Context Offsets ********/

#define SPACC_CTX_AES_KEY      0x00
#define SPACC_CTX_AES_IV       0x20

#define SPACC_CTX_DES_KEY      0x08
#define SPACC_CTX_DES_IV       0x00

#define SPACC_CTX_RC4_KEY      0x00

#define SPACC_RC4_DMA_CTRL     0x00
#define SPACC_RC4_DMA_STAT     0x04
#define SPACC_RC4_DMA_CTX      0x200

#define SPACC_RC4_CTX_IDX_O    0
#define SPACC_RC4_CTX_IDX_W    8
#define _SPACC_RC4_IMPORT      31
#define SPACC_RC4_IMPORT      (1UL << _SPACC_RC4_IMPORT)

/******** Context Page Sizes *********/

#ifndef SPACC_CTX_PAGES
#   define SPACC_CTX_PAGES             1 
#endif
#define SPACC_MAX_CTX (SPACC_CTX_PAGES * 2)

// Cipher 
#ifdef SPACC_CTX_CIPH_PAGE_SIZE
#   define SPACC_CTX_CIPH_PAGE     SPACC_CTX_CIPH_PAGE_SIZE 
#else
#   define SPACC_CTX_CIPH_PAGE      128
#endif
// Hash
#ifdef SPACC_CTX_HASH_PAGE_SIZE
#   define SPACC_CTX_HASH_PAGE     SPACC_CTX_HASH_PAGE_SIZE 
#else
#   define SPACC_CTX_HASH_PAGE     64 
#endif
// RC4
#ifdef SPACC_CTX_RC4_PAGE_SIZE
#   define SPACC_CTX_RC4_PAGE     SPACC_CTX_RC4_PAGE_SIZE 
#else
#   define SPACC_CTX_RC4_PAGE          512
#endif

#ifndef SPACC_CTX_HASH_PAGE

#if   defined (SPACC_ALG_SHA384) || defined (SPACC_ALG_SHA512)
#   define SPACC_CTX_HASH_PAGE      128
#elif defined (SPACC_ALG_MD5)    || defined (SPACC_ALG_SHA1)   || \
      defined (SPACC_ALG_SHA224) || defined (SPACC_ALG_SHA256)
#   define SPACC_CTX_HASH_PAGE      64
#else
#   error "At least one hash algorithm must be defined."
#endif

#endif

/********** IRQ_EN Bit Masks **********/

#define _SPACC_IRQ_EN_CMD_EN		0
#define _SPACC_IRQ_EN_STAT_EN		4
#define _SPACC_IRQ_EN_RC4_DMA_EN	8
#define _SPACC_IRQ_RNG_EN		27
#define _SPACC_IRQ_PKA_EN		30
#define _SPACC_IRQ_EN_GLBL_EN		31

#define SPACC_IRQ_EN_CMD_EN		(1UL << _SPACC_IRQ_EN_CMD_EN)
#define SPACC_IRQ_EN_STAT_EN		(1UL << _SPACC_IRQ_EN_STAT_EN)
#define SPACC_IRQ_EN_RC4_DMA_EN		(1UL << _SPACC_IRQ_EN_RC4_DMA_EN)
#define SPACC_IRQ_RNG_EN		(1UL << _SPACC_IRQ_RNG_EN)
#define SPACC_IRQ_PKA_EN		(1UL << _SPACC_IRQ_PKA_EN)
#define SPACC_IRQ_EN_GLBL_EN		(1UL << _SPACC_IRQ_EN_GLBL_EN)

/********* IRQ_STAT Bitmasks *********/

#define _SPACC_IRQ_STAT_CMD         0
#define _SPACC_IRQ_STAT_STAT        4
#define _SPACC_IRQ_STAT_RC4_DMA     8

#define SPACC_IRQ_STAT_CMD         (1UL << _SPACC_IRQ_STAT_CMD)
#define SPACC_IRQ_STAT_STAT        (1UL << _SPACC_IRQ_STAT_STAT)
#define SPACC_IRQ_STAT_RC4_DMA     (1UL << _SPACC_IRQ_STAT_RC4_DMA)

/********* IRQ_CTRL Bitmasks *********/

#define SPACC_IRQ_CTRL_SET_STAT_CNT(n) ((n) << SPACC_IRQ_CTRL_STAT_CNT)

#if defined(SPACC_3)

   #define SPACC_IRQ_CTRL_CMD0_CNT    0
   #define SPACC_IRQ_CTRL_STAT_CNT   16

#elif defined(SPACC_3_QOS)

   #define SPACC_IRQ_CTRL_CMD0_CNT    0
   #define SPACC_IRQ_CTRL_CMD1_CNT    8
   #define SPACC_IRQ_CTRL_CMD2_CNT   16 
   #define SPACC_IRQ_CTRL_STAT_CNT   24

#else

   #error SPACC version is not defined

#endif

/******** FIFO_STAT Bitmasks *********/

#ifdef SPACC_3_QOS
   /* SPACC 3.0 with QOS */
   #define _SPACC_FIFO_STAT_CMD_CNT     0
   #define _SPACC_FIFO_STAT_CMD_FULL    7
   #define _SPACC_FIFO_STAT_CMD0_CNT   _SPACC_FIFO_STAT_CMD_CNT
   #define _SPACC_FIFO_STAT_CMD0_FULL  _SPACC_FIFO_STAT_CMD_FULL
   #define _SPACC_FIFO_STAT_CMD1_CNT    8
   #define _SPACC_FIFO_STAT_CMD1_FULL  15
   #define _SPACC_FIFO_STAT_CMD2_CNT   16
   #define _SPACC_FIFO_STAT_CMD2_FULL  23
   #define _SPACC_FIFO_STAT_STAT_CNT   24
   #define _SPACC_FIFO_STAT_STAT_EMPTY 31
#endif

#ifdef SPACC_3
   /* SPACC 3.0  */
   #define _SPACC_FIFO_STAT_CMD_CNT     0
   #define _SPACC_FIFO_STAT_CMD_FULL    7
   #define _SPACC_FIFO_STAT_STAT_CNT   16
   #define _SPACC_FIFO_STAT_STAT_EMPTY 31
#endif

#define SPACC_FIFO_STAT_CMD_FULL   (1UL << _SPACC_FIFO_STAT_CMD_FULL)
#define SPACC_FIFO_STAT_STAT_EMPTY (1UL << _SPACC_FIFO_STAT_STAT_EMPTY)
#define SPACC_IS_FIFO_STAT_CMD_FULL(f) (((f) >> _SPACC_FIFO_STAT_CMD_FULL) & 0x01)
#define SPACC_GET_FIFO_STAT_CMD_CNT(f)   (((f)>>_SPACC_FIFO_STAT_STAT_CMD_CNT)&0x7f)
#define SPACC_GET_FIFO_STAT_CNT(f)       (((f)>>_SPACC_FIFO_STAT_STAT_CNT)&0x7f)

/******* SDMA_BRST_SZ Bitmasks *******/

#if defined (SPACC_ALG_SHA384) || defined (SPACC_ALG_SHA512)
# define SPACC_SDMA_BRST_SZ_BRST_SZ 0, 5
#else
# define SPACC_SDMA_BRST_SZ_BRST_SZ 0, 4
#endif

/********* SRC_PTR Bitmasks **********/

#define SPACC_SRC_PTR_PTR           0xFFFFFFF8

/********* DST_PTR Bitmasks **********/

#define SPACC_DST_PTR_PTR           0xFFFFFFF8

/********** OFFSET Bitmasks **********/

#define SPACC_OFFSET_SRC_O          0
#define SPACC_OFFSET_SRC_W          16
#define SPACC_OFFSET_DST_O          16
#define SPACC_OFFSET_DST_W          16

#define SPACC_MIN_CHUNK_SIZE        1024
#define SPACC_MAX_CHUNK_SIZE        16384

/********* PKT_LEN Bitmasks **********/

#ifndef _SPACC_PKT_LEN_PROC_LEN
#define _SPACC_PKT_LEN_PROC_LEN    0
#endif
#ifndef _SPACC_PKT_LEN_AAD_LEN
#define _SPACC_PKT_LEN_AAD_LEN     16
#endif

/********* ICV_INFO Bitmasks *********/

//#define _SPACC_ICV_INFO_LEN        0
//#define _SPACC_ICV_INFO_OFFSET     16

//#define SPACC_SET_ICV_INFO_LEN(l)       ((l) & 0xFF)
//#define SPACC_SET_ICV_INFO_OFFSET(o) (((o) & 0xFF) << _SPACC_ICV_INFO_OFFSET)

/********** SW_ID Bitmasks ***********/

#define SPACC_SW_ID_ID_O            0
#define SPACC_SW_ID_PRIO            30
#define SPACC_SW_ID_ID_W            8

/* Priorities */
#define SPACC_SW_ID_PRIO_HI         0
#define SPACC_SW_ID_PRIO_MED        1
#define SPACC_SW_ID_PRIO_LOW        2

/*********** CTRL Bitmasks ***********/
#define _SPACC_CTRL_CIPH_ALG         0
#define _SPACC_CTRL_HASH_ALG         4
#define _SPACC_CTRL_CIPH_MODE        8
#define _SPACC_CTRL_HASH_MODE       12
#define _SPACC_CTRL_MSG_BEGIN       14
#define _SPACC_CTRL_MSG_END         15
#define _SPACC_CTRL_CTX_IDX         16
#define _SPACC_CTRL_ENCRYPT         24
#define _SPACC_CTRL_AAD_COPY        25
#define _SPACC_CTRL_ICV_PT          26
#define _SPACC_CTRL_ICV_ENC         27
#define _SPACC_CTRL_ICV_APPEND      28
#define _SPACC_CTRL_KEY_EXP         29
#define _SPACC_CTRL_SEC_KEY         31

/********* Virtual Spacc Priority Bitmasks **********/
#define _SPACC_VPRIO_MODE          0
#define _SPACC_VPRIO_WEIGHT        8

/********* AUX INFO Bitmasks *********/

#define _SPACC_AUX_INFO_DIR        0
#define _SPACC_AUX_INFO_BIT_ALIGN  1

/********* STAT_POP Bitmasks *********/

#define _SPACC_STAT_POP_POP         0

#define SPACC_STAT_POP_POP         (1UL << _SPACC_STAT_POP_POP)

/********** STATUS Bitmasks **********/

#define _SPACC_STATUS_SW_ID        0
#define _SPACC_STATUS_RET_CODE     24
#define SPACC_GET_STATUS_RET_CODE(s)          (((s)>>_SPACC_STATUS_RET_CODE)&0x7)

/********** KEY_SZ Bitmasks **********/

#define _SPACC_KEY_SZ_SIZE         0
#define _SPACC_KEY_SZ_CTX_IDX      8
#define _SPACC_KEY_SZ_CIPHER        31

#define SPACC_KEY_SZ_CIPHER        (1UL << _SPACC_KEY_SZ_CIPHER)

#define SPACC_SET_CIPHER_KEY_SZ(z,ctx)         (((z) & 0x7F) | (1UL << _SPACC_KEY_SZ_CIPHER) | ((ctx) << _SPACC_KEY_SZ_CTX_IDX))
#define SPACC_SET_HASH_KEY_SZ(z,ctx)           (((z) & 0x7F) | ((ctx) << _SPACC_KEY_SZ_CTX_IDX))

/************ SPAcc Flags ************/

/* Enables bus mastering rather than use evaluation board memory */
#define SPACC_FLAG_BUS_MASTER        1

/* Manage evaluation board's memory internally, using virtual DDTs */
#define SPACC_FLAG_AUTO_MEM          2

/* Flags to handle various core configurations */
#define SPACC_FLAG_NODES             256
#define SPACC_FLAG_NOAES             512
#define SPACC_FLAG_NORC4             1024
#define SPACC_FLAG_NOMD5             2048
#define SPACC_FLAG_NOSHA1            4096
#define SPACC_FLAG_NOSHA224          8192
#define SPACC_FLAG_NOSHA256          16384
#define SPACC_FLAG_NOSHA384          32768
#define SPACC_FLAG_NOSHA512          65536

/*************************************/

#define MASK_INTEGER(value, offset, width) ( \
   (((1 << (width)) - 1) & (value)) << (offset) \
)

#define UNMASK_INTEGER(value, offset, width) ( \
   ((value) >> (offset)) & ((1 << (width)) - 1) \
)

/******* Hash/Cipher Bitmasks ********/

#define SPACC_PROC_NULL     -1

#define SPACC_PROC_DES      1
#define SPACC_PROC_AES      2
#define SPACC_PROC_RC4      4

#define SPACC_PROC_ECB      0
#define SPACC_PROC_CBC      (1UL<<8)
#define SPACC_PROC_CTR      (2UL<<8)
#define SPACC_PROC_CCM      (3UL<<8)
#define SPACC_PROC_GCM      (5UL<<8)
#define SPACC_PROC_F8       (9UL<<8)

#define SPACC_PROC_MD5      65536
#define SPACC_PROC_SHA1     131072
#define SPACC_PROC_SHA224   262144
#define SPACC_PROC_SHA256   524288
#define SPACC_PROC_SHA384   1048576
#define SPACC_PROC_SHA512   2097152

#define SPACC_PROC_RAW      0
#define SPACC_PROC_SSLMAC   16777216
#define SPACC_PROC_HMAC     33554432

#define SPACC_PROC_C_MASK   (0x000000ff & ~SPACC_PROC_ENCRYPT & \
                                          ~SPACC_PROC_EXPAND)
#define SPACC_PROC_CM_MASK  0x0000ff00
#define SPACC_PROC_H_MASK   0x00ff0000
#define SPACC_PROC_HM_MASK  0xff000000

/* Used internally, do not set on process words sent to the API */
#define SPACC_PROC_EXPAND   64
#define SPACC_PROC_ENCRYPT  128

#define SPACC_CTX_C_MASK    0x0000ffff
#define SPACC_CTX_H_MASK    0xffff0000

/***********************************************************************************/
/***********************************************************************************/

#define CTRL_SET_CIPH_ALG(a)    ((a) << _SPACC_CTRL_CIPH_ALG)
#define CTRL_SET_CIPH_MODE(a)   ((a) << _SPACC_CTRL_CIPH_MODE)
#define CTRL_SET_HASH_ALG(a)    ((a) << _SPACC_CTRL_HASH_ALG)
#define CTRL_SET_HASH_MODE(a)   ((a) << _SPACC_CTRL_HASH_MODE)
#define CTRL_SET_ENCRYPT        (1UL << _SPACC_CTRL_ENCRYPT)
#define CTRL_SET_AAD_COPY       (1UL << _SPACC_CTRL_AAD_COPY)
#define CTRL_SET_ICV_PT         (1UL << _SPACC_CTRL_ICV_PT)
#define CTRL_SET_ICV_ENC        (1UL << _SPACC_CTRL_ICV_ENC)
#define CTRL_SET_ICV_APPEND     (1UL << _SPACC_CTRL_ICV_APPEND)
#define CTRL_SET_KEY_EXP        (1UL << _SPACC_CTRL_KEY_EXP)
#define CTRL_SET_MSG_BEGIN      (1UL << _SPACC_CTRL_MSG_BEGIN)
#define CTRL_SET_MSG_END        (1UL << _SPACC_CTRL_MSG_END)
#define CTRL_SET_CTX_IDX(a)     ((a) << _SPACC_CTRL_CTX_IDX)
#define CTRL_SET_SEC_KEY        (1UL << _SPACC_CTRL_SEC_KEY)

#define AUX_INFO_SET_DIR(a)     ((a) << _SPACC_AUX_INFO_DIR)
#define AUX_INFO_SET_BIT_ALIGN(a) ((a) << _SPACC_AUX_INFO_BIT_ALIGN)

#define VPRIO_SET(mode,weight) (((mode)<<_SPACC_VPRIO_MODE)|((weight)<<_SPACC_VPRIO_WEIGHT))

enum ecipher
{
  C_NULL   = 0,
  C_DES    = 1,
  C_AES    = 2,
  C_RC4    = 3,
  C_MULTI2 = 4,
  C_KASUMI = 5,
  C_SNOW3G_UEA2 = 6,

  C_MAX
};

enum eciphermode
{
  CM_ECB = 0,
  CM_CBC = 1,
  CM_CTR = 2,
  CM_CCM = 3,
  CM_GCM = 5,
  CM_OFB = 7,
  CM_CFB = 8,
  CM_F8  = 9,
  CM_XTS = 10,

  CM_MAX
};

enum ehash
{
  H_NULL   = 0,
  H_MD5    = 1,
  H_SHA1   = 2,
  H_SHA224 = 3,
  H_SHA256 = 4,
  H_SHA384 = 5,
  H_SHA512 = 6,
  H_XCBC   = 7,
  H_CMAC   = 8,
  H_KF9    = 9,
  H_SNOW3G_UIA2 = 10,
  H_CRC32_I3E802_3 = 11,
  H_MAX
};

enum ehashmode
{
  HM_RAW    = 0,
  HM_SSLMAC = 1,
  HM_HMAC   = 2,

  HM_MAX
};

enum eicvmode
{
  IM_ICV_HASH         = 0,      /* HASH of plaintext */
  IM_ICV_HASH_ENCRYPT = 1,      /* HASH the plaintext and encrypt the lot */
  IM_ICV_ENCRYPT_HASH = 2,      /* HASH the ciphertext */
  IM_ICV_IGNORE       = 3,

  IM_MAX
};

enum eicvpos
{
  IP_ICV_OFFSET = 0,
  IP_ICV_APPEND = 1,
  IP_ICV_IGNORE = 2,
  IP_MAX
};

enum spacc_ret_code
{
  SPACC_OK = 0,
  SPACC_ICVFAIL,
  SPACC_MEMERR,
  SPACC_BLOCKERR,
};

#define ELP_READ_UINT(mem)		     readl(mem)
#define ELP_WRITE_UINT(mem, val)	     writel(val, mem)

/***********************************************************************************/
/*  MEMORY LAYOUT   MEMORY LAYOUT     MEMORY LAYOUT     MEMORY LAYOUT   */
/***********************************************************************************/

// DPRAM on PCI configuration
#define SPACC_DPRAM_BASE              (0)
// 0x2000 - 0x20 (0x20 is for the DDT )
#define SPACC_DPRAM_TOTAL             (0x2000)
#define SPACC_DPRAM_SIZE             (0x2000 - 0x20 )
#define SPACC_DPRAM_PACKET_OUT_SIZE  (0x800) // 2K for in/out
#define SPACC_DPRAM_PACKET_IN_SIZE    (0x800)

#define SPACC_OUT_BASE       (0)
#define SPACC_IN_BASE        (SPACC_DPRAM_PACKET_OUT_SIZE)
#define SPACC_SRC_DDT_BASE   (SPACC_DPRAM_PACKET_OUT_SIZE + SPACC_DPRAM_PACKET_IN_SIZE)
#define SPACC_DST_DDT_BASE   (SPACC_DPRAM_PACKET_OUT_SIZE + SPACC_DPRAM_PACKET_IN_SIZE + 0x10)

/***********************************************************************************/
/*  TRNG   TRNG     TRNG     TRNG     TRNG     TRNG     TRNG     TRNG     TRNG     */
/***********************************************************************************/
#define TRNG_BASE	0x18000

#define TRNG_CTRL	TRNG_BASE
#define TRNG_IRQ_STAT	TRNG_BASE + 0x04
#define TRNG_IRQ_EN	TRNG_BASE + 0x08
#define TRNG_DATA	TRNG_BASE + 0x10  // 4 WORDS
#define TRNG_DATA1	TRNG_BASE + 0x14
#define TRNG_DATA2	TRNG_BASE + 0x18
#define TRNG_DATA3	TRNG_BASE + 0x1C

#define TRNG_IRQ_STAT_BIT	27
#define TRNG_IRQ_DONE		(1 << TRNG_IRQ_STAT_BIT)

#define TRNG_IRQ_EN_BIT		27
#define TRNG_IRQ_ENABLE		(1 << TRNG_IRQ_EN_BIT)

#define TRNG_SIZE_BYTES 32
#define TRNG_SIZE_WORDS 4

// Control register definitions
#define TRNG_CTRL_RAND_RESEED               31
#define TRNG_CTRL_NONCE_RESEED              30
#define TRNG_CTRL_NONCE_RESEED_LD           29
#define TRNG_CTRL_NONCE_RESEED_SELECT       28
#define TRNG_CTRL_GEN_NEW_RANDOM             0

#define TRNG_RAND_RESEED               (1 << TRNG_CTRL_RAND_RESEED)
#define TRNG_NONCE_RESEED              (1 << TRNG_CTRL_NONCE_RESEED)
#define TRNG_NONCE_RESEED_LD           (1 << TRNG_CTRL_NONCE_RESEED_LD)
#define TRNG_NONCE_RESEED_SELECT       (1 << TRNG_CTRL_NONCE_RESEED_SELECT)
#define TRNG_GET_NEW                   (1 << TRNG_CTRL_GEN_NEW_RANDOM)
#define TRNG_BUSY                      (1 << TRNG_CTRL_GEN_NEW_RANDOM)

#define TRNG_RAND_RESEED_MODE          1
#define TRNG_NONCE_RESEED_MODE         2

#define LOOP_WAIT	1000000

/**************************************************************************/
/*  PKA   PKA     PKA     PKA     PKA     PKA     PKA     PKA     PKA     */
/**************************************************************************/
//#define CLUE_BASE	0x0
#define CLUE_BASE	0x20000
#define PKA_MEMORY_BASE_OFFSET 0x20000

// 32 bits register
#define CLUE_CTRL       CLUE_BASE + 0x0000
#define CLUE_ENTRY      CLUE_BASE + 0x0004
#define CLUE_ADDR       CLUE_BASE + 0x0008
#define CLUE_OPCODE	CLUE_BASE + 0x000C
#define CLUE_STACK      CLUE_BASE + 0x0010
#define CLUE_CONF_REG   CLUE_BASE + 0x001C
#define CLUE_STATUS     CLUE_BASE + 0x0020
#define CLUE_FLAGS      CLUE_BASE + 0x0024
#define CLUE_WATCHDOG   CLUE_BASE + 0x0028
#define CLUE_CYCLE_SINCE_GO   CLUE_BASE + 0x002C
#define CLUE_INST_SINCE_GO    CLUE_BASE + 0x0014
#define CLUE_INDEX_I    CLUE_BASE + 0x0030
#define CLUE_INDEX_J    CLUE_BASE + 0x0034
#define CLUE_INDEX_K    CLUE_BASE + 0x0038
#define CLUE_INDEX_L    CLUE_BASE + 0x003C
#define CLUE_INT_EN     CLUE_BASE + 0x0040
#define CLUE_JUMP_PROB  CLUE_BASE + 0x0044
#define CLUE_LFSR_SEED  CLUE_BASE + 0x0048

#define CLUE_OPERAND_RA   CLUE_BASE + 0x0400
#define CLUE_OPERAND_RB   CLUE_BASE + 0x0800
#define CLUE_OPERAND_RC   CLUE_BASE + 0x0C00
#define CLUE_OPERAND_RD   CLUE_BASE + 0x1000
#define CLUE_SEQUENCE     CLUE_BASE + 0x2000

// INT EN REGISTER BITS in 0x0040
#define CLUE_INT_BIT_IRQ_EN       30
#define CLUE_CTRL_BIT_IRQ_EN      CLUE_INT_BIT_IRQ_EN

// CONTROL REGISTERS definition in 0x0000
#define CLUE_CTRL_BIT_GO              31
#define CLUE_CTRL_BIT_STOP_ROST       27
#define CLUE_CTRL_BIT_ENDIAN_SWAP     26
#define CLUE_CTRL_BIT_M521_MODE       16
#define CLUE_CTRL_BIT_BASE_RADIX       8
#define CLUE_CTRL_BIT_PARTIAL_RADIX    0

#define CLUE_CTRL_GO               (U32)(1UL<<CLUE_CTRL_BIT_GO)
#define CLUE_CTRL_IRQ_EN           (U32)(1UL<<CLUE_CTRL_BIT_IRQ_EN)
#define CLUE_CTRL_STOP_ROST        (U32)(1UL<<CLUE_CTRL_BIT_STOP_ROST)
#define CLUE_CTRL_ENDIAN_SWAP      (U32)(1UL<<CLUE_CTRL_BIT_ENDIAN_SWAP)
#define CLUE_CTRL_BASE_RADIX(s)    (U32)((s)<<CLUE_CTRL_BIT_BASE_RADIX)
#define CLUE_CTRL_PARTIAL_RADIX(s) (U32)((s)<<CLUE_CTRL_BIT_PARTIAL_RADIX)
#define CLUE_CTRL_M521_MODE        (U32)(9UL << CLUE_CTRL_BIT_M521_MODE)

// CONTROL REGISTERS definition in 0x0004
#define CLUE_ENTRY_BIT_NEXT_SEQ_INDEX   0
#define CLUE_ENTRY_NEXT_SEQ_INDEX(s)   (U32)((s)<<CLUE_ENTRY_BIT_NEXT_SEQ_INDEX)

// CONTROL REGISTERS definition in 0x0008
#define CLUE_ADDR_BIT_NEXT_SEQ_ADDR      0
#define CLUE_ADDR_NEXT_SEQ_ADDR(s)      (U32)((s)<<CLUE_ADDR_BIT_NEXT_SEQ_ADDR)

// CONTROL REGISTERS definition in 0x000C
#define CLUE_OPCODE_BIT_LAST_OPCODE      0
#define CLUE_OPCODE_LAST_OPCODE(s)      (U32)((s)<<CLUE_OPCODE_BIT_LAST_OPCODE)

// CONTROL REGISTERS definition in 0x0010
#define CLUE_STACK_BIT_SLOTS_USED      0
#define CLUE_STACK_SLOTS_USED(s)      (U32)((s)<<CLUE_STACK_BIT_SLOTS_USED)

#define CLUE_REASON_NORMAL        0
#define CLUE_REASON_INVALID_OP    1
#define CLUE_REASON_FSTACK_UFLOW  2
#define CLUE_REASON_FSTACK_OFLOW  3
#define CLUE_REASON_WATCHDOG      4
#define CLUE_REASON_STOP_RQST     5
#define CLUE_REASON_PSTACK_UFLOW  6
#define CLUE_REASON_PSTACK_OFLOW  7
#define CLUE_REASON_MEM_PORT_COL  8
#define CLUE_REASON_INVALID_PNT_FAULT	67
#define CLUE_REASON_INVALID_KEY_FAULT	66
#define CLUE_REASON_NO_KEY_FAULT	65
#define CLUE_REASON_NO_EXPONENT_FAULT	64
#define CLUE_REASON_INVALID_EXPONENT	253
#define CLUE_REASON_UNLICENSED_SIZE	254

// CONTROL REGISTERS definition in 0x0020
#define CLUE_STATUS_BIT_BUSY          31
#define CLUE_STATUS_BIT_IRQ           CLUE_INT_BIT_IRQ_EN
#define CLUE_STATUS_BIT_ZERO          28
#define CLUE_STATUS_BIT_STOP_REASON   16
#define CLUE_STATUS_BUSY              (U32)(1UL<<CLUE_STATUS_BIT_BUSY)
#define CLUE_STATUS_IRQ               (U32)(1UL<<CLUE_STATUS_BIT_IRQ)
#define CLUE_STATUS_ZERO              (U32)(1UL<<CLUE_STATUS_BIT_ZERO)
#define CLUE_STATUS_STOP_REASON(s)    (U32)((s)<<CLUE_STATUS_BIT_STOP_REASON)

// CONTROL REGISTERS definition in 0x0024
#define CLUE_FLAGS_BIT_F3           7
#define CLUE_FLAGS_BIT_F2           6
#define CLUE_FLAGS_BIT_F1           5
#define CLUE_FLAGS_BIT_F0           4
#define CLUE_FLAGS_BIT_CARRY        3
#define CLUE_FLAGS_BIT_BORROW       2
#define CLUE_FLAGS_BIT_MEMBIT       1
#define CLUE_FLAGS_BIT_ZERO         0
#define CLUE_FLAGS_F3               (U32)(1UL<<CLUE_FLAGS_BIT_F3)
#define CLUE_FLAGS_F2               (U32)(1UL<<CLUE_FLAGS_BIT_F2)
#define CLUE_FLAGS_F1               (U32)(1UL<<CLUE_FLAGS_BIT_F1)
#define CLUE_FLAGS_F0               (U32)(1UL<<CLUE_FLAGS_BIT_F0)
#define CLUE_FLAGS_CARRY            (U32)(1UL<<CLUE_FLAGS_BIT_CARRY)
#define CLUE_FLAGS_BORROW           (U32)(1UL<<CLUE_FLAGS_BIT_BORROW)
#define CLUE_FLAGS_MEMBIT           (U32)(1UL<<CLUE_FLAGS_BIT_MEMBIT)
#define CLUE_FLAGS_ZERO             (U32)(1UL<<CLUE_FLAGS_BIT_ZERO)

// CONTROL REGISTERS definition in 0x0028
#define CLUE_WATCHDOG_BIT_CYCLES_UNTIL_HALT   0 //default hFFFF FFFF
#define CLUE_WATCHDOG_CYCLES_UNTIL_HALT(s)   (U32)((s)<<CLUE_WATCHDOG_BIT_CYCLES_UNTIL_HALT)

// CONTROL REGISTERS definition in 0x002C
#define CLUE_CYCLE_SINCE_GO_BIT_      0
#define CLUE_CYCLE_SINCE_GO_(s)      (U32)((s)<<CLUE_CYCLE_SINCE_GO_BIT_)

// CONTROL REGISTERS definition in 0x0030
#define CLUE_INDEX_I_BIT_         0  //0-15
#define CLUE_INDEX_I_(s)         (U32)((s)<< CLUE_INDEX_I_BIT_)
// CONTROL REGISTERS definition in 0x0034
#define CLUE_INDEX_J_BIT_         0  //0-15
#define CLUE_INDEX_J_(s)         (U32)((s)<< CLUE_INDEX_J_BIT_)
// CONTROL REGISTERS definition in 0x0038
#define CLUE_INDEX_K_BIT_         0  //0-15
#define CLUE_INDEX_K_(s)         (U32)((s)<< CLUE_INDEX_K_BIT_)
// CONTROL REGISTERS definition in 0x003C
#define CLUE_INDEX_L_BIT_         0  //0-15
#define CLUE_INDEX_L_(s)         (U32)((s)<< CLUE_INDEX_L_BIT_)

#define CLUE_CTRL_REG(mmap)       (U32 *)(mmap + CLUE_CTRL)
#define CLUE_PROG_IDX_REG(mmap)   (U32 *)(mmap + CLUE_ENTRY)
#define CLUE_STATUS_REG(mmap)     (U32 *)(mmap + CLUE_STATUS)

#define CLUE_CODE_MEMORY_START(mmap)   (U32 *)(mmap + CLUE_SEQUENCE)
#define CLUE_REGION_A_START(mmap)      (U32 *)(mmap + CLUE_OPERAND_RA)
#define CLUE_REGION_B_START(mmap)      (U32 *)(mmap + CLUE_OPERAND_RB)
#define CLUE_REGION_C_START(mmap)      (U32 *)(mmap + CLUE_OPERAND_RC)
#define CLUE_REGION_D_START(mmap)      (U32 *)(mmap + CLUE_OPERAND_RD)
#define CLUE_REGION_A_PAGE(mmap,size,idx)   (U32 *)(mmap + CLUE_OPERAND_RA + size*idx)
#define CLUE_REGION_B_PAGE(mmap,size,idx)   (U32 *)(mmap + CLUE_OPERAND_RB + size*idx)
#define CLUE_REGION_C_PAGE(mmap,size,idx)   (U32 *)(mmap + CLUE_OPERAND_RC + size*idx)
#define CLUE_REGION_D_PAGE(mmap,size,idx)   (U32 *)(mmap + CLUE_OPERAND_RD + (U32)size*idx)

#define CLUE_A_PAGE_SIZE   512
#define CLUE_B_PAGE_SIZE   512
#define CLUE_C_PAGE_SIZE   512
#define CLUE_D_PAGE_SIZE   1024

#define CLUE_SEQUENCE_SIZE   4096

// for CLUE RSA, 32 bit memory access only!!!
// Basic 32 bit memory access macros
// 32 memory write byte operation
#define CLUE_WRITE_UINT(mem,val)             (*(volatile U32 *)(mem)=(U32)(val))
// 32 memory read byte operation
#define CLUE_READ_UINT(mem)                  *(volatile U32 *)(mem)

// Read registers' values macro
#define CLUE_READ_CTRL(mmap)      CLUE_READ_UINT((U32)mmap+CLUE_CTRL)
#define CLUE_READ_CONF(mmap)      CLUE_READ_UINT((U32)mmap+CLUE_CONF_REG)
#define CLUE_READ_STATUS(mmap)    CLUE_READ_UINT((U32)mmap+CLUE_STATUS)
#define CLUE_READ_ENTRY(mmap)     CLUE_READ_UINT((U32)mmap+CLUE_ENTRY)
#define CLUE_READ_FLAGS(mmap)     CLUE_READ_UINT((U32)mmap+CLUE_FLAGS)
#define CLUE_READ_REASON(mmap)    ((CLUE_READ_UINT((U32)mmap+CLUE_STATUS) >> CLUE_STATUS_BIT_STOP_REASON) & 0xFF)
#define CLUE_READ_STACK(mmap)     CLUE_READ_UINT((U32)mmap+CLUE_STACK)
#define CLUE_READ_CYCLES(mmap)    CLUE_READ_UINT((U32)mmap+CLUE_CYCLE_SINCE_GO)
#define CLUE_READ_INSTS(mmap)     CLUE_READ_UINT((U32)mmap+CLUE_INST_SINCE_GO)

#define CLUE_READ_I(mmap)  CLUE_READ_UINT((U32)mmap+CLUE_INDEX_I)
#define CLUE_READ_J(mmap)  CLUE_READ_UINT((U32)mmap+CLUE_INDEX_J)
#define CLUE_READ_K(mmap)  CLUE_READ_UINT((U32)mmap+CLUE_INDEX_K)
#define CLUE_READ_L(mmap)  CLUE_READ_UINT((U32)mmap+CLUE_INDEX_L)

#define CLUE_CLEAR_REGISTER(mmap)\
   CLUE_WRITE_UINT(((U32)mmap+CLUE_CTRL),0);\
   CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0)

#define CLUE_SET_STACK(mmap,val) CLUE_WRITE_UINT(((U32)mmap+CLUE_STACK),val)
#define CLUE_CLEAR_STACK(mmap)   CLUE_WRITE_UINT(((U32)mmap+CLUE_STACK),0)
#define CLUE_STOP_REQUEST(mmap)  CLUE_WRITE_UINT(((U32)mmap+CLUE_CTRL),0x08000000)

#define CLUE_CLEAR_FLAG(mmap)          CLUE_WRITE_UINT(((U32)mmap+CLUE_FLAGS),0x0)
#define CLUE_CLEAR_CARRY(mmap)         CLUE_WRITE_UINT(((U32)mmap+CLUE_FLAGS),CLUE_READ_FLAGS(mmap)&~CLUE_FLAGS_CARRY)
#define CLUE_SET_ENDIAN_SWAP(mmap)     CLUE_WRITE_UINT(((U32)mmap+CLUE_CTRL),CLUE_READ_CTRL(mmap)|CLUE_CTRL_ENDIAN_SWAP)
#define CLUE_CLEAR_ENDIAN_SWAP(mmap)   CLUE_WRITE_UINT(((U32)mmap+CLUE_CTRL),CLUE_READ_CTRL(mmap)&~CLUE_CTRL_ENDIAN_SWAP)

// -------------------------------------------------
#define CLUE_CALC_MP(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x30)
#define CLUE_R_INV(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x32)
#define CLUE_R_SQR(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x37)
// crt
// crt_key_setup
#define CLUE_DIV_MODP(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x2c)
// is_a_m3
// i_to_m
#define CLUE_MOD_ADD(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x28)
#define CLUE_MOD_DIV(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x2c)
#define CLUE_MOD_EXP(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x1f)
#define CLUE_MOD_INV(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x2c)
#define CLUE_MOD_MULT(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x26)
#define CLUE_MOD_SUB(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x2a)
#define CLUE_M_RESIDUE(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x21)
// m_to_i
#define CLUE_POINT_ADD(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x14)
// padd_std_prj
#define CLUE_POINT_DOUBLE(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x10)
// pdbl_std_prj
#define CLUE_POINT_MULT(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x8)
#define CLUE_POINT_VERIFY(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x19)
#define CLUE_MOD_RED(mmap)		CLUE_WRITE_UINT(((U32)mmap+CLUE_ENTRY),0x2e)
// std_prj_to_affine

// Acknowledge interrupt macro
#define CLUE_INT_ACK(mmap)      CLUE_WRITE_UINT(((U32)mmap+CLUE_STATUS),CLUE_READ_STATUS(mmap)|CLUE_STATUS_IRQ)

// Load control registes and start an operation
// This is NO IRQ MODE , clear flag first CLUE_CLEAR_FLAG(mmap);
#define CLUE_START_OPERATION(mmap, s) CLUE_WRITE_UINT(((U32)mmap+CLUE_CTRL),CLUE_CTRL_PARTIAL_RADIX(s)|CLUE_CTRL_GO)

// This is IRQ MODE; Interrupt will be generated upon completion
#define CLUE_START_OPERATION_INT(mmap, s) CLUE_WRITE_UINT(((U32)mmap+CLUE_CTRL),CLUE_CTRL_IRQ_EN|CLUE_CTRL_BASE_RADIX(s)|CLUE_CTRL_GO)
//#define CLUE_START_OPERATION_INT(mmap, s) CLUE_WRITE_UINT(((U32)mmap+CLUE_CTRL),CLUE_CTRL_BASE_RADIX(s)|CLUE_CTRL_GO)

// Wait loop expire counter
#define CLUE_LOOP_WAIT      10000000

#endif
