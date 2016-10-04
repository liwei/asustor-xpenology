/*-----------------------------------------------------------------------
//
// Proprietary Information of Elliptic Semiconductor
// Copyright (C) 2002, all rights reserved
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
// Filename:         $Source: 
// Current Revision: $Revision: 1. $
// Last Updated:     $Date: 2012/06/22 014:30:56 $
// Current Tag:      $Name:  $
//
//
//-----------------------------------------------------------------------*/

#ifndef _ELPVAR_H_
#define _ELPVAR_H_

#include <linux/types.h>	/* size_t */

#include <asm/io.h>
#include "m86xxx_utils.h"
#if defined(CONFIG_SYNO_COMCERTO)
#include "../ocf-compat.h"
#else
#include "ocf-compat.h"
#endif

#define DRV_NAME "Comcerto 2000 ELP Crypto Offload Engine " 

/* Maximum queue length */
/*
#ifndef M821_MAX_NQUEUE
#define M821_MAX_NQUEUE	60
#endif
*/

#define	ELP_MAX_PART		8	/* Maximum scatter/gather depth */
#define ELP_MAX_ARAM_SA		8
#define ELP_PE_RING_SIZE	64
#define ELP_MAX_ESPAH_OVERHEAD	8+16+18+12 // ESP + IV + max pad + 2 + 12

#define	ELP_SESSION(sid)	( (sid) & 0x0fffffff)
#define	ELP_SID(id, sesn)	(((id+1) << 28) | ((sesn) & 0x0fffffff))

#define ELP_SESMODE_CRYPTO_CIPHER	0x00000001
#define ELP_SESMODE_CRYPTO_HASH		0x00000002
#define ELP_SESMODE_CRYPTO_HMAC		0x00000004
#define ELP_SESMODE_CRYPTO_MASK		0x00000007
#define ELP_SESMODE_ENCRYPT	0x00000010
#define ELP_SESMODE_EBC		0x00000020
#define ELP_SESMODE_CBC		0x00000040
#define ELP_SESMODE_CTR		0x00000060

#define ELP_SESDIR_INBOUND	0
#define	ELP_SESDIR_OUTBOUND	1

struct elp_session {
	u_int32_t	ses_used;
	u_int32_t	ses_new;
	u_int32_t	ses_cipher_klen;	/* key length in bits */
	u_int32_t	ses_cipher_key[8];	/* DES/3DES/AES key */
	u_int32_t	ses_iv_size;
	u_int32_t	ses_iv[4];
	u_int32_t	ses_hmac_klen;
	u_int32_t	ses_hmac_key[8];
	u_int32_t	ses_hmac_res[8];

	// raw crypto parameters
	u_int32_t	ses_mode;		// CBC ECB CTR CIPHER HMAC
	u_int32_t	ses_auth_alg;
	u_int32_t	ses_cipher_alg;
	u_int32_t	ses_packet_alg;
	u_int32_t	ses_alg_flags;
	// Packet engine session parameters
	u_int32_t	ses_direction;
	dma_addr_t 	ses_sa_dma;
	void*		ses_sa_vma;
	u_int32_t 	ses_sa_in_aram;
	u_int32_t 	ses_sa_in_aram_index;
	u_int32_t 	ses_spacc_handle;
	u_int32_t 	ses_first_packet;
};

/*
 * Cryptographic operand state.  One of these exists for each
 * source and destination operand passed in from the crypto
 * subsystem.  When possible source and destination operands
 * refer to the same memory.  More often they are distinct.
 * We track the virtual address of each operand as well as
 * where each is mapped for DMA.
 */
struct elp_operand {
	union {
		struct sk_buff *skb;
		struct uio *io;
	} u;
	void		*map;
	int		 mapsize;	/* total number of bytes in segs */
	struct {
		dma_addr_t	ds_addr;
		int		ds_len;
		int		ds_tlen;
	} segs[ELP_MAX_PART];
	int			nsegs;
};

/*
 * Scatter/Gather data descriptor table.
 */
struct elp_ddtdesc {
	volatile u_int32_t	frag_addr;		
	volatile u_int32_t	frag_size;		
};

struct elp_desc {
	u_int32_t			d_src_ddt_addr;
	u_int32_t			d_dst_ddt_addr;
	u_int32_t			d_src_ddt_size;		 /* number of fragments (2 if flat) */
	u_int32_t			d_dst_ddt_size;		 /* number of fragments (2 if flat) */
	u_int32_t			d_sa_addr;
	u_int32_t			d_status;
};

struct elp_ringentry {	
	struct elp_desc		re_desc;		/* command descriptor */
	struct cryptop		*re_crp;		/* crypto operation */

	struct elp_operand	re_src;		/* source operand */
	struct elp_operand	re_dst;		/* destination operand */

	struct elp_session	*re_ses;	/* crypto session  */
	int			re_flags;	
};

#define	re_src_skb	re_src.u.skb
#define	re_src_io	re_src.u.io
#define	re_src_map	re_src.map
#define	re_src_nsegs	re_src.nsegs
#define	re_src_segs	re_src.segs
#define	re_src_mapsize	re_src.mapsize

#define	re_dst_skb	re_dst.u.skb
#define	re_dst_io	re_dst.u.io
#define	re_dst_map	re_dst.map
#define	re_dst_nsegs	re_dst.nsegs
#define	re_dst_segs	re_dst.segs
#define	re_dst_mapsize	re_dst.mapsize

struct elp_packet_engine{

	spinlock_t		 pe_ringmtx;		/* Packet Engine ring lock */
	int			 pe_flags;
	struct elp_ringentry	*pe_ring;		/* PE ring */
	struct elp_ringentry	*pe_ringtop;		/* PE ring top */
	struct elp_ringentry	*pe_front;		/* next free entry */
	struct elp_ringentry	*pe_back;		/* next pending entry */

	dma_addr_t		 pe_src_ddt_dma;	/* DDT area (ARAM) */
	void			*pe_src_ddt_vma;
	int			 pe_src_ddtavail;	
	struct elp_ddtdesc	*pe_src_ddtring;		/* source DDT ring */
	struct elp_ddtdesc	*pe_src_ddtringtop;
	struct elp_ddtdesc	*pe_src_ddtfree;
	struct elp_ddtdesc	*pe_src_ddtbusy;

	dma_addr_t		 pe_dst_ddt_dma;	/* DDT area (ARAM) */
	void			*pe_dst_ddt_vma;	
	int			 pe_dst_ddtavail;
	struct elp_ddtdesc	*pe_dst_ddtring;	/* Inbound destination DDT ring */
	struct elp_ddtdesc	*pe_dst_ddtringtop;
	struct elp_ddtdesc	*pe_dst_ddtfree;
	struct elp_ddtdesc	*pe_dst_ddtbusy;

	int			 pe_needwakeup;	/* notify crypto layer */
	int			 pe_nqchip;		/* # passed to chip */

};

struct elp_aram_sa {
	int		sa_used;
	dma_addr_t	sa_aram_dma;		/* SADB address in internal SRAM (ARAM)*/
	void		*sa_aram_vma;
};	

#define PE_INBOUND	0
#define PE_OUTBOUND	1

enum {
  Below_512_size = 0,
  Req_512_size,
  Req_1024_size,
  Req_2048_size,
  Req_4096_size,
  Above_4k_size
};

struct elp_stats {
	u32	open_sessions;
	u32	close_sessions;
	u32	nr_hash_req;
	u32	nr_enc_req;
	u32	crypto_len_counters[32];
};

struct elp_softc {
	struct	ocf_device	_device;

	struct platform_device  *sc_dev;

	ocf_iomem_t		sc_base_addr;
	u32			sc_aram_base;
	u32			sc_irq;
	u32			sc_irq2;
	u32			sc_cid;			/* crypto tag */

	struct elp_packet_engine sc_pe[2];
	struct elp_aram_sa	 sc_aram_sa[ELP_MAX_ARAM_SA];	/* SA located in ARAM */
	struct dma_pool		*sc_sa_ddr_dma;		/* additional SA in external DDR */
	int			sc_flags;		/* device specific flags */
	int			sc_nsessions;	/* # of sessions */
	struct elp_session	*sc_sessions;	/* sessions (can be Packet Engine or raw crypto module)*/
	struct cryptop		*crp;		/* raw crypto request descriptor from OCF */
	struct cryptkop 	*krp;		/* key genration request descriptor from OCF */
	struct list_head	sc_pkq;		/* queue of PKA requests */
	struct pka_pkq		*sc_pkq_cur;	/* PKA request in use */
	int			sc_needwakeup;	/* notify crypto layer */

	struct tasklet_struct	irq_in_tasklet;
	struct tasklet_struct 	irq_out_tasklet;

	spinlock_t		 reg_lock;
	struct elp_stats	stats;
};

#endif
