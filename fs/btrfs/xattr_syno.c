/*
 * linux/fs/btrfs/xattr_syno.c
 *
 * Copyright (C) 2001-2014 Synology Inc.
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "xattr.h"

static int btrfs_xattr_syno_set(struct dentry *dentry, const char *name,
		const void *value, size_t size, int flags, int type)
{
	int ret;
	char *complete_name = NULL;
	complete_name = kmalloc(sizeof(XATTR_SYNO_PREFIX) + strlen(name), GFP_NOFS);
	if (!complete_name) {
		ret = -ENOMEM;
		goto out;
	}
	snprintf(complete_name, sizeof(XATTR_SYNO_PREFIX) + strlen(name), XATTR_SYNO_PREFIX"%s", name);
	ret = __btrfs_setxattr(NULL, dentry->d_inode, complete_name, value, size, flags);
	if (!ret) {
#ifdef CONFIG_SYNO_BTRFS_ARCHIVE_BIT
		if (!strcmp(name, XATTR_SYNO_ARCHIVE_BIT)) {
			const __le32 *archive_bit_le32 = value;
			dentry->d_inode->i_archive_bit = le32_to_cpu(*archive_bit_le32);
		}
#endif /* CONFIG_SYNO_BTRFS_ARCHIVE_BIT */
	}
out:
	kfree(complete_name);
	return ret;
}

const struct xattr_handler btrfs_xattr_syno_handler = {
	.prefix	= XATTR_SYNO_PREFIX,
	.set	= btrfs_xattr_syno_set,
};
