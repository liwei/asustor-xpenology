#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/syno.h>
#include <linux/synobios.h>
#include <linux/delay.h>

MODULE_LICENSE("Proprietary");

#define SYNO_MAX_HDD_PRZ 4
#define SYNO_HDDMON_POLL_SEC 1
#define SYNO_HDDMON_EN_WAIT_SEC 7
#define SYNO_HDDMON_STR "Syno_HDDMon"
#define SYNO_HDDMON_UPLG_STR "Syno_HDDMon_UPLGM"
extern long g_internal_hd_num;

#ifdef CONFIG_SYNO_HDD_HOTPLUG
extern long g_hdd_hotplug;
#endif /* CONFIG_SYNO_HDD_HOTPLUG */

#define GPIO_UNDEF				0xFF

extern int SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER(void);
extern int SYNO_CTRL_HDD_POWERON(int index, int value);
extern int SYNO_CHECK_HDD_PRESENT(int index);

typedef struct __SynoHddMonData {
	int iProcessingIdx;
	int blHddHotPlugSupport;
	int iMaxHddNum;
	int blHddEnStat[SYNO_MAX_HDD_PRZ];
} SynoHddMonData_t;

static struct task_struct *pHddPrzPolling;
static SynoHddMonData_t synoHddMonData;

static int syno_hddmon_data_init(SynoHddMonData_t *pData)
{
	int iRet = -1;

	if (NULL == pData) {
		goto END;
	}

	memset(pData, 0, sizeof(SynoHddMonData_t));

#ifdef CONFIG_SYNO_HDD_HOTPLUG
	pData->blHddHotPlugSupport = g_hdd_hotplug;
#endif /* CONFIG_SYNO_HDD_HOTPLUG */

	pData->iMaxHddNum = g_internal_hd_num;

	if(SYNO_MAX_HDD_PRZ < pData->iMaxHddNum) {
		goto END;
	}

	if(!SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER()){
		pData->blHddHotPlugSupport = 0;
	}

	if(0 == pData->blHddHotPlugSupport) {
		goto END;
	}

	iRet = 0;
END:
	return iRet;
}

static int syno_hddmon_unplug_monitor(void *args)
{
	int iRet = -1;
	int iIdx;
	int iPrzPinVal;
	SynoHddMonData_t *pData = NULL;
	unsigned int uiTimeout;

	if (NULL == args) {
		goto END;
	}

	pData = (SynoHddMonData_t *) args;

	while(1) {
		if (kthread_should_stop()) {
			break;
		}

		for(iIdx = 1; iIdx <= pData->iMaxHddNum; iIdx++) {
			if(pData->iProcessingIdx == iIdx) {
				continue;
			}

			iPrzPinVal = SYNO_CHECK_HDD_PRESENT(iIdx);

			if (iPrzPinVal) {
				continue;
			}

			mdelay(200);
			SYNO_CTRL_HDD_POWERON(iIdx, iPrzPinVal);
			pData->blHddEnStat[iIdx-1] = iPrzPinVal;
		}

		uiTimeout = SYNO_HDDMON_POLL_SEC * HZ;
		do{
			set_current_state(TASK_UNINTERRUPTIBLE);
			uiTimeout = schedule_timeout(uiTimeout);
		}while(uiTimeout);
	}

	iRet = 0;
END:
	return iRet;
}

static void syno_hddmon_task(SynoHddMonData_t *pData)
{
	int iIdx;
	int iPrzPinVal;
	static struct task_struct *pUnplugMonitor;
	unsigned int uiTimeout;

	if (NULL == pData) {
		goto END;
	}

	for(iIdx = 1; iIdx <= pData->iMaxHddNum; iIdx++) {
		pUnplugMonitor = NULL;
		pData->iProcessingIdx = iIdx;

		iPrzPinVal = SYNO_CHECK_HDD_PRESENT(iIdx);

		if(pData->blHddEnStat[iIdx-1] != iPrzPinVal) {

			if(iPrzPinVal) {
				//while starting a port, monitoring other ports for the disks unplugged
				pUnplugMonitor = kthread_run(syno_hddmon_unplug_monitor, pData, SYNO_HDDMON_UPLG_STR);
			}

			mdelay(200);
			SYNO_CTRL_HDD_POWERON(iIdx, iPrzPinVal);
			pData->blHddEnStat[iIdx-1] = iPrzPinVal;

			if(iPrzPinVal) {
				uiTimeout = SYNO_HDDMON_EN_WAIT_SEC * HZ;
				do{
					set_current_state(TASK_UNINTERRUPTIBLE);
					uiTimeout = schedule_timeout(uiTimeout);
				}while(uiTimeout);
			}

			if(NULL != pUnplugMonitor) {
				kthread_stop(pUnplugMonitor);
			}
		}
	}

END:
	return;
}

static void syno_hddmon_sync(SynoHddMonData_t *pData)
{
	int iIdx;
	int iPrzPinVal;

	if (NULL == pData) {
		goto END;
	}

	for(iIdx = 1; iIdx <= pData->iMaxHddNum; iIdx++) {
		pData->iProcessingIdx = iIdx;

		iPrzPinVal = SYNO_CHECK_HDD_PRESENT(iIdx);

		/* HDD Enable pins must be high just after boot-up,
		 * so turns the pins to low if the hdds do not present.
		 */
		if(!iPrzPinVal) {
			mdelay(200);
			SYNO_CTRL_HDD_POWERON(iIdx, iPrzPinVal);
			pData->blHddEnStat[iIdx-1] = iPrzPinVal;
		}

		/*sync the states*/
		pData->blHddEnStat[iIdx-1] = iPrzPinVal;

	}

END:
	return;
}
static int syno_hddmon_routine(void *args)
{
	int iRet = -1;
	SynoHddMonData_t *pData = NULL;
	unsigned int uiTimeout;

	if (NULL == args) {
		goto END;
	}

	pData = (SynoHddMonData_t *) args;

	while(1) {
		if (kthread_should_stop()) {
			break;
		}

		syno_hddmon_task(pData);

		uiTimeout = SYNO_HDDMON_POLL_SEC * HZ;
		do{
			set_current_state(TASK_UNINTERRUPTIBLE);
			uiTimeout = schedule_timeout(uiTimeout);
		}while(uiTimeout);
	}

	iRet = 0;
END:
	return iRet;
}

static int __init syno_hddmon_init(void)
{
	int iRet = -1;

	iRet = syno_hddmon_data_init(&synoHddMonData);
	if( 0 > iRet) {
		goto END;
	}

	syno_hddmon_sync(&synoHddMonData);

	/* processing */
	pHddPrzPolling = kthread_create(syno_hddmon_routine, &synoHddMonData, SYNO_HDDMON_STR);

	if (IS_ERR(pHddPrzPolling)) {
		iRet = PTR_ERR(pHddPrzPolling);
		pHddPrzPolling = NULL;
		goto END;
	}

	wake_up_process(pHddPrzPolling);

	printk("Syno_HddMon: Initialization completed.\n");

	iRet = 0;
END:
	return iRet;
}

static void __exit syno_hddman_exit(void)
{
	if(pHddPrzPolling) {
        printk("###\n");
        WARN_ON(1);
		kthread_stop(pHddPrzPolling);
	}
}

MODULE_AUTHOR("Yikai Peng");
MODULE_DESCRIPTION("Syno_HddMon\n");
MODULE_LICENSE("Synology Inc.");

module_init(syno_hddmon_init);
module_exit(syno_hddman_exit);
