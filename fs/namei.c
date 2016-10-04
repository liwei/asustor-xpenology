/*
 *  linux/fs/namei.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * Some corrections by tytso.
 */

/* [Feb 1997 T. Schoebel-Theuer] Complete rewrite of the pathname
 * lookup logic.
 */
/* [Feb-Apr 2000, AV] Rewrite to the new namespace architecture.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/pagemap.h>
#include <linux/fsnotify.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/ima.h>
#include <linux/syscalls.h>
#include <linux/mount.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/device_cgroup.h>
#include <linux/fs_struct.h>
#include <linux/posix_acl.h>
#include <asm/uaccess.h>

#ifdef CONFIG_SYNO_BTRFS_RENAME_READONLY_SUBVOL
#include <linux/magic.h>
#endif
#ifdef CONFIG_SYNO_FS_WINACL
#include "synoacl_int.h"
#endif /* CONFIG_SYNO_FS_WINACL */
#include "internal.h"
#include "mount.h"

/* [Feb-1997 T. Schoebel-Theuer]
 * Fundamental changes in the pathname lookup mechanisms (namei)
 * were necessary because of omirr.  The reason is that omirr needs
 * to know the _real_ pathname, not the user-supplied one, in case
 * of symlinks (and also when transname replacements occur).
 *
 * The new code replaces the old recursive symlink resolution with
 * an iterative one (in case of non-nested symlink chains).  It does
 * this with calls to <fs>_follow_link().
 * As a side effect, dir_namei(), _namei() and follow_link() are now 
 * replaced with a single function lookup_dentry() that can handle all 
 * the special cases of the former code.
 *
 * With the new dcache, the pathname is stored at each inode, at least as
 * long as the refcount of the inode is positive.  As a side effect, the
 * size of the dcache depends on the inode cache and thus is dynamic.
 *
 * [29-Apr-1998 C. Scott Ananian] Updated above description of symlink
 * resolution to correspond with current state of the code.
 *
 * Note that the symlink resolution is not *completely* iterative.
 * There is still a significant amount of tail- and mid- recursion in
 * the algorithm.  Also, note that <fs>_readlink() is not used in
 * lookup_dentry(): lookup_dentry() on the result of <fs>_readlink()
 * may return different results than <fs>_follow_link().  Many virtual
 * filesystems (including /proc) exhibit this behavior.
 */

/* [24-Feb-97 T. Schoebel-Theuer] Side effects caused by new implementation:
 * New symlink semantics: when open() is called with flags O_CREAT | O_EXCL
 * and the name already exists in form of a symlink, try to create the new
 * name indicated by the symlink. The old code always complained that the
 * name already exists, due to not following the symlink even if its target
 * is nonexistent.  The new semantics affects also mknod() and link() when
 * the name is a symlink pointing to a non-existent name.
 *
 * I don't know which semantics is the right one, since I have no access
 * to standards. But I found by trial that HP-UX 9.0 has the full "new"
 * semantics implemented, while SunOS 4.1.1 and Solaris (SunOS 5.4) have the
 * "old" one. Personally, I think the new semantics is much more logical.
 * Note that "ln old new" where "new" is a symlink pointing to a non-existing
 * file does succeed in both HP-UX and SunOs, but not in Solaris
 * and in the old Linux semantics.
 */

/* [16-Dec-97 Kevin Buhr] For security reasons, we change some symlink
 * semantics.  See the comments in "open_namei" and "do_link" below.
 *
 * [10-Sep-98 Alan Modra] Another symlink change.
 */

/* [Feb-Apr 2000 AV] Complete rewrite. Rules for symlinks:
 *	inside the path - always follow.
 *	in the last component in creation/removal/renaming - never follow.
 *	if LOOKUP_FOLLOW passed - follow.
 *	if the pathname has trailing slashes - follow.
 *	otherwise - don't follow.
 * (applied in that order).
 *
 * [Jun 2000 AV] Inconsistent behaviour of open() in case if flags==O_CREAT
 * restored for 2.4. This is the last surviving part of old 4.2BSD bug.
 * During the 2.4 we need to fix the userland stuff depending on it -
 * hopefully we will be able to get rid of that wart in 2.5. So far only
 * XEmacs seems to be relying on it...
 */
/*
 * [Sep 2001 AV] Single-semaphore locking scheme (kudos to David Holland)
 * implemented.  Let's see if raised priority of ->s_vfs_rename_mutex gives
 * any extra contention...
 */

/* In order to reduce some races, while at the same time doing additional
 * checking and hopefully speeding things up, we copy filenames to the
 * kernel data space before using them..
 *
 * POSIX.1 2.4: an empty pathname is invalid (ENOENT).
 * PATH_MAX includes the nul terminator --RR.
 */

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
int syno_utf8chr_to_utf16chr(u_int16_t *p, const u_int8_t *s, int n);
int syno_utf16chr_to_utf8chr(u_int8_t *s, u_int16_t wc, int maxlen);
u_int16_t *syno_generate_default_upcase_table(void);
u_int16_t *def_upcase_table(void);

/*
 * Sample implementation from Unicode home page.
 * http://www.stonehand.com/unicode/standard/fss-utf.html
 */
struct utf8_table {
	int     cmask;
	int     cval;
	int     shift;
	long    lmask;
	long    lval;
};

static struct utf8_table utf8_table[] =
{
    {0x80,  0x00,   0*6,    0x7F,           0,         /* 1 byte sequence */},
    {0xE0,  0xC0,   1*6,    0x7FF,          0x80,      /* 2 byte sequence */},
    {0xF0,  0xE0,   2*6,    0xFFFF,         0x800,     /* 3 byte sequence */},
    {0xF8,  0xF0,   3*6,    0x1FFFFF,       0x10000,   /* 4 byte sequence */},
    {0xFC,  0xF8,   4*6,    0x3FFFFFF,      0x200000,  /* 5 byte sequence */},
    {0xFE,  0xFC,   5*6,    0x7FFFFFFF,     0x4000000, /* 6 byte sequence */},
    {0,						       /* end of table    */}
};

int syno_utf8chr_to_utf16chr(u_int16_t *p, const u_int8_t *s, int n)
{
	long l;
	int c0, c, nc;
	struct utf8_table *t;

	nc = 0;
	c0 = *s;
	l = c0;
	for (t = utf8_table; t->cmask; t++) {
		nc++;
		if ((c0 & t->cmask) == t->cval) {
			l &= t->lmask;
			if (l < t->lval)
				return -1;
			*p = l;
			return nc;
		}
		if (n <= nc)
			return -1;
		s++;
		c = (*s ^ 0x80) & 0xFF;
		if (c & 0xC0)
			return -1;
		l = (l << 6) | c;
	}
	return -1;
}

int syno_utf16chr_to_utf8chr(u_int8_t *s, u_int16_t wc, int maxlen)
{
	long l;
	int c, nc;
	struct utf8_table *t;

	if (s == 0)
		return 0;

	l = wc;
	nc = 0;
	for (t = utf8_table; t->cmask && maxlen; t++, maxlen--) {
		nc++;
		if (l <= t->lmask) {
			c = t->shift;
			*s = t->cval | (l >> c);
			while (c > 0) {
				c -= 6;
				s++;
				*s = 0x80 | ((l >> c) & 0x3F);
			}
			return nc;
		}
	}
	return -1;
}

/*
 * upcase.c - Generate the full NTFS Unicode upcase table in little endian.
 *	      Part of the Linux-NTFS project.
 *
 * Copyright (C) 2001 Richard Russon <ntfs@flatcap.org>
 * Copyright (c) 2001,2002 Anton Altaparmakov
 *
 * Modified for mkntfs inclusion 9 June 2001 by Anton Altaparmakov.
 * Modified for kernel inclusion 10 September 2001 by Anton Altparmakov.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS source
 * in the file COPYING); if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

static u_int16_t gUC[UTF16_UPCASE_TABLE_SIZE];

void upcase_table_icu_fix(void) {
	int r = 0;
	u_int16_t *uc = gUC;

	/*  This is the difference part from Unicode Uppercase Table generated by ICU
	 *  Since samba/netatalk will call SetToCaseless() and override the default
	 *  Uppercase Table in kernel. It's batter eliminate those conflict as early
	 *  as possible.
	 */
	static const int uc_icu_table[][2] =	/* Offset, Value */
	{
		{0x00B5, 0x039C}, {0x0131, 0x0049}, {0x017F, 0x0053}, {0x0195, 0x01F6},
		{0x019E, 0x0220}, {0x01BF, 0x01F7}, {0x01C5, 0x01C4}, {0x01C8, 0x01C7},
		{0x01CB, 0x01CA}, {0x01F2, 0x01F1}, {0x01F9, 0x01F8}, {0x0219, 0x0218},
		{0x021B, 0x021A}, {0x021D, 0x021C}, {0x021F, 0x021E}, {0x0223, 0x0222},
		{0x0225, 0x0224}, {0x0227, 0x0226}, {0x0229, 0x0228}, {0x022B, 0x022A},
		{0x022D, 0x022C}, {0x022F, 0x022E}, {0x0231, 0x0230}, {0x0233, 0x0232},
		{0x0280, 0x01A6}, {0x0345, 0x0399}, {0x03D0, 0x0392}, {0x03D1, 0x0398},
		{0x03D5, 0x03A6}, {0x03D6, 0x03A0}, {0x03D9, 0x03D8}, {0x03DB, 0x03DA},
		{0x03DD, 0x03DC}, {0x03DF, 0x03DE}, {0x03E1, 0x03E0}, {0x03F0, 0x039A},
		{0x03F1, 0x03A1}, {0x03F2, 0x03A3}, {0x03F5, 0x0395}, {0x0450, 0x0400},
		{0x045D, 0x040D}, {0x048B, 0x048A}, {0x048D, 0x048C}, {0x048F, 0x048E},
		{0x04C6, 0x04C5}, {0x04CA, 0x04C9}, {0x04CE, 0x04CD}, {0x04ED, 0x04EC},
		{0x0501, 0x0500}, {0x0503, 0x0502}, {0x0505, 0x0504}, {0x0507, 0x0506},
		{0x0509, 0x0508}, {0x050B, 0x050A}, {0x050D, 0x050C}, {0x050F, 0x050E},
		{0x1E9B, 0x1E60}, {0x1FBE, 0x0399},
		{0}
	};

	for (r = 0; uc_icu_table[r][0]; r++)
		uc[uc_icu_table[r][0]] = uc_icu_table[r][1];
}

u_int16_t *syno_generate_default_upcase_table(void)
{
	const int uc_run_table[][3] = { /* Start, End, Add */
	{0x0061, 0x007B,  -32}, {0x0451, 0x045D, -80}, {0x1F70, 0x1F72,  74},
	{0x00E0, 0x00F7,  -32}, {0x045E, 0x0460, -80}, {0x1F72, 0x1F76,  86},
	{0x00F8, 0x00FF,  -32}, {0x0561, 0x0587, -48}, {0x1F76, 0x1F78, 100},
	{0x0256, 0x0258, -205}, {0x1F00, 0x1F08,   8}, {0x1F78, 0x1F7A, 128},
	{0x028A, 0x028C, -217}, {0x1F10, 0x1F16,   8}, {0x1F7A, 0x1F7C, 112},
	{0x03AC, 0x03AD,  -38}, {0x1F20, 0x1F28,   8}, {0x1F7C, 0x1F7E, 126},
	{0x03AD, 0x03B0,  -37}, {0x1F30, 0x1F38,   8}, {0x1FB0, 0x1FB2,   8},
	{0x03B1, 0x03C2,  -32}, {0x1F40, 0x1F46,   8}, {0x1FD0, 0x1FD2,   8},
	{0x03C2, 0x03C3,  -31}, {0x1F51, 0x1F52,   8}, {0x1FE0, 0x1FE2,   8},
	{0x03C3, 0x03CC,  -32}, {0x1F53, 0x1F54,   8}, {0x1FE5, 0x1FE6,   7},
	{0x03CC, 0x03CD,  -64}, {0x1F55, 0x1F56,   8}, {0x2170, 0x2180, -16},
	{0x03CD, 0x03CF,  -63}, {0x1F57, 0x1F58,   8}, {0x24D0, 0x24EA, -26},
	{0x0430, 0x0450,  -32}, {0x1F60, 0x1F68,   8}, {0xFF41, 0xFF5B, -32},
	{0}
	};

	const int uc_dup_table[][2] = { /* Start, End */
	{0x0100, 0x012F}, {0x01A0, 0x01A6}, {0x03E2, 0x03EF}, {0x04CB, 0x04CC},
	{0x0132, 0x0137}, {0x01B3, 0x01B7}, {0x0460, 0x0481}, {0x04D0, 0x04EB},
	{0x0139, 0x0149}, {0x01CD, 0x01DD}, {0x0490, 0x04BF}, {0x04EE, 0x04F5},
	{0x014A, 0x0178}, {0x01DE, 0x01EF}, {0x04BF, 0x04BF}, {0x04F8, 0x04F9},
	{0x0179, 0x017E}, {0x01F4, 0x01F5}, {0x04C1, 0x04C4}, {0x1E00, 0x1E95},
	{0x018B, 0x018B}, {0x01FA, 0x0218}, {0x04C7, 0x04C8}, {0x1EA0, 0x1EF9},
	{0}
	};

	const int uc_word_table[][2] = { /* Offset, Value */
	{0x00FF, 0x0178}, {0x01AD, 0x01AC}, {0x01F3, 0x01F1}, {0x0269, 0x0196},
	{0x0183, 0x0182}, {0x01B0, 0x01AF}, {0x0253, 0x0181}, {0x026F, 0x019C},
	{0x0185, 0x0184}, {0x01B9, 0x01B8}, {0x0254, 0x0186}, {0x0272, 0x019D},
	{0x0188, 0x0187}, {0x01BD, 0x01BC}, {0x0259, 0x018F}, {0x0275, 0x019F},
	{0x018C, 0x018B}, {0x01C6, 0x01C4}, {0x025B, 0x0190}, {0x0283, 0x01A9},
	{0x0192, 0x0191}, {0x01C9, 0x01C7}, {0x0260, 0x0193}, {0x0288, 0x01AE},
	{0x0199, 0x0198}, {0x01CC, 0x01CA}, {0x0263, 0x0194}, {0x0292, 0x01B7},
	{0x01A8, 0x01A7}, {0x01DD, 0x018E}, {0x0268, 0x0197},
	{0}
	};

	int i, r;
	u_int16_t *uc;

	uc = gUC;

	memset(uc, 0, UTF16_UPCASE_TABLE_SIZE * sizeof(u_int16_t));

	for (i = 0; i < UTF16_UPCASE_TABLE_SIZE; i++)
		uc[i] = i;
	for (r = 0; uc_run_table[r][0]; r++)
		for (i = uc_run_table[r][0]; i < uc_run_table[r][1]; i++)
			uc[i] = uc[i] + uc_run_table[r][2];
	for (r = 0; uc_dup_table[r][0]; r++)
		for (i = uc_dup_table[r][0]; i < uc_dup_table[r][1]; i += 2)
			uc[i + 1] = uc[i + 1] - 1;
	for (r = 0; uc_word_table[r][0]; r++)
		uc[uc_word_table[r][0]] = uc_word_table[r][1];
	upcase_table_icu_fix();
	return uc;
}

static u_int16_t *upcase_table = NULL;

u_int16_t *def_upcase_table(void)
{
    if(upcase_table==NULL)
        upcase_table = syno_generate_default_upcase_table();

    return upcase_table;
}

int syno_utf8_toupper(u_int8_t *to,const u_int8_t *from, int maxlen, int clenfrom, u_int16_t *upcasetable)
{
	u_int16_t *upcase_tbl;
	u_int16_t wc;
	u_int8_t *op;
	int size;

	upcase_tbl = (upcasetable==NULL) ? def_upcase_table() : upcasetable;

	op = to;
	while (clenfrom && maxlen) {
		size = syno_utf8chr_to_utf16chr(&wc, from, clenfrom);
		if (size == -1) {
			from++;
			clenfrom--;
			continue;
		} else {
			from += size;
			clenfrom -= size;
		}
		size = syno_utf16chr_to_utf8chr(op, upcase_tbl[wc], maxlen);
		if (size == -1) {
			continue;
		} else {
			op += size;
			maxlen -= size;
		}
	}
	*op = 0;
	return (op - to);
}
EXPORT_SYMBOL(syno_utf8_toupper);

int syno_utf8_strcmp(const u_int8_t *utf8str1,const u_int8_t *utf8str2,int len_utf8_str1, int len_utf8_str2, u_int16_t *upcasetable)
{
	u_int16_t *upcase_tbl;
	u_int16_t wc1, wc2;
	int size1, size2;
	int result = -1;

	upcase_tbl = (upcasetable==NULL) ? def_upcase_table() : upcasetable;

	while (len_utf8_str1 && len_utf8_str2) {
		size1 = syno_utf8chr_to_utf16chr(&wc1, utf8str1, len_utf8_str1);
		size2 = syno_utf8chr_to_utf16chr(&wc2, utf8str2, len_utf8_str2);

		if (size1 != -1 && size2 != -1) {
			if (upcase_tbl[wc1] != upcase_tbl[wc2])
				goto END;
		} else if (size1 == -1 && size2 == -1) {
			if (*utf8str1 != *utf8str2)
				goto END;
			size1 = size2 = 1;
		} else {
			goto END;
		}
		utf8str1 += size1;
		len_utf8_str1 -= size1;
		utf8str2 += size2;
		len_utf8_str2 -= size2;
	}
	if (len_utf8_str1 == 0 && len_utf8_str2 == 0)
		result = 0;
END:
	return result;
}
EXPORT_SYMBOL(syno_utf8_strcmp);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

void final_putname(struct filename *name)
{
	if (name->separate) {
		__putname(name->name);
		kfree(name);
	} else {
		__putname(name);
	}
}

#define EMBEDDED_NAME_MAX	(PATH_MAX - sizeof(struct filename))

static struct filename *
getname_flags(const char __user *filename, int flags, int *empty)
{
	struct filename *result, *err;
	int len;
	long max;
	char *kname;

	result = audit_reusename(filename);
	if (result)
		return result;

	result = __getname();
	if (unlikely(!result))
		return ERR_PTR(-ENOMEM);

	/*
	 * First, try to embed the struct filename inside the names_cache
	 * allocation
	 */
	kname = (char *)result + sizeof(*result);
	result->name = kname;
	result->separate = false;
	max = EMBEDDED_NAME_MAX;

recopy:
	len = strncpy_from_user(kname, filename, max);
	if (unlikely(len < 0)) {
		err = ERR_PTR(len);
		goto error;
	}

	/*
	 * Uh-oh. We have a name that's approaching PATH_MAX. Allocate a
	 * separate struct filename so we can dedicate the entire
	 * names_cache allocation for the pathname, and re-do the copy from
	 * userland.
	 */
	if (len == EMBEDDED_NAME_MAX && max == EMBEDDED_NAME_MAX) {
		kname = (char *)result;

		result = kzalloc(sizeof(*result), GFP_KERNEL);
		if (!result) {
			err = ERR_PTR(-ENOMEM);
			result = (struct filename *)kname;
			goto error;
		}
		result->name = kname;
		result->separate = true;
		max = PATH_MAX;
		goto recopy;
	}

	/* The empty path is special. */
	if (unlikely(!len)) {
		if (empty)
			*empty = 1;
		err = ERR_PTR(-ENOENT);
		if (!(flags & LOOKUP_EMPTY))
			goto error;
	}

	err = ERR_PTR(-ENAMETOOLONG);
	if (unlikely(len >= PATH_MAX))
		goto error;

	result->uptr = filename;
	audit_getname(result);
	return result;

error:
	final_putname(result);
	return err;
}

struct filename *
getname(const char __user * filename)
{
	return getname_flags(filename, 0, NULL);
}
EXPORT_SYMBOL(getname);

#ifdef CONFIG_AUDITSYSCALL
void putname(struct filename *name)
{
	if (unlikely(!audit_dummy_context()))
		return audit_putname(name);
	final_putname(name);
}
#endif

static int check_acl(struct inode *inode, int mask)
{
#ifdef CONFIG_FS_POSIX_ACL
	struct posix_acl *acl;

	if (mask & MAY_NOT_BLOCK) {
		acl = get_cached_acl_rcu(inode, ACL_TYPE_ACCESS);
	        if (!acl)
	                return -EAGAIN;
		/* no ->get_acl() calls in RCU mode... */
		if (acl == ACL_NOT_CACHED)
			return -ECHILD;
	        return posix_acl_permission(inode, acl, mask & ~MAY_NOT_BLOCK);
	}

	acl = get_cached_acl(inode, ACL_TYPE_ACCESS);

	/*
	 * A filesystem can force a ACL callback by just never filling the
	 * ACL cache. But normally you'd fill the cache either at inode
	 * instantiation time, or on the first ->get_acl call.
	 *
	 * If the filesystem doesn't have a get_acl() function at all, we'll
	 * just create the negative cache entry.
	 */
	if (acl == ACL_NOT_CACHED) {
	        if (inode->i_op->get_acl) {
			acl = inode->i_op->get_acl(inode, ACL_TYPE_ACCESS);
			if (IS_ERR(acl))
				return PTR_ERR(acl);
		} else {
		        set_cached_acl(inode, ACL_TYPE_ACCESS, NULL);
		        return -EAGAIN;
		}
	}

	if (acl) {
	        int error = posix_acl_permission(inode, acl, mask);
	        posix_acl_release(acl);
	        return error;
	}
#endif

	return -EAGAIN;
}

/*
 * This does the basic permission checking
 */
static int acl_permission_check(struct inode *inode, int mask)
{
	unsigned int mode = inode->i_mode;

	if (likely(uid_eq(current_fsuid(), inode->i_uid)))
		mode >>= 6;
	else {
		if (IS_POSIXACL(inode) && (mode & S_IRWXG)) {
			int error = check_acl(inode, mask);
			if (error != -EAGAIN)
				return error;
		}

		if (in_group_p(inode->i_gid))
			mode >>= 3;
	}

	/*
	 * If the DACs are ok we don't need any capability check.
	 */
	if ((mask & ~mode & (MAY_READ | MAY_WRITE | MAY_EXEC)) == 0)
		return 0;
	return -EACCES;
}

/**
 * generic_permission -  check for access rights on a Posix-like filesystem
 * @inode:	inode to check access rights for
 * @mask:	right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC, ...)
 *
 * Used to check for read/write/execute permissions on a file.
 * We use "fsuid" for this, letting us set arbitrary permissions
 * for filesystem access without changing the "normal" uids which
 * are used for other things.
 *
 * generic_permission is rcu-walk aware. It returns -ECHILD in case an rcu-walk
 * request cannot be satisfied (eg. requires blocking or too much complexity).
 * It would then be called again in ref-walk mode.
 */
int generic_permission(struct inode *inode, int mask)
{
	int ret;

	/*
	 * Do the basic permission checks.
	 */
	ret = acl_permission_check(inode, mask);
	if (ret != -EACCES)
		return ret;

	if (S_ISDIR(inode->i_mode)) {
		/* DACs are overridable for directories */
		if (inode_capable(inode, CAP_DAC_OVERRIDE))
			return 0;
		if (!(mask & MAY_WRITE))
			if (inode_capable(inode, CAP_DAC_READ_SEARCH))
				return 0;
		return -EACCES;
	}
	/*
	 * Read/write DACs are always overridable.
	 * Executable DACs are overridable when there is
	 * at least one exec bit set.
	 */
	if (!(mask & MAY_EXEC) || (inode->i_mode & S_IXUGO))
		if (inode_capable(inode, CAP_DAC_OVERRIDE))
			return 0;

	/*
	 * Searching includes executable on directories, else just read.
	 */
	mask &= MAY_READ | MAY_WRITE | MAY_EXEC;
	if (mask == MAY_READ)
		if (inode_capable(inode, CAP_DAC_READ_SEARCH))
			return 0;

	return -EACCES;
}

/*
 * We _really_ want to just do "generic_permission()" without
 * even looking at the inode->i_op values. So we keep a cache
 * flag in inode->i_opflags, that says "this has not special
 * permission function, use the fast case".
 */
static inline int do_inode_permission(struct inode *inode, int mask)
{
	if (unlikely(!(inode->i_opflags & IOP_FASTPERM))) {
		if (likely(inode->i_op->permission))
			return inode->i_op->permission(inode, mask);

		/* This gets set once for the inode lifetime */
		spin_lock(&inode->i_lock);
		inode->i_opflags |= IOP_FASTPERM;
		spin_unlock(&inode->i_lock);
	}
	return generic_permission(inode, mask);
}

/**
 * __inode_permission - Check for access rights to a given inode
 * @inode: Inode to check permission on
 * @mask: Right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC)
 *
 * Check for read/write/execute permissions on an inode.
 *
 * When checking for MAY_APPEND, MAY_WRITE must also be set in @mask.
 *
 * This does not check for a read-only file system.  You probably want
 * inode_permission().
 */
int __inode_permission(struct inode *inode, int mask)
{
	int retval;

	if (unlikely(mask & MAY_WRITE)) {
		/*
		 * Nobody gets write access to an immutable file.
		 */
		if (IS_IMMUTABLE(inode))
			return -EACCES;
	}

	retval = do_inode_permission(inode, mask);
	if (retval)
		return retval;

	retval = devcgroup_inode_permission(inode, mask);
	if (retval)
		return retval;

	return security_inode_permission(inode, mask);
}

/**
 * sb_permission - Check superblock-level permissions
 * @sb: Superblock of inode to check permission on
 * @inode: Inode to check permission on
 * @mask: Right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC)
 *
 * Separate out file-system wide checks from inode-specific permission checks.
 */
static int sb_permission(struct super_block *sb, struct inode *inode, int mask)
{
	if (unlikely(mask & MAY_WRITE)) {
		umode_t mode = inode->i_mode;

		/* Nobody gets write access to a read-only fs. */
		if ((sb->s_flags & MS_RDONLY) &&
		    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
			return -EROFS;
	}
	return 0;
}

/**
 * inode_permission - Check for access rights to a given inode
 * @inode: Inode to check permission on
 * @mask: Right to check for (%MAY_READ, %MAY_WRITE, %MAY_EXEC)
 *
 * Check for read/write/execute permissions on an inode.  We use fs[ug]id for
 * this, letting us set arbitrary permissions for filesystem access without
 * changing the "normal" UIDs which are used for other things.
 *
 * When checking for MAY_APPEND, MAY_WRITE must also be set in @mask.
 */
int inode_permission(struct inode *inode, int mask)
{
	int retval;

	retval = sb_permission(inode->i_sb, inode, mask);
	if (retval)
		return retval;
	return __inode_permission(inode, mask);
}

/**
 * path_get - get a reference to a path
 * @path: path to get the reference to
 *
 * Given a path increment the reference count to the dentry and the vfsmount.
 */
void path_get(const struct path *path)
{
	mntget(path->mnt);
	dget(path->dentry);
}
EXPORT_SYMBOL(path_get);

/**
 * path_put - put a reference to a path
 * @path: path to put the reference to
 *
 * Given a path decrement the reference count to the dentry and the vfsmount.
 */
void path_put(const struct path *path)
{
	dput(path->dentry);
	mntput(path->mnt);
}
EXPORT_SYMBOL(path_put);

/*
 * Path walking has 2 modes, rcu-walk and ref-walk (see
 * Documentation/filesystems/path-lookup.txt).  In situations when we can't
 * continue in RCU mode, we attempt to drop out of rcu-walk mode and grab
 * normal reference counts on dentries and vfsmounts to transition to rcu-walk
 * mode.  Refcounts are grabbed at the last known good point before rcu-walk
 * got stuck, so ref-walk may continue from there. If this is not successful
 * (eg. a seqcount has changed), then failure is returned and it's up to caller
 * to restart the path walk from the beginning in ref-walk mode.
 */

static inline void lock_rcu_walk(void)
{
	br_read_lock(&vfsmount_lock);
	rcu_read_lock();
}

static inline void unlock_rcu_walk(void)
{
	rcu_read_unlock();
	br_read_unlock(&vfsmount_lock);
}

/**
 * unlazy_walk - try to switch to ref-walk mode.
 * @nd: nameidata pathwalk data
 * @dentry: child of nd->path.dentry or NULL
 * Returns: 0 on success, -ECHILD on failure
 *
 * unlazy_walk attempts to legitimize the current nd->path, nd->root and dentry
 * for ref-walk mode.  @dentry must be a path found by a do_lookup call on
 * @nd or NULL.  Must be called from rcu-walk context.
 */
static int unlazy_walk(struct nameidata *nd, struct dentry *dentry)
{
	struct fs_struct *fs = current->fs;
	struct dentry *parent = nd->path.dentry;
	int want_root = 0;

	BUG_ON(!(nd->flags & LOOKUP_RCU));
	if (nd->root.mnt && !(nd->flags & LOOKUP_ROOT)) {
		want_root = 1;
		spin_lock(&fs->lock);
		if (nd->root.mnt != fs->root.mnt ||
				nd->root.dentry != fs->root.dentry)
			goto err_root;
	}
	spin_lock(&parent->d_lock);
	if (!dentry) {
		if (!__d_rcu_to_refcount(parent, nd->seq))
			goto err_parent;
		BUG_ON(nd->inode != parent->d_inode);
	} else {
		if (dentry->d_parent != parent)
			goto err_parent;
		spin_lock_nested(&dentry->d_lock, DENTRY_D_LOCK_NESTED);
		if (!__d_rcu_to_refcount(dentry, nd->seq))
			goto err_child;
		/*
		 * If the sequence check on the child dentry passed, then
		 * the child has not been removed from its parent. This
		 * means the parent dentry must be valid and able to take
		 * a reference at this point.
		 */
		BUG_ON(!IS_ROOT(dentry) && dentry->d_parent != parent);
		BUG_ON(!parent->d_count);
		parent->d_count++;
		spin_unlock(&dentry->d_lock);
	}
	spin_unlock(&parent->d_lock);
	if (want_root) {
		path_get(&nd->root);
		spin_unlock(&fs->lock);
	}
	mntget(nd->path.mnt);

	unlock_rcu_walk();
	nd->flags &= ~LOOKUP_RCU;
	return 0;

err_child:
	spin_unlock(&dentry->d_lock);
err_parent:
	spin_unlock(&parent->d_lock);
err_root:
	if (want_root)
		spin_unlock(&fs->lock);
	return -ECHILD;
}

static inline int d_revalidate(struct dentry *dentry, unsigned int flags)
{
	return dentry->d_op->d_revalidate(dentry, flags);
}

/**
 * complete_walk - successful completion of path walk
 * @nd:  pointer nameidata
 *
 * If we had been in RCU mode, drop out of it and legitimize nd->path.
 * Revalidate the final result, unless we'd already done that during
 * the path walk or the filesystem doesn't ask for it.  Return 0 on
 * success, -error on failure.  In case of failure caller does not
 * need to drop nd->path.
 */
static int complete_walk(struct nameidata *nd)
{
	struct dentry *dentry = nd->path.dentry;
	int status;

	if (nd->flags & LOOKUP_RCU) {
		nd->flags &= ~LOOKUP_RCU;
		if (!(nd->flags & LOOKUP_ROOT))
			nd->root.mnt = NULL;
		spin_lock(&dentry->d_lock);
		if (unlikely(!__d_rcu_to_refcount(dentry, nd->seq))) {
			spin_unlock(&dentry->d_lock);
			unlock_rcu_walk();
			return -ECHILD;
		}
		BUG_ON(nd->inode != dentry->d_inode);
		spin_unlock(&dentry->d_lock);
		mntget(nd->path.mnt);
		unlock_rcu_walk();
	}

	if (likely(!(nd->flags & LOOKUP_JUMPED)))
		return 0;

	if (likely(!(dentry->d_flags & DCACHE_OP_WEAK_REVALIDATE)))
		return 0;

	status = dentry->d_op->d_weak_revalidate(dentry, nd->flags);
	if (status > 0)
		return 0;

	if (!status)
		status = -ESTALE;

	path_put(&nd->path);
	return status;
}

static __always_inline void set_root(struct nameidata *nd)
{
	if (!nd->root.mnt)
		get_fs_root(current->fs, &nd->root);
}

static int link_path_walk(const char *, struct nameidata *);

static __always_inline void set_root_rcu(struct nameidata *nd)
{
	if (!nd->root.mnt) {
		struct fs_struct *fs = current->fs;
		unsigned seq;

		do {
			seq = read_seqcount_begin(&fs->seq);
			nd->root = fs->root;
			nd->seq = __read_seqcount_begin(&nd->root.dentry->d_seq);
		} while (read_seqcount_retry(&fs->seq, seq));
	}
}

static __always_inline int __vfs_follow_link(struct nameidata *nd, const char *link)
{
	int ret;

	if (IS_ERR(link))
		goto fail;

	if (*link == '/') {
		set_root(nd);
		path_put(&nd->path);
		nd->path = nd->root;
		path_get(&nd->root);
		nd->flags |= LOOKUP_JUMPED;
	}
	nd->inode = nd->path.dentry->d_inode;

	ret = link_path_walk(link, nd);
	return ret;
fail:
	path_put(&nd->path);
	return PTR_ERR(link);
}

static void path_put_conditional(struct path *path, struct nameidata *nd)
{
	dput(path->dentry);
	if (path->mnt != nd->path.mnt)
		mntput(path->mnt);
}

static inline void path_to_nameidata(const struct path *path,
					struct nameidata *nd)
{
	if (!(nd->flags & LOOKUP_RCU)) {
		dput(nd->path.dentry);
		if (nd->path.mnt != path->mnt)
			mntput(nd->path.mnt);
	}
	nd->path.mnt = path->mnt;
	nd->path.dentry = path->dentry;
}

/*
 * Helper to directly jump to a known parsed path from ->follow_link,
 * caller must have taken a reference to path beforehand.
 */
void nd_jump_link(struct nameidata *nd, struct path *path)
{
	path_put(&nd->path);

	nd->path = *path;
	nd->inode = nd->path.dentry->d_inode;
	nd->flags |= LOOKUP_JUMPED;
}

static inline void put_link(struct nameidata *nd, struct path *link, void *cookie)
{
	struct inode *inode = link->dentry->d_inode;
	if (inode->i_op->put_link)
		inode->i_op->put_link(link->dentry, nd, cookie);
	path_put(link);
}

int sysctl_protected_symlinks __read_mostly = 0;
int sysctl_protected_hardlinks __read_mostly = 0;

/**
 * may_follow_link - Check symlink following for unsafe situations
 * @link: The path of the symlink
 * @nd: nameidata pathwalk data
 *
 * In the case of the sysctl_protected_symlinks sysctl being enabled,
 * CAP_DAC_OVERRIDE needs to be specifically ignored if the symlink is
 * in a sticky world-writable directory. This is to protect privileged
 * processes from failing races against path names that may change out
 * from under them by way of other users creating malicious symlinks.
 * It will permit symlinks to be followed only when outside a sticky
 * world-writable directory, or when the uid of the symlink and follower
 * match, or when the directory owner matches the symlink's owner.
 *
 * Returns 0 if following the symlink is allowed, -ve on error.
 */
static inline int may_follow_link(struct path *link, struct nameidata *nd)
{
	const struct inode *inode;
	const struct inode *parent;

	if (!sysctl_protected_symlinks)
		return 0;

	/* Allowed if owner and follower match. */
	inode = link->dentry->d_inode;
	if (uid_eq(current_cred()->fsuid, inode->i_uid))
		return 0;

	/* Allowed if parent directory not sticky and world-writable. */
	parent = nd->path.dentry->d_inode;
	if ((parent->i_mode & (S_ISVTX|S_IWOTH)) != (S_ISVTX|S_IWOTH))
		return 0;

	/* Allowed if parent directory and link owner match. */
	if (uid_eq(parent->i_uid, inode->i_uid))
		return 0;

	audit_log_link_denied("follow_link", link);
	path_put_conditional(link, nd);
	path_put(&nd->path);
	return -EACCES;
}

/**
 * safe_hardlink_source - Check for safe hardlink conditions
 * @inode: the source inode to hardlink from
 *
 * Return false if at least one of the following conditions:
 *    - inode is not a regular file
 *    - inode is setuid
 *    - inode is setgid and group-exec
 *    - access failure for read and write
 *
 * Otherwise returns true.
 */
#ifdef CONFIG_SYNO_FS_WINACL
static bool safe_hardlink_source(struct dentry *dentry)
#else
static bool safe_hardlink_source(struct inode *inode)
#endif /* CONFIG_SYNO_FS_WINACL */
{
#ifdef CONFIG_SYNO_FS_WINACL
	struct inode *inode = dentry->d_inode;
#endif /* CONFIG_SYNO_FS_WINACL */
	umode_t mode = inode->i_mode;

	/* Special files should not get pinned to the filesystem. */
	if (!S_ISREG(mode))
		return false;

	/* Setuid files should not get pinned to the filesystem. */
	if (mode & S_ISUID)
		return false;

	/* Executable setgid files should not get pinned to the filesystem. */
	if ((mode & (S_ISGID | S_IXGRP)) == (S_ISGID | S_IXGRP))
		return false;

	/* Hardlinking to unreadable or unwritable sources is dangerous. */
#ifdef CONFIG_SYNO_FS_WINACL
	if (IS_SYNOACL(dentry)) {
		if (synoacl_op_perm(dentry,  MAY_READ | MAY_WRITE)) {
			return false;
		}
	} else
#endif /* CONFIG_SYNO_FS_WINACL */
	if (inode_permission(inode, MAY_READ | MAY_WRITE))
		return false;

	return true;
}

/**
 * may_linkat - Check permissions for creating a hardlink
 * @link: the source to hardlink from
 *
 * Block hardlink when all of:
 *  - sysctl_protected_hardlinks enabled
 *  - fsuid does not match inode
 *  - hardlink source is unsafe (see safe_hardlink_source() above)
 *  - not CAP_FOWNER
 *
 * Returns 0 if successful, -ve on error.
 */
static int may_linkat(struct path *link)
{
	const struct cred *cred;
	struct inode *inode;

	if (!sysctl_protected_hardlinks)
		return 0;

	cred = current_cred();
	inode = link->dentry->d_inode;

	/* Source inode owner (or CAP_FOWNER) can hardlink all they like,
	 * otherwise, it must be a safe source.
	 */
#ifdef CONFIG_SYNO_FS_WINACL
	if (uid_eq(cred->fsuid, inode->i_uid) || safe_hardlink_source(link->dentry) ||
	    capable(CAP_FOWNER)) {
		return 0;
	}
#else
	if (uid_eq(cred->fsuid, inode->i_uid) || safe_hardlink_source(inode) ||
	    capable(CAP_FOWNER))
		return 0;
#endif /* CONFIG_SYNO_FS_WINACL */

	audit_log_link_denied("linkat", link);
	return -EPERM;
}

static __always_inline int
follow_link(struct path *link, struct nameidata *nd, void **p)
{
	struct dentry *dentry = link->dentry;
	int error;
	char *s;

	BUG_ON(nd->flags & LOOKUP_RCU);

	if (link->mnt == nd->path.mnt)
		mntget(link->mnt);

	error = -ELOOP;
	if (unlikely(current->total_link_count >= 40))
		goto out_put_nd_path;

	cond_resched();
	current->total_link_count++;

	touch_atime(link);
	nd_set_link(nd, NULL);

	error = security_inode_follow_link(link->dentry, nd);
	if (error)
		goto out_put_nd_path;

	nd->last_type = LAST_BIND;
	*p = dentry->d_inode->i_op->follow_link(dentry, nd);
	error = PTR_ERR(*p);
	if (IS_ERR(*p))
		goto out_put_nd_path;

	error = 0;
	s = nd_get_link(nd);
	if (s) {
		error = __vfs_follow_link(nd, s);
		if (unlikely(error))
			put_link(nd, link, *p);
	}

	return error;

out_put_nd_path:
	*p = NULL;
	path_put(&nd->path);
	path_put(link);
	return error;
}

static int follow_up_rcu(struct path *path)
{
	struct mount *mnt = real_mount(path->mnt);
	struct mount *parent;
	struct dentry *mountpoint;

	parent = mnt->mnt_parent;
	if (&parent->mnt == path->mnt)
		return 0;
	mountpoint = mnt->mnt_mountpoint;
	path->dentry = mountpoint;
	path->mnt = &parent->mnt;
	return 1;
}

/*
 * follow_up - Find the mountpoint of path's vfsmount
 *
 * Given a path, find the mountpoint of its source file system.
 * Replace @path with the path of the mountpoint in the parent mount.
 * Up is towards /.
 *
 * Return 1 if we went up a level and 0 if we were already at the
 * root.
 */
int follow_up(struct path *path)
{
	struct mount *mnt = real_mount(path->mnt);
	struct mount *parent;
	struct dentry *mountpoint;

	br_read_lock(&vfsmount_lock);
	parent = mnt->mnt_parent;
	if (parent == mnt) {
		br_read_unlock(&vfsmount_lock);
		return 0;
	}
	mntget(&parent->mnt);
	mountpoint = dget(mnt->mnt_mountpoint);
	br_read_unlock(&vfsmount_lock);
	dput(path->dentry);
	path->dentry = mountpoint;
	mntput(path->mnt);
	path->mnt = &parent->mnt;
	return 1;
}

#ifdef CONFIG_SYNO_FS_NOTIFY
/*
   Fetch full mount point path,
   It traverse vfsmount from down to up by following mnt_parent
   @in: struct vfsmount: vfsmount structure, size_t buf_len: path buffer length
   @out: mnt_full_path: full path of vfsmount struct
   @return: -1: failed, 0 : success
 */
int syno_fetch_mountpoint_fullpath(struct vfsmount *mnt, size_t buf_len, char *mnt_full_path)
{
	int ret = -1;
	struct mount *tmp_mnt = NULL;
	char *mnt_dentry_path = NULL;
	char *mnt_dentry_path_buf = NULL;
	char *mnt_full_path_buf = NULL;
	mnt_dentry_path_buf = kmalloc(PATH_MAX, GFP_ATOMIC);
	mnt_full_path_buf = kmalloc(PATH_MAX, GFP_ATOMIC);

	if(!mnt_dentry_path_buf || !mnt_full_path_buf) {
		ret = -ENOMEM;
		goto ERR;
	}

	tmp_mnt = real_mount(mnt);
	br_read_lock(&vfsmount_lock);
	do{
		mnt_dentry_path = dentry_path_raw(tmp_mnt->mnt_mountpoint, mnt_dentry_path_buf, PATH_MAX - 1);
		if(IS_ERR(mnt_dentry_path)){
			br_read_unlock(&vfsmount_lock);
			ret = PTR_ERR(mnt_dentry_path);
			goto ERR;
		}
		snprintf(mnt_full_path_buf, PATH_MAX, "%s", mnt_full_path);
		if(mnt_full_path_buf[0] == '/' && (mnt_dentry_path[0] == '/' && mnt_dentry_path[1] == 0))
			snprintf(mnt_full_path, buf_len, "%s", mnt_full_path_buf);
		else
			snprintf(mnt_full_path, buf_len, "%s%s", mnt_dentry_path, mnt_full_path_buf);
		tmp_mnt = tmp_mnt->mnt_parent;
	} while (tmp_mnt != tmp_mnt->mnt_parent);
	br_read_unlock(&vfsmount_lock);
	ret = 0;
ERR:
	kfree(mnt_dentry_path_buf);
	kfree(mnt_full_path_buf);
	return ret;
}
#endif /* CONFIG_SYNO_FS_NOTIFY */

/*
 * Perform an automount
 * - return -EISDIR to tell follow_managed() to stop and return the path we
 *   were called with.
 */
static int follow_automount(struct path *path, unsigned flags,
			    bool *need_mntput)
{
	struct vfsmount *mnt;
	int err;

	if (!path->dentry->d_op || !path->dentry->d_op->d_automount)
		return -EREMOTE;

	/* We don't want to mount if someone's just doing a stat -
	 * unless they're stat'ing a directory and appended a '/' to
	 * the name.
	 *
	 * We do, however, want to mount if someone wants to open or
	 * create a file of any type under the mountpoint, wants to
	 * traverse through the mountpoint or wants to open the
	 * mounted directory.  Also, autofs may mark negative dentries
	 * as being automount points.  These will need the attentions
	 * of the daemon to instantiate them before they can be used.
	 */
	if (!(flags & (LOOKUP_PARENT | LOOKUP_DIRECTORY |
		     LOOKUP_OPEN | LOOKUP_CREATE | LOOKUP_AUTOMOUNT)) &&
	    path->dentry->d_inode)
		return -EISDIR;

	current->total_link_count++;
	if (current->total_link_count >= 40)
		return -ELOOP;

	mnt = path->dentry->d_op->d_automount(path);
	if (IS_ERR(mnt)) {
		/*
		 * The filesystem is allowed to return -EISDIR here to indicate
		 * it doesn't want to automount.  For instance, autofs would do
		 * this so that its userspace daemon can mount on this dentry.
		 *
		 * However, we can only permit this if it's a terminal point in
		 * the path being looked up; if it wasn't then the remainder of
		 * the path is inaccessible and we should say so.
		 */
		if (PTR_ERR(mnt) == -EISDIR && (flags & LOOKUP_PARENT))
			return -EREMOTE;
		return PTR_ERR(mnt);
	}

	if (!mnt) /* mount collision */
		return 0;

	if (!*need_mntput) {
		/* lock_mount() may release path->mnt on error */
		mntget(path->mnt);
		*need_mntput = true;
	}
	err = finish_automount(mnt, path);

	switch (err) {
	case -EBUSY:
		/* Someone else made a mount here whilst we were busy */
		return 0;
	case 0:
		path_put(path);
		path->mnt = mnt;
		path->dentry = dget(mnt->mnt_root);
		return 0;
	default:
		return err;
	}

}

/*
 * Handle a dentry that is managed in some way.
 * - Flagged for transit management (autofs)
 * - Flagged as mountpoint
 * - Flagged as automount point
 *
 * This may only be called in refwalk mode.
 *
 * Serialization is taken care of in namespace.c
 */
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
static int follow_managed(struct path *path, struct nameidata *nd)
#else
static int follow_managed(struct path *path, unsigned flags)
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
{
	struct vfsmount *mnt = path->mnt; /* held by caller, must be left alone */
	unsigned managed;
	bool need_mntput = false;
	int ret = 0;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	int flags = nd->flags;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

	/* Given that we're not holding a lock here, we retain the value in a
	 * local variable for each dentry as we look at it so that we don't see
	 * the components of that value change under us */
	while (managed = ACCESS_ONCE(path->dentry->d_flags),
	       managed &= DCACHE_MANAGED_DENTRY,
	       unlikely(managed != 0)) {
		/* Allow the filesystem to manage the transit without i_mutex
		 * being held. */
		if (managed & DCACHE_MANAGE_TRANSIT) {
			BUG_ON(!path->dentry->d_op);
			BUG_ON(!path->dentry->d_op->d_manage);
			ret = path->dentry->d_op->d_manage(path->dentry, false);
			if (ret < 0)
				break;
		}

		/* Transit to a mounted filesystem. */
		if (managed & DCACHE_MOUNTED) {
			struct vfsmount *mounted = lookup_mnt(path);
			if (mounted) {
				dput(path->dentry);
				if (need_mntput)
					mntput(path->mnt);
				path->mnt = mounted;
				path->dentry = dget(mounted->mnt_root);
				need_mntput = true;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
				nd->flags |= LOOKUP_MOUNTED;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
				continue;
			}

			/* Something is mounted on this dentry in another
			 * namespace and/or whatever was mounted there in this
			 * namespace got unmounted before we managed to get the
			 * vfsmount_lock */
		}

		/* Handle an automount point */
		if (managed & DCACHE_NEED_AUTOMOUNT) {
			ret = follow_automount(path, flags, &need_mntput);
			if (ret < 0)
				break;
			continue;
		}

		/* We didn't change the current path point */
		break;
	}

	if (need_mntput && path->mnt == mnt)
		mntput(path->mnt);
	if (ret == -EISDIR)
		ret = 0;
	return ret < 0 ? ret : need_mntput;
}

int follow_down_one(struct path *path)
{
	struct vfsmount *mounted;

	mounted = lookup_mnt(path);
	if (mounted) {
		dput(path->dentry);
		mntput(path->mnt);
		path->mnt = mounted;
		path->dentry = dget(mounted->mnt_root);
		return 1;
	}
	return 0;
}

static inline bool managed_dentry_might_block(struct dentry *dentry)
{
	return (dentry->d_flags & DCACHE_MANAGE_TRANSIT &&
		dentry->d_op->d_manage(dentry, true) < 0);
}

/*
 * Try to skip to top of mountpoint pile in rcuwalk mode.  Fail if
 * we meet a managed dentry that would need blocking.
 */
static bool __follow_mount_rcu(struct nameidata *nd, struct path *path,
			       struct inode **inode)
{
	for (;;) {
		struct mount *mounted;
		/*
		 * Don't forget we might have a non-mountpoint managed dentry
		 * that wants to block transit.
		 */
		if (unlikely(managed_dentry_might_block(path->dentry)))
			return false;

		if (!d_mountpoint(path->dentry))
			break;

		mounted = __lookup_mnt(path->mnt, path->dentry, 1);
		if (!mounted)
			break;
		path->mnt = &mounted->mnt;
		path->dentry = mounted->mnt.mnt_root;
		nd->flags |= LOOKUP_JUMPED;
		nd->seq = read_seqcount_begin(&path->dentry->d_seq);
		/*
		 * Update the inode too. We don't need to re-check the
		 * dentry sequence number here after this d_inode read,
		 * because a mount-point is always pinned.
		 */
		*inode = path->dentry->d_inode;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		nd->flags |= LOOKUP_MOUNTED;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	}
	return true;
}

static void follow_mount_rcu(struct nameidata *nd)
{
	while (d_mountpoint(nd->path.dentry)) {
		struct mount *mounted;
		mounted = __lookup_mnt(nd->path.mnt, nd->path.dentry, 1);
		if (!mounted)
			break;
		nd->path.mnt = &mounted->mnt;
		nd->path.dentry = mounted->mnt.mnt_root;
		nd->seq = read_seqcount_begin(&nd->path.dentry->d_seq);
	}
}

static int follow_dotdot_rcu(struct nameidata *nd)
{
	set_root_rcu(nd);

	while (1) {
		if (nd->path.dentry == nd->root.dentry &&
		    nd->path.mnt == nd->root.mnt) {
			break;
		}
		if (nd->path.dentry != nd->path.mnt->mnt_root) {
			struct dentry *old = nd->path.dentry;
			struct dentry *parent = old->d_parent;
			unsigned seq;

			seq = read_seqcount_begin(&parent->d_seq);
			if (read_seqcount_retry(&old->d_seq, nd->seq))
				goto failed;
			nd->path.dentry = parent;
			nd->seq = seq;
			break;
		}
		if (!follow_up_rcu(&nd->path))
			break;
		nd->seq = read_seqcount_begin(&nd->path.dentry->d_seq);
	}
	follow_mount_rcu(nd);
	nd->inode = nd->path.dentry->d_inode;
	return 0;

failed:
	nd->flags &= ~LOOKUP_RCU;
	if (!(nd->flags & LOOKUP_ROOT))
		nd->root.mnt = NULL;
	unlock_rcu_walk();
	return -ECHILD;
}

/*
 * Follow down to the covering mount currently visible to userspace.  At each
 * point, the filesystem owning that dentry may be queried as to whether the
 * caller is permitted to proceed or not.
 */
int follow_down(struct path *path)
{
	unsigned managed;
	int ret;

	while (managed = ACCESS_ONCE(path->dentry->d_flags),
	       unlikely(managed & DCACHE_MANAGED_DENTRY)) {
		/* Allow the filesystem to manage the transit without i_mutex
		 * being held.
		 *
		 * We indicate to the filesystem if someone is trying to mount
		 * something here.  This gives autofs the chance to deny anyone
		 * other than its daemon the right to mount on its
		 * superstructure.
		 *
		 * The filesystem may sleep at this point.
		 */
		if (managed & DCACHE_MANAGE_TRANSIT) {
			BUG_ON(!path->dentry->d_op);
			BUG_ON(!path->dentry->d_op->d_manage);
			ret = path->dentry->d_op->d_manage(
				path->dentry, false);
			if (ret < 0)
				return ret == -EISDIR ? 0 : ret;
		}

		/* Transit to a mounted filesystem. */
		if (managed & DCACHE_MOUNTED) {
			struct vfsmount *mounted = lookup_mnt(path);
			if (!mounted)
				break;
			dput(path->dentry);
			mntput(path->mnt);
			path->mnt = mounted;
			path->dentry = dget(mounted->mnt_root);
			continue;
		}

		/* Don't handle automount points here */
		break;
	}
	return 0;
}

/*
 * Skip to top of mountpoint pile in refwalk mode for follow_dotdot()
 */
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
static void follow_mount(struct nameidata *nd)
#else
static void follow_mount(struct path *path)
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
{
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	struct path *path = &nd->path;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	while (d_mountpoint(path->dentry)) {
		struct vfsmount *mounted = lookup_mnt(path);
		if (!mounted)
			break;
		dput(path->dentry);
		mntput(path->mnt);
		path->mnt = mounted;
		path->dentry = dget(mounted->mnt_root);
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		nd->flags |= LOOKUP_MOUNTED;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	}
}

static void follow_dotdot(struct nameidata *nd)
{
	set_root(nd);

	while(1) {
		struct dentry *old = nd->path.dentry;

		if (nd->path.dentry == nd->root.dentry &&
		    nd->path.mnt == nd->root.mnt) {
			break;
		}
		if (nd->path.dentry != nd->path.mnt->mnt_root) {
			/* rare case of legitimate dget_parent()... */
			nd->path.dentry = dget_parent(nd->path.dentry);
			dput(old);
			break;
		}
		if (!follow_up(&nd->path))
			break;
	}
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	follow_mount(nd);
#else
	follow_mount(&nd->path);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	nd->inode = nd->path.dentry->d_inode;
}

/*
 * This looks up the name in dcache, possibly revalidates the old dentry and
 * allocates a new one if not found or not valid.  In the need_lookup argument
 * returns whether i_op->lookup is necessary.
 *
 * dir->d_inode->i_mutex must be held
 */
static struct dentry *lookup_dcache(struct qstr *name, struct dentry *dir,
				    unsigned int flags, bool *need_lookup)
{
	struct dentry *dentry;
	int error;

	*need_lookup = false;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	dentry = d_lookup_case(dir, name, (LOOKUP_CASELESS_COMPARE & flags)?1:0 );
#else
	dentry = d_lookup(dir, name);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	if (dentry) {
		if (dentry->d_flags & DCACHE_OP_REVALIDATE) {
			error = d_revalidate(dentry, flags);
			if (unlikely(error <= 0)) {
				if (error < 0) {
					dput(dentry);
					return ERR_PTR(error);
				} else if (!d_invalidate(dentry)) {
					dput(dentry);
					dentry = NULL;
				}
			}
		}
	}

	if (!dentry) {
		dentry = d_alloc(dir, name);
		if (unlikely(!dentry))
			return ERR_PTR(-ENOMEM);

		*need_lookup = true;
	}
	return dentry;
}

/*
 * Call i_op->lookup on the dentry.  The dentry must be negative but may be
 * hashed if it was pouplated with DCACHE_NEED_LOOKUP.
 *
 * dir->d_inode->i_mutex must be held
 */
static struct dentry *lookup_real(struct inode *dir, struct dentry *dentry,
				  unsigned int flags)
{
	struct dentry *old;

	/* Don't create child dentry for a dead directory. */
	if (unlikely(IS_DEADDIR(dir))) {
		dput(dentry);
		return ERR_PTR(-ENOENT);
	}

	old = dir->i_op->lookup(dir, dentry, flags);
	if (unlikely(old)) {
		dput(dentry);
		dentry = old;
	}
	return dentry;
}

static struct dentry *__lookup_hash(struct qstr *name,
		struct dentry *base, unsigned int flags)
{
	bool need_lookup;
	struct dentry *dentry;

	dentry = lookup_dcache(name, base, flags, &need_lookup);
	if (!need_lookup)
		return dentry;

	return lookup_real(base->d_inode, dentry, flags);
}

/*
 *  It's more convoluted than I'd like it to be, but... it's still fairly
 *  small and for now I'd prefer to have fast path as straight as possible.
 *  It _is_ time-critical.
 */
static int lookup_fast(struct nameidata *nd,
		       struct path *path, struct inode **inode)
{
	struct vfsmount *mnt = nd->path.mnt;
	struct dentry *dentry, *parent = nd->path.dentry;
	int need_reval = 1;
	int status = 1;
	int err;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	int caseless = (LOOKUP_CASELESS_COMPARE & nd->flags)?1:0;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

	/*
	 * Rename seqlock is not required here because in the off chance
	 * of a false negative due to a concurrent rename, we're going to
	 * do the non-racy lookup, below.
	 */
	if (nd->flags & LOOKUP_RCU) {
		unsigned seq;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		dentry = __d_lookup_rcu(parent, &nd->last, &seq, nd->inode, caseless);
#else
		dentry = __d_lookup_rcu(parent, &nd->last, &seq, nd->inode);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
		if (!dentry)
			goto unlazy;

		/*
		 * This sequence count validates that the inode matches
		 * the dentry name information from lookup.
		 */
		*inode = dentry->d_inode;
		if (read_seqcount_retry(&dentry->d_seq, seq))
			return -ECHILD;

		/*
		 * This sequence count validates that the parent had no
		 * changes while we did the lookup of the dentry above.
		 *
		 * The memory barrier in read_seqcount_begin of child is
		 *  enough, we can use __read_seqcount_retry here.
		 */
		if (__read_seqcount_retry(&parent->d_seq, nd->seq))
			return -ECHILD;
		nd->seq = seq;

		if (unlikely(dentry->d_flags & DCACHE_OP_REVALIDATE)) {
			status = d_revalidate(dentry, nd->flags);
			if (unlikely(status <= 0)) {
				if (status != -ECHILD)
					need_reval = 0;
				goto unlazy;
			}
		}
		path->mnt = mnt;
		path->dentry = dentry;
		if (unlikely(!__follow_mount_rcu(nd, path, inode)))
			goto unlazy;
		if (unlikely(path->dentry->d_flags & DCACHE_NEED_AUTOMOUNT))
			goto unlazy;
		return 0;
unlazy:
		if (unlazy_walk(nd, dentry))
			return -ECHILD;
	} else {
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		dentry = __d_lookup(parent, &nd->last, caseless);
		if (caseless) {
			if (dentry && !dentry->d_inode) {
				d_invalidate(dentry);
				dput(dentry);
				dentry = NULL;
			}
		}
#else
		dentry = __d_lookup(parent, &nd->last);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	}

	if (unlikely(!dentry))
		goto need_lookup;

	if (unlikely(dentry->d_flags & DCACHE_OP_REVALIDATE) && need_reval)
		status = d_revalidate(dentry, nd->flags);
	if (unlikely(status <= 0)) {
		if (status < 0) {
			dput(dentry);
			return status;
		}
		if (!d_invalidate(dentry)) {
			dput(dentry);
			goto need_lookup;
		}
	}

	path->mnt = mnt;
	path->dentry = dentry;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	err = follow_managed(path, nd);
#else
	err = follow_managed(path, nd->flags);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	if (unlikely(err < 0)) {
		path_put_conditional(path, nd);
		return err;
	}
	if (err)
		nd->flags |= LOOKUP_JUMPED;
	*inode = path->dentry->d_inode;
	return 0;

need_lookup:
	return 1;
}

/* Fast lookup failed, do it the slow way */
static int lookup_slow(struct nameidata *nd, struct path *path)
{
	struct dentry *dentry, *parent;
	int err;

	parent = nd->path.dentry;
	BUG_ON(nd->inode != parent->d_inode);

	mutex_lock(&parent->d_inode->i_mutex);
	dentry = __lookup_hash(&nd->last, parent, nd->flags);
	mutex_unlock(&parent->d_inode->i_mutex);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);
	path->mnt = nd->path.mnt;
	path->dentry = dentry;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	err = follow_managed(path, nd);
#else
	err = follow_managed(path, nd->flags);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	if (unlikely(err < 0)) {
		path_put_conditional(path, nd);
		return err;
	}
	if (err)
		nd->flags |= LOOKUP_JUMPED;
	return 0;
}

static inline int may_lookup(struct nameidata *nd)
{
#ifdef CONFIG_SYNO_FS_WINACL
	int err;
	int is_synoacl = IS_SYNOACL_INODE(nd->inode, nd->path.dentry);
#endif /* CONFIG_SYNO_FS_WINACL */

	if (nd->flags & LOOKUP_RCU) {
#ifdef CONFIG_SYNO_FS_WINACL
		if (is_synoacl) {
			err = synoacl_op_exec_perm(nd->path.dentry, nd->inode);
		} else {
			err = inode_permission(nd->inode, MAY_EXEC|MAY_NOT_BLOCK);
		}
#else
		int err = inode_permission(nd->inode, MAY_EXEC|MAY_NOT_BLOCK);
#endif /* CONFIG_SYNO_FS_WINACL */
		if (err != -ECHILD)
			return err;
		if (unlazy_walk(nd, NULL))
			return -ECHILD;
	}

#ifdef CONFIG_SYNO_FS_WINACL
	if (is_synoacl) {
		err = synoacl_op_exec_perm(nd->path.dentry, nd->inode);
	} else {
		err = inode_permission(nd->inode, MAY_EXEC);
	}
	return err;
#else
	return inode_permission(nd->inode, MAY_EXEC);
#endif /* CONFIG_SYNO_FS_WINACL */
}

static inline int handle_dots(struct nameidata *nd, int type)
{
	if (type == LAST_DOTDOT) {
		if (nd->flags & LOOKUP_RCU) {
			if (follow_dotdot_rcu(nd))
				return -ECHILD;
		} else
			follow_dotdot(nd);
	}
	return 0;
}

static void terminate_walk(struct nameidata *nd)
{
	if (!(nd->flags & LOOKUP_RCU)) {
		path_put(&nd->path);
	} else {
		nd->flags &= ~LOOKUP_RCU;
		if (!(nd->flags & LOOKUP_ROOT))
			nd->root.mnt = NULL;
		unlock_rcu_walk();
	}
}

/*
 * Do we need to follow links? We _really_ want to be able
 * to do this check without having to look at inode->i_op,
 * so we keep a cache of "no, this doesn't need follow_link"
 * for the common case.
 */
static inline int should_follow_link(struct inode *inode, int follow)
{
	if (unlikely(!(inode->i_opflags & IOP_NOFOLLOW))) {
#ifdef CONFIG_SYNO_HFSPLUS_ERROR_HANDLE_ENHANCE
		if (likely(inode->i_op && inode->i_op->follow_link))
#else
		if (likely(inode->i_op->follow_link))
#endif
			return follow;

		/* This gets set once for the inode lifetime */
		spin_lock(&inode->i_lock);
		inode->i_opflags |= IOP_NOFOLLOW;
		spin_unlock(&inode->i_lock);
	}
	return 0;
}

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
static inline int update_real_filename(struct nameidata *nd, char *target_name, int target_len)
{
	if ((nd->real_filename_len + target_len + 2) >= SYNO_SMB_PSTRING_LEN) {
		return -1;
	}
	memcpy(nd->real_filename_cur_locate, target_name, target_len);
	nd->real_filename_cur_locate += target_len;
	nd->real_filename_len += target_len;
	if (!(nd->flags & LOOKUP_TO_LASTCOMPONENT)) {
		*(nd->real_filename_cur_locate) = '/';
		nd->real_filename_cur_locate++;
		nd->real_filename_len ++;
	}
	*(nd->real_filename_cur_locate) = '\0';
	return 0;
}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

static inline int walk_component(struct nameidata *nd, struct path *path,
		int follow)
{
	struct inode *inode;
	int err;
	/*
	 * "." and ".." are special - ".." especially so because it has
	 * to be able to know about the current root directory and
	 * parent relationships.
	 */
	if (unlikely(nd->last_type != LAST_NORM))
		return handle_dots(nd, nd->last_type);
	err = lookup_fast(nd, path, &inode);
	if (unlikely(err)) {
		if (err < 0)
			goto out_err;

		err = lookup_slow(nd, path);
		if (err < 0)
			goto out_err;

		inode = path->dentry->d_inode;
	}
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	/* After __follow_mount, next.dentry->d_name.name may be replaced to mnt path .
	 * The original path name is in next.mnt and it is what we need.
	*/
	if (LOOKUP_CASELESS_COMPARE & nd->flags) {
		int   target_len = 0;
		char *target_name = NULL;
		struct mount *mnt = NULL;

		if (nd->flags & LOOKUP_MOUNTED) {
			nd->flags &= ~LOOKUP_MOUNTED;
			mnt = real_mount(path->mnt);
			target_name = (char *)mnt->mnt_mountpoint->d_name.name;
			target_len = mnt->mnt_mountpoint->d_name.len;
		} else {
			target_name = (char *)path->dentry->d_name.name;
			target_len = path->dentry->d_name.len;
		}
		if (update_real_filename(nd, target_name, target_len)) {
			terminate_walk(nd);
			return -ENAMETOOLONG;
		}
	}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	err = -ENOENT;
	if (!inode)
		goto out_path_put;

	if (should_follow_link(inode, follow)) {
		if (nd->flags & LOOKUP_RCU) {
			if (unlikely(unlazy_walk(nd, path->dentry))) {
				err = -ECHILD;
				goto out_err;
			}
		}
		BUG_ON(inode != path->dentry->d_inode);
		return 1;
	}
	path_to_nameidata(path, nd);
	nd->inode = inode;
	return 0;

out_path_put:
	path_to_nameidata(path, nd);
out_err:
	terminate_walk(nd);
	return err;
}

/*
 * This limits recursive symlink follows to 8, while
 * limiting consecutive symlinks to 40.
 *
 * Without that kind of total limit, nasty chains of consecutive
 * symlinks can cause almost arbitrarily long lookups.
 */
static inline int nested_symlink(struct path *path, struct nameidata *nd)
{
	int res;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	int caseless_set = 0;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

	if (unlikely(current->link_count >= MAX_NESTED_LINKS)) {
		path_put_conditional(path, nd);
		path_put(&nd->path);
		return -ELOOP;
	}
	BUG_ON(nd->depth >= MAX_NESTED_LINKS);

	nd->depth++;
	current->link_count++;

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	if (LOOKUP_CASELESS_COMPARE & nd->flags) {
		caseless_set = 1;
		nd->flags &= ~LOOKUP_CASELESS_COMPARE;
	}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	do {
		struct path link = *path;
		void *cookie;

		res = follow_link(&link, nd, &cookie);
		if (res)
			break;
		res = walk_component(nd, path, LOOKUP_FOLLOW);
		put_link(nd, &link, cookie);
	} while (res > 0);
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	if (caseless_set) {
		nd->flags |= LOOKUP_CASELESS_COMPARE;
	}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

	current->link_count--;
	nd->depth--;
	return res;
}

/*
 * We really don't want to look at inode->i_op->lookup
 * when we don't have to. So we keep a cache bit in
 * the inode ->i_opflags field that says "yes, we can
 * do lookup on this inode".
 */
static inline int can_lookup(struct inode *inode)
{
	if (likely(inode->i_opflags & IOP_LOOKUP))
		return 1;
	if (likely(!inode->i_op->lookup))
		return 0;

	/* We do this once for the lifetime of the inode */
	spin_lock(&inode->i_lock);
	inode->i_opflags |= IOP_LOOKUP;
	spin_unlock(&inode->i_lock);
	return 1;
}

/*
 * We can do the critical dentry name comparison and hashing
 * operations one word at a time, but we are limited to:
 *
 * - Architectures with fast unaligned word accesses. We could
 *   do a "get_unaligned()" if this helps and is sufficiently
 *   fast.
 *
 * - Little-endian machines (so that we can generate the mask
 *   of low bytes efficiently). Again, we *could* do a byte
 *   swapping load on big-endian architectures if that is not
 *   expensive enough to make the optimization worthless.
 *
 * - non-CONFIG_DEBUG_PAGEALLOC configurations (so that we
 *   do not trap on the (extremely unlikely) case of a page
 *   crossing operation.
 *
 * - Furthermore, we need an efficient 64-bit compile for the
 *   64-bit case in order to generate the "number of bytes in
 *   the final mask". Again, that could be replaced with a
 *   efficient population count instruction or similar.
 */
#ifdef CONFIG_DCACHE_WORD_ACCESS

#include <asm/word-at-a-time.h>

#ifdef CONFIG_64BIT

static inline unsigned int fold_hash(unsigned long hash)
{
	hash += hash >> (8*sizeof(int));
	return hash;
}

#else	/* 32-bit case */

#define fold_hash(x) (x)

#endif

unsigned int full_name_hash(const unsigned char *name, unsigned int len)
{
	unsigned long a, mask;
	unsigned long hash = 0;

	for (;;) {
		a = load_unaligned_zeropad(name);
		if (len < sizeof(unsigned long))
			break;
		hash += a;
		hash *= 9;
		name += sizeof(unsigned long);
		len -= sizeof(unsigned long);
		if (!len)
			goto done;
	}
	mask = ~(~0ul << len*8);
	hash += mask & a;
done:
	return fold_hash(hash);
}
EXPORT_SYMBOL(full_name_hash);

/*
 * Calculate the length and hash of the path component, and
 * return the length of the component;
 */
static inline unsigned long hash_name(const char *name, unsigned int *hashp)
{
	unsigned long a, b, adata, bdata, mask, hash, len;
	const struct word_at_a_time constants = WORD_AT_A_TIME_CONSTANTS;

	hash = a = 0;
	len = -sizeof(unsigned long);
	do {
		hash = (hash + a) * 9;
		len += sizeof(unsigned long);
		a = load_unaligned_zeropad(name+len);
		b = a ^ REPEAT_BYTE('/');
	} while (!(has_zero(a, &adata, &constants) | has_zero(b, &bdata, &constants)));

	adata = prep_zero_mask(a, adata, &constants);
	bdata = prep_zero_mask(b, bdata, &constants);

	mask = create_zero_mask(adata | bdata);

	hash += a & zero_bytemask(mask);
	*hashp = fold_hash(hash);

	return len + find_zero(mask);
}

#else

unsigned int full_name_hash(const unsigned char *name, unsigned int len)
{
	unsigned long hash = init_name_hash();
	while (len--)
		hash = partial_name_hash(*name++, hash);
	return end_name_hash(hash);
}
EXPORT_SYMBOL(full_name_hash);

/*
 * We know there's a real path component here of at least
 * one character.
 */
static inline unsigned long hash_name(const char *name, unsigned int *hashp)
{
	unsigned long hash = init_name_hash();
	unsigned long len = 0, c;

	c = (unsigned char)*name;
	do {
		len++;
		hash = partial_name_hash(c, hash);
		c = (unsigned char)name[len];
	} while (c && c != '/');
	*hashp = end_name_hash(hash);
	return len;
}

#endif

/*
 * Name resolution.
 * This is the basic name resolution function, turning a pathname into
 * the final dentry. We expect 'base' to be positive and a directory.
 *
 * Returns 0 and nd will have valid dentry and mnt on success.
 * Returns error and drops reference to input namei data on failure.
 */
static int link_path_walk(const char *name, struct nameidata *nd)
{
	struct path next;
	int err;

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	/* We do case conversions here.
	 * The filename converted will be stored in nd->real_filename.
	 *
	 * In ext3_find_entry (ext3_dx_find_entry and search_dirblock),
	 * we sync file name of dentry stored in dentry queue with filename founded from disk.
	 * So the filename of dentry returned by do_lookup is case converted.
	 *
	 * Note:
	 * 1. If stat success, it will be copied to user space.
	 *    It means if any error occurs, we don't need store anything in nd->real_filename.
	 * 2. We should correctly update "name" and "slashes" to "cur_location" each loop.
	 * 	  We update them in every "continue", "break", "return" point.
	 * 3. If converted string longer than SYNO_SMB_PSTRING_LEN, we should return ENAMETOOLONG.
	 * 	  We use total_len to monitor it.
	*/
	int caseless_flag = LOOKUP_CASELESS_COMPARE & nd->flags;

	while (*name=='/') {
		if (caseless_flag) {
			if (update_real_filename(nd, (char *) "", 0)) {
				terminate_walk(nd);
				return -ENAMETOOLONG;
			}
		}
		name++;
	}
#else
	while (*name=='/')
		name++;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	if (!*name)
		return 0;

	/* At this point we know we have a real path component. */
	for(;;) {
		struct qstr this;
		long len;
		int type;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		int slash_count = 0;

		nd->flags &= ~LOOKUP_MOUNTED;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

		err = may_lookup(nd);
 		if (err)
			break;

		len = hash_name(name, &this.hash);
		this.name = name;
		this.len = len;

		type = LAST_NORM;
		if (name[0] == '.') switch (len) {
			case 2:
				if (name[1] == '.') {
					type = LAST_DOTDOT;
					nd->flags |= LOOKUP_JUMPED;
				}
				break;
			case 1:
				type = LAST_DOT;
		}
		if (likely(type == LAST_NORM)) {
			struct dentry *parent = nd->path.dentry;
			nd->flags &= ~LOOKUP_JUMPED;
			if (unlikely(parent->d_flags & DCACHE_OP_HASH)) {
				err = parent->d_op->d_hash(parent, nd->inode,
							   &this);
				if (err < 0)
					break;
			}
		}

		nd->last = this;
		nd->last_type = type;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		if (!name[len]) {
			nd->flags |= LOOKUP_TO_LASTCOMPONENT;
		}
		if (caseless_flag && (LAST_DOTDOT == type || LAST_DOT == type)) {
			if (update_real_filename(nd, (char *) this.name, this.len)) {
				terminate_walk(nd);
				return -ENAMETOOLONG;
			}
		}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

		if (!name[len])
			return 0;
		/*
		 * If it wasn't NUL, we know it was '/'. Skip that
		 * slash, and continue until no more slashes.
		 */
		do {
			len++;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
			slash_count++;
			/* We will count one more slash because there will be one slash
			 * added in walk_component. Substract it back later.
			 */
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
		} while (unlikely(name[len] == '/'));
		if (!name[len])
			return 0;

		name += len;

		err = walk_component(nd, &next, LOOKUP_FOLLOW);
		if (err < 0)
			return err;

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		if (caseless_flag) {
			slash_count--; // Here we are to substract one slash.
			while (slash_count > 0) {
				if (update_real_filename(nd, (char *) "", 0)) {
					terminate_walk(nd);
					return -ENAMETOOLONG;
				}
				slash_count--;
			}
		}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
		if (err) {
			err = nested_symlink(&next, nd);
			if (err)
				return err;
		}
		if (!can_lookup(nd->inode)) {
			err = -ENOTDIR; 
			break;
		}
	}
	terminate_walk(nd);
	return err;
}

static int path_init(int dfd, const char *name, unsigned int flags,
		     struct nameidata *nd, struct file **fp)
{
	int retval = 0;

	nd->last_type = LAST_ROOT; /* if there are only slashes... */
	nd->flags = flags | LOOKUP_JUMPED;
	nd->depth = 0;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	if (flags & LOOKUP_CASELESS_COMPARE) {
		nd->real_filename_cur_locate = nd->real_filename;
		nd->real_filename_len = 0;
	}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

	if (flags & LOOKUP_ROOT) {
		struct inode *inode = nd->root.dentry->d_inode;
		if (*name) {
			if (!can_lookup(inode))
				return -ENOTDIR;
#ifdef CONFIG_SYNO_FS_WINACL
			if (IS_SYNOACL_INODE(inode, nd->root.dentry)) {
				retval = synoacl_op_exec_perm(nd->root.dentry, inode);
			} else {
				retval = inode_permission(inode, MAY_EXEC);
			}
#else
			retval = inode_permission(inode, MAY_EXEC);
#endif /* CONFIG_SYNO_FS_WINACL */
			if (retval)
				return retval;
		}
		nd->path = nd->root;
		nd->inode = inode;
		if (flags & LOOKUP_RCU) {
			lock_rcu_walk();
			nd->seq = __read_seqcount_begin(&nd->path.dentry->d_seq);
		} else {
			path_get(&nd->path);
		}
		return 0;
	}

	nd->root.mnt = NULL;

	if (*name=='/') {
		if (flags & LOOKUP_RCU) {
			lock_rcu_walk();
			set_root_rcu(nd);
		} else {
			set_root(nd);
			path_get(&nd->root);
		}
		nd->path = nd->root;
	} else if (dfd == AT_FDCWD) {
		if (flags & LOOKUP_RCU) {
			struct fs_struct *fs = current->fs;
			unsigned seq;

			lock_rcu_walk();

			do {
				seq = read_seqcount_begin(&fs->seq);
				nd->path = fs->pwd;
				nd->seq = __read_seqcount_begin(&nd->path.dentry->d_seq);
			} while (read_seqcount_retry(&fs->seq, seq));
		} else {
			get_fs_pwd(current->fs, &nd->path);
		}
	} else {
		/* Caller must check execute permissions on the starting path component */
		struct fd f = fdget_raw(dfd);
		struct dentry *dentry;

		if (!f.file)
			return -EBADF;

		dentry = f.file->f_path.dentry;

		if (*name) {
			if (!can_lookup(dentry->d_inode)) {
				fdput(f);
				return -ENOTDIR;
			}
		}

		nd->path = f.file->f_path;
		if (flags & LOOKUP_RCU) {
			if (f.need_put)
				*fp = f.file;
			nd->seq = __read_seqcount_begin(&nd->path.dentry->d_seq);
			lock_rcu_walk();
		} else {
			path_get(&nd->path);
			fdput(f);
		}
	}

	nd->inode = nd->path.dentry->d_inode;
	return 0;
}

static inline int lookup_last(struct nameidata *nd, struct path *path)
{
	if (nd->last_type == LAST_NORM && nd->last.name[nd->last.len])
		nd->flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	if (LOOKUP_CASELESS_COMPARE & nd->flags) {
		nd->flags &= ~LOOKUP_MOUNTED;
	}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	nd->flags &= ~LOOKUP_PARENT;
	return walk_component(nd, path, nd->flags & LOOKUP_FOLLOW);
}

/* Returns 0 and nd will be valid on success; Retuns error, otherwise. */
static int path_lookupat(int dfd, const char *name,
				unsigned int flags, struct nameidata *nd)
{
	struct file *base = NULL;
	struct path path;
	int err;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	int caseless_set = 0;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

	/*
	 * Path walking is largely split up into 2 different synchronisation
	 * schemes, rcu-walk and ref-walk (explained in
	 * Documentation/filesystems/path-lookup.txt). These share much of the
	 * path walk code, but some things particularly setup, cleanup, and
	 * following mounts are sufficiently divergent that functions are
	 * duplicated. Typically there is a function foo(), and its RCU
	 * analogue, foo_rcu().
	 *
	 * -ECHILD is the error number of choice (just to avoid clashes) that
	 * is returned if some aspect of an rcu-walk fails. Such an error must
	 * be handled by restarting a traditional ref-walk (which will always
	 * be able to complete).
	 */
	err = path_init(dfd, name, flags | LOOKUP_PARENT, nd, &base);

	if (unlikely(err))
		return err;

	current->total_link_count = 0;
	err = link_path_walk(name, nd);

	if (!err && !(flags & LOOKUP_PARENT)) {
		err = lookup_last(nd, &path);
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		if (LOOKUP_CASELESS_COMPARE & nd->flags) {
			caseless_set = 1;
			nd->flags &= ~LOOKUP_CASELESS_COMPARE;
		}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
		while (err > 0) {
			void *cookie;
			struct path link = path;
			err = may_follow_link(&link, nd);
			if (unlikely(err))
				break;
			nd->flags |= LOOKUP_PARENT;
			err = follow_link(&link, nd, &cookie);
			if (err)
				break;
			err = lookup_last(nd, &path);
			put_link(nd, &link, cookie);
		}
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		if (caseless_set) {
			nd->flags |= LOOKUP_CASELESS_COMPARE;
		}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	}

	if (!err)
		err = complete_walk(nd);

	if (!err && nd->flags & LOOKUP_DIRECTORY) {
		if (!can_lookup(nd->inode)) {
			path_put(&nd->path);
			err = -ENOTDIR;
		}
	}

	if (base)
		fput(base);

	if (nd->root.mnt && !(nd->flags & LOOKUP_ROOT)) {
		path_put(&nd->root);
		nd->root.mnt = NULL;
	}
	return err;
}

static int filename_lookup(int dfd, struct filename *name,
				unsigned int flags, struct nameidata *nd)
{
#ifdef CONFIG_SYNO_FS_REMOVE_RCU_WALK_PATH
	int retval = path_lookupat(dfd, name->name, flags, nd);
#else
	int retval = path_lookupat(dfd, name->name, flags | LOOKUP_RCU, nd);
	if (unlikely(retval == -ECHILD))
		retval = path_lookupat(dfd, name->name, flags, nd);
#endif /* CONFIG_SYNO_FS_REMOVE_RCU_WALK_PATH */
	if (unlikely(retval == -ESTALE))
		retval = path_lookupat(dfd, name->name,
						flags | LOOKUP_REVAL, nd);

	if (likely(!retval))
		audit_inode(name, nd->path.dentry, flags & LOOKUP_PARENT);
	return retval;
}

static int do_path_lookup(int dfd, const char *name,
				unsigned int flags, struct nameidata *nd)
{
	struct filename filename = { .name = name };

	return filename_lookup(dfd, &filename, flags, nd);
}

/* does lookup, returns the object with parent locked */
struct dentry *kern_path_locked(const char *name, struct path *path)
{
	struct nameidata nd;
	struct dentry *d;
	int err = do_path_lookup(AT_FDCWD, name, LOOKUP_PARENT, &nd);
	if (err)
		return ERR_PTR(err);
	if (nd.last_type != LAST_NORM) {
		path_put(&nd.path);
		return ERR_PTR(-EINVAL);
	}
	mutex_lock_nested(&nd.path.dentry->d_inode->i_mutex, I_MUTEX_PARENT);
	d = __lookup_hash(&nd.last, nd.path.dentry, 0);
	if (IS_ERR(d)) {
		mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
		path_put(&nd.path);
		return d;
	}
	*path = nd.path;
	return d;
}

int kern_path(const char *name, unsigned int flags, struct path *path)
{
	struct nameidata nd;
	int res = do_path_lookup(AT_FDCWD, name, flags, &nd);
	if (!res)
		*path = nd.path;
	return res;
}

/**
 * vfs_path_lookup - lookup a file path relative to a dentry-vfsmount pair
 * @dentry:  pointer to dentry of the base directory
 * @mnt: pointer to vfs mount of the base directory
 * @name: pointer to file name
 * @flags: lookup flags
 * @path: pointer to struct path to fill
 */
int vfs_path_lookup(struct dentry *dentry, struct vfsmount *mnt,
		    const char *name, unsigned int flags,
		    struct path *path)
{
	struct nameidata nd;
	int err;
	nd.root.dentry = dentry;
	nd.root.mnt = mnt;
	BUG_ON(flags & LOOKUP_PARENT);
	/* the first argument of do_path_lookup() is ignored with LOOKUP_ROOT */
	err = do_path_lookup(AT_FDCWD, name, flags | LOOKUP_ROOT, &nd);
	if (!err)
		*path = nd.path;
	return err;
}

/*
 * Restricted form of lookup. Doesn't follow links, single-component only,
 * needs parent already locked. Doesn't follow mounts.
 * SMP-safe.
 */
#ifdef CONFIG_SYNO_FS_EXPORT_SYMBOL_LOOKUP_HASH
extern struct dentry *lookup_hash(struct nameidata *nd)
#else /* !CONFIG_SYNO_FS_EXPORT_SYMBOL_LOOKUP_HASH */
static struct dentry *lookup_hash(struct nameidata *nd)
#endif /* CONFIG_SYNO_FS_EXPORT_SYMBOL_LOOKUP_HASH */
{
	return __lookup_hash(&nd->last, nd->path.dentry, nd->flags);
}

/**
 * lookup_one_len - filesystem helper to lookup single pathname component
 * @name:	pathname component to lookup
 * @base:	base directory to lookup from
 * @len:	maximum length @len should be interpreted to
 *
 * Note that this routine is purely a helper for filesystem usage and should
 * not be called by generic code.  Also note that by using this function the
 * nameidata argument is passed to the filesystem methods and a filesystem
 * using this helper needs to be prepared for that.
 */
struct dentry *lookup_one_len(const char *name, struct dentry *base, int len)
{
	struct qstr this;
	unsigned int c;
	int err;

	WARN_ON_ONCE(!mutex_is_locked(&base->d_inode->i_mutex));

	this.name = name;
	this.len = len;
	this.hash = full_name_hash(name, len);
	if (!len)
		return ERR_PTR(-EACCES);

	if (unlikely(name[0] == '.')) {
		if (len < 2 || (len == 2 && name[1] == '.'))
			return ERR_PTR(-EACCES);
	}

	while (len--) {
		c = *(const unsigned char *)name++;
		if (c == '/' || c == '\0')
			return ERR_PTR(-EACCES);
	}
	/*
	 * See if the low-level filesystem might want
	 * to use its own hash..
	 */
	if (base->d_flags & DCACHE_OP_HASH) {
		int err = base->d_op->d_hash(base, base->d_inode, &this);
		if (err < 0)
			return ERR_PTR(err);
	}

#ifdef CONFIG_SYNO_FS_WINACL
	if (IS_SYNOACL(base)) {
		err = synoacl_op_exec_perm(base, base->d_inode);
	} else {
		err = inode_permission(base->d_inode, MAY_EXEC);
	}
#else
	err = inode_permission(base->d_inode, MAY_EXEC);
#endif /* CONFIG_SYNO_FS_WINACL */
	if (err)
		return ERR_PTR(err);

	return __lookup_hash(&this, base, 0);
}

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
int syno_user_path_at(int dfd, const char __user *name, unsigned flags,
		 struct path *path, char **real_filename, int *real_filename_len, int *last_component)
{
	struct nameidata nd;
	struct filename *tmp = getname(name);
	int err = PTR_ERR(tmp);
	if (!IS_ERR(tmp)) {

		BUG_ON(flags & LOOKUP_PARENT);

		nd.real_filename = *real_filename;
		nd.real_filename_cur_locate = nd.real_filename;
		nd.real_filename_len = 0;
		err = do_path_lookup(dfd, tmp->name, flags, &nd);
		putname(tmp);
		if (!err) {
			*path = nd.path;
			*real_filename_len = nd.real_filename_len;
			if (nd.flags & LOOKUP_TO_LASTCOMPONENT) {
				*last_component = 1;
			}
		}
	}
	return err;
}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

int user_path_at_empty(int dfd, const char __user *name, unsigned flags,
		 struct path *path, int *empty)
{
	struct nameidata nd;
	struct filename *tmp = getname_flags(name, flags, empty);
	int err = PTR_ERR(tmp);
	if (!IS_ERR(tmp)) {

		BUG_ON(flags & LOOKUP_PARENT);

		err = filename_lookup(dfd, tmp, flags, &nd);
		putname(tmp);
		if (!err)
			*path = nd.path;
	}
	return err;
}

int user_path_at(int dfd, const char __user *name, unsigned flags,
		 struct path *path)
{
	return user_path_at_empty(dfd, name, flags, path, NULL);
}

/*
 * NB: most callers don't do anything directly with the reference to the
 *     to struct filename, but the nd->last pointer points into the name string
 *     allocated by getname. So we must hold the reference to it until all
 *     path-walking is complete.
 */
static struct filename *
user_path_parent(int dfd, const char __user *path, struct nameidata *nd,
		 unsigned int flags)
{
	struct filename *s = getname(path);
	int error;

	/* only LOOKUP_REVAL is allowed in extra flags */
	flags &= LOOKUP_REVAL;

	if (IS_ERR(s))
		return s;

	error = filename_lookup(dfd, s, flags | LOOKUP_PARENT, nd);
	if (error) {
		putname(s);
		return ERR_PTR(error);
	}

	return s;
}

/*
 * It's inline, so penalty for filesystems that don't use sticky bit is
 * minimal.
 */
static inline int check_sticky(struct inode *dir, struct inode *inode)
{
	kuid_t fsuid = current_fsuid();

	if (!(dir->i_mode & S_ISVTX))
		return 0;
	if (uid_eq(inode->i_uid, fsuid))
		return 0;
	if (uid_eq(dir->i_uid, fsuid))
		return 0;
	return !inode_capable(inode, CAP_FOWNER);
}

/*
 *	Check whether we can remove a link victim from directory dir, check
 *  whether the type of victim is right.
 *  1. We can't do it if dir is read-only (done in permission())
 *  2. We should have write and exec permissions on dir
 *  3. We can't remove anything from append-only dir
 *  4. We can't do anything with immutable dir (done in permission())
 *  5. If the sticky bit on dir is set we should either
 *	a. be owner of dir, or
 *	b. be owner of victim, or
 *	c. have CAP_FOWNER capability
 *  6. If the victim is append-only or immutable we can't do antyhing with
 *     links pointing to it.
 *  7. If we were asked to remove a directory and victim isn't one - ENOTDIR.
 *  8. If we were asked to remove a non-directory and victim isn't one - EISDIR.
 *  9. We can't remove a root or mountpoint.
 * 10. We don't allow removal of NFS sillyrenamed files; it's handled by
 *     nfs_async_unlink().
 */
static int may_delete(struct inode *dir,struct dentry *victim,int isdir)
{
	int error;

	if (!victim->d_inode)
		return -ENOENT;

	BUG_ON(victim->d_parent->d_inode != dir);
	audit_inode_child(dir, victim, AUDIT_TYPE_CHILD_DELETE);

#ifdef CONFIG_SYNO_FS_WINACL
	if (IS_FS_SYNOACL(dir)) {
		error = synoacl_op_may_delete(victim, dir);
	} else
#endif /* CONFIG_SYNO_FS_WINACL */
	error = inode_permission(dir, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;
	if (IS_APPEND(dir))
		return -EPERM;
#ifdef CONFIG_SYNO_FS_WINACL
	if (!IS_SYNOACL(victim->d_parent) && check_sticky(dir, victim->d_inode)) {
		return -EPERM;
	}
	if (IS_APPEND(victim->d_inode) || IS_IMMUTABLE(victim->d_inode) || IS_SWAPFILE(victim->d_inode)){
		return -EPERM;
	}
#else
	if (check_sticky(dir, victim->d_inode)||IS_APPEND(victim->d_inode)||
	    IS_IMMUTABLE(victim->d_inode) || IS_SWAPFILE(victim->d_inode))
		return -EPERM;
#endif /* CONFIG_SYNO_FS_WINACL */
	if (isdir) {
		if (!S_ISDIR(victim->d_inode->i_mode))
			return -ENOTDIR;
		if (IS_ROOT(victim))
			return -EBUSY;
	} else if (S_ISDIR(victim->d_inode->i_mode))
		return -EISDIR;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	if (victim->d_flags & DCACHE_NFSFS_RENAMED)
		return -EBUSY;
	return 0;
}

/*	Check whether we can create an object with dentry child in directory
 *  dir.
 *  1. We can't do it if child already exists (open has special treatment for
 *     this case, but since we are inlined it's OK)
 *  2. We can't do it if dir is read-only (done in permission())
 *  3. We should have write and exec permissions on dir
 *  4. We can't do it if dir is immutable (done in permission())
 */
#ifdef CONFIG_SYNO_FS_WINACL
static inline int may_create(struct inode *dir, struct dentry *child, int mode)
#else 
static inline int may_create(struct inode *dir, struct dentry *child)
#endif /* CONFIG_SYNO_FS_WINACL */
{
	audit_inode_child(dir, child, AUDIT_TYPE_CHILD_CREATE);
	if (child->d_inode)
		return -EEXIST;
	if (IS_DEADDIR(dir))
		return -ENOENT;

#ifdef CONFIG_SYNO_FS_WINACL
	if (IS_SYNOACL(child->d_parent)) {
		return synoacl_op_perm(child->d_parent, (S_ISDIR(mode)?MAY_APPEND:MAY_WRITE) | MAY_EXEC);
	} 
#endif /* CONFIG_SYNO_FS_WINACL */
	return inode_permission(dir, MAY_WRITE | MAY_EXEC);
}

/*
 * p1 and p2 should be directories on the same fs.
 */
struct dentry *lock_rename(struct dentry *p1, struct dentry *p2)
{
	struct dentry *p;

	if (p1 == p2) {
		mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_PARENT);
		return NULL;
	}

	mutex_lock(&p1->d_inode->i_sb->s_vfs_rename_mutex);

	p = d_ancestor(p2, p1);
	if (p) {
		mutex_lock_nested(&p2->d_inode->i_mutex, I_MUTEX_PARENT);
		mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_CHILD);
		return p;
	}

	p = d_ancestor(p1, p2);
	if (p) {
		mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_PARENT);
		mutex_lock_nested(&p2->d_inode->i_mutex, I_MUTEX_CHILD);
		return p;
	}

	mutex_lock_nested(&p1->d_inode->i_mutex, I_MUTEX_PARENT);
	mutex_lock_nested(&p2->d_inode->i_mutex, I_MUTEX_CHILD);
	return NULL;
}

void unlock_rename(struct dentry *p1, struct dentry *p2)
{
	mutex_unlock(&p1->d_inode->i_mutex);
	if (p1 != p2) {
		mutex_unlock(&p2->d_inode->i_mutex);
		mutex_unlock(&p1->d_inode->i_sb->s_vfs_rename_mutex);
	}
}

int vfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		bool want_excl)
{
#ifdef CONFIG_SYNO_FS_WINACL
	int error = may_create(dir, dentry, S_IFREG);
#else
	int error = may_create(dir, dentry);
#endif /* CONFIG_SYNO_FS_WINACL */

	if (error)
		return error;

	if (!dir->i_op->create)
		return -EACCES;	/* shouldn't it be ENOSYS? */
	mode &= S_IALLUGO;
	mode |= S_IFREG;
	error = security_inode_create(dir, dentry, mode);
	if (error)
		return error;
	error = dir->i_op->create(dir, dentry, mode, want_excl);
	if (!error)
		fsnotify_create(dir, dentry);

#ifdef CONFIG_SYNO_FS_WINACL
	if (!error && IS_SYNOACL(dentry->d_parent)) {
		//We assume that inode has been attached to dentry by d_instantiate().
		synoacl_op_init(dentry);
	}
#endif /* CONFIG_SYNO_FS_WINACL */

	return error;
}

static int may_open(struct path *path, int acc_mode, int flag)
{
	struct dentry *dentry = path->dentry;
	struct inode *inode = dentry->d_inode;
	int error;

	/* O_PATH? */
	if (!acc_mode)
		return 0;

	if (!inode)
		return -ENOENT;

	switch (inode->i_mode & S_IFMT) {
	case S_IFLNK:
		return -ELOOP;
	case S_IFDIR:
		if (acc_mode & MAY_WRITE)
			return -EISDIR;
		break;
	case S_IFBLK:
	case S_IFCHR:
		if (path->mnt->mnt_flags & MNT_NODEV)
			return -EACCES;
		/*FALLTHRU*/
	case S_IFIFO:
	case S_IFSOCK:
		flag &= ~O_TRUNC;
		break;
	}

#ifdef CONFIG_SYNO_FS_WINACL
	if (IS_SYNOACL(dentry)) {
		error = synoacl_op_perm(dentry, acc_mode);
	} else
#endif /* CONFIG_SYNO_FS_WINACL */
	error = inode_permission(inode, acc_mode);
	if (error)
		return error;

	/*
	 * An append-only file must be opened in append mode for writing.
	 */
	if (IS_APPEND(inode)) {
		if  ((flag & O_ACCMODE) != O_RDONLY && !(flag & O_APPEND))
			return -EPERM;
		if (flag & O_TRUNC)
			return -EPERM;
	}

	/* O_NOATIME can only be set by the owner or superuser */
	if (flag & O_NOATIME && !inode_owner_or_capable(inode))
		return -EPERM;

	return 0;
}

static int handle_truncate(struct file *filp)
{
	struct path *path = &filp->f_path;
	struct inode *inode = path->dentry->d_inode;
	int error = get_write_access(inode);
	if (error)
		return error;
	/*
	 * Refuse to truncate files with mandatory locks held on them.
	 */
	error = locks_verify_locked(inode);
	if (!error)
		error = security_path_truncate(path);
	if (!error) {
		error = do_truncate(path->dentry, 0,
				    ATTR_MTIME|ATTR_CTIME|ATTR_OPEN,
				    filp);
	}
	put_write_access(inode);
	return error;
}

static inline int open_to_namei_flags(int flag)
{
	if ((flag & O_ACCMODE) == 3)
		flag--;
	return flag;
}

static int may_o_create(struct path *dir, struct dentry *dentry, umode_t mode)
{
	int error = security_path_mknod(dir, dentry, mode, 0);
	if (error)
		return error;

#ifdef CONFIG_SYNO_FS_WINACL
	if (IS_SYNOACL(dir->dentry)) {
		error = synoacl_op_perm(dir->dentry, (S_ISDIR(mode)?MAY_APPEND:MAY_WRITE) | MAY_EXEC);
	} else
#endif /* CONFIG_SYNO_FS_WINACL */
	error = inode_permission(dir->dentry->d_inode, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;

	return security_inode_create(dir->dentry->d_inode, dentry, mode);
}

/*
 * Attempt to atomically look up, create and open a file from a negative
 * dentry.
 *
 * Returns 0 if successful.  The file will have been created and attached to
 * @file by the filesystem calling finish_open().
 *
 * Returns 1 if the file was looked up only or didn't need creating.  The
 * caller will need to perform the open themselves.  @path will have been
 * updated to point to the new dentry.  This may be negative.
 *
 * Returns an error code otherwise.
 */
static int atomic_open(struct nameidata *nd, struct dentry *dentry,
			struct path *path, struct file *file,
			const struct open_flags *op,
			bool got_write, bool need_lookup,
			int *opened)
{
	struct inode *dir =  nd->path.dentry->d_inode;
	unsigned open_flag = open_to_namei_flags(op->open_flag);
	umode_t mode;
	int error;
	int acc_mode;
	int create_error = 0;
	struct dentry *const DENTRY_NOT_SET = (void *) -1UL;

	BUG_ON(dentry->d_inode);

	/* Don't create child dentry for a dead directory. */
	if (unlikely(IS_DEADDIR(dir))) {
		error = -ENOENT;
		goto out;
	}

	mode = op->mode;
	if ((open_flag & O_CREAT) && !IS_POSIXACL(dir))
		mode &= ~current_umask();

	if ((open_flag & (O_EXCL | O_CREAT)) == (O_EXCL | O_CREAT)) {
		open_flag &= ~O_TRUNC;
		*opened |= FILE_CREATED;
	}

	/*
	 * Checking write permission is tricky, bacuse we don't know if we are
	 * going to actually need it: O_CREAT opens should work as long as the
	 * file exists.  But checking existence breaks atomicity.  The trick is
	 * to check access and if not granted clear O_CREAT from the flags.
	 *
	 * Another problem is returing the "right" error value (e.g. for an
	 * O_EXCL open we want to return EEXIST not EROFS).
	 */
	if (((open_flag & (O_CREAT | O_TRUNC)) ||
	    (open_flag & O_ACCMODE) != O_RDONLY) && unlikely(!got_write)) {
		if (!(open_flag & O_CREAT)) {
			/*
			 * No O_CREATE -> atomicity not a requirement -> fall
			 * back to lookup + open
			 */
			goto no_open;
		} else if (open_flag & (O_EXCL | O_TRUNC)) {
			/* Fall back and fail with the right error */
			create_error = -EROFS;
			goto no_open;
		} else {
			/* No side effects, safe to clear O_CREAT */
			create_error = -EROFS;
			open_flag &= ~O_CREAT;
		}
	}

	if (open_flag & O_CREAT) {
		error = may_o_create(&nd->path, dentry, mode);
		if (error) {
			create_error = error;
			if (open_flag & O_EXCL)
				goto no_open;
			open_flag &= ~O_CREAT;
		}
	}

	if (nd->flags & LOOKUP_DIRECTORY)
		open_flag |= O_DIRECTORY;

	file->f_path.dentry = DENTRY_NOT_SET;
	file->f_path.mnt = nd->path.mnt;
	error = dir->i_op->atomic_open(dir, dentry, file, open_flag, mode,
				      opened);
	if (error < 0) {
		if (create_error && error == -ENOENT)
			error = create_error;
		goto out;
	}

	acc_mode = op->acc_mode;
	if (*opened & FILE_CREATED) {
		fsnotify_create(dir, dentry);
		acc_mode = MAY_OPEN;
	}

	if (error) {	/* returned 1, that is */
		if (WARN_ON(file->f_path.dentry == DENTRY_NOT_SET)) {
			error = -EIO;
			goto out;
		}
		if (file->f_path.dentry) {
			dput(dentry);
			dentry = file->f_path.dentry;
		}
		if (create_error && dentry->d_inode == NULL) {
			error = create_error;
			goto out;
		}
		goto looked_up;
	}

	/*
	 * We didn't have the inode before the open, so check open permission
	 * here.
	 */
	error = may_open(&file->f_path, acc_mode, open_flag);
	if (error)
		fput(file);

out:
	dput(dentry);
	return error;

no_open:
	if (need_lookup) {
		dentry = lookup_real(dir, dentry, nd->flags);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);

		if (create_error) {
			int open_flag = op->open_flag;

			error = create_error;
			if ((open_flag & O_EXCL)) {
				if (!dentry->d_inode)
					goto out;
			} else if (!dentry->d_inode) {
				goto out;
			} else if ((open_flag & O_TRUNC) &&
				   S_ISREG(dentry->d_inode->i_mode)) {
				goto out;
			}
			/* will fail later, go on to get the right error */
		}
	}
looked_up:
	path->dentry = dentry;
	path->mnt = nd->path.mnt;
	return 1;
}

/*
 * Look up and maybe create and open the last component.
 *
 * Must be called with i_mutex held on parent.
 *
 * Returns 0 if the file was successfully atomically created (if necessary) and
 * opened.  In this case the file will be returned attached to @file.
 *
 * Returns 1 if the file was not completely opened at this time, though lookups
 * and creations will have been performed and the dentry returned in @path will
 * be positive upon return if O_CREAT was specified.  If O_CREAT wasn't
 * specified then a negative dentry may be returned.
 *
 * An error code is returned otherwise.
 *
 * FILE_CREATE will be set in @*opened if the dentry was created and will be
 * cleared otherwise prior to returning.
 */
static int lookup_open(struct nameidata *nd, struct path *path,
			struct file *file,
			const struct open_flags *op,
			bool got_write, int *opened)
{
	struct dentry *dir = nd->path.dentry;
	struct inode *dir_inode = dir->d_inode;
	struct dentry *dentry;
	int error;
	bool need_lookup;

	*opened &= ~FILE_CREATED;
	dentry = lookup_dcache(&nd->last, dir, nd->flags, &need_lookup);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* Cached positive dentry: will open in f_op->open */
	if (!need_lookup && dentry->d_inode)
		goto out_no_open;

	if ((nd->flags & LOOKUP_OPEN) && dir_inode->i_op->atomic_open) {
		return atomic_open(nd, dentry, path, file, op, got_write,
				   need_lookup, opened);
	}

	if (need_lookup) {
		BUG_ON(dentry->d_inode);

		dentry = lookup_real(dir_inode, dentry, nd->flags);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);
	}

	/* Negative dentry, just create the file */
	if (!dentry->d_inode && (op->open_flag & O_CREAT)) {
		umode_t mode = op->mode;
		if (!IS_POSIXACL(dir->d_inode))
			mode &= ~current_umask();
		/*
		 * This write is needed to ensure that a
		 * rw->ro transition does not occur between
		 * the time when the file is created and when
		 * a permanent write count is taken through
		 * the 'struct file' in finish_open().
		 */
		if (!got_write) {
			error = -EROFS;
			goto out_dput;
		}
		*opened |= FILE_CREATED;
		error = security_path_mknod(&nd->path, dentry, mode, 0);
		if (error)
			goto out_dput;
		error = vfs_create(dir->d_inode, dentry, mode,
				   nd->flags & LOOKUP_EXCL);
		if (error)
			goto out_dput;
	}
out_no_open:
	path->dentry = dentry;
	path->mnt = nd->path.mnt;
	return 1;

out_dput:
	dput(dentry);
	return error;
}

/*
 * Handle the last step of open()
 */
static int do_last(struct nameidata *nd, struct path *path,
		   struct file *file, const struct open_flags *op,
		   int *opened, struct filename *name)
{
	struct dentry *dir = nd->path.dentry;
	int open_flag = op->open_flag;
	bool will_truncate = (open_flag & O_TRUNC) != 0;
	bool got_write = false;
	int acc_mode = op->acc_mode;
	struct inode *inode;
	bool symlink_ok = false;
	struct path save_parent = { .dentry = NULL, .mnt = NULL };
	bool retried = false;
	int error;

	nd->flags &= ~LOOKUP_PARENT;
	nd->flags |= op->intent;

	switch (nd->last_type) {
	case LAST_DOTDOT:
	case LAST_DOT:
		error = handle_dots(nd, nd->last_type);
		if (error)
			return error;
		/* fallthrough */
	case LAST_ROOT:
		error = complete_walk(nd);
		if (error)
			return error;
		audit_inode(name, nd->path.dentry, 0);
		if (open_flag & O_CREAT) {
			error = -EISDIR;
			goto out;
		}
		goto finish_open;
	case LAST_BIND:
		error = complete_walk(nd);
		if (error)
			return error;
		audit_inode(name, dir, 0);
		goto finish_open;
	}

	if (!(open_flag & O_CREAT)) {
		if (nd->last.name[nd->last.len])
			nd->flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;
		if (open_flag & O_PATH && !(nd->flags & LOOKUP_FOLLOW))
			symlink_ok = true;
		/* we _can_ be in RCU mode here */
		error = lookup_fast(nd, path, &inode);
		if (likely(!error))
			goto finish_lookup;

		if (error < 0)
			goto out;

		BUG_ON(nd->inode != dir->d_inode);
	} else {
		/* create side of things */
		/*
		 * This will *only* deal with leaving RCU mode - LOOKUP_JUMPED
		 * has been cleared when we got to the last component we are
		 * about to look up
		 */
		error = complete_walk(nd);
		if (error)
			return error;

		audit_inode(name, dir, LOOKUP_PARENT);
		error = -EISDIR;
		/* trailing slashes? */
		if (nd->last.name[nd->last.len])
			goto out;
	}

retry_lookup:
	if (op->open_flag & (O_CREAT | O_TRUNC | O_WRONLY | O_RDWR)) {
		error = mnt_want_write(nd->path.mnt);
		if (!error)
			got_write = true;
		/*
		 * do _not_ fail yet - we might not need that or fail with
		 * a different error; let lookup_open() decide; we'll be
		 * dropping this one anyway.
		 */
	}
	mutex_lock(&dir->d_inode->i_mutex);
	error = lookup_open(nd, path, file, op, got_write, opened);
	mutex_unlock(&dir->d_inode->i_mutex);

	if (error <= 0) {
		if (error)
			goto out;

		if ((*opened & FILE_CREATED) ||
		    !S_ISREG(file_inode(file)->i_mode))
			will_truncate = false;

		audit_inode(name, file->f_path.dentry, 0);
		goto opened;
	}

	if (*opened & FILE_CREATED) {
		/* Don't check for write permission, don't truncate */
		open_flag &= ~O_TRUNC;
		will_truncate = false;
		acc_mode = MAY_OPEN;
		path_to_nameidata(path, nd);
		goto finish_open_created;
	}

	/*
	 * create/update audit record if it already exists.
	 */
	if (path->dentry->d_inode)
		audit_inode(name, path->dentry, 0);

	/*
	 * If atomic_open() acquired write access it is dropped now due to
	 * possible mount and symlink following (this might be optimized away if
	 * necessary...)
	 */
	if (got_write) {
		mnt_drop_write(nd->path.mnt);
		got_write = false;
	}

	error = -EEXIST;
	if ((open_flag & (O_EXCL | O_CREAT)) == (O_EXCL | O_CREAT))
		goto exit_dput;

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	error = follow_managed(path, nd);
#else
	error = follow_managed(path, nd->flags);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	if (error < 0)
		goto exit_dput;

	if (error)
		nd->flags |= LOOKUP_JUMPED;

	BUG_ON(nd->flags & LOOKUP_RCU);
	inode = path->dentry->d_inode;
finish_lookup:
	/* we _can_ be in RCU mode here */
	error = -ENOENT;
	if (!inode) {
		path_to_nameidata(path, nd);
		goto out;
	}

	if (should_follow_link(inode, !symlink_ok)) {
		if (nd->flags & LOOKUP_RCU) {
			if (unlikely(unlazy_walk(nd, path->dentry))) {
				error = -ECHILD;
				goto out;
			}
		}
		BUG_ON(inode != path->dentry->d_inode);
		return 1;
	}

	if ((nd->flags & LOOKUP_RCU) || nd->path.mnt != path->mnt) {
		path_to_nameidata(path, nd);
	} else {
		save_parent.dentry = nd->path.dentry;
		save_parent.mnt = mntget(path->mnt);
		nd->path.dentry = path->dentry;

	}
	nd->inode = inode;
	/* Why this, you ask?  _Now_ we might have grown LOOKUP_JUMPED... */
	error = complete_walk(nd);
	if (error) {
		path_put(&save_parent);
		return error;
	}
	error = -EISDIR;
	if ((open_flag & O_CREAT) && S_ISDIR(nd->inode->i_mode))
		goto out;
	error = -ENOTDIR;
	if ((nd->flags & LOOKUP_DIRECTORY) && !can_lookup(nd->inode))
		goto out;
	audit_inode(name, nd->path.dentry, 0);
finish_open:
	if (!S_ISREG(nd->inode->i_mode))
		will_truncate = false;

	if (will_truncate) {
		error = mnt_want_write(nd->path.mnt);
		if (error)
			goto out;
		got_write = true;
	}
finish_open_created:
	error = may_open(&nd->path, acc_mode, open_flag);
	if (error)
		goto out;
	file->f_path.mnt = nd->path.mnt;
	error = finish_open(file, nd->path.dentry, NULL, opened);
	if (error) {
		if (error == -EOPENSTALE)
			goto stale_open;
		goto out;
	}
opened:
	error = open_check_o_direct(file);
	if (error)
		goto exit_fput;
	error = ima_file_check(file, op->acc_mode);
	if (error)
		goto exit_fput;

	if (will_truncate) {
		error = handle_truncate(file);
		if (error)
			goto exit_fput;
	}
out:
	if (got_write)
		mnt_drop_write(nd->path.mnt);
	path_put(&save_parent);
	terminate_walk(nd);
	return error;

exit_dput:
	path_put_conditional(path, nd);
	goto out;
exit_fput:
	fput(file);
	goto out;

stale_open:
	/* If no saved parent or already retried then can't retry */
	if (!save_parent.dentry || retried)
		goto out;

	BUG_ON(save_parent.dentry != dir);
	path_put(&nd->path);
	nd->path = save_parent;
	nd->inode = dir->d_inode;
	save_parent.mnt = NULL;
	save_parent.dentry = NULL;
	if (got_write) {
		mnt_drop_write(nd->path.mnt);
		got_write = false;
	}
	retried = true;
	goto retry_lookup;
}

static struct file *path_openat(int dfd, struct filename *pathname,
		struct nameidata *nd, const struct open_flags *op, int flags)
{
	struct file *base = NULL;
	struct file *file;
	struct path path;
	int opened = 0;
	int error;

	file = get_empty_filp();
	if (IS_ERR(file))
		return file;

	file->f_flags = op->open_flag;

	error = path_init(dfd, pathname->name, flags | LOOKUP_PARENT, nd, &base);
	if (unlikely(error))
		goto out;

	current->total_link_count = 0;
	error = link_path_walk(pathname->name, nd);
	if (unlikely(error))
		goto out;

	error = do_last(nd, &path, file, op, &opened, pathname);
	while (unlikely(error > 0)) { /* trailing symlink */
		struct path link = path;
		void *cookie;
		if (!(nd->flags & LOOKUP_FOLLOW)) {
			path_put_conditional(&path, nd);
			path_put(&nd->path);
			error = -ELOOP;
			break;
		}
		error = may_follow_link(&link, nd);
		if (unlikely(error))
			break;
		nd->flags |= LOOKUP_PARENT;
		nd->flags &= ~(LOOKUP_OPEN|LOOKUP_CREATE|LOOKUP_EXCL);
		error = follow_link(&link, nd, &cookie);
		if (unlikely(error))
			break;
		error = do_last(nd, &path, file, op, &opened, pathname);
		put_link(nd, &link, cookie);
	}
out:
	if (nd->root.mnt && !(nd->flags & LOOKUP_ROOT))
		path_put(&nd->root);
	if (base)
		fput(base);
	if (!(opened & FILE_OPENED)) {
		BUG_ON(!error);
		put_filp(file);
	}
	if (unlikely(error)) {
		if (error == -EOPENSTALE) {
			if (flags & LOOKUP_RCU)
				error = -ECHILD;
			else
				error = -ESTALE;
		}
		file = ERR_PTR(error);
	}
	return file;
}

struct file *do_filp_open(int dfd, struct filename *pathname,
		const struct open_flags *op, int flags)
{
	struct nameidata nd;
	struct file *filp;

#ifdef CONFIG_SYNO_FS_REMOVE_RCU_WALK_PATH
	filp = path_openat(dfd, pathname, &nd, op, flags);
#else
	filp = path_openat(dfd, pathname, &nd, op, flags | LOOKUP_RCU);
	if (unlikely(filp == ERR_PTR(-ECHILD)))
		filp = path_openat(dfd, pathname, &nd, op, flags);
#endif /* CONFIG_SYNO_FS_REMOVE_RCU_WALK_PATH */
	if (unlikely(filp == ERR_PTR(-ESTALE)))
		filp = path_openat(dfd, pathname, &nd, op, flags | LOOKUP_REVAL);
	return filp;
}

struct file *do_file_open_root(struct dentry *dentry, struct vfsmount *mnt,
		const char *name, const struct open_flags *op, int flags)
{
	struct nameidata nd;
	struct file *file;
	struct filename filename = { .name = name };

	nd.root.mnt = mnt;
	nd.root.dentry = dentry;

	flags |= LOOKUP_ROOT;

	if (dentry->d_inode->i_op->follow_link && op->intent & LOOKUP_OPEN)
		return ERR_PTR(-ELOOP);

#ifdef CONFIG_SYNO_FS_REMOVE_RCU_WALK_PATH
	file = path_openat(-1, &filename, &nd, op, flags);
#else
	file = path_openat(-1, &filename, &nd, op, flags | LOOKUP_RCU);
	if (unlikely(file == ERR_PTR(-ECHILD)))
		file = path_openat(-1, &filename, &nd, op, flags);
#endif /* CONFIG_SYNO_FS_REMOVE_RCU_WALK_PATH */
	if (unlikely(file == ERR_PTR(-ESTALE)))
		file = path_openat(-1, &filename, &nd, op, flags | LOOKUP_REVAL);
	return file;
}

struct dentry *kern_path_create(int dfd, const char *pathname,
				struct path *path, unsigned int lookup_flags)
{
	struct dentry *dentry = ERR_PTR(-EEXIST);
	struct nameidata nd;
	int err2;
	int error;
	bool is_dir = (lookup_flags & LOOKUP_DIRECTORY);

	/*
	 * Note that only LOOKUP_REVAL and LOOKUP_DIRECTORY matter here. Any
	 * other flags passed in are ignored!
	 */
	lookup_flags &= LOOKUP_REVAL;

	error = do_path_lookup(dfd, pathname, LOOKUP_PARENT|lookup_flags, &nd);
	if (error)
		return ERR_PTR(error);

	/*
	 * Yucky last component or no last component at all?
	 * (foo/., foo/.., /////)
	 */
	if (nd.last_type != LAST_NORM)
		goto out;
	nd.flags &= ~LOOKUP_PARENT;
	nd.flags |= LOOKUP_CREATE | LOOKUP_EXCL;

	/* don't fail immediately if it's r/o, at least try to report other errors */
	err2 = mnt_want_write(nd.path.mnt);
	/*
	 * Do the final lookup.
	 */
	mutex_lock_nested(&nd.path.dentry->d_inode->i_mutex, I_MUTEX_PARENT);
	dentry = lookup_hash(&nd);
	if (IS_ERR(dentry))
		goto unlock;

	error = -EEXIST;
	if (dentry->d_inode)
		goto fail;
	/*
	 * Special case - lookup gave negative, but... we had foo/bar/
	 * From the vfs_mknod() POV we just have a negative dentry -
	 * all is fine. Let's be bastards - you had / on the end, you've
	 * been asking for (non-existent) directory. -ENOENT for you.
	 */
	if (unlikely(!is_dir && nd.last.name[nd.last.len])) {
		error = -ENOENT;
		goto fail;
	}
	if (unlikely(err2)) {
		error = err2;
		goto fail;
	}
	*path = nd.path;
	return dentry;
fail:
	dput(dentry);
	dentry = ERR_PTR(error);
unlock:
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	if (!err2)
		mnt_drop_write(nd.path.mnt);
out:
	path_put(&nd.path);
	return dentry;
}
EXPORT_SYMBOL(kern_path_create);

void done_path_create(struct path *path, struct dentry *dentry)
{
	dput(dentry);
	mutex_unlock(&path->dentry->d_inode->i_mutex);
	mnt_drop_write(path->mnt);
	path_put(path);
}
EXPORT_SYMBOL(done_path_create);

struct dentry *user_path_create(int dfd, const char __user *pathname,
				struct path *path, unsigned int lookup_flags)
{
	struct filename *tmp = getname(pathname);
	struct dentry *res;
	if (IS_ERR(tmp))
		return ERR_CAST(tmp);
	res = kern_path_create(dfd, tmp->name, path, lookup_flags);
	putname(tmp);
	return res;
}
EXPORT_SYMBOL(user_path_create);

int vfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
#ifdef CONFIG_SYNO_FS_WINACL
	int error = may_create(dir, dentry, mode);
#else
	int error = may_create(dir, dentry);
#endif /* CONFIG_SYNO_FS_WINACL */

	if (error)
		return error;

	if ((S_ISCHR(mode) || S_ISBLK(mode)) && !capable(CAP_MKNOD))
		return -EPERM;

	if (!dir->i_op->mknod)
		return -EPERM;

	error = devcgroup_inode_mknod(mode, dev);
	if (error)
		return error;

	error = security_inode_mknod(dir, dentry, mode, dev);
	if (error)
		return error;

	error = dir->i_op->mknod(dir, dentry, mode, dev);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}

static int may_mknod(umode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFREG:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
	case 0: /* zero mode translates to S_IFREG */
		return 0;
	case S_IFDIR:
		return -EPERM;
	default:
		return -EINVAL;
	}
}

SYSCALL_DEFINE4(mknodat, int, dfd, const char __user *, filename, umode_t, mode,
		unsigned, dev)
{
	struct dentry *dentry;
	struct path path;
	int error;
	unsigned int lookup_flags = 0;

	error = may_mknod(mode);
	if (error)
		return error;
retry:
	dentry = user_path_create(dfd, filename, &path, lookup_flags);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	if (!IS_POSIXACL(path.dentry->d_inode))
		mode &= ~current_umask();
	error = security_path_mknod(&path, dentry, mode, dev);
	if (error)
		goto out;
	switch (mode & S_IFMT) {
		case 0: case S_IFREG:
			error = vfs_create(path.dentry->d_inode,dentry,mode,true);
			break;
		case S_IFCHR: case S_IFBLK:
			error = vfs_mknod(path.dentry->d_inode,dentry,mode,
					new_decode_dev(dev));
			break;
		case S_IFIFO: case S_IFSOCK:
			error = vfs_mknod(path.dentry->d_inode,dentry,mode,0);
			break;
	}
out:
	done_path_create(&path, dentry);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	return error;
}

SYSCALL_DEFINE3(mknod, const char __user *, filename, umode_t, mode, unsigned, dev)
{
	return sys_mknodat(AT_FDCWD, filename, mode, dev);
}

int vfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
#ifdef CONFIG_SYNO_FS_WINACL
	int error = may_create(dir, dentry, S_IFDIR);
#else
	int error = may_create(dir, dentry);
#endif /* CONFIG_SYNO_FS_WINACL */
	unsigned max_links = dir->i_sb->s_max_links;

	if (error)
		return error;

	if (!dir->i_op->mkdir)
		return -EPERM;

	mode &= (S_IRWXUGO|S_ISVTX);
	error = security_inode_mkdir(dir, dentry, mode);
	if (error)
		return error;

	if (max_links && dir->i_nlink >= max_links)
		return -EMLINK;

	error = dir->i_op->mkdir(dir, dentry, mode);
	if (!error)
		fsnotify_mkdir(dir, dentry);

#ifdef CONFIG_SYNO_FS_WINACL
	if (!error && IS_SYNOACL(dentry->d_parent)) {
		//We assume that inode has been attached to dentry by d_instantiate().
		synoacl_op_init(dentry);
	}
#endif /* CONFIG_SYNO_FS_WINACL */

	return error;
}

SYSCALL_DEFINE3(mkdirat, int, dfd, const char __user *, pathname, umode_t, mode)
{
	struct dentry *dentry;
	struct path path;
	int error;
	unsigned int lookup_flags = LOOKUP_DIRECTORY;

retry:
	dentry = user_path_create(dfd, pathname, &path, lookup_flags);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	if (!IS_POSIXACL(path.dentry->d_inode))
		mode &= ~current_umask();
	error = security_path_mkdir(&path, dentry, mode);
	if (!error)
		error = vfs_mkdir(path.dentry->d_inode, dentry, mode);
	done_path_create(&path, dentry);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	return error;
}

SYSCALL_DEFINE2(mkdir, const char __user *, pathname, umode_t, mode)
{
	return sys_mkdirat(AT_FDCWD, pathname, mode);
}

/*
 * The dentry_unhash() helper will try to drop the dentry early: we
 * should have a usage count of 1 if we're the only user of this
 * dentry, and if that is true (possibly after pruning the dcache),
 * then we drop the dentry now.
 *
 * A low-level filesystem can, if it choses, legally
 * do a
 *
 *	if (!d_unhashed(dentry))
 *		return -EBUSY;
 *
 * if it cannot handle the case of removing a directory
 * that is still in use by something else..
 */
void dentry_unhash(struct dentry *dentry)
{
	shrink_dcache_parent(dentry);
	spin_lock(&dentry->d_lock);
	if (dentry->d_count == 1)
		__d_drop(dentry);
	spin_unlock(&dentry->d_lock);
}

int vfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error = may_delete(dir, dentry, 1);

	if (error)
		return error;

	if (!dir->i_op->rmdir)
		return -EPERM;

	dget(dentry);
	mutex_lock(&dentry->d_inode->i_mutex);

	error = -EBUSY;
	if (d_mountpoint(dentry))
		goto out;

	error = security_inode_rmdir(dir, dentry);
	if (error)
		goto out;

	shrink_dcache_parent(dentry);
	error = dir->i_op->rmdir(dir, dentry);
	if (error)
		goto out;

	dentry->d_inode->i_flags |= S_DEAD;
	dont_mount(dentry);

out:
	mutex_unlock(&dentry->d_inode->i_mutex);
	dput(dentry);
	if (!error)
		d_delete(dentry);
	return error;
}

static long do_rmdir(int dfd, const char __user *pathname)
{
	int error = 0;
	struct filename *name;
	struct dentry *dentry;
	struct nameidata nd;
	unsigned int lookup_flags = 0;
retry:
	name = user_path_parent(dfd, pathname, &nd, lookup_flags);
	if (IS_ERR(name))
		return PTR_ERR(name);

	switch(nd.last_type) {
	case LAST_DOTDOT:
		error = -ENOTEMPTY;
		goto exit1;
	case LAST_DOT:
		error = -EINVAL;
		goto exit1;
	case LAST_ROOT:
		error = -EBUSY;
		goto exit1;
	}

	nd.flags &= ~LOOKUP_PARENT;
	error = mnt_want_write(nd.path.mnt);
	if (error)
		goto exit1;

	mutex_lock_nested(&nd.path.dentry->d_inode->i_mutex, I_MUTEX_PARENT);
	dentry = lookup_hash(&nd);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto exit2;
	if (!dentry->d_inode) {
		error = -ENOENT;
		goto exit3;
	}
	error = security_path_rmdir(&nd.path, dentry);
	if (error)
		goto exit3;
	error = vfs_rmdir(nd.path.dentry->d_inode, dentry);
exit3:
	dput(dentry);
exit2:
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	mnt_drop_write(nd.path.mnt);
exit1:
	path_put(&nd.path);
	putname(name);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	return error;
}

SYSCALL_DEFINE1(rmdir, const char __user *, pathname)
{
	return do_rmdir(AT_FDCWD, pathname);
}

int vfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int error = may_delete(dir, dentry, 0);

	if (error)
		return error;

	if (!dir->i_op->unlink)
		return -EPERM;

	mutex_lock(&dentry->d_inode->i_mutex);
	if (d_mountpoint(dentry))
		error = -EBUSY;
	else {
		error = security_inode_unlink(dir, dentry);
		if (!error) {
			error = dir->i_op->unlink(dir, dentry);
			if (!error)
				dont_mount(dentry);
		}
	}
	mutex_unlock(&dentry->d_inode->i_mutex);

	/* We don't d_delete() NFS sillyrenamed files--they still exist. */
	if (!error && !(dentry->d_flags & DCACHE_NFSFS_RENAMED)) {
		fsnotify_link_count(dentry->d_inode);
		d_delete(dentry);
	}

	return error;
}

/*
 * Make sure that the actual truncation of the file will occur outside its
 * directory's i_mutex.  Truncate can take a long time if there is a lot of
 * writeout happening, and we don't want to prevent access to the directory
 * while waiting on the I/O.
 */
static long do_unlinkat(int dfd, const char __user *pathname)
{
	int error;
	struct filename *name;
	struct dentry *dentry;
	struct nameidata nd;
	struct inode *inode = NULL;
	unsigned int lookup_flags = 0;
retry:
	name = user_path_parent(dfd, pathname, &nd, lookup_flags);
	if (IS_ERR(name))
		return PTR_ERR(name);

	error = -EISDIR;
	if (nd.last_type != LAST_NORM)
		goto exit1;

	nd.flags &= ~LOOKUP_PARENT;
	error = mnt_want_write(nd.path.mnt);
	if (error)
		goto exit1;

	mutex_lock_nested(&nd.path.dentry->d_inode->i_mutex, I_MUTEX_PARENT);
	dentry = lookup_hash(&nd);
	error = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {
		/* Why not before? Because we want correct error value */
		if (nd.last.name[nd.last.len])
			goto slashes;
		inode = dentry->d_inode;
		if (!inode)
			goto slashes;
		ihold(inode);
		error = security_path_unlink(&nd.path, dentry);
		if (error)
			goto exit2;
		error = vfs_unlink(nd.path.dentry->d_inode, dentry);
exit2:
		dput(dentry);
	}
	mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
	if (inode)
		iput(inode);	/* truncate the inode here */
	mnt_drop_write(nd.path.mnt);
exit1:
	path_put(&nd.path);
	putname(name);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		inode = NULL;
		goto retry;
	}
	return error;

slashes:
	error = !dentry->d_inode ? -ENOENT :
		S_ISDIR(dentry->d_inode->i_mode) ? -EISDIR : -ENOTDIR;
	goto exit2;
}

SYSCALL_DEFINE3(unlinkat, int, dfd, const char __user *, pathname, int, flag)
{
	if ((flag & ~AT_REMOVEDIR) != 0)
		return -EINVAL;

	if (flag & AT_REMOVEDIR)
		return do_rmdir(dfd, pathname);

	return do_unlinkat(dfd, pathname);
}

SYSCALL_DEFINE1(unlink, const char __user *, pathname)
{
	return do_unlinkat(AT_FDCWD, pathname);
}

int vfs_symlink(struct inode *dir, struct dentry *dentry, const char *oldname)
{
#ifdef CONFIG_SYNO_FS_WINACL
	int error = may_create(dir, dentry, S_IFLNK);
#else
	int error = may_create(dir, dentry);
#endif /* CONFIG_SYNO_FS_WINACL */

	if (error)
		return error;

	if (!dir->i_op->symlink)
		return -EPERM;

	error = security_inode_symlink(dir, dentry, oldname);
	if (error)
		return error;

	error = dir->i_op->symlink(dir, dentry, oldname);
	if (!error)
		fsnotify_create(dir, dentry);
	return error;
}

SYSCALL_DEFINE3(symlinkat, const char __user *, oldname,
		int, newdfd, const char __user *, newname)
{
	int error;
	struct filename *from;
	struct dentry *dentry;
	struct path path;
	unsigned int lookup_flags = 0;

	from = getname(oldname);
	if (IS_ERR(from))
		return PTR_ERR(from);
retry:
	dentry = user_path_create(newdfd, newname, &path, lookup_flags);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto out_putname;

	error = security_path_symlink(&path, dentry, from->name);
	if (!error)
		error = vfs_symlink(path.dentry->d_inode, dentry, from->name);
	done_path_create(&path, dentry);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
out_putname:
	putname(from);
	return error;
}

SYSCALL_DEFINE2(symlink, const char __user *, oldname, const char __user *, newname)
{
	return sys_symlinkat(oldname, AT_FDCWD, newname);
}

int vfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
	struct inode *inode = old_dentry->d_inode;
	unsigned max_links = dir->i_sb->s_max_links;
	int error;

	if (!inode)
		return -ENOENT;

#ifdef CONFIG_SYNO_FS_WINACL
	error = may_create(dir, new_dentry, inode->i_mode);
#else
	error = may_create(dir, new_dentry);
#endif /* CONFIG_SYNO_FS_WINACL */
	if (error)
		return error;

	if (dir->i_sb != inode->i_sb)
		return -EXDEV;

	/*
	 * A link to an append-only or immutable file cannot be created.
	 */
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return -EPERM;
	if (!dir->i_op->link)
		return -EPERM;
	if (S_ISDIR(inode->i_mode))
		return -EPERM;

	error = security_inode_link(old_dentry, dir, new_dentry);
	if (error)
		return error;

	mutex_lock(&inode->i_mutex);
	/* Make sure we don't allow creating hardlink to an unlinked file */
	if (inode->i_nlink == 0)
		error =  -ENOENT;
	else if (max_links && inode->i_nlink >= max_links)
		error = -EMLINK;
	else
		error = dir->i_op->link(old_dentry, dir, new_dentry);
	mutex_unlock(&inode->i_mutex);
	if (!error)
		fsnotify_link(dir, inode, new_dentry);
	return error;
}

/*
 * Hardlinks are often used in delicate situations.  We avoid
 * security-related surprises by not following symlinks on the
 * newname.  --KAB
 *
 * We don't follow them on the oldname either to be compatible
 * with linux 2.0, and to avoid hard-linking to directories
 * and other special files.  --ADM
 */
SYSCALL_DEFINE5(linkat, int, olddfd, const char __user *, oldname,
		int, newdfd, const char __user *, newname, int, flags)
{
	struct dentry *new_dentry;
	struct path old_path, new_path;
	int how = 0;
	int error;

	if ((flags & ~(AT_SYMLINK_FOLLOW | AT_EMPTY_PATH)) != 0)
		return -EINVAL;
	/*
	 * To use null names we require CAP_DAC_READ_SEARCH
	 * This ensures that not everyone will be able to create
	 * handlink using the passed filedescriptor.
	 */
	if (flags & AT_EMPTY_PATH) {
		if (!capable(CAP_DAC_READ_SEARCH))
			return -ENOENT;
		how = LOOKUP_EMPTY;
	}

	if (flags & AT_SYMLINK_FOLLOW)
		how |= LOOKUP_FOLLOW;
retry:
	error = user_path_at(olddfd, oldname, how, &old_path);
	if (error)
		return error;

	new_dentry = user_path_create(newdfd, newname, &new_path,
					(how & LOOKUP_REVAL));
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto out;

	error = -EXDEV;
	if (old_path.mnt != new_path.mnt)
		goto out_dput;
	error = may_linkat(&old_path);
	if (unlikely(error))
		goto out_dput;
	error = security_path_link(old_path.dentry, &new_path, new_dentry);
	if (error)
		goto out_dput;
	error = vfs_link(old_path.dentry, new_path.dentry->d_inode, new_dentry);
out_dput:
	done_path_create(&new_path, new_dentry);
	if (retry_estale(error, how)) {
		path_put(&old_path);
		how |= LOOKUP_REVAL;
		goto retry;
	}
out:
	path_put(&old_path);

	return error;
}

SYSCALL_DEFINE2(link, const char __user *, oldname, const char __user *, newname)
{
	return sys_linkat(AT_FDCWD, oldname, AT_FDCWD, newname, 0);
}

/*
 * The worst of all namespace operations - renaming directory. "Perverted"
 * doesn't even start to describe it. Somebody in UCB had a heck of a trip...
 * Problems:
 *	a) we can get into loop creation. Check is done in is_subdir().
 *	b) race potential - two innocent renames can create a loop together.
 *	   That's where 4.4 screws up. Current fix: serialization on
 *	   sb->s_vfs_rename_mutex. We might be more accurate, but that's another
 *	   story.
 *	c) we have to lock _three_ objects - parents and victim (if it exists).
 *	   And that - after we got ->i_mutex on parents (until then we don't know
 *	   whether the target exists).  Solution: try to be smart with locking
 *	   order for inodes.  We rely on the fact that tree topology may change
 *	   only under ->s_vfs_rename_mutex _and_ that parent of the object we
 *	   move will be locked.  Thus we can rank directories by the tree
 *	   (ancestors first) and rank all non-directories after them.
 *	   That works since everybody except rename does "lock parent, lookup,
 *	   lock child" and rename is under ->s_vfs_rename_mutex.
 *	   HOWEVER, it relies on the assumption that any object with ->lookup()
 *	   has no more than 1 dentry.  If "hybrid" objects will ever appear,
 *	   we'd better make sure that there's no link(2) for them.
 *	d) conversion from fhandle to dentry may come in the wrong moment - when
 *	   we are removing the target. Solution: we will have to grab ->i_mutex
 *	   in the fhandle_to_dentry code. [FIXME - current nfsfh.c relies on
 *	   ->i_mutex on parents, which works but leads to some truly excessive
 *	   locking].
 */
static int vfs_rename_dir(struct inode *old_dir, struct dentry *old_dentry,
			  struct inode *new_dir, struct dentry *new_dentry)
{
	int error = 0;
	struct inode *target = new_dentry->d_inode;
	unsigned max_links = new_dir->i_sb->s_max_links;

	/*
	 * If we are going to change the parent - check write permissions,
	 * we'll need to flip '..'.
	 */
#ifdef CONFIG_SYNO_BTRFS_RENAME_READONLY_SUBVOL
	if (new_dir != old_dir && !(old_dentry->d_sb->s_magic == BTRFS_SUPER_MAGIC && old_dentry->d_inode->i_ino == 256)) {
#else
	if (new_dir != old_dir) {
#endif
#ifdef CONFIG_SYNO_FS_WINACL
		if (!IS_SYNOACL(old_dentry)) {
			error = inode_permission(old_dentry->d_inode, MAY_WRITE);
		}
#else
		error = inode_permission(old_dentry->d_inode, MAY_WRITE);
#endif /* CONFIG_SYNO_FS_WINACL */
		if (error)
			return error;
	}

	error = security_inode_rename(old_dir, old_dentry, new_dir, new_dentry);
	if (error)
		return error;

	dget(new_dentry);
	if (target)
		mutex_lock(&target->i_mutex);

	error = -EBUSY;
	if (d_mountpoint(old_dentry) || d_mountpoint(new_dentry))
		goto out;

	error = -EMLINK;
	if (max_links && !target && new_dir != old_dir &&
	    new_dir->i_nlink >= max_links)
		goto out;

	if (target)
		shrink_dcache_parent(new_dentry);
	error = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
	if (error)
		goto out;

	if (target) {
		target->i_flags |= S_DEAD;
		dont_mount(new_dentry);
	}
out:
	if (target)
		mutex_unlock(&target->i_mutex);
	dput(new_dentry);
	if (!error)
		if (!(old_dir->i_sb->s_type->fs_flags & FS_RENAME_DOES_D_MOVE))
			d_move(old_dentry,new_dentry);
	return error;
}

static int vfs_rename_other(struct inode *old_dir, struct dentry *old_dentry,
			    struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *target = new_dentry->d_inode;
	int error;

	error = security_inode_rename(old_dir, old_dentry, new_dir, new_dentry);
	if (error)
		return error;

	dget(new_dentry);
	if (target)
		mutex_lock(&target->i_mutex);

	error = -EBUSY;
	if (d_mountpoint(old_dentry)||d_mountpoint(new_dentry))
		goto out;

	error = old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
	if (error)
		goto out;

	if (target)
		dont_mount(new_dentry);
	if (!(old_dir->i_sb->s_type->fs_flags & FS_RENAME_DOES_D_MOVE))
		d_move(old_dentry, new_dentry);
out:
	if (target)
		mutex_unlock(&target->i_mutex);
	dput(new_dentry);
	return error;
}

#ifdef CONFIG_SYNO_FS_NOTIFY
inline void free_rename_path_list(struct synotify_rename_path * rename_path_list)
{
	while(rename_path_list) {
		struct synotify_rename_path *tmp = rename_path_list;
		rename_path_list = rename_path_list->next;

		if (tmp->old_full_path)
			kfree(tmp->old_full_path);
		if (tmp->new_full_path)
			kfree(tmp->new_full_path);

		mntput(tmp->vfs_mnt);
		kfree(tmp);
	}
}

static inline struct synotify_rename_path * get_rename_path(struct vfsmount *vfsmnt, struct dentry *old_dentry, struct dentry *new_dentry)
{
	int ret = -1;
	struct synotify_rename_path * result = NULL;
	struct synotify_rename_path * rename_path = NULL;

	struct path old_path;
	struct path new_path;
	struct path root_path;
	char *old_path_buf = NULL;
	char *new_path_buf = NULL;
	char *tmp_old_full_path = NULL;
	char *tmp_new_full_path = NULL;
	char *tmp_old_path = NULL;
	char *tmp_new_path = NULL;

	if (!vfsmnt || !old_dentry || !new_dentry) {
		return NULL;
	}

	rename_path = kmalloc(sizeof(struct synotify_rename_path), GFP_NOFS);
	if (!rename_path) {
		goto end;
	}

	memset(&old_path, 0, sizeof(struct path));
	memset(&new_path, 0, sizeof(struct path));
	memset(&root_path, 0, sizeof(struct path));

	old_path.mnt = vfsmnt;
	old_path.dentry = old_dentry;
	new_path.mnt = vfsmnt;
	new_path.dentry = new_dentry;

	root_path.mnt = vfsmnt;
	root_path.dentry = vfsmnt->mnt_root;

	old_path_buf = kmalloc(PATH_MAX, GFP_NOFS);
	if (!old_path_buf) {
		goto end;
	}

	new_path_buf = kmalloc(PATH_MAX, GFP_NOFS);
	if (!new_path_buf) {
		goto end;
	}

	// set synotify_rename_path
	tmp_old_full_path = __d_path(&old_path, &root_path, old_path_buf, PATH_MAX-1);
	tmp_new_full_path = __d_path(&new_path, &root_path, new_path_buf, PATH_MAX-1);

	if (IS_ERR_OR_NULL(tmp_old_full_path) || IS_ERR_OR_NULL(tmp_new_full_path)) {
		goto end;
	}

	// get required path, update to rename_path
	tmp_old_path = kstrdup(tmp_old_full_path, GFP_NOFS);
	if (!tmp_old_path) {
		goto end;
	}
	tmp_new_path = kstrdup(tmp_new_full_path, GFP_NOFS);
	if (!tmp_new_path) {
		goto end;
	}
	rename_path->old_full_path = tmp_old_path;
	rename_path->new_full_path = tmp_new_path;
	rename_path->vfs_mnt = vfsmnt;
	rename_path->next = NULL;

	result = rename_path;
	ret = 0;
end:
	if (ret != 0) {
		if (tmp_old_path)
			kfree(tmp_old_path);
		if (tmp_new_path)
			kfree(tmp_new_path);
		if (rename_path)
			kfree(rename_path);
	}

	if (old_path_buf)
		kfree(old_path_buf);
	if (new_path_buf)
		kfree(new_path_buf);

	return result;
}

inline struct synotify_rename_path * get_rename_path_list(struct dentry *old_dentry, struct dentry *new_dentry, __u32 old_dir_mask, __u32 new_dir_mask)
{
	struct nsproxy *nsproxy = current->nsproxy;
	struct mnt_namespace *mnt_space = NULL;
	struct list_head *list_head = NULL;
	struct synotify_rename_path *head = NULL;
	struct synotify_rename_path *tail = NULL;

	if (!nsproxy) {
		return NULL;
	}

	mnt_space = nsproxy->mnt_ns;
	if (!mnt_space) {
		return NULL;
	}

	list_for_each(list_head, &mnt_space->list) {
		struct mount *mnt = list_entry(list_head, struct mount, mnt_list);
		struct synotify_rename_path *rename_path = NULL;
		struct vfsmount *vfsmnt = NULL;
		__u32 test_old_mask = (old_dir_mask & ~FS_EVENT_ON_CHILD);
		__u32 test_new_mask = (new_dir_mask & ~FS_EVENT_ON_CHILD);

		if (!mnt) {
			continue;
		}
		if (mnt->mnt.mnt_sb != new_dentry->d_sb) {
			continue;
		}

		if (!(test_old_mask & mnt->mnt_fsnotify_mask)) {
			continue;
		}
		if (!(test_new_mask & mnt->mnt_fsnotify_mask)) {
			continue;
		}

		vfsmnt = &mnt->mnt;
		// NOTE: we will hold vfsmnt till calling free_rename_path_list when getting rename path successfully
		mntget(vfsmnt);

		rename_path = get_rename_path(vfsmnt, old_dentry, new_dentry);

		if (!rename_path) {
			mntput(vfsmnt);
		} else {
			if (!head) {
				head = rename_path;
				tail = rename_path;
			} else {
				tail->next = rename_path;
				tail = rename_path;
			}
		}
	} // list_for_each mount

	return head;
}

#endif

int vfs_rename(struct inode *old_dir, struct dentry *old_dentry,
	       struct inode *new_dir, struct dentry *new_dentry)
{
	int error;
	int is_dir = S_ISDIR(old_dentry->d_inode->i_mode);
	const unsigned char *old_name;
#ifdef CONFIG_SYNO_FS_NOTIFY
	struct synotify_rename_path *rename_path_list = NULL;
	__u32 old_dir_mask = (FS_EVENT_ON_CHILD | FS_MOVED_FROM);
	__u32 new_dir_mask = (FS_EVENT_ON_CHILD | FS_MOVED_TO);
#endif

	if (old_dentry->d_inode == new_dentry->d_inode)
 		return 0;
 
	error = may_delete(old_dir, old_dentry, is_dir);
	if (error)
		return error;

	if (!new_dentry->d_inode)
#ifdef CONFIG_SYNO_FS_WINACL
	{
		error = may_create(new_dir, new_dentry, old_dentry->d_inode->i_mode);
	}
#else
		error = may_create(new_dir, new_dentry);
#endif /* CONFIG_SYNO_FS_WINACL */
	else
		error = may_delete(new_dir, new_dentry, is_dir);
	if (error)
		return error;

	if (!old_dir->i_op->rename)
		return -EPERM;

#ifdef CONFIG_SYNO_FS_NOTIFY
	if (old_dir == new_dir)
		old_dir_mask |= FS_DN_RENAME;

	if (is_dir) {
		old_dir_mask |= FS_ISDIR;
		new_dir_mask |= FS_ISDIR;
	}
	rename_path_list = get_rename_path_list(old_dentry, new_dentry, old_dir_mask, new_dir_mask);
#endif

	old_name = fsnotify_oldname_init(old_dentry->d_name.name);

	if (is_dir)
		error = vfs_rename_dir(old_dir,old_dentry,new_dir,new_dentry);
	else
		error = vfs_rename_other(old_dir,old_dentry,new_dir,new_dentry);

	if (!error)
#ifdef CONFIG_SYNO_FS_NOTIFY
		fsnotify_move(old_dir, new_dir, old_name, is_dir,
			      new_dentry->d_inode, old_dentry, rename_path_list);
#else
		fsnotify_move(old_dir, new_dir, old_name, is_dir,
			      new_dentry->d_inode, old_dentry);
#endif
	fsnotify_oldname_free(old_name);

#ifdef CONFIG_SYNO_FS_NOTIFY
	free_rename_path_list(rename_path_list);
#endif
	return error;
}

SYSCALL_DEFINE4(renameat, int, olddfd, const char __user *, oldname,
		int, newdfd, const char __user *, newname)
{
	struct dentry *old_dir, *new_dir;
	struct dentry *old_dentry, *new_dentry;
	struct dentry *trap;
	struct nameidata oldnd, newnd;
	struct filename *from;
	struct filename *to;
	unsigned int lookup_flags = 0;
	bool should_retry = false;
	int error;
retry:
	from = user_path_parent(olddfd, oldname, &oldnd, lookup_flags);
	if (IS_ERR(from)) {
		error = PTR_ERR(from);
		goto exit;
	}

	to = user_path_parent(newdfd, newname, &newnd, lookup_flags);
	if (IS_ERR(to)) {
		error = PTR_ERR(to);
		goto exit1;
	}

	error = -EXDEV;
	if (oldnd.path.mnt != newnd.path.mnt)
		goto exit2;

	old_dir = oldnd.path.dentry;
	error = -EBUSY;
	if (oldnd.last_type != LAST_NORM)
		goto exit2;

	new_dir = newnd.path.dentry;
	if (newnd.last_type != LAST_NORM)
		goto exit2;

	error = mnt_want_write(oldnd.path.mnt);
	if (error)
		goto exit2;

	oldnd.flags &= ~LOOKUP_PARENT;
	newnd.flags &= ~LOOKUP_PARENT;
	newnd.flags |= LOOKUP_RENAME_TARGET;

	trap = lock_rename(new_dir, old_dir);

	old_dentry = lookup_hash(&oldnd);
	error = PTR_ERR(old_dentry);
	if (IS_ERR(old_dentry))
		goto exit3;
	/* source must exist */
	error = -ENOENT;
	if (!old_dentry->d_inode)
		goto exit4;
	/* unless the source is a directory trailing slashes give -ENOTDIR */
	if (!S_ISDIR(old_dentry->d_inode->i_mode)) {
		error = -ENOTDIR;
		if (oldnd.last.name[oldnd.last.len])
			goto exit4;
		if (newnd.last.name[newnd.last.len])
			goto exit4;
	}
	/* source should not be ancestor of target */
	error = -EINVAL;
	if (old_dentry == trap)
		goto exit4;
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	if (new_dir->d_inode == old_dentry->d_inode) {
		/* only possible to happen in caseless filesystem */
		/* reject move myself into subdir of myself */
		error = -ENOENT;
		goto exit4;
	}
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	new_dentry = lookup_hash(&newnd);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto exit4;
	/* target should not be an ancestor of source */
	error = -ENOTEMPTY;
	if (new_dentry == trap)
		goto exit5;

	error = security_path_rename(&oldnd.path, old_dentry,
				     &newnd.path, new_dentry);
	if (error)
		goto exit5;
	error = vfs_rename(old_dir->d_inode, old_dentry,
				   new_dir->d_inode, new_dentry);
exit5:
	dput(new_dentry);
exit4:
	dput(old_dentry);
exit3:
	unlock_rename(new_dir, old_dir);
	mnt_drop_write(oldnd.path.mnt);
exit2:
	if (retry_estale(error, lookup_flags))
		should_retry = true;
	path_put(&newnd.path);
	putname(to);
exit1:
	path_put(&oldnd.path);
	putname(from);
	if (should_retry) {
		should_retry = false;
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
exit:
	return error;
}

SYSCALL_DEFINE2(rename, const char __user *, oldname, const char __user *, newname)
{
	return sys_renameat(AT_FDCWD, oldname, AT_FDCWD, newname);
}

int vfs_readlink(struct dentry *dentry, char __user *buffer, int buflen, const char *link)
{
	int len;

	len = PTR_ERR(link);
	if (IS_ERR(link))
		goto out;

	len = strlen(link);
	if (len > (unsigned) buflen)
		len = buflen;
	if (copy_to_user(buffer, link, len))
		len = -EFAULT;
out:
	return len;
}

/*
 * A helper for ->readlink().  This should be used *ONLY* for symlinks that
 * have ->follow_link() touching nd only in nd_set_link().  Using (or not
 * using) it for any given inode is up to filesystem.
 */
int generic_readlink(struct dentry *dentry, char __user *buffer, int buflen)
{
	struct nameidata nd;
	void *cookie;
	int res;

	nd.depth = 0;
	cookie = dentry->d_inode->i_op->follow_link(dentry, &nd);
	if (IS_ERR(cookie))
		return PTR_ERR(cookie);

	res = vfs_readlink(dentry, buffer, buflen, nd_get_link(&nd));
	if (dentry->d_inode->i_op->put_link)
		dentry->d_inode->i_op->put_link(dentry, &nd, cookie);
	return res;
}

int vfs_follow_link(struct nameidata *nd, const char *link)
{
	return __vfs_follow_link(nd, link);
}

/* get the link contents into pagecache */
static char *page_getlink(struct dentry * dentry, struct page **ppage)
{
	char *kaddr;
	struct page *page;
	struct address_space *mapping = dentry->d_inode->i_mapping;
	page = read_mapping_page(mapping, 0, NULL);
	if (IS_ERR(page))
		return (char*)page;
	*ppage = page;
	kaddr = kmap(page);
	nd_terminate_link(kaddr, dentry->d_inode->i_size, PAGE_SIZE - 1);
	return kaddr;
}

int page_readlink(struct dentry *dentry, char __user *buffer, int buflen)
{
	struct page *page = NULL;
	char *s = page_getlink(dentry, &page);
	int res = vfs_readlink(dentry,buffer,buflen,s);
	if (page) {
		kunmap(page);
		page_cache_release(page);
	}
	return res;
}

void *page_follow_link_light(struct dentry *dentry, struct nameidata *nd)
{
	struct page *page = NULL;
	nd_set_link(nd, page_getlink(dentry, &page));
	return page;
}

void page_put_link(struct dentry *dentry, struct nameidata *nd, void *cookie)
{
	struct page *page = cookie;

	if (page) {
		kunmap(page);
		page_cache_release(page);
	}
}

/*
 * The nofs argument instructs pagecache_write_begin to pass AOP_FLAG_NOFS
 */
int __page_symlink(struct inode *inode, const char *symname, int len, int nofs)
{
	struct address_space *mapping = inode->i_mapping;
	struct page *page;
	void *fsdata;
	int err;
	char *kaddr;
	unsigned int flags = AOP_FLAG_UNINTERRUPTIBLE;
	if (nofs)
		flags |= AOP_FLAG_NOFS;

retry:
	err = pagecache_write_begin(NULL, mapping, 0, len-1,
				flags, &page, &fsdata);
	if (err)
		goto fail;

	kaddr = kmap_atomic(page);
	memcpy(kaddr, symname, len-1);
	kunmap_atomic(kaddr);

	err = pagecache_write_end(NULL, mapping, 0, len-1, len-1,
							page, fsdata);
	if (err < 0)
		goto fail;
	if (err < len-1)
		goto retry;

	mark_inode_dirty(inode);
	return 0;
fail:
	return err;
}

int page_symlink(struct inode *inode, const char *symname, int len)
{
	return __page_symlink(inode, symname, len,
			!(mapping_gfp_mask(inode->i_mapping) & __GFP_FS));
}

const struct inode_operations page_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
};

EXPORT_SYMBOL(user_path_at);
EXPORT_SYMBOL(follow_down_one);
EXPORT_SYMBOL(follow_down);
EXPORT_SYMBOL(follow_up);
#ifdef CONFIG_SYNO_FS_NOTIFY
EXPORT_SYMBOL(syno_fetch_mountpoint_fullpath);
#endif
EXPORT_SYMBOL(get_write_access); /* nfsd */
EXPORT_SYMBOL(lock_rename);
EXPORT_SYMBOL(lookup_one_len);
#if defined(CONFIG_SYNO_FS_EXPORT_SYMBOL_LOOKUP_HASH)
EXPORT_SYMBOL(lookup_hash);
#endif
EXPORT_SYMBOL(page_follow_link_light);
EXPORT_SYMBOL(page_put_link);
EXPORT_SYMBOL(page_readlink);
EXPORT_SYMBOL(__page_symlink);
EXPORT_SYMBOL(page_symlink);
EXPORT_SYMBOL(page_symlink_inode_operations);
EXPORT_SYMBOL(kern_path);
EXPORT_SYMBOL(vfs_path_lookup);
EXPORT_SYMBOL(inode_permission);
EXPORT_SYMBOL(unlock_rename);
EXPORT_SYMBOL(vfs_create);
EXPORT_SYMBOL(vfs_follow_link);
EXPORT_SYMBOL(vfs_link);
EXPORT_SYMBOL(vfs_mkdir);
EXPORT_SYMBOL(vfs_mknod);
EXPORT_SYMBOL(generic_permission);
EXPORT_SYMBOL(vfs_readlink);
EXPORT_SYMBOL(vfs_rename);
EXPORT_SYMBOL(vfs_rmdir);
EXPORT_SYMBOL(vfs_symlink);
EXPORT_SYMBOL(vfs_unlink);
EXPORT_SYMBOL(dentry_unhash);
EXPORT_SYMBOL(generic_readlink);
