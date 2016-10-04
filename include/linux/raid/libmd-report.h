// Copyright (c) 2000-2014 Synology Inc. All rights reserved.
#ifndef _LIBMD_REPORT_H
#define _LIBMD_REPORT_H

#ifdef CONFIG_SYNO_MD_SECTOR_STATUS_REPORT

extern int (*funcSYNOSendRaidEvent)(unsigned int type, unsigned int raidno,
									unsigned int diskno, unsigned int sector);

void SynoReportBadSector(sector_t sector, unsigned long rw,
						 int md_minor, struct block_device *bdev, const char *szFuncName);

void SynoReportCorrectBadSector(sector_t sector, int md_minor,
								struct block_device *bdev, const char *szFuncName);

#endif /* CONFIG_SYNO_MD_SECTOR_STATUS_REPORT */

#ifdef CONFIG_SYNO_MD_AUTO_REMAP_REPORT
extern int (*funcSYNOSendAutoRemapLVMEvent)(const char*, unsigned long long, unsigned int);
extern int (*funcSYNOSendAutoRemapRaidEvent)(unsigned int, unsigned long long, unsigned int);
#endif /* CONFIG_SYNO_MD_AUTO_REMAP_REPORT */

#endif /* _LIBMD_REPORT_H */
