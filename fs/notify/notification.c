/*
 *  Copyright (C) 2008 Red Hat, Inc., Eric Paris <eparis@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Basic idea behind the notification queue: An fsnotify group (like inotify)
 * sends the userspace notification about events asynchronously some time after
 * the event happened.  When inotify gets an event it will need to add that
 * event to the group notify queue.  Since a single event might need to be on
 * multiple group's notification queues we can't add the event directly to each
 * queue and instead add a small "event_holder" to each queue.  This event_holder
 * has a pointer back to the original event.  Since the majority of events are
 * going to end up on one, and only one, notification queue we embed one
 * event_holder into each event.  This means we have a single allocation instead
 * of always needing two.  If the embedded event_holder is already in use by
 * another group a new event_holder (from fsnotify_event_holder_cachep) will be
 * allocated and used.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/atomic.h>

#include <linux/fsnotify_backend.h>
#include "fsnotify.h"

static struct kmem_cache *fsnotify_event_cachep;
static struct kmem_cache *fsnotify_event_holder_cachep;
/*
 * This is a magic event we send when the q is too full.  Since it doesn't
 * hold real event information we just keep one system wide and use it any time
 * it is needed.  It's refcnt is set 1 at kernel init time and will never
 * get set to 0 so it will never get 'freed'
 */
static struct fsnotify_event *q_overflow_event;
static atomic_t fsnotify_sync_cookie = ATOMIC_INIT(0);

/**
 * fsnotify_get_cookie - return a unique cookie for use in synchronizing events.
 * Called from fsnotify_move, which is inlined into filesystem modules.
 */
u32 fsnotify_get_cookie(void)
{
	return atomic_inc_return(&fsnotify_sync_cookie);
}
EXPORT_SYMBOL_GPL(fsnotify_get_cookie);

/* return true if the notify queue is empty, false otherwise */
bool fsnotify_notify_queue_is_empty(struct fsnotify_group *group)
{
	BUG_ON(!mutex_is_locked(&group->notification_mutex));
	return list_empty(&group->notification_list) ? true : false;
}

void fsnotify_get_event(struct fsnotify_event *event)
{
	atomic_inc(&event->refcnt);
}

void fsnotify_put_event(struct fsnotify_event *event)
{
	if (!event)
		return;

	if (atomic_dec_and_test(&event->refcnt)) {
		pr_debug("%s: event=%p\n", __func__, event);

#ifdef CONFIG_SYNO_FS_NOTIFY
		if (event->data_type == FSNOTIFY_EVENT_PATH || event->data_type == FSNOTIFY_EVENT_SYNO)
#else
		if (event->data_type == FSNOTIFY_EVENT_PATH)
#endif
			path_put(&event->path);

		BUG_ON(!list_empty(&event->private_data_list));

		kfree(event->file_name);
#ifdef CONFIG_SYNO_FS_NOTIFY
		kfree(event->full_name);
#endif
		put_pid(event->tgid);
		kmem_cache_free(fsnotify_event_cachep, event);
	}
}

struct fsnotify_event_holder *fsnotify_alloc_event_holder(void)
{
	return kmem_cache_alloc(fsnotify_event_holder_cachep, GFP_KERNEL);
}

void fsnotify_destroy_event_holder(struct fsnotify_event_holder *holder)
{
	if (holder)
		kmem_cache_free(fsnotify_event_holder_cachep, holder);
}

/*
 * Find the private data that the group previously attached to this event when
 * the group added the event to the notification queue (fsnotify_add_notify_event)
 */
struct fsnotify_event_private_data *fsnotify_remove_priv_from_event(struct fsnotify_group *group, struct fsnotify_event *event)
{
	struct fsnotify_event_private_data *lpriv;
	struct fsnotify_event_private_data *priv = NULL;

	assert_spin_locked(&event->lock);

	list_for_each_entry(lpriv, &event->private_data_list, event_list) {
		if (lpriv->group == group) {
			priv = lpriv;
			list_del(&priv->event_list);
			break;
		}
	}
	return priv;
}

#ifdef CONFIG_SYNO_FS_NOTIFY
// base_name should have leading '/'
static void formalize_full_path(const char *mnt_name, const char *base_name, char *full_path){
	if (mnt_name[0] == '/'){
		if(mnt_name[1] == 0){
			snprintf(full_path, PATH_MAX,"%s", base_name);
		}else{
			snprintf(full_path, PATH_MAX,"%s%s", mnt_name, base_name);
		}
	}else
		snprintf(full_path, PATH_MAX,"/%s%s", mnt_name, base_name);
}

static int SYNOFetchFullName(struct fsnotify_event *event, gfp_t gfp)
{
	char *dentry_path_buf = NULL;
	char *full_path = NULL;
	char *mnt_full_path = NULL;
	char *dentry_path = NULL;
	struct vfsmount *mnt = event->path.mnt;
	int ret = -1;
	int data_type = event->data_type;

	if(data_type == FSNOTIFY_EVENT_PATH) {
		struct path root_path;
		root_path.mnt = mnt;
		root_path.dentry = mnt->mnt_root;
		dentry_path_buf = kmalloc(PATH_MAX, gfp);
		if (unlikely(!dentry_path_buf)) {
			ret = -ENOMEM;
			goto ERR;
		}
		dentry_path = __d_path(&event->path, &root_path, dentry_path_buf, PATH_MAX-1);
		if (unlikely(IS_ERR_OR_NULL(dentry_path))) {
			ret = PTR_ERR(dentry_path);
			goto ERR;
		}
	}

	full_path = kmalloc(PATH_MAX, gfp);
	mnt_full_path = kzalloc(PATH_MAX, gfp);
	if(!full_path || !mnt_full_path){
		ret = -ENOMEM;
		goto ERR;
	}

	ret = syno_fetch_mountpoint_fullpath(mnt, PATH_MAX, mnt_full_path);
	if (ret < 0)
		goto ERR;
	if(data_type == FSNOTIFY_EVENT_PATH) {
		formalize_full_path(mnt_full_path, dentry_path, full_path);
	} else {
		formalize_full_path(mnt_full_path, event->file_name, full_path);
	}
	event->full_name = kstrdup(full_path, gfp);
	if (unlikely(!event->full_name)) {
		ret = -ENOMEM;
		goto ERR;
	}
	event->full_name_len = strlen(event->full_name);
	ret = 0;

ERR:
	kfree(dentry_path_buf);
	kfree(full_path);
	kfree(mnt_full_path);
	return ret;
}
#endif /* CONFIG_SYNO_FS_NOTIFY */

/*
 * Add an event to the group notification queue.  The group can later pull this
 * event off the queue to deal with.  If the event is successfully added to the
 * group's notification queue, a reference is taken on event.
 */
struct fsnotify_event *fsnotify_add_notify_event(struct fsnotify_group *group, struct fsnotify_event *event,
						 struct fsnotify_event_private_data *priv,
						 struct fsnotify_event *(*merge)(struct list_head *,
										 struct fsnotify_event *))
{
	struct fsnotify_event *return_event = NULL;
	struct fsnotify_event_holder *holder = NULL;
	struct list_head *list = &group->notification_list;

	pr_debug("%s: group=%p event=%p priv=%p\n", __func__, group, event, priv);

	/*
	 * There is one fsnotify_event_holder embedded inside each fsnotify_event.
	 * Check if we expect to be able to use that holder.  If not alloc a new
	 * holder.
	 * For the overflow event it's possible that something will use the in
	 * event holder before we get the lock so we may need to jump back and
	 * alloc a new holder, this can't happen for most events...
	 */
	if (!list_empty(&event->holder.event_list)) {
alloc_holder:
		holder = fsnotify_alloc_event_holder();
		if (!holder)
			return ERR_PTR(-ENOMEM);
	}

	mutex_lock(&group->notification_mutex);

	if (group->q_len >= group->max_events) {
		event = q_overflow_event;

		/*
		 * we need to return the overflow event
		 * which means we need a ref
		 */
		fsnotify_get_event(event);
		return_event = event;

		/* sorry, no private data on the overflow event */
		priv = NULL;
	}

	if (!list_empty(list) && merge) {
		struct fsnotify_event *tmp;

		tmp = merge(list, event);
		if (tmp) {
			mutex_unlock(&group->notification_mutex);

			if (return_event)
				fsnotify_put_event(return_event);
			if (holder != &event->holder)
				fsnotify_destroy_event_holder(holder);
			return tmp;
		}
	}

	spin_lock(&event->lock);

	if (list_empty(&event->holder.event_list)) {
		if (unlikely(holder))
			fsnotify_destroy_event_holder(holder);
		holder = &event->holder;
	} else if (unlikely(!holder)) {
		/* between the time we checked above and got the lock the in
		 * event holder was used, go back and get a new one */
		spin_unlock(&event->lock);
		mutex_unlock(&group->notification_mutex);

		if (return_event) {
			fsnotify_put_event(return_event);
			return_event = NULL;
		}

		goto alloc_holder;
	}

	group->q_len++;
	holder->event = event;

#ifdef CONFIG_SYNO_FS_NOTIFY
	/* we fetch full name after it is decided to inqueue. */
	if (event->data_type == FSNOTIFY_EVENT_SYNO || event->data_type == FSNOTIFY_EVENT_PATH)
	{
		if (event->full_name == NULL) {
			int ret;
			ret = SYNOFetchFullName(event, GFP_ATOMIC);
			if (ret < 0) {
				spin_unlock(&event->lock);
				mutex_unlock(&group->notification_mutex);
				return ERR_PTR(ret);
			}
		}
	}
#endif
	fsnotify_get_event(event);
	list_add_tail(&holder->event_list, list);
	if (priv)
		list_add_tail(&priv->event_list, &event->private_data_list);
	spin_unlock(&event->lock);
	mutex_unlock(&group->notification_mutex);

	wake_up(&group->notification_waitq);
	kill_fasync(&group->fsn_fa, SIGIO, POLL_IN);
	return return_event;
}

/*
 * Remove and return the first event from the notification list.  There is a
 * reference held on this event since it was on the list.  It is the responsibility
 * of the caller to drop this reference.
 */
struct fsnotify_event *fsnotify_remove_notify_event(struct fsnotify_group *group)
{
	struct fsnotify_event *event;
	struct fsnotify_event_holder *holder;

	BUG_ON(!mutex_is_locked(&group->notification_mutex));

	pr_debug("%s: group=%p\n", __func__, group);

	holder = list_first_entry(&group->notification_list, struct fsnotify_event_holder, event_list);

	event = holder->event;

	spin_lock(&event->lock);
	holder->event = NULL;
	list_del_init(&holder->event_list);
	spin_unlock(&event->lock);

	/* event == holder means we are referenced through the in event holder */
	if (holder != &event->holder)
		fsnotify_destroy_event_holder(holder);

	group->q_len--;

	return event;
}

/*
 * This will not remove the event, that must be done with fsnotify_remove_notify_event()
 */
struct fsnotify_event *fsnotify_peek_notify_event(struct fsnotify_group *group)
{
	struct fsnotify_event *event;
	struct fsnotify_event_holder *holder;

	BUG_ON(!mutex_is_locked(&group->notification_mutex));

	holder = list_first_entry(&group->notification_list, struct fsnotify_event_holder, event_list);
	event = holder->event;

	return event;
}

/*
 * Called when a group is being torn down to clean up any outstanding
 * event notifications.
 */
void fsnotify_flush_notify(struct fsnotify_group *group)
{
	struct fsnotify_event *event;
	struct fsnotify_event_private_data *priv;

	mutex_lock(&group->notification_mutex);
	while (!fsnotify_notify_queue_is_empty(group)) {
		event = fsnotify_remove_notify_event(group);
		/* if they don't implement free_event_priv they better not have attached any */
		if (group->ops->free_event_priv) {
			spin_lock(&event->lock);
			priv = fsnotify_remove_priv_from_event(group, event);
			spin_unlock(&event->lock);
			if (priv)
				group->ops->free_event_priv(priv);
		}
		fsnotify_put_event(event); /* matches fsnotify_add_notify_event */
	}
	mutex_unlock(&group->notification_mutex);
}

static void initialize_event(struct fsnotify_event *event)
{
	INIT_LIST_HEAD(&event->holder.event_list);
	atomic_set(&event->refcnt, 1);

	spin_lock_init(&event->lock);

	INIT_LIST_HEAD(&event->private_data_list);
}

/*
 * Caller damn well better be holding whatever mutex is protecting the
 * old_holder->event_list and the new_event must be a clean event which
 * cannot be found anywhere else in the kernel.
 */
int fsnotify_replace_event(struct fsnotify_event_holder *old_holder,
			   struct fsnotify_event *new_event)
{
	struct fsnotify_event *old_event = old_holder->event;
	struct fsnotify_event_holder *new_holder = &new_event->holder;

	enum event_spinlock_class {
		SPINLOCK_OLD,
		SPINLOCK_NEW,
	};

	pr_debug("%s: old_event=%p new_event=%p\n", __func__, old_event, new_event);

	/*
	 * if the new_event's embedded holder is in use someone
	 * screwed up and didn't give us a clean new event.
	 */
	BUG_ON(!list_empty(&new_holder->event_list));

	spin_lock_nested(&old_event->lock, SPINLOCK_OLD);
	spin_lock_nested(&new_event->lock, SPINLOCK_NEW);

	new_holder->event = new_event;
	list_replace_init(&old_holder->event_list, &new_holder->event_list);

	spin_unlock(&new_event->lock);
	spin_unlock(&old_event->lock);

	/* event == holder means we are referenced through the in event holder */
	if (old_holder != &old_event->holder)
		fsnotify_destroy_event_holder(old_holder);

	fsnotify_get_event(new_event); /* on the list take reference */
	fsnotify_put_event(old_event); /* off the list, drop reference */

	return 0;
}

struct fsnotify_event *fsnotify_clone_event(struct fsnotify_event *old_event)
{
	struct fsnotify_event *event;

	event = kmem_cache_alloc(fsnotify_event_cachep, GFP_KERNEL);
	if (!event)
		return NULL;

	pr_debug("%s: old_event=%p new_event=%p\n", __func__, old_event, event);

	memcpy(event, old_event, sizeof(*event));
	initialize_event(event);

	if (event->name_len) {
		event->file_name = kstrdup(old_event->file_name, GFP_KERNEL);
		if (!event->file_name) {
			kmem_cache_free(fsnotify_event_cachep, event);
			return NULL;
		}
	}
#ifdef CONFIG_SYNO_FS_NOTIFY
	if (event->full_name_len) {
		event->full_name = kstrdup(old_event->full_name, GFP_KERNEL);
		if (!event->full_name) {
			kmem_cache_free(fsnotify_event_cachep, event);
			return NULL;
		}
	}
#endif
	event->tgid = get_pid(old_event->tgid);
#ifdef CONFIG_SYNO_FS_NOTIFY
	if (event->data_type == FSNOTIFY_EVENT_PATH || event->data_type == FSNOTIFY_EVENT_SYNO)
#else
	if (event->data_type == FSNOTIFY_EVENT_PATH)
#endif
		path_get(&event->path);

	return event;
}

/*
 * fsnotify_create_event - Allocate a new event which will be sent to each
 * group's handle_event function if the group was interested in this
 * particular event.
 *
 * @to_tell the inode which is supposed to receive the event (sometimes a
 *	parent of the inode to which the event happened.
 * @mask what actually happened.
 * @data pointer to the object which was actually affected
 * @data_type flag indication if the data is a file, path, inode, nothing...
 * @name the filename, if available
 */
struct fsnotify_event *fsnotify_create_event(struct inode *to_tell, __u32 mask, void *data,
					     int data_type, const unsigned char *name,
					     u32 cookie, gfp_t gfp)
{
	struct fsnotify_event *event;

	event = kmem_cache_zalloc(fsnotify_event_cachep, gfp);
	if (!event)
		return NULL;

	pr_debug("%s: event=%p to_tell=%p mask=%x data=%p data_type=%d\n",
		 __func__, event, to_tell, mask, data, data_type);

	initialize_event(event);

	if (name) {
		event->file_name = kstrdup(name, gfp);
		if (!event->file_name) {
			kmem_cache_free(fsnotify_event_cachep, event);
			return NULL;
		}
		event->name_len = strlen(event->file_name);
	}

	event->tgid = get_pid(task_tgid(current));
	event->sync_cookie = cookie;
	event->to_tell = to_tell;
	event->data_type = data_type;

	switch (data_type) {
#ifdef CONFIG_SYNO_FS_NOTIFY
	case FSNOTIFY_EVENT_SYNO:
#endif
	case FSNOTIFY_EVENT_PATH: {
		struct path *path = data;
		event->path.dentry = path->dentry;
		event->path.mnt = path->mnt;
		path_get(&event->path);
		break;
	}
	case FSNOTIFY_EVENT_INODE:
		event->inode = data;
		break;
	case FSNOTIFY_EVENT_NONE:
		event->inode = NULL;
		event->path.dentry = NULL;
		event->path.mnt = NULL;
		break;
	default:
		BUG();
	}

	event->mask = mask;

	return event;
}

static __init int fsnotify_notification_init(void)
{
	fsnotify_event_cachep = KMEM_CACHE(fsnotify_event, SLAB_PANIC);
	fsnotify_event_holder_cachep = KMEM_CACHE(fsnotify_event_holder, SLAB_PANIC);

	q_overflow_event = fsnotify_create_event(NULL, FS_Q_OVERFLOW, NULL,
						 FSNOTIFY_EVENT_NONE, NULL, 0,
						 GFP_KERNEL);
	if (!q_overflow_event)
		panic("unable to allocate fsnotify q_overflow_event\n");

	return 0;
}
subsys_initcall(fsnotify_notification_init);
