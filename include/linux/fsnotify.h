#ifndef _LINUX_FS_NOTIFY_H
#define _LINUX_FS_NOTIFY_H

/*
 * include/linux/fsnotify.h - generic hooks for filesystem notification, to
 * reduce in-source duplication from both dnotify and inotify.
 *
 * We don't compile any of this away in some complicated menagerie of ifdefs.
 * Instead, we rely on the code inside to optimize away as needed.
 *
 * (C) Copyright 2005 Robert Love
 */

#include <linux/fsnotify_backend.h>
#include <linux/audit.h>
#include <linux/slab.h>
#include <linux/bug.h>

#ifdef CONFIG_SYNO_FS_ARCHIVE_VERSION
#include <linux/xattr.h>
#endif

#ifdef CONFIG_SYNO_FS_NOTIFY
#include <linux/mount.h>
#endif

#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
static inline void SYNO_ArchiveModify(struct inode *inode, int blSetSMBArchive)
{
	struct dentry *dentry;
#ifdef CONFIG_SYNO_FS_ARCHIVE_BIT
	u32 new_archive_bit;
	u32 old_archive_bit;
#endif
#ifdef CONFIG_SYNO_FS_ARCHIVE_VERSION
	u32 old_version;
	u32 new_version;
	int err;
#endif
	if (NULL == inode) {
		return;
	}
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode) ||
		S_ISFIFO(inode->i_mode) || S_ISSOCK(inode->i_mode)) {
		return;
	}
	dentry = d_find_alias(inode);
	if (!dentry)
		return;

#ifdef CONFIG_SYNO_FS_ARCHIVE_BIT
	mutex_lock(&inode->i_syno_mutex);
	if (syno_op_get_archive_bit(dentry, &old_archive_bit)) {
		goto next;
	}

	if (blSetSMBArchive) {
		new_archive_bit = old_archive_bit | (S2_SMB_ARCHIVE|ALL_IARCHIVE);
	} else {
		new_archive_bit = old_archive_bit | ALL_IARCHIVE;
	}
	if (new_archive_bit == old_archive_bit) {
		goto next;
	}
	syno_op_set_archive_bit_nolock(dentry, new_archive_bit);
next:
	mutex_unlock(&inode->i_syno_mutex);
#endif /* CONFIG_SYNO_FS_ARCHIVE_BIT */
#ifdef CONFIG_SYNO_FS_ARCHIVE_VERSION
	if (!inode->i_op->syno_get_archive_ver)
		goto out;

	err = inode->i_op->syno_get_archive_ver(dentry, &old_version);
	if (err)
		goto out;

	inode->i_sb->s_op->syno_get_sb_archive_ver(inode->i_sb, &new_version);
	if (err)
		goto out;

	new_version += 1;
	if (new_version != old_version)
		inode->i_op->syno_set_archive_ver(dentry, new_version);
out:
#endif /* CONFIG_SYNO_FS_ARCHIVE_VERSION */
	if (dentry) {
		dput(dentry);
	}
}
#endif /* CONFIG_SYNO_FS_ARCHIVE_BIT || CONFIG_SYNO_FS_ARCHIVE_VERSION*/

#ifdef CONFIG_SYNO_FS_NOTIFY
extern int SYNONotify(struct dentry *dentry, __u32 mask);
#endif

/*
 * fsnotify_d_instantiate - instantiate a dentry for inode
 */
static inline void fsnotify_d_instantiate(struct dentry *dentry,
					  struct inode *inode)
{
	__fsnotify_d_instantiate(dentry, inode);
}

/* Notify this dentry's parent about a child's events. */
static inline int fsnotify_parent(struct path *path, struct dentry *dentry, __u32 mask)
{
	if (!dentry)
		dentry = path->dentry;

	return __fsnotify_parent(path, dentry, mask);
}

/* simple call site for access decisions */
static inline int fsnotify_perm(struct file *file, int mask)
{
	struct path *path = &file->f_path;
	struct inode *inode = path->dentry->d_inode;
	__u32 fsnotify_mask = 0;
	int ret;

	if (file->f_mode & FMODE_NONOTIFY)
		return 0;
	if (!(mask & (MAY_READ | MAY_OPEN)))
		return 0;
	if (mask & MAY_OPEN)
		fsnotify_mask = FS_OPEN_PERM;
	else if (mask & MAY_READ)
		fsnotify_mask = FS_ACCESS_PERM;
	else
		BUG();

	ret = fsnotify_parent(path, NULL, fsnotify_mask);
	if (ret)
		return ret;

	return fsnotify(inode, fsnotify_mask, path, FSNOTIFY_EVENT_PATH, NULL, 0);
}

/*
 * fsnotify_d_move - dentry has been moved
 */
static inline void fsnotify_d_move(struct dentry *dentry)
{
	/*
	 * On move we need to update dentry->d_flags to indicate if the new parent
	 * cares about events from this dentry.
	 */
	__fsnotify_update_dcache_flags(dentry);
}

/*
 * fsnotify_link_count - inode's link count changed
 */
static inline void fsnotify_link_count(struct inode *inode)
{
	fsnotify(inode, FS_ATTRIB, inode, FSNOTIFY_EVENT_INODE, NULL, 0);
}

#ifdef CONFIG_SYNO_FS_NOTIFY
struct synotify_rename_path {
	char *old_full_path;
	char *new_full_path;
	struct vfsmount *vfs_mnt;
	struct synotify_rename_path *next;
};

#endif
/*
 * fsnotify_move - file old_name at old_dir was moved to new_name at new_dir
 */
#ifdef CONFIG_SYNO_FS_NOTIFY
static inline void fsnotify_move(struct inode *old_dir, struct inode *new_dir,
				 const unsigned char *old_name,
				 int isdir, struct inode *target, struct dentry *moved, struct synotify_rename_path *path_list)

#else
static inline void fsnotify_move(struct inode *old_dir, struct inode *new_dir,
				 const unsigned char *old_name,
				 int isdir, struct inode *target, struct dentry *moved)
#endif
{
	struct inode *source = moved->d_inode;
	u32 fs_cookie = fsnotify_get_cookie();
	__u32 old_dir_mask = (FS_EVENT_ON_CHILD | FS_MOVED_FROM);
	__u32 new_dir_mask = (FS_EVENT_ON_CHILD | FS_MOVED_TO);
	const unsigned char *new_name = moved->d_name.name;

	if (old_dir == new_dir)
		old_dir_mask |= FS_DN_RENAME;

	if (isdir) {
		old_dir_mask |= FS_ISDIR;
		new_dir_mask |= FS_ISDIR;
	}

#ifdef CONFIG_SYNO_FS_NOTIFY
	/* handle syno notify:
	 * 1. we should check if file/dir moved within same mnt point. If does, we simply
	 *    notify a rename event.
	 * 2. if this rename does not occur within same mnt point, then we have to send MOVE_FROM
	 *    and MOVE_TO to mnt points respectively.
	 */

	// prepare source notify data
	while(path_list) {
		struct synotify_rename_path *tmp = path_list;
		struct path tmp_path;
		memset (&tmp_path, 0, sizeof(struct path));

		tmp_path.mnt = tmp->vfs_mnt;

		SYNOFsnotify(old_dir_mask, &tmp_path, FSNOTIFY_EVENT_SYNO, tmp->old_full_path, fs_cookie);
		SYNOFsnotify(new_dir_mask, &tmp_path, FSNOTIFY_EVENT_SYNO, tmp->new_full_path, fs_cookie);

		path_list = path_list->next;
	}
#endif

	fsnotify(old_dir, old_dir_mask, old_dir, FSNOTIFY_EVENT_INODE, old_name, fs_cookie);
	fsnotify(new_dir, new_dir_mask, new_dir, FSNOTIFY_EVENT_INODE, new_name, fs_cookie);

#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	SYNO_ArchiveModify(old_dir, 0);
	if (old_dir != new_dir) {
		SYNO_ArchiveModify(new_dir, 0);
	}
#endif /* CONFIG_SYNO_FS_ARCHIVE_BIT || CONFIG_SYNO_FS_ARCHIVE_VERSION*/

	if (target)
#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	{
		fsnotify_link_count(target);
		SYNO_ArchiveModify(target, 0);
	}
#else
		fsnotify_link_count(target);
#endif /* CONFIG_SYNO_FS_ARCHIVE_BIT || CONFIG_SYNO_FS_ARCHIVE_VERSION*/

	if (source)
#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	{
		fsnotify(source, FS_MOVE_SELF, moved->d_inode, FSNOTIFY_EVENT_INODE, NULL, 0);
		SYNO_ArchiveModify(source, 1);
	}
#else
		fsnotify(source, FS_MOVE_SELF, moved->d_inode, FSNOTIFY_EVENT_INODE, NULL, 0);
#endif /* CONFIG_SYNO_FS_ARCHIVE_BIT || CONFIG_SYNO_FS_ARCHIVE_VERSION*/
	audit_inode_child(new_dir, moved, AUDIT_TYPE_CHILD_CREATE);
}

/*
 * fsnotify_inode_delete - and inode is being evicted from cache, clean up is needed
 */
static inline void fsnotify_inode_delete(struct inode *inode)
{
	__fsnotify_inode_delete(inode);
}

/*
 * fsnotify_vfsmount_delete - a vfsmount is being destroyed, clean up is needed
 */
static inline void fsnotify_vfsmount_delete(struct vfsmount *mnt)
{
	__fsnotify_vfsmount_delete(mnt);
}

/*
 * fsnotify_nameremove - a filename was removed from a directory
 */
static inline void fsnotify_nameremove(struct dentry *dentry, int isdir)
{
	__u32 mask = FS_DELETE;

	if (isdir)
		mask |= FS_ISDIR;

#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	SYNO_ArchiveModify(dentry->d_parent->d_inode, 0);
#endif

#ifdef CONFIG_SYNO_FS_NOTIFY
	SYNONotify(dentry, mask);
#endif

	fsnotify_parent(NULL, dentry, mask);
}

/*
 * fsnotify_inoderemove - an inode is going away
 */
static inline void fsnotify_inoderemove(struct inode *inode)
{
	fsnotify(inode, FS_DELETE_SELF, inode, FSNOTIFY_EVENT_INODE, NULL, 0);
	__fsnotify_inode_delete(inode);
}

/*
 * fsnotify_create - 'name' was linked in
 */
static inline void fsnotify_create(struct inode *inode, struct dentry *dentry)
{
	audit_inode_child(inode, dentry, AUDIT_TYPE_CHILD_CREATE);

#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	SYNO_ArchiveModify(dentry->d_inode, 0);
#endif

#ifdef CONFIG_SYNO_FS_NOTIFY
	SYNONotify(dentry, FS_CREATE);
#endif

	fsnotify(inode, FS_CREATE, dentry->d_inode, FSNOTIFY_EVENT_INODE, dentry->d_name.name, 0);
}

/*
 * fsnotify_link - new hardlink in 'inode' directory
 * Note: We have to pass also the linked inode ptr as some filesystems leave
 *   new_dentry->d_inode NULL and instantiate inode pointer later
 */
static inline void fsnotify_link(struct inode *dir, struct inode *inode, struct dentry *new_dentry)
{
	fsnotify_link_count(inode);
	audit_inode_child(dir, new_dentry, AUDIT_TYPE_CHILD_CREATE);

#ifdef CONFIG_SYNO_FS_NOTIFY
	SYNONotify(new_dentry, FS_CREATE);
#endif
	fsnotify(dir, FS_CREATE, inode, FSNOTIFY_EVENT_INODE, new_dentry->d_name.name, 0);
}

/*
 * fsnotify_mkdir - directory 'name' was created
 */
static inline void fsnotify_mkdir(struct inode *inode, struct dentry *dentry)
{
	__u32 mask = (FS_CREATE | FS_ISDIR);
	struct inode *d_inode = dentry->d_inode;

	audit_inode_child(inode, dentry, AUDIT_TYPE_CHILD_CREATE);

#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	SYNO_ArchiveModify(d_inode, 0);
#endif

#ifdef CONFIG_SYNO_FS_NOTIFY
	SYNONotify(dentry, mask);
#endif
	fsnotify(inode, mask, d_inode, FSNOTIFY_EVENT_INODE, dentry->d_name.name, 0);
}

/*
 * fsnotify_access - file was read
 */
static inline void fsnotify_access(struct file *file)
{
	struct path *path = &file->f_path;
	struct inode *inode = path->dentry->d_inode;
	__u32 mask = FS_ACCESS;

	if (S_ISDIR(inode->i_mode))
		mask |= FS_ISDIR;

	if (!(file->f_mode & FMODE_NONOTIFY)) {
		fsnotify_parent(path, NULL, mask);
		fsnotify(inode, mask, path, FSNOTIFY_EVENT_PATH, NULL, 0);
	}
}

/*
 * fsnotify_modify - file was modified
 */
static inline void fsnotify_modify(struct file *file)
{
	struct path *path = &file->f_path;
	struct inode *inode = path->dentry->d_inode;
	__u32 mask = FS_MODIFY;

	if (S_ISDIR(inode->i_mode))
		mask |= FS_ISDIR;

	if (!(file->f_mode & FMODE_NONOTIFY)) {
#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
		SYNO_ArchiveModify(inode, 1);
#endif
		fsnotify_parent(path, NULL, mask);
		fsnotify(inode, mask, path, FSNOTIFY_EVENT_PATH, NULL, 0);
	}
}

/*
 * fsnotify_open - file was opened
 */
static inline void fsnotify_open(struct file *file)
{
	struct path *path = &file->f_path;
	struct inode *inode = path->dentry->d_inode;
	__u32 mask = FS_OPEN;

	if (S_ISDIR(inode->i_mode))
		mask |= FS_ISDIR;

	fsnotify_parent(path, NULL, mask);
	fsnotify(inode, mask, path, FSNOTIFY_EVENT_PATH, NULL, 0);
}

/*
 * fsnotify_close - file was closed
 */
static inline void fsnotify_close(struct file *file)
{
	struct path *path = &file->f_path;
	struct inode *inode = file_inode(file);
	fmode_t mode = file->f_mode;
	__u32 mask = (mode & FMODE_WRITE) ? FS_CLOSE_WRITE : FS_CLOSE_NOWRITE;

	if (S_ISDIR(inode->i_mode))
		mask |= FS_ISDIR;

	if (!(file->f_mode & FMODE_NONOTIFY)) {
		fsnotify_parent(path, NULL, mask);
		fsnotify(inode, mask, path, FSNOTIFY_EVENT_PATH, NULL, 0);
	}
}

/*
 * fsnotify_xattr - extended attributes were changed
 */
static inline void fsnotify_xattr(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	__u32 mask = FS_ATTRIB;

	if (S_ISDIR(inode->i_mode))
		mask |= FS_ISDIR;

#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	SYNO_ArchiveModify(inode, 1);
#endif
	fsnotify_parent(NULL, dentry, mask);

#ifdef CONFIG_SYNO_FS_NOTIFY
	SYNONotify(dentry, mask);
#endif

	fsnotify(inode, mask, inode, FSNOTIFY_EVENT_INODE, NULL, 0);
}

/*
 * fsnotify_change - notify_change event.  file was modified and/or metadata
 * was changed.
 */
static inline void fsnotify_change(struct dentry *dentry, unsigned int ia_valid)
{
	struct inode *inode = dentry->d_inode;
	__u32 mask = 0;

	if (ia_valid & ATTR_UID)
		mask |= FS_ATTRIB;
	if (ia_valid & ATTR_GID)
		mask |= FS_ATTRIB;
	if (ia_valid & ATTR_SIZE)
#if defined(CONFIG_SYNO_FS_ARCHIVE_BIT) || defined(CONFIG_SYNO_FS_ARCHIVE_VERSION)
	{
		mask |= FS_MODIFY;
		SYNO_ArchiveModify(inode, 1);
	}
#else
		mask |= FS_MODIFY;
#endif /* CONFIG_SYNO_FS_ARCHIVE_BIT || CONFIG_SYNO_FS_ARCHIVE_BIT */

	/* both times implies a utime(s) call */
	if ((ia_valid & (ATTR_ATIME | ATTR_MTIME)) == (ATTR_ATIME | ATTR_MTIME))
		mask |= FS_ATTRIB;
	else if (ia_valid & ATTR_ATIME)
		mask |= FS_ACCESS;
	else if (ia_valid & ATTR_MTIME)
		mask |= FS_MODIFY;

	if (ia_valid & ATTR_MODE)
		mask |= FS_ATTRIB;

	if (mask) {
		if (S_ISDIR(inode->i_mode))
			mask |= FS_ISDIR;

		fsnotify_parent(NULL, dentry, mask);
#ifdef CONFIG_SYNO_FS_NOTIFY
		SYNONotify(dentry, mask);
#endif
		fsnotify(inode, mask, inode, FSNOTIFY_EVENT_INODE, NULL, 0);
	}
}

#if defined(CONFIG_FSNOTIFY)	/* notify helpers */

/*
 * fsnotify_oldname_init - save off the old filename before we change it
 */
static inline const unsigned char *fsnotify_oldname_init(const unsigned char *name)
{
	return kstrdup(name, GFP_KERNEL);
}

/*
 * fsnotify_oldname_free - free the name we got from fsnotify_oldname_init
 */
static inline void fsnotify_oldname_free(const unsigned char *old_name)
{
	kfree(old_name);
}

#else	/* CONFIG_FSNOTIFY */

static inline const char *fsnotify_oldname_init(const unsigned char *name)
{
	return NULL;
}

static inline void fsnotify_oldname_free(const unsigned char *old_name)
{
}

#endif	/*  CONFIG_FSNOTIFY */

#endif	/* _LINUX_FS_NOTIFY_H */
