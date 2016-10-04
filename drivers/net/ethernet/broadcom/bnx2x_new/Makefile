#!/usr/bin/make
# Makefile for building Linux QLogic 10-Gigabit ethernet driver as a module ; edited by Trantor for XPEnology

KVER=3.10.35

__ARCH=$(shell uname -m)

# PREFIX may be set by the RPM build to set the effective root.
PREFIX =

MAN_DIR = $(PREFIX)/usr/share/man/man4

IDLE_CHK_C = bnx2x_self_test.c
REG_DUMP_H = bnx2x_dump.h
REG_HSI_H = bnx2x_reg.h bnx2x_fw_defs.h bnx2x_hsi.h bnx2x_mfw_req.h
CNIC_INC = ../../bnx2/src/cnic_if.h
INIT_OPS_H = bnx2x_init.h bnx2x_init_ops.h
SP_VERBS = bnx2x_sp.c bnx2x_sp.h
HW_CHANNEL_H = bnx2x_vfpf.h

SOURCES_PF = bnx2x_main.c bnx2x_cmn.[ch] bnx2x_link.c bnx2x.h bnx2x_link.h bnx2x_compat.h $(INIT_OPS_H) bnx2x_fw_file_hdr.h bnx2x_dcb.[ch] $(SP_VERBS) bnx2x_stats.[ch] bnx2x_ethtool.c $(IDLE_CHK_C) bnx2x_sriov.[ch] bnx2x_vfpf.c
INIT_VAL_C = bnx2x_init_values_e1.c bnx2x_init_values_e1h.c bnx2x_init_values_e2.c

CHAR_FILES_C = bnx2x_char_dev.c
CHAR_FILES_H = bnx2x_char_dev.h bnx2x_user_iface.h
CHAR_FILES = $(CHAR_FILES_C) $(CHAR_FILES_H)

MODULE_NAME = bnx2x
ifeq ($(NPA),1)
override EXTRA_CFLAGS += -D__VMKNPA__
endif

override EXTRA_CFLAGS += -DDRV_MODULE_NAME=\"$(MODULE_NAME)\"

obj-m +=  $(MODULE_NAME).o
bnx2x-objs = bnx2x_main.o bnx2x_link.o bnx2x_cmn.o $(INIT_VAL_C:%.c=%.o) bnx2x_dcb.o bnx2x_sp.o bnx2x_stats.o bnx2x_ethtool.o bnx2x_self_test.o bnx2x_sriov.o bnx2x_vfpf.o

ifeq ($(DISABLE_WERROR),)
        override EXTRA_CFLAGS += -Werror
endif

ifeq ($(NX2_BOOT_DISK),1)
override EXTRA_CFLAGS += -DBNX2X_DRIVER_DISK
endif

override EXTRA_CFLAGS += -DHAS_SRIOV_CONFIGURE
override EXTRA_CFLAGS += -DHAS_NDO_FIX_FEATURES
override EXTRA_CFLAGS += -D_DEFINE_PCIE_GET_MIN_LINK_
override EXTRA_CFLAGS += -D_DEFINE_PCIE_WAIT_PEND_TRANS_
override EXTRA_CFLAGS += -D_DEFINE_PHYS_PORT_ID
override EXTRA_CFLAGS += -D_DEFINE_PHYS_ITEM_ID
override EXTRA_CFLAGS += -D_DEFINE_PCI_ENABLE_MSIX_RANGE
override EXTRA_CFLAGS += -D_DEFINE_IFLA_VF_RATE
override EXTRA_CFLAGS += -D_HAS_HW_TSTAMP
override EXTRA_CFLAGS += -D_DEFINE_SMP_MB_BEFORE_ATOMIC
override EXTRA_CFLAGS += -D_DEFINE_SMP_MB_BEFORE_ATOMIC_V2
override EXTRA_CFLAGS += -D_DEFINE_LL_RX_POLL
override EXTRA_CFLAGS += -D_DEFINE_SKB_SET_HASH
override EXTRA_CFLAGS += -D_HAS_CLAMP
override EXTRA_CFLAGS += -D_GETNUMTCS_RETURNS_INT
override EXTRA_CFLAGS += -DHAS_NDO_FIX_FEATURES
override EXTRA_CFLAGS += -D_DEFINE_RSS_KEY_FILL
override EXTRA_CFLAGS += -D_DEFINE_NAPI_SCHEDULE_IRQOFF
override EXTRA_CFLAGS += -D_DEFINE_CYCLECOUNTER_MASK
override EXTRA_CFLAGS += -D_DEFINE_SKB_VLAN_TAG
override EXTRA_CFLAGS += -D_HAS_PCI_FRESET
override EXTRA_CFLAGS += -D_DEFINE_READ_ONCE
override EXTRA_CFLAGS += -D_DEFINE_WRITE_ONCE


ifeq ($(SOE),1)
override EXTRA_CFLAGS += -DBNX2X_STOP_ON_ERROR
endif

ifeq ($(NSTS),1)
override EXTRA_CFLAGS += -DBNX2X_NO_STATS
endif


ifeq ($(CHAR_BUILD),1)
override EXTRA_CFLAGS += -DBNX2X_CHAR_DEV
bnx2x-objs += $(CHAR_FILES_C:%.c=%.o)
SOURCES_PF += $(CHAR_FILES)
endif

# targets and rules
.PHONY: all modules_install docs_install clean install

all default: $(obj-m)

soe:
	$(MAKE) SOE=1

nsts:
	$(MAKE) NSTS=1

char:
	$(MAKE) CHAR_BUILD=1

$(obj-m): $(SOURCES_PF) $(INIT_VAL_C) $(REG_DUMP_H) $(REG_HSI_H) $(CNIC_INC) $(HW_CHANNEL_H)
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` modules

modules_install: $(obj-m)
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` INSTALL_MOD_PATH=$(PREFIX) INSTALL_MOD_DIR=$(DRV_DIR) modules_install
	@if [ "$(sles_distro)" = "" -a "$(ubuntu_distro)" = "" ]; then\
		echo "override bnx2x * $(DRV_DIR)" > bnx2x.conf ;\
		install -m 644 -D bnx2x.conf $(PREFIX)/etc/depmod.d/bnx2x.conf ;\
	fi
	@if [ "$(PREFIX)" = "" ] ;\
		then /sbin/depmod -a ;\
		else echo " *** Run '/sbin/depmod -a' to update the module database." ;\
	fi
	@if [ -f /boot/initramfs-$(KVER).img ] && [ "`lsinitrd /boot/initramfs-$(KVER).img | grep bnx2x.ko`" ] ; then\
		echo "back up initrd to /boot/initramfs-$(KVER).img.bak" ;\
		cp /boot/initramfs-$(KVER).img /boot/initramfs-$(KVER).img.bak ;\
		echo "add driver bnx2x to initrd" ;\
		dracut -f --add-drivers bnx2x ;\
	fi

docs_install:
	gzip -c bnx2x.4 > bnx2x.4.gz
	install -m 644 bnx2x.4.gz $(MAN_DIR)

clean:
	$(MAKE) -C $(KERNEL_DIR) M=`pwd` clean
	@rm -f *.fw *.ihex

install: modules_install docs_install

# generate the include files
-include Makefile.gen
