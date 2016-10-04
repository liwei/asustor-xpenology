/*
 * linux/fs/ext4/syno_acl.c
 *
 * Copyright (C) 2001-2013 Synology Inc.
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include "ext4_jbd2.h"
#include "ext4.h"
#include "xattr.h"
#include "syno_acl.h"

static inline int
ext4_ace_map_from_xattr_ace(struct syno_acl_entry *pAce, ext4_syno_acl_entry *pEntry)
{
	unsigned short tag = le16_to_cpu(pEntry->e_tag);

	//ID: user/group/everyone
	if (SYNO_ACL_XATTR_TAG_ID_GROUP & tag) {
		pAce->e_tag = SYNO_ACL_GROUP;
		pAce->e_id = le32_to_cpu(pEntry->e_id);
	} else if (SYNO_ACL_XATTR_TAG_ID_EVERYONE & tag) {
		pAce->e_tag = SYNO_ACL_EVERYONE;
		pAce->e_id = SYNO_ACL_UNDEFINED_ID;
	} else if (SYNO_ACL_XATTR_TAG_ID_USER & tag) {
		pAce->e_tag = SYNO_ACL_USER;
		pAce->e_id = le32_to_cpu(pEntry->e_id);
	} else if (SYNO_ACL_XATTR_TAG_ID_OWNER & tag) {
		pAce->e_tag = SYNO_ACL_OWNER;
		pAce->e_id = SYNO_ACL_UNDEFINED_ID;
	} else {
		return -1;
	}

	//Allow/Deny
	if (SYNO_ACL_XATTR_TAG_IS_DENY & tag) {
		pAce->e_allow = SYNO_ACL_DENY;
	} else if (SYNO_ACL_XATTR_TAG_IS_ALLOW & tag){
		pAce->e_allow = SYNO_ACL_ALLOW;
	} else {
		return -1;
	}

	//Permission
	pAce->e_perm = le32_to_cpu(pEntry->e_perm);

	//Inherit
	pAce->e_inherit = le16_to_cpu(pEntry->e_inherit);

	//Inherit level
	pAce->e_level = 0;

	return 0;
}

static inline int
ext4_syno_ace_map_to_xattr_ace(const struct syno_acl_entry *pAce, ext4_syno_acl_entry *pEntry)
{
	int ret = 0;
	unsigned short tag = 0;

	//ID: user/group/everyone
	switch(pAce->e_tag){
	case SYNO_ACL_GROUP:
		tag |= SYNO_ACL_XATTR_TAG_ID_GROUP;
		break;
	case SYNO_ACL_EVERYONE:
		tag |= SYNO_ACL_XATTR_TAG_ID_EVERYONE;
		break;
	case SYNO_ACL_USER:
		tag |= SYNO_ACL_XATTR_TAG_ID_USER;
		break;
	case SYNO_ACL_OWNER:
		tag |= SYNO_ACL_XATTR_TAG_ID_OWNER;
		break;
	default:
		ret = -EIO;
		goto Err;
	}

	//Allow/Deny
	switch(pAce->e_allow){
	case SYNO_ACL_DENY:
		tag |= SYNO_ACL_XATTR_TAG_IS_DENY;
		break;
	case SYNO_ACL_ALLOW:
		tag |= SYNO_ACL_XATTR_TAG_IS_ALLOW;
		break;
	default:
		ret = -EIO;
		goto Err;
	}

	pEntry->e_tag = cpu_to_le16(tag);
	pEntry->e_inherit  = cpu_to_le16(pAce->e_inherit);
	pEntry->e_perm = cpu_to_le32(pAce->e_perm);
	pEntry->e_id   = cpu_to_le32(pAce->e_id);

Err:
	return ret;
}

/*
 * Convert from filesystem to in-memory representation.
 */
static struct syno_acl *
ext4_syno_acl_from_disk(const void *value, size_t size)
{
	const char *end = (char *)value + size;
	int i, count;
	struct syno_acl *acl;

	if (!value)
		return NULL;
	if (size < sizeof(ext4_syno_acl_header))
		 return ERR_PTR(-EINVAL);
	if (((ext4_syno_acl_header *)value)->a_version !=
	    cpu_to_le16(EXT4_SYNO_ACL_VERSION))
		return ERR_PTR(-EINVAL);
	value = (char *)value + sizeof(ext4_syno_acl_header);
	count = ext4_syno_acl_count(size);
	if (count < 0)
		return ERR_PTR(-EINVAL);
	if (count == 0)
		return NULL;
	acl = syno_acl_alloc(count, GFP_NOFS);
	if (!acl)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++) {
		ext4_syno_acl_entry *entry = (ext4_syno_acl_entry *)value;

		if ((char *)value + sizeof(ext4_syno_acl_entry) > end)
			goto fail;

		ext4_ace_map_from_xattr_ace(&(acl->a_entries[i]), entry);

		value = (char *)value + sizeof(ext4_syno_acl_entry);
	}

	if (value != end)
		goto fail;
	return acl;

fail:
	syno_acl_release(acl);
	return ERR_PTR(-EINVAL);
}

/*
 * Convert from in-memory to filesystem representation.
 */
static void *
ext4_syno_acl_to_disk(const struct syno_acl *acl, size_t *size)
{
	ext4_syno_acl_header *ext_acl;
	char *e;
	int i;

	*size = ext4_syno_acl_size(acl->a_count);
	ext_acl = kmalloc(sizeof(ext4_syno_acl_header) + acl->a_count *
			sizeof(ext4_syno_acl_entry), GFP_NOFS);
	if (!ext_acl)
		return ERR_PTR(-ENOMEM);
	ext_acl->a_version = cpu_to_le16(EXT4_SYNO_ACL_VERSION);
	e = (char *)ext_acl + sizeof(ext4_syno_acl_header);
	for (i = 0; i < acl->a_count; i++, e += sizeof(ext4_syno_acl_entry)) {
		ext4_syno_acl_entry *entry = (ext4_syno_acl_entry *)e;

		if (0 > ext4_syno_ace_map_to_xattr_ace(&(acl->a_entries[i]), entry)) {
			goto fail;
		}
	}
	return (char *)ext_acl;

fail:
	kfree(ext_acl);
	return ERR_PTR(-EINVAL);
}

/*
 * Inode operation syno_acl_get().
 *
 * inode->i_mutex: don't care
 */
struct syno_acl *
ext4_get_syno_acl(struct inode *inode)
{
	char *value = NULL;
	struct syno_acl *acl;
	int retval;

	acl = get_cached_syno_acl(inode);
	if (acl != ACL_NOT_CACHED)
		return acl;

	retval = ext4_xattr_get(inode, EXT4_XATTR_INDEX_SYNO_ACL_ACCESS, "", NULL, 0);
	if (retval > 0) {
		value = kmalloc(retval, GFP_NOFS);
		if (!value)
			return ERR_PTR(-ENOMEM);
		retval = ext4_xattr_get(inode, EXT4_XATTR_INDEX_SYNO_ACL_ACCESS, "", value, retval);
	}
	if (retval > 0)
		acl = ext4_syno_acl_from_disk(value, retval);
	else if (retval == -ENODATA || retval == -ENOSYS)
		acl = NULL;
	else
		acl = ERR_PTR(retval);
	kfree(value);

	if (!IS_ERR(acl))
		set_cached_syno_acl(inode, acl);

	return acl;
}

/*
 * Set the syno acl of an inode.
 *
 * inode->i_mutex: down unless called from ext4_new_inode
 */
static int
__ext4_set_syno_acl(handle_t *handle, struct inode *inode, struct syno_acl *acl)
{
	void *value = NULL;
	size_t size = 0;
	int error;

	if (S_ISLNK(inode->i_mode))
		return -EOPNOTSUPP;

	if (acl) {
		value = ext4_syno_acl_to_disk(acl, &size);
		if (IS_ERR(value))
			return (int)PTR_ERR(value);
	}

	error = ext4_xattr_set_handle(handle, inode, EXT4_XATTR_INDEX_SYNO_ACL_ACCESS, "",
				      value, size, 0);

	kfree(value);
	if (!error)
		set_cached_syno_acl(inode, acl);

	return error;
}


/*
 * Inode operation syno_acl_set().
 */
int ext4_set_syno_acl(struct inode *inode, struct syno_acl *acl)
{
	handle_t *handle;
	int error, retries = 0;

	if (!inode || !inode->i_sb || !acl) {
		return -EINVAL;
	}
retry:
	handle = ext4_journal_start(inode, EXT4_HT_SYNO, EXT4_DATA_TRANS_BLOCKS(inode->i_sb));
	if (IS_ERR(handle)){
		return PTR_ERR(handle);
	}

	error = __ext4_set_syno_acl(handle, inode, acl);

	ext4_journal_stop(handle);
	if (error == -ENOSPC && ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry;

	return error;
}

/*
 * Extended attribute handlers
 */
static size_t
ext4_xattr_list_syno_acl_access(struct dentry *dentry, char *list, size_t list_len,
			   const char *name, size_t name_len, int type)
{
	const size_t size = sizeof(SYNO_ACL_XATTR_ACCESS);

	if (!IS_EXT4_SYNOACL(dentry->d_inode))
		return 0;
	if (list && size <= list_len)
		memcpy(list, SYNO_ACL_XATTR_ACCESS, size);
	return size;
}

static int
ext4_xattr_get_syno_acl(struct dentry *dentry, const char *name, void *buffer,
		   size_t size, int type)
{
	struct syno_acl *acl;
	int error;

	if (strcmp(name, "") != 0)
		return -EINVAL;

	acl = ext4_get_syno_acl(dentry->d_inode);
	if (IS_ERR(acl))
		return PTR_ERR(acl);
	if (NULL == acl)
		return -ENODATA;
	error = syno_acl_to_xattr(acl, buffer, size);
	syno_acl_release(acl);

	return error;
}

static int
ext4_xattr_set_syno_acl(struct dentry *dentry, const char *name, const void *value,
		   size_t size, int flags, int type)
{
	struct inode *inode = dentry->d_inode;
	handle_t *handle;
	struct syno_acl *acl;
	int error, retries = 0;

	if (strcmp(name, "") != 0)
		return -EINVAL;

	if (value) {
		acl = syno_acl_from_xattr(value, size);
		if (IS_ERR(acl))
			return PTR_ERR(acl);
		else if (acl) {
			error = syno_acl_valid(acl);
			if (error)
				goto release_and_out;
		}
	} else
		acl = NULL;

retry:
	handle = ext4_journal_start(inode, EXT4_HT_SYNO, EXT4_DATA_TRANS_BLOCKS(inode->i_sb));
	if (IS_ERR(handle)) {
		error = PTR_ERR(handle);
		goto release_and_out;
	}
	error = __ext4_set_syno_acl(handle, inode, acl);
	ext4_journal_stop(handle);
	if (error == -ENOSPC && ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry;

release_and_out:
	syno_acl_release(acl);
	return error;
}

const struct xattr_handler ext4_xattr_synoacl_access_handler = {
	.prefix	= SYNO_ACL_XATTR_ACCESS,
	.list	= ext4_xattr_list_syno_acl_access,
	.get	= ext4_xattr_get_syno_acl,
	.set	= ext4_xattr_set_syno_acl,
};

const struct xattr_handler ext4_xattr_synoacl_noperm_access_handler = {
	.prefix	= SYNO_ACL_XATTR_ACCESS_NOPERM,
	.list	= ext4_xattr_list_syno_acl_access,
	.get	= ext4_xattr_get_syno_acl,
	.set	= ext4_xattr_set_syno_acl,
};
