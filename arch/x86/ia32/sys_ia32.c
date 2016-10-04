/*
 * sys_ia32.c: Conversion between 32bit and 64bit native syscalls. Based on
 *             sys_sparc32
 *
 * Copyright (C) 2000		VA Linux Co
 * Copyright (C) 2000		Don Dugger <n0ano@valinux.com>
 * Copyright (C) 1999		Arun Sharma <arun.sharma@intel.com>
 * Copyright (C) 1997,1998	Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 1997		David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 2000		Hewlett-Packard Co.
 * Copyright (C) 2000		David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 2000,2001,2002	Andi Kleen, SuSE Labs (x86-64 port)
 *
 * These routines maintain argument size conversion between 32bit and 64bit
 * environment. In 2.5 most of this should be moved to a generic directory.
 *
 * This file assumes that there is a hole at the end of user address space.
 *
 * Some of the functions are LE specific currently. These are
 * hopefully all marked.  This should be fixed.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/times.h>
#include <linux/utsname.h>
#include <linux/mm.h>
#include <linux/uio.h>
#include <linux/poll.h>
#include <linux/personality.h>
#include <linux/stat.h>
#include <linux/rwsem.h>
#include <linux/compat.h>
#include <linux/vfs.h>
#include <linux/ptrace.h>
#include <linux/highuid.h>
#include <linux/sysctl.h>
#include <linux/slab.h>
#include <asm/mman.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/atomic.h>
#include <asm/vgtod.h>
#include <asm/sys_ia32.h>

#define AA(__x)		((unsigned long)(__x))

asmlinkage long sys32_truncate64(const char __user *filename,
				 unsigned long offset_low,
				 unsigned long offset_high)
{
       return sys_truncate(filename, ((loff_t) offset_high << 32) | offset_low);
}

asmlinkage long sys32_ftruncate64(unsigned int fd, unsigned long offset_low,
				  unsigned long offset_high)
{
       return sys_ftruncate(fd, ((loff_t) offset_high << 32) | offset_low);
}

/*
 * Another set for IA32/LFS -- x86_64 struct stat is different due to
 * support for 64bit inode numbers.
 */
static int cp_stat64(struct stat64 __user *ubuf, struct kstat *stat)
{
	typeof(ubuf->st_uid) uid = 0;
	typeof(ubuf->st_gid) gid = 0;
	SET_UID(uid, from_kuid_munged(current_user_ns(), stat->uid));
	SET_GID(gid, from_kgid_munged(current_user_ns(), stat->gid));
	if (!access_ok(VERIFY_WRITE, ubuf, sizeof(struct stat64)) ||
	    __put_user(huge_encode_dev(stat->dev), &ubuf->st_dev) ||
	    __put_user(stat->ino, &ubuf->__st_ino) ||
	    __put_user(stat->ino, &ubuf->st_ino) ||
	    __put_user(stat->mode, &ubuf->st_mode) ||
	    __put_user(stat->nlink, &ubuf->st_nlink) ||
	    __put_user(uid, &ubuf->st_uid) ||
	    __put_user(gid, &ubuf->st_gid) ||
	    __put_user(huge_encode_dev(stat->rdev), &ubuf->st_rdev) ||
	    __put_user(stat->size, &ubuf->st_size) ||
	    __put_user(stat->atime.tv_sec, &ubuf->st_atime) ||
	    __put_user(stat->atime.tv_nsec, &ubuf->st_atime_nsec) ||
	    __put_user(stat->mtime.tv_sec, &ubuf->st_mtime) ||
	    __put_user(stat->mtime.tv_nsec, &ubuf->st_mtime_nsec) ||
	    __put_user(stat->ctime.tv_sec, &ubuf->st_ctime) ||
	    __put_user(stat->ctime.tv_nsec, &ubuf->st_ctime_nsec) ||
	    __put_user(stat->blksize, &ubuf->st_blksize) ||
	    __put_user(stat->blocks, &ubuf->st_blocks))
		return -EFAULT;
	return 0;
}

#ifdef CONFIG_SYNO_DEBUG_FLAG
#include <linux/synolib.h>
extern int syno_hibernation_log_level;
#endif /* CONFIG_SYNO_DEBUG_FLAG */

#ifdef CONFIG_SYNO_FS_CASELESS_STAT
extern int __SYNOCaselessStat(char __user * filename, int no_follow_link, struct kstat *stat, int *last_component, int flags);
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

#ifdef CONFIG_SYNO_SYSTEM_CALL
asmlinkage long sys32_SYNOCaselessStat64(char __user *filename, struct stat64 __user *statbuf)
{
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	int last_component = 0;
	long error = -1;
	struct kstat stat;

	error =  __SYNOCaselessStat(filename, 0, &stat, &last_component, 0);
	if (!error) {
		error = cp_stat64(statbuf, &stat);
	}

	return error;
#else
	return -EOPNOTSUPP;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
}

asmlinkage long sys32_SYNOCaselessLStat64(char __user *filename, struct stat64 __user *statbuf)
{
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
	int last_component = 0;
	long error = -1;
	struct kstat stat;

	error =  __SYNOCaselessStat(filename, 1, &stat, &last_component, 0);
	if (!error) {
		error = cp_stat64(statbuf, &stat);
	}

	return error;
#else
	return -EOPNOTSUPP;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
}
#endif /* CONFIG_SYNO_SYSTEM_CALL */

#ifdef CONFIG_SYNO_FS_STAT

#include <linux/namei.h>

extern int syno_vfs_fstat(unsigned int fd, struct kstat *stat, int stat_flags);
extern int syno_vfs_fstatat(const char __user *name, struct kstat *stat, int lookup_flags, int stat_flags);

static int SYNOStat64CopyToUser(struct kstat *pKst, unsigned int flags, struct SYNOSTAT64 __user * pSt)
{
	int error = -EFAULT;

	if (flags & SYNOST_STAT) {
		error = cp_stat64(&pSt->st, pKst);
		if (error) {
			goto Out;
		}
	}
#ifdef CONFIG_SYNO_FS_ARCHIVE_BIT
	if (flags & SYNOST_ARCHIVE_BIT) {
		if (__put_user(pKst->syno_archive_bit, &pSt->ext.archive_bit)){
			goto Out;
		}
	}
#endif /* CONFIG_SYNO_FS_ARCHIVE_BIT */

#ifdef CONFIG_SYNO_FS_ARCHIVE_VERSION
	if (flags & SYNOST_ARCHIVE_VER) {
		if (__put_user(pKst->syno_archive_version, &pSt->ext.archive_version)){
			goto Out;
		}
	}
#endif /* CONFIG_SYNO_FS_ARCHIVE_VERSION */

#ifdef CONFIG_SYNO_FS_CREATE_TIME
	if (flags & SYNOST_CREATE_TIME) {
		if (__put_user(pKst->syno_create_time.tv_sec, &pSt->ext.create_time.tv_sec)){
			goto Out;
		}
		if (__put_user(pKst->syno_create_time.tv_nsec, &pSt->ext.create_time.tv_nsec)){
			goto Out;
		}
	}
#endif /* CONFIG_SYNO_FS_CREATE_TIME */
	error = 0;
Out:
	return error;
}

static long do_SYNOStat64(char __user * filename, int no_follow_link, unsigned int flags, struct SYNOSTAT64 __user * pSt)
{
	long error = -EINVAL;
	struct kstat kst;

	if (flags & SYNOST_IS_CASELESS) {
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
		int last_component = 0;
		error = __SYNOCaselessStat(filename, no_follow_link, &kst, &last_component, flags);
		if (-ENOENT == error) {
			if (__put_user(last_component, &pSt->ext.last_component)){
				goto Out;
			}
		}
#else
		error = -EOPNOTSUPP;
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */
	} else if (no_follow_link) {
		error = syno_vfs_fstatat(filename, &kst, 0, flags);
	} else {
#ifdef CONFIG_SYNO_DEBUG_FLAG
			if(syno_hibernation_log_level > 0) {
				syno_do_hibernation_filename_log(filename);
			}
#endif /* CONFIG_SYNO_DEBUG_FLAG */
		error = syno_vfs_fstatat(filename, &kst, LOOKUP_FOLLOW, flags);
	}

	if (error) {
		goto Out;
	}

	error = SYNOStat64CopyToUser(&kst, flags, pSt);
Out:
	return error;
}

asmlinkage long sys32_SYNOStat64(char __user * filename, unsigned int flags, struct SYNOSTAT64 __user * pSt)
{
	return do_SYNOStat64(filename, 0, flags, pSt);
}

asmlinkage long sys32_SYNOFStat64(unsigned int fd, unsigned int flags, struct SYNOSTAT64 __user * pSt)
{
	int error;
	struct kstat kst;

	error = syno_vfs_fstat(fd, &kst, flags);
	if (!error) {
		error = SYNOStat64CopyToUser(&kst, flags, pSt);
	}
	return error;
}

asmlinkage long sys32_SYNOLStat64(char __user * filename, unsigned int flags, struct SYNOSTAT64 __user * pSt)
{
	return do_SYNOStat64(filename, 1, flags, pSt);
}
#endif /* CONFIG_SYNO_FS_STAT */

asmlinkage long sys32_stat64(const char __user *filename,
			     struct stat64 __user *statbuf)
{
	struct kstat stat;
	int ret = vfs_stat(filename, &stat);

	if (!ret)
		ret = cp_stat64(statbuf, &stat);
	return ret;
}

asmlinkage long sys32_lstat64(const char __user *filename,
			      struct stat64 __user *statbuf)
{
	struct kstat stat;
	int ret = vfs_lstat(filename, &stat);
	if (!ret)
		ret = cp_stat64(statbuf, &stat);
	return ret;
}

asmlinkage long sys32_fstat64(unsigned int fd, struct stat64 __user *statbuf)
{
	struct kstat stat;
	int ret = vfs_fstat(fd, &stat);
	if (!ret)
		ret = cp_stat64(statbuf, &stat);
	return ret;
}

asmlinkage long sys32_fstatat(unsigned int dfd, const char __user *filename,
			      struct stat64 __user *statbuf, int flag)
{
	struct kstat stat;
	int error;

	error = vfs_fstatat(dfd, filename, &stat, flag);
	if (error)
		return error;
	return cp_stat64(statbuf, &stat);
}

/*
 * Linux/i386 didn't use to be able to handle more than
 * 4 system call parameters, so these system calls used a memory
 * block for parameter passing..
 */

struct mmap_arg_struct32 {
	unsigned int addr;
	unsigned int len;
	unsigned int prot;
	unsigned int flags;
	unsigned int fd;
	unsigned int offset;
};

asmlinkage long sys32_mmap(struct mmap_arg_struct32 __user *arg)
{
	struct mmap_arg_struct32 a;

	if (copy_from_user(&a, arg, sizeof(a)))
		return -EFAULT;

	if (a.offset & ~PAGE_MASK)
		return -EINVAL;

	return sys_mmap_pgoff(a.addr, a.len, a.prot, a.flags, a.fd,
			       a.offset>>PAGE_SHIFT);
}

asmlinkage long sys32_waitpid(compat_pid_t pid, unsigned int __user *stat_addr,
			      int options)
{
	return compat_sys_wait4(pid, stat_addr, options, NULL);
}

/* warning: next two assume little endian */
asmlinkage long sys32_pread(unsigned int fd, char __user *ubuf, u32 count,
			    u32 poslo, u32 poshi)
{
	return sys_pread64(fd, ubuf, count,
			 ((loff_t)AA(poshi) << 32) | AA(poslo));
}

asmlinkage long sys32_pwrite(unsigned int fd, const char __user *ubuf,
			     u32 count, u32 poslo, u32 poshi)
{
	return sys_pwrite64(fd, ubuf, count,
			  ((loff_t)AA(poshi) << 32) | AA(poslo));
}

/*
 * Some system calls that need sign extended arguments. This could be
 * done by a generic wrapper.
 */
long sys32_fadvise64_64(int fd, __u32 offset_low, __u32 offset_high,
			__u32 len_low, __u32 len_high, int advice)
{
	return sys_fadvise64_64(fd,
			       (((u64)offset_high)<<32) | offset_low,
			       (((u64)len_high)<<32) | len_low,
				advice);
}

long sys32_vm86_warning(void)
{
	struct task_struct *me = current;
	static char lastcomm[sizeof(me->comm)];

	if (strncmp(lastcomm, me->comm, sizeof(lastcomm))) {
		compat_printk(KERN_INFO
			      "%s: vm86 mode not supported on 64 bit kernel\n",
			      me->comm);
		strncpy(lastcomm, me->comm, sizeof(lastcomm));
	}
	return -ENOSYS;
}

asmlinkage ssize_t sys32_readahead(int fd, unsigned off_lo, unsigned off_hi,
				   size_t count)
{
	return sys_readahead(fd, ((u64)off_hi << 32) | off_lo, count);
}

asmlinkage long sys32_sync_file_range(int fd, unsigned off_low, unsigned off_hi,
				      unsigned n_low, unsigned n_hi,  int flags)
{
	return sys_sync_file_range(fd,
				   ((u64)off_hi << 32) | off_low,
				   ((u64)n_hi << 32) | n_low, flags);
}

asmlinkage long sys32_fadvise64(int fd, unsigned offset_lo, unsigned offset_hi,
				size_t len, int advice)
{
	return sys_fadvise64_64(fd, ((u64)offset_hi << 32) | offset_lo,
				len, advice);
}

asmlinkage long sys32_fallocate(int fd, int mode, unsigned offset_lo,
				unsigned offset_hi, unsigned len_lo,
				unsigned len_hi)
{
	return sys_fallocate(fd, mode, ((u64)offset_hi << 32) | offset_lo,
			     ((u64)len_hi << 32) | len_lo);
}
