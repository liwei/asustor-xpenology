#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * linux/fs/hfsplus/xattr.c
 *
 * Vyacheslav Dubeyko <slava@dubeyko.com>
 *
 * Logic of processing extended attributes
 */

#include "hfsplus_fs.h"
#include "xattr.h"

#ifdef CONFIG_SYNO_HFSPLUS_EA
/* Zero out the date added field for the specified cnode */
static void hfsplus_zero_dateadded(u16 entry_type, u8 *finderinfo) {

	/* Advance finfo by 16 bytes to the 2nd half of the finderinfo */
    finderinfo = finderinfo + 16;

	if (entry_type == HFSPLUS_FOLDER) {
		struct DXInfo *fldinfo = (struct DXInfo *)finderinfo;
		fldinfo->date_added = 0;
	} else if (entry_type == HFSPLUS_FILE) {
		struct FXInfo *fileinfo = (struct FXInfo *)finderinfo;
		fileinfo->date_added = 0;
    }
}
#endif

#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
extern struct mutex syno_hfsplus_global_mutex;
#endif

const struct xattr_handler *hfsplus_xattr_handlers[] = {
	&hfsplus_xattr_osx_handler,
	&hfsplus_xattr_user_handler,
	&hfsplus_xattr_trusted_handler,
	&hfsplus_xattr_security_handler,
	NULL
};

static int strcmp_xattr_finder_info(const char *name)
{
	if (name) {
		return strncmp(name, HFSPLUS_XATTR_FINDER_INFO_NAME,
				sizeof(HFSPLUS_XATTR_FINDER_INFO_NAME));
	}
	return -1;
}

static int strcmp_xattr_acl(const char *name)
{
	if (name) {
		return strncmp(name, HFSPLUS_XATTR_ACL_NAME,
				sizeof(HFSPLUS_XATTR_ACL_NAME));
	}
	return -1;
}

static inline int is_known_namespace(const char *name)
{
	if (strncmp(name, XATTR_SYSTEM_PREFIX, XATTR_SYSTEM_PREFIX_LEN) &&
	    strncmp(name, XATTR_USER_PREFIX, XATTR_USER_PREFIX_LEN) &&
	    strncmp(name, XATTR_SECURITY_PREFIX, XATTR_SECURITY_PREFIX_LEN) &&
	    strncmp(name, XATTR_TRUSTED_PREFIX, XATTR_TRUSTED_PREFIX_LEN))
		return false;

	return true;
}

static int can_set_xattr(struct inode *inode, const char *name,
				const void *value, size_t value_len)
{
	if (!strncmp(name, XATTR_SYSTEM_PREFIX, XATTR_SYSTEM_PREFIX_LEN))
		return -EOPNOTSUPP; /* TODO: implement ACL support */

	if (!strncmp(name, XATTR_MAC_OSX_PREFIX, XATTR_MAC_OSX_PREFIX_LEN)) {
		/*
		 * This makes sure that we aren't trying to set an
		 * attribute in a different namespace by prefixing it
		 * with "osx."
		 */
		if (is_known_namespace(name + XATTR_MAC_OSX_PREFIX_LEN))
			return -EOPNOTSUPP;

		return 0;
	}

	/*
	 * Don't allow setting an attribute in an unknown namespace.
	 */
	if (strncmp(name, XATTR_TRUSTED_PREFIX, XATTR_TRUSTED_PREFIX_LEN) &&
	    strncmp(name, XATTR_SECURITY_PREFIX, XATTR_SECURITY_PREFIX_LEN) &&
	    strncmp(name, XATTR_USER_PREFIX, XATTR_USER_PREFIX_LEN))
		return -EOPNOTSUPP;

	return 0;
}

int __hfsplus_setxattr(struct inode *inode, const char *name,
			const void *value, size_t size, int flags)
{
	int err = 0;
	struct hfs_find_data cat_fd;
	hfsplus_cat_entry entry;
	u16 cat_entry_flags, cat_entry_type;
	u16 folder_finderinfo_len = sizeof(struct DInfo) +
					sizeof(struct DXInfo);
	u16 file_finderinfo_len = sizeof(struct FInfo) +
					sizeof(struct FXInfo);

	if ((!S_ISREG(inode->i_mode) &&
			!S_ISDIR(inode->i_mode)) ||
				HFSPLUS_IS_RSRC(inode))
		return -EOPNOTSUPP;

	err = can_set_xattr(inode, name, value, size);
	if (err)
		return err;

	if (strncmp(name, XATTR_MAC_OSX_PREFIX,
				XATTR_MAC_OSX_PREFIX_LEN) == 0)
		name += XATTR_MAC_OSX_PREFIX_LEN;

	if (value == NULL) {
		value = "";
		size = 0;
	}

	err = hfs_find_init(HFSPLUS_SB(inode->i_sb)->cat_tree, &cat_fd);
	if (err) {
		pr_err("can't init xattr find struct\n");
		return err;
	}

	err = hfsplus_find_cat(inode->i_sb, inode->i_ino, &cat_fd);
	if (err) {
		pr_err("catalog searching failed\n");
		goto end_setxattr;
	}

	if (!strcmp_xattr_finder_info(name)) {
		if (flags & XATTR_CREATE) {
			pr_err("xattr exists yet\n");
			err = -EOPNOTSUPP;
			goto end_setxattr;
		}
#ifdef CONFIG_SYNO_HFSPLUS_EA // SOLVE GFP call trace
		cat_entry_type = hfs_bnode_read_u16(cat_fd.bnode, cat_fd.entryoffset);
		if (cat_entry_type == HFSPLUS_FOLDER) {
			hfs_bnode_read(cat_fd.bnode, &entry, cat_fd.entryoffset, sizeof(struct hfsplus_cat_folder));
		} else if (cat_entry_type == HFSPLUS_FILE) {
			hfs_bnode_read(cat_fd.bnode, &entry, cat_fd.entryoffset, sizeof(struct hfsplus_cat_file));
		}
#else
		hfs_bnode_read(cat_fd.bnode, &entry, cat_fd.entryoffset,
					sizeof(hfsplus_cat_entry));
#endif
		if (be16_to_cpu(entry.type) == HFSPLUS_FOLDER) {
			if (size == folder_finderinfo_len) {
				memcpy(&entry.folder.user_info, value,
						folder_finderinfo_len);
				hfs_bnode_write(cat_fd.bnode, &entry,
					cat_fd.entryoffset,
					sizeof(struct hfsplus_cat_folder));
				hfsplus_mark_inode_dirty(inode,
						HFSPLUS_I_CAT_DIRTY);
			} else {
				err = -ERANGE;
				goto end_setxattr;
			}
		} else if (be16_to_cpu(entry.type) == HFSPLUS_FILE) {
			if (size == file_finderinfo_len) {
				memcpy(&entry.file.user_info, value,
						file_finderinfo_len);
				hfs_bnode_write(cat_fd.bnode, &entry,
					cat_fd.entryoffset,
					sizeof(struct hfsplus_cat_file));
				hfsplus_mark_inode_dirty(inode,
						HFSPLUS_I_CAT_DIRTY);
			} else {
				err = -ERANGE;
				goto end_setxattr;
			}
		} else {
			err = -EOPNOTSUPP;
			goto end_setxattr;
		}
		goto end_setxattr;
	}

	if (!HFSPLUS_SB(inode->i_sb)->attr_tree) {
		err = -EOPNOTSUPP;
		goto end_setxattr;
	}

	if (hfsplus_attr_exists(inode, name)) {
		if (flags & XATTR_CREATE) {
			pr_err("xattr exists yet\n");
			err = -EOPNOTSUPP;
			goto end_setxattr;
		}
		err = hfsplus_delete_attr(inode, name);
		if (err)
			goto end_setxattr;
		err = hfsplus_create_attr(inode, name, value, size);
		if (err)
			goto end_setxattr;
	} else {
		if (flags & XATTR_REPLACE) {
			pr_err("cannot replace xattr\n");
			err = -EOPNOTSUPP;
			goto end_setxattr;
		}
		err = hfsplus_create_attr(inode, name, value, size);
		if (err)
			goto end_setxattr;
	}

	cat_entry_type = hfs_bnode_read_u16(cat_fd.bnode, cat_fd.entryoffset);
	if (cat_entry_type == HFSPLUS_FOLDER) {
		cat_entry_flags = hfs_bnode_read_u16(cat_fd.bnode,
				    cat_fd.entryoffset +
				    offsetof(struct hfsplus_cat_folder, flags));
		cat_entry_flags |= HFSPLUS_XATTR_EXISTS;
		if (!strcmp_xattr_acl(name))
			cat_entry_flags |= HFSPLUS_ACL_EXISTS;
		hfs_bnode_write_u16(cat_fd.bnode, cat_fd.entryoffset +
				offsetof(struct hfsplus_cat_folder, flags),
				cat_entry_flags);
		hfsplus_mark_inode_dirty(inode, HFSPLUS_I_CAT_DIRTY);
	} else if (cat_entry_type == HFSPLUS_FILE) {
		cat_entry_flags = hfs_bnode_read_u16(cat_fd.bnode,
				    cat_fd.entryoffset +
				    offsetof(struct hfsplus_cat_file, flags));
		cat_entry_flags |= HFSPLUS_XATTR_EXISTS;
		if (!strcmp_xattr_acl(name))
			cat_entry_flags |= HFSPLUS_ACL_EXISTS;
		hfs_bnode_write_u16(cat_fd.bnode, cat_fd.entryoffset +
				    offsetof(struct hfsplus_cat_file, flags),
				    cat_entry_flags);
		hfsplus_mark_inode_dirty(inode, HFSPLUS_I_CAT_DIRTY);
	} else {
		pr_err("invalid catalog entry type\n");
		err = -EIO;
		goto end_setxattr;
	}

end_setxattr:
	hfs_find_exit(&cat_fd);
	return err;
}

static inline int is_osx_xattr(const char *xattr_name)
{
	return !is_known_namespace(xattr_name);
}

static int name_len(const char *xattr_name, int xattr_name_len)
{
	int len = xattr_name_len + 1;

	if (is_osx_xattr(xattr_name))
		len += XATTR_MAC_OSX_PREFIX_LEN;

	return len;
}

static int copy_name(char *buffer, const char *xattr_name, int name_len)
{
	int len = name_len;
	int offset = 0;

	if (is_osx_xattr(xattr_name)) {
		strncpy(buffer, XATTR_MAC_OSX_PREFIX, XATTR_MAC_OSX_PREFIX_LEN);
		offset += XATTR_MAC_OSX_PREFIX_LEN;
		len += XATTR_MAC_OSX_PREFIX_LEN;
	}

	strncpy(buffer + offset, xattr_name, name_len);
	memset(buffer + offset + name_len, 0, 1);
	len += 1;

	return len;
}

static ssize_t hfsplus_getxattr_finder_info(struct dentry *dentry,
						void *value, size_t size)
{
	ssize_t res = 0;
	struct inode *inode = dentry->d_inode;
	struct hfs_find_data fd;
	u16 entry_type;
	u16 folder_rec_len = sizeof(struct DInfo) + sizeof(struct DXInfo);
	u16 file_rec_len = sizeof(struct FInfo) + sizeof(struct FXInfo);
	u16 record_len = max(folder_rec_len, file_rec_len);
	u8 folder_finder_info[sizeof(struct DInfo) + sizeof(struct DXInfo)];
	u8 file_finder_info[sizeof(struct FInfo) + sizeof(struct FXInfo)];

	if (size >= record_len) {
		res = hfs_find_init(HFSPLUS_SB(inode->i_sb)->cat_tree, &fd);
		if (res) {
			pr_err("can't init xattr find struct\n");
			return res;
		}
		res = hfsplus_find_cat(inode->i_sb, inode->i_ino, &fd);
		if (res)
			goto end_getxattr_finder_info;
		entry_type = hfs_bnode_read_u16(fd.bnode, fd.entryoffset);

		if (entry_type == HFSPLUS_FOLDER) {
			hfs_bnode_read(fd.bnode, folder_finder_info,
				fd.entryoffset +
				offsetof(struct hfsplus_cat_folder, user_info),
				folder_rec_len);
			hfsplus_zero_dateadded(entry_type, folder_finder_info);
			memcpy(value, folder_finder_info, folder_rec_len);
			res = folder_rec_len;
		} else if (entry_type == HFSPLUS_FILE) {
			hfs_bnode_read(fd.bnode, file_finder_info,
				fd.entryoffset +
				offsetof(struct hfsplus_cat_file, user_info),
				file_rec_len);
			hfsplus_zero_dateadded(entry_type, file_finder_info);
			memcpy(value, file_finder_info, file_rec_len);
			res = file_rec_len;
		} else {
			res = -EOPNOTSUPP;
			goto end_getxattr_finder_info;
		}
	} else
		res = size ? -ERANGE : record_len;

end_getxattr_finder_info:
	if (size >= record_len)
		hfs_find_exit(&fd);
	return res;
}

ssize_t hfsplus_getxattr(struct dentry *dentry, const char *name,
			 void *value, size_t size)
{
	struct inode *inode = dentry->d_inode;
	struct hfs_find_data fd;
	hfsplus_attr_entry *entry;
	__be32 xattr_record_type;
	u32 record_type;
#ifdef CONFIG_SYNO_HFSPLUS_EA
	u32 record_length = 0;
#else
	u16 record_length = 0;
#endif
	ssize_t res = 0;

	if ((!S_ISREG(inode->i_mode) &&
			!S_ISDIR(inode->i_mode)) ||
				HFSPLUS_IS_RSRC(inode))
		return -EOPNOTSUPP;

	if (strncmp(name, XATTR_MAC_OSX_PREFIX,
				XATTR_MAC_OSX_PREFIX_LEN) == 0) {
		/* skip "osx." prefix */
		name += XATTR_MAC_OSX_PREFIX_LEN;
		/*
		 * Don't allow retrieving properly prefixed attributes
		 * by prepending them with "osx."
		 */
		if (is_known_namespace(name))
			return -EOPNOTSUPP;
	}

	if (!strcmp_xattr_finder_info(name))
		return hfsplus_getxattr_finder_info(dentry, value, size);

	if (!HFSPLUS_SB(inode->i_sb)->attr_tree)
		return -EOPNOTSUPP;

	entry = hfsplus_alloc_attr_entry();
	if (!entry) {
		pr_err("can't allocate xattr entry\n");
		return -ENOMEM;
	}

	res = hfs_find_init(HFSPLUS_SB(inode->i_sb)->attr_tree, &fd);
	if (res) {
		pr_err("can't init xattr find struct\n");
		goto failed_getxattr_init;
	}

	res = hfsplus_find_attr(inode->i_sb, inode->i_ino, name, &fd);
	if (res) {
		if (res == -ENOENT)
			res = -ENODATA;
		else
			pr_err("xattr searching failed\n");
		goto out;
	}

	hfs_bnode_read(fd.bnode, &xattr_record_type,
			fd.entryoffset, sizeof(xattr_record_type));
	record_type = be32_to_cpu(xattr_record_type);
	if (record_type == HFSPLUS_ATTR_INLINE_DATA) {
#ifdef CONFIG_SYNO_HFSPLUS_EA
		record_length = hfs_bnode_read_u32(fd.bnode,
				fd.entryoffset +
				offsetof(struct hfsplus_attr_inline_data,
				length));
		if ((offsetof(struct hfsplus_attr_inline_data,
					raw_bytes) + record_length) > hfsplus_get_attr_tree_cache_size()) {
			pr_err("invalid xattr record size\n");
			res = -EIO;
			goto out;
		}
#else
		record_length = hfs_bnode_read_u16(fd.bnode,
				fd.entryoffset +
				offsetof(struct hfsplus_attr_inline_data,
				length));
		if (record_length > HFSPLUS_MAX_INLINE_DATA_SIZE) {
			pr_err("invalid xattr record size\n");
			res = -EIO;
			goto out;
		}
#endif
	} else if (record_type == HFSPLUS_ATTR_FORK_DATA ||
			record_type == HFSPLUS_ATTR_EXTENTS) {
		pr_err("only inline data xattr are supported\n");
		res = -EOPNOTSUPP;
		goto out;
	} else {
		pr_err("invalid xattr record\n");
		res = -EIO;
		goto out;
	}

	if (size) {
		hfs_bnode_read(fd.bnode, entry, fd.entryoffset,
				offsetof(struct hfsplus_attr_inline_data,
					raw_bytes) + record_length);
	}

	if (size >= record_length) {
		memcpy(value, entry->inline_data.raw_bytes, record_length);
		res = record_length;
	} else
		res = size ? -ERANGE : record_length;

out:
	hfs_find_exit(&fd);

failed_getxattr_init:
	hfsplus_destroy_attr_entry(entry);
	return res;
}

static inline int can_list(const char *xattr_name)
{
	if (!xattr_name)
		return 0;

	return strncmp(xattr_name, XATTR_TRUSTED_PREFIX,
			XATTR_TRUSTED_PREFIX_LEN) ||
				capable(CAP_SYS_ADMIN);
}

#ifdef CONFIG_SYNO_HFSPLUS_EA
static ssize_t hfsplus_listxattr_rfork(struct dentry *dentry,
						char *buffer, size_t size)
{
	ssize_t res = 0;
	struct inode *inode = dentry->d_inode;
	struct hfs_find_data fd;
	u16 entry_type;
	int xattr_name_len, symbols_count;
	struct hfsplus_fork_raw rsrc_fork = {0};

	res = hfs_find_init(HFSPLUS_SB(inode->i_sb)->cat_tree, &fd);
	if (res) {
		pr_err("can't init xattr find struct\n");
		return res;
	}

	res = hfsplus_find_cat(inode->i_sb, inode->i_ino, &fd);
	if (res)
		goto end_listxattr_rfork;

	entry_type = hfs_bnode_read_u16(fd.bnode, fd.entryoffset);
	if (entry_type == HFSPLUS_FILE) {
		hfs_bnode_read(fd.bnode, &rsrc_fork,
				fd.entryoffset +
				offsetof(struct hfsplus_cat_file, rsrc_fork),
				sizeof(struct hfsplus_fork_raw));
	}
	if (be64_to_cpu(rsrc_fork.total_size) > 0) {
		symbols_count = sizeof(HFSPLUS_XATTR_RESOURCE_FORK_NAME) - 1;
		xattr_name_len =
			name_len(HFSPLUS_XATTR_RESOURCE_FORK_NAME, symbols_count);
		if (!buffer || !size) {
			if (can_list(HFSPLUS_XATTR_RESOURCE_FORK_NAME))
				res = xattr_name_len;
		} else if (can_list(HFSPLUS_XATTR_RESOURCE_FORK_NAME)) {
			if (size < xattr_name_len)
				res = -ERANGE;
			else {
				res = copy_name(buffer,
						HFSPLUS_XATTR_RESOURCE_FORK_NAME,
						symbols_count);
			}
		}
	}

end_listxattr_rfork:
	hfs_find_exit(&fd);

	return res;
}
#endif
static ssize_t hfsplus_listxattr_finder_info(struct dentry *dentry,
						char *buffer, size_t size)
{
	ssize_t res = 0;
	struct inode *inode = dentry->d_inode;
	struct hfs_find_data fd;
	u16 entry_type;
	u8 folder_finder_info[sizeof(struct DInfo) + sizeof(struct DXInfo)];
	u8 file_finder_info[sizeof(struct FInfo) + sizeof(struct FXInfo)];
	unsigned long len, found_bit;
	int xattr_name_len, symbols_count;

	res = hfs_find_init(HFSPLUS_SB(inode->i_sb)->cat_tree, &fd);
	if (res) {
		pr_err("can't init xattr find struct\n");
		return res;
	}

	res = hfsplus_find_cat(inode->i_sb, inode->i_ino, &fd);
	if (res)
		goto end_listxattr_finder_info;

	entry_type = hfs_bnode_read_u16(fd.bnode, fd.entryoffset);
	if (entry_type == HFSPLUS_FOLDER) {
		len = sizeof(struct DInfo) + sizeof(struct DXInfo);
		hfs_bnode_read(fd.bnode, folder_finder_info,
				fd.entryoffset +
				offsetof(struct hfsplus_cat_folder, user_info),
				len);
#ifdef CONFIG_SYNO_HFSPLUS_EA
		hfsplus_zero_dateadded(entry_type, folder_finder_info);
#endif
		found_bit = find_first_bit((void *)folder_finder_info, len*8);
	} else if (entry_type == HFSPLUS_FILE) {
		len = sizeof(struct FInfo) + sizeof(struct FXInfo);
		hfs_bnode_read(fd.bnode, file_finder_info,
				fd.entryoffset +
				offsetof(struct hfsplus_cat_file, user_info),
				len);
#ifdef CONFIG_SYNO_HFSPLUS_EA
		hfsplus_zero_dateadded(entry_type, file_finder_info);
#endif
		found_bit = find_first_bit((void *)file_finder_info, len*8);
	} else {
		res = -EOPNOTSUPP;
		goto end_listxattr_finder_info;
	}

	if (found_bit >= (len*8))
		res = 0;
	else {
		symbols_count = sizeof(HFSPLUS_XATTR_FINDER_INFO_NAME) - 1;
		xattr_name_len =
			name_len(HFSPLUS_XATTR_FINDER_INFO_NAME, symbols_count);
		if (!buffer || !size) {
			if (can_list(HFSPLUS_XATTR_FINDER_INFO_NAME))
				res = xattr_name_len;
		} else if (can_list(HFSPLUS_XATTR_FINDER_INFO_NAME)) {
			if (size < xattr_name_len)
				res = -ERANGE;
			else {
				res = copy_name(buffer,
						HFSPLUS_XATTR_FINDER_INFO_NAME,
						symbols_count);
			}
		}
	}

end_listxattr_finder_info:
	hfs_find_exit(&fd);

	return res;
}

#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
int
hfsplus_syno_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags)
{
	int ret = 0;
	mutex_lock(&syno_hfsplus_global_mutex);
	ret = generic_setxattr(dentry, name, value, size, flags);
	mutex_unlock(&syno_hfsplus_global_mutex);
	return ret;
}

ssize_t
hfsplus_syno_getxattr(struct dentry *dentry, const char *name, void *buffer, size_t size)
{
	ssize_t ret = 0;
	mutex_lock(&syno_hfsplus_global_mutex);
	ret = generic_getxattr(dentry, name, buffer, size);
	mutex_unlock(&syno_hfsplus_global_mutex);
	return ret;
}
#endif

ssize_t hfsplus_listxattr(struct dentry *dentry, char *buffer, size_t size)
{
	ssize_t err;
	ssize_t res = 0;
	struct inode *inode = dentry->d_inode;
	struct hfs_find_data fd;
	u16 key_len = 0;
	struct hfsplus_attr_key attr_key;
	char strbuf[HFSPLUS_ATTR_MAX_STRLEN +
			XATTR_MAC_OSX_PREFIX_LEN + 1] = {0};
	int xattr_name_len;

	if ((!S_ISREG(inode->i_mode) &&
			!S_ISDIR(inode->i_mode)) ||
				HFSPLUS_IS_RSRC(inode))
		return -EOPNOTSUPP;

#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	mutex_lock(&syno_hfsplus_global_mutex);
#endif
	res = hfsplus_listxattr_finder_info(dentry, buffer, size);
	if (res < 0)
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	{
		mutex_unlock(&syno_hfsplus_global_mutex);
		return res;
	}
#else
		return res;
#endif
#ifdef CONFIG_SYNO_HFSPLUS_EA
	res += hfsplus_listxattr_rfork(dentry, buffer + res, size);
	if (res < 0) {
#ifdef MY_DEF_HERE
		mutex_unlock(&syno_hfsplus_global_mutex);
#endif
		return res;
	}
#endif
	else if (!HFSPLUS_SB(inode->i_sb)->attr_tree)
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	{
		mutex_unlock(&syno_hfsplus_global_mutex);
		return (res == 0) ? -EOPNOTSUPP : res;
	}
#else
		return (res == 0) ? -EOPNOTSUPP : res;
#endif

	err = hfs_find_init(HFSPLUS_SB(inode->i_sb)->attr_tree, &fd);
	if (err) {
		pr_err("can't init xattr find struct\n");
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
		mutex_unlock(&syno_hfsplus_global_mutex);
#endif
		return err;
	}

	err = hfsplus_find_attr(inode->i_sb, inode->i_ino, NULL, &fd);
	if (err) {
		if (err == -ENOENT) {
			if (res == 0)
				res = -ENODATA;
			goto end_listxattr;
		} else {
			res = err;
			goto end_listxattr;
		}
	}

	for (;;) {
		key_len = hfs_bnode_read_u16(fd.bnode, fd.keyoffset);
		if (key_len == 0 || key_len > fd.tree->max_key_len) {
			pr_err("invalid xattr key length: %d\n", key_len);
			res = -EIO;
			goto end_listxattr;
		}

		hfs_bnode_read(fd.bnode, &attr_key,
				fd.keyoffset, key_len + sizeof(key_len));

		if (be32_to_cpu(attr_key.cnid) != inode->i_ino)
			goto end_listxattr;

		xattr_name_len = HFSPLUS_ATTR_MAX_STRLEN;
#ifdef CONFIG_SYNO_HFSPLUS_EA
		if (hfsplus_attr_uni2asc(inode->i_sb,
			(const struct hfsplus_unistr *)&fd.key->attr.key_name,
					strbuf, &xattr_name_len)) {
			pr_err("unicode conversion failed\n");
			res = -EIO;
			goto end_listxattr;
		}
#else
		if (hfsplus_uni2asc(inode->i_sb,
			(const struct hfsplus_unistr *)&fd.key->attr.key_name,
					strbuf, &xattr_name_len)) {
			pr_err("unicode conversion failed\n");
			res = -EIO;
			goto end_listxattr;
		}
#endif

		if (!buffer || !size) {
			if (can_list(strbuf))
				res += name_len(strbuf, xattr_name_len);
		} else if (can_list(strbuf)) {
			if (size < (res + name_len(strbuf, xattr_name_len))) {
				res = -ERANGE;
				goto end_listxattr;
			} else
				res += copy_name(buffer + res,
						strbuf, xattr_name_len);
		}

		if (hfs_brec_goto(&fd, 1))
			goto end_listxattr;
	}

end_listxattr:
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	mutex_unlock(&syno_hfsplus_global_mutex);
#endif
	hfs_find_exit(&fd);
	return res;
}

int hfsplus_removexattr(struct dentry *dentry, const char *name)
{
	int err = 0;
	struct inode *inode = dentry->d_inode;
	struct hfs_find_data cat_fd;
	u16 flags;
	u16 cat_entry_type;
	int is_xattr_acl_deleted = 0;
	int is_all_xattrs_deleted = 0;

	if ((!S_ISREG(inode->i_mode) &&
			!S_ISDIR(inode->i_mode)) ||
				HFSPLUS_IS_RSRC(inode))
		return -EOPNOTSUPP;

	if (!HFSPLUS_SB(inode->i_sb)->attr_tree)
		return -EOPNOTSUPP;
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	mutex_lock(&syno_hfsplus_global_mutex);
#endif

	err = can_set_xattr(inode, name, NULL, 0);
	if (err)
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	{
		mutex_unlock(&syno_hfsplus_global_mutex);
		return err;
	}
#else
		return err;
#endif

	if (strncmp(name, XATTR_MAC_OSX_PREFIX,
				XATTR_MAC_OSX_PREFIX_LEN) == 0)
		name += XATTR_MAC_OSX_PREFIX_LEN;

	if (!strcmp_xattr_finder_info(name))
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	{
		mutex_unlock(&syno_hfsplus_global_mutex);
		return -EOPNOTSUPP;
	}
#else
		return -EOPNOTSUPP;
#endif

	err = hfs_find_init(HFSPLUS_SB(inode->i_sb)->cat_tree, &cat_fd);
	if (err) {
		pr_err("can't init xattr find struct\n");
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
		mutex_unlock(&syno_hfsplus_global_mutex);
#endif
		return err;
	}

	err = hfsplus_find_cat(inode->i_sb, inode->i_ino, &cat_fd);
	if (err) {
		pr_err("catalog searching failed\n");
		goto end_removexattr;
	}

	err = hfsplus_delete_attr(inode, name);
	if (err)
		goto end_removexattr;

	is_xattr_acl_deleted = !strcmp_xattr_acl(name);
	is_all_xattrs_deleted = !hfsplus_attr_exists(inode, NULL);

	if (!is_xattr_acl_deleted && !is_all_xattrs_deleted)
		goto end_removexattr;

	cat_entry_type = hfs_bnode_read_u16(cat_fd.bnode, cat_fd.entryoffset);

	if (cat_entry_type == HFSPLUS_FOLDER) {
		flags = hfs_bnode_read_u16(cat_fd.bnode, cat_fd.entryoffset +
				offsetof(struct hfsplus_cat_folder, flags));
		if (is_xattr_acl_deleted)
			flags &= ~HFSPLUS_ACL_EXISTS;
		if (is_all_xattrs_deleted)
			flags &= ~HFSPLUS_XATTR_EXISTS;
		hfs_bnode_write_u16(cat_fd.bnode, cat_fd.entryoffset +
				offsetof(struct hfsplus_cat_folder, flags),
				flags);
		hfsplus_mark_inode_dirty(inode, HFSPLUS_I_CAT_DIRTY);
	} else if (cat_entry_type == HFSPLUS_FILE) {
		flags = hfs_bnode_read_u16(cat_fd.bnode, cat_fd.entryoffset +
				offsetof(struct hfsplus_cat_file, flags));
		if (is_xattr_acl_deleted)
			flags &= ~HFSPLUS_ACL_EXISTS;
		if (is_all_xattrs_deleted)
			flags &= ~HFSPLUS_XATTR_EXISTS;
		hfs_bnode_write_u16(cat_fd.bnode, cat_fd.entryoffset +
				offsetof(struct hfsplus_cat_file, flags),
				flags);
		hfsplus_mark_inode_dirty(inode, HFSPLUS_I_CAT_DIRTY);
	} else {
		pr_err("invalid catalog entry type\n");
		err = -EIO;
		goto end_removexattr;
	}

end_removexattr:
	hfs_find_exit(&cat_fd);
#ifdef CONFIG_SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	mutex_unlock(&syno_hfsplus_global_mutex);
#endif
	return err;
}

static int hfsplus_osx_getxattr(struct dentry *dentry, const char *name,
					void *buffer, size_t size, int type)
{
	char xattr_name[HFSPLUS_ATTR_MAX_STRLEN +
				XATTR_MAC_OSX_PREFIX_LEN + 1] = {0};
	size_t len = strlen(name);

	if (!strcmp(name, ""))
		return -EINVAL;

	if (len > HFSPLUS_ATTR_MAX_STRLEN)
		return -EOPNOTSUPP;

	strcpy(xattr_name, XATTR_MAC_OSX_PREFIX);
	strcpy(xattr_name + XATTR_MAC_OSX_PREFIX_LEN, name);

	return hfsplus_getxattr(dentry, xattr_name, buffer, size);
}

static int hfsplus_osx_setxattr(struct dentry *dentry, const char *name,
		const void *buffer, size_t size, int flags, int type)
{
	char xattr_name[HFSPLUS_ATTR_MAX_STRLEN +
				XATTR_MAC_OSX_PREFIX_LEN + 1] = {0};
	size_t len = strlen(name);

	if (!strcmp(name, ""))
		return -EINVAL;

	if (len > HFSPLUS_ATTR_MAX_STRLEN)
		return -EOPNOTSUPP;

	strcpy(xattr_name, XATTR_MAC_OSX_PREFIX);
	strcpy(xattr_name + XATTR_MAC_OSX_PREFIX_LEN, name);

	return hfsplus_setxattr(dentry, xattr_name, buffer, size, flags);
}

static size_t hfsplus_osx_listxattr(struct dentry *dentry, char *list,
		size_t list_size, const char *name, size_t name_len, int type)
{
	/*
	 * This method is not used.
	 * It is used hfsplus_listxattr() instead of generic_listxattr().
	 */
	return -EOPNOTSUPP;
}

const struct xattr_handler hfsplus_xattr_osx_handler = {
	.prefix	= XATTR_MAC_OSX_PREFIX,
	.list	= hfsplus_osx_listxattr,
	.get	= hfsplus_osx_getxattr,
	.set	= hfsplus_osx_setxattr,
};
