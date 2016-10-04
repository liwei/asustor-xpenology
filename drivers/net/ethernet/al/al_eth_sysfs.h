/* al_eth_sysfs.h: AnnapurnaLabs Unified 1GbE and 10GbE ethernet driver.
 *
 * Copyright (c) 2013 AnnapurnaLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */
#ifndef __AL_ETH_SYSFS_H__
#define __AL_ETH_SYSFS_H__

int al_eth_sysfs_init(struct device *dev);

void al_eth_sysfs_terminate(struct device *dev);

#endif /* __AL_ETH_SYSFS_H__ */
