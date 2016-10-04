#include <linux/synotify.h>
#include <linux/fdtable.h>
#include <linux/fsnotify_backend.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h> /* UINT_MAX */
#include <linux/mount.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>

static bool should_merge(struct fsnotify_event *old, struct fsnotify_event *new)
{
	pr_debug("%s: old=%p, mask=%x, new=%p, mask=%x\n", __func__, old, old->mask, new, new->mask);
	if (old->data_type == FSNOTIFY_EVENT_NONE && new->data_type ==FSNOTIFY_EVENT_NONE) {
		return (old->mask & FS_Q_OVERFLOW) && (new->mask & FS_Q_OVERFLOW);
	}
	if (old->mask != new->mask) return false;

	if ( (new->mask & (FS_ATTRIB | FS_ACCESS | FS_MODIFY))
			&& (old->path.mnt == new->path.mnt)
			&& (old->path.dentry == new->path.dentry))
		return true;
	return false;
}

/* and the list better be locked by something too! */
static struct fsnotify_event *synotify_merge(struct list_head *list,
					     struct fsnotify_event *event)
{
	struct fsnotify_event_holder *last_holder;
	struct fsnotify_event *last_event;
	pr_debug("%s: list=%p event=%p, mask=%x\n", __func__, list, event, event->mask);

	spin_lock(&event->lock);

	last_holder = list_entry(list->prev, struct fsnotify_event_holder, event_list);
	last_event = last_holder->event;

	if (should_merge(last_event, event)) {
		fsnotify_get_event(last_event);
	} else {
		last_event = NULL;
	}
	spin_unlock(&event->lock);

	return last_event;
}

static int synotify_handle_event(struct fsnotify_group *group,
				 struct fsnotify_mark *inode_mark,
				 struct fsnotify_mark *synotify_mark,
				 struct fsnotify_event *event)
{
	int ret = 0;
	struct fsnotify_event *notify_event = NULL;

	pr_debug("%s: group=%p event=%p, mnt=%p, mask=%x\n", __func__, group, event, event->path.mnt, event->mask);
	// to prevent event dependecy, we should have much clever way to do merge
	notify_event = fsnotify_add_notify_event(group, event, NULL, synotify_merge);
	if (IS_ERR(notify_event))
		return PTR_ERR(notify_event);

	if (notify_event)
		fsnotify_put_event(notify_event);

	return ret;
}

static bool synotify_should_send_event(struct fsnotify_group *group,
				       struct inode *to_tell,
				       struct fsnotify_mark *inode_mark,
				       struct fsnotify_mark *vfsmnt_mark,
				       __u32 event_mask, void *data, int data_type)
{
	__u32 marks_mask;

	pr_debug("%s: group=%p to_tell=%p inode_mark=%p vfsmnt_mark=%p "
		 "mask=%x data=%p data_type=%d\n", __func__, group, to_tell,
		 inode_mark, vfsmnt_mark, event_mask, data, data_type);

	/* if we don't have enough info to send an event to userspace say no */
	if (data_type != FSNOTIFY_EVENT_SYNO && data_type != FSNOTIFY_EVENT_PATH)
		return false;

	if (vfsmnt_mark) {
		marks_mask = vfsmnt_mark->mask;
	} else {
		BUG();
	}

	if (event_mask & marks_mask)
		return true;

	return false;
}

static void synotify_free_group_priv(struct fsnotify_group *group)
{
	struct user_struct *user;

	user = group->synotify_data.user;
	atomic_dec(&user->synotify_instances);
	free_uid(user);
}

const struct fsnotify_ops synotify_fsnotify_ops = {
	.handle_event = synotify_handle_event,
	.should_send_event = synotify_should_send_event,
	.free_group_priv = synotify_free_group_priv,
	.free_event_priv = NULL,
	.freeing_mark = NULL,
};
