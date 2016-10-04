/*
 * Copyright (C) 2003 Christophe Saout <christophe@saout.de>
 * Copyright (C) 2004 Clemens Fruhwirth <clemens@endorphin.org>
 * Copyright (C) 2006-2009 Red Hat, Inc. All rights reserved.
 *
 * This file is released under the GPL.
 */

#include <linux/completion.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <linux/workqueue.h>
#include <linux/backing-dev.h>
#include <linux/percpu.h>
#if defined(CONFIG_SYNO_LSP_ARMADA)
#include <asm/atomic.h>
#else /* CONFIG_SYNO_LSP_ARMADA */
#include <linux/atomic.h>
#endif /* CONFIG_SYNO_LSP_ARMADA */
#include <linux/scatterlist.h>
#include <asm/page.h>
#include <asm/unaligned.h>
#if defined(CONFIG_SYNO_LSP_ARMADA)
// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/algapi.h>
#endif /* CONFIG_SYNO_LSP_ARMADA */

#include <linux/device-mapper.h>

#if defined(CONFIG_SYNO_LSP_ARMADA)
#if defined(CONFIG_OCF_DM_CRYPT)
extern int cesaReqResources[];
extern void DUMP_OCF_POOL(void);

//#define DM_DEBUG
#undef DM_DEBUG
#ifdef DM_DEBUG
#define dmprintk printk
#else
#define dmprintk(fmt,args...)
#endif

#include <../crypto/ocf/cryptodev.h>
#endif /* CONFIG_OCF_DM_CRYPT */

#define DM_MSG_PREFIX "crypt"
#define MESG_STR(x) x, sizeof(x)

extern int crypto_debug;
/*
 * per bio private data
 */
struct crypt_io {
	struct dm_target *target;
	struct bio *base_bio;
	struct work_struct work;
	atomic_t pending;
	int error;
	int post_process;
};
#else /* CONFIG_SYNO_LSP_ARMADA */
#define DM_MSG_PREFIX "crypt"
#endif /* CONFIG_SYNO_LSP_ARMADA */

/*
 * context holding the current state of a multi-part conversion
 */
#if defined(CONFIG_SYNO_LSP_ARMADA)
struct convert_context {
	struct bio *bio_in;
	struct bio *bio_out;
	unsigned int offset_in;
	unsigned int offset_out;
	unsigned int idx_in;
	unsigned int idx_out;
	sector_t sector;
	int write;
};
#else /* CONFIG_SYNO_LSP_ARMADA */
struct convert_context {
	struct completion restart;
	struct bio *bio_in;
	struct bio *bio_out;
	unsigned int offset_in;
	unsigned int offset_out;
	unsigned int idx_in;
	unsigned int idx_out;
	sector_t cc_sector;
	atomic_t cc_pending;
};

/*
 * per bio private data
 */
struct dm_crypt_io {
	struct crypt_config *cc;
	struct bio *base_bio;
	struct work_struct work;

	struct convert_context ctx;

	atomic_t io_pending;
	int error;
	sector_t sector;
	struct dm_crypt_io *base_io;
};

struct dm_crypt_request {
	struct convert_context *ctx;
	struct scatterlist sg_in;
	struct scatterlist sg_out;
	sector_t iv_sector;
};
#endif /* CONFIG_SYNO_LSP_ARMADA */

struct crypt_config;

#if defined(CONFIG_SYNO_LSP_ARMADA)
struct crypt_iv_operations {
	int (*ctr)(struct crypt_config *cc, struct dm_target *ti,
		   const char *opts);
	void (*dtr)(struct crypt_config *cc);
	const char *(*status)(struct crypt_config *cc);
	int (*generator)(struct crypt_config *cc, u8 *iv, sector_t sector);
};
#else /* CONFIG_SYNO_LSP_ARMADA */
struct crypt_iv_operations {
	int (*ctr)(struct crypt_config *cc, struct dm_target *ti,
		   const char *opts);
	void (*dtr)(struct crypt_config *cc);
	int (*init)(struct crypt_config *cc);
	int (*wipe)(struct crypt_config *cc);
	int (*generator)(struct crypt_config *cc, u8 *iv,
			 struct dm_crypt_request *dmreq);
	int (*post)(struct crypt_config *cc, u8 *iv,
		    struct dm_crypt_request *dmreq);
};

struct iv_essiv_private {
	struct crypto_hash *hash_tfm;
	u8 *salt;
};

struct iv_benbi_private {
	int shift;
};

#define LMK_SEED_SIZE 64 /* hash + 0 */
struct iv_lmk_private {
	struct crypto_shash *hash_tfm;
	u8 *seed;
};
#endif /* CONFIG_SYNO_LSP_ARMADA */

/*
 * Crypt: maps a linear range of a block device
 * and encrypts / decrypts at the same time.
 */
enum flags { DM_CRYPT_SUSPENDED, DM_CRYPT_KEY_VALID };

/*
 * Duplicated per-CPU state for cipher.
 */
struct crypt_cpu {
	struct ablkcipher_request *req;
};

#if defined(CONFIG_SYNO_LSP_ARMADA)
struct crypt_config {
	struct dm_dev *dev;
	sector_t start;

	/*
	 * pool for per bio private data and
	 * for encryption buffer pages
	 */
	mempool_t *io_pool;
	mempool_t *page_pool;
	struct bio_set *bs;

	/*
	 * crypto related data
	 */
	struct crypt_iv_operations *iv_gen_ops;
	char *iv_mode;
	union {
		struct crypto_cipher *essiv_tfm;
		int benbi_shift;
	} iv_gen_private;
	sector_t iv_offset;
	unsigned int iv_size;

	char cipher[CRYPTO_MAX_ALG_NAME];
	char chainmode[CRYPTO_MAX_ALG_NAME];
#if defined(CONFIG_OCF_DM_CRYPT)
	struct cryptoini 	cr_dm;    		/* OCF session */
	uint64_t 	 	ocf_cryptoid;		/* OCF sesssion ID */
#else
	struct crypto_blkcipher *tfm;
#endif
   unsigned long flags;
   unsigned int key_size;
   u8 key[0];
};

#define MIN_IOS        256
#define MIN_POOL_PAGES 32
#define MIN_BIO_PAGES  8

static unsigned int _crypt_requests;
static DEFINE_SPINLOCK(_crypt_lock);
static wait_queue_head_t _crypt_waitq;
static struct kmem_cache *_crypt_io_pool;

static void clone_init(struct crypt_io *, struct bio *);
#else /* CONFIG_SYNO_LSP_ARMADA */
/*
 * The fields in here must be read only after initialization,
 * changing state should be in crypt_cpu.
 */
struct crypt_config {
	struct dm_dev *dev;
	sector_t start;

	/*
	 * pool for per bio private data, crypto requests and
	 * encryption requeusts/buffer pages
	 */
	mempool_t *io_pool;
	mempool_t *req_pool;
	mempool_t *page_pool;
	struct bio_set *bs;

	struct workqueue_struct *io_queue;
	struct workqueue_struct *crypt_queue;

	char *cipher;
	char *cipher_string;

	struct crypt_iv_operations *iv_gen_ops;
	union {
		struct iv_essiv_private essiv;
		struct iv_benbi_private benbi;
		struct iv_lmk_private lmk;
	} iv_gen_private;
	sector_t iv_offset;
	unsigned int iv_size;

	/*
	 * Duplicated per cpu state. Access through
	 * per_cpu_ptr() only.
	 */
	struct crypt_cpu __percpu *cpu;

	/* ESSIV: struct crypto_cipher *essiv_tfm */
	void *iv_private;
	struct crypto_ablkcipher **tfms;
	unsigned tfms_count;

	/*
	 * Layout of each crypto request:
	 *
	 *   struct ablkcipher_request
	 *      context
	 *      padding
	 *   struct dm_crypt_request
	 *      padding
	 *   IV
	 *
	 * The padding is added so that dm_crypt_request and the IV are
	 * correctly aligned.
	 */
	unsigned int dmreq_start;

	unsigned long flags;
	unsigned int key_size;
	unsigned int key_parts;
	u8 key[0];
};

#define MIN_IOS        16
#define MIN_POOL_PAGES 32

static struct kmem_cache *_crypt_io_pool;

static void clone_init(struct dm_crypt_io *, struct bio *);
static void kcryptd_queue_crypt(struct dm_crypt_io *io);
static u8 *iv_of_dmreq(struct crypt_config *cc, struct dm_crypt_request *dmreq);

static struct crypt_cpu *this_crypt_config(struct crypt_config *cc)
{
	return this_cpu_ptr(cc->cpu);
}

/*
 * Use this to access cipher attributes that are the same for each CPU.
 */
static struct crypto_ablkcipher *any_tfm(struct crypt_config *cc)
{
	return cc->tfms[0];
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

/*
 * Different IV generation algorithms:
 *
 * plain: the initial vector is the 32-bit little-endian version of the sector
 *        number, padded with zeros if necessary.
 */
#if defined(CONFIG_SYNO_LSP_ARMADA)
// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
/*
 * plain64: the initial vector is the 64-bit little-endian version of the sector
 *        number, padded with zeros if necessary.
 */
#endif /* CONFIG_SYNO_LSP_ARMADA */
/*
 * essiv: "encrypted sector|salt initial vector", the sector number is
 *        encrypted with the bulk cipher using a salt as key. The salt
 *        should be derived from the bulk cipher's key via hashing.
 *
 * benbi: the 64-bit "big-endian 'narrow block'-count", starting at 1
 *        (needed for LRW-32-AES and possible other narrow block modes)
 *
 * null: the initial vector is always zero.  Provides compatibility with
 *       obsolete loop_fish2 devices.  Do not use for new devices.
 */
#if defined(CONFIG_SYNO_LSP_ARMADA)
// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
/*
 * lmk:  Compatible implementation of the block chaining mode used
 *       by the Loop-AES block device encryption system
 *       designed by Jari Ruusu. See http://loop-aes.sourceforge.net/
 *       It operates on full 512 byte sectors and uses CBC
 *       with an IV derived from the sector number, the data and
 *       optionally extra IV seed.
 *       This means that after decryption the first block
 *       of sector must be tweaked according to decrypted data.
 *       Loop-AES can use three encryption schemes:
 *         version 1: is plain aes-cbc mode
 *         version 2: uses 64 multikey scheme with lmk IV generator
 *         version 3: the same as version 2 with additional IV seed
 *                   (it uses 65 keys, last key is used as IV seed)
 */
#endif /* CONFIG_SYNO_LSP_ARMADA */
/*
 * plumb: unimplemented, see:
 * http://article.gmane.org/gmane.linux.kernel.device-mapper.dm-crypt/454
 */

#if defined(CONFIG_SYNO_LSP_ARMADA)
static int crypt_iv_plain_gen(struct crypt_config *cc, u8 *iv, sector_t sector)
{
	memset(iv, 0, cc->iv_size);
	*(u32 *)iv = cpu_to_le32(sector & 0xffffffff);

	return 0;
}

static int crypt_iv_essiv_ctr(struct crypt_config *cc, struct dm_target *ti,
	                      const char *opts)
{
	struct crypto_cipher *essiv_tfm;
	struct crypto_hash *hash_tfm;
	struct hash_desc desc;
	struct scatterlist sg;
	unsigned int saltsize;
	u8 *salt;
	int err;

	if (opts == NULL) {
		ti->error = "Digest algorithm missing for ESSIV mode";
		return -EINVAL;
	}

	/* Hash the cipher key with the given hash algorithm */
	hash_tfm = crypto_alloc_hash(opts, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(hash_tfm)) {
		ti->error = "Error initializing ESSIV hash";
		return PTR_ERR(hash_tfm);
	}

	saltsize = crypto_hash_digestsize(hash_tfm);
	salt = kmalloc(saltsize, GFP_KERNEL);
	if (salt == NULL) {
		ti->error = "Error kmallocing salt storage in ESSIV";
		crypto_free_hash(hash_tfm);
		return -ENOMEM;
	}

	sg_init_one(&sg, cc->key, cc->key_size);
	desc.tfm = hash_tfm;
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	err = crypto_hash_digest(&desc, &sg, cc->key_size, salt);
	crypto_free_hash(hash_tfm);

	if (err) {
		ti->error = "Error calculating hash in ESSIV";
		kfree(salt);
		return err;
	}

	/* Setup the essiv_tfm with the given salt */
	essiv_tfm = crypto_alloc_cipher(cc->cipher, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(essiv_tfm)) {
		ti->error = "Error allocating crypto tfm for ESSIV";
		kfree(salt);
		return PTR_ERR(essiv_tfm);
	}
#if  defined(CONFIG_OCF_DM_CRYPT)
	if (crypto_cipher_blocksize(essiv_tfm) != cc->iv_size) {
#else
	if (crypto_cipher_blocksize(essiv_tfm) !=
	    crypto_blkcipher_ivsize(cc->tfm)) {
#endif
		ti->error = "Block size of ESSIV cipher does "
			        "not match IV size of block cipher";
		crypto_free_cipher(essiv_tfm);
		kfree(salt);
		return -EINVAL;
	}
	err = crypto_cipher_setkey(essiv_tfm, salt, saltsize);
	if (err) {
		ti->error = "Failed to set key for ESSIV cipher";
		crypto_free_cipher(essiv_tfm);
		kfree(salt);
		return err;
	}
	kfree(salt);

	cc->iv_gen_private.essiv_tfm = essiv_tfm;
	return 0;
}

static void crypt_iv_essiv_dtr(struct crypt_config *cc)
{
	crypto_free_cipher(cc->iv_gen_private.essiv_tfm);
	cc->iv_gen_private.essiv_tfm = NULL;
}

static int crypt_iv_essiv_gen(struct crypt_config *cc, u8 *iv, sector_t sector)
{
	memset(iv, 0, cc->iv_size);
	*(u64 *)iv = cpu_to_le64(sector);
	crypto_cipher_encrypt_one(cc->iv_gen_private.essiv_tfm, iv, iv);
	return 0;
}
#else /* CONFIG_SYNO_LSP_ARMADA */
static int crypt_iv_plain_gen(struct crypt_config *cc, u8 *iv,
			      struct dm_crypt_request *dmreq)
{
	memset(iv, 0, cc->iv_size);
	*(__le32 *)iv = cpu_to_le32(dmreq->iv_sector & 0xffffffff);

	return 0;
}

static int crypt_iv_plain64_gen(struct crypt_config *cc, u8 *iv,
				struct dm_crypt_request *dmreq)
{
	memset(iv, 0, cc->iv_size);
	*(__le64 *)iv = cpu_to_le64(dmreq->iv_sector);

	return 0;
}

/* Initialise ESSIV - compute salt but no local memory allocations */
static int crypt_iv_essiv_init(struct crypt_config *cc)
{
	struct iv_essiv_private *essiv = &cc->iv_gen_private.essiv;
	struct hash_desc desc;
	struct scatterlist sg;
	struct crypto_cipher *essiv_tfm;
	int err;

	sg_init_one(&sg, cc->key, cc->key_size);
	desc.tfm = essiv->hash_tfm;
	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	err = crypto_hash_digest(&desc, &sg, cc->key_size, essiv->salt);
	if (err)
		return err;

	essiv_tfm = cc->iv_private;

	err = crypto_cipher_setkey(essiv_tfm, essiv->salt,
			    crypto_hash_digestsize(essiv->hash_tfm));
	if (err)
		return err;

	return 0;
}

/* Wipe salt and reset key derived from volume key */
static int crypt_iv_essiv_wipe(struct crypt_config *cc)
{
	struct iv_essiv_private *essiv = &cc->iv_gen_private.essiv;
	unsigned salt_size = crypto_hash_digestsize(essiv->hash_tfm);
	struct crypto_cipher *essiv_tfm;
	int r, err = 0;

	memset(essiv->salt, 0, salt_size);

	essiv_tfm = cc->iv_private;
	r = crypto_cipher_setkey(essiv_tfm, essiv->salt, salt_size);
	if (r)
		err = r;

	return err;
}

/* Set up per cpu cipher state */
static struct crypto_cipher *setup_essiv_cpu(struct crypt_config *cc,
					     struct dm_target *ti,
					     u8 *salt, unsigned saltsize)
{
	struct crypto_cipher *essiv_tfm;
	int err;

	/* Setup the essiv_tfm with the given salt */
	essiv_tfm = crypto_alloc_cipher(cc->cipher, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(essiv_tfm)) {
		ti->error = "Error allocating crypto tfm for ESSIV";
		return essiv_tfm;
	}

	if (crypto_cipher_blocksize(essiv_tfm) !=
	    crypto_ablkcipher_ivsize(any_tfm(cc))) {
		ti->error = "Block size of ESSIV cipher does "
			    "not match IV size of block cipher";
		crypto_free_cipher(essiv_tfm);
		return ERR_PTR(-EINVAL);
	}

	err = crypto_cipher_setkey(essiv_tfm, salt, saltsize);
	if (err) {
		ti->error = "Failed to set key for ESSIV cipher";
		crypto_free_cipher(essiv_tfm);
		return ERR_PTR(err);
	}

	return essiv_tfm;
}

static void crypt_iv_essiv_dtr(struct crypt_config *cc)
{
	struct crypto_cipher *essiv_tfm;
	struct iv_essiv_private *essiv = &cc->iv_gen_private.essiv;

	crypto_free_hash(essiv->hash_tfm);
	essiv->hash_tfm = NULL;

	kzfree(essiv->salt);
	essiv->salt = NULL;

	essiv_tfm = cc->iv_private;

	if (essiv_tfm)
		crypto_free_cipher(essiv_tfm);

	cc->iv_private = NULL;
}

static int crypt_iv_essiv_ctr(struct crypt_config *cc, struct dm_target *ti,
			      const char *opts)
{
	struct crypto_cipher *essiv_tfm = NULL;
	struct crypto_hash *hash_tfm = NULL;
	u8 *salt = NULL;
	int err;

	if (!opts) {
		ti->error = "Digest algorithm missing for ESSIV mode";
		return -EINVAL;
	}

	/* Allocate hash algorithm */
	hash_tfm = crypto_alloc_hash(opts, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(hash_tfm)) {
		ti->error = "Error initializing ESSIV hash";
		err = PTR_ERR(hash_tfm);
		goto bad;
	}

	salt = kzalloc(crypto_hash_digestsize(hash_tfm), GFP_KERNEL);
	if (!salt) {
		ti->error = "Error kmallocing salt storage in ESSIV";
		err = -ENOMEM;
		goto bad;
	}

	cc->iv_gen_private.essiv.salt = salt;
	cc->iv_gen_private.essiv.hash_tfm = hash_tfm;

	essiv_tfm = setup_essiv_cpu(cc, ti, salt,
				crypto_hash_digestsize(hash_tfm));
	if (IS_ERR(essiv_tfm)) {
		crypt_iv_essiv_dtr(cc);
		return PTR_ERR(essiv_tfm);
	}
	cc->iv_private = essiv_tfm;

	return 0;

bad:
	if (hash_tfm && !IS_ERR(hash_tfm))
		crypto_free_hash(hash_tfm);
	kfree(salt);
	return err;
}

static int crypt_iv_essiv_gen(struct crypt_config *cc, u8 *iv,
			      struct dm_crypt_request *dmreq)
{
	struct crypto_cipher *essiv_tfm = cc->iv_private;

	memset(iv, 0, cc->iv_size);
	*(__le64 *)iv = cpu_to_le64(dmreq->iv_sector);
	crypto_cipher_encrypt_one(essiv_tfm, iv, iv);

	return 0;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if !defined(CONFIG_SYNO_LSP_ARMADA) || (defined(CONFIG_SYNO_LSP_ARMADA) && !defined(CONFIG_OCF_DM_CRYPT))
static int crypt_iv_benbi_ctr(struct crypt_config *cc, struct dm_target *ti,
			      const char *opts)
{
#if defined(CONFIG_SYNO_LSP_ARMADA)
	unsigned int bs = crypto_blkcipher_blocksize(cc->tfm);
#else /* CONFIG_SYNO_LSP_ARMADA */
	unsigned bs = crypto_ablkcipher_blocksize(any_tfm(cc));
#endif /* CONFIG_SYNO_LSP_ARMADA */
	int log = ilog2(bs);

	/* we need to calculate how far we must shift the sector count
	 * to get the cipher block count, we use this shift in _gen */

	if (1 << log != bs) {
		ti->error = "cypher blocksize is not a power of 2";
		return -EINVAL;
	}

	if (log > 9) {
		ti->error = "cypher blocksize is > 512";
		return -EINVAL;
	}

#if defined(CONFIG_SYNO_LSP_ARMADA)
	cc->iv_gen_private.benbi_shift = 9 - log;
#else /* CONFIG_SYNO_LSP_ARMADA */
	cc->iv_gen_private.benbi.shift = 9 - log;
#endif /* CONFIG_SYNO_LSP_ARMADA */

	return 0;
}

static void crypt_iv_benbi_dtr(struct crypt_config *cc)
{
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
static int crypt_iv_benbi_gen(struct crypt_config *cc, u8 *iv, sector_t sector)
#else /* CONFIG_SYNO_LSP_ARMADA */
static int crypt_iv_benbi_gen(struct crypt_config *cc, u8 *iv,
			      struct dm_crypt_request *dmreq)
#endif /* CONFIG_SYNO_LSP_ARMADA */
{
	__be64 val;

	memset(iv, 0, cc->iv_size - sizeof(u64)); /* rest is cleared below */

#if defined(CONFIG_SYNO_LSP_ARMADA)
	val = cpu_to_be64(((u64)sector << cc->iv_gen_private.benbi_shift) + 1);
#else /* CONFIG_SYNO_LSP_ARMADA */
	val = cpu_to_be64(((u64)dmreq->iv_sector << cc->iv_gen_private.benbi.shift) + 1);
#endif /* CONFIG_SYNO_LSP_ARMADA */
	put_unaligned(val, (__be64 *)(iv + cc->iv_size - sizeof(u64)));

	return 0;
}
#endif

#if defined(CONFIG_SYNO_LSP_ARMADA)
static int crypt_iv_null_gen(struct crypt_config *cc, u8 *iv, sector_t sector)
#else /* CONFIG_SYNO_LSP_ARMADA */
static int crypt_iv_null_gen(struct crypt_config *cc, u8 *iv,
			     struct dm_crypt_request *dmreq)
#endif /* CONFIG_SYNO_LSP_ARMADA */
{
	memset(iv, 0, cc->iv_size);

	return 0;
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
static void crypt_iv_lmk_dtr(struct crypt_config *cc)
{
	struct iv_lmk_private *lmk = &cc->iv_gen_private.lmk;

	if (lmk->hash_tfm && !IS_ERR(lmk->hash_tfm))
		crypto_free_shash(lmk->hash_tfm);
	lmk->hash_tfm = NULL;

	kzfree(lmk->seed);
	lmk->seed = NULL;
}

static int crypt_iv_lmk_ctr(struct crypt_config *cc, struct dm_target *ti,
			    const char *opts)
{
	struct iv_lmk_private *lmk = &cc->iv_gen_private.lmk;

	lmk->hash_tfm = crypto_alloc_shash("md5", 0, 0);
	if (IS_ERR(lmk->hash_tfm)) {
		ti->error = "Error initializing LMK hash";
		return PTR_ERR(lmk->hash_tfm);
	}

	/* No seed in LMK version 2 */
	if (cc->key_parts == cc->tfms_count) {
		lmk->seed = NULL;
		return 0;
	}

	lmk->seed = kzalloc(LMK_SEED_SIZE, GFP_KERNEL);
	if (!lmk->seed) {
		crypt_iv_lmk_dtr(cc);
		ti->error = "Error kmallocing seed storage in LMK";
		return -ENOMEM;
	}

	return 0;
}

static int crypt_iv_lmk_init(struct crypt_config *cc)
{
	struct iv_lmk_private *lmk = &cc->iv_gen_private.lmk;
	int subkey_size = cc->key_size / cc->key_parts;

	/* LMK seed is on the position of LMK_KEYS + 1 key */
	if (lmk->seed)
		memcpy(lmk->seed, cc->key + (cc->tfms_count * subkey_size),
		       crypto_shash_digestsize(lmk->hash_tfm));

	return 0;
}

static int crypt_iv_lmk_wipe(struct crypt_config *cc)
{
	struct iv_lmk_private *lmk = &cc->iv_gen_private.lmk;

	if (lmk->seed)
		memset(lmk->seed, 0, LMK_SEED_SIZE);

	return 0;
}

static int crypt_iv_lmk_one(struct crypt_config *cc, u8 *iv,
			    struct dm_crypt_request *dmreq,
			    u8 *data)
{
	struct iv_lmk_private *lmk = &cc->iv_gen_private.lmk;
	struct {
		struct shash_desc desc;
		char ctx[crypto_shash_descsize(lmk->hash_tfm)];
	} sdesc;
	struct md5_state md5state;
	u32 buf[4];
	int i, r;

	sdesc.desc.tfm = lmk->hash_tfm;
	sdesc.desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	r = crypto_shash_init(&sdesc.desc);
	if (r)
		return r;

	if (lmk->seed) {
		r = crypto_shash_update(&sdesc.desc, lmk->seed, LMK_SEED_SIZE);
		if (r)
			return r;
	}

	/* Sector is always 512B, block size 16, add data of blocks 1-31 */
	r = crypto_shash_update(&sdesc.desc, data + 16, 16 * 31);
	if (r)
		return r;

	/* Sector is cropped to 56 bits here */
	buf[0] = cpu_to_le32(dmreq->iv_sector & 0xFFFFFFFF);
	buf[1] = cpu_to_le32((((u64)dmreq->iv_sector >> 32) & 0x00FFFFFF) | 0x80000000);
	buf[2] = cpu_to_le32(4024);
	buf[3] = 0;
	r = crypto_shash_update(&sdesc.desc, (u8 *)buf, sizeof(buf));
	if (r)
		return r;

	/* No MD5 padding here */
	r = crypto_shash_export(&sdesc.desc, &md5state);
	if (r)
		return r;

	for (i = 0; i < MD5_HASH_WORDS; i++)
		__cpu_to_le32s(&md5state.hash[i]);
	memcpy(iv, &md5state.hash, cc->iv_size);

	return 0;
}

static int crypt_iv_lmk_gen(struct crypt_config *cc, u8 *iv,
			    struct dm_crypt_request *dmreq)
{
	u8 *src;
	int r = 0;

	if (bio_data_dir(dmreq->ctx->bio_in) == WRITE) {
		src = kmap_atomic(sg_page(&dmreq->sg_in));
		r = crypt_iv_lmk_one(cc, iv, dmreq, src + dmreq->sg_in.offset);
		kunmap_atomic(src);
	} else
		memset(iv, 0, cc->iv_size);

	return r;
}

static int crypt_iv_lmk_post(struct crypt_config *cc, u8 *iv,
			     struct dm_crypt_request *dmreq)
{
	u8 *dst;
	int r;

	if (bio_data_dir(dmreq->ctx->bio_in) == WRITE)
		return 0;

	dst = kmap_atomic(sg_page(&dmreq->sg_out));
	r = crypt_iv_lmk_one(cc, iv, dmreq, dst + dmreq->sg_out.offset);

	/* Tweak the first block of plaintext sector */
	if (!r)
		crypto_xor(dst + dmreq->sg_out.offset, iv, cc->iv_size);

	kunmap_atomic(dst);
	return r;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

static struct crypt_iv_operations crypt_iv_plain_ops = {
	.generator = crypt_iv_plain_gen
};

#if defined(CONFIG_SYNO_LSP_ARMADA)
// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
static struct crypt_iv_operations crypt_iv_plain64_ops = {
	.generator = crypt_iv_plain64_gen
};
#endif /* CONFIG_SYNO_LSP_ARMADA */

static struct crypt_iv_operations crypt_iv_essiv_ops = {
	.ctr       = crypt_iv_essiv_ctr,
	.dtr       = crypt_iv_essiv_dtr,
#if defined(CONFIG_SYNO_LSP_ARMADA)
	// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
	.init      = crypt_iv_essiv_init,
	.wipe      = crypt_iv_essiv_wipe,
#endif /* CONFIG_SYNO_LSP_ARMADA */
	.generator = crypt_iv_essiv_gen
};

#if !defined(CONFIG_SYNO_LSP_ARMADA) || (defined(CONFIG_SYNO_LSP_ARMADA) && !defined(CONFIG_OCF_DM_CRYPT))
static struct crypt_iv_operations crypt_iv_benbi_ops = {
	.ctr	   = crypt_iv_benbi_ctr,
	.dtr	   = crypt_iv_benbi_dtr,
	.generator = crypt_iv_benbi_gen
};
#endif

static struct crypt_iv_operations crypt_iv_null_ops = {
	.generator = crypt_iv_null_gen
};

#if defined(CONFIG_SYNO_LSP_ARMADA)
#if defined(CONFIG_OCF_DM_CRYPT)
static void dec_pending(struct crypt_io *io, int error);

struct ocf_wr_priv {
	u32 		 	dm_ocf_wr_completed;	/* Num of wr completions */
	u32 		 	dm_ocf_wr_pending;	/* Num of wr pendings */
	wait_queue_head_t	dm_ocf_wr_queue;	/* waiting Q, for wr completion */
};

/* WARN: ordering between processes is not guaranteed due to 'wake' handling */
static int dm_ocf_wr_cb(struct cryptop *crp)
{
	struct ocf_wr_priv *ocf_wr_priv;
	unsigned long flags;

	if(crp == NULL) {
		printk("dm_ocf_wr_cb: crp is NULL!! \n");
		return 0;
	}

	ocf_wr_priv = (struct ocf_wr_priv*)crp->crp_opaque;

	ocf_wr_priv->dm_ocf_wr_completed++;

	/* if no more pending for read, wake up the read task. */
	if(ocf_wr_priv->dm_ocf_wr_completed == ocf_wr_priv->dm_ocf_wr_pending)
		wake_up(&ocf_wr_priv->dm_ocf_wr_queue);

	crypto_freereq(crp);

	spin_lock_irqsave(&_crypt_lock, flags);
	if (_crypt_requests > 0)
		_crypt_requests -= 1;
	spin_unlock_irqrestore(&_crypt_lock, flags);

	wake_up(&_crypt_waitq);
	return 0;
}

static int dm_ocf_rd_cb(struct cryptop *crp)
{
	struct crypt_io *io;
	unsigned long flags;

	if(crp == NULL) {
		printk("dm_ocf_rd_cb: crp is NULL!! \n");
		return 0;
	}

	io = (struct crypt_io *)crp->crp_opaque;

	crypto_freereq(crp);

	if(io != NULL)
		dec_pending(io, 0);

	spin_lock_irqsave(&_crypt_lock, flags);
	if (_crypt_requests > 0)
		_crypt_requests -= 1;
	spin_unlock_irqrestore(&_crypt_lock, flags);

	wake_up(&_crypt_waitq);
	return 0;
}

static inline int dm_ocf_process(struct crypt_config *cc, struct scatterlist *out,
		struct scatterlist *in, unsigned int len, u8 *iv, int iv_size, int write, void *priv)
{
	struct cryptop *crp;
	struct cryptodesc *crda = NULL;
	unsigned long flags;
	unsigned int cr;

	if(!iv) {
		printk("dm_ocf_process: only CBC mode is supported\n");
		return -EPERM;
	}

	crp = crypto_getreq(1);	 /* only encryption/decryption */
	if (!crp) {
		printk("dm_ocf_process: crypto_getreq failed!!\n");
		return -ENOMEM;
	}

	crda = crp->crp_desc;

	crda->crd_flags  = (write)? CRD_F_ENCRYPT: 0;
	crda->crd_alg    = cc->cr_dm.cri_alg;
	crda->crd_skip   = 0;
	crda->crd_len    = len;
	crda->crd_inject = 0; /* NA */
	crda->crd_klen   = cc->cr_dm.cri_klen;
	crda->crd_key    = cc->cr_dm.cri_key;

	if (iv) {
		crda->crd_flags |= (CRD_F_IV_EXPLICIT | CRD_F_IV_PRESENT);
		if( iv_size > EALG_MAX_BLOCK_LEN ) {
			printk("dm_ocf_process: iv is too big!!\n");
		}
		memcpy(&crda->crd_iv, iv, iv_size);
	}

	/* according to the current implementation the in and the out are the same buffer for read, and different for write*/
	if (sg_virt(out) != sg_virt(in)) {
		memcpy(sg_virt(out), sg_virt(in), len);
		dmprintk("dm_ocf_process: copy buffers!! \n");
	}

	dmprintk("len: %d\n",len);
	crp->crp_ilen = len; /* Total input length */
	crp->crp_flags = CRYPTO_F_CBIMM | CRYPTO_F_BATCH;
	crp->crp_buf = sg_virt(out);
	crp->crp_opaque = priv;
	if (write) {
       crp->crp_callback = dm_ocf_wr_cb;
	}
	else {
		crp->crp_callback = dm_ocf_rd_cb;
	}
	crp->crp_sid = cc->ocf_cryptoid;

	spin_lock_irqsave(&_crypt_lock, flags);
	while (crypto_dispatch(crp) != 0) {
		if (_crypt_requests == 0) {
			spin_unlock_irqrestore(&_crypt_lock, flags);
			schedule();
			spin_lock_irqsave(&_crypt_lock, flags);
		} else {
			cr = _crypt_requests;
			spin_unlock_irqrestore(&_crypt_lock, flags);
			wait_event(_crypt_waitq, _crypt_requests < cr);
			spin_lock_irqsave(&_crypt_lock, flags);
		}
	}
	_crypt_requests += 1;
	spin_unlock_irqrestore(&_crypt_lock, flags);

	return 0;
}

static inline int
ocf_crypt_convert_scatterlist(struct crypt_config *cc, struct scatterlist *out,
                          struct scatterlist *in, unsigned int length,
                          int write, sector_t sector, void *priv)
{
	u8 iv[cc->iv_size];
	int r;
	if (cc->iv_gen_ops) {
		r = cc->iv_gen_ops->generator(cc, iv, sector);
		if (r < 0)
			return r;
		r = dm_ocf_process(cc, out, in, length, iv, cc->iv_size, write, priv);
	} else {
		r = dm_ocf_process(cc, out, in, length, NULL, 0, write, priv);
	}

	return r;
}

/*
 * Encrypt / decrypt data from one bio to another one (can be the same one)
 */
static int ocf_crypt_convert(struct crypt_config *cc,
                         struct convert_context *ctx, struct crypt_io *io)
{
	int r = 0;
	long wr_timeout = 30000;
	long wr_tm;
	int num = 0;
	void *priv = NULL;
	struct ocf_wr_priv *ocf_wr_priv = NULL;
	if (ctx->write) {
		ocf_wr_priv = kmalloc(sizeof(struct ocf_wr_priv),GFP_KERNEL);
		if(!ocf_wr_priv) {
			printk("ocf_crypt_convert: out of memory \n");
			return -ENOMEM;
		}
		ocf_wr_priv->dm_ocf_wr_pending = 0;
		ocf_wr_priv->dm_ocf_wr_completed = 0;
		init_waitqueue_head(&ocf_wr_priv->dm_ocf_wr_queue);
		priv = ocf_wr_priv;
	}

	while(ctx->idx_in < ctx->bio_in->bi_vcnt &&
	      ctx->idx_out < ctx->bio_out->bi_vcnt) {
		struct bio_vec *bv_in = bio_iovec_idx(ctx->bio_in, ctx->idx_in);
		struct bio_vec *bv_out = bio_iovec_idx(ctx->bio_out, ctx->idx_out);
		struct scatterlist sg_in, sg_out;
		sg_init_table(&sg_in, 1);
		sg_set_page(&sg_in, bv_in->bv_page, 1 << SECTOR_SHIFT,
				bv_in->bv_offset + ctx->offset_in);

		sg_init_table(&sg_out, 1);
		sg_set_page(&sg_out, bv_out->bv_page, 1 << SECTOR_SHIFT,
				bv_out->bv_offset + ctx->offset_out);

		ctx->offset_in += sg_in.length;
		if (ctx->offset_in >= bv_in->bv_len) {
			ctx->offset_in = 0;
			ctx->idx_in++;
		}

		ctx->offset_out += sg_out.length;
		if (ctx->offset_out >= bv_out->bv_len) {
			ctx->offset_out = 0;
			ctx->idx_out++;
		}

		if(ctx->write) {
			num++;
		}
		/* if last read in the context - send the io, so the OCF read callback will release the IO. */
		else if(!(ctx->idx_in < ctx->bio_in->bi_vcnt && ctx->idx_out < ctx->bio_out->bi_vcnt)) {
			priv = io;
		}

		r = ocf_crypt_convert_scatterlist(cc, &sg_out, &sg_in, sg_in.length,
		                              ctx->write, ctx->sector, priv);
		if (r < 0){
			printk("ocf_crypt_convert: ocf_crypt_convert_scatterlist failed \n");
			break;
		}

		ctx->sector++;
	}

	if (ctx->write) {
		ocf_wr_priv->dm_ocf_wr_pending += num;
		wr_tm = wait_event_timeout(ocf_wr_priv->dm_ocf_wr_queue,
				(ocf_wr_priv->dm_ocf_wr_pending == ocf_wr_priv->dm_ocf_wr_completed)
									, msecs_to_jiffies(wr_timeout) );
		if (!wr_tm) {
			printk("ocf_crypt_convert: wr work was not finished in %ld msecs, %d pending %d completed.\n",
				wr_timeout, ocf_wr_priv->dm_ocf_wr_pending, ocf_wr_priv->dm_ocf_wr_completed);
		}
		kfree(ocf_wr_priv);
	}

	return r;
}

#else /* CONFIG_OCF_DM_CRYPT */

static int
crypt_convert_scatterlist(struct crypt_config *cc, struct scatterlist *out,
                          struct scatterlist *in, unsigned int length,
                          int write, sector_t sector)
{
	u8 iv[cc->iv_size] __attribute__ ((aligned(__alignof__(u64))));
	struct blkcipher_desc desc = {
		.tfm = cc->tfm,
		.info = iv,
		.flags = CRYPTO_TFM_REQ_MAY_SLEEP,
	};
	int r;
	if (cc->iv_gen_ops) {
		r = cc->iv_gen_ops->generator(cc, iv, sector);
		if (r < 0)
			return r;

		if (write)
			r = crypto_blkcipher_encrypt_iv(&desc, out, in, length);
		else
			r = crypto_blkcipher_decrypt_iv(&desc, out, in, length);
	} else {
		if (write)
			r = crypto_blkcipher_encrypt(&desc, out, in, length);
		else
			r = crypto_blkcipher_decrypt(&desc, out, in, length);
	}

	return r;
}

#endif
#else /* CONFIG_SYNO_LSP_ARMADA */
static struct crypt_iv_operations crypt_iv_lmk_ops = {
	.ctr	   = crypt_iv_lmk_ctr,
	.dtr	   = crypt_iv_lmk_dtr,
	.init	   = crypt_iv_lmk_init,
	.wipe	   = crypt_iv_lmk_wipe,
	.generator = crypt_iv_lmk_gen,
	.post	   = crypt_iv_lmk_post
};
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if defined(CONFIG_SYNO_LSP_ARMADA)
static void
crypt_convert_init(struct crypt_config *cc, struct convert_context *ctx,
                   struct bio *bio_out, struct bio *bio_in,
                   sector_t sector, int write)
#else /* CONFIG_SYNO_LSP_ARMADA */
static void crypt_convert_init(struct crypt_config *cc,
			       struct convert_context *ctx,
			       struct bio *bio_out, struct bio *bio_in,
			       sector_t sector)
#endif /* CONFIG_SYNO_LSP_ARMADA */
{
	ctx->bio_in = bio_in;
	ctx->bio_out = bio_out;
	ctx->offset_in = 0;
	ctx->offset_out = 0;
	ctx->idx_in = bio_in ? bio_in->bi_idx : 0;
	ctx->idx_out = bio_out ? bio_out->bi_idx : 0;
#if defined(CONFIG_SYNO_LSP_ARMADA)
	ctx->sector = sector + cc->iv_offset;
	ctx->write = write;
#else /* CONFIG_SYNO_LSP_ARMADA */
	ctx->cc_sector = sector + cc->iv_offset;
	init_completion(&ctx->restart);
#endif /* CONFIG_SYNO_LSP_ARMADA */
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
static struct dm_crypt_request *dmreq_of_req(struct crypt_config *cc,
					     struct ablkcipher_request *req)
{
	return (struct dm_crypt_request *)((char *)req + cc->dmreq_start);
}

static struct ablkcipher_request *req_of_dmreq(struct crypt_config *cc,
					       struct dm_crypt_request *dmreq)
{
	return (struct ablkcipher_request *)((char *)dmreq - cc->dmreq_start);
}

static u8 *iv_of_dmreq(struct crypt_config *cc,
		       struct dm_crypt_request *dmreq)
{
	return (u8 *)ALIGN((unsigned long)(dmreq + 1),
		crypto_ablkcipher_alignmask(any_tfm(cc)) + 1);
}

static int crypt_convert_block(struct crypt_config *cc,
			       struct convert_context *ctx,
			       struct ablkcipher_request *req)
{
	struct bio_vec *bv_in = bio_iovec_idx(ctx->bio_in, ctx->idx_in);
	struct bio_vec *bv_out = bio_iovec_idx(ctx->bio_out, ctx->idx_out);
	struct dm_crypt_request *dmreq;
	u8 *iv;
	int r;

	dmreq = dmreq_of_req(cc, req);
	iv = iv_of_dmreq(cc, dmreq);

	dmreq->iv_sector = ctx->cc_sector;
	dmreq->ctx = ctx;
	sg_init_table(&dmreq->sg_in, 1);
	sg_set_page(&dmreq->sg_in, bv_in->bv_page, 1 << SECTOR_SHIFT,
		    bv_in->bv_offset + ctx->offset_in);

	sg_init_table(&dmreq->sg_out, 1);
	sg_set_page(&dmreq->sg_out, bv_out->bv_page, 1 << SECTOR_SHIFT,
		    bv_out->bv_offset + ctx->offset_out);

	ctx->offset_in += 1 << SECTOR_SHIFT;
	if (ctx->offset_in >= bv_in->bv_len) {
		ctx->offset_in = 0;
		ctx->idx_in++;
	}

	ctx->offset_out += 1 << SECTOR_SHIFT;
	if (ctx->offset_out >= bv_out->bv_len) {
		ctx->offset_out = 0;
		ctx->idx_out++;
	}

	if (cc->iv_gen_ops) {
		r = cc->iv_gen_ops->generator(cc, iv, dmreq);
		if (r < 0)
			return r;
	}

	ablkcipher_request_set_crypt(req, &dmreq->sg_in, &dmreq->sg_out,
				     1 << SECTOR_SHIFT, iv);

	if (bio_data_dir(ctx->bio_in) == WRITE)
		r = crypto_ablkcipher_encrypt(req);
	else
		r = crypto_ablkcipher_decrypt(req);

	if (!r && cc->iv_gen_ops && cc->iv_gen_ops->post)
		r = cc->iv_gen_ops->post(cc, iv, dmreq);

	return r;
}

static void kcryptd_async_done(struct crypto_async_request *async_req,
			       int error);

static void crypt_alloc_req(struct crypt_config *cc,
			    struct convert_context *ctx)
{
	struct crypt_cpu *this_cc = this_crypt_config(cc);
	unsigned key_index = ctx->cc_sector & (cc->tfms_count - 1);

	if (!this_cc->req)
		this_cc->req = mempool_alloc(cc->req_pool, GFP_NOIO);

	ablkcipher_request_set_tfm(this_cc->req, cc->tfms[key_index]);
	ablkcipher_request_set_callback(this_cc->req,
	    CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
	    kcryptd_async_done, dmreq_of_req(cc, this_cc->req));
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if defined(CONFIG_SYNO_LSP_ARMADA)
#if !defined(CONFIG_OCF_DM_CRYPT)
/*
 * Encrypt / decrypt data from one bio to another one (can be the same one)
 */
static int crypt_convert(struct crypt_config *cc,
			 struct convert_context *ctx)
{
	int r = 0;
	while(ctx->idx_in < ctx->bio_in->bi_vcnt &&
	      ctx->idx_out < ctx->bio_out->bi_vcnt) {
		struct bio_vec *bv_in = bio_iovec_idx(ctx->bio_in, ctx->idx_in);
		struct bio_vec *bv_out = bio_iovec_idx(ctx->bio_out, ctx->idx_out);
		struct scatterlist sg_in, sg_out;
		sg_init_table(&sg_in, 1);
		sg_set_page(&sg_in, bv_in->bv_page, 1 << SECTOR_SHIFT,
				bv_in->bv_offset + ctx->offset_in);
		sg_init_table(&sg_out, 1);
		sg_set_page(&sg_out, bv_out->bv_page, 1 << SECTOR_SHIFT,
				bv_out->bv_offset + ctx->offset_out);

		ctx->offset_in += sg_in.length;
		if (ctx->offset_in >= bv_in->bv_len) {
			ctx->offset_in = 0;
			ctx->idx_in++;
		}

		ctx->offset_out += sg_out.length;
		if (ctx->offset_out >= bv_out->bv_len) {
			ctx->offset_out = 0;
			ctx->idx_out++;
		}

		r = crypt_convert_scatterlist(cc, &sg_out, &sg_in, sg_in.length,
		                              ctx->write, ctx->sector);
		if (r < 0)
			break;

		ctx->sector++;
	}

	return r;
}
#endif /* !CONFIG_OCF_DM_CRYPT */
#else /* CONFIG_SYNO_LSP_ARMADA */
/*
 * Encrypt / decrypt data from one bio to another one (can be the same one)
 */
static int crypt_convert(struct crypt_config *cc,
			 struct convert_context *ctx)
{
	struct crypt_cpu *this_cc = this_crypt_config(cc);
	int r;

	atomic_set(&ctx->cc_pending, 1);

	while(ctx->idx_in < ctx->bio_in->bi_vcnt &&
	      ctx->idx_out < ctx->bio_out->bi_vcnt) {

		crypt_alloc_req(cc, ctx);

		atomic_inc(&ctx->cc_pending);

		r = crypt_convert_block(cc, ctx, this_cc->req);

		switch (r) {
		/* async */
		case -EBUSY:
			wait_for_completion(&ctx->restart);
			INIT_COMPLETION(ctx->restart);
			/* fall through*/
		case -EINPROGRESS:
			this_cc->req = NULL;
			ctx->cc_sector++;
			continue;

		/* sync */
		case 0:
			atomic_dec(&ctx->cc_pending);
			ctx->cc_sector++;
			cond_resched();
			continue;

		/* error */
		default:
			atomic_dec(&ctx->cc_pending);
			return r;
		}
	}

	return 0;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if defined(CONFIG_SYNO_LSP_ARMADA)
/*
 * Generate a new unfragmented bio with the given size
 * This should never violate the device limitations
 * May return a smaller bio when running out of pages
 */
static struct bio *crypt_alloc_buffer(struct crypt_io *io, unsigned int size)
{
	struct crypt_config *cc = io->target->private;
	struct bio *clone;
	unsigned int nr_iovecs = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	gfp_t gfp_mask = GFP_NOIO;
	unsigned int i;

	clone = bio_alloc_bioset(GFP_NOIO, nr_iovecs, cc->bs);
	if (!clone)
		return NULL;

	clone_init(io, clone);

	for (i = 0; i < nr_iovecs; i++) {
		struct bio_vec *bv = bio_iovec_idx(clone, i);

		bv->bv_page = mempool_alloc(cc->page_pool, gfp_mask);
		if (!bv->bv_page)
			break;

		/*
		 * if additional pages cannot be allocated without waiting,
		 * return a partially allocated bio, the caller will then try
		 * to allocate additional bios while submitting this partial bio
		 */
		if (i == (MIN_BIO_PAGES - 1))
			gfp_mask = (gfp_mask | __GFP_NOWARN) & ~__GFP_WAIT;

		bv->bv_offset = 0;
		if (size > PAGE_SIZE)
			bv->bv_len = PAGE_SIZE;
		else
			bv->bv_len = size;

		clone->bi_size += bv->bv_len;
		clone->bi_vcnt++;
		size -= bv->bv_len;
	}

	if (!clone->bi_size) {
		bio_put(clone);
		return NULL;
	}

	return clone;
}
#else /* CONFIG_SYNO_LSP_ARMADA */
/*
 * Generate a new unfragmented bio with the given size
 * This should never violate the device limitations
 * May return a smaller bio when running out of pages, indicated by
 * *out_of_pages set to 1.
 */
static struct bio *crypt_alloc_buffer(struct dm_crypt_io *io, unsigned size,
				      unsigned *out_of_pages)
{
	struct crypt_config *cc = io->cc;
	struct bio *clone;
	unsigned int nr_iovecs = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	gfp_t gfp_mask = GFP_NOIO | __GFP_HIGHMEM;
	unsigned i, len;
	struct page *page;

	clone = bio_alloc_bioset(GFP_NOIO, nr_iovecs, cc->bs);
	if (!clone)
		return NULL;

	clone_init(io, clone);
	*out_of_pages = 0;

	for (i = 0; i < nr_iovecs; i++) {
		page = mempool_alloc(cc->page_pool, gfp_mask);
		if (!page) {
			*out_of_pages = 1;
			break;
		}

		/*
		 * If additional pages cannot be allocated without waiting,
		 * return a partially-allocated bio.  The caller will then try
		 * to allocate more bios while submitting this partial bio.
		 */
		gfp_mask = (gfp_mask | __GFP_NOWARN) & ~__GFP_WAIT;

		len = (size > PAGE_SIZE) ? PAGE_SIZE : size;

		if (!bio_add_page(clone, page, len, 0)) {
			mempool_free(page, cc->page_pool);
			break;
		}

		size -= len;
	}

	if (!clone->bi_size) {
		bio_put(clone);
		return NULL;
	}

	return clone;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

static void crypt_free_buffer_pages(struct crypt_config *cc, struct bio *clone)
{
	unsigned int i;
	struct bio_vec *bv;

#if defined(CONFIG_SYNO_LSP_ARMADA)
	for (i = 0; i < clone->bi_vcnt; i++) {
		bv = bio_iovec_idx(clone, i);
#else /* CONFIG_SYNO_LSP_ARMADA */
	bio_for_each_segment_all(bv, clone, i) {
#endif /* CONFIG_SYNO_LSP_ARMADA */
		BUG_ON(!bv->bv_page);
		mempool_free(bv->bv_page, cc->page_pool);
		bv->bv_page = NULL;
	}
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
/*
 * One of the bios was finished. Check for completion of
 * the whole request and correctly clean up the buffer.
 */
static void dec_pending(struct crypt_io *io, int error)
{
	struct crypt_config *cc = (struct crypt_config *) io->target->private;
	struct bio_vec *tovec, *fromvec;
	struct bio *bio = io->base_bio;
#ifdef CONFIG_HIGHMEM
	struct bio *origbio;
	unsigned long flags;
	char *vfrom, *vto;
	unsigned int i;
#endif /* CONFIG_HIGHMEM */

	if (error < 0)
		io->error = error;

	if (!atomic_dec_and_test(&io->pending))
		return;

#ifdef CONFIG_HIGHMEM
	if (bio_flagged(bio, BIO_BOUNCED)) {
		origbio = bio->bi_private;

		/* We have bounced bio, so copy data back if it is necessary */
		if (bio_data_dir(bio) == READ) {
			__bio_for_each_segment(tovec, origbio, i, 0) {
				fromvec = bio->bi_io_vec + i;

				/* Page not bounced */
				if (tovec->bv_page == fromvec->bv_page)
					continue;

				/*
				 * Page bounced - we have to copy data.
				 * We are using tovec->bv_offset and
				 * tovec->bv_len as originals might
				 * have been modified.
				 */
				vfrom = page_address(fromvec->bv_page) + tovec->bv_offset;
				local_irq_save(flags);

				vto = kmap_atomic(tovec->bv_page);
				memcpy(vto + tovec->bv_offset, vfrom, tovec->bv_len);
				kunmap_atomic(vto);

				local_irq_restore(flags);
			}
		}

		/* Free bounced pages */
		__bio_for_each_segment(fromvec, bio, i, 0) {
			tovec = origbio->bi_io_vec + i;

			/* Page not bounced */
			if (tovec->bv_page == fromvec->bv_page)
				continue;

			/* Page bounced: free it! */
			mempool_free(fromvec->bv_page, cc->page_pool);
		}

		/* Release our bounced bio */
		bio_put(bio);
		bio = origbio;
	}
#endif /* CONFIG_HIGHMEM */

	bio_endio(bio, io->error);
	mempool_free(io, cc->io_pool);
}

/*
 * kcryptd:
 *
 * Needed because it would be very unwise to do decryption in an
 * interrupt context.
 */
static struct workqueue_struct *_kcryptd_workqueue;
static void kcryptd_do_work(struct work_struct *work);

static void kcryptd_queue_io(struct crypt_io *io)
{
	INIT_WORK(&io->work, kcryptd_do_work);
	queue_work(_kcryptd_workqueue, &io->work);
}

static void crypt_endio(struct bio *clone, int error)
{
	struct crypt_io *io = clone->bi_private;
	struct crypt_config *cc = io->target->private;
	unsigned read_io = bio_data_dir(clone) == READ;
	/*
	 * free the processed pages, even if
	 * it's only a partially completed write
	 */
	if (!read_io)
		crypt_free_buffer_pages(cc, clone);

	if (!read_io) {
		goto out;
	}
	if (unlikely(!bio_flagged(clone, BIO_UPTODATE))) {
		error = -EIO;
		goto out;
	}
	bio_put(clone);
	io->post_process = 1;
	kcryptd_queue_io(io);
	return;

out:
	bio_put(clone);
	dec_pending(io, error);
}

static void clone_init(struct crypt_io *io, struct bio *clone)
{
	struct crypt_config *cc = io->target->private;

	clone->bi_private = io;
	clone->bi_end_io  = crypt_endio;
	clone->bi_bdev    = cc->dev->bdev;
	clone->bi_rw      = io->base_bio->bi_rw;
}
#else /* CONFIG_SYNO_LSP_ARMADA */
static struct dm_crypt_io *crypt_io_alloc(struct crypt_config *cc,
					  struct bio *bio, sector_t sector)
{
	struct dm_crypt_io *io;

	io = mempool_alloc(cc->io_pool, GFP_NOIO);
	io->cc = cc;
	io->base_bio = bio;
	io->sector = sector;
	io->error = 0;
	io->base_io = NULL;
	atomic_set(&io->io_pending, 0);

	return io;
}

static void crypt_inc_pending(struct dm_crypt_io *io)
{
	atomic_inc(&io->io_pending);
}

/*
 * One of the bios was finished. Check for completion of
 * the whole request and correctly clean up the buffer.
 * If base_io is set, wait for the last fragment to complete.
 */
static void crypt_dec_pending(struct dm_crypt_io *io)
{
	struct crypt_config *cc = io->cc;
	struct bio *base_bio = io->base_bio;
	struct dm_crypt_io *base_io = io->base_io;
	int error = io->error;

	if (!atomic_dec_and_test(&io->io_pending))
		return;

	mempool_free(io, cc->io_pool);

	if (likely(!base_io))
		bio_endio(base_bio, error);
	else {
		if (error && !base_io->error)
			base_io->error = error;
		crypt_dec_pending(base_io);
	}
}

/*
 * kcryptd/kcryptd_io:
 *
 * Needed because it would be very unwise to do decryption in an
 * interrupt context.
 *
 * kcryptd performs the actual encryption or decryption.
 *
 * kcryptd_io performs the IO submission.
 *
 * They must be separated as otherwise the final stages could be
 * starved by new requests which can block in the first stages due
 * to memory allocation.
 *
 * The work is done per CPU global for all dm-crypt instances.
 * They should not depend on each other and do not block.
 */
static void crypt_endio(struct bio *clone, int error)
{
	struct dm_crypt_io *io = clone->bi_private;
	struct crypt_config *cc = io->cc;
	unsigned rw = bio_data_dir(clone);

	if (unlikely(!bio_flagged(clone, BIO_UPTODATE) && !error))
		error = -EIO;

	/*
	 * free the processed pages
	 */
	if (rw == WRITE)
		crypt_free_buffer_pages(cc, clone);

	bio_put(clone);

	if (rw == READ && !error) {
		kcryptd_queue_crypt(io);
		return;
	}

	if (unlikely(error))
		io->error = error;

	crypt_dec_pending(io);
}

static void clone_init(struct dm_crypt_io *io, struct bio *clone)
{
	struct crypt_config *cc = io->cc;

	clone->bi_private = io;
	clone->bi_end_io  = crypt_endio;
	clone->bi_bdev    = cc->dev->bdev;
	clone->bi_rw      = io->base_bio->bi_rw;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if defined(CONFIG_SYNO_LSP_ARMADA)
static void process_read(struct crypt_io *io)
{
	struct crypt_config *cc = io->target->private;
	struct bio *base_bio = io->base_bio;
	struct bio *clone;
	sector_t sector = base_bio->bi_sector - io->target->begin;
	atomic_inc(&io->pending);

	/*
	 * The block layer might modify the bvec array, so always
	 * copy the required bvecs because we need the original
	 * one in order to decrypt the whole bio data *afterwards*.
	 */
	clone = bio_alloc_bioset(GFP_NOIO, bio_segments(base_bio), cc->bs);
	if (unlikely(!clone)) {
		dec_pending(io, -ENOMEM);
		return;
	}

	clone_init(io, clone);
	clone->bi_idx = 0;
	clone->bi_vcnt = bio_segments(base_bio);
	clone->bi_size = base_bio->bi_size;
	clone->bi_sector = cc->start + sector;
	memcpy(clone->bi_io_vec, bio_iovec(base_bio),
	       sizeof(struct bio_vec) * clone->bi_vcnt);

	generic_make_request(clone);
}

static void process_write(struct crypt_io *io)
{
	struct crypt_config *cc = io->target->private;
	struct bio *base_bio = io->base_bio;
	struct bio *clone;
	struct convert_context ctx;
	unsigned remaining = base_bio->bi_size;
	sector_t sector = base_bio->bi_sector - io->target->begin;
	atomic_inc(&io->pending);

	crypt_convert_init(cc, &ctx, NULL, base_bio, sector, 1);

	/*
	 * The allocated buffers can be smaller than the whole bio,
	 * so repeat the whole process until all the data can be handled.
	 */
	while (remaining) {
		clone = crypt_alloc_buffer(io, remaining);
		if (unlikely(!clone)) {
			dec_pending(io, -ENOMEM);
			return;
		}

		ctx.bio_out = clone;
		ctx.idx_out = 0;
#if defined(CONFIG_OCF_DM_CRYPT)
	if (unlikely(ocf_crypt_convert(cc, &ctx, io)< 0)) {
#else
		if (unlikely(crypt_convert(cc, &ctx) < 0)) {
#endif
			crypt_free_buffer_pages(cc, clone);
			bio_put(clone);
			dec_pending(io, -EIO);
			return;
		}

		/* crypt_convert should have filled the clone bio */
		BUG_ON(ctx.idx_out < clone->bi_vcnt);

		clone->bi_sector = cc->start + sector;
		remaining -= clone->bi_size;
		sector += bio_sectors(clone);

		/* Grab another reference to the io struct
		 * before we kick off the request */
		if (remaining)
			atomic_inc(&io->pending);

		generic_make_request(clone);

		/* Do not reference clone after this - it
		 * may be gone already. */

		/* out of memory -> run queues */
		if (remaining)
			congestion_wait(WRITE, HZ/100);
	}
}

static void process_read_endio(struct crypt_io *io)
{
	struct crypt_config *cc = io->target->private;
	struct convert_context ctx;
#if defined(CONFIG_OCF_DM_CRYPT)
	u32 r;
#endif

	crypt_convert_init(cc, &ctx, io->base_bio, io->base_bio,
			   io->base_bio->bi_sector - io->target->begin, 0);

#if defined(CONFIG_OCF_DM_CRYPT)
	r = ocf_crypt_convert(cc, &ctx, io);
	if (r < 0) {
		u32 rd_failed_timeout = 500;
		wait_queue_head_t dm_ocf_rd_failed_queu;

		init_waitqueue_head(&dm_ocf_rd_failed_queu);

		/* wait a bit before freeing the io, maybe few requests are still in process */
		wait_event_timeout(dm_ocf_rd_failed_queu, 0, msecs_to_jiffies(rd_failed_timeout) );

		dec_pending(io, r);

	}
#else
	dec_pending(io, crypt_convert(cc, &ctx));
#endif
}

static void kcryptd_do_work(struct work_struct *work)
{
	struct crypt_io *io = container_of(work, struct crypt_io, work);
	if (io->post_process) {
		process_read_endio(io);
	}
	else if (bio_data_dir(io->base_bio) == READ) {
		process_read(io);
	}
	else
		process_write(io);
}

#else /* CONFIG_SYNO_LSP_ARMADA */
static int kcryptd_io_read(struct dm_crypt_io *io, gfp_t gfp)
{
	struct crypt_config *cc = io->cc;
	struct bio *base_bio = io->base_bio;
	struct bio *clone;

	/*
	 * The block layer might modify the bvec array, so always
	 * copy the required bvecs because we need the original
	 * one in order to decrypt the whole bio data *afterwards*.
	 */
	clone = bio_clone_bioset(base_bio, gfp, cc->bs);
	if (!clone)
		return 1;

	crypt_inc_pending(io);

	clone_init(io, clone);
	clone->bi_sector = cc->start + io->sector;

	generic_make_request(clone);
	return 0;
}

static void kcryptd_io_write(struct dm_crypt_io *io)
{
	struct bio *clone = io->ctx.bio_out;
	generic_make_request(clone);
}

static void kcryptd_io(struct work_struct *work)
{
	struct dm_crypt_io *io = container_of(work, struct dm_crypt_io, work);

	if (bio_data_dir(io->base_bio) == READ) {
		crypt_inc_pending(io);
		if (kcryptd_io_read(io, GFP_NOIO))
			io->error = -ENOMEM;
		crypt_dec_pending(io);
	} else
		kcryptd_io_write(io);
}

static void kcryptd_queue_io(struct dm_crypt_io *io)
{
	struct crypt_config *cc = io->cc;

	INIT_WORK(&io->work, kcryptd_io);
	queue_work(cc->io_queue, &io->work);
}

static void kcryptd_crypt_write_io_submit(struct dm_crypt_io *io, int async)
{
	struct bio *clone = io->ctx.bio_out;
	struct crypt_config *cc = io->cc;

	if (unlikely(io->error < 0)) {
		crypt_free_buffer_pages(cc, clone);
		bio_put(clone);
		crypt_dec_pending(io);
		return;
	}

	/* crypt_convert should have filled the clone bio */
	BUG_ON(io->ctx.idx_out < clone->bi_vcnt);

	clone->bi_sector = cc->start + io->sector;

	if (async)
		kcryptd_queue_io(io);
	else
		generic_make_request(clone);
}

static void kcryptd_crypt_write_convert(struct dm_crypt_io *io)
{
	struct crypt_config *cc = io->cc;
	struct bio *clone;
	struct dm_crypt_io *new_io;
	int crypt_finished;
	unsigned out_of_pages = 0;
	unsigned remaining = io->base_bio->bi_size;
	sector_t sector = io->sector;
	int r;

	/*
	 * Prevent io from disappearing until this function completes.
	 */
	crypt_inc_pending(io);
	crypt_convert_init(cc, &io->ctx, NULL, io->base_bio, sector);

	/*
	 * The allocated buffers can be smaller than the whole bio,
	 * so repeat the whole process until all the data can be handled.
	 */
	while (remaining) {
		clone = crypt_alloc_buffer(io, remaining, &out_of_pages);
		if (unlikely(!clone)) {
			io->error = -ENOMEM;
			break;
		}

		io->ctx.bio_out = clone;
		io->ctx.idx_out = 0;

		remaining -= clone->bi_size;
		sector += bio_sectors(clone);

		crypt_inc_pending(io);

		r = crypt_convert(cc, &io->ctx);
		if (r < 0)
			io->error = -EIO;

		crypt_finished = atomic_dec_and_test(&io->ctx.cc_pending);

		/* Encryption was already finished, submit io now */
		if (crypt_finished) {
			kcryptd_crypt_write_io_submit(io, 0);

			/*
			 * If there was an error, do not try next fragments.
			 * For async, error is processed in async handler.
			 */
			if (unlikely(r < 0))
				break;

			io->sector = sector;
		}

		/*
		 * Out of memory -> run queues
		 * But don't wait if split was due to the io size restriction
		 */
		if (unlikely(out_of_pages))
			congestion_wait(BLK_RW_ASYNC, HZ/100);

		/*
		 * With async crypto it is unsafe to share the crypto context
		 * between fragments, so switch to a new dm_crypt_io structure.
		 */
		if (unlikely(!crypt_finished && remaining)) {
			new_io = crypt_io_alloc(io->cc, io->base_bio,
						sector);
			crypt_inc_pending(new_io);
			crypt_convert_init(cc, &new_io->ctx, NULL,
					   io->base_bio, sector);
			new_io->ctx.idx_in = io->ctx.idx_in;
			new_io->ctx.offset_in = io->ctx.offset_in;

			/*
			 * Fragments after the first use the base_io
			 * pending count.
			 */
			if (!io->base_io)
				new_io->base_io = io;
			else {
				new_io->base_io = io->base_io;
				crypt_inc_pending(io->base_io);
				crypt_dec_pending(io);
			}

			io = new_io;
		}
	}

	crypt_dec_pending(io);
}

static void kcryptd_crypt_read_done(struct dm_crypt_io *io)
{
	crypt_dec_pending(io);
}

static void kcryptd_crypt_read_convert(struct dm_crypt_io *io)
{
	struct crypt_config *cc = io->cc;
	int r = 0;

	crypt_inc_pending(io);

	crypt_convert_init(cc, &io->ctx, io->base_bio, io->base_bio,
			   io->sector);

	r = crypt_convert(cc, &io->ctx);
	if (r < 0)
		io->error = -EIO;

	if (atomic_dec_and_test(&io->ctx.cc_pending))
		kcryptd_crypt_read_done(io);

	crypt_dec_pending(io);
}

static void kcryptd_async_done(struct crypto_async_request *async_req,
			       int error)
{
	struct dm_crypt_request *dmreq = async_req->data;
	struct convert_context *ctx = dmreq->ctx;
	struct dm_crypt_io *io = container_of(ctx, struct dm_crypt_io, ctx);
	struct crypt_config *cc = io->cc;

	if (error == -EINPROGRESS) {
		complete(&ctx->restart);
		return;
	}

	if (!error && cc->iv_gen_ops && cc->iv_gen_ops->post)
		error = cc->iv_gen_ops->post(cc, iv_of_dmreq(cc, dmreq), dmreq);

	if (error < 0)
		io->error = -EIO;

	mempool_free(req_of_dmreq(cc, dmreq), cc->req_pool);

	if (!atomic_dec_and_test(&ctx->cc_pending))
		return;

	if (bio_data_dir(io->base_bio) == READ)
		kcryptd_crypt_read_done(io);
	else
		kcryptd_crypt_write_io_submit(io, 1);
}

static void kcryptd_crypt(struct work_struct *work)
{
	struct dm_crypt_io *io = container_of(work, struct dm_crypt_io, work);

	if (bio_data_dir(io->base_bio) == READ)
		kcryptd_crypt_read_convert(io);
	else
		kcryptd_crypt_write_convert(io);
}

static void kcryptd_queue_crypt(struct dm_crypt_io *io)
{
	struct crypt_config *cc = io->cc;

	INIT_WORK(&io->work, kcryptd_crypt);
	queue_work(cc->crypt_queue, &io->work);
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

/*
 * Decode key from its hex representation
 */
static int crypt_decode_key(u8 *key, char *hex, unsigned int size)
{
	char buffer[3];
#if defined(CONFIG_SYNO_LSP_ARMADA)
	char *endp;
#endif /* CONFIG_SYNO_LSP_ARMADA */
	unsigned int i;

	buffer[2] = '\0';

	for (i = 0; i < size; i++) {
		buffer[0] = *hex++;
		buffer[1] = *hex++;

#if defined(CONFIG_SYNO_LSP_ARMADA)
		key[i] = (u8)simple_strtoul(buffer, &endp, 16);

		if (endp != &buffer[2])
#else /* CONFIG_SYNO_LSP_ARMADA */
		if (kstrtou8(buffer, 16, &key[i]))
#endif /* CONFIG_SYNO_LSP_ARMADA */
			return -EINVAL;
	}

	if (*hex != '\0')
		return -EINVAL;

	return 0;
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
static void crypt_free_tfms(struct crypt_config *cc)
{
	unsigned i;

	if (!cc->tfms)
		return;

	for (i = 0; i < cc->tfms_count; i++)
		if (cc->tfms[i] && !IS_ERR(cc->tfms[i])) {
			crypto_free_ablkcipher(cc->tfms[i]);
			cc->tfms[i] = NULL;
		}

	kfree(cc->tfms);
	cc->tfms = NULL;
}

static int crypt_alloc_tfms(struct crypt_config *cc, char *ciphermode)
{
	unsigned i;
	int err;

	cc->tfms = kmalloc(cc->tfms_count * sizeof(struct crypto_ablkcipher *),
			   GFP_KERNEL);
	if (!cc->tfms)
		return -ENOMEM;

	for (i = 0; i < cc->tfms_count; i++) {
		cc->tfms[i] = crypto_alloc_ablkcipher(ciphermode, 0, 0);
		if (IS_ERR(cc->tfms[i])) {
			err = PTR_ERR(cc->tfms[i]);
			crypt_free_tfms(cc);
			return err;
		}
	}

	return 0;
}

static int crypt_setkey_allcpus(struct crypt_config *cc)
{
	unsigned subkey_size = cc->key_size >> ilog2(cc->tfms_count);
	int err = 0, i, r;

	for (i = 0; i < cc->tfms_count; i++) {
		r = crypto_ablkcipher_setkey(cc->tfms[i],
					     cc->key + (i * subkey_size),
					     subkey_size);
		if (r)
			err = r;
	}

	return err;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

#if defined(CONFIG_SYNO_LSP_ARMADA)
static int crypt_set_key(struct crypt_config *cc, char *key)
{
	unsigned key_size = strlen(key) >> 1;
	if (cc->key_size && cc->key_size != key_size)
		return -EINVAL;

	cc->key_size = key_size; /* initial settings */

	if ((!key_size && strcmp(key, "-")) ||
	    (key_size && crypt_decode_key(cc->key, key, key_size) < 0))
		return -EINVAL;

	set_bit(DM_CRYPT_KEY_VALID, &cc->flags);

	return 0;
}
#else /* CONFIG_SYNO_LSP_ARMADA */
static int crypt_set_key(struct crypt_config *cc, char *key)
{
	int r = -EINVAL;
	int key_string_len = strlen(key);

	/* The key size may not be changed. */
	if (cc->key_size != (key_string_len >> 1))
		goto out;

	/* Hyphen (which gives a key_size of zero) means there is no key. */
	if (!cc->key_size && strcmp(key, "-"))
		goto out;

	if (cc->key_size && crypt_decode_key(cc->key, key, cc->key_size) < 0)
		goto out;

	set_bit(DM_CRYPT_KEY_VALID, &cc->flags);

	r = crypt_setkey_allcpus(cc);

out:
	/* Hex key string not needed after here, so wipe it. */
	memset(key, '0', key_string_len);

	return r;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

static int crypt_wipe_key(struct crypt_config *cc)
{
	clear_bit(DM_CRYPT_KEY_VALID, &cc->flags);
	memset(&cc->key, 0, cc->key_size * sizeof(u8));

#if defined(CONFIG_SYNO_LSP_ARMADA)
	return 0;
#else /* CONFIG_SYNO_LSP_ARMADA */
	return crypt_setkey_allcpus(cc);
#endif /* CONFIG_SYNO_LSP_ARMADA */
}

#if defined(CONFIG_SYNO_LSP_ARMADA)
/*
 * Construct an encryption mapping:
 * <cipher> <key> <iv_offset> <dev_path> <start>
 */
static int crypt_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct crypt_config *cc;
#ifndef CONFIG_OCF_DM_CRYPT
	struct crypto_blkcipher *tfm;
#endif
	char *tmp;
	char *cipher;
	char *chainmode;
	char *ivmode;
	char *ivopts;
	unsigned int key_size;
	unsigned long long tmpll;
	if (argc != 5) {
		ti->error = "Not enough arguments";
		return -EINVAL;
	}

	tmp = argv[0];
	cipher = strsep(&tmp, "-");
	chainmode = strsep(&tmp, "-");
	ivopts = strsep(&tmp, "-");
	ivmode = strsep(&ivopts, ":");

	if (tmp)
		DMWARN("Unexpected additional cipher options");

	key_size = strlen(argv[1]) >> 1;

	cc = kzalloc(sizeof(*cc) + key_size * sizeof(u8), GFP_KERNEL);
	if (cc == NULL) {
		ti->error =
			"Cannot allocate transparent encryption context";
		return -ENOMEM;
	}

	if (crypt_set_key(cc, argv[1])) {
		ti->error = "Error decoding key";
		goto bad1;
	}

	/* Compatiblity mode for old dm-crypt cipher strings */
	if (!chainmode || (strcmp(chainmode, "plain") == 0 && !ivmode)) {
		chainmode = "cbc";
		ivmode = "plain";
	}

	if (strcmp(chainmode, "ecb") && !ivmode) {
		ti->error = "This chaining mode requires an IV mechanism";
		goto bad1;
	}

	if (snprintf(cc->cipher, CRYPTO_MAX_ALG_NAME, "%s(%s)", chainmode,
		     cipher) >= CRYPTO_MAX_ALG_NAME) {
		ti->error = "Chain mode + cipher name is too long";
		goto bad1;
	}

#if defined(CONFIG_OCF_DM_CRYPT)
	/* prepare a new OCF session */
        memset(&cc->cr_dm, 0, sizeof(struct cryptoini));

	if((strcmp(cipher,"aes") == 0) && (strcmp(chainmode, "cbc") == 0))
		cc->cr_dm.cri_alg  = CRYPTO_AES_CBC;
	else if((strcmp(cipher,"des") == 0) && (strcmp(chainmode, "cbc") == 0))
		cc->cr_dm.cri_alg  = CRYPTO_DES_CBC;
	else if((strcmp(cipher,"des3_ede") == 0) && (strcmp(chainmode, "cbc") == 0))
		cc->cr_dm.cri_alg  = CRYPTO_3DES_CBC;
	else {
		ti->error = DM_MSG_PREFIX "using OCF: unknown cipher or bad chain mode";
		goto bad1;
	}

	/*strcpy(cc->cipher, cipher);*/
	dmprintk("key size is %d\n",cc->key_size);
        cc->cr_dm.cri_klen = cc->key_size*8;
        cc->cr_dm.cri_key  = cc->key;
        cc->cr_dm.cri_next = NULL;

        if(crypto_newsession(&cc->ocf_cryptoid, &cc->cr_dm, 0)){
		dmprintk("crypt_ctr: crypto_newsession failed\n");
                ti->error = DM_MSG_PREFIX "crypto_newsession failed";
                goto bad2;
        }

#else

	tfm = crypto_alloc_blkcipher(cc->cipher, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		ti->error = "Error allocating crypto tfm";
		goto bad1;
	}
#endif
	strcpy(cc->cipher, cipher);
	strcpy(cc->chainmode, chainmode);
#if !defined(CONFIG_OCF_DM_CRYPT)
	cc->tfm = tfm;
#endif

	/*
	 * Choose ivmode. Valid modes: "plain", "essiv:<esshash>", "benbi".
	 * See comments at iv code
	 */

	if (ivmode == NULL)
		cc->iv_gen_ops = NULL;
	else if (strcmp(ivmode, "plain") == 0)
		cc->iv_gen_ops = &crypt_iv_plain_ops;
	else if (strcmp(ivmode, "essiv") == 0)
		cc->iv_gen_ops = &crypt_iv_essiv_ops;
#if !defined(CONFIG_OCF_DM_CRYPT)
	else if (strcmp(ivmode, "benbi") == 0)
		cc->iv_gen_ops = &crypt_iv_benbi_ops;
#endif
	else if (strcmp(ivmode, "null") == 0)
		cc->iv_gen_ops = &crypt_iv_null_ops;
	else {
		ti->error = "Invalid IV mode";
		goto bad2;
	}

#if defined(CONFIG_OCF_DM_CRYPT)
	switch (cc->cr_dm.cri_alg) {
		case CRYPTO_AES_CBC:
			cc->iv_size = 16;
			break;
		default:
			cc->iv_size = 8;
			break;
	}

	if (cc->iv_gen_ops && cc->iv_gen_ops->ctr &&
	    cc->iv_gen_ops->ctr(cc, ti, ivopts) < 0)
		goto bad2;
#else

	if (cc->iv_gen_ops && cc->iv_gen_ops->ctr &&
	    cc->iv_gen_ops->ctr(cc, ti, ivopts) < 0)
		goto bad2;

	cc->iv_size = crypto_blkcipher_ivsize(tfm);
	if (cc->iv_size)
		/* at least a 64 bit sector number should fit in our buffer */
		cc->iv_size = max(cc->iv_size,
		                  (unsigned int)(sizeof(u64) / sizeof(u8)));
	else {
		if (cc->iv_gen_ops) {
			DMWARN("Selected cipher does not support IVs");
			if (cc->iv_gen_ops->dtr)
				cc->iv_gen_ops->dtr(cc);
			cc->iv_gen_ops = NULL;
		}
	}
#endif
	cc->io_pool = mempool_create_slab_pool(MIN_IOS, _crypt_io_pool);
	if (!cc->io_pool) {
		ti->error = "Cannot allocate crypt io mempool";
		goto bad3;
	}

	cc->page_pool = mempool_create_page_pool(MIN_POOL_PAGES, 0);
	if (!cc->page_pool) {
		ti->error = "Cannot allocate page mempool";
		goto bad4;
	}

	cc->bs = bioset_create(MIN_IOS, MIN_IOS);
	if (!cc->bs) {
		ti->error = "Cannot allocate crypt bioset";
		goto bad_bs;
	}
#if !defined(CONFIG_OCF_DM_CRYPT)
	if (crypto_blkcipher_setkey(tfm, cc->key, key_size) < 0) {
		ti->error = "Error setting key";
		goto bad5;
	}
#endif

	if (sscanf(argv[2], "%llu", &tmpll) != 1) {
		ti->error = "Invalid iv_offset sector";
		goto bad5;
	}
	cc->iv_offset = tmpll;

	if (sscanf(argv[4], "%llu", &tmpll) != 1) {
		ti->error = "Invalid device sector";
		goto bad5;
	}
	cc->start = tmpll;

	if (dm_get_device(ti, argv[3], dm_table_get_mode(ti->table),
								&cc->dev)) {
		ti->error = "Device lookup failed";
		goto bad5;
	}

	if (ivmode && cc->iv_gen_ops) {
		if (ivopts)
			*(ivopts - 1) = ':';
		cc->iv_mode = kmalloc(strlen(ivmode) + 1, GFP_KERNEL);
		if (!cc->iv_mode) {
			ti->error = "Error kmallocing iv_mode string";
			goto bad5;
		}
		strcpy(cc->iv_mode, ivmode);
	} else
		cc->iv_mode = NULL;

	ti->private = cc;
	return 0;

bad5:
	bioset_free(cc->bs);
bad_bs:
	mempool_destroy(cc->page_pool);
bad4:
	mempool_destroy(cc->io_pool);
bad3:
	if (cc->iv_gen_ops && cc->iv_gen_ops->dtr)
		cc->iv_gen_ops->dtr(cc);
bad2:
#if defined(CONFIG_OCF_DM_CRYPT)
	crypto_freesession(cc->ocf_cryptoid);
#else
	crypto_free_blkcipher(tfm);
#endif
bad1:
	/* Must zero key material before freeing */
	memset(cc, 0, sizeof(*cc) + cc->key_size * sizeof(u8));
	kfree(cc);
	return -EINVAL;
}

static void crypt_dtr(struct dm_target *ti)
{
	struct crypt_config *cc = (struct crypt_config *) ti->private;
	flush_workqueue(_kcryptd_workqueue);

	bioset_free(cc->bs);
	mempool_destroy(cc->page_pool);
	mempool_destroy(cc->io_pool);

	kfree(cc->iv_mode);
	if (cc->iv_gen_ops && cc->iv_gen_ops->dtr)
		cc->iv_gen_ops->dtr(cc);
#if defined(CONFIG_OCF_DM_CRYPT)
	crypto_freesession(cc->ocf_cryptoid);
#else
	crypto_free_blkcipher(cc->tfm);
#endif
	dm_put_device(ti, cc->dev);

	/* Must zero key material before freeing */
	memset(cc, 0, sizeof(*cc) + cc->key_size * sizeof(u8));
	kfree(cc);
}

static int crypt_map(struct dm_target *ti, struct bio *bio)
{
	struct crypt_config *cc = ti->private;
	struct crypt_io *io;
#ifdef CONFIG_HIGHMEM
	struct bio *newbio = NULL;
	struct bio_vec *from, *to;
	struct page *page;
	char *vto, *vfrom;
	unsigned int i;
#endif /* CONFIG_HIGHMEM */

	io = mempool_alloc(cc->io_pool, GFP_NOIO);

	/*
	 * Because OCF and CESA do not support high memory
	 * we have to create bounce pages if request
	 * with data in high memory arrives.
	 */

#ifdef CONFIG_HIGHMEM
	/* Check if we have to bounce */
	bio_for_each_segment(from, bio, i) {
		page = from->bv_page;

		if (!PageHighMem(page))
			continue;

		/* We have to bounce */
		if (newbio == NULL) {
			newbio = bio_alloc(GFP_NOIO, bio->bi_vcnt);
			memset(newbio->bi_io_vec, 0, bio->bi_vcnt *
							sizeof(struct bio_vec));
		}

		/* Allocate new vector */
		to = newbio->bi_io_vec + i;
		to->bv_page = mempool_alloc(cc->page_pool, GFP_NOIO);
		to->bv_len = from->bv_len;
		to->bv_offset = from->bv_offset;

		/* Copy data if this is required */
		if (bio_data_dir(bio) == WRITE) {
			vto = page_address(to->bv_page) + to->bv_offset;
			vfrom = kmap(from->bv_page) + from->bv_offset;
			memcpy(vto, vfrom, to->bv_len);
			kunmap(from->bv_page);
		}
	}

	/* We have at least one page bounced */
	if (newbio != NULL) {
		__bio_for_each_segment(from, bio, i, 0) {
			to = bio_iovec_idx(newbio, i);
			if (!to->bv_page) {
				to->bv_page = from->bv_page;
				to->bv_len = from->bv_len;
				to->bv_offset = from->bv_offset;
			}
		}

		newbio->bi_bdev = bio->bi_bdev;
		newbio->bi_sector = bio->bi_sector;
		newbio->bi_rw = bio->bi_rw;
		newbio->bi_vcnt = bio->bi_vcnt;
		newbio->bi_idx = bio->bi_idx;
		newbio->bi_size = bio->bi_size;

		newbio->bi_flags |= (1 << BIO_BOUNCED);
		newbio->bi_private = bio;
		bio = newbio;
	}
#endif /* CONFIG_HIGHMEM */

	io->target = ti;
	io->base_bio = bio;
	io->error = io->post_process = 0;
	atomic_set(&io->pending, 0);
	kcryptd_queue_io(io);

	return DM_MAPIO_SUBMITTED;
}
#else /* CONFIG_SYNO_LSP_ARMADA */
static void crypt_dtr(struct dm_target *ti)
{
	struct crypt_config *cc = ti->private;
	struct crypt_cpu *cpu_cc;
	int cpu;

	ti->private = NULL;

	if (!cc)
		return;

	if (cc->io_queue)
		destroy_workqueue(cc->io_queue);
	if (cc->crypt_queue)
		destroy_workqueue(cc->crypt_queue);

	if (cc->cpu)
		for_each_possible_cpu(cpu) {
			cpu_cc = per_cpu_ptr(cc->cpu, cpu);
			if (cpu_cc->req)
				mempool_free(cpu_cc->req, cc->req_pool);
		}

	crypt_free_tfms(cc);

	if (cc->bs)
		bioset_free(cc->bs);

	if (cc->page_pool)
		mempool_destroy(cc->page_pool);
	if (cc->req_pool)
		mempool_destroy(cc->req_pool);
	if (cc->io_pool)
		mempool_destroy(cc->io_pool);

	if (cc->iv_gen_ops && cc->iv_gen_ops->dtr)
		cc->iv_gen_ops->dtr(cc);

	if (cc->dev)
		dm_put_device(ti, cc->dev);

	if (cc->cpu)
		free_percpu(cc->cpu);

	kzfree(cc->cipher);
	kzfree(cc->cipher_string);

	/* Must zero key material before freeing */
	kzfree(cc);
}

static int crypt_ctr_cipher(struct dm_target *ti,
			    char *cipher_in, char *key)
{
	struct crypt_config *cc = ti->private;
	char *tmp, *cipher, *chainmode, *ivmode, *ivopts, *keycount;
	char *cipher_api = NULL;
	int ret = -EINVAL;
	char dummy;

	/* Convert to crypto api definition? */
	if (strchr(cipher_in, '(')) {
		ti->error = "Bad cipher specification";
		return -EINVAL;
	}

	cc->cipher_string = kstrdup(cipher_in, GFP_KERNEL);
	if (!cc->cipher_string)
		goto bad_mem;

	/*
	 * Legacy dm-crypt cipher specification
	 * cipher[:keycount]-mode-iv:ivopts
	 */
	tmp = cipher_in;
	keycount = strsep(&tmp, "-");
	cipher = strsep(&keycount, ":");

	if (!keycount)
		cc->tfms_count = 1;
	else if (sscanf(keycount, "%u%c", &cc->tfms_count, &dummy) != 1 ||
		 !is_power_of_2(cc->tfms_count)) {
		ti->error = "Bad cipher key count specification";
		return -EINVAL;
	}
	cc->key_parts = cc->tfms_count;

	cc->cipher = kstrdup(cipher, GFP_KERNEL);
	if (!cc->cipher)
		goto bad_mem;

	chainmode = strsep(&tmp, "-");
	ivopts = strsep(&tmp, "-");
	ivmode = strsep(&ivopts, ":");

	if (tmp)
		DMWARN("Ignoring unexpected additional cipher options");

	cc->cpu = __alloc_percpu(sizeof(*(cc->cpu)),
				 __alignof__(struct crypt_cpu));
	if (!cc->cpu) {
		ti->error = "Cannot allocate per cpu state";
		goto bad_mem;
	}

	/*
	 * For compatibility with the original dm-crypt mapping format, if
	 * only the cipher name is supplied, use cbc-plain.
	 */
	if (!chainmode || (!strcmp(chainmode, "plain") && !ivmode)) {
		chainmode = "cbc";
		ivmode = "plain";
	}

	if (strcmp(chainmode, "ecb") && !ivmode) {
		ti->error = "IV mechanism required";
		return -EINVAL;
	}

	cipher_api = kmalloc(CRYPTO_MAX_ALG_NAME, GFP_KERNEL);
	if (!cipher_api)
		goto bad_mem;

	ret = snprintf(cipher_api, CRYPTO_MAX_ALG_NAME,
		       "%s(%s)", chainmode, cipher);
	if (ret < 0) {
		kfree(cipher_api);
		goto bad_mem;
	}

	/* Allocate cipher */
	ret = crypt_alloc_tfms(cc, cipher_api);
	if (ret < 0) {
		ti->error = "Error allocating crypto tfm";
		goto bad;
	}

	/* Initialize and set key */
	ret = crypt_set_key(cc, key);
	if (ret < 0) {
		ti->error = "Error decoding and setting key";
		goto bad;
	}

	/* Initialize IV */
	cc->iv_size = crypto_ablkcipher_ivsize(any_tfm(cc));
	if (cc->iv_size)
		/* at least a 64 bit sector number should fit in our buffer */
		cc->iv_size = max(cc->iv_size,
				  (unsigned int)(sizeof(u64) / sizeof(u8)));
	else if (ivmode) {
		DMWARN("Selected cipher does not support IVs");
		ivmode = NULL;
	}

	/* Choose ivmode, see comments at iv code. */
	if (ivmode == NULL)
		cc->iv_gen_ops = NULL;
	else if (strcmp(ivmode, "plain") == 0)
		cc->iv_gen_ops = &crypt_iv_plain_ops;
	else if (strcmp(ivmode, "plain64") == 0)
		cc->iv_gen_ops = &crypt_iv_plain64_ops;
	else if (strcmp(ivmode, "essiv") == 0)
		cc->iv_gen_ops = &crypt_iv_essiv_ops;
	else if (strcmp(ivmode, "benbi") == 0)
		cc->iv_gen_ops = &crypt_iv_benbi_ops;
	else if (strcmp(ivmode, "null") == 0)
		cc->iv_gen_ops = &crypt_iv_null_ops;
	else if (strcmp(ivmode, "lmk") == 0) {
		cc->iv_gen_ops = &crypt_iv_lmk_ops;
		/* Version 2 and 3 is recognised according
		 * to length of provided multi-key string.
		 * If present (version 3), last key is used as IV seed.
		 */
		if (cc->key_size % cc->key_parts)
			cc->key_parts++;
	} else {
		ret = -EINVAL;
		ti->error = "Invalid IV mode";
		goto bad;
	}

	/* Allocate IV */
	if (cc->iv_gen_ops && cc->iv_gen_ops->ctr) {
		ret = cc->iv_gen_ops->ctr(cc, ti, ivopts);
		if (ret < 0) {
			ti->error = "Error creating IV";
			goto bad;
		}
	}

	/* Initialize IV (set keys for ESSIV etc) */
	if (cc->iv_gen_ops && cc->iv_gen_ops->init) {
		ret = cc->iv_gen_ops->init(cc);
		if (ret < 0) {
			ti->error = "Error initialising IV";
			goto bad;
		}
	}

	ret = 0;
bad:
	kfree(cipher_api);
	return ret;

bad_mem:
	ti->error = "Cannot allocate cipher strings";
	return -ENOMEM;
}

/*
 * Construct an encryption mapping:
 * <cipher> <key> <iv_offset> <dev_path> <start>
 */
static int crypt_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct crypt_config *cc;
	unsigned int key_size, opt_params;
	unsigned long long tmpll;
	int ret;
	struct dm_arg_set as;
	const char *opt_string;
	char dummy;

	static struct dm_arg _args[] = {
		{0, 1, "Invalid number of feature args"},
	};

	if (argc < 5) {
		ti->error = "Not enough arguments";
		return -EINVAL;
	}

	key_size = strlen(argv[1]) >> 1;

	cc = kzalloc(sizeof(*cc) + key_size * sizeof(u8), GFP_KERNEL);
	if (!cc) {
		ti->error = "Cannot allocate encryption context";
		return -ENOMEM;
	}
	cc->key_size = key_size;

	ti->private = cc;
	ret = crypt_ctr_cipher(ti, argv[0], argv[1]);
	if (ret < 0)
		goto bad;

	ret = -ENOMEM;
	cc->io_pool = mempool_create_slab_pool(MIN_IOS, _crypt_io_pool);
	if (!cc->io_pool) {
		ti->error = "Cannot allocate crypt io mempool";
		goto bad;
	}

	cc->dmreq_start = sizeof(struct ablkcipher_request);
	cc->dmreq_start += crypto_ablkcipher_reqsize(any_tfm(cc));
	cc->dmreq_start = ALIGN(cc->dmreq_start, crypto_tfm_ctx_alignment());
	cc->dmreq_start += crypto_ablkcipher_alignmask(any_tfm(cc)) &
			   ~(crypto_tfm_ctx_alignment() - 1);

	cc->req_pool = mempool_create_kmalloc_pool(MIN_IOS, cc->dmreq_start +
			sizeof(struct dm_crypt_request) + cc->iv_size);
	if (!cc->req_pool) {
		ti->error = "Cannot allocate crypt request mempool";
		goto bad;
	}

	cc->page_pool = mempool_create_page_pool(MIN_POOL_PAGES, 0);
	if (!cc->page_pool) {
		ti->error = "Cannot allocate page mempool";
		goto bad;
	}

	cc->bs = bioset_create(MIN_IOS, 0);
	if (!cc->bs) {
		ti->error = "Cannot allocate crypt bioset";
		goto bad;
	}

	ret = -EINVAL;
	if (sscanf(argv[2], "%llu%c", &tmpll, &dummy) != 1) {
		ti->error = "Invalid iv_offset sector";
		goto bad;
	}
	cc->iv_offset = tmpll;

	if (dm_get_device(ti, argv[3], dm_table_get_mode(ti->table), &cc->dev)) {
		ti->error = "Device lookup failed";
		goto bad;
	}

	if (sscanf(argv[4], "%llu%c", &tmpll, &dummy) != 1) {
		ti->error = "Invalid device sector";
		goto bad;
	}
	cc->start = tmpll;

	argv += 5;
	argc -= 5;

	/* Optional parameters */
	if (argc) {
		as.argc = argc;
		as.argv = argv;

		ret = dm_read_arg_group(_args, &as, &opt_params, &ti->error);
		if (ret)
			goto bad;

		opt_string = dm_shift_arg(&as);

		if (opt_params == 1 && opt_string &&
		    !strcasecmp(opt_string, "allow_discards"))
			ti->num_discard_bios = 1;
		else if (opt_params) {
			ret = -EINVAL;
			ti->error = "Invalid feature arguments";
			goto bad;
		}
	}

	ret = -ENOMEM;
	cc->io_queue = alloc_workqueue("kcryptd_io",
				       WQ_NON_REENTRANT|
				       WQ_MEM_RECLAIM,
				       1);
	if (!cc->io_queue) {
		ti->error = "Couldn't create kcryptd io queue";
		goto bad;
	}

	cc->crypt_queue = alloc_workqueue("kcryptd",
					  WQ_NON_REENTRANT|
					  WQ_CPU_INTENSIVE|
					  WQ_MEM_RECLAIM,
					  1);
	if (!cc->crypt_queue) {
		ti->error = "Couldn't create kcryptd queue";
		goto bad;
	}

	ti->num_flush_bios = 1;
	ti->discard_zeroes_data_unsupported = true;

	return 0;

bad:
	crypt_dtr(ti);
	return ret;
}

static int crypt_map(struct dm_target *ti, struct bio *bio)
{
	struct dm_crypt_io *io;
	struct crypt_config *cc = ti->private;

	/*
	 * If bio is REQ_FLUSH or REQ_DISCARD, just bypass crypt queues.
	 * - for REQ_FLUSH device-mapper core ensures that no IO is in-flight
	 * - for REQ_DISCARD caller must use flush if IO ordering matters
	 */
	if (unlikely(bio->bi_rw & (REQ_FLUSH | REQ_DISCARD))) {
		bio->bi_bdev = cc->dev->bdev;
		if (bio_sectors(bio))
			bio->bi_sector = cc->start + dm_target_offset(ti, bio->bi_sector);
		return DM_MAPIO_REMAPPED;
	}

	io = crypt_io_alloc(cc, bio, dm_target_offset(ti, bio->bi_sector));

	if (bio_data_dir(io->base_bio) == READ) {
		if (kcryptd_io_read(io, GFP_NOWAIT))
			kcryptd_queue_io(io);
	} else
		kcryptd_queue_crypt(io);

	return DM_MAPIO_SUBMITTED;
}
#endif /* CONFIG_SYNO_LSP_ARMADA */

static void crypt_status(struct dm_target *ti, status_type_t type,
			 unsigned status_flags, char *result, unsigned maxlen)
{
	struct crypt_config *cc = ti->private;
	unsigned i, sz = 0;

	switch (type) {
	case STATUSTYPE_INFO:
		result[0] = '\0';
		break;

	case STATUSTYPE_TABLE:
#if defined(CONFIG_SYNO_LSP_ARMADA)
		if (cc->iv_mode)
			DMEMIT("%s-%s-%s ", cc->cipher, cc->chainmode,
			       cc->iv_mode);
		else
			DMEMIT("%s-%s ", cc->cipher, cc->chainmode);
#else /* CONFIG_SYNO_LSP_ARMADA */
		DMEMIT("%s ", cc->cipher_string);
#endif /* CONFIG_SYNO_LSP_ARMADA */

		if (cc->key_size > 0)
			for (i = 0; i < cc->key_size; i++)
				DMEMIT("%02x", cc->key[i]);
		else
			DMEMIT("-");

		DMEMIT(" %llu %s %llu", (unsigned long long)cc->iv_offset,
				cc->dev->name, (unsigned long long)cc->start);

#if defined(CONFIG_SYNO_LSP_ARMADA)
		// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
		if (ti->num_discard_bios)
			DMEMIT(" 1 allow_discards");
#endif /* CONFIG_SYNO_LSP_ARMADA */

		break;
	}
}

static void crypt_postsuspend(struct dm_target *ti)
{
	struct crypt_config *cc = ti->private;

	set_bit(DM_CRYPT_SUSPENDED, &cc->flags);
}

static int crypt_preresume(struct dm_target *ti)
{
	struct crypt_config *cc = ti->private;

	if (!test_bit(DM_CRYPT_KEY_VALID, &cc->flags)) {
		DMERR("aborting resume - crypt key is not set.");
		return -EAGAIN;
	}

	return 0;
}

static void crypt_resume(struct dm_target *ti)
{
	struct crypt_config *cc = ti->private;

	clear_bit(DM_CRYPT_SUSPENDED, &cc->flags);
}

/* Message interface
 *	key set <key>
 *	key wipe
 */
static int crypt_message(struct dm_target *ti, unsigned argc, char **argv)
{
	struct crypt_config *cc = ti->private;
#if defined(CONFIG_SYNO_LSP_ARMADA)
	// do nothing
#else /* CONFIG_SYNO_LSP_ARMADA */
	int ret = -EINVAL;
#endif /* CONFIG_SYNO_LSP_ARMADA */

	if (argc < 2)
		goto error;

#if defined(CONFIG_SYNO_LSP_ARMADA)
	if (!strnicmp(argv[0], MESG_STR("key"))) {
#else /* CONFIG_SYNO_LSP_ARMADA */
	if (!strcasecmp(argv[0], "key")) {
#endif /* CONFIG_SYNO_LSP_ARMADA */
		if (!test_bit(DM_CRYPT_SUSPENDED, &cc->flags)) {
			DMWARN("not suspended during key manipulation.");
			return -EINVAL;
		}
#if defined(CONFIG_SYNO_LSP_ARMADA)
		if (argc == 3 && !strnicmp(argv[1], MESG_STR("set")))
			return crypt_set_key(cc, argv[2]);
		if (argc == 2 && !strnicmp(argv[1], MESG_STR("wipe")))
			return crypt_wipe_key(cc);
#else /* CONFIG_SYNO_LSP_ARMADA */
		if (argc == 3 && !strcasecmp(argv[1], "set")) {
			ret = crypt_set_key(cc, argv[2]);
			if (ret)
				return ret;
			if (cc->iv_gen_ops && cc->iv_gen_ops->init)
				ret = cc->iv_gen_ops->init(cc);
			return ret;
		}
		if (argc == 2 && !strcasecmp(argv[1], "wipe")) {
			if (cc->iv_gen_ops && cc->iv_gen_ops->wipe) {
				ret = cc->iv_gen_ops->wipe(cc);
				if (ret)
					return ret;
			}
			return crypt_wipe_key(cc);
		}
#endif /* CONFIG_SYNO_LSP_ARMADA */
	}

error:
	DMWARN("unrecognised message received.");
	return -EINVAL;
}

static int crypt_merge(struct dm_target *ti, struct bvec_merge_data *bvm,
		       struct bio_vec *biovec, int max_size)
{
	struct crypt_config *cc = ti->private;
	struct request_queue *q = bdev_get_queue(cc->dev->bdev);

	if (!q->merge_bvec_fn)
		return max_size;

	bvm->bi_bdev = cc->dev->bdev;
#if defined(CONFIG_SYNO_LSP_ARMADA)
	bvm->bi_sector = cc->start + bvm->bi_sector - ti->begin;
#else /* CONFIG_SYNO_LSP_ARMADA */
	bvm->bi_sector = cc->start + dm_target_offset(ti, bvm->bi_sector);
#endif /* CONFIG_SYNO_LSP_ARMADA */

	return min(max_size, q->merge_bvec_fn(q, bvm, biovec));
}

static int crypt_iterate_devices(struct dm_target *ti,
				 iterate_devices_callout_fn fn, void *data)
{
	struct crypt_config *cc = ti->private;

	return fn(ti, cc->dev, cc->start, ti->len, data);
}

static struct target_type crypt_target = {
	.name   = "crypt",
#if defined(CONFIG_SYNO_LSP_ARMADA)
	.version = {1, 5, 1},
#else /* CONFIG_SYNO_LSP_ARMADA */
	.version = {1, 12, 1},
#endif /* CONFIG_SYNO_LSP_ARMADA */
	.module = THIS_MODULE,
	.ctr    = crypt_ctr,
	.dtr    = crypt_dtr,
	.map    = crypt_map,
	.status = crypt_status,
	.postsuspend = crypt_postsuspend,
	.preresume = crypt_preresume,
	.resume = crypt_resume,
	.message = crypt_message,
	.merge  = crypt_merge,
	.iterate_devices = crypt_iterate_devices,
};

static int __init dm_crypt_init(void)
{
	int r;

#if defined(CONFIG_SYNO_LSP_ARMADA)
	_crypt_io_pool = KMEM_CACHE(crypt_io, 0);
#else /* CONFIG_SYNO_LSP_ARMADA */
	_crypt_io_pool = KMEM_CACHE(dm_crypt_io, 0);
#endif /* CONFIG_SYNO_LSP_ARMADA */
	if (!_crypt_io_pool)
		return -ENOMEM;

#if defined(CONFIG_SYNO_LSP_ARMADA)
	_kcryptd_workqueue = create_workqueue("kcryptd");
	if (!_kcryptd_workqueue) {
		r = -ENOMEM;
		DMERR("couldn't create kcryptd");
		goto bad1;
	}
#endif /* CONFIG_SYNO_LSP_ARMADA */

	r = dm_register_target(&crypt_target);
	if (r < 0) {
		DMERR("register failed %d", r);
#if defined(CONFIG_SYNO_LSP_ARMADA)
		goto bad2;
#else /* CONFIG_SYNO_LSP_ARMADA */
		kmem_cache_destroy(_crypt_io_pool);
#endif /* CONFIG_SYNO_LSP_ARMADA */
	}

#if defined(CONFIG_SYNO_LSP_ARMADA)
	_crypt_requests = 0;
	init_waitqueue_head(&_crypt_waitq);

#ifdef CONFIG_OCF_DM_CRYPT
	printk("dm_crypt using the OCF package.\n");
#endif

	return 0;

bad2:
	destroy_workqueue(_kcryptd_workqueue);
bad1:
	kmem_cache_destroy(_crypt_io_pool);
#endif /* CONFIG_SYNO_LSP_ARMADA */
	return r;
}

static void __exit dm_crypt_exit(void)
{
	dm_unregister_target(&crypt_target);
#if defined(CONFIG_SYNO_LSP_ARMADA)
	destroy_workqueue(_kcryptd_workqueue);
#endif /* CONFIG_SYNO_LSP_ARMADA */
	kmem_cache_destroy(_crypt_io_pool);
}

module_init(dm_crypt_init);
module_exit(dm_crypt_exit);

MODULE_AUTHOR("Christophe Saout <christophe@saout.de>");
MODULE_DESCRIPTION(DM_NAME " target for transparent encryption / decryption");
MODULE_LICENSE("GPL");
