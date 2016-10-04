/*
 *  arch/arm/include/asm/unistd.h
 *
 *  Copyright (C) 2001-2005 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Please forward _all_ changes to this file to rmk@arm.linux.org.uk,
 * no matter what the change is.  Thanks!
 */
#ifndef __ASM_ARM_UNISTD_H
#define __ASM_ARM_UNISTD_H

#include <uapi/asm/unistd.h>

#ifdef CONFIG_SYNO_SYSTEM_CALL
#include <asm-generic/bitsperlong.h>
#define __NR_syscalls  (380 + 48)
#else /* CONFIG_SYNO_SYSTEM_CALL */
#define __NR_syscalls  (380)
#endif /* CONFIG_SYNO_SYSTEM_CALL */

#define __ARM_NR_cmpxchg		(__ARM_NR_BASE+0x00fff0)

#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SYS_GETHOSTNAME
#define __ARCH_WANT_SYS_PAUSE
#define __ARCH_WANT_SYS_GETPGRP
#define __ARCH_WANT_SYS_LLSEEK
#define __ARCH_WANT_SYS_NICE
#define __ARCH_WANT_SYS_SIGPENDING
#define __ARCH_WANT_SYS_SIGPROCMASK
#define __ARCH_WANT_SYS_OLD_MMAP
#define __ARCH_WANT_SYS_OLD_SELECT

#if !defined(CONFIG_AEABI) || defined(CONFIG_OABI_COMPAT)
#define __ARCH_WANT_SYS_TIME
#define __ARCH_WANT_SYS_IPC
#define __ARCH_WANT_SYS_OLDUMOUNT
#define __ARCH_WANT_SYS_ALARM
#define __ARCH_WANT_SYS_UTIME
#define __ARCH_WANT_SYS_OLD_GETRLIMIT
#define __ARCH_WANT_OLD_READDIR
#define __ARCH_WANT_SYS_SOCKETCALL
#endif
#define __ARCH_WANT_SYS_FORK
#define __ARCH_WANT_SYS_VFORK
#define __ARCH_WANT_SYS_CLONE

/*
 * Unimplemented (or alternatively implemented) syscalls
 */
#define __IGNORE_fadvise64_64
#define __IGNORE_migrate_pages
#define __IGNORE_kcmp

#ifdef CONFIG_SYNO_SYSTEM_CALL
#ifdef CONFIG_SYNO_FS_CASELESS_STAT
#if BITS_PER_LONG == 32
#define __IGNORE_SYNOCaselessStat
#define __IGNORE_SYNOCaselessLStat
#endif /* BITS_PER_LONG == 32 */
#endif /* CONFIG_SYNO_FS_CASELESS_STAT */

#ifdef CONFIG_SYNO_FS_STAT
#if BITS_PER_LONG == 32
#define __IGNORE_SYNOStat
#define __IGNORE_SYNOFStat
#define __IGNORE_SYNOLStat
#endif /* BITS_PER_LONG == 32 */
#endif /* CONFIG_SYNO_FS_STAT */
#endif /* CONFIG_SYNO_SYSTEM_CALL */

#endif /* __ASM_ARM_UNISTD_H */
