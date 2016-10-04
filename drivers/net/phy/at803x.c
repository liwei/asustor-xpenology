/*
 * drivers/net/phy/at803x.c
 *
 * Driver for Atheros 803x PHY
 *
 * Author: Matus Ujhelyi <ujhelyi.m@gmail.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/phy.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#define AT803X_INTR_ENABLE			0x12
#define AT803X_INTR_STATUS			0x13
#define AT803X_WOL_ENABLE			0x01
#define AT803X_DEVICE_ADDR			0x03
#define AT803X_LOC_MAC_ADDR_0_15_OFFSET		0x804C
#define AT803X_LOC_MAC_ADDR_16_31_OFFSET	0x804B
#define AT803X_LOC_MAC_ADDR_32_47_OFFSET	0x804A
#define AT803X_MMD_ACCESS_CONTROL		0x0D
#define AT803X_MMD_ACCESS_CONTROL_DATA		0x0E
#define AT803X_FUNC_DATA			0x4003
#if defined(CONFIG_SYNO_LSP_ALPINE)
#define AT803X_DEBUG_ADDR			0x1D
#define AT803X_DEBUG_DATA			0x1E
#define AT803X_DEBUG_SYSTEM_MODE_CTRL		0x05
#define AT803X_DEBUG_RGMII_TX_CLK_DLY		BIT(8)
#endif /* CONFIG_SYNO_LSP_ALPINE */

MODULE_DESCRIPTION("Atheros 803x PHY driver");
MODULE_AUTHOR("Matus Ujhelyi");
MODULE_LICENSE("GPL");

#if defined(CONFIG_SYNO_LSP_ALPINE)
static int at803x_set_wol(struct phy_device *phydev,
			  struct ethtool_wolinfo *wol)
{
	struct net_device *ndev = phydev->attached_dev;
	const u8 *mac;
	int ret;
	u32 value;
	unsigned int i, offsets[] = {
		AT803X_LOC_MAC_ADDR_32_47_OFFSET,
		AT803X_LOC_MAC_ADDR_16_31_OFFSET,
		AT803X_LOC_MAC_ADDR_0_15_OFFSET,
	};

	if (!ndev)
		return -ENODEV;

	if (wol->wolopts & WAKE_MAGIC) {
		mac = (const u8 *) ndev->dev_addr;

		if (!is_valid_ether_addr(mac))
			return -EFAULT;

		for (i = 0; i < 3; i++) {
			phy_write(phydev, AT803X_MMD_ACCESS_CONTROL,
				  AT803X_DEVICE_ADDR);
			phy_write(phydev, AT803X_MMD_ACCESS_CONTROL_DATA,
				  offsets[i]);
			phy_write(phydev, AT803X_MMD_ACCESS_CONTROL,
				  AT803X_FUNC_DATA);
			phy_write(phydev, AT803X_MMD_ACCESS_CONTROL_DATA,
				  mac[(i * 2) + 1] | (mac[(i * 2)] << 8));
		}

		value = phy_read(phydev, AT803X_INTR_ENABLE);
		value |= AT803X_WOL_ENABLE;
		ret = phy_write(phydev, AT803X_INTR_ENABLE, value);
		if (ret)
			return ret;
		value = phy_read(phydev, AT803X_INTR_STATUS);
	} else {
		value = phy_read(phydev, AT803X_INTR_ENABLE);
		value &= (~AT803X_WOL_ENABLE);
		ret = phy_write(phydev, AT803X_INTR_ENABLE, value);
		if (ret)
			return ret;
		value = phy_read(phydev, AT803X_INTR_STATUS);
	}

	return ret;
}

static void at803x_get_wol(struct phy_device *phydev,
			   struct ethtool_wolinfo *wol)
{
	u32 value;

	wol->supported = WAKE_MAGIC;
	wol->wolopts = 0;

	value = phy_read(phydev, AT803X_INTR_ENABLE);
	if (value & AT803X_WOL_ENABLE)
		wol->wolopts |= WAKE_MAGIC;
}
#else /* CONFIG_SYNO_LSP_ALPINE */
static void at803x_set_wol_mac_addr(struct phy_device *phydev)
{
	struct net_device *ndev = phydev->attached_dev;
	const u8 *mac;
	unsigned int i, offsets[] = {
		AT803X_LOC_MAC_ADDR_32_47_OFFSET,
		AT803X_LOC_MAC_ADDR_16_31_OFFSET,
		AT803X_LOC_MAC_ADDR_0_15_OFFSET,
	};

	if (!ndev)
		return;

	mac = (const u8 *) ndev->dev_addr;

	if (!is_valid_ether_addr(mac))
		return;

	for (i = 0; i < 3; i++) {
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL,
				  AT803X_DEVICE_ADDR);
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL_DATA,
				  offsets[i]);
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL,
				  AT803X_FUNC_DATA);
		phy_write(phydev, AT803X_MMD_ACCESS_CONTROL_DATA,
				  mac[(i * 2) + 1] | (mac[(i * 2)] << 8));
	}
}
#endif /* CONFIG_SYNO_LSP_ALPINE */

static int at803x_config_init(struct phy_device *phydev)
{
	int val;
#if defined(CONFIG_SYNO_LSP_ALPINE)
	int ret;
	u32 features;
#else /* CONFIG_SYNO_LSP_ALPINE */
	u32 features;
	int status;
#endif /* CONFIG_SYNO_LSP_ALPINE */

	features = SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_AUI |
		   SUPPORTED_FIBRE | SUPPORTED_BNC;

	val = phy_read(phydev, MII_BMSR);
	if (val < 0)
		return val;

	if (val & BMSR_ANEGCAPABLE)
		features |= SUPPORTED_Autoneg;
	if (val & BMSR_100FULL)
		features |= SUPPORTED_100baseT_Full;
	if (val & BMSR_100HALF)
		features |= SUPPORTED_100baseT_Half;
	if (val & BMSR_10FULL)
		features |= SUPPORTED_10baseT_Full;
	if (val & BMSR_10HALF)
		features |= SUPPORTED_10baseT_Half;

	if (val & BMSR_ESTATEN) {
		val = phy_read(phydev, MII_ESTATUS);
		if (val < 0)
			return val;

		if (val & ESTATUS_1000_TFULL)
			features |= SUPPORTED_1000baseT_Full;
		if (val & ESTATUS_1000_THALF)
			features |= SUPPORTED_1000baseT_Half;
	}

	phydev->supported = features;
	phydev->advertising = features;

#if defined(CONFIG_SYNO_LSP_ALPINE)
	ret = phy_write(phydev, AT803X_DEBUG_ADDR,
			AT803X_DEBUG_SYSTEM_MODE_CTRL);
	if (ret)
		return ret;
	ret = phy_write(phydev, AT803X_DEBUG_DATA,
			AT803X_DEBUG_RGMII_TX_CLK_DLY);
	if (ret)
		return ret;
#else /* CONFIG_SYNO_LSP_ALPINE */
	/* enable WOL */
	at803x_set_wol_mac_addr(phydev);
	status = phy_write(phydev, AT803X_INTR_ENABLE, AT803X_WOL_ENABLE);
	status = phy_read(phydev, AT803X_INTR_STATUS);
#endif /* CONFIG_SYNO_LSP_ALPINE */

	return 0;
}

#if defined(CONFIG_SYNO_LSP_ALPINE)
static struct phy_driver at803x_driver[] = {
{
	/* ATHEROS 8035 */
#else /* CONFIG_SYNO_LSP_ALPINE */
/* ATHEROS 8035 */
static struct phy_driver at8035_driver = {
#endif /* CONFIG_SYNO_LSP_ALPINE */
	.phy_id		= 0x004dd072,
	.name		= "Atheros 8035 ethernet",
	.phy_id_mask	= 0xffffffef,
	.config_init	= at803x_config_init,
#if defined(CONFIG_SYNO_LSP_ALPINE)
	.set_wol	= at803x_set_wol,
	.get_wol	= at803x_get_wol,
#endif /* CONFIG_SYNO_LSP_ALPINE */
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.driver		= {
		.owner = THIS_MODULE,
	},
#if defined(CONFIG_SYNO_LSP_ALPINE)
}, {
	/* ATHEROS 8030 */
#else /* CONFIG_SYNO_LSP_ALPINE */
};

/* ATHEROS 8030 */
static struct phy_driver at8030_driver = {
#endif /* CONFIG_SYNO_LSP_ALPINE */
	.phy_id		= 0x004dd076,
	.name		= "Atheros 8030 ethernet",
	.phy_id_mask	= 0xffffffef,
	.config_init	= at803x_config_init,
#if defined(CONFIG_SYNO_LSP_ALPINE)
	.set_wol	= at803x_set_wol,
	.get_wol	= at803x_get_wol,
#endif /* CONFIG_SYNO_LSP_ALPINE */
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.driver		= {
		.owner = THIS_MODULE,
	},
#if defined(CONFIG_SYNO_LSP_ALPINE)
}, {
	/* ATHEROS 8031 */
	.phy_id		= 0x004dd074,
	.name		= "Atheros 8031 ethernet",
	.phy_id_mask	= 0xffffffef,
	.config_init	= at803x_config_init,
	.set_wol	= at803x_set_wol,
	.get_wol	= at803x_get_wol,
	.features	= PHY_GBIT_FEATURES,
	.flags		= PHY_HAS_INTERRUPT,
	.config_aneg	= &genphy_config_aneg,
	.read_status	= &genphy_read_status,
	.driver		= {
		.owner = THIS_MODULE,
	},
} };
#else /* CONFIG_SYNO_LSP_ALPINE */
};
#endif /* CONFIG_SYNO_LSP_ALPINE */

#if defined(CONFIG_SYNO_LSP_ALPINE)
static int __init atheros_init(void)
{
	return phy_drivers_register(at803x_driver,
				    ARRAY_SIZE(at803x_driver));
}

static void __exit atheros_exit(void)
{
	return phy_drivers_unregister(at803x_driver,
				      ARRAY_SIZE(at803x_driver));
}
#else /* CONFIG_SYNO_LSP_ALPINE */
static int __init atheros_init(void)
{
	int ret;

	ret = phy_driver_register(&at8035_driver);
	if (ret)
		goto err1;

	ret = phy_driver_register(&at8030_driver);
	if (ret)
		goto err2;

	return 0;

err2:
	phy_driver_unregister(&at8035_driver);
err1:
	return ret;
}

static void __exit atheros_exit(void)
{
	phy_driver_unregister(&at8035_driver);
	phy_driver_unregister(&at8030_driver);
}
#endif /* CONFIG_SYNO_LSP_ALPINE */

module_init(atheros_init);
module_exit(atheros_exit);

static struct mdio_device_id __maybe_unused atheros_tbl[] = {
	{ 0x004dd076, 0xffffffef },
#if defined(CONFIG_SYNO_LSP_ALPINE)
	{ 0x004dd074, 0xffffffef },
#endif /* CONFIG_SYNO_LSP_ALPINE */
	{ 0x004dd072, 0xffffffef },
	{ }
};

MODULE_DEVICE_TABLE(mdio, atheros_tbl);
