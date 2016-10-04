/*
 * linux/fs/synoacl_api.c
 *
 * Copyright (c) 2000-2014 Synology Inc.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>

#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/errno.h>

#include "synoacl_int.h"

#define VFS_MODULE	psynoacl_vfs_op
#define SYSCALL_MODULE	psynoacl_syscall_op

#define IS_VFS_ACL_READY(x) (VFS_MODULE && VFS_MODULE->x)
#define IS_SYSCALL_ACL_READY(x) (SYSCALL_MODULE && SYSCALL_MODULE->x)
#define DO_VFS(x, ...) VFS_MODULE->x(__VA_ARGS__)
#define DO_SYSCALL(x, ...) SYSCALL_MODULE->x(__VA_ARGS__)

struct synoacl_vfs_operations *VFS_MODULE = NULL;
struct synoacl_syscall_operations *SYSCALL_MODULE = NULL;

int SYNOACLModuleStatusGet(const char *szModName)
{
	int st = -1;
	struct module *mod = NULL;

	mutex_lock(&module_mutex);

	if (NULL == (mod = find_module(szModName))){
		goto Err;
	}

	st = mod->state;
Err:
	mutex_unlock(&module_mutex);

	return st;
}
EXPORT_SYMBOL(SYNOACLModuleStatusGet);

void UseACLModule(const char *szModName, int isGet)
{
	struct module *mod = NULL;

	mutex_lock(&module_mutex);

	if (NULL == (mod = find_module(szModName))){
		printk("synoacl module [%s] is not loaded \n", szModName);
		goto Err;
	}

	if (isGet) {
		try_module_get(mod);
	} else {
		module_put(mod);
	}
Err:
	mutex_unlock(&module_mutex);
}
EXPORT_SYMBOL(UseACLModule);

/* --------------- Register Function ---------------- */
int synoacl_vfs_register(struct synoacl_vfs_operations *pvfs, struct synoacl_syscall_operations *psys)
{
	if (!pvfs || !psys) {
		return -1;
	}

	VFS_MODULE = pvfs;
	SYSCALL_MODULE = psys;

	return 0;
}
EXPORT_SYMBOL(synoacl_vfs_register);

void synoacl_vfs_unregister(void)
{
	VFS_MODULE = NULL;
	SYSCALL_MODULE = NULL;
}
EXPORT_SYMBOL(synoacl_vfs_unregister);

/* --------------- VFS API ---------------- */
int synoacl_mod_archive_change_ok(struct dentry *d, unsigned int cmd, int tag, int mask)
{
	if (IS_VFS_ACL_READY(archive_change_ok)) {
		return DO_VFS(archive_change_ok, d, cmd, tag, mask);
	}
	return 0; //is settable
}
EXPORT_SYMBOL(synoacl_mod_archive_change_ok);

int synoacl_mod_may_delete(struct dentry *d, struct inode *dir)
{
	if (IS_VFS_ACL_READY(syno_acl_may_delete)) {
		return DO_VFS(syno_acl_may_delete, d, dir, 1);
	}
	return inode_permission(dir, MAY_WRITE | MAY_EXEC);
}
EXPORT_SYMBOL(synoacl_mod_may_delete);

int synoacl_mod_setattr_post(struct dentry *dentry, struct iattr *attr)
{
	if (IS_VFS_ACL_READY(syno_acl_setattr_post)) {
		return DO_VFS(syno_acl_setattr_post, dentry, attr);
	}
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(synoacl_mod_setattr_post);

int synoacl_mod_inode_change_ok(struct dentry *d, struct iattr *attr)
{
	if (IS_VFS_ACL_READY(syno_inode_change_ok)) {
		return DO_VFS(syno_inode_change_ok, d, attr);
	}
	return inode_change_ok(d->d_inode, attr);
}
EXPORT_SYMBOL(synoacl_mod_inode_change_ok);

void synoacl_mod_to_mode(struct dentry *d, struct kstat *stat)
{
	if (IS_VFS_ACL_READY(syno_acl_to_mode)) {
		DO_VFS(syno_acl_to_mode, d, stat);
	}
}
EXPORT_SYMBOL(synoacl_mod_to_mode);

int synoacl_mod_access(struct dentry *d, int mask)
{
	if (IS_VFS_ACL_READY(syno_acl_access)) {
		return DO_VFS(syno_acl_access, d, mask);
	}
	return inode_permission(d->d_inode, mask);
}
EXPORT_SYMBOL(synoacl_mod_access);

int synoacl_mod_exec_permission(struct dentry *d)
{
	if (IS_VFS_ACL_READY(syno_acl_exec_permission)) {
		return DO_VFS(syno_acl_exec_permission, d);
	}
	return 0;
}
EXPORT_SYMBOL(synoacl_mod_exec_permission);

int synoacl_mod_permission(struct dentry *d, int mask)
{
	if (IS_VFS_ACL_READY(syno_acl_permission)) {
		return DO_VFS(syno_acl_permission, d, mask);
	}
	return 0;
}
EXPORT_SYMBOL(synoacl_mod_permission);

int synoacl_mod_get_acl_xattr(struct dentry *d, int cmd, void *value, size_t size)
{
	if (IS_VFS_ACL_READY(syno_acl_xattr_get)) {
		return DO_VFS(syno_acl_xattr_get, d, cmd, value, size);
	}
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(synoacl_mod_get_acl_xattr);

int synoacl_mod_init_acl(struct dentry *dentry, struct inode *inode)
{
	if (IS_VFS_ACL_READY(syno_acl_init)) {
		return DO_VFS(syno_acl_init, dentry, inode);
	}
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(synoacl_mod_init_acl);

SYSCALL_DEFINE2(SYNOACLCheckPerm, const char __user *, szPath, int , mask)
{
	if (IS_SYSCALL_ACL_READY(check_perm)) {
		return DO_SYSCALL(check_perm, szPath, mask);
	}
	return -EOPNOTSUPP;
}

SYSCALL_DEFINE3(SYNOACLIsSupport, const char __user *, szPath, int , fd, int , tag)
{
	if (IS_SYSCALL_ACL_READY(is_acl_support)) {
		return DO_SYSCALL(is_acl_support, szPath, fd, tag);
	}
	return -EOPNOTSUPP;
}

SYSCALL_DEFINE2(SYNOACLGetPerm, const char __user *, szPath, int __user *, pOutPerm)
{
	if (IS_SYSCALL_ACL_READY(get_perm)) {
		return DO_SYSCALL(get_perm, szPath, pOutPerm);
	}
	return -EOPNOTSUPP;
}
