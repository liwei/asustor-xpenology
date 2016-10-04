/*
 * usb hub driver head file
 *
 * Copyright (C) 1999 Linus Torvalds
 * Copyright (C) 1999 Johannes Erdfelt
 * Copyright (C) 1999 Gregory P. Smith
 * Copyright (C) 2001 Brad Hards (bhards@bigpond.net.au)
 * Copyright (C) 2012 Intel Corp (tianyu.lan@intel.com)
 *
 *  move struct usb_hub to this file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/usb.h>
#include <linux/usb/ch11.h>
#include <linux/usb/hcd.h>
#include "usb.h"

#ifdef CONFIG_SYNO_USB_CONNECT_DEBOUNCER
#define SYNO_CONNECT_BOUNCE 0x400
#endif /* CONFIG_SYNO_USB_CONNECT_DEBOUNCER */

struct usb_hub {
	struct device		*intfdev;	/* the "interface" device */
	struct usb_device	*hdev;
	struct kref		kref;
	struct urb		*urb;		/* for interrupt polling pipe */

	/* buffer for urb ... with extra space in case of babble */
	u8			(*buffer)[8];
	union {
		struct usb_hub_status	hub;
		struct usb_port_status	port;
	}			*status;	/* buffer for status reports */
	struct mutex		status_mutex;	/* for the status buffer */

	int			error;		/* last reported error */
	int			nerrors;	/* track consecutive errors */

	struct list_head	event_list;	/* hubs w/data or errs ready */
	unsigned long		event_bits[1];	/* status change bitmask */
	unsigned long		change_bits[1];	/* ports with logical connect
							status change */
	unsigned long		busy_bits[1];	/* ports being reset or
							resumed */
	unsigned long		removed_bits[1]; /* ports with a "removed"
							device present */
	unsigned long		wakeup_bits[1];	/* ports that have signaled
							remote wakeup */
#if defined(CONFIG_USB_ETRON_HUB)
	unsigned long		bot_mode_bits[1];
#endif /* CONFIG_USB_ETRON_HUB */

#if USB_MAXCHILDREN > 31 /* 8*sizeof(unsigned long) - 1 */
#error event_bits[] is too short!
#endif

	struct usb_hub_descriptor *descriptor;	/* class descriptor */
	struct usb_tt		tt;		/* Transaction Translator */

	unsigned		mA_per_port;	/* current for each child */
#ifdef	CONFIG_PM
	unsigned		wakeup_enabled_descendants;
#endif

	unsigned		limited_power:1;
	unsigned		quiescing:1;
	unsigned		disconnected:1;

	unsigned		quirk_check_port_auto_suspend:1;

	unsigned		has_indicators:1;
	u8			indicator[USB_MAXCHILDREN];
	struct delayed_work	leds;
	struct delayed_work	init_work;
	struct usb_port		**ports;

#ifdef CONFIG_SYNO_USB_UPS_DISCONNECT_FILTER
	struct timer_list	ups_discon_flt_timer;
	int			ups_discon_flt_port;
	unsigned long		ups_discon_flt_last; /* last filtered time */
#endif /* CONFIG_SYNO_USB_UPS_DISCONNECT_FILTER */
};

/**
 * struct usb port - kernel's representation of a usb port
 * @child: usb device attatched to the port
 * @dev: generic device interface
 * @port_owner: port's owner
 * @connect_type: port's connect type
 * @portnum: port index num based one
 * @power_is_on: port's power state
 * @did_runtime_put: port has done pm_runtime_put().
 */
struct usb_port {
	struct usb_device *child;
	struct device dev;
	struct dev_state *port_owner;
	enum usb_port_connect_type connect_type;
	u8 portnum;
	unsigned power_is_on:1;
	unsigned did_runtime_put:1;
#if defined (CONFIG_SYNO_USB_POWER_RESET)
	unsigned int power_cycle_counter;
#endif /* CONFIG_SYNO_USB_POWER_RESET */
#ifdef CONFIG_SYNO_CASTRATED_XHC
#define SYNO_USB_PORT_CASTRATED_XHC 0x01
	unsigned int flag;
#endif /* CONFIG_SYNO_CASTRATED_XHC */
};
#if defined (CONFIG_SYNO_USB_POWER_RESET)
#define SYNO_POWER_CYCLE_TRIES	(3)
#endif /* CONFIG_SYNO_USB_POWER_RESET */

#define to_usb_port(_dev) \
	container_of(_dev, struct usb_port, dev)

extern int usb_hub_create_port_device(struct usb_hub *hub,
		int port1);
extern void usb_hub_remove_port_device(struct usb_hub *hub,
		int port1);
extern int usb_hub_set_port_power(struct usb_device *hdev,
		int port1, bool set);
extern struct usb_hub *usb_hub_to_struct_hub(struct usb_device *hdev);
extern int hub_port_debounce(struct usb_hub *hub, int port1,
		bool must_be_connected);
extern int usb_clear_port_feature(struct usb_device *hdev,
		int port1, int feature);
#if defined(CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4)
extern void root_hub_recovery(struct usb_hub *hub);
#endif /* CONFIG_SYNO_LSP_ARMADA_2015_T1_1p4 */

static inline int hub_port_debounce_be_connected(struct usb_hub *hub,
		int port1)
{
	return hub_port_debounce(hub, port1, true);
}

static inline int hub_port_debounce_be_stable(struct usb_hub *hub,
		int port1)
{
	return hub_port_debounce(hub, port1, false);
}
