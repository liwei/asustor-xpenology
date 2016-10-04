/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Semiconductor
// Copyright (C) 2002-2007, all rights reserved
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
// Filename:         $Source: /home/repository/cvsrep/spacc/include/elpspacc.h,v $
// Current Revision: $Revision: 1.70 $
// Last Updated:     $Date: 2010-09-17 16:00:57 $
// Current Tag:      $Name: EPN1802_MINDSPEED_27SEP2010 $
//
//
//-----------------------------------------------------------------------*/

#ifndef _ELPSPACC_H_
#define _ELPSPACC_H_

#include "m86xxx_types.h"

#include "m86xxx_spacchw.h"
#include "m86xxx_var.h"
#include "cryptodev.h"

#define CRYPTO_INVALID_ICV_KEY_SIZE  (S32)-100
#define CRYPTO_INVALID_PARAMETER_SIZE (S32)-101
#define CRYPTO_SEQUENCE_OVERFLOW      (S32)-102
#define CRYPTO_DISABLED               (S32)-103
#define CRYPTO_INVALID_VERSION        (S32)-104
#define CRYPTO_FATAL                  (S32)-105
#define CRYPTO_INVALID_PAD            (S32)-106
#define CRYPTO_FIFO_FULL              (S32)-107
#define CRYPTO_INVALID_SEQUENCE       (S32)-108

#define SPACC_CRYPTO_OK				(S32)0
#define SPACC_CRYPTO_FAILED			(S32)-1
#define SPACC_CRYPTO_INPROGRESS			(S32)-2
#define SPACC_CRYPTO_INVALID_HANDLE		(S32)-3
#define SPACC_CRYPTO_INVALID_CONTEXT		(S32)-4
#define SPACC_CRYPTO_INVALID_SIZE		(S32)-5
#define SPACC_CRYPTO_NOT_INITIALIZED		(S32)-6
#define SPACC_CRYPTO_NO_MEM			(S32)-7
#define SPACC_CRYPTO_INVALID_ALG		(S32)-8
#define SPACC_CRYPTO_INVALID_KEY_SIZE		(S32)-9
#define SPACC_CRYPTO_INVALID_ARGUMENT		(S32)-10
#define SPACC_CRYPTO_MODULE_DISABLED		(S32)-11
#define SPACC_CRYPTO_NOT_IMPLEMENTED		(S32)-12
#define SPACC_CRYPTO_INVALID_BLOCK_ALIGNMENT	(S32)-13
#define SPACC_CRYPTO_INVALID_MODE		(S32)-14
#define SPACC_CRYPTO_INVALID_KEY		(S32)-15
#define SPACC_CRYPTO_AUTHENTICATION_FAILED	(S32)-16
#define SPACC_CRYPTO_INVALID_IV_SIZE		(S32)-17
#define SPACC_CRYPTO_MEMORY_ERROR		(S32)-18
#define SPACC_CRYPTO_LAST_ERROR			(S32)-19
#define SPACC_CRYPTO_HALTED			(S16)-20
#define SPACC_CRYPTO_TIMEOUT			(S16)-21
#define SPACC_CRYPTO_SRM_FAILED			(S32)-22

#define SPACC_CRYPTO_COMMON_ERROR_MAX		(S32)-100

#define ENCRYPT          0
#define DECRYPT          1

#define OP_ENCRYPT          0
#define OP_DECRYPT          1

#define CRYPTO_MAC_MIN_BLOCK_SIZE     1
#define CRYPTO_MAC_MD5_SIZE           16
#define CRYPTO_MAC_SHA1_SIZE          20
#define CRYPTO_MAC_SHA256_SIZE          32
#define CRYPTO_MAC_SHA512_SIZE          64
#define CRYPTO_MAC_ALIGNMENT          64

#define SPACC_CRYPTO_OPERATION   1
#define SPACC_HASH_OPERATION     2

#define SPACC_AADCOPY_FLAG        0x80000000

#define CRYPTO_AES_KEY_LENGTH_128   16
#define CRYPTO_AES_KEY_LENGTH_192   24
#define CRYPTO_AES_KEY_LENGTH_256   32

#define CRYPTO_AES_KEY_LENGTH_MIN CRYPTO_AES_KEY_LENGTH_128
#define CRYPTO_AES_KEY_LENGTH_MAX CRYPTO_AES_KEY_LENGTH_256

#define CRYPTO_3DES_KEY_LENGTH            24
#define CRYPTO_3DES_KEY_LENGTH_MIN CRYPTO_3DES_KEY_LENGTH
#define CRYPTO_3DES_KEY_LENGTH_MAX CRYPTO_3DES_KEY_LENGTH

#define CRYPTO_DES_KEY_LENGTH            8
#define CRYPTO_DES_KEY_LENGTH_MIN CRYPTO_DES_KEY_LENGTH
#define CRYPTO_DES_KEY_LENGTH_MAX CRYPTO_DES_KEY_LENGTH

#define CRYPTO_DES_IV_LENGTH            8
#define CRYPTO_DES_MIN_BLOCK_SIZE       8
#define CRYPTO_AES_IV_LENGTH           16
#define CRYPTO_AES_MIN_BLOCK_SIZE      16
#define CRYPTO_HASH_SHA1_SIZE          20
#define CRYPTO_HASH_SHA224_SIZE        28
#define CRYPTO_HASH_SHA256_SIZE        32
#define CRYPTO_HASH_SHA384_SIZE        48
#define CRYPTO_HASH_SHA512_SIZE        64
#define CRYPTO_HASH_MD5_SIZE           16

#define CRYPTO_MAC_XCBC_SIZE           16 
#define CRYPTO_MAC_CMAC_SIZE           16 
#define CRYPTO_MAC_KASUMIF9_SIZE        8 
#define CRYPTO_MAC_SNOW3GUIA2_SIZE      4 
#define CRYPTO_HASH_CRC32_SIZE          4

#define CRYPTO_MODULE_SPACC	0x0001
#define CRYPTO_MODULE_TRNG	0x0010
#define CRYPTO_MODULE_PKA	0x0100

// Each module have the same number of contexts
#ifndef CRYPTO_CONTEXTS_MAX
#define CRYPTO_CONTEXTS_MAX	16
#endif

enum crypto_modes {
  CRYPTO_MODE_NULL,
  CRYPTO_MODE_RC4_40,
  CRYPTO_MODE_RC4_128,
  CRYPTO_MODE_AES_ECB,
  CRYPTO_MODE_AES_CBC,
  CRYPTO_MODE_AES_CTR,
  CRYPTO_MODE_AES_CCM,
  CRYPTO_MODE_AES_GCM,
  CRYPTO_MODE_AES_F8,
  CRYPTO_MODE_AES_XTS,
  CRYPTO_MODE_MULTI2_ECB,
  CRYPTO_MODE_MULTI2_CBC,
  CRYPTO_MODE_MULTI2_OFB,
  CRYPTO_MODE_MULTI2_CFB,
  CRYPTO_MODE_3DES_CBC,
  CRYPTO_MODE_3DES_ECB,
  CRYPTO_MODE_DES_CBC,
  CRYPTO_MODE_DES_ECB,
  CRYPTO_MODE_KASUMI_ECB,
  CRYPTO_MODE_KASUMI_F8,
  CRYPTO_MODE_SNOW3G_UEA2,

  CRYPTO_MODE_HASH_SHA1,
  CRYPTO_MODE_HMAC_SHA1,
  CRYPTO_MODE_HASH_MD5,
  CRYPTO_MODE_HMAC_MD5,
  CRYPTO_MODE_HASH_SHA224,
  CRYPTO_MODE_HMAC_SHA224,
  CRYPTO_MODE_HASH_SHA256,
  CRYPTO_MODE_HMAC_SHA256,
  CRYPTO_MODE_HASH_SHA384,
  CRYPTO_MODE_HMAC_SHA384,
  CRYPTO_MODE_HASH_SHA512,
  CRYPTO_MODE_HMAC_SHA512,

  CRYPTO_MODE_MAC_XCBC,
  CRYPTO_MODE_MAC_CMAC,
  CRYPTO_MODE_MAC_KASUMI_F9,
  CRYPTO_MODE_MAC_SNOW3G_UIA2,

  CRYPTO_MODE_SSLMAC_MD5,
  CRYPTO_MODE_SSLMAC_SHA1,
  CRYPTO_MODE_HASH_CRC32,
};

enum {
  SPACC_DEQUEUE_MAP = 0,
  SPACC_DEQUEUE_UNMAP
};

/*
 * Codes
*/
#define ELP_HMAC_NULL		0
#define ELP_HMAC_MD5		1
#define ELP_HMAC_SHA1		2
#define ELP_HMAC_SHA2		3
#define ELP_GCM64		4
#define ELP_GCM96		5
#define ELP_GCM128		6

#define ELP_CIPHER_NULL		0
#define ELP_CIPHER_DES		1
#define ELP_CIPHER_3DES		2
#define ELP_CIPHER_AES128	3
#define ELP_CIPHER_AES192	4
#define ELP_CIPHER_AES256	5
#define ELP_CIPHER_AES128_GCM	6
#define ELP_CIPHER_AES192_GCM	7
#define ELP_CIPHER_AES256_GCM	8
#define ELP_CIPHER_AES128_CTR	9
#define ELP_CIPHER_AES192_CTR	10
#define ELP_CIPHER_AES256_CTR	11
#define ELP_CIPHER_ARC4_40	12
#define ELP_CIPHER_ARC4_128	13

typedef struct ddt_descr_
{
   void *map;
   void *buf;
   U32   len;
} ddt_descr;

typedef struct ddt_entry_
{
   U32 ptr;
   U32 len;
} ddt_entry;

#ifndef MAX_DDT_ENTRIES
   #define MAX_DDT_ENTRIES 20
#endif

#ifndef MAX_JOBS
   #define MAX_JOBS        16
#endif

//  crypto module context descriptor
typedef struct handle_ctx_
{
  S32 taken;
  U32 enc_mode;                 // Encription Algorith mode
  U32 hash_mode;                // HASH Algorith mode
  U32 icv_len;
  U32 icv_offset;
  U32 op;                       // Operation
  U32 ctrl;                     // CTRL shadoe register
  U32 first_use;                // indicates that context just has been initialized/taken
  // and this is the first use
  U32 *ctx;                     // crypto module context pointer
  U32 pre_aad_sz, post_aad_sz;                   // size of AAD for the latest packet
  U32 hkey_sz, ckey_sz;
  volatile U32 job_id[MAX_JOBS], job_idx, job_done, job_err;
  //, pkt_sz;
  U32 *ciph_key, *hash_key, *rc4_key;
  U32 auxinfo_dir, auxinfo_bit_align; // Direction and bit alignment parameters for the AUX_INFO reg

  struct elp_operand	op_src;	/* source operand */
  struct elp_operand	op_dst; 	/* destination operand */
  struct cryptop *crp;

   void *ddt_map, *sddt_map;               // physical (mapped) DDT address
   ddt_entry *ddt, *sddt;
   ddt_descr ddt_desc[MAX_DDT_ENTRIES];   // assosiated user bugffers
   ddt_descr sddt_desc[MAX_DDT_ENTRIES];  // assosiated user bugffers
   int ddt_descr_status[MAX_DDT_ENTRIES];  // status array to mark if both input and output buffer address is same
   U32 ddt_len;                           //  total length of the chain
   int ddt_idx;                           // next free ddt index 
   U32 sddt_len;                          //  total length of the chain
   int sddt_idx;                          // next free ddt index 

  struct platform_device *dev;

} elp_spacc_handle_ctx;

#define	op_src_skb	op_src.u.skb
#define	op_src_io	op_src.u.io
#define	op_src_map	op_src.map
#define	op_src_nsegs	op_src.nsegs
#define	op_src_segs	op_src.segs
#define	op_src_mapsize	op_src.mapsize

#define	op_dst_skb	op_dst.u.skb
#define	op_dst_io	op_dst.u.io
#define	op_dst_map	op_dst.map
#define	op_dst_nsegs	op_dst.nsegs
#define	op_dst_segs	op_dst.segs
#define	op_dst_mapsize	op_dst.mapsize

#define DDT_ENTRY_SIZE (sizeof(ddt_entry)*MAX_DDT_ENTRIES)

#define	MAX_IOP_SIZE	64	/* words */
#define	MAX_OOP_SIZE	128

#define KRP_PARAM_BASE 0
#define KRP_PARAM_EXP  1
#define KRP_PARAM_MOD  2
#define KRP_PARAM_RES  3
struct pka_pkq {
	struct list_head	pkq_list;
	struct cryptkop		*pkq_krp;

	U32	pkq_M[MAX_IOP_SIZE];
	U32	pkq_e[MAX_IOP_SIZE];
	U32	pkq_N[MAX_IOP_SIZE];
	U32	pkq_M_len;
	U32	pkq_e_len;
	U32	pkq_N_len;
	U32	*pkq_obuf;
	U32	pkq_result_len;	
};

#define DDT_ENTRY_SIZE (sizeof(ddt_entry)*MAX_DDT_ENTRIES)

// This is the main structure to define a complete
// SPAcc hardware and provide a uniform acces to all
// hardware modules and global control registers
typedef struct elp_spacc_
{
  U32 *aux_info, *ctrl;                    // Global control register
  U32 *stat_pop;                // Writing any value to this register causes a 
  volatile U32 *status;         // sw tag and result to be popped off of the status FIFO.
  volatile U32 *fifo_stat;
  U32 *key_sz;                  //Set the size of the key in bytes + context + type

  U32 *ciph_key;                // Memory context to store cipher keys
  U32 *hash_key;                // Memory context to store hash keys
  U32 *rc4_key;                 // Memory context to store internal RC4 keys

  U32 *irq_stat;                // To Read and Clear Individual status interrupts
  U32 *irq_enable;              // To Enable individual interruipts
  U32 *irq_ctrl;                // Configures the command FIFO count to cause a CMD interrupt to be generated.
  U32 *sdma_burst_sz;           // Maximum size of DMA burst over the master bus (in words).
  U32 *src_ptr;                 // Pointer to source packet DDT structure
  U32 *dst_ptr;                 // Pointer to destination packet DDT structure
  U32 *aad_offset;              // src/dst of AAD offset
  U32 *pre_aad_len;
  U32 *post_aad_len;
  U32 *iv_offset;
  U32 *proc_len;                // packet length for processing
  U32 *icv_len;                 // length of ICV
  U32 *icv_offset;              // offset of ICV
  volatile U32 *sw_tag;                  // Software tag of packet ID.  Will be returned incremented with the packet in the outbound status FIFO.
  volatile U32 *vspacc_rqst;             // Virtual spacc request
  volatile U32 *vspacc_alloc;            // Allocate a spacc
  volatile U32 *vspacc_prio;             // priority
  volatile U32 *vspacc_rc4_key_req;      // request RC4 context access
  volatile U32 *vspacc_rc4_key_gnt;      // granted RC4 access
  
  volatile U32 *spacc_id;

  U32 regmap;                   // store virtual/physical system memory map start

} elp_spacc_regmap;

//typedef elp_spacc_regmap elp_spacc;

typedef struct elp_trng_
{
  U32 cookie;                   // magic cookie
  int active;
  U32 *rand;                    // read:  128 bit random number
  U32 *seed;                    // write: 128 bit seed value
  U32 *ctrl;
  U32 *status;
  U32 *irq_stat;                // To Read and Clear TRNG status interrupts
  U32 *irq_en;
} elp_trng;

#define TRNG_DATA_SIZE_WORDS	4
#define TRNG_DATA_SIZE_BYTES	16
#define TRNG_NONCE_SIZE_WORDS	8
#define TRNG_NONCE_SIZE_BYTES	32

typedef struct elp_pka_
{
  int active;
  U32 *ctrl;
  U32 *entry;
  U32 *addr;
  U32 *opcode;
  U32 *stack;
  U32 *conf_reg;
  U32 *status;
  U32 *flags;
  U32 *watchdog;
  U32 *cycle_since_go;
  U32 *inst_since_go;
  U32 *index_i;
  U32 *index_j;
  U32 *index_k;
  U32 *index_l;
  U32 *int_en;	
  U32 *jump_prob;	
  U32 *lfsr_seed;	
} elp_pka;

typedef int elp_mutex;

struct elp_spacc_device {
   elp_spacc_regmap     reg;
   elp_spacc_handle_ctx ctx[CRYPTO_CONTEXTS_MAX];
   elp_mutex  context_lock;

   int        _module_initialized;
   S32        _pd_verbose;
   
   int job_pool[1U<<SPACC_SW_ID_ID_W][2];
};

//extern struct elp_spacc_device _spacc_dev;

void elp_register_ocf(struct elp_softc *sc);

elp_spacc_handle_ctx *context_lookup (S32 handle);
S32 spacc_init (U32 opmodules, U32 regmap);
void spacc_fini(void);
void spacc_set_debug (S32 dflag);
U8 *spacc_error_msg (S32 err);
S32 spacc_open (S32 enc, S32 hash,  S32 ctxid, S32 ctxid_skip, UINT rc4len, UINT first_packet);
S32 spacc_close (S32 handle, struct elp_softc *sc);
S32 spacc_release_ddt(S32 handle, struct elp_softc *sc);
S32 spacc_add_ddt (S32 handle, U8 *data, U32 len, struct elp_softc *sc);
S32 spacc_release_dst_ddt(S32 handle, struct elp_softc *sc);
S32 spacc_add_dst_ddt (S32 handle, U8 *data, U32 len, struct elp_softc *sc, bool same_buff);
S32 spacc_set_context (S32 handle, S32 op, U8 * key, S32 ksz, U8 * iv, S32 ivsz);
S32 spacc_get_context (S32 handle, U8 * iv, S32 ivsize, S32 * psize);
S32 spacc_set_operation (S32 handle, S32 op, U32 prot, U32 icvpos, U32 icvoff, U32 icvsz);
S32 spacc_set_auxinfo(S32 handle, U32 direction, U32 bitsize);
S32 spacc_packet_enqueue_ddt (S32 handle, U32 proc_sz, U32 aad_offset, U32 pre_aad_sz, U32 post_aad_sz, U32 iv_offset, U32 prio);
S32 spacc_packet_enqueue (S32 handle, U8 * in_buf, U32 in_sz, U8 * out_buf, U32 out_sz, U32 pre_aad_sz, U32 post_aad_sz, U32 iv_offset, U32 prio);
S32 spacc_packet_dequeue (S32 handle, int demap);
S32 spacc_status (S32 handle, U8 * buf, S32 * size);
U32 spacc_get_version (void);

void spacc_virtual_set_weight(S32 weight);
S32 spacc_virtual_request_rc4(void);

S32 crypto_is_debug (S32 dflag);

void spacc_pdu_enable_int (void);
void spacc_pdu_disable_int (void);

void spacc_enable_int (void);
void spacc_disable_int (void);
int spacc_fifo_stat (void);
int spacc_fifo_stat_cmd(void);

int spacc_pci_probe (struct pci_dev *dev, const struct pci_device_id *id);
int spacc_probe (void);

void spacc_remove (void);

int trng_init (U32 mmap);
int trng_wait_done (void);
int trng_get_rand (U8 * randbuf, int size);
int trng_read_random(void *arg, u_int32_t *buf, int maxwords);
void trng_enable_int (void);
void trng_disable_int (void);
void trng_dump_registers (void);

int pka_init (U32 mmap);
void pka_enable_int (void);
void pka_disable_int (void);
void pka_dump_registers (void);
int pka_copy_kparam(struct crparam *p, U32 *buf, U32 alen);
int pka_kstart(struct elp_softc *sc);
void pka_kfeed(struct elp_softc *sc);
void pka_halt_engine(void);
int clue_start_engine (int size);
int clue_page_size (int size);
unsigned clue_base_and_partial_radix (int size);

S16 clue_ec_load_curve_data (U16 curve);
S16 clue_ec_load_curve_padd (U16 curve);
S16 clue_ec_point_add (U8 * x1, U8 * y1, U8 * x2, U8 * y2, U8 * rx, U8 * ry, U16 size);
S16 clue_ec_load_curve_pmult (U16 curve, S16 loadbase);
S16 clue_ec_load_curve_pdbl (U16 curve);
S16 clue_ec_point_double (U8 * x, U8 * y, U8 * rx, U8 * ry, U16 size);
S16 clue_ec_load_curve_pver (U16 curve);
S16 clue_ec_point_verify (U8 * x, U8 * y, U16 size);
S16 clue_ec_point_mult_base (U8 * k, U8 * rx, U8 * ry, U16 size, U16 ksize);

//RSA maximum 4096 bit= 512 bytes
#define BN_RSA_MAX_RADIX_SIZE  512
#define BN_RSA_BASE_RADIX_SIZE  256
void clue_bn_init (U32 mmap);
void clue_ec_init (U32 mmap);
int clue_bn_modexp_crt (U8 * c_a, U8 * c_e, U8 * c_p, U8 * c_q, U8 * c_c, int size, int precomp);
int clue_bn_precompute (U8 * n, int size);
int clue_bn_load_precompute (U8 * mp,U8 * rr, int size);
int clue_bn_modexp (U8 * a, U8 * e, U8 * m, U8 * c, int size, int precomp);
int clue_bn_modmult (U8 * a, U8 * b, U8 * m, U8 * c, int size, int precomp);
int clue_bn_moddiv (U8 * a, U8 * b, U8 * m, U8 * c, int size);
int clue_bn_modinv (U8 * b, U8 * m, U8 * c, int size);
int clue_bn_modadd (U8 * a, U8 * b, U8 * m, U8 * c, int size);
int clue_bn_modsub (U8 * a, U8 * b, U8 * m, U8 * c, int size);
int clue_bn_modred (U8 * a, U8 * m, U8 * c, int size);
void clue_check_register (void);
void clue_bn_out_lastregister (void);
void clue_check_data_memory (void);
void clue_dumpmemreg (U8 *mem, int size, U8 *msg, char r);
int clue_load_microcode (U32 * code, int size);
int clue_get_microcode (U32 * code, int size);
void dumpword(U32 *from, int size, char *msg);
void    memcpy32htonl(U32 *dst, U32 *src, int len);
void    memcpy32(volatile U32 *dst, volatile U32 *src, int len);
void    memcpy32htonl_r(U32 *dst, U32 *src, int len);
void    memcpy32_e(U32 *dst, U32 *src, int len);
void    memcpy32_re(U32 *dst, U32 *src, int len);
void    memcpy32_r(U32 *dst, U32 *src, int len);

#define MEMCPY32_E(d,s,z)	memcpy32_e((U32 *)(d),(U32 *)(s),(z))
#define MEMCPY32_RE(d,s,z)	memcpy32_re((U32 *)(d),(U32 *)(s),(z))
#define MEMCPY32(d,s,z)		memcpy32((U32 *)(d),(U32 *)(s),(S16)(z))
#define MEMCPY32_R(d,s,z)	memcpy32_r((U32 *)(d),(U32 *)(s),(z))
#define MEMCPY32HTONL(d,s,z)	memcpy32htonl((U32 *)(d),(U32 *)(s),(S16)(z))
#define MEMCPY32HTONL_R(d,s,z)	memcpy32htonl_r((U32 *)(d),(U32 *)(s),(z))
#define MEMCPY32EX(d,s,z)	memcpy32htonl((U32 *)(d),(U32 *)(s),(S16)(z))
#define MEMCPY32EX_R(d,s,z)	memcpy32htonl_r((U32 *)(d),(U32 *)(s),(z))

#ifndef MAX_BUFFERS
#define MAX_BUFFERS 16
#endif

typedef struct spacc_dev_
{
  struct cdev cdev;
  int handle;
  int alg_op;                   /* Encrypt/DEcrypt */

  void *buffers[MAX_BUFFERS];
  unsigned long buflen[MAX_BUFFERS], read_idx, read_ofs;
  int no_buffers;
  int mode;

} spacc_dev;

S16 crypto_rand_mode (U16 mode);
S16 crypto_srand (U8 * seed, S16 size);
S16 crypto_rand (U8 * rbuf, S16 size);

//typedef struct spacc_dev_;
typedef struct spacc_ent_
{
	struct list_head  entry;
	struct spacc_dev_ *dev; /* device information */
	char *buf;
	int  *len;

} spacc_ent;

typedef struct spacc_if_ {
	int version;
	void (*ready)(spacc_ent *ent);
	void (*put_out)(spacc_ent *ent);
	void (*get_in)(spacc_ent *ent);
} spacc_if;
#define xregister_device(name) register_device(name)

#define register_device(name) \
  int spacc_register_device_if_ ## name (spacc_if *dif)

#define xexport_device(name) export_device(name)
#define export_device(name) \
  EXPORT_SYMBOL (spacc_register_device_if_ ## name)

#define KERNSTR(name) \
  #name

#define XKERNSTR(name) \
  KERNSTR(name)

#ifdef PROCNAME
  xregister_device(PROCNAME);
#else
int spacc_register_device_if (spacc_if *dif);
#endif

#define spacc_ack_int() 	ELP_WRITE_UINT(_spacc_dev.reg.irq_stat, ELP_READ_UINT(_spacc_dev.reg.irq_stat) | SPACC_IRQ_STAT_STAT | SPACC_IRQ_STAT_CMD)

#endif
