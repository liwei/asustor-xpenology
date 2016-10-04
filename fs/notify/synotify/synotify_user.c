#include <linux/file.h>
#include <linux/fs.h> /* struct inode */
#include <linux/fsnotify_backend.h>
#include <linux/idr.h>
#include <linux/init.h> /* module_init */
#include <linux/synotify.h>
#include <linux/kernel.h> /* roundup() */
#include <linux/namei.h> /* LOOKUP_FOLLOW */
#include <linux/sched.h> /* struct user */
#include <linux/slab.h> /* struct kmem_cache */
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/anon_inodes.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/mount.h> /* struct vfsmount */

#include <asm/ioctls.h>
#include <asm-generic/bitsperlong.h>
#include "../../mount.h"

#define SYNOTIFY_DEFAULT_MAX_EVENTS	16384 /* per group */
#define SYNOTIFY_DEFAULT_MAX_WATCHERS	8192 /* per group */
#define SYNOTIFY_DEFAULT_MAX_INSTANCES	128 /* per user */

extern const struct fsnotify_ops synotify_fsnotify_ops;

static struct kmem_cache *synotify_mark_cache __read_mostly;

/*
 * Get an fsnotify notification event if one exists and is small
 * enough to fit in "count". Return an error pointer if the count
 * is not large enough.
 *
 * Called with the group->notification_mutex held.
 */
static struct fsnotify_event *get_one_event(struct fsnotify_group *group,
					    size_t count)
{
	size_t event_size = sizeof(struct synotify_event);
	struct fsnotify_event *event;

	BUG_ON(!mutex_is_locked(&group->notification_mutex));

	if (fsnotify_notify_queue_is_empty(group))
		return NULL;

	event = fsnotify_peek_notify_event(group);

	pr_debug("%s: group=%p count=%zd, event=%p\n", __func__, group, count, event);

	if (event->full_name_len)
		event_size += roundup(event->full_name_len + 1, event_size);

	if (event_size > count)
		return ERR_PTR(-EINVAL);

	/* held the notification_mutex the whole time, so this is the
	 * same event we peeked above */
	return fsnotify_remove_notify_event(group);
}

static inline u32 synotify_mask_to_arg(__u32 mask)
{
	return mask & SYNO_ALL_EVENTS;
}

static ssize_t copy_event_to_user(struct fsnotify_group *group,
				  struct fsnotify_event *event,
				  char __user *buf)
{
	struct synotify_event synotify_event;

	size_t event_size = sizeof(struct synotify_event);
	size_t name_len = 0;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	/*
	 * round up event->name_len so it is a multiple of event_size
	 * plus an extra byte for the terminating '\0'.
	 */
	if (event->full_name_len)
		name_len = roundup(event->full_name_len + 1, event_size);

	synotify_event.len = name_len;
	synotify_event.mask = synotify_mask_to_arg(event->mask);
	synotify_event.cookie = event->sync_cookie;

	/* send the main event */
	if (copy_to_user(buf, &synotify_event, event_size))
		return -EFAULT;

	buf += event_size;

	/*
	 * fsnotify only stores the pathname, so here we have to send the pathname
	 * and then pad that pathname out to a multiple of sizeof(inotify_event)
	 * with zeros.  I get my zeros from the nul_inotify_event.
	 */
	if (name_len) {
		unsigned int len_to_zero = name_len - event->full_name_len;
		/* copy the path name */
		if (copy_to_user(buf, event->full_name, event->full_name_len))
			return -EFAULT;
		buf += event->full_name_len;

		/* fill userspace with 0's */
		if (clear_user(buf, len_to_zero))
			return -EFAULT;
		buf += len_to_zero;
		event_size += name_len;
	}

	return event_size;
}

/* synotifiy userspace file descriptor functions */
static unsigned int synotify_poll(struct file *file, poll_table *wait)
{
	struct fsnotify_group *group = file->private_data;
	int ret = 0;

	poll_wait(file, &group->notification_waitq, wait);
	mutex_lock(&group->notification_mutex);
	if (!fsnotify_notify_queue_is_empty(group))
		ret = POLLIN | POLLRDNORM;
	mutex_unlock(&group->notification_mutex);

	return ret;
}

static ssize_t synotify_read(struct file *file, char __user *buf,
			     size_t count, loff_t *pos)
{
	struct fsnotify_group *group;
	struct fsnotify_event *kevent;
	char __user *start;
	int ret;
	DEFINE_WAIT(wait);

	start = buf;
	group = file->private_data;

	pr_debug("%s: group=%p\n", __func__, group);

	while (1) {
		prepare_to_wait(&group->notification_waitq, &wait, TASK_INTERRUPTIBLE);

		mutex_lock(&group->notification_mutex);
		kevent = get_one_event(group, count);
		mutex_unlock(&group->notification_mutex);

		if (kevent) {
			ret = PTR_ERR(kevent);
			if (IS_ERR(kevent))
				break;
			ret = copy_event_to_user(group, kevent, buf);
			fsnotify_put_event(kevent);
			if (ret < 0)
				break;
			buf += ret;
			count -= ret;
			continue;
		}

		ret = -EAGAIN;
		if (file->f_flags & O_NONBLOCK)
			break;
		ret = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		if (start != buf)
			break;

		schedule();
	}

	finish_wait(&group->notification_waitq, &wait);
	if (start != buf && ret != -EFAULT)
		ret = buf - start;
	return ret;
}

static int synotify_release(struct inode *ignored, struct file *file)
{
	struct fsnotify_group *group = file->private_data;

	pr_debug("%s group=%p\n",__FUNCTION__, group);

	fsnotify_clear_marks_by_group(group);

	/* matches the fanotify_init->fsnotify_alloc_group */
	fsnotify_destroy_group(group);

	return 0;
}

static long synotify_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fsnotify_group *group;
	struct fsnotify_event_holder *holder;
	struct fsnotify_event *event;
	void __user *p;
	int ret = -ENOTTY;
	size_t send_len = 0;

	group = file->private_data;

	p = (void __user *) arg;

	pr_debug("%s: group=%p cmd=%u\n", __func__, group, cmd);

	switch (cmd) {
	case FIONREAD:
		mutex_lock(&group->notification_mutex);
		list_for_each_entry(holder, &group->notification_list, event_list) {
			event = holder->event;
			send_len += sizeof(struct synotify_event);
			if (event->full_name_len)
				send_len += roundup(event->full_name_len + 1,
						sizeof(struct synotify_event));
		}
		mutex_unlock(&group->notification_mutex);
		ret = put_user(send_len, (int __user *) p);
		break;
	}

	return ret;
}

static const struct file_operations synotify_fops = {
	.poll		= synotify_poll,
	.read		= synotify_read,
	.fasync		= NULL,
	.release	= synotify_release,
	.unlocked_ioctl	= synotify_ioctl,
	.compat_ioctl	= synotify_ioctl,
	.llseek		= noop_llseek,
};

static void synotify_free_mark(struct fsnotify_mark *fsn_mark)
{
	kmem_cache_free(synotify_mark_cache, fsn_mark);
}

static int synotify_find_path(const char __user *filename,
			      struct path *path, unsigned int flags)
{
	int error;

	pr_debug("%s: filename=%p flags=%x\n", __func__, filename, flags);

	error = user_path_at(AT_FDCWD, filename, flags, path);
	if (error)
		return error;
	/* you can only watch an inode if you have read permissions on it */
	error = inode_permission(path->dentry->d_inode, MAY_READ);
	if (error)
		path_put(path);
	return error;
}

static __u32 synotify_mark_remove_from_mask(struct fsnotify_mark *fsn_mark,
					    __u32 mask, struct fsnotify_group *group)
{
	__u32 oldmask;
	__u32 setmask;

	setmask = mask & ~SYNO_DONT_FOLLOW;

	spin_lock(&fsn_mark->lock);

	oldmask = fsn_mark->mask;
	fsnotify_set_mark_mask_locked(fsn_mark, (oldmask & ~mask));

	spin_unlock(&fsn_mark->lock);

	if (!(oldmask & ~mask))
		fsnotify_destroy_mark(fsn_mark, group);

	return mask & oldmask;
}

static int synotify_remove_vfsmount_mark(struct fsnotify_group *group,
					 struct vfsmount *mnt, __u32 mask)
{
	struct fsnotify_mark *fsn_mark = NULL;
	__u32 removed;

	fsn_mark = fsnotify_find_vfsmount_mark(group, mnt);
	if (!fsn_mark)
		return -ENOENT;

	removed = synotify_mark_remove_from_mask(fsn_mark, mask, group);

	/* synotify_mark_remove_from_mask invokes fsnotify_get_mark, so we put here */
	fsnotify_put_mark(fsn_mark);
	// FIXME: it will remove entire mount mask
	if (removed & real_mount(mnt)->mnt_fsnotify_mask)
		fsnotify_recalc_vfsmount_mask(mnt);

	return 0;
}

static __u32 synotify_mark_add_to_mask(struct fsnotify_mark *fsn_mark,
				       __u32 mask)
{
	__u32 oldmask = 0;
	__u32 setmask = 0;

	setmask = mask & ~SYNO_DONT_FOLLOW;

	spin_lock(&fsn_mark->lock);

	oldmask = fsn_mark->mask;
	fsnotify_set_mark_mask_locked(fsn_mark, (oldmask | setmask));

	spin_unlock(&fsn_mark->lock);

	/* return new add event */
	return setmask & ~oldmask;
}

static int synotify_add_vfsmount_mark(struct fsnotify_group *group,
				      struct vfsmount *mnt, __u32 mask)
{
	struct fsnotify_mark *fsn_mark;
	__u32 added;
	int ret = 0;

	fsn_mark = fsnotify_find_vfsmount_mark(group, mnt);
	if (!fsn_mark) {
		if (atomic_read(&group->num_marks) >= group->synotify_data.max_watchers)
			return -ENOSPC;

		fsn_mark = kmem_cache_alloc(synotify_mark_cache, GFP_KERNEL);
		if (!fsn_mark)
			return -ENOMEM;

		fsnotify_init_mark(fsn_mark, synotify_free_mark);
		ret = fsnotify_add_mark(fsn_mark, group, NULL, mnt, 0);
		if (ret)
			goto err;
	}

	/* update mark flags/ignored_flags */
	added = synotify_mark_add_to_mask(fsn_mark, mask);

	/* Check if we have any new event we need to take care */
	if (added & ~real_mount(mnt)->mnt_fsnotify_mask)
		fsnotify_recalc_vfsmount_mask(mnt);
err:
	fsnotify_put_mark(fsn_mark);
	return ret;
}

/* synotify syscalls */
SYSCALL_DEFINE1(SYNONotifyInit, unsigned int, flags)
{
	struct fsnotify_group *group;
	int f_flags = 0;
	int fd = 0;
	struct user_struct *user;

	pr_debug("%s: flags=%x\n",__func__, flags);

	if(flags & ~(SYNO_NONBLOCK | SYNO_CLOEXEC))
		return -EINVAL;

	if(flags & SYNO_CLOEXEC){
		f_flags |= O_CLOEXEC;
	}
	if(flags & SYNO_NONBLOCK){
		f_flags |= O_NONBLOCK;
	}

	user = get_current_user();
	if (user->uid != 0) {
		return -EPERM;
	}
	if (atomic_read(&user->synotify_instances) > SYNOTIFY_DEFAULT_MAX_INSTANCES) {
		free_uid(user);
		return -EMFILE;
	}

	/* fsnotify_alloc_group takes a ref.  Dropped in synotify_release */
	group = fsnotify_alloc_group(&synotify_fsnotify_ops);
	if (IS_ERR(group)) {
		free_uid(user);
		return PTR_ERR(group);
	}

	group->synotify_data.user = user;
	atomic_inc(&user->synotify_instances);

	group->max_events = SYNOTIFY_DEFAULT_MAX_EVENTS;
	group->synotify_data.max_watchers = SYNOTIFY_DEFAULT_MAX_WATCHERS;

	fd = anon_inode_getfd("[synotify]", &synotify_fops, group, f_flags);
	if (fd < 0)
		goto out_destroy_group;

	return fd;

out_destroy_group:
	fsnotify_destroy_group(group);
	return fd;
}

SYSCALL_DEFINE3(SYNONotifyRemoveWatch, int, synotify_fd, const char __user *, pathname, __u64, mask)
{
	struct vfsmount *mnt = NULL;
	struct fsnotify_group *group;
	struct file *filp;
	struct path path;
	int ret = -EINVAL, fput_needed;
	unsigned int flags = 0;

	pr_debug("%s: synotify_fd=%d pathname=%p mask=%llx\n",__FUNCTION__, synotify_fd, pathname, mask);

	/*
	 * we only use lower 32 bits right now.
	 * Depends on arch, it may be in lower or upper 32 bits of input parameter.
	 * If it is in lower 32 bits, it is OK. If it is in upper 32 bits, we move it to
	 * lower 32 bits. If it is in both upper and lower, we return -EINVAL.
	 */
	if (mask >> 32) {
		if (mask << 32)
			return ret;
		mask >>= 32;
	}

	filp = fget_light(synotify_fd, &fput_needed);
	if (unlikely(!filp))
		return -EBADF;

	/* verify that this is indeed an fanotify instance */
	if (unlikely(filp->f_op != &synotify_fops))
		goto fput_and_out;

	if (!(mask & SYNO_DONT_FOLLOW))
		flags |= LOOKUP_FOLLOW;

	ret = synotify_find_path(pathname, &path, flags);
	if (ret)
		goto fput_and_out;

	group = filp->private_data;

	mnt = path.mnt;

	ret = synotify_remove_vfsmount_mark(group, mnt, mask);

	path_put(&path);
fput_and_out:
	fput_light(filp, fput_needed);
	return ret;
}

SYSCALL_DEFINE3(SYNONotifyAddWatch, int, synotify_fd, const char __user *, pathname, __u64, mask)
{
	struct vfsmount *mnt = NULL;
	struct fsnotify_group *group;
	struct file *filp;
	struct path path;
	int ret = -EINVAL;
	int fput_needed;
	unsigned int flags = 0;

	pr_debug("%s: synotify_fd=%d pathname=%p mask=%llx\n",
		 __func__, synotify_fd, pathname, mask);

	/*
	 * we only use lower 32 bits right now.
	 * Depends on arch, it may be in lower or upper 32 bits of input parameter.
	 * If it is in lower 32 bits, it is OK. If it is in upper 32 bits, we move it to
	 * lower 32 bits. If it is in both upper and lower, we return -EINVAL.
	 */
	if (mask >> 32) {
		if (mask << 32)
			return ret;
		mask >>= 32;
	}

	filp = fget_light(synotify_fd, &fput_needed);
	if (unlikely(!filp))
		return -EBADF;

	/* verify that this is indeed an syno notify instance */
	if (unlikely(filp->f_op != &synotify_fops))
		goto fput_and_out;

	if (!(mask & SYNO_DONT_FOLLOW))
		flags |= LOOKUP_FOLLOW;

	ret = synotify_find_path(pathname, &path, flags);
	if (ret)
		goto fput_and_out;

	group = filp->private_data;

	/* In current design, synotify monitors mount point event only */
	mnt = path.mnt;

	ret = synotify_add_vfsmount_mark(group, mnt, mask);

	path_put(&path);
fput_and_out:
	fput_light(filp, fput_needed);
	return ret;
}

#if BITS_PER_LONG == 32
SYSCALL_DEFINE3(SYNONotifyRemoveWatch32, int, synotify_fd, const char __user *, pathname, __u32, mask){
	__u64 mask64 = ((__u64)mask & 0xffffffffULL);
	return sys_SYNONotifyRemoveWatch(synotify_fd, pathname, mask64);
}

SYSCALL_DEFINE3(SYNONotifyAddWatch32, int, synotify_fd, const char __user *, pathname, __u32, mask){
	__u64 mask64 = ((__u64)mask & 0xffffffffULL);
	return sys_SYNONotifyAddWatch(synotify_fd, pathname, mask64);
}
#endif /* BITS_PER_LONG == 32 */

static int __init synotify_user_setup(void)
{
	synotify_mark_cache = kmem_cache_create("synotify_mark",sizeof(struct fsnotify_mark), __alignof__(struct fsnotify_mark),SLAB_PANIC , NULL);

	return 0;
}
device_initcall(synotify_user_setup);
