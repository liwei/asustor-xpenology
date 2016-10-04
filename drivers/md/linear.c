/*
   linear.c : Multiple Devices driver for Linux
	      Copyright (C) 1994-96 Marc ZYNGIER
	      <zyngier@ufr-info-p7.ibp.fr> or
	      <maz@gloups.fdn.fr>

   Linear mode management functions.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include <linux/blkdev.h>
#include <linux/raid/md_u.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "md.h"
#include "linear.h"

/*
 * find which device holds a particular offset 
 */
static inline struct dev_info *which_dev(struct mddev *mddev, sector_t sector)
{
	int lo, mid, hi;
	struct linear_conf *conf;

	lo = 0;
	hi = mddev->raid_disks - 1;
	conf = rcu_dereference(mddev->private);

	/*
	 * Binary Search
	 */

	while (hi > lo) {

		mid = (hi + lo) / 2;
		if (sector < conf->disks[mid].end_sector)
			hi = mid;
		else
			lo = mid + 1;
	}

	return conf->disks + lo;
}

/**
 *	linear_mergeable_bvec -- tell bio layer if two requests can be merged
 *	@q: request queue
 *	@bvm: properties of new bio
 *	@biovec: the request that could be merged to it.
 *
 *	Return amount of bytes we can take at this offset
 */
static int linear_mergeable_bvec(struct request_queue *q,
				 struct bvec_merge_data *bvm,
				 struct bio_vec *biovec)
{
	struct mddev *mddev = q->queuedata;
	struct dev_info *dev0;
	unsigned long maxsectors, bio_sectors = bvm->bi_size >> 9;
	sector_t sector = bvm->bi_sector + get_start_sect(bvm->bi_bdev);
	int maxbytes = biovec->bv_len;
	struct request_queue *subq;
#ifdef CONFIG_SYNO_MD_FIX_LINEAR_ACCESS_NULL_RDEV
	struct md_rdev *rdev = NULL;
#endif /* CONFIG_SYNO_MD_FIX_LINEAR_ACCESS_NULL_RDEV */

	rcu_read_lock();
	dev0 = which_dev(mddev, sector);
	maxsectors = dev0->end_sector - sector;
#ifdef CONFIG_SYNO_MD_FIX_LINEAR_ACCESS_NULL_RDEV
	rdev = rcu_dereference(dev0->rdev);
	if (NULL != rdev) {
		subq = bdev_get_queue(rdev->bdev);
		if (subq->merge_bvec_fn) {
			bvm->bi_bdev = rdev->bdev;
			bvm->bi_sector -= dev0->end_sector - rdev->sectors;
			maxbytes = min(maxbytes, subq->merge_bvec_fn(subq, bvm,
									 biovec));
		}
	}
#else /* CONFIG_SYNO_MD_FIX_LINEAR_ACCESS_NULL_RDEV */
	subq = bdev_get_queue(dev0->rdev->bdev);
	if (subq->merge_bvec_fn) {
		bvm->bi_bdev = dev0->rdev->bdev;
		bvm->bi_sector -= dev0->end_sector - dev0->rdev->sectors;
		maxbytes = min(maxbytes, subq->merge_bvec_fn(subq, bvm,
							     biovec));
	}
#endif /* CONFIG_SYNO_MD_FIX_LINEAR_ACCESS_NULL_RDEV */
	rcu_read_unlock();

	if (maxsectors < bio_sectors)
		maxsectors = 0;
	else
		maxsectors -= bio_sectors;

	if (maxsectors <= (PAGE_SIZE >> 9 ) && bio_sectors == 0)
		return maxbytes;

	if (maxsectors > (maxbytes >> 9))
		return maxbytes;
	else
		return maxsectors << 9;
}

static int linear_congested(void *data, int bits)
{
	struct mddev *mddev = data;
	struct linear_conf *conf;
	int i, ret = 0;

#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
	if (mddev->degraded) {
		return ret;
	}
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */

	if (mddev_congested(mddev, bits))
		return 1;

	rcu_read_lock();
	conf = rcu_dereference(mddev->private);

#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
	for (i = 0; i < mddev->raid_disks && !ret ; i++) {
		struct md_rdev *rdev = rcu_dereference(conf->disks[i].rdev);
		struct request_queue *q = NULL;

		if (!rdev) {
			continue;
		}

		q = bdev_get_queue(rdev->bdev);
		ret |= bdi_congested(&q->backing_dev_info, bits);
	}
#else /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */
	for (i = 0; i < mddev->raid_disks && !ret ; i++) {
		struct request_queue *q = bdev_get_queue(conf->disks[i].rdev->bdev);
		ret |= bdi_congested(&q->backing_dev_info, bits);
	}
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */

	rcu_read_unlock();
	return ret;
}

static sector_t linear_size(struct mddev *mddev, sector_t sectors, int raid_disks)
{
	struct linear_conf *conf;
	sector_t array_sectors;

	rcu_read_lock();
	conf = rcu_dereference(mddev->private);
	WARN_ONCE(sectors || raid_disks,
		  "%s does not support generic reshape\n", __func__);
	array_sectors = conf->array_sectors;
	rcu_read_unlock();

	return array_sectors;
}

static struct linear_conf *linear_conf(struct mddev *mddev, int raid_disks)
{
	struct linear_conf *conf;
	struct md_rdev *rdev;
	int i, cnt;
	bool discard_supported = false;

	conf = kzalloc (sizeof (*conf) + raid_disks*sizeof(struct dev_info),
			GFP_KERNEL);
	if (!conf)
		return NULL;

	cnt = 0;
	conf->array_sectors = 0;

	rdev_for_each(rdev, mddev) {
		int j = rdev->raid_disk;
		struct dev_info *disk = conf->disks + j;
		sector_t sectors;

		if (j < 0 || j >= raid_disks || disk->rdev) {
			printk(KERN_ERR "md/linear:%s: disk numbering problem. Aborting!\n",
			       mdname(mddev));
			goto out;
		}

		disk->rdev = rdev;
		if (mddev->chunk_sectors) {
			sectors = rdev->sectors;
			sector_div(sectors, mddev->chunk_sectors);
			rdev->sectors = sectors * mddev->chunk_sectors;
		}

		disk_stack_limits(mddev->gendisk, rdev->bdev,
				  rdev->data_offset << 9);

		conf->array_sectors += rdev->sectors;
		cnt++;

		if (blk_queue_discard(bdev_get_queue(rdev->bdev)))
			discard_supported = true;
	}
	if (cnt != raid_disks) {
#ifdef CONFIG_SYNO_MD_STATUS_GET
		/*
		 * for Linear status consistense to other raid type
		 * Let it can assemble.
		 */
		mddev->degraded = mddev->raid_disks - cnt;
#ifdef CONFIG_SYNO_MD_EIO_NODEV_HANDLER
		mddev->nodev_and_crashed = 1;
#endif /* CONFIG_SYNO_MD_EIO_NODEV_HANDLER */
		printk(KERN_ERR "md/linear:%s: not enough drives present.\n",
		       mdname(mddev));
		return conf;
#else /* CONFIG_SYNO_MD_STATUS_GET */
		printk(KERN_ERR "md/linear:%s: not enough drives present. Aborting!\n",
		       mdname(mddev));
		goto out;
#endif /* CONFIG_SYNO_MD_STATUS_GET */
	}

	if (!discard_supported)
		queue_flag_clear_unlocked(QUEUE_FLAG_DISCARD, mddev->queue);
	else
		queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, mddev->queue);

	/*
	 * Here we calculate the device offsets.
	 */
	conf->disks[0].end_sector = conf->disks[0].rdev->sectors;

	for (i = 1; i < raid_disks; i++)
		conf->disks[i].end_sector =
			conf->disks[i-1].end_sector +
			conf->disks[i].rdev->sectors;

	return conf;

out:
	kfree(conf);
	return NULL;
}

static int linear_run (struct mddev *mddev)
{
	struct linear_conf *conf;
	int ret;

	if (md_check_no_bitmap(mddev))
		return -EINVAL;
#ifdef CONFIG_SYNO_MD_STATUS_GET
	mddev->degraded = 0;
#endif /* CONFIG_SYNO_MD_STATUS_GET */
	conf = linear_conf(mddev, mddev->raid_disks);

	if (!conf)
		return 1;
	mddev->private = conf;
	md_set_array_sectors(mddev, linear_size(mddev, 0, 0));

	blk_queue_merge_bvec(mddev->queue, linear_mergeable_bvec);
	mddev->queue->backing_dev_info.congested_fn = linear_congested;
	mddev->queue->backing_dev_info.congested_data = mddev;

	ret =  md_integrity_register(mddev);
	if (ret) {
		kfree(conf);
		mddev->private = NULL;
	}
	return ret;
}

static int linear_add(struct mddev *mddev, struct md_rdev *rdev)
{
	/* Adding a drive to a linear array allows the array to grow.
	 * It is permitted if the new drive has a matching superblock
	 * already on it, with raid_disk equal to raid_disks.
	 * It is achieved by creating a new linear_private_data structure
	 * and swapping it in in-place of the current one.
	 * The current one is never freed until the array is stopped.
	 * This avoids races.
	 */
	struct linear_conf *newconf, *oldconf;

	if (rdev->saved_raid_disk != mddev->raid_disks)
		return -EINVAL;

	rdev->raid_disk = rdev->saved_raid_disk;
	rdev->saved_raid_disk = -1;

	newconf = linear_conf(mddev,mddev->raid_disks+1);

	if (!newconf)
		return -ENOMEM;

	oldconf = rcu_dereference_protected(mddev->private,
					    lockdep_is_held(
						    &mddev->reconfig_mutex));
	mddev->raid_disks++;
	rcu_assign_pointer(mddev->private, newconf);
	md_set_array_sectors(mddev, linear_size(mddev, 0, 0));
	set_capacity(mddev->gendisk, mddev->array_sectors);
	revalidate_disk(mddev->gendisk);
	kfree_rcu(oldconf, rcu);
	return 0;
}

static int linear_stop (struct mddev *mddev)
{
	struct linear_conf *conf =
		rcu_dereference_protected(mddev->private,
					  lockdep_is_held(
						  &mddev->reconfig_mutex));

	/*
	 * We do not require rcu protection here since
	 * we hold reconfig_mutex for both linear_add and
	 * linear_stop, so they cannot race.
	 * We should make sure any old 'conf's are properly
	 * freed though.
	 */
	rcu_barrier();
	blk_sync_queue(mddev->queue); /* the unplug fn references 'conf'*/
	kfree(conf);
	mddev->private = NULL;

	return 0;
}

#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
/**
 * This is end_io callback function.
 * We can use this for bad sector report and device error
 * handing. Prevent umount panic from file system
 *
 * @author \$Author: khchen $
 * @version \$Revision: 1.1
 *
 * @param bio    Should not be NULL. Passing from block layer
 * @param error  error number
 */
static void
SynoLinearEndRequest(struct bio *bio, int error)
{
	int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct mddev *mddev = NULL;
	struct md_rdev *rdev = NULL;
	struct bio *data_bio;

	data_bio = bio->bi_private;

	rdev = (struct md_rdev *)data_bio->bi_next;
	mddev = rdev->mddev;

	bio->bi_end_io = data_bio->bi_end_io;
	bio->bi_private = data_bio->bi_private;

	if (!uptodate) {
#ifdef CONFIG_SYNO_MD_EIO_NODEV_HANDLER
		if (IsDeviceDisappear(rdev->bdev)) {
			syno_md_error(mddev, rdev);
		} else {
#ifdef CONFIG_SYNO_MD_SECTOR_STATUS_REPORT
#ifdef CONFIG_SYNO_MD_AUTO_REMAP_REPORT
			if (bio_flagged(bio, BIO_AUTO_REMAP)) {
				SynoReportBadSector(bio->bi_sector, bio->bi_rw, mddev->md_minor, bio->bi_bdev, __FUNCTION__);
			}
#else /* CONFIG_SYNO_MD_AUTO_REMAP_REPORT */
			SynoReportBadSector(bio->bi_sector, bio->bi_rw, mddev->md_minor, bio->bi_bdev, __FUNCTION__);
#endif /* CONFIG_SYNO_MD_AUTO_REMAP_REPORT */
#endif /* CONFIG_SYNO_MD_SECTOR_STATUS_REPORT */
			md_error(mddev, rdev);
		}
#else /* CONFIG_SYNO_MD_EIO_NODEV_HANDLER */
		md_error(mddev, rdev);
#endif /* CONFIG_SYNO_MD_EIO_NODEV_HANDLER */
	}

	atomic_dec(&rdev->nr_pending);
	bio_put(data_bio);
	/* Let mount could successful and bad sector could keep accessing, no matter it success or not */
	bio_endio(bio, 0);
}
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */

static void linear_make_request(struct mddev *mddev, struct bio *bio)
{
	struct dev_info *tmp_dev;
	sector_t start_sector;
#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
	struct bio *data_bio;
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */

	if (unlikely(bio->bi_rw & REQ_FLUSH)) {
		md_flush_request(mddev, bio);
		return;
	}

#ifdef CONFIG_SYNO_MD_STATUS_GET
	/**
	* if there has any device offline, we don't make any request to
	* our linear md array
	*/
#ifdef CONFIG_SYNO_MD_EIO_NODEV_HANDLER
	if (mddev->nodev_and_crashed) {
#else /* CONFIG_SYNO_MD_EIO_NODEV_HANDLER */
	if (mddev->degraded) {
#endif /* CONFIG_SYNO_MD_EIO_NODEV_HANDLER */
		bio_endio(bio, 0);
		return;
	}
#endif /* CONFIG_SYNO_MD_STATUS_GET */
	rcu_read_lock();
	tmp_dev = which_dev(mddev, bio->bi_sector);
	start_sector = tmp_dev->end_sector - tmp_dev->rdev->sectors;

	if (unlikely(bio->bi_sector >= (tmp_dev->end_sector)
		     || (bio->bi_sector < start_sector))) {
		char b[BDEVNAME_SIZE];

		printk(KERN_ERR
		       "md/linear:%s: make_request: Sector %llu out of bounds on "
		       "dev %s: %llu sectors, offset %llu\n",
		       mdname(mddev),
		       (unsigned long long)bio->bi_sector,
		       bdevname(tmp_dev->rdev->bdev, b),
		       (unsigned long long)tmp_dev->rdev->sectors,
		       (unsigned long long)start_sector);
		rcu_read_unlock();
		bio_io_error(bio);
		return;
	}
	if (unlikely(bio_end_sector(bio) > tmp_dev->end_sector)) {
		/* This bio crosses a device boundary, so we have to
		 * split it.
		 */
		struct bio_pair *bp;
		sector_t end_sector = tmp_dev->end_sector;

		rcu_read_unlock();

		bp = bio_split(bio, end_sector - bio->bi_sector);

		linear_make_request(mddev, &bp->bio1);
		linear_make_request(mddev, &bp->bio2);
		bio_pair_release(bp);
		return;
	}

	bio->bi_bdev = tmp_dev->rdev->bdev;
	bio->bi_sector = bio->bi_sector - start_sector
		+ tmp_dev->rdev->data_offset;

#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
	data_bio = bio_clone(bio, GFP_NOIO);

	if (data_bio) {
		atomic_inc(&tmp_dev->rdev->nr_pending);
		data_bio->bi_end_io = bio->bi_end_io;
		data_bio->bi_private = bio->bi_private;
		data_bio->bi_next = (void *)tmp_dev->rdev;

		bio->bi_end_io = SynoLinearEndRequest;
		bio->bi_private = data_bio;
	}
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */

	rcu_read_unlock();

	if (unlikely((bio->bi_rw & REQ_DISCARD) &&
		     !blk_queue_discard(bdev_get_queue(bio->bi_bdev)))) {
		/* Just ignore it */
		bio_endio(bio, 0);
		return;
	}

	generic_make_request(bio);
}

#ifdef CONFIG_SYNO_MD_STATUS_GET

static void
syno_linear_status(struct seq_file *seq, struct mddev *mddev)
{
	struct linear_conf *conf;
	struct md_rdev *rdev;
	int j;

	seq_printf(seq, " %dk rounding", mddev->chunk_sectors / 2);
	seq_printf(seq, " [%d/%d] [", mddev->raid_disks, mddev->raid_disks - mddev->degraded);
	rcu_read_lock();
	conf = rcu_dereference(mddev->private);
	for (j = 0; j < mddev->raid_disks; j++)
	{
		rdev = rcu_dereference(conf->disks[j].rdev);
#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
		if(rdev &&
		   !test_bit(Faulty, &rdev->flags)) {
#else /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */
		if(rdev) {
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */
#ifdef CONFIG_SYNO_MD_STATUS_DISKERROR
			seq_printf (seq, "%s", 
						test_bit(In_sync, &rdev->flags) ? 
						(test_bit(DiskError, &rdev->flags) ? "E" : "U") : "_");
#else /* CONFIG_SYNO_MD_STATUS_DISKERROR */
			seq_printf (seq, "%s", "U");
#endif /* CONFIG_SYNO_MD_STATUS_DISKERROR */
		} else {
			seq_printf (seq, "%s", "_");
		}
	}
	rcu_read_unlock();
	seq_printf (seq, "]");
}
#else /* CONFIG_SYNO_MD_STATUS_GET */
static void linear_status (struct seq_file *seq, struct mddev *mddev)
{

	seq_printf(seq, " %dk rounding", mddev->chunk_sectors / 2);
}
#endif /* CONFIG_SYNO_MD_STATUS_GET */

#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
static int
SynoLinearRemoveDisk(struct mddev *mddev, struct md_rdev *rdev)
{
	int err = 0;
	char nm[20];
	struct linear_conf *conf = mddev->private;
	int number = rdev->raid_disk;

	if (!rdev) {
		goto END;
	}

	/*
		use the same synchronize method as RAID5
		see raid5.c:raid5_remove_disk
	*/
	conf->disks[number].rdev = NULL;
	synchronize_rcu();
	if (atomic_read(&rdev->nr_pending)) {
		/* lost the race, try later */
		err = -EBUSY;
		conf->disks[number].rdev = rdev;
		goto END;
	}

	/**
	 * Linear don't has their own thread, we just remove it's sysfs
	 * when there has no other pending request
	 */
	sprintf(nm,"rd%d", number);
	sysfs_remove_link(&mddev->kobj, nm);
	rdev->raid_disk = -1;
END:
	return err;
}

/**
 * This is our implement for raid handler.
 * It mainly for handling device hotplug.
 * We let it look like other raid type.
 * Set it faulty could let SDK know it's status
 *
 * @author \$Author: khchen $
 * @version \$Revision: 1.1
 *
 * @param mddev  Should not be NULL. passing from md.c
 * @param rdev   Should not be NULL. passing from md.c
 */
static void
SynoLinearError(struct mddev *mddev, struct md_rdev *rdev)
{
	if (test_and_clear_bit(In_sync, &rdev->flags)) {
		if (mddev->degraded < mddev->raid_disks) {
			SYNO_UPDATE_SB_WORK *update_sb = NULL;
			mddev->degraded++;
#ifdef CONFIG_SYNO_MD_EIO_NODEV_HANDLER
			mddev->nodev_and_crashed = 1;
#endif /* CONFIG_SYNO_MD_EIO_NODEV_HANDLER */
			set_bit(Faulty, &rdev->flags);
#ifdef CONFIG_SYNO_MD_STATUS_DISKERROR
			clear_bit(DiskError, &rdev->flags);
#endif /* CONFIG_SYNO_MD_STATUS_DISKERROR */

			if (NULL == (update_sb = kzalloc(sizeof(SYNO_UPDATE_SB_WORK), GFP_ATOMIC))) {
				WARN_ON(!update_sb);
				goto END;
			}

			INIT_WORK(&update_sb->work, SynoUpdateSBTask);
			update_sb->mddev = mddev;
			schedule_work(&update_sb->work);
			set_bit(MD_CHANGE_DEVS, &mddev->flags);
		}
	}
END:
	return;
}

/**
 * This is our implement for raid handler.
 * It mainly for mdadm set device faulty. We let it look like
 * other raid type. Let it become read only (scemd would remount
 * if it find DiskError)
 *
 * You should not sync super block in the same thread, otherwise
 * would panic.
 *
 * @author \$Author: khchen $
 * @version \$Revision: 1.1  *
 *
 * @param mddev  Should not be NULL. passing from md.c
 * @param rdev   Should not be NULL. passing from md.c
 */
static void
SynoLinearErrorInternal(struct mddev *mddev, struct md_rdev *rdev)
{
#ifdef CONFIG_SYNO_MD_STATUS_DISKERROR
	if (!test_bit(DiskError, &rdev->flags)) {
		SYNO_UPDATE_SB_WORK *update_sb = NULL;

		set_bit(DiskError, &rdev->flags);
		if (NULL == (update_sb = kzalloc(sizeof(SYNO_UPDATE_SB_WORK), GFP_ATOMIC))) {
			WARN_ON(!update_sb);
			goto END;
		}

		INIT_WORK(&update_sb->work, SynoUpdateSBTask);
		update_sb->mddev = mddev;
		schedule_work(&update_sb->work);
		set_bit(MD_CHANGE_DEVS, &mddev->flags);
	}

END:
#endif /* CONFIG_SYNO_MD_STATUS_DISKERROR */
	return;
}
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */

static struct md_personality linear_personality =
{
	.name		= "linear",
	.level		= LEVEL_LINEAR,
	.owner		= THIS_MODULE,
	.make_request	= linear_make_request,
	.run		= linear_run,
	.stop		= linear_stop,
#ifdef CONFIG_SYNO_MD_STATUS_GET
	.status		= syno_linear_status,
#else /* CONFIG_SYNO_MD_STATUS_GET */
	.status		= linear_status,
#endif /* CONFIG_SYNO_MD_STATUS_GET */
	.hot_add_disk	= linear_add,
#ifdef CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY
	.hot_remove_disk	= SynoLinearRemoveDisk,
	.error_handler		= SynoLinearErrorInternal,
	.syno_error_handler	= SynoLinearError,
#endif /* CONFIG_SYNO_MD_DEVICE_HOTPLUG_NOTIFY */
	.size		= linear_size,
};

static int __init linear_init (void)
{
	return register_md_personality (&linear_personality);
}

static void linear_exit (void)
{
	unregister_md_personality (&linear_personality);
}

module_init(linear_init);
module_exit(linear_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linear device concatenation personality for MD");
MODULE_ALIAS("md-personality-1"); /* LINEAR - deprecated*/
MODULE_ALIAS("md-linear");
MODULE_ALIAS("md-level--1");
