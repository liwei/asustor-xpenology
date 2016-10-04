/*
 * Copyright (C) 2014 Synology Inc.  All rights reserved.
 */
#include <linux/rbtree.h>
#include <linux/btrfs.h>

#include "ctree.h"
#include "transaction.h"
#include "disk-io.h"
#include "locking.h"
#include "extent_io.h"
#include "qgroup.h"
#include "ulist.h"

#define USRQUOTA_RO_SUBVOL_EXIST_GEN 10

struct btrfs_usrquota {
	u64 uq_objectid;
	uid_t uq_uid;

	// info item
	u64 uq_generation;
	u64 uq_rfer_used;
	// limit item
	u64 uq_rfer_soft;
	u64 uq_rfer_hard;
	// reservation tracking
	u64 uq_reserved;

	struct list_head uq_dirty;
	u64 uq_refcnt; // accurate on dummy node only

	// tree of userquota
	struct rb_node uq_node;
};

struct btrfs_subvol_list  {
	u64 subvol_id;
	struct list_head list;
};

#define set_inconsistent(fs_info) \
	do {\
		if (!(fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_INCONSISTENT)) {\
			btrfs_err(fs_info, "set inconsistent");\
		}\
		fs_info->usrquota_flags |= BTRFS_USRQUOTA_STATUS_FLAG_INCONSISTENT;\
	} while (0)\

static int usrquota_rescan_init(struct btrfs_fs_info *fs_info, u64 rootid, u64 objectid);
static int usrquota_rescan_check(struct btrfs_fs_info *fs_info, u64 rootid, u64 objectid);

static int usrquota_subtree_unload(struct btrfs_fs_info *fs_info, u64 rootid);
static int usrquota_subtree_unload_nolock(struct btrfs_fs_info *fs_info, u64 rootid);
static int usrquota_subtree_load(struct btrfs_fs_info *fs_info, u64 rootid);

static u64 find_next_valid_objectid(struct btrfs_fs_info *fs_info, u64 objectid)
{
	struct rb_node *node = fs_info->usrquota_tree.rb_node;
	struct btrfs_usrquota *usrquota;
	u64 valid_objectid = 0;

	while (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		if (usrquota->uq_objectid > objectid) {
			valid_objectid = usrquota->uq_objectid;
			node = node->rb_left;
		} else if (usrquota->uq_objectid < objectid)
			node = node->rb_right;
		 else
			break;
	}
	if (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		valid_objectid = usrquota->uq_objectid;
	}
	return valid_objectid;
}

/*
 * *_usrquota_rb should protected by usrquota_lock
 */
static struct rb_node *find_usrquota_first_rb(struct btrfs_fs_info *fs_info,
                                            u64 objectid)
{
	struct rb_node *node = fs_info->usrquota_tree.rb_node;
	struct rb_node *found = NULL;
	struct btrfs_usrquota *usrquota;

again:
	while (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		if (usrquota->uq_objectid > objectid)
			node = node->rb_left;
		else if (usrquota->uq_objectid < objectid)
			node = node->rb_right;
		else
			break;
	}
	if (!node)
		goto out;
	found = node;
	while (node->rb_left) {
		usrquota = rb_entry(node->rb_left, struct btrfs_usrquota, uq_node);
		node = node->rb_left;
		if (usrquota->uq_objectid != objectid)
			goto again;
		found = node;
	}
out:
	return found;
}

static struct btrfs_usrquota *find_usrquota_rb(struct btrfs_fs_info *fs_info,
                                               u64 objectid, uid_t uid)
{
	struct rb_node *node = fs_info->usrquota_tree.rb_node;
	struct btrfs_usrquota *usrquota;

	while (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		if (usrquota->uq_objectid > objectid)
			node = node->rb_left;
		else if (usrquota->uq_objectid < objectid)
			node = node->rb_right;
		else if (usrquota->uq_uid > uid)
			node = node->rb_left;
		else if (usrquota->uq_uid < uid)
			node = node->rb_right;
		else
			return usrquota;
	}
	return NULL;
}

static struct btrfs_usrquota *__add_usrquota_rb(struct btrfs_fs_info *fs_info,
                                                u64 objectid, uid_t uid, int subtree_check)
{
	struct rb_node **p = &fs_info->usrquota_tree.rb_node;
	struct rb_node *parent = NULL;
	struct btrfs_usrquota *usrquota;
	struct btrfs_root *subvol_root;
	struct btrfs_key key;
	int subtree_loaded = 0;

	while (*p) {
		parent = *p;
		usrquota = rb_entry(parent, struct btrfs_usrquota, uq_node);

		if (usrquota->uq_objectid > objectid)
			p = &(*p)->rb_left;
		else if (usrquota->uq_objectid < objectid)
			p = &(*p)->rb_right;
		else if (usrquota->uq_uid > uid) {
			subtree_loaded = 1;
			p = &(*p)->rb_left;
		} else if (usrquota->uq_uid < uid) {
			subtree_loaded = 1;
			p = &(*p)->rb_right;
		} else
			return usrquota;
	}

	if (subtree_check && !subtree_loaded) {
		key.objectid = objectid;
		key.type = BTRFS_ROOT_ITEM_KEY;
		key.offset = (u64)-1;

		subvol_root = btrfs_read_fs_root_no_name(fs_info, &key);
		if (IS_ERR(subvol_root)) {
			btrfs_err(fs_info, "Failed to get subvol from rootid[%llu].", objectid);
		} else {
			btrfs_debug(fs_info, "Usrquota subtree for rootid [%llu], read-only[%d], is not loaded.",
			    objectid, btrfs_root_readonly(subvol_root));
		}
		return ERR_PTR(-ENOENT);
	}

	usrquota = kzalloc(sizeof(*usrquota), GFP_ATOMIC);
	if (!usrquota)
		return ERR_PTR(-ENOMEM);

	usrquota->uq_objectid = objectid;
	usrquota->uq_uid = uid;
	INIT_LIST_HEAD(&usrquota->uq_dirty);

	rb_link_node(&usrquota->uq_node, parent, p);
	rb_insert_color(&usrquota->uq_node, &fs_info->usrquota_tree);

	return usrquota;
}

/*
 * Caller of add_usrquota_rb[_no_check] should hold usrquota_lock
 */
static inline struct btrfs_usrquota *add_usrquota_rb_nocheck(struct btrfs_fs_info *fs_info,
                                                    u64 objectid, uid_t uid)
{
	return __add_usrquota_rb(fs_info, objectid, uid, 0);
}

static inline struct btrfs_usrquota *add_usrquota_rb(struct btrfs_fs_info *fs_info,
                                                    u64 objectid, uid_t uid)
{
	return __add_usrquota_rb(fs_info, objectid, uid, 1);
}

static int del_usrquota_rb(struct btrfs_fs_info *fs_info, struct btrfs_usrquota *usrquota)
{
	rb_erase(&usrquota->uq_node, &fs_info->usrquota_tree);
	list_del(&usrquota->uq_dirty);
	kfree(usrquota);
	return 0;
}

/*
 * Caller of usrquota_subtree_load has resposible to call usrquota_subtree_unload
 * to dec reference count except btrfs_usrquota_load_config
 */
static int usrquota_subtree_load(struct btrfs_fs_info *fs_info, u64 rootid)
{
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_root *usrquota_root = fs_info->usrquota_root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_usrquota_info_item *info_item;
	struct btrfs_usrquota_limit_item *limit_item;
	struct btrfs_usrquota *usrquota;
	int slot;
	int ret = 0;
	struct rb_node *node;

	spin_lock(&fs_info->usrquota_lock);
	node = find_usrquota_first_rb(fs_info, rootid);
	if (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		usrquota->uq_refcnt++;
		spin_unlock(&fs_info->usrquota_lock);
		return 0;
	}
	// insert a dummy node to identify if subtree is loaded or not
	usrquota = add_usrquota_rb_nocheck(fs_info, rootid, 0);
	if (IS_ERR(usrquota)) {
		spin_unlock(&fs_info->usrquota_lock);
		return PTR_ERR(usrquota);
	}
	usrquota->uq_refcnt = 1;
	spin_unlock(&fs_info->usrquota_lock);

	path = btrfs_alloc_path();
	if (!path) {
		return -ENOMEM;
	}

	key.objectid = rootid;
	key.type = 0;
	key.offset = 0;
	ret = btrfs_search_slot_for_read(usrquota_root, &key, path, 1, 0);
	if (ret < 0)
		goto out;
	if (ret) {
		ret = 0;
		goto out;
	}
	while (1) {
		slot = path->slots[0];
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, slot);
		if (found_key.objectid > rootid)
			break;
		if (found_key.type != BTRFS_USRQUOTA_INFO_KEY &&
		    found_key.type != BTRFS_USRQUOTA_LIMIT_KEY)
			goto next_item;

		spin_lock(&fs_info->usrquota_lock);
		usrquota = add_usrquota_rb_nocheck(fs_info, found_key.objectid, (uid_t)found_key.offset);
		if (IS_ERR(usrquota)) {
			spin_unlock(&fs_info->usrquota_lock);
			ret = PTR_ERR(usrquota);
			goto out;
		}
		spin_unlock(&fs_info->usrquota_lock);

		switch (found_key.type) {
		case BTRFS_USRQUOTA_INFO_KEY:
			info_item = btrfs_item_ptr(leaf, slot,
					           struct btrfs_usrquota_info_item);
			usrquota->uq_generation = btrfs_usrquota_info_generation(leaf, info_item);
			usrquota->uq_rfer_used = btrfs_usrquota_info_rfer_used(leaf, info_item);
			break;
		case BTRFS_USRQUOTA_LIMIT_KEY:
			limit_item = btrfs_item_ptr(leaf, slot,
					            struct btrfs_usrquota_limit_item);
			usrquota->uq_rfer_soft = btrfs_usrquota_limit_rfer_soft(leaf, limit_item);
			usrquota->uq_rfer_hard = btrfs_usrquota_limit_rfer_hard(leaf, limit_item);
			break;
		}
next_item:
		ret = btrfs_next_item(usrquota_root, path);
		if (ret < 0) {
			btrfs_err(fs_info, "failed to get next_item of usrquota tree");
			goto out;
		}
		if (ret) {
			ret = 0;
			break;
		}
	}
out:
	if (ret) {
		 // refcnt is possible greater than 1,
		 // such that we use unload function to check refcnt
		usrquota_subtree_unload(fs_info, rootid);
	}
	btrfs_free_path(path);
	return ret;
}

static int usrquota_ro_subvol_check(struct btrfs_fs_info *fs_info, struct btrfs_root *root)
{
	int ret = 0;
	struct btrfs_root *cur;
	mutex_lock(&fs_info->usrquota_ro_roots_lock);
	if (!btrfs_root_readonly(root)) {
		goto out;
	}
	root->usrquota_loaded_gen = fs_info->generation;
	list_for_each_entry(cur, &fs_info->usrquota_ro_roots, usrquota_ro_root) {
		if (cur->objectid == root->objectid) {
			goto out;
		}
	}
	ret = usrquota_subtree_load(fs_info, root->objectid);
	if (ret) {
		btrfs_err(fs_info, "failed to load ro subvol usrquota subtree [%llu].", root->objectid);
		goto out;
	}
	btrfs_debug(fs_info, "Load ro sub [id:%llu, gen:%llu]", root->objectid,
		root->usrquota_loaded_gen);
	list_add_tail(&root->usrquota_ro_root, &fs_info->usrquota_ro_roots);
out:
	mutex_unlock(&fs_info->usrquota_ro_roots_lock);
	return ret;
}

static int usrquota_subtree_unload_nolock(struct btrfs_fs_info *fs_info, u64 rootid)
{
	struct btrfs_usrquota *usrquota;
	struct rb_node *node;

	node = find_usrquota_first_rb(fs_info, rootid);
	if (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		if (usrquota->uq_refcnt > 1) {
			usrquota->uq_refcnt--;
			goto out;
		}
	}
	while (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		node = rb_next(node);
		if (usrquota->uq_objectid > rootid)
			break;
		del_usrquota_rb(fs_info, usrquota);
	}
out:
	return 0;
}

static int usrquota_subtree_unload(struct btrfs_fs_info *fs_info, u64 rootid)
{
	spin_lock(&fs_info->usrquota_lock);
	usrquota_subtree_unload_nolock(fs_info, rootid);
	spin_unlock(&fs_info->usrquota_lock);
	return 0;
}

static int usrquota_subtree_load_one(struct btrfs_fs_info *fs_info, struct btrfs_path *path, struct list_head *subvol_queue, u64 subvol_id)
{
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_root *subvol_root;
	struct extent_buffer *leaf;
	struct btrfs_subvol_list *subvol_list;
	int slot;
	int ret = 0;

	key.objectid = subvol_id;
	key.type = BTRFS_ROOT_ITEM_KEY;
	key.offset = (u64)-1;

	subvol_root = btrfs_read_fs_root_no_name(fs_info, &key);
	if (IS_ERR(subvol_root)) {
		ret = PTR_ERR(subvol_root);
		goto out;
	}
	if (btrfs_root_readonly(subvol_root))
		goto update_queue;

	ret = usrquota_subtree_load(fs_info, subvol_id);
	if (ret) {
		goto out;
	}

update_queue:
	if (btrfs_root_noload_usrquota(subvol_root))
		goto out;

	key.objectid = subvol_id;
	key.type = BTRFS_ROOT_REF_KEY;
	key.offset = 0;
	ret = btrfs_search_slot_for_read(fs_info->tree_root, &key, path, 1, 0);
	if (ret < 0)
		goto out;
	if (ret > 0) {
		ret = 0; //no entry
		goto out;
	}
	while (1) {
		slot = path->slots[0];
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, slot);
		if (found_key.type != BTRFS_ROOT_REF_KEY)
			break;

		key.objectid = found_key.offset;
		key.type = BTRFS_ROOT_ITEM_KEY;
		key.offset = (u64)-1;

		subvol_root = btrfs_read_fs_root_no_name(fs_info, &key);
		if (IS_ERR(subvol_root)) {
			ret = PTR_ERR(subvol_root);
			goto out;
		}

		subvol_list = kzalloc(sizeof(*subvol_list), GFP_NOFS);
		if (!subvol_list) {
			ret = -ENOMEM;
			goto out;
		}
		INIT_LIST_HEAD(&subvol_list->list);
		subvol_list->subvol_id = key.objectid;
		list_add_tail(&subvol_list->list, subvol_queue);
		ret = btrfs_next_item(fs_info->tree_root, path);
		if (ret < 0)
			goto out;
		if (ret > 0) {
			ret = 0; //no entry
			goto out;
		}
	}
out:
	btrfs_release_path(path);
	return ret;
}

static int usrquota_subtree_load_all(struct btrfs_fs_info *fs_info)
{
	struct btrfs_path *path = NULL;
	struct list_head subvol_queue;
	struct btrfs_subvol_list *subvol_list;
	int ret = 0;

	INIT_LIST_HEAD(&subvol_queue);
	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	subvol_list = kzalloc(sizeof(*subvol_list), GFP_NOFS);
	if (!subvol_list) {
		ret = -ENOMEM;
		goto out;
	}
	INIT_LIST_HEAD(&subvol_list->list);
	subvol_list->subvol_id = BTRFS_FS_TREE_OBJECTID;
	list_add_tail(&subvol_list->list, &subvol_queue);

	while (!list_empty(&subvol_queue)) {
		subvol_list = list_first_entry(&subvol_queue, struct btrfs_subvol_list, list);
		list_del_init(&subvol_list->list);
		if (!ret) {
			ret = usrquota_subtree_load_one(fs_info, path, &subvol_queue, subvol_list->subvol_id);
			if (ret) {
				btrfs_err(fs_info, "failed to load usrquota subtree %llu, ret=%d", subvol_list->subvol_id, ret);
			}
		}
		kfree(subvol_list);
	}
out:
	btrfs_free_path(path);
	return ret;
}

int btrfs_read_usrquota_config(struct btrfs_fs_info *fs_info)
{
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_root *usrquota_root = fs_info->usrquota_root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_usrquota_status_item *status_item;
	int slot;
	int ret = 0;
	u64 rescan_rootid = 0;
	u64 rescan_objectid = 0;

	if (!fs_info->usrquota_enabled)
		return 0;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	fs_info->usrquota_flags = 0;

	key.objectid = 0;
	key.type = BTRFS_USRQUOTA_STATUS_KEY;
	key.offset = 0;
	ret = btrfs_search_slot_for_read(usrquota_root, &key, path, 1, 0);
	if (ret)
		goto out;

	slot = path->slots[0];
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &found_key, slot);

	if (found_key.type != BTRFS_USRQUOTA_STATUS_KEY) {
		ret = -ENOENT;
		goto out;
	}

	status_item = btrfs_item_ptr(leaf, slot,
			     struct btrfs_usrquota_status_item);

	if (btrfs_usrquota_status_version(leaf, status_item) !=
	    BTRFS_USRQUOTA_STATUS_VERSION) {
		btrfs_err(fs_info, "old usrquota version, usrquota disabled");
		goto out;
	}
	if (btrfs_usrquota_status_generation(leaf, status_item) != fs_info->generation)
		set_inconsistent(fs_info);

	fs_info->usrquota_flags |= btrfs_usrquota_status_flags(leaf, status_item);
	rescan_rootid = btrfs_usrquota_status_rescan_rootid(leaf, status_item);
	rescan_objectid = btrfs_usrquota_status_rescan_objectid(leaf, status_item);
	btrfs_release_path(path);

	ret = usrquota_subtree_load_all(fs_info);
out:
	if (ret) {
		btrfs_err(fs_info, "usrquota disabled due to faield to load tree\n");
		fs_info->usrquota_flags &= ~BTRFS_USRQUOTA_STATUS_FLAG_ON;
	}

	if (!(fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_ON)) {
		fs_info->usrquota_enabled = 0;
		fs_info->pending_usrquota_state = 0;
	} else if (fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_RESCAN && !ret) {
		ret = usrquota_rescan_init(fs_info, rescan_rootid, rescan_objectid);
	}

	if (ret) {
		fs_info->usrquota_flags &= ~BTRFS_USRQUOTA_STATUS_FLAG_RESCAN;
	}
	btrfs_free_path(path);
	return ret;
}

void btrfs_free_usrquota_config(struct btrfs_fs_info *fs_info)
{
	struct rb_node *node;
	struct btrfs_usrquota *usrquota;

	while ((node = rb_first(&fs_info->usrquota_tree))) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		del_usrquota_rb(fs_info, usrquota);
	}
}

/*
 *  Helper function called when usrquota_info_item cnt or usrquota_limit_item cnt for a root
 *  has been changed. The caller must hold usrquota_tree_lock.
 */
static int update_usrquota_root_item(struct btrfs_trans_handle *trans,
                                      struct btrfs_fs_info *fs_info, struct btrfs_path *path,
                                      u64 objectid, int info_item_diff, int limit_item_diff)
{
	struct btrfs_key key;
	struct extent_buffer *leaf = NULL;
	struct btrfs_usrquota_root_item *usrquota_root = NULL;
	u64 info_item_cnt;
	u64 limit_item_cnt;
	int ret;

	key.objectid = objectid;
	key.type = BTRFS_USRQUOTA_ROOT_KEY;
	key.offset = 0;
	ret = btrfs_insert_empty_item(trans, fs_info->usrquota_root, path, &key,
                                  sizeof(struct btrfs_usrquota_root_item));
	if (ret && ret != -EEXIST)
		goto out;

	leaf = path->nodes[0];
	usrquota_root = btrfs_item_ptr(leaf, path->slots[0],
                                   struct btrfs_usrquota_root_item);
	if (ret == -EEXIST) {
		info_item_cnt = btrfs_usrquota_root_info_item_cnt(leaf, usrquota_root);
		limit_item_cnt = btrfs_usrquota_root_limit_item_cnt(leaf, usrquota_root);
		info_item_cnt += info_item_diff;
		limit_item_cnt += limit_item_diff;
	} else {
		info_item_cnt = info_item_diff;
		limit_item_cnt = limit_item_diff;
	}

	if (info_item_cnt > (1ULL << 63) || limit_item_cnt > (1ULL << 63)) {
		WARN_ON(1);
		ret = -ERANGE;
		goto out;
	}

	btrfs_set_usrquota_root_info_item_cnt(leaf, usrquota_root, info_item_cnt);
	btrfs_set_usrquota_root_limit_item_cnt(leaf, usrquota_root, limit_item_cnt);
	btrfs_mark_buffer_dirty(leaf);
	ret = 0;
out:
	btrfs_release_path(path);
	return ret;
}

static int remove_usrquota_limit_item(struct btrfs_trans_handle *trans,
				      struct btrfs_fs_info *fs_info, u64 rootid, u64 uid)
{
	int ret = 0;
	struct btrfs_root *usrquota_root = fs_info->usrquota_root;
	struct btrfs_path *path;
	struct btrfs_key key;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	mutex_lock(&fs_info->usrquota_tree_lock);

	key.objectid = rootid;
	key.offset = uid;
	key.type = BTRFS_USRQUOTA_LIMIT_KEY;
	path->leave_spinning = 1;
	ret = btrfs_search_slot(trans, usrquota_root, &key, path, -1, 1);
	if (ret) {
		ret = 0;
		goto out;
	}
	ret = btrfs_del_item(trans, usrquota_root, path);
	if (ret) {
		printk(KERN_ERR "failed to delete limit item, rootid=%llu, uid=%llu\n", rootid, uid);
		goto out;
	}

	btrfs_release_path(path);
	ret = update_usrquota_root_item(trans, fs_info, path, rootid, 0, -1);
	if (ret) {
		printk(KERN_ERR "failed to dec limit item cnt, rootid=%llu, uid=%llu\n", rootid, uid);
		goto out;
	}
out:
	mutex_unlock(&fs_info->usrquota_tree_lock);
	btrfs_free_path(path);
	return ret;
}

static int update_usrquota_limit_item(struct btrfs_trans_handle *trans,
                                      struct btrfs_fs_info *fs_info,
                                      u64 objectid, uid_t uid, u64 rfer_soft, u64 rfer_hard)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_usrquota_limit_item *usrquota_limit;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	mutex_lock(&fs_info->usrquota_tree_lock);
	key.objectid = objectid;
	key.type = BTRFS_USRQUOTA_LIMIT_KEY;
	key.offset = uid;
	ret = btrfs_insert_empty_item(trans, fs_info->usrquota_root, path, &key,
	                              sizeof(struct btrfs_usrquota_limit_item));
	if (ret && ret != -EEXIST)
		goto out;

	leaf = path->nodes[0];
	usrquota_limit = btrfs_item_ptr(leaf, path->slots[0],
	                                struct btrfs_usrquota_limit_item);
	btrfs_set_usrquota_limit_rfer_soft(leaf, usrquota_limit, rfer_soft);
	btrfs_set_usrquota_limit_rfer_hard(leaf, usrquota_limit, rfer_hard);
	btrfs_mark_buffer_dirty(leaf);

	if (ret == 0) {
		btrfs_release_path(path);
		ret = update_usrquota_root_item(trans, fs_info, path, objectid, 0, 1);
	} else // ret == -EEXIST
		ret = 0;
out:
	mutex_unlock(&fs_info->usrquota_tree_lock);
	btrfs_free_path(path);
	return ret;
}

static int update_usrquota_info_item(struct btrfs_trans_handle *trans,
                                     struct btrfs_fs_info *fs_info,
                                     u64 objectid, uid_t uid, u64 rfer_used)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_usrquota_info_item *usrquota_info;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	mutex_lock(&fs_info->usrquota_tree_lock);
	key.objectid = objectid;
	key.type = BTRFS_USRQUOTA_INFO_KEY;
	key.offset = uid;
	ret = btrfs_insert_empty_item(trans, fs_info->usrquota_root, path, &key,
                                      sizeof(struct btrfs_usrquota_info_item));
	if (ret && ret != -EEXIST)
		goto out;

	leaf = path->nodes[0];
	usrquota_info = btrfs_item_ptr(leaf, path->slots[0],
	                               struct btrfs_usrquota_info_item);
	btrfs_set_usrquota_info_rfer_used(leaf, usrquota_info, rfer_used);
	btrfs_set_usrquota_info_generation(leaf, usrquota_info, trans->transid);
	btrfs_mark_buffer_dirty(leaf);

	if (ret == 0) {
		btrfs_release_path(path);
		ret = update_usrquota_root_item(trans, fs_info, path, objectid, 1, 0);
	} else // ret == -EEXIST
		ret = 0;
out:
	mutex_unlock(&fs_info->usrquota_tree_lock);
	btrfs_free_path(path);
	return ret;
}

static int update_usrquota_status_item(struct btrfs_trans_handle *trans,
                                       struct btrfs_fs_info *fs_info,
                                       struct btrfs_root *root)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_usrquota_status_item *ptr;
	int ret;
	int slot;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = 0;
	key.type = BTRFS_USRQUOTA_STATUS_KEY;
	key.offset = 0;
	ret = btrfs_search_slot(trans, root, &key, path, 0, 1);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		goto out;
	}

	leaf = path->nodes[0];
	slot = path->slots[0];
	ptr = btrfs_item_ptr(leaf, slot, struct btrfs_usrquota_status_item);
	btrfs_set_usrquota_status_flags(leaf, ptr, fs_info->usrquota_flags);
	btrfs_set_usrquota_status_generation(leaf, ptr, trans->transid);
	btrfs_set_usrquota_status_rescan_rootid(leaf, ptr, fs_info->usrquota_rescan_rootid);
	btrfs_set_usrquota_status_rescan_objectid(leaf, ptr, fs_info->usrquota_rescan_objectid);
	btrfs_mark_buffer_dirty(leaf);

out:
	btrfs_free_path(path);
	return ret;
}

static int btrfs_clean_usrquota_tree(struct btrfs_trans_handle *trans,
                                     struct btrfs_root *root)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	struct extent_buffer *leaf;
	int ret;
	int nr = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->leave_spinning = 1;
	key.objectid = 0;
	key.offset = 0;
	key.type = 0;

	while (1) {
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0)
			goto out;
		leaf = path->nodes[0];
		nr = btrfs_header_nritems(leaf);
		if (!nr)
			break;
		path->slots[0] = 0;
		ret = btrfs_del_items(trans, root, path, 0, nr);
		if (ret)
			goto out;

		btrfs_release_path(path);
	}
	ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

int btrfs_usrquota_dumptree(struct btrfs_fs_info *fs_info)
{
	int ret = 0;
	struct rb_node *node;
	struct btrfs_usrquota *usrquota;

	if (!fs_info->usrquota_enabled)
		return 0;

	spin_lock(&fs_info->usrquota_lock);
	printk(KERN_INFO "=====================\n");
	node= rb_first(&fs_info->usrquota_tree);
	while (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);

		node = rb_next(node);
		printk(KERN_INFO "==========\n");
		printk(KERN_INFO "objectid:%llu, uid:%u\n", usrquota->uq_objectid, usrquota->uq_uid);
		printk(KERN_INFO "generation:%llu, rfer_used:%llu, rfer_soft:%llu, rfer_hard:%llu\n",
		       usrquota->uq_generation, usrquota->uq_rfer_used, usrquota->uq_rfer_soft, usrquota->uq_rfer_hard);
	}
	spin_unlock(&fs_info->usrquota_lock);
	return ret;
}

int btrfs_usrquota_enable(struct btrfs_trans_handle *trans,
                          struct btrfs_fs_info *fs_info)
{
	struct btrfs_root *usrquota_root;
	struct btrfs_path *path = NULL;
	struct btrfs_usrquota_status_item *ptr;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	int ret = 0;

	mutex_lock(&fs_info->usrquota_ioctl_lock);
	if (fs_info->usrquota_root) {
		fs_info->pending_usrquota_state = 1;
		goto out;
	}

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&fs_info->usrquota_tree_lock);
	usrquota_root = btrfs_create_tree(trans, fs_info,
	                                  BTRFS_USRQUOTA_TREE_OBJECTID);
	if (IS_ERR(usrquota_root)) {
		ret = PTR_ERR(usrquota_root);
		mutex_unlock(&fs_info->usrquota_tree_lock);
		goto out;
	}

	key.objectid = 0;
	key.type = BTRFS_USRQUOTA_STATUS_KEY;
	key.offset = 0;
	ret = btrfs_insert_empty_item(trans, usrquota_root, path, &key, sizeof(*ptr));
	if (ret) {
		mutex_unlock(&fs_info->usrquota_tree_lock);
		goto out_free_root;
	}

	leaf = path->nodes[0];
	ptr = btrfs_item_ptr(leaf, path->slots[0],
	                     struct btrfs_usrquota_status_item);
	btrfs_set_usrquota_status_generation(leaf, ptr, trans->transid);
	btrfs_set_usrquota_status_version(leaf, ptr, BTRFS_USRQUOTA_STATUS_VERSION);
	fs_info->usrquota_flags = BTRFS_USRQUOTA_STATUS_FLAG_ON;
	btrfs_set_usrquota_status_flags(leaf, ptr, fs_info->usrquota_flags);
	btrfs_set_usrquota_status_rescan_rootid(leaf, ptr, 0);
	btrfs_set_usrquota_status_rescan_objectid(leaf, ptr, (u64)-1);

	btrfs_mark_buffer_dirty(leaf);

	mutex_unlock(&fs_info->usrquota_tree_lock);

	spin_lock(&fs_info->usrquota_lock);
	fs_info->usrquota_root = usrquota_root;
	spin_unlock(&fs_info->usrquota_lock);

	ret = usrquota_subtree_load_all(fs_info);
	if (ret) {
		btrfs_err(fs_info, "failed to init usrquota subtree during enable usrquota");
		goto out_free_root;
	}

	spin_lock(&fs_info->usrquota_lock);
	fs_info->pending_usrquota_state = 1;
	spin_unlock(&fs_info->usrquota_lock);

out_free_root:
	if (ret) {
		spin_lock(&fs_info->usrquota_lock);
		fs_info->usrquota_root = NULL;
		spin_unlock(&fs_info->usrquota_lock);
		free_extent_buffer(usrquota_root->node);
		free_extent_buffer(usrquota_root->commit_root);
		kfree(usrquota_root);
	}
out:
	btrfs_free_path(path);
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
	return ret;
}

int btrfs_usrquota_disable(struct btrfs_trans_handle *trans,
                           struct btrfs_fs_info *fs_info)
{
	struct btrfs_root *tree_root = fs_info->tree_root;
	struct btrfs_root *usrquota_root;
	int ret = 0;

	mutex_lock(&fs_info->usrquota_ioctl_lock);
	if (!fs_info->usrquota_root)
		goto out;
	spin_lock(&fs_info->usrquota_lock);
	fs_info->usrquota_enabled = 0;
	fs_info->pending_usrquota_state = 0;
	usrquota_root = fs_info->usrquota_root;
	fs_info->usrquota_root = NULL;
	btrfs_free_usrquota_config(fs_info);
	spin_unlock(&fs_info->usrquota_lock);

	mutex_lock(&fs_info->usrquota_tree_lock);
	ret = btrfs_clean_usrquota_tree(trans, usrquota_root);
	if (ret)
		goto unlock;

	ret = btrfs_del_root(trans, tree_root, &usrquota_root->root_key);
	if (ret)
		goto unlock;
	list_del(&usrquota_root->dirty_list);

	btrfs_tree_lock(usrquota_root->node);
	clean_tree_block(trans, tree_root, usrquota_root->node);
	btrfs_tree_unlock(usrquota_root->node);
	btrfs_free_tree_block(trans, usrquota_root, usrquota_root->node, 0, 1);

	free_extent_buffer(usrquota_root->node);
	free_extent_buffer(usrquota_root->commit_root);
	kfree(usrquota_root);
unlock:
	mutex_unlock(&fs_info->usrquota_tree_lock);
out:
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
	return ret;
}

/*
 * This function should protected by usrquota_lock
 */
static void usrquota_dirty(struct btrfs_fs_info *fs_info,
                           struct btrfs_usrquota *usrquota)
{
	if (list_empty(&usrquota->uq_dirty))
		list_add(&usrquota->uq_dirty, &fs_info->usrquota_dirty);
}

int btrfs_usrquota_limit(struct btrfs_trans_handle *trans,
                         struct btrfs_fs_info *fs_info,
                         u64 objectid, u64 uid, u64 rfer_soft, u64 rfer_hard)
{
	struct btrfs_usrquota *usrquota;
	int ret = 0;

	mutex_lock(&fs_info->usrquota_ioctl_lock);
	if (!fs_info->usrquota_root) {
		ret = -EINVAL;
		goto out;
	}

	spin_lock(&fs_info->usrquota_lock);
	usrquota = add_usrquota_rb(fs_info, objectid, uid);
	if (IS_ERR(usrquota)) {
		spin_unlock(&fs_info->usrquota_lock);
		ret = PTR_ERR(usrquota);
		goto out;
	}
	usrquota->uq_rfer_soft = rfer_soft;
	usrquota->uq_rfer_hard = rfer_hard;
	spin_unlock(&fs_info->usrquota_lock);

	ret = update_usrquota_limit_item(trans, fs_info,
	                                 objectid, uid, rfer_soft, rfer_hard);
	if (ret) {
		btrfs_err(fs_info, "failed to update limit item");
		goto out;
	}
out:
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
	return ret;
}

int btrfs_usrquota_clean(struct btrfs_trans_handle *trans,
                         struct btrfs_fs_info *fs_info, u64 uid)
{
	struct btrfs_usrquota *usrquota;
	int ret = 0;
	u64 objectid = 0;

	if (!fs_info->usrquota_enabled)
		return 0;
	mutex_lock(&fs_info->usrquota_ioctl_lock);
	if (!fs_info->usrquota_root) {
		ret = -EINVAL;
		goto out;
	}
	while (1) {
		spin_lock(&fs_info->usrquota_lock);
		objectid = find_next_valid_objectid(fs_info, objectid);
		if (!objectid) {
			spin_unlock(&fs_info->usrquota_lock);
			break;
		}
		usrquota = find_usrquota_rb(fs_info, objectid, uid);
		if (usrquota) {
			printk(KERN_ERR "find usrquota, objectid=%llu, uid=%u\n", usrquota->uq_objectid, usrquota->uq_uid);
			usrquota->uq_rfer_soft = 0;
			usrquota->uq_rfer_hard = 0;
			spin_unlock(&fs_info->usrquota_lock);
			ret = remove_usrquota_limit_item(trans, fs_info, objectid, uid);
			if (ret) {
				btrfs_err(fs_info, "failed to remove limit item");
				break;
			}
		} else {
			spin_unlock(&fs_info->usrquota_lock);
		}
		objectid++;
	}
out:
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
	return ret;
}

/*
 * btrfs_usrquota_account_ref is called for every ref that is added to or deleted
 * from the fs.
 */
int btrfs_usrquota_account_ref(struct btrfs_trans_handle *trans,
                               struct btrfs_fs_info *fs_info,
                               struct btrfs_qgroup_operation *oper)
{
	u64 rootid;
	u64 objectid;
	int ret = 0;
	int sgn;
	struct btrfs_usrquota *usrquota = NULL;
	uid_t uid = 0;
	struct btrfs_key key;
	struct btrfs_root *root;

	if (!fs_info->usrquota_enabled)
		return 0;

	BUG_ON(!fs_info->usrquota_root);

	if (oper->ref_type != BTRFS_EXTENT_DATA_REF_KEY &&
	    oper->ref_type != BTRFS_SHARED_DATA_REF_KEY) {
		return 0;
	}

	rootid= oper->ref_root;
	if (!is_fstree(rootid)) {
		return 0;
	}

	objectid = oper->objectid;
	if (objectid < BTRFS_FIRST_FREE_OBJECTID) {
		return 0;
	}

	switch (oper->type) {
	case BTRFS_QGROUP_OPER_ADD_EXCL:
	case BTRFS_QGROUP_OPER_ADD_SHARED:
		sgn = 1;
		break;
	case BTRFS_QGROUP_OPER_SUB_EXCL:
	case BTRFS_QGROUP_OPER_SUB_SHARED:
		sgn = -1;
		break;
	default:
		BUG();
	}

	if (usrquota_rescan_check(fs_info, rootid, objectid)) {
		return 0;
	}

	key.objectid = rootid;
	key.type = BTRFS_ROOT_ITEM_KEY;
	key.offset = (u64)-1;

	root = btrfs_read_fs_root_no_name(fs_info, &key);
	if (IS_ERR(root)) {
		if (PTR_ERR(root) != -ENOENT)
			ret = PTR_ERR(root);
		goto out;
	}

	if (!oper->is_uid_valid) {
		key.objectid = objectid;
		key.type = BTRFS_INODE_ITEM_KEY;
		key.offset = 0;

		// Can't use btrfs_search_slot to get inode_item due to delayed inode item
		ret = btrfs_iget_uid(fs_info->sb, &key, root, &uid);
		if (ret) {
			btrfs_err(fs_info,"failed to get uid. [err:%d]", ret);
			goto out;
		}
	} else {
		uid = oper->uid;
	}

	usrquota_ro_subvol_check(fs_info, root);
	spin_lock(&fs_info->usrquota_lock);
	usrquota = add_usrquota_rb(fs_info, rootid, uid);
	if (IS_ERR(usrquota)) {
		ret = PTR_ERR(usrquota);
		goto unlock;
	}
	btrfs_debug(fs_info, "account_ref rootid %llu uid %u orig %llu num_bytes %lld",
	            rootid, uid, usrquota->uq_rfer_used, sgn * oper->num_bytes);

	if (unlikely(sgn == -1 && (usrquota->uq_rfer_used < oper->num_bytes))) {
		// underflow
		usrquota->uq_rfer_used = 0;
		WARN_ON_ONCE(1);
	} else {
		usrquota->uq_rfer_used += sgn * oper->num_bytes;
	}
	usrquota_dirty(fs_info, usrquota);
unlock:
	spin_unlock(&fs_info->usrquota_lock);
out:
	return ret;
}

/*
 * called from commit_transaction. Writes all changed usrquota to disk.
 */
int btrfs_run_usrquota(struct btrfs_trans_handle *trans,
                       struct btrfs_fs_info *fs_info)
{
	struct btrfs_root *usrquota_root = fs_info->usrquota_root;
	struct btrfs_root *subvol_root, *next;
	int ret = 0;

	if (!usrquota_root)
		goto out;
	fs_info->usrquota_enabled = fs_info->pending_usrquota_state;

	spin_lock(&fs_info->usrquota_lock);
	while (!list_empty(&fs_info->usrquota_dirty)) {
		struct btrfs_usrquota *usrquota;

		usrquota = list_first_entry(&fs_info->usrquota_dirty,
		                            struct btrfs_usrquota, uq_dirty);
		list_del_init(&usrquota->uq_dirty);
		spin_unlock(&fs_info->usrquota_lock);
		ret = update_usrquota_info_item(trans, fs_info, usrquota->uq_objectid,
		                                usrquota->uq_uid, usrquota->uq_rfer_used);
		if (ret)
			set_inconsistent(fs_info);
		spin_lock(&fs_info->usrquota_lock);
	}
	if (fs_info->usrquota_enabled)
		fs_info->usrquota_flags |= BTRFS_USRQUOTA_STATUS_FLAG_ON;
	else
		fs_info->usrquota_flags &= ~BTRFS_USRQUOTA_STATUS_FLAG_ON;
	spin_unlock(&fs_info->usrquota_lock);

	ret = update_usrquota_status_item(trans, fs_info, usrquota_root);
	if (ret)
		set_inconsistent(fs_info);
out:
	mutex_lock(&fs_info->usrquota_ro_roots_lock);
	spin_lock(&fs_info->usrquota_lock);
	list_for_each_entry_safe(subvol_root, next, &fs_info->usrquota_ro_roots, usrquota_ro_root) {
		if (subvol_root->usrquota_loaded_gen + USRQUOTA_RO_SUBVOL_EXIST_GEN < fs_info->generation) {
			list_del(&subvol_root->usrquota_ro_root);
			btrfs_debug(fs_info, "Unload ro sub [id:%llu] uq subtree [%llu, %llu]",
			          subvol_root->objectid, subvol_root->usrquota_loaded_gen, fs_info->generation);
			usrquota_subtree_unload_nolock(fs_info, subvol_root->objectid);
		}
	}
	spin_unlock(&fs_info->usrquota_lock);
	mutex_unlock(&fs_info->usrquota_ro_roots_lock);
	return ret;
}

int btrfs_usrquota_reserve(struct btrfs_root *root, uid_t uid, u64 num_bytes)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_usrquota *usrquota;
	u64 rootid = root->root_key.objectid;
	int ret = 0;

	if (!is_fstree(rootid))
		return 0;

	if (!fs_info->usrquota_enabled)
		return 0;

	usrquota_ro_subvol_check(fs_info, root);
	spin_lock(&fs_info->usrquota_lock);
	usrquota = add_usrquota_rb(fs_info, rootid, uid);
	if (IS_ERR(usrquota)) {
		ret = PTR_ERR(usrquota);
		goto out;
	}

	if (usrquota->uq_rfer_hard && !capable(CAP_SYS_RESOURCE)) {
		if (usrquota->uq_rfer_used + usrquota->uq_reserved + num_bytes > usrquota->uq_rfer_hard) {
			ret = -EDQUOT;
			goto out;
		}
	}
	usrquota->uq_reserved += num_bytes;
out:
	spin_unlock(&fs_info->usrquota_lock);
	return ret;
}

int btrfs_usrquota_free(struct btrfs_root *root, uid_t uid, u64 num_bytes)
{

	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_usrquota *usrquota;
	u64 rootid= root->root_key.objectid;

	if (!is_fstree(rootid))
		return 0;
	if (num_bytes == 0)
		return 0;

	spin_lock(&fs_info->usrquota_lock);
	if (!fs_info->usrquota_enabled)
		goto out;

	usrquota = find_usrquota_rb(fs_info, rootid, uid);
	if (!usrquota) {
		goto out;
	}
	usrquota->uq_reserved -= num_bytes;
out:
	spin_unlock(&fs_info->usrquota_lock);
	return 0;
}

static int usrquota_rescan_check(struct btrfs_fs_info *fs_info, u64 rootid, u64 objectid)
{
	int ret = 0;

	mutex_lock(&fs_info->usrquota_rescan_lock);
	if (fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_RESCAN) {
		if (fs_info->usrquota_rescan_rootid == rootid &&
		    fs_info->usrquota_rescan_objectid <= objectid) {
			ret = 1;
		}
	}
	mutex_unlock(&fs_info->usrquota_rescan_lock);
	return ret;
}

static int
usrquota_rescan_inode(struct btrfs_fs_info *fs_info, struct btrfs_path *path,
                      struct btrfs_trans_handle *trans,
                      struct extent_buffer *scratch_leaf)
{
	struct btrfs_key key;
	struct btrfs_key found;
	struct btrfs_root *rescan_root = fs_info->usrquota_rescan_root;
	struct extent_buffer *leaf;
	struct btrfs_file_extent_item *extent_item;
	struct btrfs_inode_item *inode_item;
	struct btrfs_usrquota *usrquota;

	u64 objectid;
	int slot;
	int ret = 0;

	u64 num_bytes = 0;
	uid_t uid;
	mode_t mode;

	if (fs_info->usrquota_rescan_objectid < BTRFS_FIRST_FREE_OBJECTID)
		objectid = BTRFS_FIRST_FREE_OBJECTID;
	else
		objectid = fs_info->usrquota_rescan_objectid + 1;
next_inode:
	key.objectid = objectid;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;
	path->leave_spinning = 1;

	// return 1, when no items
	ret = btrfs_search_slot_for_read(rescan_root, &key, path, 1, 0);
	if (ret) {
		btrfs_release_path(path);
		goto out;
	}
	slot = path->slots[0];
	leaf = path->nodes[0];

	btrfs_item_key_to_cpu(leaf, &found, slot);
	if (btrfs_key_type(&found) != BTRFS_INODE_ITEM_KEY) {
		btrfs_err(fs_info, "failed to find INODE_ITEM");
		ret = -ENOENT;
		btrfs_release_path(path);
		goto out;
	}
	objectid = found.objectid;
	inode_item = btrfs_item_ptr(leaf, slot, struct btrfs_inode_item);
	mode = btrfs_inode_mode(leaf, inode_item);
	if (S_ISDIR(mode)) {
		btrfs_release_path(path);
		objectid++;
		goto next_inode;
	}
	uid = btrfs_inode_uid(leaf, inode_item);

	btrfs_release_path(path);
	key.objectid = objectid;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = 0;
next_extents:
	btrfs_debug(fs_info, "seartch extent obj %llu offset %llu", key.objectid, key.offset);
	ret = btrfs_search_slot_for_read(rescan_root, &key, path, 1, 0);
	if (ret) {
		btrfs_release_path(path);
		if (num_bytes) {
			ret = 0;
			goto update;
		}
		goto out;
	}
	slot = path->slots[0];
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &found, slot);
	if (btrfs_key_type(&found) != BTRFS_EXTENT_DATA_KEY) {
		btrfs_release_path(path);
		if (num_bytes) {
			ret = 0;
			goto update;
		}
		objectid++;
		goto next_inode;
	}

	memcpy(scratch_leaf, path->nodes[0], sizeof(*scratch_leaf));
	slot = path->slots[0];
	btrfs_release_path(path);

	for (; slot < btrfs_header_nritems(scratch_leaf); ++slot) {
		btrfs_item_key_to_cpu(scratch_leaf, &found, slot);

		if (btrfs_key_type(&found) != BTRFS_EXTENT_DATA_KEY)
			break;

		extent_item  = btrfs_item_ptr(scratch_leaf, slot, struct btrfs_file_extent_item);
		if (btrfs_file_extent_type(scratch_leaf, extent_item) != BTRFS_FILE_EXTENT_INLINE) {
			num_bytes += btrfs_file_extent_disk_num_bytes(scratch_leaf, extent_item);
		}
	}

	if (btrfs_key_type(&found) == BTRFS_EXTENT_DATA_KEY) {
		key.offset = found.offset + 1;
		goto next_extents;
	}
	if (num_bytes == 0) {
		objectid++;
		goto next_inode;
	}

update:
	btrfs_debug(fs_info, "rescan_inode rootid %llu uid %u objectid %llu num_bytes %llu",
	            fs_info->usrquota_rescan_rootid, uid, objectid, num_bytes);

	spin_lock(&fs_info->usrquota_lock);
	usrquota = add_usrquota_rb(fs_info, fs_info->usrquota_rescan_rootid, uid);
	if (IS_ERR(usrquota)) {
		spin_unlock(&fs_info->usrquota_lock);
		ret = PTR_ERR(usrquota);
		goto out;
	}
	usrquota->uq_rfer_used += num_bytes;
	usrquota_dirty(fs_info, usrquota);
	spin_unlock(&fs_info->usrquota_lock);

	mutex_lock(&fs_info->usrquota_rescan_lock);
	fs_info->usrquota_rescan_objectid = objectid;
	mutex_unlock(&fs_info->usrquota_rescan_lock);
out:
	return ret;
}

static void btrfs_usrquota_rescan_work(struct btrfs_work *work)
{
	struct btrfs_fs_info *fs_info = container_of(work, struct btrfs_fs_info,
	                                             usrquota_rescan_work);
	struct btrfs_path *path;
	struct btrfs_trans_handle *trans = NULL;
	struct extent_buffer *scratch_leaf = NULL;
	int ret = -ENOMEM;

	path = btrfs_alloc_path();
	if (!path)
		goto out;
	scratch_leaf = kmalloc(sizeof(*scratch_leaf), GFP_NOFS);
	if (!scratch_leaf)
		goto out;

	ret = 0;
	while (!ret) {
		trans = btrfs_start_transaction(fs_info->fs_root, 1);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			break;
		}
		if (!fs_info->usrquota_enabled || !fs_info->usrquota_rescan_root) {
			ret = -EINTR;
		} else {
			ret = usrquota_rescan_inode(fs_info, path, trans, scratch_leaf);
		}
		if (ret > 0)
			btrfs_commit_transaction(trans, fs_info->fs_root);
		else
			btrfs_end_transaction(trans, fs_info->fs_root);
	}

out:
	kfree(scratch_leaf);
	btrfs_free_path(path);

	usrquota_subtree_unload(fs_info, fs_info->usrquota_rescan_rootid);
	mutex_lock(&fs_info->usrquota_rescan_lock);
	fs_info->usrquota_flags &= ~BTRFS_USRQUOTA_STATUS_FLAG_RESCAN;
	fs_info->usrquota_rescan_objectid = (u64)-1;
	if (ret < 0)
		set_inconsistent(fs_info);
	mutex_unlock(&fs_info->usrquota_rescan_lock);

	if (ret < 0) {
		btrfs_err(fs_info, "usrquota scan failed with %d", ret);
	} else {
		btrfs_info(fs_info, "usrquota scan completed");
	}
	complete_all(&fs_info->usrquota_rescan_completion);
}

static int usrquota_rescan_init(struct btrfs_fs_info *fs_info, u64 rootid, u64 objectid)
{
	int ret = 0;
	struct btrfs_root *root;
	struct btrfs_key key;

	key.objectid = rootid;
	key.type = BTRFS_ROOT_ITEM_KEY;
	key.offset = (u64)-1;

	root = btrfs_read_fs_root_no_name(fs_info, &key);
	if (IS_ERR(root)) {
		ret = PTR_ERR(root);
		goto out;
	}

	mutex_lock(&fs_info->usrquota_rescan_lock);
	spin_lock(&fs_info->usrquota_lock);

	if (fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_RESCAN)
		ret = -EINPROGRESS;
	else if (!(fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_ON))
		ret = -EINVAL;

	if (ret) {
		spin_unlock(&fs_info->usrquota_lock);
		mutex_unlock(&fs_info->usrquota_rescan_lock);
		goto out;
	}

	fs_info->usrquota_flags |= BTRFS_USRQUOTA_STATUS_FLAG_RESCAN;
	fs_info->usrquota_rescan_rootid = rootid;
	fs_info->usrquota_rescan_objectid = objectid;
	fs_info->usrquota_rescan_root = root;

	spin_unlock(&fs_info->usrquota_lock);
	mutex_unlock(&fs_info->usrquota_rescan_lock);

	init_completion(&fs_info->usrquota_rescan_completion);

	memset(&fs_info->usrquota_rescan_work, 0,
	       sizeof(fs_info->usrquota_rescan_work));
	btrfs_init_work(&fs_info->usrquota_rescan_work,
	                btrfs_usrquota_rescan_work, NULL, NULL);

out:
	if (ret) {
		btrfs_err(fs_info, "usrquota_rescan_init failed with %d", ret);
	}
	return ret;
}

int btrfs_usrquota_rescan(struct btrfs_fs_info *fs_info, u64 rootid)
{
	int ret = 0;
	int subtree_loaded = 0;
	struct btrfs_trans_handle *trans;
	struct btrfs_usrquota *usrquota;
	struct rb_node *node;

	ret = usrquota_rescan_init(fs_info, rootid, 0);
	if (ret)
		return ret;

	// flush existing delayed_ref
	trans = btrfs_join_transaction(fs_info->fs_root);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out;
	}
	ret = btrfs_commit_transaction(trans, fs_info->fs_root);
	if (ret) {
		goto out;
	}

	if (!fs_info->usrquota_enabled) {
		ret = -EINVAL;
		goto out;
	}
	ret = usrquota_subtree_load(fs_info, rootid);
	if (ret) {
		btrfs_err(fs_info, "failed to load usrquota subtree %llu", rootid);
		goto out;
	}
	subtree_loaded = 1;

	spin_lock(&fs_info->usrquota_lock);
	node = find_usrquota_first_rb(fs_info, rootid);
	while (node) {
		usrquota = rb_entry(node, struct btrfs_usrquota, uq_node);
		node = rb_next(node);
		if (usrquota->uq_objectid > rootid)
			break;
		usrquota->uq_rfer_used = 0;
		usrquota->uq_generation = 0;
		usrquota_dirty(fs_info, usrquota);
	}
	spin_unlock(&fs_info->usrquota_lock);

	// flush usrquota_info items
	trans = btrfs_start_transaction(fs_info->fs_root, 0);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out;
	}
	ret = btrfs_commit_transaction(trans, fs_info->fs_root);
	if (ret)
		goto out;

	btrfs_queue_work(fs_info->usrquota_rescan_workers,
	                 &fs_info->usrquota_rescan_work);
out:
	if (ret) {
		if (subtree_loaded)
			usrquota_subtree_unload(fs_info, rootid);
		fs_info->usrquota_flags &= ~BTRFS_USRQUOTA_STATUS_FLAG_RESCAN;
	}
	return ret;
}

int btrfs_usrquota_wait_for_completion(struct btrfs_fs_info *fs_info)
{
	int running;
	int ret = 0;

	mutex_lock(&fs_info->usrquota_rescan_lock);
	spin_lock(&fs_info->usrquota_lock);
	running = fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_RESCAN;
	spin_unlock(&fs_info->usrquota_lock);
	mutex_unlock(&fs_info->usrquota_rescan_lock);

	if (running) {
		ret = wait_for_completion_interruptible(
		          &fs_info->usrquota_rescan_completion);
	}
	return ret;
}

void btrfs_usrquota_rescan_resume(struct btrfs_fs_info *fs_info)
{
	int ret;

	if (fs_info->usrquota_flags & BTRFS_USRQUOTA_STATUS_FLAG_RESCAN) {
		ret = usrquota_subtree_load(fs_info, fs_info->usrquota_rescan_rootid);
		if (ret) {
			btrfs_err(fs_info, "rescan resume failed due to failed to load usrquota");
			fs_info->usrquota_flags &= ~BTRFS_USRQUOTA_STATUS_FLAG_RESCAN;
		} else {
			btrfs_queue_work(fs_info->usrquota_rescan_workers,
					 &fs_info->usrquota_rescan_work);
		}
	}
}

int btrfs_usrquota_transfer(struct inode *inode, uid_t new_uid)
{
	struct btrfs_usrquota *usrquota_orig;
	struct btrfs_usrquota *usrquota_dest;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 rootid = root->objectid;

	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_file_extent_item *fi;
	struct btrfs_trans_handle *trans;
	struct ulist *disko_ulist;
	u64 disko;
	int ret = 0;
	u64 num_bytes = 0;

	if (!fs_info->usrquota_enabled)
		return 0;
	BUG_ON(!fs_info->usrquota_root);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	disko_ulist = ulist_alloc(GFP_NOFS);
	if (!disko_ulist) {
		btrfs_free_path(path);
		return -ENOMEM;
	}

	if (usrquota_rescan_check(fs_info, rootid, BTRFS_I(inode)->location.objectid))
		goto out;

	// commit delayed wrtie
	filemap_flush(inode->i_mapping);

	trans = btrfs_start_transaction(root, 0);
	if (IS_ERR(trans))
		goto out;

	ret = btrfs_run_delayed_refs(trans, root, (unsigned long)-1);
	if (ret) {
		btrfs_end_transaction(trans, root);
		goto out;
	}
	ret = btrfs_end_transaction(trans, root);
	if (ret)
		goto out;

	key.objectid = BTRFS_I(inode)->location.objectid;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = 0;
	path->leave_spinning = 1;

	ret = btrfs_search_slot_for_read(root, &key, path, 1, 0);
	if (ret) {
		if (ret == 1) /* found nothing */
			ret = 0;
		goto out;
	}
	while (1) {
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

		if (found_key.objectid != key.objectid)
			break;
		if (btrfs_key_type(&found_key) != BTRFS_EXTENT_DATA_KEY)
			break;

		fi = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_file_extent_item);
		if (btrfs_file_extent_type(leaf, fi) != BTRFS_FILE_EXTENT_INLINE) {
			disko = btrfs_file_extent_disk_bytenr(leaf, fi);
			if (disko && ulist_add_lru_adjust(disko_ulist, disko, GFP_NOFS)) {
				num_bytes += btrfs_file_extent_disk_num_bytes(leaf, fi);
				if (disko_ulist->nnodes > ULIST_NODES_MAX)
					ulist_remove_first(disko_ulist);
			}
		}
		ret = btrfs_next_item(root, path);
		if (ret) {
			if (ret < 0)
				goto out;
			ret = 0;
			break;
		}
	}

	spin_lock(&fs_info->usrquota_lock);
	usrquota_orig = add_usrquota_rb(fs_info, rootid, inode->i_uid);
	if (IS_ERR(usrquota_orig)) {
		ret = PTR_ERR(usrquota_orig);
		spin_unlock(&fs_info->usrquota_lock);
		goto out;
	}
	usrquota_dest = add_usrquota_rb(fs_info, rootid, new_uid);
	if (IS_ERR(usrquota_dest)) {
		ret = PTR_ERR(usrquota_dest);
		spin_unlock(&fs_info->usrquota_lock);
		goto out;
	}
	// reference: ignore_hardlimit
	// check if quota full
	if (usrquota_dest->uq_rfer_hard && !capable(CAP_SYS_RESOURCE)) {
		if (usrquota_dest->uq_rfer_used + usrquota_dest->uq_reserved + num_bytes > usrquota_dest->uq_rfer_hard) {
			ret = -EDQUOT;
			spin_unlock(&fs_info->usrquota_lock);
			goto out;
		}

	}
	if (unlikely(num_bytes > usrquota_orig->uq_rfer_used)) {
		//underflow
		usrquota_orig->uq_rfer_used = 0;
		WARN_ON_ONCE(1);
	} else {
		usrquota_orig->uq_rfer_used -= num_bytes;
	}
	usrquota_dest->uq_rfer_used += num_bytes;
	usrquota_dirty(fs_info, usrquota_orig);
	usrquota_dirty(fs_info, usrquota_dest);
	spin_unlock(&fs_info->usrquota_lock);

	btrfs_debug(fs_info, "usrquota_transfer uid_new %u uid_old %u num_bytes %llu",
	            new_uid, inode->i_uid, num_bytes);

out:
	ulist_free(disko_ulist);
	btrfs_free_path(path);
	return ret;
}

/*
 * Calculate number of usrquota_{info/limit}_item that need to be reserved for space
 * when taking snapsho.
 */
int btrfs_usrquota_calc_reserve_snap(struct btrfs_root *root,
                                     u64 copy_limit_from, u64 *reserve_items)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_path *path = NULL;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	struct btrfs_usrquota_root_item *item;
	int ret = 0;

	mutex_lock(&fs_info->usrquota_ioctl_lock);
	if (!fs_info->usrquota_enabled) {
		goto unlock_ioctl;
	}

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto unlock_ioctl;
	}

	key.objectid = root->root_key.objectid;
	key.type = BTRFS_USRQUOTA_ROOT_KEY;
	key.offset = 0;
	*reserve_items = 0;
	mutex_lock(&fs_info->usrquota_tree_lock);

//calc_info_items:
	ret = btrfs_search_slot(NULL, fs_info->usrquota_root, &key, path, 0, 0);
	if (ret < 0)
		goto unlock_tree;
	if (ret == 1)
		goto calc_limit_items;

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_usrquota_root_item);
	*reserve_items += btrfs_usrquota_root_info_item_cnt(leaf, item);

calc_limit_items:
	if (copy_limit_from == 0)
		goto success;

	btrfs_release_path(path);
	key.objectid = copy_limit_from;
	ret = btrfs_search_slot(NULL, fs_info->usrquota_root, &key, path, 0, 0);
	if (ret < 0)
		goto unlock_tree;
	if (ret == 1)
		goto success;

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_usrquota_root_item);
	*reserve_items += btrfs_usrquota_root_limit_item_cnt(leaf, item);

success:
	ret = 0;
unlock_tree:
	mutex_unlock(&fs_info->usrquota_tree_lock);
	btrfs_free_path(path);
unlock_ioctl:
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
	return ret;
}

int btrfs_usrquota_mksubvol(struct btrfs_trans_handle *trans,
                            struct btrfs_fs_info *fs_info,
                            u64 objectid)
{
	int ret = 0;
	struct btrfs_usrquota *usrquota;

	if (!fs_info->usrquota_enabled) {
		return 0;
	}
	// insert dummy node
	spin_lock(&fs_info->usrquota_lock);
	usrquota = add_usrquota_rb_nocheck(fs_info, objectid, 0);
	if (IS_ERR(usrquota)) {
		btrfs_err(fs_info, "failed to add_usrquota_rb %ld", PTR_ERR(usrquota));
		ret = PTR_ERR(usrquota);
		goto unlock;
	}
	usrquota->uq_refcnt = 1;
unlock:
	spin_unlock(&fs_info->usrquota_lock);
	return ret;
}

int btrfs_usrquota_mksnap(struct btrfs_trans_handle *trans,
                          struct btrfs_fs_info *fs_info,
                          u64 srcid, u64 objectid,
                          bool readonly, u64 copy_limit_from)
{
	int ret = 0;
	int src_loaded = 0;
	int copy_loaded = 0;

	struct rb_node *node;
	struct btrfs_usrquota *usrquota_new;
	struct btrfs_usrquota *usrquota_orig;

	mutex_lock(&fs_info->usrquota_ioctl_lock);
	if (!fs_info->usrquota_enabled) {
		goto out;
	}
	BUG_ON(!fs_info->usrquota_root);

	// create dummy node
	spin_lock(&fs_info->usrquota_lock);
	usrquota_new = add_usrquota_rb_nocheck(fs_info, objectid, 0);
	if (IS_ERR(usrquota_new)) {
		btrfs_err(fs_info, "failed to add_usrquota_rb %ld", PTR_ERR(usrquota_new));
		ret = PTR_ERR(usrquota_new);
		goto unlock;
	}
	usrquota_new->uq_refcnt = 1;
	spin_unlock(&fs_info->usrquota_lock);

	if (!copy_limit_from)
		goto copy_info_items;

	ret = usrquota_subtree_load(fs_info, copy_limit_from);
	if (ret) {
		btrfs_err(fs_info, "failed to load usrquota subtree %llu", copy_limit_from);
		goto out;
	}
	copy_loaded = 1;
	spin_lock(&fs_info->usrquota_lock);
	node = find_usrquota_first_rb(fs_info, copy_limit_from);
	while (node) {
		usrquota_orig = rb_entry(node, struct btrfs_usrquota, uq_node);
		node = rb_next(node);
		if (usrquota_orig->uq_objectid > copy_limit_from)
			break;
		if (!usrquota_orig->uq_rfer_soft && !usrquota_orig->uq_rfer_hard)
			continue;

		usrquota_new = add_usrquota_rb_nocheck(fs_info, objectid, usrquota_orig->uq_uid);
		if (IS_ERR(usrquota_new)) {
			btrfs_err(fs_info, "failed to add_usrquota_rb %ld", PTR_ERR(usrquota_new));
			ret = PTR_ERR(usrquota_new);
			goto unlock;
		}
		usrquota_new->uq_rfer_soft = usrquota_orig->uq_rfer_soft;
		usrquota_new->uq_rfer_hard = usrquota_orig->uq_rfer_hard;
	}
	spin_unlock(&fs_info->usrquota_lock);

copy_info_items:
	ret = usrquota_subtree_load(fs_info, srcid);
	if (ret) {
		btrfs_err(fs_info, "failed to load usrquota subtree %llu", srcid);
		goto out;
	}
	src_loaded = 1;
	spin_lock(&fs_info->usrquota_lock);
	node = find_usrquota_first_rb(fs_info, srcid);
	while (node) {
		usrquota_orig = rb_entry(node, struct btrfs_usrquota, uq_node);
		node = rb_next(node);
		if (usrquota_orig->uq_objectid > srcid)
			break;
		usrquota_new = add_usrquota_rb_nocheck(fs_info, objectid, usrquota_orig->uq_uid);
		if (IS_ERR(usrquota_new)) {
			btrfs_err(fs_info, "failed to add_usrquota_rb %ld", PTR_ERR(usrquota_new));
			ret = PTR_ERR(usrquota_new);
			goto unlock;
		}
		usrquota_new->uq_rfer_used = usrquota_orig->uq_rfer_used;
		usrquota_new->uq_generation = usrquota_orig->uq_generation;
	}

	// add info & limit items
	node = find_usrquota_first_rb(fs_info, objectid);
	while (node) {
		usrquota_new = rb_entry(node, struct btrfs_usrquota, uq_node);
		node = rb_next(node);
		if (usrquota_new->uq_objectid > objectid)
			break;
		if (usrquota_new->uq_rfer_soft || usrquota_new->uq_rfer_hard) {
			spin_unlock(&fs_info->usrquota_lock);
			ret = update_usrquota_limit_item(trans, fs_info, objectid,
			                                 usrquota_new->uq_uid,
			                                 usrquota_new->uq_rfer_soft,
			                                 usrquota_new->uq_rfer_hard);
			spin_lock(&fs_info->usrquota_lock);
			if (ret) {
				set_inconsistent(fs_info);
				break;
			}
		}
		if (usrquota_new->uq_rfer_used || usrquota_new->uq_generation) {
			spin_unlock(&fs_info->usrquota_lock);
			ret = update_usrquota_info_item(trans, fs_info, objectid,
			                                usrquota_new->uq_uid,
			                                usrquota_new->uq_rfer_used);
			spin_lock(&fs_info->usrquota_lock);
			if (ret) {
				set_inconsistent(fs_info);
				break;
			}
		}
	}
unlock:
	spin_unlock(&fs_info->usrquota_lock);
out:
	if (src_loaded)
		usrquota_subtree_unload(fs_info, srcid);
	if (copy_loaded)
		usrquota_subtree_unload(fs_info, copy_limit_from);
	if (ret || readonly)
		usrquota_subtree_unload(fs_info, objectid);
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
	return ret;
}

int btrfs_usrquota_delsnap(struct btrfs_trans_handle *trans,
                           struct btrfs_fs_info *fs_info, u64 rootid)
{
	int ret = 0;
	struct btrfs_root *usrquota_root = fs_info->usrquota_root;
	struct btrfs_root *subvol_root, *next;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct extent_buffer *leaf;
	int pending_del_nr = 0;
	int pending_del_slot = 0;

	if (!fs_info->usrquota_enabled)
		return 0;
	if (!usrquota_root)
		return -EINVAL;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	mutex_lock(&fs_info->usrquota_ro_roots_lock);
	list_for_each_entry_safe(subvol_root, next, &fs_info->usrquota_ro_roots, usrquota_ro_root) {
		if (subvol_root->objectid == rootid) {
			list_del(&subvol_root->usrquota_ro_root);
			btrfs_debug(fs_info, "Unload ro sub [id:%llu] uq subtree [%llu, %llu]",
			          subvol_root->objectid, subvol_root->usrquota_loaded_gen, fs_info->generation);
			usrquota_subtree_unload(fs_info, subvol_root->objectid);
			break;
		}
	}
	mutex_unlock(&fs_info->usrquota_ro_roots_lock);
	mutex_lock(&fs_info->usrquota_ioctl_lock);
	usrquota_subtree_unload(fs_info, rootid);

	// copyed from btrfs_truncate_inode_items
	mutex_lock(&fs_info->usrquota_tree_lock);
	key.objectid = rootid;
	key.offset = (u64) -1;
	key.type = (u8) -1;
search_again:
	path->leave_spinning = 1;
	ret = btrfs_search_slot(trans, usrquota_root, &key, path, -1, 1);
	if (ret < 0) {
		goto out;
	}
	if (ret > 0) {
		ret = 0;
		if (path->slots[0] == 0) {
			btrfs_err(fs_info, "failed to search usrquota");
			goto out;
		}
		path->slots[0]--;
	}
	while (1) {
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		if (found_key.objectid != rootid)
			break;

		if (!pending_del_nr) {
			pending_del_nr = 1;
			pending_del_slot = path->slots[0];
		} else {
			pending_del_nr++;
			pending_del_slot = path->slots[0];
		}

		if (path->slots[0] == 0) {
			if (pending_del_nr) {
				ret = btrfs_del_items(trans, usrquota_root, path,
				                      pending_del_slot,
						      pending_del_nr);
				if (ret) {
					btrfs_abort_transaction(trans, usrquota_root, ret);
					goto out;
				}
				pending_del_nr = 0;
			}
			btrfs_release_path(path);
			goto search_again;
		} else {
			path->slots[0]--;
		}
	}
	if (pending_del_nr) {
		ret = btrfs_del_items(trans, usrquota_root, path,
		                      pending_del_slot, pending_del_nr);
		if (ret) {
			btrfs_abort_transaction(trans, usrquota_root, ret);
			goto out;
		}
	}
out:
	btrfs_free_path(path);
	mutex_unlock(&fs_info->usrquota_tree_lock);
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
	return ret;
}

/*
 * struct btrfs_ioctl_usrquota_query_args should be initialized to zero
 */
void btrfs_usrquota_query(struct btrfs_fs_info *fs_info, u64 rootid,
                          struct btrfs_ioctl_usrquota_query_args *uqa)
{
	struct btrfs_usrquota *usrquota;

	mutex_lock(&fs_info->usrquota_ioctl_lock);
	if (!fs_info->usrquota_enabled)
		goto unlock;

	if (usrquota_subtree_load(fs_info, rootid))
		goto unlock;

	usrquota = find_usrquota_rb(fs_info, rootid, uqa->uid);
	if (!usrquota)
		goto unload;

	uqa->rfer_used = usrquota->uq_rfer_used;
	uqa->rfer_soft = usrquota->uq_rfer_soft;
	uqa->rfer_hard = usrquota->uq_rfer_hard;
	uqa->reserved = usrquota->uq_reserved;
unload:
	usrquota_subtree_unload(fs_info, rootid);
unlock:
	mutex_unlock(&fs_info->usrquota_ioctl_lock);
}
