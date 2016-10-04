ifeq ($(CONFIG_SYNO_LSP_ARMADA),y)
snd-soc-kirkwood-objs := kirkwood-dma.o kirkwood-i2s.o
else
snd-soc-kirkwood-objs := kirkwood-dma.o
snd-soc-kirkwood-i2s-objs := kirkwood-i2s.o
endif

obj-$(CONFIG_SND_KIRKWOOD_SOC) += snd-soc-kirkwood.o
ifeq ($(CONFIG_SYNO_LSP_ARMADA),y)

snd-soc-armada-370-db-objs := armada-370-db.o

obj-$(CONFIG_SND_KIRKWOOD_SOC_ARMADA370_DB) += snd-soc-armada-370-db.o
else
obj-$(CONFIG_SND_KIRKWOOD_SOC_I2S) += snd-soc-kirkwood-i2s.o

snd-soc-openrd-objs := kirkwood-openrd.o
snd-soc-t5325-objs := kirkwood-t5325.o

obj-$(CONFIG_SND_KIRKWOOD_SOC_OPENRD) += snd-soc-openrd.o
obj-$(CONFIG_SND_KIRKWOOD_SOC_T5325) += snd-soc-t5325.o
endif
