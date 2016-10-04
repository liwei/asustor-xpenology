#!/usr/bin/make
# Makefile for building Linux QLogic Gigabit ethernet driver as a module ; edited by Trantor for XPEnology
# $id$
KVER=3.10.35

ifeq ($(BCMCFGDIR),)
	BCMCFGS := /lib/modules/$(KVER)/build \
	           /usr/src/kernels/$(KVER)-$(BCMPROC) \
	           /usr/src/linux-$(shell echo $(KVER) | sed "s|\(.*\)-\([a-z]*\)|\1-obj/$(shell uname -i)/\2|") \
	           /usr/src/linux

	# Delete any entries that do not exist.
	BCMTSTDIR = $(shell if ls $(DIR) > /dev/null 2>&1; then echo $(DIR); fi)
	BCMCFGS := $(foreach DIR, $(BCMCFGS), $(BCMTSTDIR))

	ifeq ($(BCMCFGS),)
		$(error Linux kernel config directory not found)
	else
		BCMCFGDIR := $(firstword $(BCMCFGS))
	endif
endif

UAPI=uapi

NETDUMP_CFLAG =
VLAN_RX_CFLAG =
NETDEV_FEAT_CFLAG =

SKB_CFLAG = -DNEW_SKB
IP_CFLAG = -DHAVE_IP_HDR
LE32_CFLAG = -DHAVE_LE32
GFP_CFLAG = -DHAVE_GFP
BOOL_CFLAG = -DHAVE_BOOL
AER_CFLAG = -DHAVE_AER
DEV_ERR_CFLAG = -DHAVE_DEV_ERR
DEV_PRINTK_CFLAG = -DHAVE_DEV_PRINTK
REAL_TX_CFLAG = -DHAVE_REAL_TX
REAL_RX_CFLAG = -DHAVE_REAL_RX
IX_FEAT_CFLAG = -DHAVE_FIX_FEATURES
ETHTOOL_CFLAG = -DNEW_ETHTOOL
MDIX_CFLAG = -DHAVE_MDIX_STATUS
MDIX_CTRL_CFLAG = -DHAVE_MDIX_CTRL
SKB_FRAG_CFLAG = -DHAVE_SKB_FRAG
SKB_FRAG_PAGE_CFLAG = -DHAVE_SKB_FRAG_PAGE
ETHTOOL_MII_CFLAG = -DHAVE_ETHTOOL_TO_MII
IS_PCIE_CFLAG = -DHAVE_IS_PCIE
WAKE_CAP_CFLAG = -DHAVE_DEVICE_SET_WAKEUP_CAP
PME_CFLAG = -DHAVE_PCI_PME_CAPABLE
WAKE_D3_CFLAG = -DHAVE_PCI_WAKE_FROM_D3

BNX2X_EXTRA_CFLAGS += -D_DEFINE_SKB_SET_HASH
BNX2X_EXTRA_CFLAGS += -D_DEFINE_READ_ONCE
BNX2X_EXTRA_CFLAGS += -D_DEFINE_WRITE_ONCE
BNX2X_EXTRA_CFLAGS += -D_DEFINE_SMP_MB_BEFORE_ATOMIC
BNX2X_EXTRA_CFLAGS += -D_DEFINE_SMP_MB_BEFORE_ATOMIC_V2

BCM_KVER:=3

ifeq ($(BCM_KVER), 3)

BCM_CNIC:=1
BCM_DRV = bnx2.ko cnic.ko

ifneq ($(KERNELRELEASE),)

obj-m += bnx2.o
obj-m += cnic.o


EXTRA_CFLAGS = $(NETDUMP_CFLAG) $(SKB_CFLAG) $(IP_CFLAG) $(LE32_CFLAG) $(BOOL_CFLAG) $(GFP_CFLAG) $(AER_CFLAG) $(DEV_ERR_CFLAG) $(DEV_PRINTK_CFLAG) $(REAL_TX_CFLAG) $(REAL_RX_CFLAG) $(IS_PCIE_CFLAG) $(SKB_FRAG_CFLAG) $(SKB_FRAG_PAGE_CFLAG) $(ETHTOOL_MII_CFLAG) $(FIX_FEAT_CFLAG) $(NETDEV_FEAT_CFLAG) $(ETHTOOL_CFLAG) $(BNX2X_EXTRA_CFLAGS) $(PME_CFLAG) $(WAKE_D3_CFLAG) $(WAKE_CAP_CFLAG) $(MDIX_CFLAG) $(MDIX_CTRL_CFLAG)

else

default:
	make -C $(BCMCFGDIR) SUBDIRS=$(shell pwd) modules

endif

else # ifeq ($(BCM_KVER),3)
# Makefile for 2.4 kernel

BCM_DRV = bnx2.o

CC = gcc

CFLAGS=-DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -I$(BCMCFGDIR)/include $(NETDUMP_CFLAG) $(GFP_CFLAG)

ifeq ($(shell grep netif_poll_disable $(LINUXSRC)/include/linux/netdevice.h > /dev/null 2>&1 && echo newnetif),)
  CFLAGS+=-DOLD_NETIF
endif

ifeq ($(wildcard ~/rpmdir),)
  rpmdir = /usr/src/redhat
else
  rpmdir = $(wildcard ~/rpmdir)
endif

ifeq ($(__ARCH),x86_64)
  CFLAGS+=-mno-red-zone -mcmodel=kernel -pipe -finline-limit=2000
endif

ifeq ($(__ARCH),ia64)
  CFLAGS+=-pipe -ffixed-r13 -mfixed-range=f10-f15,f32-f127 -falign-functions=32
endif

ifeq ($(__ARCH),ppc64)
  ifneq ($(shell ls /opt/cross/bin/powerpc64-linux-gcc > /dev/null 2>&1 && echo ppcgcc),)
    CC=/opt/cross/bin/powerpc64-linux-gcc
  endif

  CFLAGS+=-fno-strict-aliasing -fno-common -fomit-frame-pointer -msoft-float -pipe -mminimal-toc -fno-builtin
endif

ifdef SMALL
  CFLAGS += -Os
else
  CFLAGS += -O2
endif

all: bnx2.o

endif # ifeq ($(BCM_KVER),3)

bnx2.4.gz:
	gzip -c bnx2.4 > bnx2.4.gz

ifeq ($(BCM_KVER), 3)
install: default bnx2.4.gz
else
install: $(BCM_DRV) bnx2.4.gz
endif
	mkdir -p $(PREFIX)/$(BCMMODDIR);
	install -m 444 $(BCM_DRV) $(PREFIX)/$(BCMMODDIR);
	install -m 444 bnx2.4.gz $(PREFIX)/usr/share/man/man4;
	@if [ "$(PREFIX)" = "" ]; then /sbin/depmod -a ;\
	else echo " *** Run '/sbin/depmod -a' to update the module database.";\
	fi

.PHONEY: all clean install

clean:
	-rm -f bnx2.o bnx2.ko bnx2.mod.c bnx2.mod.o bnx2.4.gz cnic.o cnic.ko cnic.mod.c cnic.mod.o .bnx2.*.cmd .cnic.*.cmd *.markers *.order *.symvers
	-rm -rf .tmp_versions

