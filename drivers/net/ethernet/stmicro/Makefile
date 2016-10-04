#
# Makefile for the STMicroelectronics device drivers.
#
ifeq ($(CONFIG_ARCH_STI),y)
obj-$(CONFIG_STMMAC_ETH) += stmmac_lsp/
else
obj-$(CONFIG_STMMAC_ETH) += stmmac/
endif
obj-$(CONFIG_FASTPATH_ST) += stmfp/
obj-$(CONFIG_ISVE_ST) += isve/
