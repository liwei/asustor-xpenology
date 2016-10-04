/* ---------------------------------------------------------------------------
 *
 * An OCF-Linux module that uses Mindspeed Comcerto 2000  HW security Engine to do the crypto.
 * Based on crypto/ocf/hifn and crypto/ocf/safe OCF drivers
 *
 * Copyright (c) 2012 Mindspeed Technologies, Inc.
 *
 * This code written by Mindspeed Technologies
 * some code copied from files with the following:
 * Copyright (C) 2004 David McCullough <davidm@snapgear.com>
 *  Kim A. B. Phillips <kim.phillips@freescale.com>
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ---------------------------------------------------------------------------
 *
 * NOTES:
 *
 */
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
#include <linux/skbuff.h>
#include <asm/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/uio.h>
#include <linux/dmapool.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <mach/reset.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
#include <linux/platform_device.h>
#endif

#include <asm/io.h>

#include "m86xxx_spacc.h"
#include "m86xxx_spacchw.h"
#include "m86xxx_var.h"
#include "cryptodev.h"
#include "uio.h"

#ifndef SPACC_MEMORY_BASE_OFFSET
#define SPACC_MEMORY_BASE_OFFSET 0x00000
#endif

#define read_random(p,l)	get_random_bytes(p,l)
#define bcopy(s,d,l)		memcpy(d,s,l)

struct elp_softc *g_elp_sc;

void clue_ec_init (U32 mmap);

static int comcerto_elp_probe(struct platform_device *pdev);
static int comcerto_elp_remove(struct platform_device *pdev);

static	int elp_process(device_t, struct cryptop *, int);
static	int elp_newsession(device_t, u_int32_t *, struct cryptoini *);
static	int elp_freesession(device_t, u_int64_t);
static int elp_kprocess(device_t dev, struct cryptkop *krp, int hint);

static device_method_t elp_methods = {
	/* crypto device methods */
	DEVMETHOD(cryptodev_newsession,	elp_newsession),
	DEVMETHOD(cryptodev_freesession,elp_freesession),
	DEVMETHOD(cryptodev_process,	elp_process),
#if defined(CONFIG_SYNO_COMCERTO)
	DEVMETHOD(cryptodev_kprocess,	NULL),
#else
	DEVMETHOD(cryptodev_kprocess,	elp_kprocess),
#endif
};

/* HW module structures */
struct elp_spacc_device _spacc_dev;

static elp_trng _trngm;
elp_pka _pka;

static S32 alloc_handle (S32 ctx, S32 ctx_skip);
static S32 free_handle (S32 handle);
static int _module_initialized;
//spinlock_t elp_context_spinlock =SPIN_LOCK_UNLOCKED;
DEFINE_SPINLOCK(elp_context_spinlock);
DEFINE_SPINLOCK(elp_ddt_lock);

struct tasklet_struct 	irq_spacc_tasklet;
struct tasklet_struct 	irq_pka_tasklet;
spinlock_t	reg_lock;

static LIST_HEAD(pka_pkq); /* current PKA wait list */
static spinlock_t pka_pkq_lock;

elp_id tid;
elphw_if tif;

/* IPSEC spacc AXI Clock */
static struct clk *spacc_clk;

/* Module arguments */
int elp_debug = 0x1;
//int elp_debug = 0x1 | 0x2 | 0x4 | 0x8;
module_param(elp_debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debug");

static int little_endian = 1;
module_param(little_endian, int, 0644);
MODULE_PARM_DESC(little_endian, "Switch clp into BIG Endian Mode");

static int irq_mode = 1;
module_param(irq_mode, int, 0644); // insmod elp1800 irq_mode=1
MODULE_PARM_DESC(irq_mode, "Enable IRQ mode");

//Enable ELP_TRNG_SELF_TEST to check TRNG module is working
//#define ELP_TRNG_SELF_TEST 1

//Enable ELP_PKA_SELF_TEST to check PKA module
//#define ELP_PKA_SELF_TEST 1

#ifdef ELP_PKA_SELF_TEST
//#define PKA_ECC_TEST_POINT_ADD
//#define PKA_ECC_TEST_POINT_DBL
//#define PKA_ECC_TEST_POINT_PVER
//#define PKA_ECC_TEST_POINT_MUL
#define PKA_TEST_MOD_EXP
#define PKA_TEST_MOD_MULT
#define PKA_TEST_MOD_DIV
#define PKA_TEST_MOD_ADD
#define PKA_TEST_MOD_SUB
#define PKA_TEST_MOD_INV
#endif

// Enable ELP_TRNG_IRQ_MODE to switch TRNG module from polling to IRQ mode 
//#define ELP_TRNG_IRQ_MODE 1

extern char *pka_errmsgs[];

#if defined(CONFIG_SYNO_COMCERTO)
static DEFINE_SPINLOCK(syno_ocf_lock);
#endif

/************************* OCF Higher level API ****************************************/

/*
 * Generate a new software session.
 */
/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
static int elp_newsession(device_t arg, u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *c, *encini = NULL, *macini = NULL;
	struct elp_softc *sc = arg->softc;
	struct elp_session *ses = NULL;
	int sesn = 0;
	int ret = EINVAL;
#if defined(CONFIG_SYNO_COMCERTO)
	unsigned long flags = 0;
#endif

	if (sidp == NULL || cri == NULL || sc == NULL) {
		DPRINTF(ELP_ERR, "wrong session parameters\n");
		goto err;
	}

#if defined(CONFIG_SYNO_COMCERTO)
	spin_lock_irqsave(&syno_ocf_lock, flags);
#endif
	for (c = cri; c != NULL; c = c->cri_next) {
		if (c->cri_alg == CRYPTO_MD5 ||
				c->cri_alg == CRYPTO_MD5_HMAC ||
				c->cri_alg == CRYPTO_SHA1 ||
				c->cri_alg == CRYPTO_SHA1_HMAC ||
				c->cri_alg == CRYPTO_SHA2_256_HMAC ||
				c->cri_alg == CRYPTO_NULL_HMAC) {
			if (macini) {
				DPRINTF(ELP_WRN, "aalg already\n");
				goto err;
			}
			macini = c;
		} else if (c->cri_alg == CRYPTO_DES_CBC ||
				c->cri_alg == CRYPTO_3DES_CBC ||
				c->cri_alg == CRYPTO_AES_CBC ||
				c->cri_alg == CRYPTO_ARC4 ||
				c->cri_alg == CRYPTO_NULL_CBC) {
			if (encini) {
				DPRINTF(ELP_WRN, "ealg already\n");
					goto err;
				}
				encini = c;
			} else {
				DPRINTF(ELP_ERR, "alg %d not suported\n", c->cri_alg);
				goto err;
			}
	}
	if (encini == NULL && macini == NULL) {
		DPRINTF(ELP_ERR, "both encini and macini are NULL\n");
		goto err;
	}

	if (encini) {			/* validate key length */
		switch (encini->cri_alg) {
			case CRYPTO_DES_CBC:
				if (encini->cri_klen != 64)
					goto err1;
				break;

			case CRYPTO_3DES_CBC:
				if (encini->cri_klen != 192)
					goto err1;
				break;

			case CRYPTO_AES_CBC:
				if (encini->cri_klen != 128 &&
						encini->cri_klen != 192 &&
						encini->cri_klen != 256)
					goto err1;
				break;

			case CRYPTO_ARC4:
					if (encini->cri_klen != 128 &&
					    encini->cri_klen != 40)
						goto err1;
					break;

			default:
					DPRINTF(ELP_ERR, "encini->cri_alg %d not supported\n", encini->cri_alg);
					goto err1;
		}
	}

	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct elp_session *) kmalloc(sizeof(struct elp_session), GFP_ATOMIC);
		if (ses == NULL) {
			DPRINTF(ELP_ERR, "Failed to allocate memory for sc_session\n");
			ret = ENOMEM;
			goto err;
		}
		memset(ses, 0, sizeof(struct elp_session));
		sesn = 0;
		sc->sc_nsessions = 1;
	} else {
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
			if (sc->sc_sessions[sesn].ses_used == 0) {
				ses = &sc->sc_sessions[sesn];
				break;
			}
		}

		if (ses == NULL) {
			sesn = sc->sc_nsessions;
			DPRINTF(ELP_DBG, "Re allocating memory for sessions %d\n", sesn + 1);
			ses = (struct elp_session *)
				kmalloc((sesn + 1) * sizeof(struct elp_session), GFP_ATOMIC);
			if (ses == NULL) {
				DPRINTF(ELP_ERR, "Failed to allocate memory for sc_session\n");
				ret = ENOMEM;
				goto err;
			}
			memset(ses, 0, (sesn + 1) * sizeof(struct elp_session));
			bcopy(sc->sc_sessions, ses, sesn * sizeof(struct elp_session));
			bzero(sc->sc_sessions, sesn * sizeof(struct elp_session));
			kfree(sc->sc_sessions);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sesn];
			sc->sc_nsessions++;
		}
	}

	bzero(ses, sizeof(struct elp_session));
	ses->ses_used = 1;

	if (encini) {
		if (encini->cri_alg  == CRYPTO_AES_CBC) {
			DPRINTF(ELP_DBG, "CRYPTO_AES_CBC\n");
			if (encini->cri_klen == 128)
				ses->ses_cipher_alg = ELP_CIPHER_AES128;
			else if (encini->cri_klen == 192)
				ses->ses_cipher_alg = ELP_CIPHER_AES192;
			else if (encini->cri_klen == 256)
				ses->ses_cipher_alg = ELP_CIPHER_AES256;
			ses->ses_iv_size = 16;
		}
		else if (encini->cri_alg  == CRYPTO_DES_CBC) {
			DPRINTF(ELP_DBG, "CRYPTO_DES_CBC\n");
			ses->ses_cipher_alg = ELP_CIPHER_DES;
			ses->ses_iv_size = 8;
		}
		else if (encini->cri_alg  == CRYPTO_3DES_CBC) {
			DPRINTF(ELP_DBG, "CRYPTO_3DES_CBC\n");
			ses->ses_cipher_alg = ELP_CIPHER_3DES;
			ses->ses_iv_size = 8;
		}
		else if (encini->cri_alg  == CRYPTO_ARC4) {
			DPRINTF(ELP_DBG, "CRYPTO_ARC4\n");
			if (encini->cri_klen == 40)
				ses->ses_cipher_alg = ELP_CIPHER_ARC4_40;
			else if (encini->cri_klen == 128)
				ses->ses_cipher_alg = ELP_CIPHER_ARC4_128;
			ses->ses_iv_size = 8;
			ses->ses_first_packet=1;
		}
		else {
			DPRINTF(ELP_ERR, "encini->cri_alg %d not supported\n", encini->cri_alg);
			goto err;
		}

		ses->ses_mode |=  ELP_SESMODE_CRYPTO_CIPHER  | ELP_SESMODE_CBC;
		if (encini->cri_flags & CRYPTO_ENCRYPT)
			ses->ses_mode |= ELP_SESMODE_ENCRYPT;
		/* get an IV */
		if(_trngm.active == 1) {
			if(trng_get_rand((U8*)ses->ses_iv, ses->ses_iv_size) == SPACC_CRYPTO_FAILED) {
				DPRINTF(ELP_ERR, "%d\n",__LINE__);
				goto err;
			}
		} else {
			read_random(ses->ses_iv, ses->ses_iv_size);
		}

		ses->ses_cipher_klen = (encini->cri_klen + 7) / 8;
		if (encini->cri_key != NULL)
			bcopy(encini->cri_key, ses->ses_cipher_key, ses->ses_cipher_klen);
	}
	if (macini) {
		if (macini->cri_alg == CRYPTO_MD5) {
			DPRINTF(ELP_DBG, "CRYPTO_MD5\n");
		}
		else if (macini->cri_alg == CRYPTO_MD5_HMAC) {
			DPRINTF(ELP_DBG, "CRYPTO_MD5_HMAC\n");
			ses->ses_auth_alg =  ELP_HMAC_MD5;
		}
		else if (macini->cri_alg == CRYPTO_SHA1) {
			DPRINTF(ELP_DBG, "CRYPTO_SHA1\n");
		}
		else if (macini->cri_alg == CRYPTO_SHA1_HMAC) {
			DPRINTF(ELP_DBG, "CRYPTO_SHA1_HMAC\n");
			ses->ses_auth_alg =  ELP_HMAC_SHA1;
		}
		else if (macini->cri_alg == CRYPTO_SHA2_256_HMAC) {
			DPRINTF(ELP_DBG, "CRYPTO_SHA2_256_HMAC\n");
			ses->ses_auth_alg =  ELP_HMAC_SHA2;
		}
		else{
			DPRINTF(ELP_ERR, "macini->cri_alg %d not supported\n", macini->cri_alg);
			goto err;
		}

		ses->ses_mode |=  ELP_SESMODE_CRYPTO_HMAC ;
		ses->ses_hmac_klen = (macini->cri_klen + 7) / 8;
		if(macini->cri_key != NULL)
			bcopy(macini->cri_key, ses->ses_hmac_key, ses->ses_hmac_klen);

	}
	*sidp = ELP_SID(sc->sc_cid, sesn);
	sc->stats.open_sessions++;

#if defined(CONFIG_SYNO_COMCERTO)
	spin_unlock_irqrestore(&syno_ocf_lock, flags);
#endif
	return (0);
err1:
#if defined(CONFIG_SYNO_COMCERTO)
	spin_unlock_irqrestore(&syno_ocf_lock, flags);
#endif
	DPRINTF(ELP_ERR, "%s: wrong key len\n", __FUNCTION__);
	return ret;
err:
#if defined(CONFIG_SYNO_COMCERTO)
	spin_unlock_irqrestore(&syno_ocf_lock, flags);
#endif
	return ret;
}

/*
 * Deallocate a session.
 */
static int elp_freesession(device_t dev, u_int64_t tid)
{
	struct elp_softc *sc = device_get_softc(dev);
	int session, ret = EINVAL;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;
	struct elp_session *ses = NULL;
#if defined(CONFIG_SYNO_COMCERTO)
	unsigned long flags = 0;
#endif

	if (sc == NULL) {
		DPRINTF(ELP_ERR, "Invalid software context\n");
		return ret;
	}

#if defined(CONFIG_SYNO_COMCERTO)
	spin_lock_irqsave(&syno_ocf_lock, flags);
#endif
	session = ELP_SESSION(sid);
	if (session < sc->sc_nsessions) {
		ses = &sc->sc_sessions[session];

		/*
		   if((ret = spacc_close (sc->sc_sessions[session].ses_spacc_handle)) < 0) {
		   DPRINTF ("warning: %s\n", spacc_error_msg (ret));
		   }
		 */
		bzero(&sc->sc_sessions[session], sizeof(sc->sc_sessions[session]));
		ret = 0;
		sc->stats.close_sessions++;
	} else
		DPRINTF(ELP_ERR, "Invalid session to free %d\n", session);

#if defined(CONFIG_SYNO_COMCERTO)
	spin_unlock_irqrestore(&syno_ocf_lock, flags);
#endif
	return (ret);
}

static int elp_process(device_t dev, struct cryptop *crp, int hint)
{
	struct elp_softc *sc;
	struct cryptodesc *crd1, *crd2, *maccrd = NULL, *enccrd = NULL;
	struct elp_session *ses = NULL;
	elp_spacc_handle_ctx *ctx = NULL;
	int err = EINVAL;
	int handle = 0;
	//int i;
	struct iovec *iov = NULL;
	struct uio *uio = NULL;
	//unsigned char mackey[24] = {0};
	S32 ctxid = -1;
	S32 ctxid_skip = -1;
	bool same_buff = 0;

	spin_lock_bh(&elp_ddt_lock); /*This lock is to protect the sessions on SMP, if this lock is not used then data corruption is observed*/
	sc = device_get_softc(dev);
	if (crp == NULL || crp->crp_callback == NULL || sc == NULL ||
			(ELP_SESSION(crp->crp_sid) >= sc->sc_nsessions)) {
		DPRINTF(ELP_ERR, "Invalid session or request \n");
		crp->crp_etype = err;
		crypto_done(crp);
		spin_unlock_bh(&elp_ddt_lock);
		return (err);
	}

	ses = &sc->sc_sessions[ELP_SESSION(crp->crp_sid)];
	DPRINTF(ELP_DBG, "session %d\n", (int)crp->crp_sid);

	//non packet session (Raw crypto mode)
	if (crp->crp_desc == NULL || crp->crp_buf == NULL) {
		DPRINTF(ELP_ERR, "Invalid request \n");
		crp->crp_etype = err;
		crypto_done(crp);
		spin_unlock_bh(&elp_ddt_lock);
		return (err);
	}
	crd1 = crp->crp_desc;
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		switch (crd1->crd_alg) {
			case CRYPTO_MD5_HMAC:
			case CRYPTO_SHA1_HMAC:
			case CRYPTO_SHA1:
			case CRYPTO_MD5:
				DPRINTF(ELP_DBG, " crd1 is maccrd\n");
				ses->ses_direction = ELP_SESDIR_OUTBOUND;
				maccrd = crd1;
				break;
			case CRYPTO_DES_CBC:
			case CRYPTO_3DES_CBC:
			case CRYPTO_AES_CBC:
			case CRYPTO_ARC4:
				DPRINTF(ELP_DBG, " crd1 is enccrd\n");
				if (crd1->crd_flags & CRD_F_ENCRYPT)
					ses->ses_direction = ELP_SESDIR_OUTBOUND;
				else
					ses->ses_direction = ELP_SESDIR_INBOUND;
				enccrd = crd1;
				break;
			default:
				DPRINTF(ELP_ERR, " Crypt algo %x not supportedL\n", crd1->crd_alg);
				err = EINVAL;
				crp->crp_etype = err;
				crypto_done(crp);
				spin_unlock_bh(&elp_ddt_lock);
				return (err);
		}
	} else {
		DPRINTF(ELP_ERR, "Request for enc+hash\n");
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC ||
                     crd1->crd_alg == CRYPTO_SHA1_HMAC ||
                     crd1->crd_alg == CRYPTO_MD5 ||
                     crd1->crd_alg == CRYPTO_SHA1) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
		     crd2->crd_alg == CRYPTO_3DES_CBC ||
		     crd2->crd_alg == CRYPTO_AES_CBC ||
		     crd2->crd_alg == CRYPTO_ARC4)) {

			if((crd2->crd_flags & CRD_F_ENCRYPT) == 0)
				ses->ses_direction = ELP_SESDIR_OUTBOUND;
			else
				ses->ses_direction = ELP_SESDIR_INBOUND;
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
		     crd1->crd_alg == CRYPTO_ARC4 ||
		     crd1->crd_alg == CRYPTO_3DES_CBC ||
		     crd1->crd_alg == CRYPTO_AES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC ||
                     crd2->crd_alg == CRYPTO_SHA1_HMAC ||
                     crd2->crd_alg == CRYPTO_MD5 ||
                     crd2->crd_alg == CRYPTO_SHA1)) {

			if (crd1->crd_flags & CRD_F_ENCRYPT)
		    		ses->ses_direction = ELP_SESDIR_OUTBOUND;
			else
				ses->ses_direction = ELP_SESDIR_INBOUND;
			enccrd = crd1;
			maccrd = crd2;
		} else {
			/*
			 * We cannot order the spacc  as requested
			 */
			DPRINTF(ELP_ERR, " Crypt algo %x %x flags %x not supported\n", crd1->crd_alg, crd2->crd_alg, crd1->crd_flags);
			crp->crp_etype = err;
			crypto_done(crp);
			spin_unlock_bh(&elp_ddt_lock);
			return (err);
		}
	}

	if(enccrd == NULL && maccrd == NULL)
	{
		DPRINTF (ELP_ERR, " enccrd %lx maccrd %lx\n", (unsigned long int)enccrd, (unsigned long int)maccrd);
		crp->crp_etype = err;
		crypto_done(crp);
		spin_unlock_bh(&elp_ddt_lock);
		return (err);
	}

	if(enccrd == NULL) {
		sc->stats.nr_hash_req++;

		if((handle = spacc_open (CRYPTO_MODE_NULL, maccrd->crd_alg, ctxid, ctxid_skip, 0, ses->ses_first_packet)) < 0)
		{
			err = ERESTART;
			goto out;
		}
	} else if (maccrd == NULL){
		sc->stats.nr_enc_req++;

		if(enccrd->crd_alg == CRYPTO_ARC4)
		{
			if((handle = spacc_open (enccrd->crd_alg, CRYPTO_MODE_NULL, ctxid, ctxid_skip, enccrd->crd_klen, ses->ses_first_packet)) < 0)
			{
				err = ERESTART;
				goto out;
			}
		}
		else if((handle = spacc_open (enccrd->crd_alg, CRYPTO_MODE_NULL, ctxid, ctxid_skip, 0, ses->ses_first_packet)) < 0)
		{
			err = ERESTART;
			goto out;
		}
	} else {
		sc->stats.nr_hash_req++;
		sc->stats.nr_enc_req++;
		DPRINTF (ELP_DBG, "spacc_open CIPHER ealg = %d & HASH aalg = %d\n", enccrd->crd_alg, enccrd->crd_alg);
		if((handle = spacc_open (enccrd->crd_alg, maccrd->crd_alg, ctxid, ctxid_skip, 0, ses->ses_first_packet)) < 0)
		{
			DPRINTF (ELP_ERR, "spacc_open CIPHER & HASH: %s\n", spacc_error_msg (handle));
			goto out;
		}
	}
	ses->ses_first_packet = 0;
	//ses->ses_spacc_handle = handle;
	/* sc->crp = crp;*/

	ctx = context_lookup (handle);

	ctx->dev = sc->sc_dev;
	ctx->crp = crp;

	if (crp->crp_flags & CRYPTO_F_SKBUF) {
		DPRINTF (ELP_ERR, "Buffer model CRYPTO_F_SKBUF is not supported\n");
		goto out;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		ctx->op_src_io = (struct uio *)crp->crp_buf;
		ctx->op_dst_io = (struct uio *)crp->crp_buf;

		uio = ctx->op_src_io;
		iov = uio->uio_iov;
	}

	if(enccrd) {
		dumpword ((U32*)enccrd->crd_key, (enccrd->crd_klen >> 3), "CIPHER KEY from cryptodesc");

		if (enccrd->crd_flags & CRD_F_IV_EXPLICIT) {
			dumpword ((U32*)enccrd->crd_iv, ses->ses_iv_size, "CRD_F_IV_EXPLICIT CIPHER IV");

			if ((err = spacc_set_context (handle, SPACC_CRYPTO_OPERATION, (U8 *)enccrd->crd_key, (enccrd->crd_klen >> 3), (U8 *)enccrd->crd_iv, ses->ses_iv_size)) != SPACC_CRYPTO_OK) {
				DPRINTF(ELP_ERR, "spacc_set_context %s\n", spacc_error_msg (err));
				goto out;
			}
		}
		else {
			dumpword ((U32*)ses->ses_iv, ses->ses_iv_size, "session random CIPHER IV ");

			if ((err = spacc_set_context (handle, SPACC_CRYPTO_OPERATION, (U8 *)enccrd->crd_key, (enccrd->crd_klen >> 3), (U8 *)ses->ses_iv, ses->ses_iv_size)) != SPACC_CRYPTO_OK) {
				DPRINTF (ELP_ERR, "spacc_set_context %s\n", spacc_error_msg (err));
				goto out;
			}
		}
	}

	if(maccrd) {
		DPRINTF(ELP_DBG, "HASH KEY from cryptodesc klen = %d flags %08x\n", maccrd->crd_klen, maccrd->crd_flags);
		if(maccrd->crd_klen) {
			dumpword((U32 *)maccrd->crd_key, maccrd->crd_klen >> 3, "HASH KEY");
		}

		if(maccrd->crd_flags & CRD_F_KEY_EXPLICIT) {
			if ((err = spacc_set_context (handle, SPACC_HASH_OPERATION, (unsigned char*) maccrd->crd_key, (maccrd->crd_klen >> 3), 0, 0)) != SPACC_CRYPTO_OK) {
				DPRINTF (ELP_ERR, "spacc_set_context %s\n", spacc_error_msg (err));
				goto out;
			}
		}
		else {
			DPRINTF (ELP_DBG, "key is not explicitly provided\n");
#if 0 /*TODO */
			read_random(mackey, (maccrd->crd_klen >> 3));
			if ((err = spacc_set_context (handle, SPACC_HASH_OPERATION, mackey, (maccrd->crd_klen >> 3), 0, 0)) != SPACC_CRYPTO_OK) {
				DPRINTF ("spacc_set_context %s\n", spacc_error_msg (err));
				goto out;
			}
#endif
		}

		if((maccrd->crd_alg == CRYPTO_SHA1) || (maccrd->crd_alg == CRYPTO_MD5) ||
		   (maccrd->crd_alg == CRYPTO_SHA1_HMAC) || (maccrd->crd_alg == CRYPTO_MD5_HMAC))
		{
			DPRINTF(ELP_DBG, "iov_len %d crd_klen%d\n", iov->iov_len,(maccrd->crd_klen>>3));
			if (crp->crp_flags & CRYPTO_F_IOV) {
 				iov->iov_len -= (maccrd->crd_klen>>3); /* Upper layer giving padded len for output also */
			}
		}
	}

	if (ses->ses_direction == ELP_SESDIR_OUTBOUND)
	{
		if ((err = spacc_set_operation (handle, OP_ENCRYPT, IM_ICV_IGNORE, IP_ICV_IGNORE, 0, 0)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_ERR, "spacc_set_operation %s\n", spacc_error_msg (err));
			goto out;
		}
	}
	else
	{
		if ((err = spacc_set_operation (handle, OP_DECRYPT, IM_ICV_IGNORE, IP_ICV_IGNORE, 0, 0) != SPACC_CRYPTO_OK)) {
			DPRINTF (ELP_ERR, "spacc_set_operation %s\n", spacc_error_msg (err));
			goto out;
		}
	}

	sc->stats.crypto_len_counters[((u32)crp->crp_ilen >> 10) & 0x1f]++;

	if (crp->crp_flags & CRYPTO_F_IOV) {
		int out_len=0;

		if (iov->iov_base) {
			dumpword((U32*)iov->iov_base, iov->iov_len, "INPUT BUFFER IOV");
		} else {
			DPRINTF (ELP_ERR, "%s():%d EINVAL\n",__FUNCTION__,__LINE__);
			err = EINVAL; goto out;
		}

		if(crp->crp_ilen > iov->iov_len)
			out_len = crp->crp_ilen - iov->iov_len;

		if ((err = spacc_add_ddt (handle, (U8*)iov->iov_base, iov->iov_len, sc)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_ERR, "spacc_add_ddt : %s\n", spacc_error_msg (err));
			goto out;
		}

		if ((err = spacc_add_dst_ddt (handle, (U8*)iov->iov_base, iov->iov_len < out_len?out_len:iov->iov_len, sc, 0)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_ERR, "spacc_add_dst_ddt : %s\n", spacc_error_msg (err));
			goto out;
		}

		if ((err = spacc_packet_enqueue_ddt(handle, 0, 0, 0, 0, 0, SPACC_SW_ID_PRIO_HI)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_ERR, "spacc_packet_enqueue_ddt : %s\n", spacc_error_msg (err));
			goto out;
		}
	} else {

		if (crp->crp_buf) {
			DPRINTF(ELP_DBG, "%s: INPUT BUFFER (len %d)\n", __FUNCTION__, (unsigned int)crp->crp_ilen);
			/*for(i = 0; i < crp->crp_ilen; i++)
				printk("%2x ", *((U8*)crp->crp_buf + i));
			printk("\n");*/
		} else {
			DPRINTF (ELP_ERR, "%s():%d EINVAL\n",__FUNCTION__,__LINE__);
			err = EINVAL; goto out;
		}

		if ((err = spacc_add_ddt (handle, (unsigned char*)crp->crp_buf, crp->crp_ilen, sc)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_ERR, "spacc_add_ddt : %s\n", spacc_error_msg (err));
			goto out;
		}

		if (ses->ses_direction == ELP_SESDIR_INBOUND) {
			same_buff = 1;
		}
#if defined(CONFIG_SYNO_COMCERTO)
		if ((err = spacc_add_dst_ddt (handle, (unsigned char*)crp->crp_buf, crp->crp_ilen, sc, same_buff)) != SPACC_CRYPTO_OK) {
#else
		if ((err = spacc_add_dst_ddt (handle, (unsigned char*)crp->crp_out_buf, crp->crp_ilen, sc, same_buff)) != SPACC_CRYPTO_OK) {
#endif
			DPRINTF (ELP_ERR, "spacc_add_dst_ddt : %s\n", spacc_error_msg (err));
			goto out;
		}

		if ((err = spacc_packet_enqueue_ddt(handle, 0, 0, 0, 0, 0, SPACC_SW_ID_PRIO_HI)) != SPACC_CRYPTO_OK) {
			DPRINTF (ELP_ERR, "spacc_packet_enqueue_ddt : %s\n", spacc_error_msg (err));
			goto out;
		}
	}

	spin_unlock_bh(&elp_ddt_lock);
	return 0;
out:
	DPRINTF(ELP_DBG, "%s: failed\n", __FUNCTION__);

	if (err != ERESTART) {
		crp->crp_etype = err;
		crypto_done(crp);
	} else {
		sc->sc_needwakeup |= CRYPTO_SYMQ;
	}

	spin_unlock_bh(&elp_ddt_lock);
	return (err);
}

#ifdef ELP_PKA_SELF_TEST
#include "pka_test_vect_rsa.h"
static void pka_self_test()
{
	int i;
	unsigned char result1[2048];
	unsigned char x[512],y[512];

#ifdef PKA_ECC_TEST_POINT_ADD
#include "pka_test_vect_ecc_add.h"
	printk("Testing ecc point add\n");
	for(i=0;i<eccadd_size/32;i++)
	{
		eccadd_a[i] = htonl(eccadd_a[i]);
		eccadd_b[i] = htonl(eccadd_b[i]);
		eccadd_m[i] = htonl(eccadd_m[i]);
		eccadd_mp[i] = htonl(eccadd_mp[i]);
		eccadd_x[i] = htonl(eccadd_x[i]);
		eccadd_y[i] = htonl(eccadd_y[i]);
	}
	if ((clue_ec_load_curve_data (eccadd_curve)) != SPACC_CRYPTO_OK)
		printk("Failed to load curve_data\n");
	if ((clue_ec_load_curve_padd (eccadd_curve)) != SPACC_CRYPTO_OK)
		printk("Failed to load curve_padd\n");
	if ((clue_ec_point_add ((U8*)&eccadd_a[0], (U8*)&eccadd_b[0], (U8*)&eccadd_m[0], (U8*)&eccadd_mp[0], x, y, eccadd_size / 8)) != SPACC_CRYPTO_OK)
		printk("Failed to do point add\n");
	else
	{
		if ((memcmp (x, eccadd_x, eccadd_size / 8)) || (memcmp(y, eccadd_y, eccadd_size/8))) {
			printk("Failed \n");
			dumpword (eccadd_x, eccadd_size/8, "Expected result x");
			dumpword (eccadd_y, eccadd_size/8, "Expected result y");
		}
		else 
			printk(" Done\n");
	}

#endif

#ifdef PKA_ECC_TEST_POINT_MUL
#include "pka_test_vect_ecc_pmul.h"
	
	printk("Testing ecc point mul\n");
	for(i=0;i<pmul_k_size/4;i++)
	{
		pmul_m[i] = htonl(pmul_m[i]);
	}

	for(i=0;i<pmul_size/32;i++)
	{
		pmul_rx[i] = htonl(pmul_rx[i]);
		pmul_ry[i] = htonl(pmul_ry[i]);
	}

	if ((clue_ec_load_curve_data (pmul_curve)) != SPACC_CRYPTO_OK)
		printk("Failed to load curve_data\n");
	if ((clue_ec_load_curve_pmult (pmul_curve, 1)) != SPACC_CRYPTO_OK)
		printk("Failed\n");
	if ((clue_ec_point_mult_base (pmul_m, x, y, pmul_size / 8, pmul_k_size)) != SPACC_CRYPTO_OK)
		printk("Failed to do mult base\n");
	else
	{
		if ((memcmp (x, pmul_rx, pmul_size / 8)) || (memcmp(y, pmul_ry, pmul_size/8))) {
			printk("Failed \n");
			dumpword (pmul_rx, pmul_size/8, "Expected result x");
			dumpword (pmul_ry, pmul_size/8, "Expected result y");
		}
		else 
			printk(" Done\n");
	}
#endif

#ifdef PKA_ECC_TEST_POINT_DBL
#include "pka_test_vect_ecc_pdbl.h"

	printk("Testing ecc point dbl\n");
	for(i=0;i<eccpdbl_size/32;i++)
	{
		eccpdbl_a[i] = htonl(eccpdbl_a[i]);
		eccpdbl_b[i] = htonl(eccpdbl_b[i]);
		eccpdbl_rx[i] = htonl(eccpdbl_rx[i]);
		eccpdbl_ry[i] = htonl(eccpdbl_ry[i]);
	}

	if ((clue_ec_load_curve_data (eccpdbl_curve)) != SPACC_CRYPTO_OK)
		printk("Failed to load curve_data\n");
	if ((clue_ec_load_curve_pdbl (eccpdbl_curve)) != SPACC_CRYPTO_OK)
		printk("Failed\n");
	if ((clue_ec_point_double (eccpdbl_a, eccpdbl_b, x, y, eccpdbl_size / 8)) != SPACC_CRYPTO_OK)
		printk("Failed to do point dbl\n");
	else
	{
		if ((memcmp (x, eccpdbl_rx, eccpdbl_size / 8)) || (memcmp(y, eccpdbl_ry, eccpdbl_size/8))) {
			printk("Failed \n");
			dumpword (eccpdbl_rx, eccpdbl_size/8, "Expected result x");
			dumpword (eccpdbl_ry, eccpdbl_size/8, "Expected result y");
		}
		else 
			printk(" Done\n");
	}

#endif

#ifdef PKA_ECC_TEST_POINT_PVER
#include "pka_test_vect_ecc_pver.h"

	printk("Testing ecc point verify\n");
	for(i=0;i<eccpverf_size/32;i++)
	{
		eccpverf_x[i] = htonl(eccpverf_x[i]);
		eccpverf_y[i] = htonl(eccpverf_y[i]);
	}
	printk("ecc point verify\n");
	if ((clue_ec_load_curve_data (eccpverf_curve)) != SPACC_CRYPTO_OK)
		printk("Failed to load curve_data\n");
	if ((clue_ec_load_curve_pver (eccpverf_curve)) != SPACC_CRYPTO_OK)
		printk("Failed\n");
	if ((clue_ec_point_verify (eccpverf_x, eccpverf_y, eccpverf_size / 8)) != SPACC_CRYPTO_OK)
		printk("Failed to do point verify\n");
	else
		printk("Done\n");

#endif

#ifdef PKA_TEST_MOD_EXP
	for(i=0;i<modexp_size/32;i++)
	{
		modexp_m[i] = htonl(modexp_m[i]);
		modexp_a[i] = htonl(modexp_a[i]);
		modexp_b[i] = htonl(modexp_b[i]);
		modexp_ry[i] = htonl(modexp_ry[i]);
	}

	// modexp self test
	printk("\nTesting modexp....");
	if (clue_bn_modexp ((U8*)&modexp_a[0], (U8*)&modexp_b[0], (U8*)&modexp_m[0], result1, (modexp_size / 8), 1) != SPACC_CRYPTO_OK)
	{
		printk(" ERROR \n");
	}
	else 
	{
		if (memcmp (result1, modexp_ry, modexp_size / 8)) {
			printk("Failed \n");
			dumpword ((U32 *)modexp_ry, modexp_size/8, "Expected result");
			printk("\n");
			dumpword ((U32 *)result1, modexp_size/8, "Actual result");
		}
		else 
			printk(" Done\n");
	}

#endif

#ifdef PKA_TEST_MOD_MULT
	printk("Testing modmult....");
	for(i=0;i<modmult_size/32;i++)
	{
		modmult_m[i] = htonl(modmult_m[i]);
		modmult_a[i] = htonl(modmult_a[i]);
		modmult_b[i] = htonl(modmult_b[i]);
		modmult_ry[i] = htonl(modmult_ry[i]);
	}

	if (clue_bn_modmult ((U8*)&modmult_a[0], (U8*)&modmult_b[0], (U8*)&modmult_m[0], result1, (modmult_size / 8), 1) != SPACC_CRYPTO_OK)
	{
		printk(" ERROR \n");
	}
	else 
	{
		if (memcmp (result1, modmult_ry, modmult_size / 8)) {
			printk("Failed \n");
			dumpword ((U32 *)modmult_ry, modmult_size/8, "Expected result");
		}
		else 
			printk(" Done\n");
	}
#endif

#ifdef PKA_TEST_MOD_DIV
	printk("Testing moddiv....");
	for(i=0;i<moddiv_size/32;i++)
	{
		moddiv_m[i] = htonl(moddiv_m[i]);
		moddiv_a[i] = htonl(moddiv_a[i]);
		moddiv_b[i] = htonl(moddiv_b[i]);
		moddiv_ry[i] = htonl(moddiv_ry[i]);
	}

	if (clue_bn_moddiv ((U8*)&moddiv_a[0], (U8*)&moddiv_b[0], (U8*)&moddiv_m[0], result1, (moddiv_size / 8)) != SPACC_CRYPTO_OK)
	{
		printk(" ERROR \n");
	}
	else 
	{
		if (memcmp (result1, moddiv_ry, moddiv_size / 8)) {
			printk("Failed \n");
			dumpword ((U32 *)moddiv_ry, moddiv_size/8, "Expected result");
		}
		else 
			printk(" Done\n");
	}
#endif
#ifdef PKA_TEST_MOD_ADD
	printk("Testing modadd....");
	for(i=0;i<modadd_size/32;i++)
	{
		modadd_m[i] = htonl(modadd_m[i]);
		modadd_a[i] = htonl(modadd_a[i]);
		modadd_b[i] = htonl(modadd_b[i]);
		modadd_ry[i] = htonl(modadd_ry[i]);
	}

	if (clue_bn_modadd ((U8*)&modadd_a[0], (U8*)&modadd_b[0], (U8*)&modadd_m[0], result1, (modadd_size / 8)) != SPACC_CRYPTO_OK)
	{
		printk(" ERROR \n");
	}
	else 
	{
		if (memcmp (result1, modadd_ry, modadd_size / 8)) {
			printk("Failed \n");
			dumpword ((U32 *)modadd_ry, modadd_size/8, "Expected result");
		}
		else {
			printk(" Done\n");
			dumpword ((U32 *)modadd_ry, modadd_size/8, "Expected result");
		}
	}
#endif

#ifdef PKA_TEST_MOD_SUB
	printk("Testing modsub....");
	for(i=0;i<modsub_size/32;i++)
	{
		modsub_m[i] = htonl(modsub_m[i]);
		modsub_a[i] = htonl(modsub_a[i]);
		modsub_b[i] = htonl(modsub_b[i]);
		modsub_ry[i] = htonl(modsub_ry[i]);
	}

	if (clue_bn_modsub ((U8*)&modsub_a[0], (U8*)&modsub_b[0], (U8*)&modsub_m[0], result1, (modsub_size / 8)) != SPACC_CRYPTO_OK)
	{
		printk(" ERROR \n");
	}
	else 
	{
		if (memcmp (result1, modsub_ry, modsub_size / 8)) {
			printk("Failed \n");
			dumpword ((U32 *)modsub_ry, modsub_size/8, "Expected result");
		}
		else
			printk(" Done\n");
	}
#endif

#ifdef PKA_TEST_MOD_INV
	printk("Testing modinv....");
	for(i=0;i<modinv_size/32;i++)
	{
		modinv_m[i] = htonl(modinv_m[i]);
		modinv_b[i] = htonl(modinv_b[i]);
		modinv_ry[i] = htonl(modinv_ry[i]);
	}

	if (clue_bn_modinv ((U8*)&modinv_b[0], (U8*)&modinv_m[0], result1, (modinv_size / 8)) != SPACC_CRYPTO_OK)
	{
		printk(" ERROR \n");
	}
	else 
	{
		if (memcmp (result1, modinv_ry, modinv_size / 8)) {
			printk("Failed \n");
			dumpword ((U32*)moddiv_ry, modinv_size/8, "Expected result");
		}
		else 
			printk(" Done\n");
	}
#endif

	return;

}
#endif
static int elp_kprocess(device_t dev, struct cryptkop *krp, int hint)
{
	struct elp_softc *sc = device_get_softc(dev);
	struct pka_pkq *q = NULL;
	unsigned long flags; 

	DPRINTF(ELP_DBG, "%s: KOP=%d iparams=%d oparams=%d l1=%d l2=%d l3=%d l4=%d\n", __FUNCTION__,
			krp->krp_op,krp->krp_iparams, krp->krp_oparams,
			krp->krp_param[0].crp_nbits,
			krp->krp_param[1].crp_nbits,
			krp->krp_param[2].crp_nbits,
			krp->krp_param[3].crp_nbits);

	switch (krp->krp_op) {
		case CRK_MOD_EXP:
			if (krp->krp_iparams == 3 && krp->krp_oparams == 1)
				break;
			goto err;
		case CRK_MOD_EXP_CRT:
			if (krp->krp_iparams == 6 && krp->krp_oparams == 1)
				break;
			goto err;
		case CRK_DSA_SIGN:
			if (krp->krp_iparams == 5 && krp->krp_oparams == 2)
				break;
			goto err;
		case CRK_DSA_VERIFY:
			if (krp->krp_iparams == 7 && krp->krp_oparams == 0)
				break;
			goto err;
		case CRK_DH_COMPUTE_KEY:
			if (krp->krp_iparams == 3 && krp->krp_oparams == 1)
				break;
			goto err;
		default:
			goto err;
	}

	if (sc == NULL) {
		krp->krp_status = EINVAL;
		DPRINTF(ELP_ERR, "%s: sc == NULL\n", __FUNCTION__);
		goto err;
	}

	/* For now only modular exponentiation is supported */
	if (krp->krp_op != CRK_MOD_EXP) {
		krp->krp_status = EOPNOTSUPP;
		DPRINTF(ELP_ERR, "%s: krp_op %d NOT SUPPORTED\n", __FUNCTION__, krp->krp_op);
		goto err;
	}

	sc->krp = krp;

	q = (struct pka_pkq *) kmalloc(sizeof(*q), GFP_KERNEL);
	if (q == NULL) {
		krp->krp_status = ENOMEM;
		goto err;
	}

	memset(q, 0, sizeof(*q));
	q->pkq_krp = krp;
	INIT_LIST_HEAD(&q->pkq_list);

	q->pkq_obuf = (U32 *)krp->krp_param[KRP_PARAM_RES].crp_p;
	q->pkq_result_len = krp->krp_param[KRP_PARAM_RES].crp_nbits / 8;  //use length in bytes unit

	if (pka_copy_kparam(&krp->krp_param[KRP_PARAM_BASE], q->pkq_M, q->pkq_result_len)) {
		krp->krp_status = ERANGE; goto err;
	}
	q->pkq_M_len = krp->krp_param[KRP_PARAM_BASE].crp_nbits / 8;

	if (pka_copy_kparam(&krp->krp_param[KRP_PARAM_EXP], q->pkq_e,q->pkq_result_len)) {
		krp->krp_status = ERANGE; goto err;
	}
	q->pkq_e_len = krp->krp_param[KRP_PARAM_EXP].crp_nbits / 8;
	
	if (pka_copy_kparam(&krp->krp_param[KRP_PARAM_MOD], q->pkq_N,q->pkq_result_len)) {
		krp->krp_status = ERANGE; goto err;
	}
	q->pkq_N_len = krp->krp_param[KRP_PARAM_MOD].crp_nbits / 8;

#if 0
	if (q != NULL) {
		dumpword (q->pkq_M, q->pkq_M_len, "q->pkq_M");
		dumpword (q->pkq_e, q->pkq_e_len, "q->pkq_e");
		dumpword (q->pkq_N, q->pkq_N_len, "q->pkq_N");
		DPRINTF(ELP_DBG, "q->pkq_result_len %ld\n", q->pkq_result_len);
	}
#endif

	spin_lock_irqsave(&pka_pkq_lock, flags);
	list_add_tail(&q->pkq_list, &sc->sc_pkq);
	pka_kfeed(sc);
	spin_unlock_irqrestore(&pka_pkq_lock, flags);

	return (0);
err:
	if (q)
		kfree(q);
	crypto_kdone(krp);

	return (0);
}

void elp_register_ocf(struct elp_softc *sc)
{
	/* register algorithms with the framework */
	DPRINTF(ELP_ERR, DRV_NAME);

#if !defined(CONFIG_SYNO_COMCERTO)
	printk("\nm86xxx_elp: Registering  key ");
	crypto_kregister(sc->sc_cid, CRK_MOD_EXP, 0);
#endif

#ifdef CONFIG_OCF_RANDOMHARVEST_MODULE
	printk("random ");
	crypto_rregister(sc->sc_cid, trng_read_random, sc);
#endif

	printk("des/3des ");
	crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, 0, 0);
	crypto_register(sc->sc_cid, CRYPTO_DES_CBC, 0, 0);

	printk("aes ");
	crypto_register(sc->sc_cid, CRYPTO_AES_CBC, 0, 0);

#if !defined(CONFIG_SYNO_COMCERTO)
	printk("rc4 ");
	crypto_register(sc->sc_cid, CRYPTO_ARC4, 0, 0);
#endif

	printk("md5 ");
	crypto_register(sc->sc_cid, CRYPTO_MD5, 0, 0);

	/* HMAC support only with IPsec for now */
	crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC, 0, 0);

	printk("sha1 ");
	crypto_register(sc->sc_cid, CRYPTO_SHA1, 0, 0);

	/* HMAC support only with IPsec for now */
	crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC, 0, 0);

	printk("sha256 ");
	crypto_register(sc->sc_cid, CRYPTO_SHA2_256_HMAC, 0, 0);

	printk("null\n");
	crypto_register(sc->sc_cid, CRYPTO_NULL_CBC, 0, 0);
	crypto_register(sc->sc_cid, CRYPTO_NULL_HMAC, 0, 0);

	/* XXX other supported algorithms */
}

/********************* SPACC-PDU interrupt management ****************/
static irqreturn_t spacc_intr(int irq, void *dev_id)
{
	unsigned long flags;
	int cnt;

	//DPRINTF(ELP_DBG, "%s()\n", __FUNCTION__);

	spin_lock_irqsave(&reg_lock, flags);

	cnt = spacc_fifo_stat();

	if (cnt) {

		spacc_disable_int();

		tasklet_schedule(&irq_spacc_tasklet);
	}
#ifdef ELP_TRNG_IRQ_MODE
	else if ((ELP_READ_UINT(_trngm.irq_stat) & TRNG_IRQ_DONE) == TRNG_IRQ_DONE) {

		trng_disable_int();

		/* Ack interrupt */
		ELP_WRITE_UINT( _trngm.irq_stat, ELP_READ_UINT(_trngm.irq_stat) | TRNG_IRQ_DONE);

		/* process TRNG IRQ (call tasklet...)*/

		trng_enable_int();
	}
#endif
	else {
		printk("spacc_intr: This shouldn't happen \n");
	}

	spin_unlock_irqrestore(&reg_lock, flags);
	return IRQ_HANDLED;
}

#if 0
static void elp_pka_tasklet(unsigned long arg)
{
	struct elp_softc *sc = (struct elp_softc *)arg;
	struct cryptkop *krp = sc->krp;
	struct pka_pkq *q = sc->sc_pkq_cur;
	unsigned long flags;
	U32 reason;

	reason = CLUE_READ_REASON (_spacc_dev.reg.regmap);
	switch (reason) {
		case CLUE_REASON_NORMAL:
			break;
		case CLUE_REASON_STOP_RQST:
			CLUE_CLEAR_STACK (_spacc_dev.reg.regmap);
			break;
		default:
			DPRINTF(ELP_ERR, "CLUE STOP: Reason %X [%s]\n", reason, pka_errmsgs[reason]);
			break;
	}

	if(sc->sc_pkq_cur)
	dumpword(q->pkq_obuf, q->pkq_result_len, "PKA ouput");

	if(sc->sc_pkq_cur)
		kfree(sc->sc_pkq_cur);

	sc->sc_pkq_cur = NULL;

#ifndef ELP_PKA_SELF_TEST
	crypto_kdone(krp);
#else
	printk("We are not indicating crypto_kdone\n");
#endif

	spin_lock_irqsave(&reg_lock, flags);
	pka_enable_int();
	spin_unlock_irqrestore(&reg_lock, flags);
}
#endif

static void elp_spacc_tasklet(unsigned long arg)
{
	struct cryptop *crp = NULL;/*(struct cryptop *)sc->crp;*/
	S32 ret = SPACC_CRYPTO_INPROGRESS;
  	elp_spacc_handle_ctx *ctx = NULL;
  	U32 status, cmdstat;
	u_int32_t x;
	U32 y, jobid;
	unsigned long flags;
	struct elp_softc *sc = (struct elp_softc *)arg;

#if defined(CONFIG_SYNO_COMCERTO)
	if(irqs_disabled()) {
		tasklet_schedule(&irq_spacc_tasklet);
		return;
	}
#endif
	while (spacc_fifo_stat() > 0)
	{
		spin_lock_irqsave(&reg_lock, flags);
		ELP_WRITE_UINT(_spacc_dev.reg.stat_pop,0x1);
		status  = ELP_READ_UINT(_spacc_dev.reg.fifo_stat);
		cmdstat = ELP_READ_UINT(_spacc_dev.reg.status);

/*
		DPRINTF(ELP_DBG, "elp_spacc_tasklet: after pop fifo => fifo_stat:%lX status %lX fifo_stat_count:%lu return code:%lX\n",
			status, cmdstat, SPACC_GET_FIFO_STAT_CNT (status),
			SPACC_GET_STATUS_RET_CODE (cmdstat));
*/

		jobid = cmdstat & 0xFF;
		if (_spacc_dev.job_pool[jobid][0] == 0xFFFFFFFF) {
			DPRINTF(ELP_ERR, "Invalid job id (%d) popped off the stack\n", jobid);
			spacc_ack_int();
#if defined(CONFIG_SYNO_COMCERTO)
			spin_unlock_irqrestore(&reg_lock, flags);
#endif
			continue;
		}
		x = _spacc_dev.job_pool[jobid][0];
		y = _spacc_dev.job_pool[jobid][1];
		_spacc_dev.job_pool[jobid][0] = 0xFFFFFFFF;
		_spacc_dev.job_pool[jobid][1] = 0xFFFFFFFF;
		spin_unlock_irqrestore(&reg_lock, flags);

		if (x == CRYPTO_CONTEXTS_MAX) {
			DPRINTF(ELP_ERR, "%s: Invalid job_id %u\n", __FUNCTION__, cmdstat & 0xFF);
			spacc_ack_int();
			continue;
		}

		ctx = context_lookup (x);

		crp = ctx->crp;

		/* check SPacc return code */
		switch (SPACC_GET_STATUS_RET_CODE (cmdstat)) {
		case SPACC_ICVFAIL:
			DPRINTF(ELP_ERR, "%s: handle %x: SPACC_ICVFAIL\n", __FUNCTION__, x);
			crp->crp_etype = SPACC_CRYPTO_AUTHENTICATION_FAILED;
			if (ctx->ddt_idx > 0) {
				spacc_close (x, sc);
			}
			break;
		case SPACC_MEMERR:
			DPRINTF(ELP_ERR, "%s: handle %x: SPACC_MEMERR\n", __FUNCTION__, x);
			if (ctx->ddt_idx > 0) {
				spacc_close (x, sc);
			}
			crp->crp_etype = SPACC_CRYPTO_MEMORY_ERROR;
			break;
		case SPACC_BLOCKERR:
			DPRINTF(ELP_ERR, "%s: handle %d: SPACC_BLOCKERR\n", __FUNCTION__, x);
			if (ctx->ddt_idx > 0) {
				spacc_close (x, sc);
			}
			crp->crp_etype = SPACC_CRYPTO_INVALID_BLOCK_ALIGNMENT;
			break;

		case SPACC_OK:
			ctx->job_done = 1;

			/* move the last job to this slot to free up a spot */
			if (y != (ctx->job_idx - 1)) {
				ctx->job_id[y] = ctx->job_id[ctx->job_idx-1];
				y = ctx->job_idx-1;
			}

			/* mark job as done */
			ctx->job_id[y] = 0xFFFFFFFF;
			--(ctx->job_idx);

			crp->crp_etype = SPACC_CRYPTO_OK;
			break;
		}

		if((ret = spacc_close (x, sc)) < 0) {
			DPRINTF (ELP_ERR, "warning: %s\n", spacc_error_msg (ret));
		}

		if (crp->crp_flags & CRYPTO_F_IOV) {
			struct iovec *iov = NULL;
			struct uio *uio = NULL;
			int hash_out_len=0;

			uio = ctx->op_dst_io;
			iov = uio->uio_iov;
			crp->crp_olen = iov->iov_len;
			hash_out_len = crp->crp_ilen - iov->iov_len;

			if(hash_out_len>0)
			{
				if(iov->iov_len < hash_out_len)
				{
					U8 *dest, *src;
					//Input len is smaller than output len
					dest = (U8*)iov->iov_base + iov->iov_len + hash_out_len;
					src = (U8*)iov->iov_base + hash_out_len;
					while((U8*)src >= (U8*)iov->iov_base) {
						*dest-- = *src--;
					}
				}
				else
					memcpy((U8*)iov->iov_base+iov->iov_len, iov->iov_base, hash_out_len);
			}
			if (iov->iov_base == NULL) {
				DPRINTF(ELP_ERR, "%s: iov->iov_base == NULL\n", __FUNCTION__);
			}
		}

		/*Acknowledge each interrupt here only as we are emptying fifo_stat*/
		spacc_ack_int();

		crypto_done(crp);

	}
	if (sc->sc_needwakeup) {
		int wakeup = sc->sc_needwakeup & (CRYPTO_SYMQ|CRYPTO_ASYMQ);
		sc->sc_needwakeup &= ~wakeup;
		crypto_unblock(sc->sc_cid, wakeup);
	}

	spin_lock_irqsave(&reg_lock, flags);
	spacc_enable_int();
	spin_unlock_irqrestore(&reg_lock, flags);
}

int spacc_probe(void)
{
	int err = 0;

	/* DPRINTF (ELP_DBG, "spacc_probe: irq_mode=%d irq=%d  little_endian=%d\n",
		irq_mode, dev->irq,  little_endian); */
	if (spacc_init (CRYPTO_MODULE_SPACC | CRYPTO_MODULE_TRNG | CRYPTO_MODULE_PKA, ((U32)tif.preg) + SPACC_MEMORY_BASE_OFFSET) != SPACC_CRYPTO_OK)
	{
		DPRINTF (ELP_ERR, "spacc_probe: spacc_init failed\n");
		err = -2;
	}

	DPRINTF(ELP_DBG, "elpspacc: Initialization status: %d\n", err);

	return err;
}

void spacc_remove (void)
{

	if (irq_mode) {
#ifdef ELP_TRNG_IRQ_MODE
		trng_disable_int();
#endif
		spacc_disable_int();
		//pka_disable_int();
		spacc_pdu_disable_int();
		ELP_WRITE_UINT(_spacc_dev.reg.irq_stat, ELP_READ_UINT(_spacc_dev.reg.irq_stat)|SPACC_IRQ_STAT_STAT);
	}

	while(SPACC_GET_FIFO_STAT_CNT (ELP_READ_UINT(_spacc_dev.reg.fifo_stat))) {
		ELP_WRITE_UINT(_spacc_dev.reg.stat_pop,  0x1);
	}

	spacc_fini ();

}

/************************************************************************/

/* Elliptic SPacc APIs **************************************************/

int spacc_fifo_stat (void)
{
	return SPACC_GET_FIFO_STAT_CNT (ELP_READ_UINT(_spacc_dev.reg.fifo_stat));
}

void spacc_disable_int (void)
{
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, ELP_READ_UINT(_spacc_dev.reg.irq_enable) & ~SPACC_IRQ_EN_STAT_EN);
}

void spacc_enable_int (void)
{
	// Enable only FIFO status interrupts
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, ELP_READ_UINT(_spacc_dev.reg.irq_enable) | SPACC_IRQ_EN_STAT_EN);
	ELP_WRITE_UINT(_spacc_dev.reg.irq_ctrl, SPACC_IRQ_CTRL_SET_STAT_CNT (1));
}

void spacc_pdu_enable_int (void)
{
#ifdef ELP_TRNG_IRQ_MODE
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, SPACC_IRQ_EN_STAT_EN | SPACC_IRQ_RNG_EN | SPACC_IRQ_EN_GLBL_EN);
#else
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, SPACC_IRQ_EN_STAT_EN | SPACC_IRQ_EN_GLBL_EN);
#endif
}

void spacc_pdu_disable_int (void)
{
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable,0);
}

// Initialize crypto context for all implemented
// hardware crypto modules.
// mmap:  start of the crypto hardware physical memory
S32 spacc_init (U32 opmodules, U32 regmap)
{
	int x, y;
	U32 id;

	DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);

	// Initialize all global structures
	_module_initialized = 0;

	MEMSET((unsigned char *)&_spacc_dev.reg,   0, sizeof (elp_spacc_regmap));
	MEMSET((unsigned char *)&_spacc_dev.ctx[0], 0, sizeof (elp_spacc_handle_ctx) * CRYPTO_CONTEXTS_MAX);

	_spacc_dev.reg.regmap = regmap;

	if ((opmodules & CRYPTO_MODULE_SPACC) == CRYPTO_MODULE_SPACC) {

		_spacc_dev.reg.ctrl          = (U32 *) (regmap + SPACC_REG_CTRL);
		_spacc_dev.reg.aux_info      = (U32 *) (regmap + SPACC_REG_AUX_INFO);
		_spacc_dev.reg.stat_pop      = (U32 *) (regmap + SPACC_REG_STAT_POP);
		_spacc_dev.reg.status        = (U32 *) (regmap + SPACC_REG_STATUS);
		_spacc_dev.reg.key_sz        = (U32 *) (regmap + SPACC_REG_KEY_SZ);
		_spacc_dev.reg.irq_stat      = (U32 *) (regmap + SPACC_REG_IRQ_STAT);
		_spacc_dev.reg.irq_enable    = (U32 *) (regmap + SPACC_REG_IRQ_EN);
		_spacc_dev.reg.irq_ctrl      = (U32 *) (regmap + SPACC_REG_IRQ_CTRL);
		_spacc_dev.reg.fifo_stat     = (U32 *) (regmap + SPACC_REG_FIFO_STAT);
		_spacc_dev.reg.sdma_burst_sz = (U32 *) (regmap + SPACC_REG_SDMA_BRST_SZ);
		_spacc_dev.reg.src_ptr       = (U32 *) (regmap + SPACC_REG_SRC_PTR);
		_spacc_dev.reg.dst_ptr       = (U32 *) (regmap + SPACC_REG_DST_PTR);

		_spacc_dev.reg.aad_offset = (U32 *) (regmap + SPACC_REG_OFFSET);
		_spacc_dev.reg.pre_aad_len   = (U32 *) (regmap + SPACC_REG_PRE_AAD_LEN);
		_spacc_dev.reg.post_aad_len  = (U32 *) (regmap + SPACC_REG_POST_AAD_LEN);
		_spacc_dev.reg.iv_offset     = (U32 *) (regmap + SPACC_REG_IV_OFFSET);

		_spacc_dev.reg.proc_len   = (U32 *) (regmap + SPACC_REG_PROC_LEN);
		_spacc_dev.reg.icv_offset = (U32 *) (regmap + SPACC_REG_ICV_OFFSET);
		_spacc_dev.reg.icv_len    = (U32 *) (regmap + SPACC_REG_ICV_LEN);

		_spacc_dev.reg.sw_tag   = (U32 *) (regmap + SPACC_REG_SW_ID);
		_spacc_dev.reg.ciph_key = (U32 *) (regmap + SPACC_CTX_CIPH_KEY);

		_spacc_dev.reg.hash_key = (U32 *) (regmap + SPACC_CTX_HASH_KEY);
		_spacc_dev.reg.rc4_key  = (U32 *) (regmap + SPACC_CTX_RC4_CTX);

		_spacc_dev.reg.vspacc_prio        = (U32 *) (regmap + SPACC_REG_VIRTUAL_PRIO);
		_spacc_dev.reg.vspacc_rc4_key_req = (U32 *) (regmap + SPACC_REG_VIRTUAL_RC4_KEY_RQST);
		_spacc_dev.reg.vspacc_rc4_key_gnt = (U32 *) (regmap + SPACC_REG_VIRTUAL_RC4_KEY_GNT);
		_spacc_dev.reg.spacc_id           = (U32 *) (regmap + SPACC_REG_ID);

		_spacc_dev._module_initialized = 1;
		_module_initialized = 1;

		DPRINTF(ELP_DBG, "SPacc Memory mapping (base @ %x) - irq_en(%x) %x - irq_stat(%x) %x - irq_ctrl(%x) %x - fifo_stat(%x) %x - status(%x) %x\n",
		regmap,
		(U32)_spacc_dev.reg.irq_enable, ELP_READ_UINT(_spacc_dev.reg.irq_enable),
		(U32)_spacc_dev.reg.irq_stat, ELP_READ_UINT(_spacc_dev.reg.irq_stat),
		(U32)_spacc_dev.reg.irq_ctrl, ELP_READ_UINT(_spacc_dev.reg.irq_ctrl),
		(U32)_spacc_dev.reg.fifo_stat, ELP_READ_UINT(_spacc_dev.reg.fifo_stat),
		(U32)_spacc_dev.reg.status, ELP_READ_UINT(_spacc_dev.reg.status));

	}

	if ((opmodules & CRYPTO_MODULE_TRNG) == CRYPTO_MODULE_TRNG) {
		if (trng_init (regmap) != SPACC_CRYPTO_OK)
			goto ERR;
	}

	if ((opmodules & CRYPTO_MODULE_PKA) == CRYPTO_MODULE_PKA) {
		if (pka_init (regmap) != SPACC_CRYPTO_OK)
			goto ERR;
	}

	id = ELP_READ_UINT(_spacc_dev.reg.spacc_id);
	DPRINTF(ELP_DBG, "SPACC ID: (%08lx)\n   MAJOR  : %x\n", (unsigned long)id, SPACC_ID_MAJOR(id));
	DPRINTF(ELP_DBG, "   MINOR  : %x\n", SPACC_ID_MINOR(id));
	DPRINTF(ELP_DBG, "   QOS    : %x\n", SPACC_ID_QOS(id));
	DPRINTF(ELP_DBG, "   PDU    : %x\n", SPACC_ID_PDU(id));
	DPRINTF(ELP_DBG, "   AUX    : %x\n", SPACC_ID_AUX(id));
	DPRINTF(ELP_DBG, "   IDX    : %x\n", SPACC_ID_VIDX(id));
	DPRINTF(ELP_DBG, "   PROJECT: %x\n", SPACC_ID_PROJECT(id));

#if 0
	// Test to read from FIFO_STAT register to make sure we have the correct regmap
	if ((ELPSEC_READ_UINT(_spacc_dev.reg.fifo_stat) & SPACC_FIFO_STAT_STAT_EMPTY) != SPACC_FIFO_STAT_STAT_EMPTY) {
		DPRINTF(ELP_DBG, "FIFO_STAT: %X : %X\n",(U32)_spacc_dev.reg.fifo_stat, ELPSEC_READ_UINT(_spacc_dev.reg.fifo_stat));
		goto ERR;
	}
	// Quick sanity check for ptr registers (mask unused bits)
	ELP_WRITE_UINT(_spacc_dev.reg.dst_ptr, 0x1234567F);
	ELP_WRITE_UINT(_spacc_dev.reg.src_ptr, 0xDEADBEEF);
	if (( (ELPSEC_READ_UINT(_spacc_dev.reg.dst_ptr) & SPACC_DST_PTR_PTR) != (0x1234567F & SPACC_DST_PTR_PTR) ) ||
	( (ELPSEC_READ_UINT(_spacc_dev.reg.src_ptr) & SPACC_SRC_PTR_PTR) != (0xDEADBEEF & SPACC_SRC_PTR_PTR) )) {
			DPRINTF (ELP_DBG, "DST_PTR: %X : %X\n",(U32)_spacc_dev.reg.dst_ptr, ELPSEC_READ_UINT(_spacc_dev.reg.dst_ptr));
			DPRINTF (ELP_DBG, "SRC_PTR: %X : %X\n",(U32)_spacc_dev.reg.src_ptr, ELPSEC_READ_UINT(_spacc_dev.reg.src_ptr));
			goto ERR;
	}
#endif
	for (x = 0; x < (1U<<SPACC_SW_ID_ID_W); x++) {
		_spacc_dev.job_pool[x][0] = 0xFFFFFFFF;
		_spacc_dev.job_pool[x][1] = 0xFFFFFFFF;
	}

	for (x = 0; x < CRYPTO_CONTEXTS_MAX; x++) {
		_spacc_dev.ctx[x].ciph_key = (U32*)((U32)_spacc_dev.reg.ciph_key + x*SPACC_CTX_CIPH_PAGE);
		_spacc_dev.ctx[x].hash_key = (U32*)((U32)_spacc_dev.reg.hash_key + x*SPACC_CTX_HASH_PAGE);
		_spacc_dev.ctx[x].rc4_key  = (U32*)((U32)_spacc_dev.reg.rc4_key  + x*SPACC_CTX_RC4_PAGE);
		_spacc_dev.ctx[x].job_idx   = 0;

		for (y = 0; y < MAX_JOBS; y++) {
			_spacc_dev.ctx[x].job_id[y] = 0xFFFFFFFF;
		}
		_spacc_dev.ctx[x].job_err  = SPACC_CRYPTO_INPROGRESS;
		_spacc_dev.ctx[x].ddt      = MEM_ALLOC_MAP (NULL, DDT_ENTRY_SIZE, &_spacc_dev.ctx[x].ddt_map);
		if (_spacc_dev.ctx[x].ddt == NULL) {
			goto ERR;
		}

		DPRINTF (ELP_DBG, "spacc_init: MEM_ALLOC_MAP ddt[%d]@ %p %d entries\n",x, _spacc_dev.ctx[x].ddt_map, MAX_DDT_ENTRIES);

		// allocate physical mem for the ops
		_spacc_dev.ctx[x].sddt = MEM_ALLOC_MAP (NULL, DDT_ENTRY_SIZE, &_spacc_dev.ctx[x].sddt_map);
		if (_spacc_dev.ctx[x].sddt == NULL) {
			goto ERR;
		}
		DPRINTF (ELP_DBG, "spacc_init: MEM_ALLOC_MAP sddt[%d]@ %p %d entries\n", x, _spacc_dev.ctx[x].sddt_map, MAX_DDT_ENTRIES);
	}

	return SPACC_CRYPTO_OK;
ERR:
	spacc_fini ();

	DPRINTF(ELP_ERR, "%s: %d SPACC_CRYPTO_FAILED\n", __FUNCTION__,__LINE__);

	return SPACC_CRYPTO_FAILED;
}

void spacc_fini (void)
{
	int x;

	for (x = 0; x < CRYPTO_CONTEXTS_MAX; x++) {
		if (_spacc_dev.ctx[x].ddt) {
			MEM_FREE_MAP (NULL, DDT_ENTRY_SIZE, _spacc_dev.ctx[x].ddt, (dma_addr_t) _spacc_dev.ctx[x].ddt_map);
			DPRINTF (ELP_DBG, "spacc_fini: MEM_FREE_MAP ddt[%d]@ %X\n", x, (int) _spacc_dev.ctx[x].ddt);
		}
		_spacc_dev.ctx[x].ddt = NULL;
		if (_spacc_dev.ctx[x].sddt) {
			MEM_FREE_MAP (NULL, DDT_ENTRY_SIZE, _spacc_dev.ctx[x].sddt, (dma_addr_t) _spacc_dev.ctx[x].sddt_map);
			DPRINTF (ELP_DBG, "spacc_fini: MEM_FREE_MAP sddt[%d]@ %X\n", x, (int) _spacc_dev.ctx[x].sddt);
		}
		_spacc_dev.ctx[x].sddt = NULL;
	}
}

// Return a context structure for a handle
// or null if invalid
elp_spacc_handle_ctx *context_lookup (S32 handle)
{
	elp_spacc_handle_ctx *ctx = NULL;
	unsigned long flags;

	spin_lock_irqsave(&elp_context_spinlock, flags);
	if ((_module_initialized == 0)
		|| (handle < 0)
		|| (handle > CRYPTO_CONTEXTS_MAX)
		|| (_spacc_dev.ctx[handle].taken == 0) ) {
		ctx = NULL;
	} else {
		ctx = &_spacc_dev.ctx[handle];
	}
	spin_unlock_irqrestore(&elp_context_spinlock, flags);

	return ctx;
}

// Releases a crypto context back into appropriate module's pool
S32 spacc_close (S32 handle, struct elp_softc *sc)
{
	S32 ret = SPACC_CRYPTO_OK;

	ret = spacc_release_ddt (handle, sc);
	if (ret == SPACC_CRYPTO_OK) {
		ret = spacc_release_dst_ddt(handle, sc);
	}

	if (free_handle (handle) != handle) {
		ret = SPACC_CRYPTO_INVALID_HANDLE;
	}

	return ret;
}

// Allocates a handle for spacc module context and initialize it with
// an appropriate type. Here we receive encryption and hashing algos
// as defined by the OCF layer. So we need to remap theses definitions
// to match the elliptic ones.
/*
#define CRYPTO_DES_CBC			1
#define CRYPTO_3DES_CBC			2
#define CRYPTO_BLF_CBC			3
#define CRYPTO_CAST_CBC			4
#define CRYPTO_SKIPJACK_CBC		5
#define CRYPTO_MD5_HMAC			6
#define CRYPTO_SHA1_HMAC		7
#define CRYPTO_RIPEMD160_HMAC		8
#define CRYPTO_MD5_KPDK			9
#define CRYPTO_SHA1_KPDK		10
#define CRYPTO_RIJNDAEL128_CBC		11 
#define CRYPTO_AES_CBC			11 
#define CRYPTO_ARC4			12
#define CRYPTO_MD5			13
#define CRYPTO_SHA1			14
#define CRYPTO_NULL_HMAC		15
#define CRYPTO_NULL_CBC			16
#define CRYPTO_DEFLATE_COMP		17 
#define CRYPTO_SHA2_256_HMAC		18
#define CRYPTO_SHA2_384_HMAC		19
#define CRYPTO_SHA2_512_HMAC		20
#define CRYPTO_CAMELLIA_CBC		21
#define CRYPTO_SHA2_256			22
#define CRYPTO_SHA2_384			23
#define CRYPTO_SHA2_512			24
#define CRYPTO_RIPEMD160		25
*/

/*
 Allocates a handle for spacc module context and initialize it with an
 appropriate type. the ctxid_skip param is used to skip a "rezerved" ctx id,
 i.e. when using spacc with a Effective Master Key(EMK), the EMK context must
 not be used. ctxid_skip == -1 indicates no ctx id should be skipped.
 */

S32 spacc_open (S32 enc, S32 hash,  S32 ctxid, S32 ctxid_skip, UINT rc4len, UINT first_packet)
{
	S32 ret = SPACC_CRYPTO_OK;
	S32 handle = 0;
	S32 elpenc = CRYPTO_MODE_NULL;
	S32 elphash = CRYPTO_MODE_NULL;
	elp_spacc_handle_ctx *ctx = NULL;
	U32 ctrl = 0;
	int y;

	DPRINTF(ELP_DBG, "enc=%d hash=%d\n", enc, hash);

	if ((handle = alloc_handle (ctxid, ctxid_skip)) < 0) {
		DPRINTF(ELP_WRN, "alloc_handle() failed\n");
		ret = handle;
		goto err_out;
	} else {
		ctx = context_lookup (handle);
		ctx->icv_len = 0;
	}

	// Expand the key on the first use of the context
	// Should be reset after the first call
	if(enc == CRYPTO_ARC4) {
		if(first_packet)
			ctrl |= CTRL_SET_KEY_EXP;
	}
	else
		ctrl |= CTRL_SET_KEY_EXP;
	switch (enc) {
		case CRYPTO_NULL_CBC:
		case CRYPTO_MODE_NULL:
			elpenc = CRYPTO_MODE_NULL;
			break;

		case CRYPTO_ARC4:
			if(rc4len == 40)
			{
				elpenc = CRYPTO_MODE_RC4_40;
			}
			else if(rc4len == 128)
			{
				elpenc = CRYPTO_MODE_RC4_128;
			}
			ctrl |= CTRL_SET_CIPH_ALG (C_RC4);
			break;

		case CRYPTO_AES_CBC:
			elpenc = CRYPTO_MODE_AES_CBC;
			ctrl |= CTRL_SET_CIPH_ALG (C_AES);
			ctrl |= CTRL_SET_CIPH_MODE (CM_CBC);
			break;

		case CRYPTO_3DES_CBC:
			elpenc = CRYPTO_MODE_3DES_CBC;
			ctrl |= CTRL_SET_CIPH_ALG (C_DES);
			ctrl |= CTRL_SET_CIPH_MODE (CM_CBC);
			break;

		case CRYPTO_DES_CBC:
			elpenc = CRYPTO_MODE_DES_CBC;
			ctrl |= CTRL_SET_CIPH_ALG (C_DES);
			ctrl |= CTRL_SET_CIPH_MODE (CM_CBC);
			break;

		default:
			DPRINTF(ELP_ERR, "%s: CRYPTO_INVALID_EALG\n", __FUNCTION__);
			ret = SPACC_CRYPTO_INVALID_ALG;
			goto err_out1;
	}

	switch (hash) {
		case CRYPTO_NULL_HMAC:
		case CRYPTO_MODE_NULL:
			elphash = CRYPTO_MODE_NULL;
			ctrl |= CTRL_SET_HASH_ALG (H_NULL);
			break;

		case CRYPTO_SHA1:
			elphash = CRYPTO_MODE_HASH_SHA1;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA1);
			ctrl |= CTRL_SET_HASH_MODE (HM_RAW);
			ctx->icv_len = CRYPTO_HASH_SHA1_SIZE;
			break;

		case CRYPTO_SHA1_HMAC:
			elphash = CRYPTO_MODE_HMAC_SHA1;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA1);
			ctrl |= CTRL_SET_HASH_MODE (HM_HMAC);
			ctx->icv_len = CRYPTO_HASH_SHA1_SIZE;
			break;

		case CRYPTO_MD5_HMAC:
			elphash = CRYPTO_MODE_HMAC_MD5;
			ctrl |= CTRL_SET_HASH_ALG (H_MD5);
			ctrl |= CTRL_SET_HASH_MODE (HM_HMAC);
			ctx->icv_len = CRYPTO_HASH_MD5_SIZE;
			break;

		case CRYPTO_SHA2_256_HMAC:
			elphash = CRYPTO_MODE_HMAC_SHA256;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA256);
			ctrl |= CTRL_SET_HASH_MODE (HM_HMAC);
			ctx->icv_len = CRYPTO_HASH_SHA256_SIZE;
			break;

		case CRYPTO_SHA2_384_HMAC:
			elphash = CRYPTO_MODE_HMAC_SHA384;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA384);
			ctrl |= CTRL_SET_HASH_MODE (HM_HMAC);
			ctx->icv_len = CRYPTO_HASH_SHA384_SIZE;
			break;
		case CRYPTO_SHA2_512_HMAC:
			elphash = CRYPTO_MODE_HMAC_SHA512;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA512);
			ctrl |= CTRL_SET_HASH_MODE (HM_HMAC);
			ctx->icv_len = CRYPTO_HASH_SHA512_SIZE;
			break;

		case CRYPTO_MD5:
			elphash = CRYPTO_MODE_HASH_MD5;
			ctrl |= CTRL_SET_HASH_ALG (H_MD5);
			ctrl |= CTRL_SET_HASH_MODE (HM_RAW);
			ctx->icv_len = CRYPTO_HASH_MD5_SIZE;
			break;

		case CRYPTO_SHA2_256:
			elphash = CRYPTO_MODE_HASH_SHA256;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA256);
			ctrl |= CTRL_SET_HASH_MODE (HM_RAW);
			ctx->icv_len = CRYPTO_HASH_SHA256_SIZE;
			break;

		case CRYPTO_SHA2_384:
			elphash = CRYPTO_MODE_HASH_SHA384;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA384);
			ctrl |= CTRL_SET_HASH_MODE (HM_RAW);
			ctx->icv_len = CRYPTO_HASH_SHA384_SIZE;
			break;

		case CRYPTO_SHA2_512:
			elphash = CRYPTO_MODE_HASH_SHA512;
			ctrl |= CTRL_SET_HASH_ALG (H_SHA512);
			ctrl |= CTRL_SET_HASH_MODE (HM_RAW);
			ctx->icv_len = CRYPTO_HASH_SHA512_SIZE;
			break;

		default:
			DPRINTF(ELP_ERR, "CRYPTO_INVALID_AALG\n");
			ret = SPACC_CRYPTO_INVALID_ALG;
			break;
	}

	ctrl |= (1UL<<_SPACC_CTRL_MSG_BEGIN)|(1UL<<_SPACC_CTRL_MSG_END);

err_out1:
	if (ret != SPACC_CRYPTO_OK) {
		free_handle (handle);
	} else {
		ret            = handle;
		ctx->first_use = 1;
		ctx->enc_mode  = elpenc;
		ctx->hash_mode = elphash;
		ctx->ckey_sz   = 0;
		ctx->hkey_sz   = 0;
		ctx->job_done  = 0;
		ctx->job_idx    = 0;
		for (y = 0; y < MAX_JOBS; y++) {
			ctx->job_id[y] = 0xFFFFFFFF;
		}
		ctx->job_err   = SPACC_CRYPTO_INPROGRESS;
		ctx->ctrl      = ctrl | CTRL_SET_CTX_IDX(handle);
	}

err_out:
	return ret;
}

// return a software handle back to the pool
// if failed return -1 else tha same handle value
S32 free_handle (S32 handle)
{
	S32 ret = -1;

	spin_lock_bh(&elp_context_spinlock);
	if ((handle >= 0) && (handle < CRYPTO_CONTEXTS_MAX)) {
		if (_spacc_dev.ctx[handle].taken == 1) {
			_spacc_dev.ctx[handle].taken = 0;
			ret = handle;
		}
	}
	spin_unlock_bh(&elp_context_spinlock);
	return ret;
}

// allocate a software context handle
S32 alloc_handle (S32 ctx, S32 ctx_skip)
{
	U8 i;
	S32 ret = -1;

	ret = SPACC_CRYPTO_FAILED;
	i   = 0;

	spin_lock_bh(&elp_context_spinlock);

	if (ctx >= 0) {
		if (_spacc_dev.ctx[ctx].taken == 0) {
			_spacc_dev.ctx[ctx].taken = 1;
			ret = ctx;
		}
	} else {
		for (i = 0; i < CRYPTO_CONTEXTS_MAX; i++) {
			if (i != ctx_skip) {
				if (_spacc_dev.ctx[i].taken == 0) {
					_spacc_dev.ctx[i].taken = 1;
					ret = i;
					break;
				}
			}
		}
	}
	spin_unlock_bh(&elp_context_spinlock);

	return ret;
}

// Universal function to initialize a crypto module with an appropreate context
// data which is actually depends on module functionality
S32 spacc_set_context (S32 handle, S32 op, U8 * key, S32 ksz, U8 * iv, S32 ivsz)
{
	S32 ret = SPACC_CRYPTO_OK;
	elp_spacc_handle_ctx *ctx = NULL;
	//int i;

	ctx = context_lookup (handle);

	if(!ksz) //Will it happen this?
		DPRINTF(ELP_DBG, "%s: HASH KEY not provided\n", __FUNCTION__);

	if (ctx == NULL) {
		ret = SPACC_CRYPTO_FAILED;
	} else {
		switch (op) {
		case SPACC_CRYPTO_OPERATION:
			switch (ctx->enc_mode) {
				case CRYPTO_MODE_RC4_40:
					MEMCPY32 (ctx->ciph_key, key, 2);
					break;
				case CRYPTO_MODE_RC4_128:
					MEMCPY32 (ctx->ciph_key, key, 4);
					break;
				case CRYPTO_MODE_AES_ECB:
				case CRYPTO_MODE_AES_CBC:
				case CRYPTO_MODE_AES_CTR:
				case CRYPTO_MODE_AES_CCM:
				case CRYPTO_MODE_AES_GCM:
					if (key) {
						MEMCPY32 (ctx->ciph_key, key, ksz >> 2);
						ctx->first_use = 1;
					}
					if (iv) {
						unsigned char one[4] = { 0, 0, 0, 1 };
						MEMCPY32 (&ctx->ciph_key[8], iv, ivsz >> 2);
						if (ivsz == 12 && ctx->enc_mode == CRYPTO_MODE_AES_GCM) {
							MEMCPY32 (&ctx->ciph_key[11], &one, 1);
						}
					}
					break;
				case CRYPTO_MODE_AES_F8:
					if (key) {
						MEMCPY32 (ctx->ciph_key, key + ksz, ksz >> 2);
						MEMCPY32 (&ctx->ciph_key[12], key, ksz >> 2);
					}
					if (iv)
						MEMCPY32 (&ctx->ciph_key[8], iv, ivsz >> 2);
					break;
				case CRYPTO_MODE_AES_XTS:
					if (key) {
						MEMCPY32(ctx->ciph_key, key, ksz >> 3); // divide by 4 bytes per word then in half
						MEMCPY32(&ctx->ciph_key[8+4], key + (ksz>>1), ksz >> 3);
						ksz = ksz >> 1; // divide by two since that's what we program the hardware with
					}
					if (iv) {
						MEMCPY32(&ctx->ciph_key[8], iv, 4);
					}
					break;

				case CRYPTO_MODE_MULTI2_ECB:
				case CRYPTO_MODE_MULTI2_CBC:
				case CRYPTO_MODE_MULTI2_OFB:
				case CRYPTO_MODE_MULTI2_CFB:
					// Number of rounds at the end of the IV
					if (key)
						MEMCPY32 (ctx->ciph_key, key, ksz >> 2);
					if (iv)
						MEMCPY32 (&ctx->ciph_key[10], iv, ivsz >> 2);
					if (ivsz == 0) {
						ctx->ciph_key[0x30/4] = 0x80000000; // default to 128 rounds
					}
					break;

				case CRYPTO_MODE_3DES_CBC:
				case CRYPTO_MODE_3DES_ECB:
				case CRYPTO_MODE_DES_CBC:
				case CRYPTO_MODE_DES_ECB:
					if (iv)
						MEMCPY32 (ctx->ciph_key, iv, ivsz >> 2);
					if (key)
						MEMCPY32 (&ctx->ciph_key[2], key, ksz >> 2);
					break;

				case CRYPTO_MODE_KASUMI_ECB:
				case CRYPTO_MODE_KASUMI_F8:
					if (iv)
						MEMCPY32 (&ctx->ciph_key[4], iv, 2);
					if (key)
						MEMCPY32 (ctx->ciph_key, key, 4);
					break;

				case CRYPTO_MODE_NULL:
				default:

					break;
			}

			// Set context might be called only to set iv separately
			// so only when having a key set the key size
			if (key) {
				//ctx->ctrl    |= CTRL_SET_KEY_EXP;
				ctx->ckey_sz  = SPACC_SET_CIPHER_KEY_SZ (ksz, handle);
			}
			break;

		case SPACC_HASH_OPERATION:
			switch (ctx->hash_mode) {
				case CRYPTO_MODE_MAC_XCBC:
					MEMCPY32(&ctx->hash_key[8], key + (ksz - 32), 32/4);
					MEMCPY32(&ctx->hash_key[0], key, (ksz - 32)/4);
					ctx->hkey_sz = SPACC_SET_HASH_KEY_SZ (ksz - 32, handle);
					break;
				case CRYPTO_MODE_HASH_CRC32:
					if (iv) {
						MEMCPY32 (&ctx->hash_key[0], iv, ivsz >> 2);
					}
					ctx->hkey_sz = SPACC_SET_HASH_KEY_SZ (ksz, handle);
					break;
				case CRYPTO_MODE_MAC_SNOW3G_UIA2:
					MEMCPY32 (ctx->hash_key, key, ksz >> 2);
					if (iv) {
						MEMCPY32 (&ctx->hash_key[4], iv, ivsz >> 2);
					}
					ctx->hkey_sz = SPACC_SET_HASH_KEY_SZ (ksz, handle);
					break;
				default:
					if(ksz) {
						ctx->hkey_sz = SPACC_SET_HASH_KEY_SZ (ksz, handle);
						MEMCPY32 (ctx->hash_key, key, (ksz+3) >> 2); //allow for odd sized keys
					}
					break;
			}
			break;
		default:
			ret = SPACC_CRYPTO_INVALID_MODE;
			break;
		}
	}
	return ret;
}

S32 spacc_set_operation (S32 handle, S32 op, U32 prot, U32 icvpos, U32 icvoff, U32 icvsz)
{
	S32 ret = SPACC_CRYPTO_OK;
	elp_spacc_handle_ctx *ctx = NULL;

	ctx = context_lookup (handle);
	if (ctx == NULL) {
		ret = SPACC_CRYPTO_FAILED;
	} else {
		if (op == OP_ENCRYPT) {
			ctx->op = OP_ENCRYPT;
			ctx->ctrl |= CTRL_SET_ENCRYPT;
		} else {
			ctx->op = OP_DECRYPT;
		}

		switch (prot) {
			case IM_ICV_HASH:          /* HASH of plaintext */
				ctx->ctrl |= CTRL_SET_ICV_PT;
				break;
			case IM_ICV_HASH_ENCRYPT:  /* HASH the plaintext and encrypt the lot */
				ctx->ctrl |= CTRL_SET_ICV_PT;
				ctx->ctrl |= CTRL_SET_ICV_ENC;
				ctx->ctrl |= CTRL_SET_ICV_APPEND;
				break;
			case IM_ICV_ENCRYPT_HASH:  /* HASH the ciphertext */
				ctx->ctrl &= ~(CTRL_SET_ICV_PT); // ICV_PT=0
				ctx->ctrl &= ~(CTRL_SET_ICV_ENC); // ICV_ENC=0
				break;
			case IM_ICV_IGNORE:
				break;
			default:
				ret = SPACC_CRYPTO_INVALID_MODE;
				break;
		}

		ctx->icv_len = icvsz;

		switch (icvpos) {
			case IP_ICV_OFFSET:
				ctx->icv_offset = icvoff;
				break;
			case IP_ICV_APPEND:
				ctx->ctrl |= CTRL_SET_ICV_APPEND;
				break;
			case IP_ICV_IGNORE:
				break;
			default:
				ret = SPACC_CRYPTO_INVALID_MODE;
				break;
		}
	}
	return ret;
}

S32 spacc_add_ddt (S32 handle, U8 * data, U32 len, struct elp_softc *sc)
{
	S32 ret = SPACC_CRYPTO_OK;
	elp_spacc_handle_ctx *ctx = NULL;
	struct platform_device *pdev = sc->sc_dev;

	// NOP so let's just skip it
	if (data == NULL || len == 0) return SPACC_CRYPTO_OK;

	ctx = context_lookup (handle);
	if (NULL == ctx) {
		DPRINTF(ELP_ERR, "spacc_add_ddt: context_lookup failed\n");
		ret = SPACC_CRYPTO_FAILED;
	} else {
		U32 newlen;

		/* split things larger than the max particle size */
		if (len > SPACC_MAX_PARTICLE_SIZE) {
			newlen = len - SPACC_MAX_PARTICLE_SIZE;
			len    = SPACC_MAX_PARTICLE_SIZE;
		} else {
			newlen = 0;
		}

		if ((ctx->ddt_desc[ctx->ddt_idx].map = MEM_MAP (&pdev->dev, data, len)) == NULL) {
			DPRINTF (ELP_ERR, "spacc_add_ddt: MEM_MAP failed for data %u bytes\n", len);
			return SPACC_CRYPTO_FAILED;
		}

		DPRINTF (ELP_DBG, "spacc_add_ddt:%d: ddt_idx=%d @%X %u bytes\n", handle, ctx->ddt_idx, (U32)ctx->ddt_desc[ctx->ddt_idx].map, len);

		ctx->ddt_desc[ctx->ddt_idx].buf = data;
		ctx->ddt_desc[ctx->ddt_idx].len = len;
		ctx->ddt[ctx->ddt_idx].ptr = (U32) ctx->ddt_desc[ctx->ddt_idx].map;
		ctx->ddt[ctx->ddt_idx].len = (U32) len;
		ctx->ddt[ctx->ddt_idx+1].ptr = (U32)NULL;
		ctx->ddt[ctx->ddt_idx+1].len = 0;
		ctx->ddt_len += len;
	 	ctx->ddt_idx++;

		/* if there are remaining bytes, recurse and add it as a new DDT entry */
		if (newlen != 0) {
			return spacc_add_ddt(handle, data + SPACC_MAX_PARTICLE_SIZE, newlen, sc);
		}
	}
	return ret;
}

S32 spacc_release_ddt (S32 handle, struct elp_softc *sc)
{
	int i;
	S32 ret = SPACC_CRYPTO_OK;
	struct platform_device *pdev = sc->sc_dev;

	elp_spacc_handle_ctx *ctx = context_lookup (handle);
	if (NULL == ctx) {
		return SPACC_CRYPTO_INVALID_HANDLE;
	}

	for (i = 0; i < ctx->ddt_idx; i++) {
		if ((ctx->ddt_desc[i].map == NULL) || (ctx->ddt_desc[i].len == 0)) {
			DPRINTF(ELP_ERR, "spacc_release_ddt:%d: Invalid entry %d map=%X len=%d\n",
			(int) handle, i, (int) ctx->ddt_desc[i].map,
			(int) ctx->ddt_desc[i].len);
		} else {
/*			DPRINTF(ELP_DBG,"spacc_release_ddt:%d: ddt_idx=%d @ %lX %lu bytes\n",
			handle, i, (U32)ctx->ddt_desc[i].map, ctx->ddt_desc[i].len);
*/

			MEM_UNMAP (&pdev->dev, ctx->ddt_desc[i].map, ctx->ddt_desc[i].len);

			ctx->ddt_desc[i].map = 0;
			ctx->ddt_desc[i].buf = 0;
			ctx->ddt_desc[i].len = 0;
			ctx->ddt[i].ptr = 0;
			ctx->ddt[i].len = 0;
		}
	}
	ctx->ddt_idx = 0;
	ctx->ddt_len = 0;

	return ret;
}

S32 spacc_add_dst_ddt (S32 handle, U8 * data, U32 len, struct elp_softc *sc, bool same_buff)
{
	S32 ret = SPACC_CRYPTO_OK;
	elp_spacc_handle_ctx *ctx = NULL;
	struct platform_device *pdev = sc->sc_dev;

	// NOP so let's just skip it
	if (data == NULL || len == 0) return SPACC_CRYPTO_OK;

	ctx = context_lookup (handle);
	if (ctx == NULL) {
		ret = SPACC_CRYPTO_FAILED;
	} else {
		U32 newlen;

		/* split things larger than the max particle size */
		if (len > SPACC_MAX_PARTICLE_SIZE) {
			newlen = len - SPACC_MAX_PARTICLE_SIZE;
			len    = SPACC_MAX_PARTICLE_SIZE;
		} else {
			newlen = 0;
		}

		if(same_buff) { //Input and output buffers have same address so avoid DMA_MAP 
			ctx->sddt_desc[ctx->sddt_idx].map = ctx->ddt_desc[ctx->ddt_idx - 1].map;
			ctx->ddt_descr_status[ctx->sddt_idx] = 1; //Mark the status to check in release_dst_ddt to avoid DMA_UNMAP
		}
		else {
			if ((ctx->sddt_desc[ctx->sddt_idx].map = MEM_MAP (&pdev->dev, data, len)) == NULL) {
				DPRINTF (ELP_ERR, "spacc_add_dst_ddt: MEM_MAP failed for data %u bytes\n", len);
				return SPACC_CRYPTO_FAILED;
			}
		}

		DPRINTF (ELP_DBG, "spacc_add_dst_ddt:%d: ddt_idx=%d @%X %u bytes\n", handle, ctx->sddt_idx, (U32)ctx->sddt_desc[ctx->sddt_idx].map, len);

		ctx->sddt_desc[ctx->sddt_idx].buf = data;
		ctx->sddt_desc[ctx->sddt_idx].len = len;
		ctx->sddt[ctx->sddt_idx].ptr = (U32) ctx->sddt_desc[ctx->sddt_idx].map;
		ctx->sddt[ctx->sddt_idx].len = (U32) len;
		ctx->sddt[ctx->sddt_idx+1].ptr = (U32)NULL;
		ctx->sddt[ctx->sddt_idx+1].len = 0;
		ctx->sddt_len += len;
		ctx->sddt_idx++;

		/* if there are remaining bytes, recurse and add it as a new DDT entry */
		if (newlen != 0) {
			return spacc_add_dst_ddt(handle, data + SPACC_MAX_PARTICLE_SIZE, newlen, sc, same_buff);
		}

	}

	return ret;
}

S32 spacc_release_dst_ddt (S32 handle, struct elp_softc *sc)
{
	int i;
	struct platform_device *pdev = sc->sc_dev;

	elp_spacc_handle_ctx *ctx = context_lookup (handle);
	if (ctx == NULL) {
		return SPACC_CRYPTO_INVALID_HANDLE;
	}

	for (i = 0; i < ctx->sddt_idx; i++) {
		if ((ctx->sddt_desc[i].map == NULL) || (ctx->sddt_desc[i].len == 0)) {
/*			DPRINTF(ELP_DBG,"spacc_release_dst_ddt:%d: Invalid entry %d map=%X len=%d\n",
			(int) handle, i, (int) ctx->sddt_desc[i].map,
			(int) ctx->sddt_desc[i].len);
*/
		} else {
/*			DPRINTF(ELP_DBG,"spacc_release_dst_ddt:%d: ddt_idx=%d @ %lX %lu bytes\n",
			handle, i,(U32) ctx->sddt_desc[i].map, ctx->sddt_desc[i].len);
*/
			if(ctx->ddt_descr_status[i] != 1)
				MEM_UNMAP (&pdev->dev, ctx->sddt_desc[i].map, ctx->sddt_desc[i].len);

			ctx->sddt_desc[i].map = 0;
			ctx->sddt_desc[i].buf = 0;
			ctx->sddt_desc[i].len = 0;
			ctx->sddt[i].ptr = 0;
			ctx->sddt[i].len = 0;
			ctx->ddt_descr_status[i] = 0;
		}
	}
	ctx->sddt_idx = 0;
	ctx->sddt_len = 0;

	return SPACC_CRYPTO_OK;
}

S32 spacc_packet_enqueue_ddt (S32 handle, U32 proc_sz, U32 aad_offset, U32 pre_aad_sz, U32 post_aad_sz, U32 iv_offset, U32 prio)
{
	S32 ret = SPACC_CRYPTO_OK, proc_len;
	unsigned long flags;
	//U32 status;
	elp_spacc_handle_ctx *ctx = NULL;

	ctx = context_lookup (handle);
	if (NULL == ctx) {
		DPRINTF(ELP_ERR, "spacc_packet_enqueue_ddt:%d: ctx = NULL\n", handle);
		ret = SPACC_CRYPTO_FAILED;
	} else {

		if (ctx->ddt_idx == 0) {
			DPRINTF(ELP_ERR, "spacc_packet_enqueue_ddt:%d: DDT entry not valid\n", handle);
			return SPACC_CRYPTO_FAILED;
		}
		if (ctx->job_idx == MAX_JOBS) {
			DPRINTF(ELP_ERR, "spacc_packet_enqueue_ddt:%d: Too many pending jobs for this context\n", handle);
			return SPACC_CRYPTO_FAILED;
		}

		/* compute the length we must process, in decrypt mode with an ICV (hash, hmac or CCM modes)
		 * we must subtract the icv length from the buffer size
		 */
		if (proc_sz == 0) {
			if ((ctx->op == OP_DECRYPT) &&
				((ctx->hash_mode > 0) || (ctx->enc_mode == CRYPTO_MODE_AES_CCM || ctx->enc_mode == CRYPTO_MODE_AES_GCM)) &&
				!(ctx->ctrl & CTRL_SET_ICV_ENC)) {
				proc_len = ctx->ddt_len - ctx->icv_len;
			} else {
				proc_len = ctx->ddt_len;
			}
		} else {
			proc_len = proc_sz;
		}

		if (pre_aad_sz & SPACC_AADCOPY_FLAG) {
			ctx->ctrl |= CTRL_SET_AAD_COPY;
			pre_aad_sz &= ~(SPACC_AADCOPY_FLAG);
		} else {
			ctx->ctrl &= ~(CTRL_SET_AAD_COPY);
		}

		ctx->pre_aad_sz  = pre_aad_sz;
		ctx->post_aad_sz = post_aad_sz;

		spin_lock_irqsave(&reg_lock, flags);

		ELP_WRITE_UINT(_spacc_dev.reg.src_ptr,    (U32)ctx->ddt_map);
		ELP_WRITE_UINT(_spacc_dev.reg.dst_ptr,    (U32)ctx->sddt_map);
		ELP_WRITE_UINT(_spacc_dev.reg.proc_len,   proc_len);
		ELP_WRITE_UINT(_spacc_dev.reg.icv_len,    ctx->icv_len);
		ELP_WRITE_UINT(_spacc_dev.reg.icv_offset, ctx->icv_offset);
		ELP_WRITE_UINT(_spacc_dev.reg.pre_aad_len, ctx->pre_aad_sz);
		ELP_WRITE_UINT(_spacc_dev.reg.post_aad_len, ctx->post_aad_sz);
		ELP_WRITE_UINT(_spacc_dev.reg.iv_offset, iv_offset);
		ELP_WRITE_UINT(_spacc_dev.reg.aad_offset, aad_offset);

		ELP_WRITE_UINT(_spacc_dev.reg.aux_info,    (ctx->auxinfo_dir << _SPACC_AUX_INFO_DIR) | (ctx->auxinfo_bit_align << _SPACC_AUX_INFO_BIT_ALIGN));

		 if (ctx->first_use == 1) {
			ELP_WRITE_UINT(_spacc_dev.reg.key_sz,     ctx->ckey_sz);
			ELP_WRITE_UINT(_spacc_dev.reg.key_sz,     ctx->hkey_sz);
		}

		ctx->job_id[ctx->job_idx] = (ELP_READ_UINT(_spacc_dev.reg.sw_tag) >> SPACC_SW_ID_ID_O) & ((1U<<SPACC_SW_ID_ID_W)-1);
		_spacc_dev.job_pool[ctx->job_id[ctx->job_idx]][0] = handle;
		_spacc_dev.job_pool[ctx->job_id[ctx->job_idx]][1] = ctx->job_idx;

		ELP_WRITE_UINT(_spacc_dev.reg.sw_tag,  (ctx->job_id[ctx->job_idx]  << SPACC_SW_ID_ID_O) | (prio << SPACC_SW_ID_PRIO));

		//start processing...
		ELP_WRITE_UINT(_spacc_dev.reg.ctrl, ctx->ctrl);

		++(ctx->job_idx);
		spin_unlock_irqrestore(&reg_lock, flags);

#if 0
printk("%p %p %p %p\n", spacc_dev.reg.proc_len, spacc_dev.reg.pre_aad_len, spacc_dev.reg.post_aad_len, spacc_dev.reg.iv_offset);
printk("CTRL=%08x\nproc_len=%u(%u)\npre_aad_len=%u\npost_aad_len=%u\niv_offset=%u\nicv_len=%u\nicv_offset=%u HANDLE=%d\nJob ID: %u\nAUX: %x\n", ctx->ctrl, *spacc_dev.reg.proc_len, proc_len, *spacc_dev.reg.pre_aad_len, *spacc_dev.reg.post_aad_len, *spacc_dev.reg.iv_offset & 0x7FFFFFFF, ctx->icv_len, ctx->icv_offset, handle, ctx->job_id[ctx->job_idx-1],
*spacc_dev.reg.aux_info
  );
printk("IN  OFF=%u\nOUT OFF=%u\n", (aad_offset>>SPACC_OFFSET_SRC_O)&0xFFFF, (aad_offset>>SPACC_OFFSET_DST_O)&0xFFFF);
printk("ICV_OFFSET in HW=%u\n", *spacc_dev.reg.icv_offset);
printk("spacc key_sz in =%X\n", ctx->ckey_sz);
printk("spacc hkey_sz in =%X\n", ctx->hkey_sz);
printk("src_ptr: %X\n",*spacc_dev.reg.src_ptr);
printk("dst_ptr: %X\n",*spacc_dev.reg.dst_ptr);
#endif

		/*status = ELP_READ_UINT(_spacc_dev.reg.fifo_stat);
		DPRINTF (ELP_DBG, "spacc_packet_enqueue_ddt:Fifo Status:%X Commands:%u\n", status, SPACC_GET_FIFO_STAT_CNT (status));*/

		// Clear an expansion key after the first call
		if (ctx->first_use == 1) {
			ctx->first_use = 0;
			ctx->ctrl &= ~CTRL_SET_KEY_EXP;
		}
	}
	return ret;
}

// Returns a user friendly error message for a correspponding error code
U8 *spacc_error_msg (S32 err)
{
	U8 *msg = NULL;
	
	switch (err) {
	case SPACC_CRYPTO_OK:
	msg = (U8 *) "Operation has succeded";
	break;
	case SPACC_CRYPTO_FAILED:
	msg = (U8 *) "Operation has failed";
	break;
	case SPACC_CRYPTO_INPROGRESS:
	msg = (U8 *) "Operation in progress";
	break;
	case SPACC_CRYPTO_INVALID_HANDLE:
	msg = (U8 *) "Invalid handle";
	break;
	case SPACC_CRYPTO_INVALID_CONTEXT:
	msg = (U8 *) "Invalid context";
	break;
	case SPACC_CRYPTO_INVALID_SIZE:
	msg = (U8 *) "Invalid size";
	break;
	case SPACC_CRYPTO_NOT_INITIALIZED:
	msg = (U8 *) "Crypto library has not been initialized";
	break;
	case SPACC_CRYPTO_NO_MEM:
	msg = (U8 *) "No context memory";
	break;
	case SPACC_CRYPTO_INVALID_ALG:
	msg = (U8 *) "Algorithm is not supported";
	break;
	case SPACC_CRYPTO_INVALID_IV_SIZE:
	msg = (U8 *) "Invalid IV size";
	break;
	case SPACC_CRYPTO_INVALID_KEY_SIZE:
	msg = (U8 *) "Invalid key size";
	break;
	case SPACC_CRYPTO_INVALID_ARGUMENT:
	msg = (U8 *) "Invalid argument";
	break;
	case SPACC_CRYPTO_MODULE_DISABLED:
	msg = (U8 *) "Crypto module disabled";
	break;
	case SPACC_CRYPTO_NOT_IMPLEMENTED:
	msg = (U8 *) "Function is not implemented";
	break;
	case SPACC_CRYPTO_INVALID_BLOCK_ALIGNMENT:
	msg = (U8 *) "Invalid block alignment";
	break;
	case SPACC_CRYPTO_INVALID_MODE:
	msg = (U8 *) "Invalid mode";
	break;
	case SPACC_CRYPTO_INVALID_KEY:
	msg = (U8 *) "Invalid key";
	break;
	case SPACC_CRYPTO_AUTHENTICATION_FAILED:
	msg = (U8 *) "Authentication failed";
	break;
	case SPACC_CRYPTO_MEMORY_ERROR:
	msg = (U8 *) "Internal Memory Error";
	break;
	default:
	msg = (U8 *) "Invalid error code";
	break;
	}
	
	return msg;
}

/************************ TRNG ***********************************/
// init trng context
//input: spacc pdu base address 
//output: engine status
int trng_init (U32 mmap)
{
#ifdef ELP_TRNG_SELF_TEST
	int ret = 0;
	int i;
	U8 rng_buffer[16];
#endif

	DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);
	
	memset ((U8 *) & _trngm, 0, sizeof (_trngm));

	// Initialize TRNG crypto module contexts
	_trngm.active = 1;
	_trngm.status = (U32 *) (mmap + TRNG_CTRL);
	_trngm.ctrl = (U32 *) (mmap + TRNG_CTRL);
	_trngm.irq_en = (U32 *) (mmap + TRNG_IRQ_EN);
	_trngm.rand = (U32 *) (mmap + TRNG_DATA);
	_trngm.seed = (U32 *) (mmap + TRNG_DATA);
	_trngm.irq_stat = (U32 *) (mmap + TRNG_IRQ_STAT);
	
	//for now using polling mode to get trng output
#ifdef ELP_TRNG_IRQ_MODE
	trng_enable_int();
#endif

	//default setting is RAND mode
	ELP_WRITE_UINT(_trngm.ctrl,  TRNG_RAND_RESEED);

	trng_dump_registers();

#ifdef ELP_TRNG_SELF_TEST
{
	unsigned int numTRNG = 1;
	while(numTRNG--) {
		memset(rng_buffer, 0, sizeof(rng_buffer));
		if((ret = trng_get_rand (rng_buffer, 16)) !=  SPACC_CRYPTO_OK) {
			DPRINTF (ELP_ERR, "trng_get_rand : %s\n", spacc_error_msg (ret));
			DPRINTF (ELP_ERR, "trng_get_rand : ret = %d\n", ret);
			goto err;
		}
		else {
			DPRINTF(ELP_DBG, "%s: TRNG self test:\n", __FUNCTION__);
			for(i = 0; i < 16; i++)
				printk("%2x ", rng_buffer[i]);
			printk("\n");
		}
	}
}
err:
#endif	
	return SPACC_CRYPTO_OK;
}

void trng_dump_registers (void)
{
	DPRINTF (ELP_DBG, "TRNG SEED (W): TRNG0 %p TRNG1 %p TRNG2 %p TRNG3 %p\n", &_trngm.seed[0], &_trngm.seed[1], &_trngm.seed[2], &_trngm.seed[3]);

        DPRINTF (ELP_DBG, "TRNG RAND (R): RAND0 %p RAND1 %p RAND2 %p RAND3 %p\n", (void*)_trngm.rand[0], (void*)_trngm.rand[1], (void*)_trngm.rand[2], (void*)_trngm.rand[0]); 

	DPRINTF (ELP_DBG, "TRNG CTRL register: CTRL %p = %08x\n", _trngm.ctrl, ELP_READ_UINT(_trngm.ctrl));
	
	DPRINTF (ELP_DBG, "TRNG IRQ register: IRQ_EN %p = %08x - IRQ_STAT %p = %08x\n", _trngm.irq_en, ELP_READ_UINT(_trngm.irq_en), _trngm.irq_stat, ELP_READ_UINT(_trngm.irq_stat));
}

int trng_read_random(void *arg, u_int32_t *buf, int maxwords)
{
	int rc=SPACC_CRYPTO_OK;

	//DPRINTF(ELP_DBG, "%s (%p, %d)\n", __FUNCTION__, buf, maxwords);
	
	rc = trng_get_rand ((U8*)buf, maxwords);
	
	return(rc);
}
/* get a stream of random bytes */
/* input: *randbuf and it size, the size should be > 0,
         s/w supports byte access even if h/w word (4 bytes) access */
/* output: data into randbuf and returns engine status */

int trng_get_rand (U8 * randbuf, int size)
{
	int ret = SPACC_CRYPTO_OK;
	int buf[TRNG_DATA_SIZE_WORDS];
	int i;
	int n;
	
	//DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);

	if (_trngm.active == 0) {
		DPRINTF (ELP_ERR, "%s SPACC_CRYPTO_FAILED\n", __FUNCTION__);
		ret = SPACC_CRYPTO_FAILED;
	} else if ((!randbuf) || (size == 0)) {
		DPRINTF(ELP_ERR, "%s: CRYPTO_INVALID_ARGUMENT\n", __FUNCTION__);
		ret = SPACC_CRYPTO_FAILED;
	} else {
		for (; size;) {
		
			/* This tests for a reseeding operation or a new value generation */
			if (ELP_READ_UINT (_trngm.ctrl) > 0) {
				if ((ret = trng_wait_done ()) != SPACC_CRYPTO_OK) {
					DPRINTF (ELP_DBG, "%s: %d SPACC_CRYPTO_FAILED\n", __FUNCTION__,__LINE__);
					ret = SPACC_CRYPTO_FAILED;
					break;
				}
			}

			/* read out in maximum TRNG_DATA_SIZE_BYTES byte chunks */
			i = size > TRNG_DATA_SIZE_BYTES ? TRNG_DATA_SIZE_BYTES : size;

			for (n = 0; n < TRNG_DATA_SIZE_WORDS; n++) {
				buf[n] = ELP_READ_UINT (_trngm.rand + n);
			}

			memcpy (randbuf, (uint8_t *) buf, i);
			randbuf += i;
			size -= i;

			/* request next data */
			ELP_WRITE_UINT(_trngm.ctrl, ELP_READ_UINT( _trngm.ctrl)| TRNG_GET_NEW);
			if (trng_wait_done () != SPACC_CRYPTO_OK) {
				DPRINTF (ELP_DBG, "%s: %d SPACC_CRYPTO_FAILED\n", __FUNCTION__,__LINE__);
				ret = SPACC_CRYPTO_FAILED;
				break;
			}
			//dumpword ((U32*)randbuf, i, "TRNG output:");
		}
	}
	//DPRINTF (ELP_DBG, "%s done\n", __FUNCTION__);
	return ret;
}

//internal use only, used after set mode and go 
//input: void
//output: engine status
int trng_wait_done (void)
{
	int ret = SPACC_CRYPTO_OK;
	S32 wait_count = LOOP_WAIT;

//	DPRINTF (ELP_DBG, "%s\n", __FUNCTION__);
	
	// check high level status
	while ((wait_count > 0) && ((ELP_READ_UINT(_trngm.status) & TRNG_BUSY) == TRNG_BUSY)) {
		wait_count--;
	}
	
	if (wait_count == 0) {
		ret = SPACC_CRYPTO_FAILED;
		DPRINTF (ELP_ERR, "%s: %s\n", __FUNCTION__, spacc_error_msg (ret));
	}
	
#ifdef  ELP_TRNG_IRQ_MODE
	/* Ack Done bit */
	if ((ELP_READ_UINT(_trngm.status) & TRNG_BUSY) != TRNG_BUSY) {
		ELP_WRITE_UINT(_trngm.irq_stat, ELP_READ_UINT( _trngm.irq_stat)| TRNG_IRQ_DONE);
	}
#endif
	
	return ret;
}

void trng_enable_int (void)
{
	ELP_WRITE_UINT(_trngm.irq_en,  TRNG_IRQ_ENABLE);
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, ELP_READ_UINT(_spacc_dev.reg.irq_enable) | SPACC_IRQ_RNG_EN);
}

void trng_disable_int (void)
{
	ELP_WRITE_UINT(_trngm.irq_en,  0);
	ELP_WRITE_UINT(_spacc_dev.reg.irq_enable, ELP_READ_UINT(_spacc_dev.reg.irq_enable)& ~SPACC_IRQ_RNG_EN);
}

void pka_kfeed(struct elp_softc *sc)
{
	struct pka_pkq *q, *tmp = NULL;

	DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);

	if(sc == NULL)
		return;

	if (list_empty(&sc->sc_pkq) && sc->sc_pkq_cur == NULL){
		DPRINTF(ELP_WRN,"%s list_empty(&sc->sc_pkq) && sc->sc_pkq_cur == NULL\n", __FUNCTION__);	
		return;
	}
	if (sc->sc_pkq_cur != NULL) {
		DPRINTF(ELP_WRN, "%s sc->sc_pkq_cur != NULL\n", __FUNCTION__);
		return;
	}

	list_for_each_entry_safe(q, tmp, &sc->sc_pkq, pkq_list) {
		sc->sc_pkq_cur = q;
		list_del(&q->pkq_list);

		DPRINTF(ELP_DBG, "%s dequeue pka request\n", __FUNCTION__);

		if (pka_kstart(sc) != SPACC_CRYPTO_OK) {
			DPRINTF(ELP_ERR, "%s: %d  SPACC_CRYPTO_FAILED\n", __FUNCTION__, __LINE__);
			if(q!=NULL)
				crypto_kdone(q->pkq_krp);
		        if (q)
        		        kfree(q);
			sc->sc_pkq_cur = NULL;
		} else {
			if(sc->sc_pkq_cur)
				kfree(sc->sc_pkq_cur);

			sc->sc_pkq_cur = NULL;
			crypto_kdone(q->pkq_krp);
			break;
		}
	}
}

int pka_kstart(struct elp_softc *sc)
{
	struct pka_pkq *q = sc->sc_pkq_cur;

	DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);

	//inputs:	q->pkq_M (base)
	//		q->pkq_e (exponent applied to base)
	//		q->pkq_N (modulus)
	//		q->pkq_obuf (output buffer)
	//		q->pkq_result_len (expected length of modexp result)

	if (q != NULL) {
		dumpword (q->pkq_M, q->pkq_M_len, "q->pkq_M");
		dumpword (q->pkq_e, q->pkq_e_len, "q->pkq_e");
		dumpword (q->pkq_N, q->pkq_N_len, "q->pkq_N");
		DPRINTF(ELP_DBG, "q->pkq_result_len %d\n", q->pkq_result_len);
	}
 
	return clue_bn_modexp ((U8*)q->pkq_M, (U8*)q->pkq_e, (U8*)q->pkq_N, (U8*)q->pkq_obuf, q->pkq_result_len, 1);
}
int pka_copy_kparam(struct crparam *p, U32 *buf, U32 alen)
{
	unsigned char  *src = (unsigned char *)p->crp_p;
	unsigned char  *dst;
	int len, bits = p->crp_nbits;

	DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);

	if (bits > MAX_IOP_SIZE * sizeof(U32) * 8) {
		DPRINTF(ELP_ERR, "%s - ibuf too big (%d > %d)\n", __FUNCTION__, bits, MAX_IOP_SIZE * sizeof(U32) * 8);
		return -1;
	}

	len = (bits + 31) / 32;
	dst = (unsigned char *) &buf[alen/4];
	dst--;

        while (bits > 0) { /*TODO this logic can be improved */
                *dst-- = *src++;
                bits -= 8;
        }
	return 0;
}

void memset32(U32 *dst, U32 val, int len)
{
	register int i = len;

	while(i)
	{
		i--;
		dst[i]=val;
	}
}

void memset8(U8 *dst, U8 val, int len)
{
	register int i = len;

	while(i)
	{
		i--;
		dst[i]=val;
	}
}

void memcpy32(volatile U32 *dst, volatile U32 *src, int len)
{
	int i;

	for (i = len - 1; i >= 0; i--) {
		dst[i] = src[i];
	}
}

void memcpy32_r(U32 *dst, U32 *src, int len)
{
	register int i = len, j=0;
	while(i)
	{
		i--;
		dst[i]=(src[j]);
		j++;
	}

}

void memcpy32_re(U32 *dst, U32 *src, int len)
{
	register int i = len,j=0;
	while(i)
	{
		i--;
		dst[i]=htonl_e(src[j]);
		j++;
	}

}

void memcpy32htonl_r(U32 *dst, U32 *src, int len)
{
	register int i = len,j=0;
	while(i)
	{
		i--;
		dst[i]=htonl(src[j]);
		j++;
	}

}

void memcpy32htonl_r_x(U32 *dst, U32 *src, int len)
{
	register int i = len,j=0;
	U32 tmp;
	U8 k, val, *pTmp = (U8 *)&tmp;
	while(i)
	{
		i--;
		tmp =htonl(src[j]);
		for (k=0;k<4;k++)
		{
			val = pTmp[k];
			pTmp[k] = ((val & 0x0F) << 4) | ((pTmp[k]&0xF0) >> 4);
		}
		dst[i]=tmp;
		j++;
	}

}

void memcpy32htonl(U32 *dst, U32 *src, int len)
{
	register int i = len;
	while(i)
	{
		i--;
		dst[i]=htonl(src[i]);
	}

}

void dumpword(U32 *from, int size, char *msg)
{
	U32 off = 0;
	unsigned long  wlen = ((size)>>2);

	if(!(elp_debug&ELP_DUMP))
		return;

	if (size%4)
		wlen++;

	DPRINTF(ELP_DUMP, "%s: (%ld) %d bytes @ %p\n",msg?msg:" ",wlen,size,from);

	while (wlen--)
	{
		printk("0x%02X: %08X\n",off,*from);
		from++;
		off += 4;
	}
}

static ssize_t elp_show_stats(struct device *dev, struct device_attribute *attr, char *buf)
{
        ssize_t len = 0;
	struct elp_softc *sc = g_elp_sc;
	int i;

        len += sprintf(buf + len, "Sessions allocated %d\n", sc->sc_nsessions);
        len += sprintf(buf + len, "Sessions opened %d closed %d\n", 
			sc->stats.open_sessions, sc->stats.close_sessions);
	len += sprintf(buf + len, "Num of Req:\n  hash %d\n  enc  %d\n", 
			sc->stats.nr_hash_req, sc->stats.nr_enc_req);
        for (i = 0; i < 32; i++)
                len += sprintf(buf + len, "Num of requests with size  > %d Bytes = %u\n", i * 512, sc->stats.crypto_len_counters[i]);

        return len;
}

static DEVICE_ATTR(elp_stats, 0444, elp_show_stats, NULL);

int elp_sysfs_init(struct elp_softc *sc)
{
	if (device_create_file(&sc->sc_dev->dev, &dev_attr_elp_stats))
		return -1;
	return 0;
}

void elp_sysfs_exit(struct elp_softc *sc)
{
	device_remove_file(&sc->sc_dev->dev, &dev_attr_elp_stats);
}

/********************************************************************/

/* Set up the crypto device structure, private data,
 * and anything else we need before we start */
static int comcerto_elp_probe(struct platform_device *pdev)
{
	struct elp_softc *sc;
	struct resource *r;
	static int num_chips = 0;
	int err;

	DPRINTF(ELP_DBG, "%s\n", __FUNCTION__);

	/*Take spacc out of reset*/
	c2000_block_reset(COMPONENT_AXI_IPSEC_SPACC,0);

	/* Get the spacc clock */
	spacc_clk = clk_get(NULL ,"ipsec_spacc");

	if (IS_ERR(spacc_clk)) {
		pr_err("%s: Unable to obtain ipsec_spacc clock: %ld\n",__func__,PTR_ERR(spacc_clk));
		/* Elliptic driver cannot proceed from here */
		return PTR_ERR(spacc_clk);
	}

	/* Enable the IPSEC SPAcc clk  */
	err = clk_enable(spacc_clk);
	if (err){
		pr_err("%s: IPSEC SPACC clock enable failed:\n",__func__);
		/* Elliptic driver cannot proceed from here */
		return err;
	}

	sc = (struct elp_softc *) kmalloc(sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;
	memset(sc, 0, sizeof(*sc));

	softc_device_init(sc, "elp", num_chips, elp_methods);

	g_elp_sc = sc;
	sc->sc_needwakeup = 0;
	sc->sc_irq = -1;
	sc->sc_cid = -1;
	sc->sc_dev = pdev;
	INIT_LIST_HEAD(&sc->sc_pkq);

	platform_set_drvdata(sc->sc_dev, sc);

	tif.pmem  = 0;
	tif.preg  = 0;
	tif.preg2  = 0;

	/* get a pointer to the register memory */
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elp");

	sc->sc_base_addr = (ocf_iomem_t) ioremap(r->start, (r->end - r->start));
	if (!sc->sc_base_addr) {
		DPRINTF(ELP_ERR, KERN_ERR DRV_NAME ": failed to ioremap\n");
		goto out;
	}

	tif.preg = sc->sc_base_addr;
	spacc_probe();

	spin_lock_init(&reg_lock);
	spin_lock_init(&pka_pkq_lock);

	DPRINTF(ELP_DBG, "%s(): irq_mode %d\n", __FUNCTION__, irq_mode);
	/* get the irq line */
	sc->sc_irq = platform_get_irq(pdev, 0);
	if ((err = request_irq (sc->sc_irq, spacc_intr, IRQF_SHARED, "spacc", pdev))) {
		DPRINTF (ELP_ERR, "%s: failed request_irq irq %i [%d] \n", __FUNCTION__, sc->sc_irq, err);
		sc->sc_irq =  -1;
		goto out;
	} else {
		DPRINTF (ELP_DBG, "%s:request_irq:  assigned irq %i\n", __FUNCTION__, sc->sc_irq);
		spacc_pdu_enable_int(); //global IRQ enable for SPacc-PDU Core
		spacc_enable_int (); //IRQ enabled for SPacc-Raw Crypto module
		//pka_enable_int(); //IRQ enabled for PKA module
	}

	tasklet_init(&irq_spacc_tasklet, elp_spacc_tasklet, (unsigned long)sc);
	//tasklet_init(&irq_pka_tasklet, elp_pka_tasklet, (unsigned long)sc);

	sc->sc_cid = crypto_get_driverid(softc_get_device(sc), CRYPTOCAP_F_HARDWARE);
	if (sc->sc_cid < 0) {
		DPRINTF(ELP_ERR, "could not get crypto driver id\n");
		goto out;
	}

	elp_register_ocf(sc);
	if(elp_sysfs_init(sc) < 0)
		goto out;

#ifdef ELP_PKA_SELF_TEST
	pka_self_test();
#endif
	return (0);
out:
	comcerto_elp_remove(pdev);

	return -ENOMEM;

}

static int comcerto_elp_remove(struct platform_device *pdev) 
{ 
	struct elp_softc *sc = platform_get_drvdata(pdev); 

	DPRINTF(ELP_DBG, "%s() irq %d\n", __FUNCTION__, sc->sc_irq);

	elp_sysfs_exit(sc);

	tasklet_disable(&irq_spacc_tasklet);
	//tasklet_disable(&irq_pka_tasklet);

	if (sc->sc_cid >= 0) {
		crypto_unregister_all(sc->sc_cid);
#ifdef CONFIG_OCF_RANDOMHARVEST_MODULE
		crypto_runregister_all(sc->sc_cid);
#endif
	}
	spacc_remove();

	if (irq_mode) {
		if (sc->sc_irq != -1)
			free_irq(sc->sc_irq, pdev);
	}

	if (sc->sc_base_addr)
		iounmap((void *) sc->sc_base_addr);

	if(sc->sc_sessions)
		kfree(sc->sc_sessions);

	if(sc != NULL)
		kfree(sc);

	/*Disable the IPSEC SPAcc clock here*/
	clk_disable(spacc_clk);
	clk_put(spacc_clk);
	/*put IPSEC spacc into reset */
	c2000_block_reset(COMPONENT_AXI_IPSEC_SPACC,1);

	return 0;
}

#ifdef CONFIG_PM
static int comcerto_elp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct elp_softc *sc = platform_get_drvdata(pdev); 

	/* SPACC HW is going to be clock gated. Let Linux kernel OCF framework 
	 * knew about it and unregister the algos. If any user space application
	 * wants to do crypto operation it will be done in software crypto.
	 * For ex: openssl speed test.
	 * Thanks to Linux kernel , for providing API.
	*/
	if (sc->sc_cid >= 0) {
		crypto_unregister_all(sc->sc_cid);
#ifdef CONFIG_OCF_RANDOMHARVEST_MODULE
		crypto_runregister_all(sc->sc_cid);
#endif
	}
	/*Clock disable for SPACC (AXI clock domain)*/
	clk_disable(spacc_clk);
        return 0;
}

static int comcerto_elp_resume(struct platform_device *pdev)
{
	struct elp_softc *sc = platform_get_drvdata(pdev); 
	int ret=0;
	/*Clock enable for SPACC(AXI clock domain)*/
	ret = clk_enable(spacc_clk);
	if (ret){
		pr_err("%s: IPSEC SPACC clock enable failed:\n",__func__);
		return ret;
	}

	/* SPACC HW is back(clock on), So lets register it with 
	 * Kernel OCF framework again, for the HW acceleration .
	 */

	sc->sc_cid = crypto_get_driverid(softc_get_device(sc), CRYPTOCAP_F_HARDWARE);
	if (sc->sc_cid < 0) {
		pr_err("%s could not get crypto driver id\n",__func__);
		return -ENOMEM;
	}

	elp_register_ocf(sc);

        return ret;
}
#endif

/* Structure for a platform device driver */
static struct platform_driver comcerto_elp_driver = {
	.probe = comcerto_elp_probe,
	.remove = comcerto_elp_remove,
#ifdef CONFIG_PM
        .suspend = comcerto_elp_suspend,
        .resume = comcerto_elp_resume,
#endif
	.driver = {
		.name = "Elliptic-EPN1802",
	}
};

static int __init comcerto_elp_init(void)
{
	int rc;

	rc = platform_driver_register(&comcerto_elp_driver);
	DPRINTF(ELP_DBG, "init driver rc=%d\n", rc);

	return rc;
}

static void __exit comcerto_elp_exit(void)
{
	platform_driver_unregister(&comcerto_elp_driver);
	DPRINTF(ELP_DBG, "unregister elp ocf driver \n");
}

module_init(comcerto_elp_init);
module_exit(comcerto_elp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("anand.gurram@mindspeed.com");
MODULE_DESCRIPTION("OCF driver for Mindspeed M86xxx (Elliptic Crypto Offload Engine)");
