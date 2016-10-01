/*
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2010 - 2012 Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *    Intel Corporation
 *    2200 Mission College Blvd.
 *    Santa Clara, CA  97052
 *
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/pci-intelce_pm.h>
#include "../base/base.h"
#include "pci.h"

#ifdef CONFIG_PM
extern int suspend_device(struct device *dev, pm_message_t state);
extern int resume_device(struct device *dev, pm_message_t state);
extern struct device *next_device(struct klist_iter *i);
extern int async_error;

static intel_pm_pci_ops_t external_pm_ops;

static struct semaphore icepm_lock;

#define CALL_EXTERNAL(rc, x, args...)                     \
down(&icepm_lock);                                        \
if (external_pm_ops.x) { rc = external_pm_ops.x(args); }  \
up(&icepm_lock);

static pci_power_t intel_pm_pci_choose_state(struct pci_dev *dev)
{
    pci_power_t rc = 0;
    CALL_EXTERNAL(rc, choose_state, dev);
	return rc;
}

static bool intel_pm_pci_power_manageable(struct pci_dev *dev)
{
    bool rc = true;
    CALL_EXTERNAL(rc, is_manageable, dev);
	return rc;
}

static int intel_pm_pci_set_power_state(struct pci_dev *dev, pci_power_t state)
{
    int rc = 0;
    CALL_EXTERNAL(rc, set_state, dev, state);
	return rc;
}

static int intel_pm_pci_sleep_wake(struct pci_dev *dev, bool enable)
{
    int rc = 0;
    CALL_EXTERNAL(rc, sleep_wake, dev, enable);
	return rc;
}

static struct pci_platform_pm_ops pci_pm =
{
	.is_manageable = intel_pm_pci_power_manageable,
	.set_state     = intel_pm_pci_set_power_state,
	.choose_state  = intel_pm_pci_choose_state,
	.sleep_wake    = intel_pm_pci_sleep_wake,
};


static int __init intel_pm_pci_init(void)
{
    memset(&external_pm_ops, 0x0, sizeof(intel_pm_pci_ops_t));

    sema_init(&icepm_lock, 1);

	pci_set_platform_pm(&pci_pm);
	return 0;
}


void intel_pm_register_callback(intel_pm_pci_ops_t * ops)
{
    down(&icepm_lock);
    if (ops == NULL)
    {
        memset(&external_pm_ops, 0x0, sizeof(intel_pm_pci_ops_t));
    }
    else
    {
        memcpy(&external_pm_ops, ops, sizeof(intel_pm_pci_ops_t));
    }
    up(&icepm_lock);
}

EXPORT_SYMBOL(intel_pm_register_callback);

arch_initcall_sync(intel_pm_pci_init);

void clear_async_error(void)
{
	async_error = 0;
}
EXPORT_SYMBOL(clear_async_error);

int suspend_devices_rooted(struct device *root, pm_message_t state)
{
       struct klist_iter i;
       struct device *child;
       int error = 0;
       if (!root->p)
	return 0;

       klist_iter_init(&root->p->klist_children, &i);
       while ((child = next_device(&i)) && !error)
               error = suspend_devices_rooted(child, state);
       klist_iter_exit(&i);

       if(error)
               return error;
       return suspend_device(root, state);
}

EXPORT_SYMBOL(suspend_devices_rooted);


int resume_devices_rooted(struct device *root, pm_message_t state)
{
       struct klist_iter i;
       struct device *child;
       int error =0;
       error = resume_device(root, state);
       if(error)
               return error;

       if (!root->p)
	return 0;
       klist_iter_init(&root->p->klist_children, &i);
       while ((child = next_device(&i)) && !error)
               error = resume_devices_rooted(child, state);
       klist_iter_exit(&i);

       return error;
}

EXPORT_SYMBOL(resume_devices_rooted);

#endif
