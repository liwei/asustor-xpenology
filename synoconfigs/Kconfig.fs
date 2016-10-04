menu "File Systems"

menu "Basic"

config SYNO_FS_STAT
	bool "SYNOStat"
	default y
	help
	  <DSM> #39836
	  Support SYNOStat

config SYNO_FS_XATTR
	bool "syno xattr"
	default y
	help
	  <DSM> #12362
	  Support syno extended attribute.

config SYNO_FS_ARCHIVE_BIT
	bool "syno archive bit"
	default y
	help
	  <DSM> #44005
	  Support syno archive bit

config SYNO_FS_ARCHIVE_VERSION
	bool "syno archive version"
	default y
	help
	  <DSM> #12362, #39525
	  Add syno archive version for backup. Each inode and
	  superblock all keep a version info. Once a file or directory
	  is modified, inode's version will be set to super block's
	  version plus 1. Super block's version could be increased by
	  user program when complete a backup. With each inode's
	  version comparing to backup's version, we could quickly find
	  out modified files. May refer to SYNO_ARCHIVE_BIT.
	  Archive version shall supported by syno xattr.

config SYNO_FS_CREATE_TIME
	bool "syno create time"
	default y
	help
	  Add syno create time for files/directories, ext3 doesn't have create time attribute,
	  we find a reserved field to store the create time. Ext4 has this field, so we just load it into
	  syno_create_time and store it back to ext4 crtime. Btrfs syno create time is stored in xattr.
	  previous naming: SYNO_CREATE_TIME

config SYNO_FS_EXPORT_SYMBOL_FALLOCATE
	bool "Export symbol: do_fallocate"
	default y
	help
	  <DSM> #36482
	  Export symbol "do_fallocate" for UNMAP command.

config SYNO_FS_WINACL
	bool ""
	default y
	depends on SYNO_FS_ARCHIVE_BIT
	help
	  <DSM> #14752
	  Support WIN ACL

config SYNO_FS_SKIP_RO_NEW_INODE_WARNING
	bool "Skip I_NEW WARN_ON when read-write on crached raid"
	default y
	help
	  <DSM> #35583
	  It will show error message instead of call trace if file system
	  keep read-write on crashed raid.
	  previous naming: SYNO_FS_SKIP_RDONLY_NEW_INODE_WARNING

config SYNO_FS_RECVFILE
	bool "Support system call recvfile"
	default y
	help
	  Add syscall recvfile() to speed up receive files from network to file system.
	  previous naming: SYNO_RECVFILE

config SYNO_EXT_SKIP_FSCK_REMINDER
	bool "Skip unused fsck remainder warning on ext2/3/4"
	default y
	depends on EXT2_FS || EXT3_FS || EXT4_FS
	help
	  Skip unused fsck remainder warning on ext2/3/4:
	  1. maximal mount count since last fsck reached.
	  2. maximal time interval since last fsck reached.
	  previous naming: SYNO_REMOVE_CHECKTIME_WARNING

config SYNO_FS_CASELESS_STAT
	bool "Support caseless stat in filesystem "
	default y
	help
	  <DSM> #34495, #45945, #39235, #39533, #43821, #46005, #48255
	  Let ext3 and ext4 filesystem be case-insensitive. This modify should
	  sync with Samba and e2fsprogs.

config SYNO_FS_EXPORT_SYMBOL_LOOKUP_HASH
	bool "Export symbol: lookup_hash"
	default y
	help
	  <Linux Kernel Porting - linux-3.10.x> #266
	  Export symbol "lookup_hash" (for lio-4.0 now).
	  previous naming: SYNO_EXPORT_SYMBOL_FOR_LIO

config SYNO_FS_REMOVE_RCU_WALK_PATH
	bool "Remove rcu path walk to prevent deadlock"
	default y
	help
	  <Linux Kernel Porting - linux-3.10.x> #263
	  previous naming: CONFIG_SYNO_REMOVE_RCU_WALK_PATH

config SYNO_FS_NOTIFY
	bool "Support Synotify"
	default y
	depends on FSNOTIFY && ANON_INODES
	help
	  <DSM> #40813
	  Say Y here to enable syno notify support for userspace, including the
	  associated system calls.  Syno notify allows monitoring of both files and
	  directories via a single open fd.  Events are read from the file
	  descriptor, which is also select()- and poll()-able.
	  Syno notify is whole mount point file system event monitor.
	  If unsure, say Y.
	  previous naming: CONFIG_SYNO_NOTIFY

endmenu #Basic

menu "CIFS"
config SYNO_CIFS_REPLACE_NATIVE_OS
	bool "Identify Synology CIFS mount"
	default y
	help
	  DSM#25749 
	  The "Native OS" in SMB packets sent by cifs.mount will be
	  replaced with "Linux version 2.6.32". However, we have to
	  identify those packets to export @eaDir for such clients.
	  Therefore we change the Native OS to
          "Synology Linux version 2.6.32" for identification.

config SYNO_CIFS_TCON_RECONNECT_CODEPAGE_UTF8
	bool "Cifs with UTF8 code page"
	default y
	help
	  DSM#29392
	  Force tree reconnect iocharset=utf8 to fix failed reconnecting
	  to remote cifs share with non-ascii share folder names

config SYNO_CIFS_INIT_NLINK
	bool "Initialize fattr with cf_nlink=1 in cifs_dir_info_to_fattr"
	default y
	help
	  DSM#29931
	  Initialize fattr with "cf_nlink = 1" in cifs_dir_info_to_fattr.
	  Without this, st_nlink for directory will be zero in cifs filesystem.
  
config SYNO_CIFS_SPECIAL_CHAR_CONVER
	bool "CIFS convert special char for MAC"
	default y
	help
	  DSM#46499, #26543, #26544
	  When upload a file with icon from mac to ds, and the destination on
	  ds is a cifs mount point of another mac share, the upload action fail
	  in mac os 10.6.2, or lose ea in mac os 10.6.8/ The reason is that
	  icon is store in 'icon\r', and '\r' will be conver by mac. Therefore
	  we change '/r' (0x0d) to (0xf00d).

config SYNO_CIFS_MOUNT_CASELESS
	bool "Caseless remote mount"
	default y
	help
	  DSM#43373
	  Disable posix semantics for caseless remote mount

config SYNO_CIFS_NO_SPECIAL_CHAR_LOGON
	bool "STATUS_LOGON_FAILURE when password contains '/'"
	default y
	depends on SYNO_CIFS_SPECIAL_CHAR_CONVER
	help
	  DSM#49684, #49685
	  Linux-3.x uses cifs_strtoUCS to deal with log on, which is different
	  with linux-2.6.x. It causes LOGON_FAILURE when password contains '/'.

config SYNO_CIFS_FORCE_UMOUNT
	bool ""
	default y
	help
	  DSM#65071
	  fix cifs umount -f cause other cifs mount point unreachable

config SYNO_CIFS_FIX_DOUBLE_FREE_INVALID_FILE
	bool "Prevent double free in find_writable_file()"
	default y
	help
	  DSM#72690
	  In cifs find_writable_file(), it may re-use the file handler even if the handler has freed.
	  We re-init the pointer to prevent it. see also:2c0c2a08bed

endmenu #CIFS

menu "FAT"

config SYNO_FAT_DEFAULT_MNT_FLUSH
	bool "Set FAT default mount option 'flush'"
	default y
	depends on FAT_FS
	help
	  It will flush data to disk ASAP. Since user may remove the external device
	  without umount, we use this to mitigate data loss.

config SYNO_FAT_LOAD_DEF_NLS_IF_FAIL
	bool "Try default nls as codepage setting when default codepage cannot be loaded"
	default y
	depends on FAT_FS
	help
	  When we fail to load default codepage, which is assigned by mount option or
	  config FAT_DEFAULT_CODEPAGE, try config NLS_DEFAULT as codepage setting.

endmenu

menu "EXT3"

config SYNO_EXT3_STAT
	bool "Ext3 SYNOStat"
	default y
	depends on SYNO_FS_STAT && EXT3_FS
	help
	  <DSM> #39836
	  Split from SYNO_STAT
	  Support SYNOStat in ext3

config SYNO_EXT3_XATTR
	bool "Ext3 syno xattr"
	default y
	depends on SYNO_FS_XATTR && EXT3_FS
	help
	  <DSM> #12362
	  Support syno extended attribute in ext3.

config SYNO_EXT3_ARCHIVE_BIT
	bool "Ext3 syno archive bit"
	default y
	depends on SYNO_FS_ARCHIVE_BIT && EXT3_FS
	help
	  <DSM>
	  Support syno archive bit in ext3

config SYNO_EXT3_ARCHIVE_VERSION
	bool "Ext3 syno archive version"
	default y
	depends on SYNO_FS_ARCHIVE_VERSION && EXT3_FS && SYNO_EXT3_XATTR
	help
	  <DSM> #12362, #39525
	  Support syno archive version in ext3

config SYNO_EXT3_CREATE_TIME
	bool "Ext3 syno create time"
	default y
	depends on SYNO_FS_CREATE_TIME && EXT3_FS
	help
	  Support syno create time in ext3
	  Split from SYNO_CREATE_TIME

config SYNO_EXT3_CASELESS_STAT
	bool "Support caseless stat in ext3"
	default y
	depends on SYNO_FS_CASELESS_STAT && EXT3_FS
	help
	  Support caseless stat in ext3

endmenu #EXT3

menu "EXT4"

config SYNO_EXT4_LAZYINIT_INFO
	bool "Export lazyinit progress to sysfs"
	default y
	depends on EXT4_FS
	help
	  <DSM> #42089
	  Export lazyinit progress to sysfs

config SYNO_EXT4_LAZYINIT_DYNAMIC_SPEED
	bool "Adjust lazyinit speed dynamically"
	default y
	depends on EXT4_FS
	help
	  <DSM> #43366
	  The interval of time Kernel take in lazyinit process depends on the time first group take to initiate.
	  Once the time is determined, it won't change during the whole lazyinit process.
	  If this config is set, kernel will adjust interval whenever fixed amount of groups have been processed.

config SYNO_EXT4_LAZYINIT_WAIT_MULT
	int "Number of lazyinit wait multiplier"
	default 2
	depends on EXT4_FS
	help
	  <DSM> #44990
	  Adjust default lazyinit wait multiplier. Higher value will cause lazyinit
	  process to be scheduled on a lower freauency.
	  Previous Naming: SYNO_EXT4_LAZYINIT_DEFAULT_MULTIPLY

config SYNO_EXT4_STAT
	bool "Ext4 SYNOStat"
	default y
	depends on SYNO_FS_STAT && EXT4_FS
	help
	  <DSM> #39836
	  Split from SYNO_STAT
	  Support SYNOStat in ext4

config SYNO_EXT4_XATTR
	bool "Ext4 syno xattr"
	default y
	depends on SYNO_FS_XATTR && EXT4_FS
	help
	  <DSM> #12362
	  Support syno extended attribute in ext4.

config SYNO_EXT4_ARCHIVE_BIT
	bool "Ext4 syno archive bit"
	default y
	depends on SYNO_FS_ARCHIVE_BIT && EXT4_FS
	help
	  <DSM>
	  Support syno archive bit in ext4

config SYNO_EXT4_ARCHIVE_VERSION
	bool "Ext4 syno archive version"
	default y
	depends on SYNO_FS_ARCHIVE_VERSION && EXT4_FS && SYNO_EXT4_XATTR
	help
	  <DSM> #12362, #39525
	  Support syno archive version in ext4

config SYNO_EXT4_ARCHIVE_VERSION_FIX
	bool "Ext4 syno arhchive version fix"
	default y
	depends on SYNO_EXT4_ARCHIVE_VERSION
	help
	  <DSM> #39525
	  During dsm 2508 to 2637 ext4 syno archive version field is placed at wrong place in ext4 superblock.
	  This patch will provide ioctl to get, set and clear archive version at wrong place, so that synofstool could
	  dump it to the correct archive version place and clear the wrong one.
	  Previous naming: SYNO_FIX_ARCHIVE_VERSION

config SYNO_EXT4_CREATE_TIME
	bool "Ext4 syno create time"
	default y
	depends on SYNO_FS_CREATE_TIME && EXT4_FS
	help
	  Support syno create time in ext4
	  Split from SYNO_CREATE_TIME

config SYNO_EXT4_CREATE_TIME_BIG_ENDIAN_SWAP
	bool "Ext4 syno create time swap for big endian"
	default y if SYNO_QORIQ || SYNO_MPC8533
	depends on SYNO_EXT4_CREATE_TIME
	help
	  <DSM> #45668
	  This option should be enabled on big endian architecture for compatible with old disk migration.
	  In ext4_do_update_inode, we do not translate endian, this would result in create time error in big endian model.
	  This bug happens from first version of syno create time to DSM 3719. This config will change the way to load create time(don't do endian translate)
	  if this is big-endian model and fs is created before 3719.

config SYNO_EXT4_DEFAULT_MNTOPT_JOURNAL_CKSUM
	bool "Ext4 set default mount option journal_checksum"
	default y
	depends on EXT4_FS
	help
	  <DSM> #14281
	  Set default mount option journal_checksum. This also
	  bring a mount option 'nojournal_checksum' which can
	  unset it.
	  previous naming: SYNO_EXT4_DEFAULT_JOURNAL_CHECKSUM

config SYNO_EXT4_DEFAULT_MNTOPT_NOBARRIER
	bool "Ext4 set default mount option barrier=0"
	default y
	depends on EXT4_FS
	help
	  <DSM> #8406
	  Set default mount option barrier=0 to gain performance.
	  For root fs, refer to CONFIG_SYNO_EXT4_DEFAULT_MNTOPT_BARRIER_ROOTFS.
	  previous naming: SYNO_IGNORE_MNT_OPT

config SYNO_EXT4_DEFAULT_MNTOPT_BARRIER_ROOTFS
	bool "Ext4 set default mount option barrier=1 on root fs"
	default y
	depends on EXT4_FS
	help
	  <DSM> #38963
	  Set default mount option barrier=1 on root fs.
	  root fs = /dev/md0.
	  previous naming: SYNO_ROOTFS_ENABLE_BARRIER

config SYNO_EXT4_ERROR_REPORT
	bool "Enable ext4 error report mechanism"
	default y
	depends on EXT4_FS
	help
	  <DSM> #40639
	  This enable the error report mechanism of ext4.
	  When an error was found, it will call a hook function
	  defined in synobios, which will send an event to scemd,
	  and eventually show warning message in UI and suggest
	  user to do fsck.
	  previous naming: SYNO_EXT4_ERROR_FS_REPORT

config SYNO_EXT4_INODE_NUM_OVERFLOW_FIX
	bool "Fix ext4 inode number overflow problem on large volume (>64TB)"
	default y
	depends on EXT4_FS && 64BIT
	help
	  <DSM> #40578
	  With current config of inode_ratio = 16384, on a volume
	  larger than 64TB, we have a chance to get a group number
	  so that (group number x inodes per group) will overflow
	  the 32bits inode number field. Fix it.
	  previous naming: SYNO_FIX_DIRENT_INODE_NUMBER_OVERFLOW

config SYNO_EXT4_CASELESS_STAT
	bool "Support caseless stat in ext4"
	default y
	depends on SYNO_FS_CASELESS_STAT && EXT4_FS
	help
	  Support caseless stat in ext4

config SYNO_EXT4_SKIP_ADD_RESERVED_BLOCKS
	bool "Don't update reserved blocks on resize"
	default y
	depends on EXT4_FS
	help
	  <DSM> #44227
	  Keep reserved block a constant value, don't update on resize.

config SYNO_EXT4_META_BG_BACKUP_DESC_FIX
	bool "Fix update correct backup descriptor in meta_bg"
	default y
	depends on EXT4_FS
	help
	  <DSM> #40823
	  Meta_bg backup group descriptor block in its own meta_bg group. Before this fix,
	  it will update group #1 and #63. It should update only second and last group of its own
	  meta_bg group.

config SYNO_EXT4_SKIP_JOURNAL_SYMLINK
	bool "Use writeback mode instead of jounal mode when doing ext4 symlink"
	default y
	depends on EXT4_FS
	help
	  <DSM> #47657
	  Use writeback mode instead of jounal mode when doing ext4 symlink. Prevent
	  journal replay of symlink from corrupting data blocks when the volume is not
	  properly umount.
	  previous naming: SYNO_FS_EXT4_SKIP_JOURNAL_SYMLINK

config SYNO_EXT4_OLDALLOC
	bool "Provide oldalloc as hidden mount option"
	default y
	depends on EXT4_FS
	help
	  <DSM> #8459, #9283, #11447, #15830
	  Provide oldalloc as a hidden mount option to stabilize performance in a newly created ext4 volume.
	  previous naming: SYNO_RESERVE_OLDALLOC, SYNO_HIDE_OLDALLOC, SYNO_FORCE_GET_AVAILABLE_GROUP

config SYNO_EXT4_MBALLOC_RANDOM
	bool "Provide random multiblock alloc option"
	default y
	depends on EXT4_FS
	help
	  <DSM> #27625
	  Provide random multiblock allocation scheme on stage 0 of finding suitable blocks.
	  This helps to prevent from long searching time after front parts of filesystem utilization is at high water-level
	  due to old alloc algorithm.
	  Previous naming: SYNO_MBALLOC_RANDOM

config SYNO_EXT4_FIX_RESIZE_16TB_IN_32BIT
	bool "Support on-line resize over 16TB on 32-bit platform"
	depends on SYNO_ALPINE || SYNO_ARMADA
	default y
	help
	  32-bit platforms with 4K page size are bounded to UINT32_MAX*4K=16TB volume size.
	  With larger page size, it is possible to use and resize volumes larger than 16TB;
	  but some numeric overflows must be cleaned up first by using 64-bit API.

config SYNO_EXT4_PROTECT_DISKSIZE_WRITE
	bool "Add lock on i_disksize update for writeback race"
	default y
	depends on EXT4_FS
	help
	  <Linux Kernel Porting - linux-3.10.x> #651
	  It is modified from the two patches. If the patches back porting, we should remove this define.
	  90e775b71a ext4: fix lost truncate due to race with writeback
	  622cad1325 ext4: move ext4_update_i_disksize() into mpage_map_and_submit_extent()


config SYNO_EXT4_FORCE_UPDATE_DA_FILE_SIZE
	bool "Force update file size on buffer_delay if the file is growing"
	default y
	depends on EXT4_FS
	help
	  <DSM> #73519
	  When delayed allocation, file size may not update in some reason while file copy stress.
	  We force update file size if the file is growing, even if the buffer is mark delay.


endmenu #EXT4

menu "BTRFS"

config SYNO_BTRFS_PORTING
	bool "Btrfs back porting"
	default y
	help
	  <DSM> #47400
	  Add support for back porting btrfs

config SYNO_BTRFS_STAT
	bool "Btrfs SYNOStat"
	default y
	depends on SYNO_FS_STAT && BTRFS_FS
	help
	  <FS Snapshot> #5
	  Split from SYNO_STAT
	  Support SYNOStat in btrfs

config SYNO_BTRFS_XATTR
	bool "Btrfs syno xattr"
	default y
	depends on SYNO_FS_XATTR && BTRFS_FS
	help
	  <DSM> #48010
	  Support syno extended attribute in btrfs.

config SYNO_BTRFS_ARCHIVE_BIT
	bool "Btrfs syno archive bit"
	default y
	depends on SYNO_FS_ARCHIVE_BIT && BTRFS_FS && SYNO_BTRFS_XATTR
	help
	  <FS Snapshot> #6
	  Support syno archive bit in btrfs

config SYNO_BTRFS_AVOID_SEARCH_EXTENT_STATE_WHILE_EVICT_INODE
	bool "Avoid search extent_state again while evict inode to skip unnecessary split extent_state."
	default y
	help
	  <DSM> #69930
	  Fix deadlock in rm with ecryptfs on btrfs.

config SYNO_BTRFS_PIN_LOG_ON_DELETE_INODE
	bool "Pin tree-log while unlink to prevent deadlock."
	default y
	help
	  <FS Snapshot> #271
	  <DSM> #69252
	  Fix deadlock between btrfs unlink and sync tree log.

config SYNO_BTRFS_PREVENT_FLUSH_STUCK_IN_BALANCE_DIRTY_PAGES
	bool "Skip balance dirty pages in flush-btrfs-X  to prevent deadlock."
	default y
	help
	  <FS Snapshot> #248
	  flush-btrfs-X will stuck in balance_dirty_pages(). It may be fixed in new kernel.
	  We can pass it safely because page will be balance every command.

config SYNO_BTRFS_METADATA_OVERCOMMIT_POLICY
	bool "Change metadata over commit policy to prevent file system crash."
	default y
	help
	  <FS Snapshot> #174
	  Because of btrfs metadata delayed allocation,
	  space_info may keep pinned data too much. It will strictly make overcommit check failed.

config SYNO_BTRFS_FLUSHONCOMMIT_THRESHOLD
	bool "Release btrfs ordered extent to prevent OOM"
	default y
	help
	  <FS Snapshot> #110
	  In btrfs, ordered extent will keep in memory via delayed allocation.
	  It may triger oom if it is keeped too much.

config SYNO_BTRFS_FIX_PAGE_LEAK_WHILE_CLONE_EXTENT_BUFFER
	bool "Fix btrfs memory leak on clone extent buffer."
	default y
	help
	  <FS Snapshot> #171
	  Fix memory leak for btrfs quota group.
	  It will make page leakage while btrfs_clone_extent_buffer().

config SYNO_BTRFS_ADD_MISSING_FINISH_PLUG_FOR_TREE_LOG
	bool "Add missing finish plug for btrfs tree log."
	default y
	help
	  <FS Snapshot> #121
	  In 3.15 patches, 0cb5320747cd177a1962c61bb039d3425698089b.
	  It is missing blk_finish_plug, and make plug mech BUG_ON.

config SYNO_BTRFS_CLONE_CHECK_QUOTA
	bool "Add quota check for IOC_CLONE ioctl command"
	default y
	help
	  <FS Snapshot> #152
	  add quota check for IOC_CLONE ioctl command

config SYNO_BTRFS_FREE_EXTENT_MAPS
	bool "Add a machanisim to drop extent map cache"
	default y
	help
	  <FS Snapshot> #291
	  Add a machanisim to drop extent map cache

config SYNO_BTRFS_CLUSTER_RESERVE
	bool "Reserve meta block to suppress problem of extent tree loop"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #71147
	  Reserve meta block to suppress problem of extent tree loop.

config SYNO_BTRFS_CREATE_TIME
	bool "Add syno create time for btrfs"
	default y
	depends on SYNO_FS_CREATE_TIME && SYNO_BTRFS_XATTR && BTRFS_FS
	help
	  <FS Snapshot> #5
	  Support syno create time for btrfs

config SYNO_BTRFS_RESIZE_QUERY
	bool "Add a dry-run mode in BTRFS_IOC_RESIZE"
	default y
	depends on BTRFS_FS
	help
	  <FS Snapshot> #90
	  Add a dry-run mode in resize ioctl to test if a resize can be applied.

config SYNO_BTRFS_SUBVOLUME_HIDE
	bool "Support subvolume hide flag"
	default y
	depends on BTRFS_FS
	help
	  <FS Snapshot>	#40
	  Add subvolume hide flag. If hide flag of subvolume is set, this
	  subvolume will be skipped from readdir

config SYNO_BTRFS_ARCHIVE_VERSION
	bool "Support syno archive version for btrfs"
	default y
	depends on SYNO_FS_ARCHIVE_VERSION && SYNO_BTRFS_XATTR && BTRFS_FS
	help
	  <FS Snapshot> #7, #252
	  Support syno archive version for btrfs. Store volume archive version on xattr.

config SYNO_BTRFS_COMPAT_IOCTL
	bool "Add some compat ioctl support for btrfs"
	default y
	depends on COMPAT && 64BIT && BTRFS_FS
	help
	  <FS Snapshot> #158
	  Fix FREEZE,THAW and FIEMAP ioctl won't work on 32bit userspace because it will
	  return NOTTY. It should return ENOIOCTLCMD to use default ioctl for these.

config SYNO_BTRFS_SEND
	bool "Add syno btrfs send"
	default y
	depends on BTRFS_FS
	help
	  <FS Snapshot> #264, #265, #267
	  Support syno btrfs send, handle receive uuid and subvolume create time

config SYNO_BTRFS_FIX_ASYNC_DIRECT_IO_CSUM_FAILED
	bool "Disable btrfs async read on direct io."
	default y
	depends on BTRFS_FS
	help
	  <DSM> #71627
	  Disable btrfs async read on direct io to prevent btrfs csum failed.

config SYNO_BTRFS_FIX_NOSPC_DEL_NOUSED_BLOCKGROUP
	bool "Fix nospc problem when delete unused blockgroup"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #75194
	  Fix nospc problem when delete unused blockgroup

config SYNO_BTRFS_SUPPORT_FULLY_CLONE_BETWEEN_CSUM_AND_NOCSUM_DIR
	bool "Fix cp --reflink failed between csum/nocsum"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #71706
	  Btrfs not allow clone file between the folder which is datacow or nodatacow.
	  It prevent file partly checksummed through file clone.
	  In fact, it could be clone if dst is fully clone.
	  see also:0e7b824c

config SYNO_BTRFS_QGROUP_QUERY
	bool "Add ioctl for btrfs qgroup query"
	default y
	depends on BTRFS_FS
	help
	  <FS Snapshot> #163
	  Add ioctl for userspace to query btrfs subvolume quota info.

config SYNO_BTRFS_RENAME_READONLY_SUBVOL
	bool "Fix rename readonly subvol fail"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #71806
	  Fix rename readonly subvol fail.

config SYNO_BTRFS_CORRECT_SPACEINFO_LOCK
	bool "Correct btrfs statfs block group list lock"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #72842
	  Btrfs may deadlock on block allocation and statfs. Because statfs
	  use wrong spinlock to protect block group list.
	  see also:80eb234a

config SYNO_BTRFS_FIX_ALLOC_CHUNK
	bool "Chunk allocation may fail if device tree has hole"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #73192
	  Chunk allocation may fail if device tree has hole

config SYNO_BTRFS_CLEAR_SPACE_FULL
	bool "Clear space full flag after transaction committed"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #73192
	  Clear space full flag after transaction committed

config SYNO_BTRFS_REMOVE_UNUSED_QGROUP
	bool "Remove qgroup item when snapshot got deleted"
	default y
	depends on BTRFS_FS
	help
	  <FS Snapshot> #243
	  Remove qgroup item when snapshot got deleted.

config SYNO_BTRFS_BIG_BLOCK_GROUP
	bool "Use big block group to reduce mount time on big volume"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #75730
	  Use big block group to reduce mount time on big volume. It
	  will be applied only on volumes who have size larger than 1T.

config SYNO_BTRFS_BIG_BLOCK_GROUP
	bool "Use big block group to reduce mount time on big volume"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #75057
	  Use big block group to reduce mount time on big volume. It
	  will be applied only on volumes who have size larger than 1T.

config SYNO_BTRFS_REVERT_WAIT_OR_COMMIT_SELF_TRANS
	bool "Revert commit: wait or commit self transaction"
	default y
	depends on BTRFS_FS
	help
	  <FS Snapshot> #145
	  Open source - Btrfs: just wait or commit our own log sub-transaction
	  will cause btrfs panic everywhere. This config revert that commit.

config SYNO_BTRFS_REVERT_BIO_COUNT_FOR_DEV_REPLACING
	bool "Fix btrfs hang on btrfs_end_io*"
	default y
	depends on BTRFS_FS
	help
	  <FS Snapshot> #149
	  Open source - Btrfs: fix use-after-free in the finishing procedure of the device replace
	  will cause bio_couter inc/dec call trace. This config revert that patch.

config SYNO_BTRFS_REVERT_DELAYED_DELETE_INODE
	bool "Fix dbench hang on delayed_delete_inode"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #69252
	  Open source - Btrfs: introduce the delayed inode ref deletion for the single link inode
	  will cause delay cause release delay inode hang on dbench stress. This config revert
	  this patch.

config SYNO_BTRFS_UMOUNT_ERROR_VOLUME
	bool "Fix umount on a error Btrfs volume"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #76402
	  Fix a infinite loop and a null pointer dereference bug so that umount
	  process will not stuck or killed when unmount a error Btrfs volume.

config SYNO_BTRFS_REMOVE_RAID_CRASH_QGROUP_BUG_ON
	bool "Remove qgroup bug on when raid crashes"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #77718
	  qgroup wil bug_on when raid crash. This patch add additional check on whether
	  filesystem is already readonly. If filesystem is already changed to readonly,
	  skip bug_on.

config SYNO_BTRFS_AVOID_NULL_ACCESS_IN_PENDING_SNAPSHOT
	bool "Avoid NULL pointer dereference at create_pending_snapshots"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #78512
	  During stress, NULL pointer dereference might happen on create_pending_snapshots
	  when trying to delete pending_snapshot from list. If last ioctl of creating snapshot
	  failed at commit transaction before it is deleted from pending snapshot list, the entry
	  would be freed while it was still in the list. This causes call trace on next commit
	  transctioncall to create_pending_snapshots. This patch add a check: If this entry has
	  not been deleted from list right before it is about to free, delete it.

config SYNO_BTRFS_ALLOC_EXTENT_STATE_RETRY
	bool "Retry kmalloc if alloc extent state failed"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #72318
	  Btrfs will trigger BUG_ON if kmalloc failed on alloc extent state. We make it by retry.

config SYNO_BTRFS_METADATA_RESERVE
	bool "Pre-allocate btrfs metadata chunk with metadata_ratio."
	default y
	depends on BTRFS_FS
	help
	  <DSM> #76547
	  Btrfs may be run out of free chunk by data chunk
	  We allocate metadata chunk in ratio if data chunk allocated.

config SYNO_BTRFS_FIX_PAGE_WRITEBACK
	bool "Fix page writeback BUG_ON on release extent buffer code"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #75451
	  Use memory barrier to fix page writeback BUG_ON. We don't know
	  exactly why it works but it passed our stress and the fix has no
	  harm.

config SYNO_BTRFS_REMOVE_RAID_CRASH_QGROUP_BUG_ON
	bool "Remove qgroup bug on when raid crashes"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #77718
	  qgroup wil bug_on when raid crash. This patch add additional check on whether
	  filesystem is already readonly. If filesystem is already changed to readonly,
	  skip bug_on.

config SYNO_BTRFS_AVOID_NULL_ACCESS_IN_PENDING_SNAPSHOT
	bool "Avoid NULL pointer dereference at create_pending_snapshots"
	default y
	depends on BTRFS_FS
	help
	  <DSM> #78512
	  During stress, NULL pointer dereference might happen on create_pending_snapshots
	  when trying to delete pending_snapshot from list. If last ioctl of creating snapshot
	  failed at commit transaction before it is deleted from pending snapshot list, the entry
	  would be freed while it was still in the list. This causes call trace on next commit
	  transctioncall to create_pending_snapshots. This patch add a check: If this entry has
	  not been deleted from list right before it is about to free, delete it.

config SYNO_BTRFS_ADD_LOCK_ON_FLUSH_ORDERED_EXTENT
	bool "Add mutex lock on btrfs ordered extent flush to prevent memory leak."
	default y
	depends on BTRFS_FS
	help
	  <DSM> #78861
	  During dbench stress, it will be memory leak on run_ordered_extent. The function
	  will be called on sync and btrfs transaction commit. The attribute "Inactive(file)"
	  in meminfo will be increase all the time, even we drop all cache. We add a lock to
	  prevent race condition from run_ordered_extent hold the pages forever.

config SYNO_BTRFS_BLOCK_GROUP_HINT_TREE
	bool "Add a block group hint tree to speedup volume mount."
	default y
	depends on BTRFS_FS
	help
	  <DSM> #75730
	  Add a block group hint tree to collect block group items from extent tree. When
	  mounting volume, we can lookup this tree, knowing the keys of following block
	  group items, and readahead theses items to speedup mounting time.

config SYNO_BTRFS_AVOID_TRIM_SYS_CHUNK
	bool "Avoid trim system chunk."
	default y
	depends on BTRFS_FS
	help
	  <DSM> #84616
	  Btrfs trimming doesn't check block reserve, leading ENOSPC if we do chunk item
	  operation while trimming system chunk. Since system chunk is usually small and
	  seldom updates, we avoid trimming system chunk to workarond this problem.

endmenu #BTRFS

menu "ECRYPT"

config SYNO_ECRYPTFS_STAT
	bool "Ecryptfs SYNOStat"
	default y
	depends on SYNO_FS_STAT && ECRYPT_FS
	help
	  <DSM> #39836
	  Split from SYNO_STAT
	  Support SYNOStat in ecryptfs

config SYNO_ECRYPTFS_ARCHIVE_BIT
	bool "Ecryptfs archive bit"
	default y
	depends on SYNO_FS_ARCHIVE_BIT && ECRYPT_FS
	help
	  <DSM>
	  Support syno archive bit in ecryptfs

config SYNO_ECRYPTFS_ARCHIVE_VERSION
	bool "Ecryptfs archive version"
	default y
	depends on SYNO_FS_ARCHIVE_VERSION && ECRYPT_FS
	help
	  <DSM> #12362, #39525
	  Support syno archive version in ecryptfs

config SYNO_ECRYPTFS_CREATE_TIME
	bool "Ecryptfs syno create time"
	default y
	depends on SYNO_FS_CREATE_TIME && ECRYPT_FS
	help
	  Support syno create time in ecryptfs
	  Split from SYNO_CREATE_TIME

config SYNO_ECRYPTFS_SKIP_EDQUOT_WARNING
	bool "Ecryptfs skip EDQUOT, ENOSPC warning log"
	default y
	depends on ECRYPT_FS
	help
	  <DSM> #14250, #35332, #62672
	  EcryptFs will print a lots of error message if the lower has error EDQUOT or ENOSPC.

config SYNO_ECRYPTFS_SKIP_AUTH_WARNING
	bool "Ecryptfs add ratelimit to auth tok not found error message"
	default y
	depends on ECRYPT_FS
	help
	  <DSM> #38020
	  When encryption share key is lost and user use wrong password to mount encryption share,
	  logs will be overwhelmed by lots of encryption errro messages. This config add printk_ratelimit
	  to those error log.

config SYNO_ECRYPTFS_REMOVE_TRUNCATE_WRITE
	bool "Speed up ecryptfs truncate by skipping zeros write"
	default y
	depends on ECRYPT_FS
	help
	  When truncate, only write file metadata to lower, don't write file body. Since we'll write it later, there's
	  no need to write ecrypted zeros in advance. This will speedup truncate.

config SYNO_ECRYPTFS_CHECK_SYMLINK_LENGTH
	bool "Check ecryptfs symlink target length after encryption"
	default y
	depends on ECRYPT_FS
	help
	  ecryptfs should check symlink target length after encryption. Or the target may be cut and decrypt failed.

config SYNO_ECRYPTFS_FILENAME_SYSCALL
	bool "System calls to get encrypt or decrypt filename"
	default y
	depends on ECRYPT_FS
	help
	  Add two syscalls for encryptfs. They could convert file name from cipher to plaintext, and from plaintext to cipher.
	  From plaintext to cipher, it needs only plaintext full pathname and return encrypt file name.  From cipher to plaintext,
	  it needs both full encrypted pathname (not including plaintext part) and ecryptfs mount point path, then it will return
	  plaintext full pathname. This function should be called at ecryptfs mounted.

config SYNO_ECRYPTFS_OCF
	bool "enable ocf framework"
	default n
	depends on ECRYPT_FS && OCF_OCF
	help
	  <DSM> No Bug Entry
	  combine ecryptfs and ocf framework.
	  Only open this function when OCF framework is used.

endmenu #ECRYPT
menu "NFS"

config SYNO_NFSD_WRITE_SIZE_MIN
	int "NFSD min packet size"
	default 131072
	help
		Fix: DS 2.0 #14452
		Dsc: Enlarge max write size of NFS for Vmware to
		write 65536 bytes at once.
		In: fs/nfsd/nfssvc.c

config SYNO_NFSD_UDP_PACKET
	bool "Provide a interface for user to set the udp packet size they want"
	default y
	help
		Fix: DSM #6585, #40483, #44152

config SYNO_NFSD_UDP_MAX_PACKET_SIZE
	int "Provide a interface for user to set the udp packet size they want"
	default 32768
	depends on SYNO_NFSD_UDP_PACKET
	help

config SYNO_NFSD_UDP_MIN_PACKET_SIZE
	int "Provide a interface for user to set the udp packet size they want"
	default	4096
	depends on SYNO_NFSD_UDP_PACKET
	help

config SYNO_NFSD_UDP_DEF_PACKET_SIZE
	int "Provide a interface for user to set the udp packet size they want"
	default 8192
	depends on SYNO_NFSD_UDP_PACKET
	help

config SYNO_NFSD_UNIX_PRI
	bool "Provide a interface for user to enable command chmod or not on ACL share"
	default y
	help
		Fix: DSM #55927

config SYNO_NFS4_DISABLE_UDP
	bool "disable NFSv4 over UDP"
	default y
	help
		Fix: DSM #46220 Disable NFSv4 over UDP
		Dsc: According to RFC 3530
		- Where an NFS version 4 implementation supports operation over the IP
		- network protocol, the supported transports between NFS and IP MUST be
		- among the IETF-approved congestion control transport protocols, which
		- include TCP and SCTP.
		This define disable NFSv4 over UDP by
		- 1. Do not register NFS version 4 UDP port to rpcbind on svc_register
		- 2. Return nfserr_acces on nfsd4_access when client connect from UDP protocol
		IN: net/sunrpc/svc.c
		IN: fs/nfsd/nfs4proc.c

endmenu #NFS

menu "HFSPLUS"

config SYNO_HFSPLUS_MAX_FILENAME_CHECK
	bool "HFS+ return error when filename's length > 255"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #41596
	  When looking up a filename with length > 255,  we return a
	  ENAMETOOLONG error.
	  previous naming: SYNO_HFSPLUS_CHECK_MAX_FILENAME_LEN

config SYNO_HFSPLUS_DONT_ZERO_ON_NEW_FILE
	bool "HFS+ don't zero a newly created file"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #46407, #47300
	  When creating files through samba, the inode set size will be
	  called first. In HFS+, it will incur real blocks zeroing, bad
	  performance, and even timeout for a large file. Skip the zeroing
	  action if it is a newly created file.
	  previous naming: SYNO_HFSPLUS_DONT_ZERO_ON_SET_SIZE

config SYNO_HFSPLUS_ADD_MUTEX_FOR_VFS_OPERATION
	bool "HFS+ add mutex lock on all vsf operation"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #15588
	  The hfs+ module of vanilla kernel have no lock on vfs operations,
	  thus causes problem when running with multi-processes. Add the
	  mutex locks.
	  FIXME: we may use per inode lock or any other smaller lock,
	  instead of a global lock.

config SYNO_HFSPLUS_RSRC_INODE_COUNT_FIX
	bool "HFS+ fix rsrc inode count problem"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #42155
	  RSRC inode is the inode of resource fork of a file. We can
	  lookup a regular file and bind the dentry of its resource fork to
	  RSRC inode, but in some cases we doesn't inc the i_count of the
	  RSRC inode. Fix it.
	  previous naming: SYNO_FIX_HFSPLUS_INODE_COUNT_WRONG

config SYNO_HFSPLUS_ERROR_HANDLE_ENHANCE
	bool "HFS+ change some WARN_ON to warning message and RO fs"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #42155
	  Change some WARN_ON or BUG_ON to warning message and switch it to RO fs.

config SYNO_HFSPLUS_SHOW_CASELESS_INFO
	bool "HFS+ show caseless option"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #46346
	  Show 'caseless' in /proc/mounts if it is a caseless hfs+ partition.
	  This info is mainly used by afp. See netatalk-2.x commit:
	  4f741f18ad493ae4c9da715e5a3cd74ccf03ec3a

config SYNO_HFSPLUS_CASELESS_CREATE_BY_NEW_NAME
	bool "HFS+ create caseless dentry by new name"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #46710
	  When doing caseless dentry lookup, if found, override the name of the
	  dentry with the new one. This is to solve the problem like:
	  touch AAA; rm AAA; touch aaa; ls

config SYNO_HFSPLUS_EA
	bool "HFS+ enable EA support"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #38013
	  Enable EA support on hfs+ volume.

config SYNO_HFSPLUS_BREC_FIND_RET_CHECK
	bool "Check brec_find return value while update parent"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #68998
	  hfsplus will try to update the attribute of parent brec even if it is empty.
	  We skip update empty brec to prevent illegal memory write and hfsplus data corruption.

config SYNO_HFSPLUS_GET_PAGE_IF_IN_USE
	bool "Add page get/put mech for bnode page to prevent bad page"
	default y
	depends on HFSPLUS_FS
	help
	  <DSM> #74413
	  There is no protection for hfsplus data page. The page may be mark in use after the page is release.

endmenu #HFSPLUS

menu "UDF"
config SYNO_UDF_CASELESS
	bool "UDF use caseless lookup"
	default y
	depends on UDF_FS
	help
	  <DSM> #25869
	  Use caseless lookup in UDF. Add another mount option "casesensitive" to
	  disable the caseless lookup.
	  previous naming: SYNO_FORCE_UDF_CASELESS

config SYNO_UDF_UINT_UID_GID
	bool "UDF uses unsigned int UID/GID"
	default y
	help
	  <DSM> #72580
	  ISO mount (fs/isofs, fs/udf) uses match_int to parse uid/gid and
	  the function is revised to avoid overflow issue in linux-3.10.
	  Therefore, ISO mount is not workable for uid/gid being greater
	  than signed int with original linux-3.10.
          Add SYNO_get_option_ul which is based on get_option_ul in CIFS
	  module to parse unsigned long type options.

endmenu #UDF

menu "ISOFS"

config SYNO_ISOFS_UINT_UID_GID
	bool "ISOFS uses unsigned int UID/GID"
	default y
	help
	  <DSM> #72580
	  ISO mount (fs/isofs, fs/udf) uses match_int to parse uid/gid and
	  the function is revised to avoid overflow issue in linux-3.10.
	  Therefore, ISO mount is not workable for uid/gid being greater
	  than signed int with original linux-3.10.
          Add SYNO_get_option_ul which is based on get_option_ul in CIFS
	  module to parse unsigned long type options.

endmenu #ISOFS

menu "ZRAM"

config SYNO_ZRAM_32KB_PAGE_SUPPORT
	bool "Support ZRAM for 32KB Page Size"
	default y
	depends on ARM_PAGE_SIZE_32KB
	help
	  <DSM> #61642
	  Make ZRAM workable on Alpine platform with 32KB page size.

endmenu #ZRAM

endmenu #File Systems
