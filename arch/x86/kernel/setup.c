/*
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *
 *  Memory region support
 *	David Parsons <orc@pell.chi.il.us>, July-August 1999
 *
 *  Added E820 sanitization routine (removes overlapping memory regions);
 *  Brian Moyle <bmoyle@mvista.com>, February 2001
 *
 * Moved CPU detection code to cpu/${cpu}.c
 *    Patrick Mochel <mochel@osdl.org>, March 2002
 *
 *  Provisions for empty E820 memory regions (reported by certain BIOSes).
 *  Alex Achenbach <xela@slit.de>, December 2002.
 *
 */

/*
 * This file handles the architecture-dependent parts of initialization
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/screen_info.h>
#include <linux/ioport.h>
#include <linux/acpi.h>
#include <linux/sfi.h>
#include <linux/apm_bios.h>
#include <linux/initrd.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/seq_file.h>
#include <linux/console.h>
#include <linux/root_dev.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/efi.h>
#include <linux/init.h>
#include <linux/edd.h>
#include <linux/iscsi_ibft.h>
#include <linux/nodemask.h>
#include <linux/kexec.h>
#include <linux/dmi.h>
#include <linux/pfn.h>
#include <linux/pci.h>
#include <asm/pci-direct.h>
#include <linux/init_ohci1394_dma.h>
#include <linux/kvm_para.h>
#include <linux/dma-contiguous.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/delay.h>

#include <linux/kallsyms.h>
#include <linux/cpufreq.h>
#include <linux/dma-mapping.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>

#include <linux/percpu.h>
#include <linux/crash_dump.h>
#include <linux/tboot.h>
#include <linux/jiffies.h>

#include <video/edid.h>

#include <asm/mtrr.h>
#include <asm/apic.h>
#include <asm/realmode.h>
#include <asm/e820.h>
#include <asm/mpspec.h>
#include <asm/setup.h>
#include <asm/efi.h>
#include <asm/timer.h>
#include <asm/i8259.h>
#include <asm/sections.h>
#include <asm/io_apic.h>
#include <asm/ist.h>
#include <asm/setup_arch.h>
#include <asm/bios_ebda.h>
#include <asm/cacheflush.h>
#include <asm/processor.h>
#include <asm/bugs.h>

#include <asm/vsyscall.h>
#include <asm/cpu.h>
#include <asm/desc.h>
#include <asm/dma.h>
#include <asm/iommu.h>
#include <asm/gart.h>
#include <asm/mmu_context.h>
#include <asm/proto.h>

#include <asm/paravirt.h>
#include <asm/hypervisor.h>
#include <asm/olpc_ofw.h>

#include <asm/percpu.h>
#include <asm/topology.h>
#include <asm/apicdef.h>
#include <asm/amd_nb.h>
#include <asm/mce.h>
#include <asm/alternative.h>
#include <asm/prom.h>

#if defined(CONFIG_SYNO_ATA_PWR_CTRL) && defined(CONFIG_SYNO_X64)
#include  <linux/synobios.h>

extern int grgPwrCtlPin[];
#ifdef CONFIG_SYNO_X86_PINCTRL_GPIO
#include <linux/gpio.h>
#endif /* CONFIG_SYNO_X86_PINCTRL_GPIO */

#ifdef CONFIG_SYNO_ICH_GPIO_CTRL
extern u32 syno_pch_lpc_gpio_pin(int pin, int *pValue, int isWrite);
#endif /* CONFIG_SYNO_ICH_GPIO_CTRL */
#endif /* CONFIG_SYNO_ATA_PWR_CTRL && CONFIG_SYNO_X64 */

#ifdef CONFIG_SYNO_SATA_PORT_MAP
extern char gszSataPortMap[8];
#endif /* CONFIG_SYNO_SATA_PORT_MAP */

#ifdef CONFIG_SYNO_DISK_INDEX_MAP
extern char gszDiskIdxMap[16];
#endif /* CONFIG_SYNO_DISK_INDEX_MAP */

#ifdef CONFIG_SYNO_FIXED_DISK_NAME_MV14XX
extern char gszDiskIdxMapMv14xx[8];
#endif /* CONFIG_SYNO_FIXED_DISK_NAME_MV14XX */

#ifdef CONFIG_SYNO_DYN_MODULE_INSTALL
extern int gSynoHasDynModule;
#endif /*CONFIG_SYNO_DYN_MODULE_INSTALL*/

#ifdef  CONFIG_SYNO_HW_REVISION
extern char gszSynoHWRevision[];
#endif /* CONFIG_SYNO_HW_REVISION */

#ifdef CONFIG_SYNO_HW_VERSION
extern char gszSynoHWVersion[];
#endif /* CONFIG_SYNO_HW_VERSION */

#ifdef CONFIG_SYNO_INTERNAL_HD_NUM
extern long g_internal_hd_num;
#endif /* CONFIG_SYNO_INTERNAL_HD_NUM */

#ifdef CONFIG_SYNO_AHCI_SWITCH
extern char g_ahci_switch;
#endif /* CONFIG_SYNO_AHCI_SWITCH */

#ifdef CONFIG_SYNO_HDD_HOTPLUG
extern long g_hdd_hotplug;
#endif /* CONFIG_SYNO_HDD_HOTPLUG */

#ifdef CONFIG_SYNO_MAC_ADDRESS
extern unsigned char grgbLanMac[CONFIG_SYNO_MAC_MAX][16];
extern int giVenderFormatVersion;
extern char gszSkipVenderMacInterfaces[256];
#endif /* CONFIG_SYNO_MAC_ADDRESS */

#ifdef CONFIG_SYNO_SERIAL
extern char gszSerialNum[32];
extern char gszCustomSerialNum[32];
#endif /* CONFIG_SYNO_SERIAL */

#ifdef CONFIG_SYNO_SATA_DISK_SEQ_REVERSE
extern char giDiskSeqReverse[8];
#endif /* CONFIG_SYNO_SATA_DISK_SEQ_REVERSE */

#ifdef CONFIG_SYNO_INTERNAL_NETIF_NUM
extern long g_internal_netif_num;
#endif /* CONFIG_SYNO_INTERNAL_NETIF_NUM*/

#ifdef CONFIG_SYNO_SATA_MV_LED
extern long g_sata_mv_led;
#endif /* CONFIG_SYNO_SATA_MV_LED */

#ifdef CONFIG_SYNO_SAS_DISK_NAME
extern long g_is_sas_model;
#endif /* CONFIG_SYNO_SAS_DISK_NAME */

#ifdef CONFIG_SYNO_DUAL_HEAD
extern int gSynoDualHead;
#endif /* CONFIG_SYNO_DUAL_HEAD */

#ifdef CONFIG_SYNO_SAS_RESERVATION_WRITE_CONFLICT_KERNEL_PANIC
extern int gSynoSASWriteConflictPanic;
#endif /* CONFIG_SYNO_SAS_RESERVATION_WRITE_CONFLICT_KERNEL_PANIC */

#ifdef CONFIG_SYNO_BOOT_SATA_DOM
extern int gSynoBootSATADOM;
#endif /* CONFIG_SYNO_BOOT_SATA_DOM */

#ifdef CONFIG_SYNO_FACTORY_USB_FAST_RESET
extern int gSynoFactoryUSBFastReset;
#endif /* CONFIG_SYNO_FACTORY_USB_FAST_RESET */

#ifdef CONFIG_SYNO_FACTORY_USB3_DISABLE
extern int gSynoFactoryUSB3Disable;
#endif /* CONFIG_SYNO_FACTORY_USB3_DISABLE */

#ifdef CONFIG_XPENO_SYNOBOOT_ID
extern int gSynoVid;
extern int gSynoPid;
#endif

#ifdef CONFIG_SYNO_CASTRATED_XHC
extern char gSynoCastratedXhcAddr[CONFIG_SYNO_NUM_CASTRATED_XHC][13];
extern unsigned int gSynoCastratedXhcPortBitmap[CONFIG_SYNO_NUM_CASTRATED_XHC];
#endif /* CONFIG_SYNO_CASTRATED_XHC */

/*
 * max_low_pfn_mapped: highest direct mapped pfn under 4GB
 * max_pfn_mapped:     highest direct mapped pfn over 4GB
 *
 * The direct mapping only covers E820_RAM regions, so the ranges and gaps are
 * represented by pfn_mapped
 */
unsigned long max_low_pfn_mapped;
unsigned long max_pfn_mapped;

#ifdef CONFIG_DMI
RESERVE_BRK(dmi_alloc, 65536);
#endif

static __initdata unsigned long _brk_start = (unsigned long)__brk_base;
unsigned long _brk_end = (unsigned long)__brk_base;

#ifdef CONFIG_X86_64
int default_cpu_present_to_apicid(int mps_cpu)
{
	return __default_cpu_present_to_apicid(mps_cpu);
}

int default_check_phys_apicid_present(int phys_apicid)
{
	return __default_check_phys_apicid_present(phys_apicid);
}
#endif

struct boot_params boot_params;

/*
 * Machine setup..
 */
static struct resource data_resource = {
	.name	= "Kernel data",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

static struct resource code_resource = {
	.name	= "Kernel code",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

static struct resource bss_resource = {
	.name	= "Kernel bss",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

#ifdef CONFIG_X86_32
/* cpu data as detected by the assembly code in head.S */
struct cpuinfo_x86 new_cpu_data __cpuinitdata = {
	.wp_works_ok = -1,
};
/* common cpu data for all cpus */
struct cpuinfo_x86 boot_cpu_data __read_mostly = {
	.wp_works_ok = -1,
};
EXPORT_SYMBOL(boot_cpu_data);

unsigned int def_to_bigsmp;

/* for MCA, but anyone else can use it if they want */
unsigned int machine_id;
unsigned int machine_submodel_id;
unsigned int BIOS_revision;

struct apm_info apm_info;
EXPORT_SYMBOL(apm_info);

#if defined(CONFIG_X86_SPEEDSTEP_SMI) || \
	defined(CONFIG_X86_SPEEDSTEP_SMI_MODULE)
struct ist_info ist_info;
EXPORT_SYMBOL(ist_info);
#else
struct ist_info ist_info;
#endif

#else
struct cpuinfo_x86 boot_cpu_data __read_mostly = {
	.x86_phys_bits = MAX_PHYSMEM_BITS,
};
EXPORT_SYMBOL(boot_cpu_data);
#endif

#if !defined(CONFIG_X86_PAE) || defined(CONFIG_X86_64)
unsigned long mmu_cr4_features;
#else
unsigned long mmu_cr4_features = X86_CR4_PAE;
#endif

/* Boot loader ID and version as integers, for the benefit of proc_dointvec */
int bootloader_type, bootloader_version;

/*
 * Setup options
 */
struct screen_info screen_info;
EXPORT_SYMBOL(screen_info);
struct edid_info edid_info;
EXPORT_SYMBOL_GPL(edid_info);

extern int root_mountflags;

unsigned long saved_video_mode;

#define RAMDISK_IMAGE_START_MASK	0x07FF
#define RAMDISK_PROMPT_FLAG		0x8000
#define RAMDISK_LOAD_FLAG		0x4000

static char __initdata command_line[COMMAND_LINE_SIZE];
#ifdef CONFIG_CMDLINE_BOOL
static char __initdata builtin_cmdline[COMMAND_LINE_SIZE] = CONFIG_CMDLINE;
#endif

#if defined(CONFIG_EDD) || defined(CONFIG_EDD_MODULE)
struct edd edd;
#ifdef CONFIG_EDD_MODULE
EXPORT_SYMBOL(edd);
#endif
/**
 * copy_edd() - Copy the BIOS EDD information
 *              from boot_params into a safe place.
 *
 */
static inline void __init copy_edd(void)
{
     memcpy(edd.mbr_signature, boot_params.edd_mbr_sig_buffer,
	    sizeof(edd.mbr_signature));
     memcpy(edd.edd_info, boot_params.eddbuf, sizeof(edd.edd_info));
     edd.mbr_signature_nr = boot_params.edd_mbr_sig_buf_entries;
     edd.edd_info_nr = boot_params.eddbuf_entries;
}
#else
static inline void __init copy_edd(void)
{
}
#endif

void * __init extend_brk(size_t size, size_t align)
{
	size_t mask = align - 1;
	void *ret;

	BUG_ON(_brk_start == 0);
	BUG_ON(align & mask);

	_brk_end = (_brk_end + mask) & ~mask;
	BUG_ON((char *)(_brk_end + size) > __brk_limit);

	ret = (void *)_brk_end;
	_brk_end += size;

	memset(ret, 0, size);

	return ret;
}

#if defined(CONFIG_SYNO_ATA_PWR_CTRL) && defined(CONFIG_SYNO_X64)
/*
 * Synology sata power control functions
 */
#define SYNO_MAX_HDD_PRZ	4
#define GPIO_UNDEF			0xFF

/* SYNO_GET_HDD_ENABLE_PIN
 * Query HDD power control pin for x86_64, cedarview, avoton, braswell
 * input: index - disk index, 1-based
 * return: Pin Number
 */
static u8 SYNO_GET_HDD_ENABLE_PIN(const int index)
{
	u8 ret = GPIO_UNDEF;

#if defined(CONFIG_SYNO_CEDARVIEW)
	u8 HddEnPinMap[] = {16, 20, 21, 32};
#elif defined(CONFIG_SYNO_AVOTON)
	u8 HddEnPinMap[] = {10, 15, 16, 17};
#elif defined(CONFIG_SYNO_BRASWELL)
	u8 HddEnPinMap[] = {61, 60, 64, 58};
#else
	u8 *HddEnPinMap = NULL;
#endif

	/* Check support HDD enable pin*/
	if (NULL == HddEnPinMap) {
		goto END;
	}

	if (1 > index || (0 < g_internal_hd_num && g_internal_hd_num < index)) {
		printk("SYNO_GET_HDD_ENABLE_PIN(%d) is illegal", index);
		WARN_ON(1);
		goto END;
	}

	ret = HddEnPinMap[index-1];

END:
	return ret;
}

static u32 SYNO_X86_GPIO_PIN_SET(int pin, int *pValue)
{
	u32 ret = 0;
#if defined(CONFIG_SYNO_ICH_GPIO_CTRL)
	ret = syno_pch_lpc_gpio_pin(pin, pValue, 1);
#elif defined(CONFIG_SYNO_X86_PINCTRL_GPIO)
	ret = syno_gpio_value_set(pin, *pValue);
#endif
	return ret;
}

/* SYNO_CTRL_HDD_POWERON
 * HDD power control for x86_64 and cedarview
 * input: index - disk index, 1-based, 0 for all hdd.
 *        value - 0 for off, 1 for on.
 */
int SYNO_CTRL_HDD_POWERON(int index, int value)
{
	int iRet = -EINVAL;

	if(syno_is_hw_version(HW_DS712pv20) ||
	   syno_is_hw_version(HW_DS712pv10)) {
		switch(index){
			case 0:
				/* index is 1-based, so apply 0 for all*/
				SYNO_X86_GPIO_PIN_SET(15 , &value);
				mdelay(200);
				SYNO_X86_GPIO_PIN_SET(25 , &value);
				break;
			case 1:
				SYNO_X86_GPIO_PIN_SET(15 , &value);
				break;
			case 2:
				SYNO_X86_GPIO_PIN_SET(25 , &value);
				break;
			default:
				goto END;
		}
	}else if(syno_is_hw_version(HW_DS412p) ||
		 syno_is_hw_version(HW_DS415p) ||
		 syno_is_hw_version(HW_DS416p)) {
		switch(index){
			case 0:
				/* index is 1-based, so apply 0 for all*/
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(1), &value);
				mdelay(200);
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(2) , &value);
				mdelay(200);
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(3) , &value);
				mdelay(200);
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(4) , &value);
				break;
			case 1 ... 4:
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(index) , &value);
				break;
			default:
				goto END;
		}
	}else if(syno_is_hw_version(HW_DS713p) ||
		syno_is_hw_version(HW_DS716p) ||
		syno_is_hw_version(HW_DS216p)) {
		switch(index){
			case 0:
				/* index is 1-based, so apply 0 for all*/
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(1) , &value);
				mdelay(200);
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(2) , &value);
				break;
			case 1 ... 2:
				SYNO_X86_GPIO_PIN_SET(SYNO_GET_HDD_ENABLE_PIN(index) , &value);
				break;
			default:
				goto END;
		}
	/* Add models below with else if*/
	} else {
		goto END;
	}

	iRet = 0;
END:
	return iRet;
}

/* SYNO_GET_HDD_PRESENT_PIN
 * Query HDD present  pin for x86_64 and cedarview
 * input: index - disk index, 1-based.
 * return: Pin Number,
 */
static u8 SYNO_GET_HDD_PRESENT_PIN(const int index)
{
	u8 ret = GPIO_UNDEF;

#if defined(CONFIG_SYNO_CEDARVIEW)
	u8 przPinMap[]   = {33, 35, 49, 18};
#elif defined(CONFIG_SYNO_AVOTON)
	u8 przPinMap[] = {18, 28, 34, 44};
#elif defined(CONFIG_SYNO_BRASWELL)
	u8 przPinMap[] = {56, 59, 63, 57};
#else
	u8 *przPinMap = NULL;
#endif

	/* Check support HDD present pin*/
	if (NULL == przPinMap) {
		goto END;
	}

	if (1 > index || (0 < g_internal_hd_num && g_internal_hd_num < index)) {
		printk("SYNO_GET_HDD_PRESENT_PIN(%d) is illegal", index);
		WARN_ON(1);
		goto END;
	}

	ret = przPinMap[index-1];

END:
	return ret;
}

/* SYNO_CHECK_HDD_PRESENT
 * Check HDD present for x86_64, cedarview, Avoton, Braswell
 * input : index - disk index, 1-based.
 * output: 0 - HDD not present, 1 - HDD present.
 */
int SYNO_CHECK_HDD_PRESENT(int index)
{
	int iPrzVal = 1; /*defult is persent*/
	u8 iPin = SYNO_GET_HDD_PRESENT_PIN(index);

	/* please check spec with HW */
#if defined(CONFIG_SYNO_AVOTON) || defined(CONFIG_SYNO_BRASWELL)
	const int iInverseValue = 1;
#else
	const int iInverseValue = 0;
#endif

	if (GPIO_UNDEF == iPin) {
		goto END;
	}

	/* Check is internal disk*/
	if (0 < g_internal_hd_num && g_internal_hd_num < index) {
		goto END;
	}

#if defined(CONFIG_SYNO_ICH_GPIO_CTRL)
	syno_pch_lpc_gpio_pin(iPin, &iPrzVal, 0);
#elif defined(CONFIG_SYNO_X86_PINCTRL_GPIO)
	syno_gpio_value_get(iPin, &iPrzVal);
#endif

	if (iInverseValue) {
		if (iPrzVal) {
			iPrzVal = 0;
		} else {
			iPrzVal = 1;
		}
	}

END:
	return iPrzVal;
}

/* SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER
 * Query support HDD dynamic Power
 * output: 0 - not support, 1 - support.
 */
int SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER(void)
{
	int iRet = 0;

	if (0 > g_internal_hd_num || SYNO_MAX_HDD_PRZ < g_internal_hd_num) {
		goto END;
	}

	if (syno_is_hw_version(HW_DS712pv20) ||
	    syno_is_hw_version(HW_DS712pv10) ||
	    syno_is_hw_version(HW_DS412p) ||
	    syno_is_hw_version(HW_DS415p) ||
	    syno_is_hw_version(HW_DS416p) ||
	    syno_is_hw_version(HW_DS713p) ||
		syno_is_hw_version(HW_DS716p) ||
		syno_is_hw_version(HW_DS216p)) {
		iRet = 1;
	} else {
		goto END;
	}

END:
	return iRet;
}

EXPORT_SYMBOL(SYNO_CTRL_HDD_POWERON);
EXPORT_SYMBOL(SYNO_CHECK_HDD_PRESENT);
EXPORT_SYMBOL(SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER);
#endif /* CONFIG_SYNO_ATA_PWR_CTRL && CONFIG_SYNO_X64 */

#ifdef CONFIG_SYNO_SAS_ENCOLURE_PWR_CTL
/* Export Sysctl interface for RXD1215sas power control
 *
 * Drive GPIO20 to low for poweroff process. udev will
 * use this interface when when encolure plugged in.
 */
int SynoProcEncPwrCtl(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int iValue = 0;

	if (write) {
		if (-1 == SYNO_X86_GPIO_PIN_SET(20, &iValue)) {
			printk("fail to drive PCH GPIO20 to low\n");
		}
	}

	return proc_dointvec(table, write, buffer, lenp, ppos);
}
EXPORT_SYMBOL(SynoProcEncPwrCtl);
#endif /* CONFIG_SYNO_SAS_ENCOLURE_PWR_CTL */

#ifdef CONFIG_X86_32
static void __init cleanup_highmap(void)
{
}
#endif

static void __init reserve_brk(void)
{
	if (_brk_end > _brk_start)
		memblock_reserve(__pa_symbol(_brk_start),
				 _brk_end - _brk_start);

	/* Mark brk area as locked down and no longer taking any
	   new allocations */
	_brk_start = 0;
}

#ifdef CONFIG_BLK_DEV_INITRD

static u64 __init get_ramdisk_image(void)
{
	u64 ramdisk_image = boot_params.hdr.ramdisk_image;

	ramdisk_image |= (u64)boot_params.ext_ramdisk_image << 32;

	return ramdisk_image;
}
static u64 __init get_ramdisk_size(void)
{
	u64 ramdisk_size = boot_params.hdr.ramdisk_size;

	ramdisk_size |= (u64)boot_params.ext_ramdisk_size << 32;

	return ramdisk_size;
}

#define MAX_MAP_CHUNK	(NR_FIX_BTMAPS << PAGE_SHIFT)
static void __init relocate_initrd(void)
{
	/* Assume only end is not page aligned */
	u64 ramdisk_image = get_ramdisk_image();
	u64 ramdisk_size  = get_ramdisk_size();
	u64 area_size     = PAGE_ALIGN(ramdisk_size);
	u64 ramdisk_here;
	unsigned long slop, clen, mapaddr;
	char *p, *q;

	/* We need to move the initrd down into directly mapped mem */
	ramdisk_here = memblock_find_in_range(0, PFN_PHYS(max_pfn_mapped),
						 area_size, PAGE_SIZE);

	if (!ramdisk_here)
		panic("Cannot find place for new RAMDISK of size %lld\n",
			 ramdisk_size);

	/* Note: this includes all the mem currently occupied by
	   the initrd, we rely on that fact to keep the data intact. */
	memblock_reserve(ramdisk_here, area_size);
	initrd_start = ramdisk_here + PAGE_OFFSET;
	initrd_end   = initrd_start + ramdisk_size;
	printk(KERN_INFO "Allocated new RAMDISK: [mem %#010llx-%#010llx]\n",
			 ramdisk_here, ramdisk_here + ramdisk_size - 1);

	q = (char *)initrd_start;

	/* Copy the initrd */
	while (ramdisk_size) {
		slop = ramdisk_image & ~PAGE_MASK;
		clen = ramdisk_size;
		if (clen > MAX_MAP_CHUNK-slop)
			clen = MAX_MAP_CHUNK-slop;
		mapaddr = ramdisk_image & PAGE_MASK;
		p = early_memremap(mapaddr, clen+slop);
		memcpy(q, p+slop, clen);
		early_iounmap(p, clen+slop);
		q += clen;
		ramdisk_image += clen;
		ramdisk_size  -= clen;
	}

	ramdisk_image = get_ramdisk_image();
	ramdisk_size  = get_ramdisk_size();
	printk(KERN_INFO "Move RAMDISK from [mem %#010llx-%#010llx] to"
		" [mem %#010llx-%#010llx]\n",
		ramdisk_image, ramdisk_image + ramdisk_size - 1,
		ramdisk_here, ramdisk_here + ramdisk_size - 1);
}

static void __init early_reserve_initrd(void)
{
	/* Assume only end is not page aligned */
	u64 ramdisk_image = get_ramdisk_image();
	u64 ramdisk_size  = get_ramdisk_size();
	u64 ramdisk_end   = PAGE_ALIGN(ramdisk_image + ramdisk_size);

	if (!boot_params.hdr.type_of_loader ||
	    !ramdisk_image || !ramdisk_size)
		return;		/* No initrd provided by bootloader */

	memblock_reserve(ramdisk_image, ramdisk_end - ramdisk_image);
}
static void __init reserve_initrd(void)
{
	/* Assume only end is not page aligned */
	u64 ramdisk_image = get_ramdisk_image();
	u64 ramdisk_size  = get_ramdisk_size();
	u64 ramdisk_end   = PAGE_ALIGN(ramdisk_image + ramdisk_size);
	u64 mapped_size;

	if (!boot_params.hdr.type_of_loader ||
	    !ramdisk_image || !ramdisk_size)
		return;		/* No initrd provided by bootloader */

	initrd_start = 0;

	mapped_size = memblock_mem_size(max_pfn_mapped);
	if (ramdisk_size >= (mapped_size>>1))
		panic("initrd too large to handle, "
		       "disabling initrd (%lld needed, %lld available)\n",
		       ramdisk_size, mapped_size>>1);

	printk(KERN_INFO "RAMDISK: [mem %#010llx-%#010llx]\n", ramdisk_image,
			ramdisk_end - 1);

	if (pfn_range_is_mapped(PFN_DOWN(ramdisk_image),
				PFN_DOWN(ramdisk_end))) {
		/* All are mapped, easy case */
		initrd_start = ramdisk_image + PAGE_OFFSET;
		initrd_end = initrd_start + ramdisk_size;
		return;
	}

	relocate_initrd();

	memblock_free(ramdisk_image, ramdisk_end - ramdisk_image);
}
#else
static void __init early_reserve_initrd(void)
{
}
static void __init reserve_initrd(void)
{
}
#endif /* CONFIG_BLK_DEV_INITRD */

static void __init parse_setup_data(void)
{
	struct setup_data *data;
	u64 pa_data, pa_next;

	pa_data = boot_params.hdr.setup_data;
	while (pa_data) {
		u32 data_len, map_len, data_type;

		map_len = max(PAGE_SIZE - (pa_data & ~PAGE_MASK),
			      (u64)sizeof(struct setup_data));
		data = early_memremap(pa_data, map_len);
		data_len = data->len + sizeof(struct setup_data);
		data_type = data->type;
		pa_next = data->next;
		early_iounmap(data, map_len);

		switch (data_type) {
		case SETUP_E820_EXT:
			parse_e820_ext(pa_data, data_len);
			break;
		case SETUP_DTB:
			add_dtb(pa_data);
			break;
		default:
			break;
		}
		pa_data = pa_next;
	}
}

static void __init e820_reserve_setup_data(void)
{
	struct setup_data *data;
	u64 pa_data;
	int found = 0;

	pa_data = boot_params.hdr.setup_data;
	while (pa_data) {
		data = early_memremap(pa_data, sizeof(*data));
		e820_update_range(pa_data, sizeof(*data)+data->len,
			 E820_RAM, E820_RESERVED_KERN);
		found = 1;
		pa_data = data->next;
		early_iounmap(data, sizeof(*data));
	}
	if (!found)
		return;

	sanitize_e820_map(e820.map, ARRAY_SIZE(e820.map), &e820.nr_map);
	memcpy(&e820_saved, &e820, sizeof(struct e820map));
	printk(KERN_INFO "extended physical RAM map:\n");
	e820_print_map("reserve setup_data");
}

static void __init memblock_x86_reserve_range_setup_data(void)
{
	struct setup_data *data;
	u64 pa_data;

	pa_data = boot_params.hdr.setup_data;
	while (pa_data) {
		data = early_memremap(pa_data, sizeof(*data));
		memblock_reserve(pa_data, sizeof(*data) + data->len);
		pa_data = data->next;
		early_iounmap(data, sizeof(*data));
	}
}

#ifdef CONFIG_SYNO_SATA_PORT_MAP
static int __init early_sataport_map(char *p)
{
	snprintf(gszSataPortMap, sizeof(gszSataPortMap), "%s", p);

	if(0 != gszSataPortMap[0]) {
		printk("Sata Port Map: %s\n", gszSataPortMap);
	}

	return 1;
}
__setup("SataPortMap=", early_sataport_map);
#endif /* CONFIG_SYNO_SATA_PORT_MAP */

#ifdef CONFIG_SYNO_DISK_INDEX_MAP
static int __init early_disk_idx_map(char *p)
{
	snprintf(gszDiskIdxMap, sizeof(gszDiskIdxMap), "%s", p);

	if('\0' != gszDiskIdxMap[0]) {
		printk("Disk Index Map: %s\n", gszDiskIdxMap);
	}

	return 1;
}
__setup("DiskIdxMap=", early_disk_idx_map);
#endif /* CONFIG_SYNO_DISK_INDEX_MAP */

#ifdef CONFIG_SYNO_FIXED_DISK_NAME_MV14XX
static int __init early_disk_idx_map_mv14xx(char *p)
{
	snprintf(gszDiskIdxMapMv14xx, sizeof(gszDiskIdxMapMv14xx), "%s", p);

	if('\0' != gszDiskIdxMapMv14xx[0]) {
		printk("Disk Indx Map on MV14xx: %s\n", gszDiskIdxMapMv14xx);
	}

	return 1;
}
__setup("DiskIdxMapMv14xx=", early_disk_idx_map_mv14xx);
#endif /* CONFIG_SYNO_FIXED_DISK_NAME_MV14XX */

#ifdef CONFIG_SYNO_HW_REVISION
static int __init early_hw_revision(char *p)
{
	snprintf(gszSynoHWRevision, 4, "%s", p);

	printk("Synology Hardware Revision: %s\n", gszSynoHWRevision);

	return 1;
}
__setup("rev=", early_hw_revision);
#endif /* CONFIG_SYNO_HW_REVISION */

#ifdef CONFIG_SYNO_DYN_MODULE_INSTALL
static int __init early_is_dyn_module(char *p)
{
	int iLen = 0;

	gSynoHasDynModule = 1;

	if ((NULL == p) || (0 == (iLen = strlen(p)))) {
		goto END;
	}

	if ( 0 == strcmp (p, "n")) {
		gSynoHasDynModule = 0;
		printk("Synology Dynamic Module support disabled.\n");
	}

END:
	return 1;
}
__setup("syno_dyn_module=", early_is_dyn_module);
#endif /* CONFIG_SYNO_DYN_MODULE_INSTALL */

#ifdef CONFIG_SYNO_HW_VERSION
static int __init early_hw_version(char *p)
{
	char *szPtr;

	snprintf(gszSynoHWVersion, 16, "%s", p);

	szPtr = gszSynoHWVersion;
	while ((*szPtr != ' ') && (*szPtr != '\t') && (*szPtr != '\0')) {
		szPtr++;
	}
	*szPtr = 0;
	strcat(gszSynoHWVersion, "-j");

	printk("Synology Hardware Version: %s\n", gszSynoHWVersion);

	return 1;
}
__setup("syno_hw_version=", early_hw_version);
#endif /* CONFIG_SYNO_HW_VERSION */

#ifdef CONFIG_SYNO_INTERNAL_HD_NUM
static int __init early_internal_hd_num(char *p)
{
	g_internal_hd_num = simple_strtol(p, NULL, 10);

	printk("Internal HD num: %d\n", (int)g_internal_hd_num);

    return 1;
}
__setup("ihd_num=", early_internal_hd_num);
#endif /* CONFIG_SYNO_INTERNAL_HD_NUM */

#ifdef  CONFIG_SYNO_AHCI_SWITCH
static int __init early_ahci_switch(char *p)
{
	g_ahci_switch = p[0];
	if ('0' == g_ahci_switch) {
		printk("AHCI: 0\n");
	} else {
		printk("AHCI: 1\n");
	}

	return 1;
}
__setup("ahci=", early_ahci_switch);
#endif /* CONFIG_SYNO_AHCI_SWITCH */

#ifdef CONFIG_SYNO_HDD_HOTPLUG
static int __init early_hdd_hotplug(char *p)
{
	g_hdd_hotplug = simple_strtol(p, NULL, 10);

	if ( g_hdd_hotplug > 0 ) {
		printk("Support HDD Hotplug.\n");
	}

	return 1;
}
__setup("HddHotplug=", early_hdd_hotplug);
#endif /* CONFIG_SYNO_HDD_HOTPLUG */
#ifdef CONFIG_SYNO_MAC_ADDRESS
static int __init early_mac1(char *p)
{
	snprintf(grgbLanMac[0], sizeof(grgbLanMac[0]), "%s", p);

	printk("Mac1: %s\n", grgbLanMac[0]);

	return 1;
}
__setup("mac1=", early_mac1);

static int __init early_mac2(char *p)
{
	snprintf(grgbLanMac[1], sizeof(grgbLanMac[1]), "%s", p);

	printk("Mac2: %s\n", grgbLanMac[1]);

	return 1;
}
__setup("mac2=", early_mac2);

static int __init early_mac3(char *p)
{
	snprintf(grgbLanMac[2], sizeof(grgbLanMac[2]), "%s", p);

	printk("Mac3: %s\n", grgbLanMac[2]);

	return 1;
}
__setup("mac3=", early_mac3);

static int __init early_mac4(char *p)
{
	snprintf(grgbLanMac[3], sizeof(grgbLanMac[3]), "%s", p);

	printk("Mac4: %s\n", grgbLanMac[3]);

	return 1;
}
__setup("mac4=", early_mac4);

static int __init early_macs(char *p)
{
	int iMacCount = 0;
	char *pBegin = p;
	char *pEnd = strstr(pBegin, ",");

	while (NULL != pEnd && CONFIG_SYNO_MAC_MAX > iMacCount) {
		*pEnd = '\0';
		snprintf(grgbLanMac[iMacCount], sizeof(grgbLanMac[iMacCount]), "%s", pBegin);
		pBegin = pEnd + 1;
		pEnd = strstr(pBegin, ",");
		iMacCount++;
	}

	if ('\0' != *pBegin && CONFIG_SYNO_MAC_MAX > iMacCount) {
		snprintf(grgbLanMac[iMacCount], sizeof(grgbLanMac[iMacCount]), "%s", pBegin);
	}

	return 1;
}
__setup("macs=", early_macs);

static int __init early_skip_vender_mac_interfaces(char *p)
{
	snprintf(gszSkipVenderMacInterfaces, sizeof(gszSkipVenderMacInterfaces), "%s", p);

	printk("Skip vender mac interfaces: %s\n", gszSkipVenderMacInterfaces);

	return 1;
}
__setup("skip_vender_mac_interfaces=", early_skip_vender_mac_interfaces);

static int __init early_vender_format_version(char *p)
{
	giVenderFormatVersion = simple_strtol(p, NULL, 10);

	printk("Vender format version: %d\n", giVenderFormatVersion);

	return 1;
}
__setup("vender_format_version=", early_vender_format_version);
#endif /* CONFIG_SYNO_MAC_ADDRESS */

#ifdef CONFIG_SYNO_SERIAL
static int __init early_sn(char *p)
{
	snprintf(gszSerialNum, sizeof(gszSerialNum), "%s", p);
	printk("Serial Number: %s\n", gszSerialNum);
	return 1;
}
__setup("sn=", early_sn);

static int __init early_custom_sn(char *p)
{
	snprintf(gszCustomSerialNum, sizeof(gszCustomSerialNum), "%s", p);
	printk("Custom Serial Number: %s\n", gszCustomSerialNum);
	return 1;
}
__setup("custom_sn=", early_custom_sn);
#endif /* CONFIG_SYNO_SERIAL */

#ifdef CONFIG_SYNO_FACTORY_USB_FAST_RESET
static int __init early_factory_usb_fast_reset(char *p)
{
	gSynoFactoryUSBFastReset = simple_strtol(p, NULL, 10);

	printk("Factory USB Fast Reset: %d\n", (int)gSynoFactoryUSBFastReset);

	return 1;
}
__setup("syno_usb_fast_reset=", early_factory_usb_fast_reset);
#endif /* CONFIG_SYNO_FACTORY_USB_FAST_RESET */

#ifdef CONFIG_SYNO_FACTORY_USB3_DISABLE
static int __init early_factory_usb3_disable(char *p)
{
	gSynoFactoryUSB3Disable = simple_strtol(p, NULL, 10);

	printk("Factory USB3 Disable: %d\n", (int)gSynoFactoryUSB3Disable);

	return 1;
}
__setup("syno_disable_usb3=", early_factory_usb3_disable);
#endif /* CONFIG_SYNO_FACTORY_USB3_DISABLE */

#ifdef CONFIG_SYNO_CASTRATED_XHC
static int __init early_castrated_xhc(char *p)
{
	int iCount = 0;
	char *pBegin = p;
	char *pEnd = strstr(pBegin, ",");
	char *pPortSep = NULL;

	while (iCount < CONFIG_SYNO_NUM_CASTRATED_XHC) {
		if(NULL != pEnd)
			*pEnd = '\0';
		pPortSep = strstr(pBegin, "@");
		if (pPortSep == NULL) {
			printk("Castrated xHC - Parameter format not correct\n");
			break;
		}
		*pPortSep = '\0';
		snprintf(gSynoCastratedXhcAddr[iCount],
				sizeof(gSynoCastratedXhcAddr[iCount]), "%s", pBegin);
		gSynoCastratedXhcPortBitmap[iCount] = simple_strtoul(pPortSep + 1, NULL,
				16);
		if (NULL == pEnd)
			break;
		pBegin = pEnd + 1;
		pEnd = strstr(pBegin, ",");
		iCount++;
	}

	return 1;
}
__setup("syno_castrated_xhc=", early_castrated_xhc);
#endif /* CONFIG_SYNO_CASTRATED_XHC */

#ifdef CONFIG_SYNO_SATA_DISK_SEQ_REVERSE
static int __init early_disk_seq_reserve(char *p)
{
	snprintf(giDiskSeqReverse, sizeof(giDiskSeqReverse), "%s", p);

	if('\0' != giDiskSeqReverse[0]) {
		printk("Disk Sequence Reverse: %s\n", giDiskSeqReverse);
	}

	return 1;
}
__setup("DiskSeqReverse=", early_disk_seq_reserve);
#endif /* CONFIG_SYNO_SATA_DISK_SEQ_REVERSE */

#ifdef CONFIG_SYNO_INTERNAL_NETIF_NUM
static int __init early_internal_netif_num(char *p)
{
	g_internal_netif_num = simple_strtol(p, NULL, 10);

	if ( g_internal_netif_num >= 0 ) {
		printk("Internal netif num: %d\n", (int)g_internal_netif_num);
	}

	return 1;
}
__setup("netif_num=", early_internal_netif_num);
#endif /*CONFIG_SYNO_INTERNAL_NETIF_NUM*/

#ifdef CONFIG_SYNO_SATA_MV_LED
static int __init early_sataled_special(char *p)
{
	g_sata_mv_led = simple_strtol(p, NULL, 10);

	if ( g_sata_mv_led >= 0 ) {
		printk("Special Sata LEDs.\n");
	}

	return 1;
}
__setup("SataLedSpecial=", early_sataled_special);
#endif /* CONFIG_SYNO_SATA_MV_LED */

#ifdef CONFIG_SYNO_SAS_DISK_NAME
static int __init early_SASmodel(char *p)
{
	g_is_sas_model = simple_strtol(p, NULL, 10);

	if (1 == g_is_sas_model) {
		printk("SAS model: %d\n", (int)g_is_sas_model);
	}

	return 1;
}
__setup("SASmodel=", early_SASmodel);
#endif /* CONFIG_SYNO_SAS_DISK_NAME */

#ifdef CONFIG_SYNO_DUAL_HEAD
static int __init early_dual_head(char *p)
{
	gSynoDualHead = simple_strtol(p, NULL, 10);
#ifdef CONFIG_SYNO_BOOT_SATA_DOM
	gSynoBootSATADOM = gSynoDualHead;
#endif /* CONFIG_SYNO_BOOT_SATA_DOM */
#ifdef CONFIG_SYNO_SAS_RESERVATION_WRITE_CONFLICT_KERNEL_PANIC
	gSynoSASWriteConflictPanic = gSynoDualHead;
#endif

	printk("Synology Dual Head: %d\n", gSynoDualHead);

	return 1;
}
__setup("dual_head=", early_dual_head);
#endif /* CONFIG_SYNO_DUAL_HEAD */

#ifdef CONFIG_SYNO_SAS_RESERVATION_WRITE_CONFLICT_KERNEL_PANIC
static int __init early_sas_reservation_write_conflict(char *p)
{
	gSynoSASWriteConflictPanic = simple_strtol(p, NULL, 10);

	printk("Let kernel panic if sas reservation write conflict: %d\n", gSynoSASWriteConflictPanic);

	return 1;
}
__setup("sas_reservation_write_conflict=", early_sas_reservation_write_conflict);
#endif /* CONFIG_SYNO_SAS_RESERVATION_WRITE_CONFLICT_KERNEL_PANIC */

#ifdef CONFIG_SYNO_BOOT_SATA_DOM
static int __init early_synoboot_satadom(char *p)
{
	gSynoBootSATADOM = simple_strtol(p, NULL, 10);

	printk("Synology boot device SATADOM: %d\n", gSynoBootSATADOM);

	return 1;
}
__setup("synoboot_satadom=", early_synoboot_satadom);
#endif /* CONFIG_SYNO_BOOT_SATA_DOM */

#ifdef CONFIG_XPENO_SYNOBOOT_ID
static int __init early_vid(char *p)
{
	gSynoVid = simple_strtol(p, NULL, 16);

	printk("synoboot vid: %d\n", gSynoVid);

	return 1;
}
__setup("vid=", early_vid);

static int __init early_pid(char *p)
{
	gSynoPid = simple_strtol(p, NULL, 16);

	printk("synoboot pid: %d\n", gSynoPid);

	return 1;
}
__setup("pid=", early_pid);
#endif /* CONFIG_XPENO_SYNOBOOT_ID */

/*
 * --------- Crashkernel reservation ------------------------------
 */

#ifdef CONFIG_KEXEC

/*
 * Keep the crash kernel below this limit.  On 32 bits earlier kernels
 * would limit the kernel to the low 512 MiB due to mapping restrictions.
 * On 64bit, old kexec-tools need to under 896MiB.
 */
#ifdef CONFIG_X86_32
# define CRASH_KERNEL_ADDR_LOW_MAX	(512 << 20)
# define CRASH_KERNEL_ADDR_HIGH_MAX	(512 << 20)
#else
# define CRASH_KERNEL_ADDR_LOW_MAX	(896UL<<20)
# define CRASH_KERNEL_ADDR_HIGH_MAX	MAXMEM
#endif

static void __init reserve_crashkernel_low(void)
{
#ifdef CONFIG_X86_64
	const unsigned long long alignment = 16<<20;	/* 16M */
	unsigned long long low_base = 0, low_size = 0;
	unsigned long total_low_mem;
	unsigned long long base;
	bool auto_set = false;
	int ret;

	total_low_mem = memblock_mem_size(1UL<<(32-PAGE_SHIFT));
	/* crashkernel=Y,low */
	ret = parse_crashkernel_low(boot_command_line, total_low_mem,
						&low_size, &base);
	if (ret != 0) {
		/*
		 * two parts from lib/swiotlb.c:
		 *	swiotlb size: user specified with swiotlb= or default.
		 *	swiotlb overflow buffer: now is hardcoded to 32k.
		 *		We round it to 8M for other buffers that
		 *		may need to stay low too.
		 */
		low_size = swiotlb_size_or_default() + (8UL<<20);
		auto_set = true;
	} else {
		/* passed with crashkernel=0,low ? */
		if (!low_size)
			return;
	}

	low_base = memblock_find_in_range(low_size, (1ULL<<32),
					low_size, alignment);

	if (!low_base) {
		if (!auto_set)
			pr_info("crashkernel low reservation failed - No suitable area found.\n");

		return;
	}

	memblock_reserve(low_base, low_size);
	pr_info("Reserving %ldMB of low memory at %ldMB for crashkernel (System low RAM: %ldMB)\n",
			(unsigned long)(low_size >> 20),
			(unsigned long)(low_base >> 20),
			(unsigned long)(total_low_mem >> 20));
	crashk_low_res.start = low_base;
	crashk_low_res.end   = low_base + low_size - 1;
	insert_resource(&iomem_resource, &crashk_low_res);
#endif
}

static void __init reserve_crashkernel(void)
{
	const unsigned long long alignment = 16<<20;	/* 16M */
	unsigned long long total_mem;
	unsigned long long crash_size, crash_base;
	bool high = false;
	int ret;

	total_mem = memblock_phys_mem_size();

	/* crashkernel=XM */
	ret = parse_crashkernel(boot_command_line, total_mem,
			&crash_size, &crash_base);
	if (ret != 0 || crash_size <= 0) {
		/* crashkernel=X,high */
		ret = parse_crashkernel_high(boot_command_line, total_mem,
				&crash_size, &crash_base);
		if (ret != 0 || crash_size <= 0)
			return;
		high = true;
	}

	/* 0 means: find the address automatically */
	if (crash_base <= 0) {
		/*
		 *  kexec want bzImage is below CRASH_KERNEL_ADDR_MAX
		 */
		crash_base = memblock_find_in_range(alignment,
					high ? CRASH_KERNEL_ADDR_HIGH_MAX :
					       CRASH_KERNEL_ADDR_LOW_MAX,
					crash_size, alignment);

		if (!crash_base) {
			pr_info("crashkernel reservation failed - No suitable area found.\n");
			return;
		}

	} else {
		unsigned long long start;

		start = memblock_find_in_range(crash_base,
				 crash_base + crash_size, crash_size, 1<<20);
		if (start != crash_base) {
			pr_info("crashkernel reservation failed - memory is in use.\n");
			return;
		}
	}
	memblock_reserve(crash_base, crash_size);

	printk(KERN_INFO "Reserving %ldMB of memory at %ldMB "
			"for crashkernel (System RAM: %ldMB)\n",
			(unsigned long)(crash_size >> 20),
			(unsigned long)(crash_base >> 20),
			(unsigned long)(total_mem >> 20));

	crashk_res.start = crash_base;
	crashk_res.end   = crash_base + crash_size - 1;
	insert_resource(&iomem_resource, &crashk_res);

	if (crash_base >= (1ULL<<32))
		reserve_crashkernel_low();
}
#else
static void __init reserve_crashkernel(void)
{
}
#endif

static struct resource standard_io_resources[] = {
	{ .name = "dma1", .start = 0x00, .end = 0x1f,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "pic1", .start = 0x20, .end = 0x21,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "timer0", .start = 0x40, .end = 0x43,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "timer1", .start = 0x50, .end = 0x53,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "keyboard", .start = 0x60, .end = 0x60,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "keyboard", .start = 0x64, .end = 0x64,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "dma page reg", .start = 0x80, .end = 0x8f,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "pic2", .start = 0xa0, .end = 0xa1,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "dma2", .start = 0xc0, .end = 0xdf,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO },
	{ .name = "fpu", .start = 0xf0, .end = 0xff,
		.flags = IORESOURCE_BUSY | IORESOURCE_IO }
};

void __init reserve_standard_io_resources(void)
{
	int i;

	/* request I/O space for devices used on all i[345]86 PCs */
	for (i = 0; i < ARRAY_SIZE(standard_io_resources); i++)
		request_resource(&ioport_resource, &standard_io_resources[i]);

}

static __init void reserve_ibft_region(void)
{
	unsigned long addr, size = 0;

	addr = find_ibft_region(&size);

	if (size)
		memblock_reserve(addr, size);
}

static bool __init snb_gfx_workaround_needed(void)
{
#ifdef CONFIG_PCI
	int i;
	u16 vendor, devid;
	static const __initconst u16 snb_ids[] = {
		0x0102,
		0x0112,
		0x0122,
		0x0106,
		0x0116,
		0x0126,
		0x010a,
	};

	/* Assume no if something weird is going on with PCI */
	if (!early_pci_allowed())
		return false;

	vendor = read_pci_config_16(0, 2, 0, PCI_VENDOR_ID);
	if (vendor != 0x8086)
		return false;

	devid = read_pci_config_16(0, 2, 0, PCI_DEVICE_ID);
	for (i = 0; i < ARRAY_SIZE(snb_ids); i++)
		if (devid == snb_ids[i])
			return true;
#endif

	return false;
}

/*
 * Sandy Bridge graphics has trouble with certain ranges, exclude
 * them from allocation.
 */
static void __init trim_snb_memory(void)
{
	static const __initconst unsigned long bad_pages[] = {
		0x20050000,
		0x20110000,
		0x20130000,
		0x20138000,
		0x40004000,
	};
	int i;

	if (!snb_gfx_workaround_needed())
		return;

	printk(KERN_DEBUG "reserving inaccessible SNB gfx pages\n");

	/*
	 * Reserve all memory below the 1 MB mark that has not
	 * already been reserved.
	 */
	memblock_reserve(0, 1<<20);

	for (i = 0; i < ARRAY_SIZE(bad_pages); i++) {
		if (memblock_reserve(bad_pages[i], PAGE_SIZE))
			printk(KERN_WARNING "failed to reserve 0x%08lx\n",
			       bad_pages[i]);
	}
}

/*
 * Here we put platform-specific memory range workarounds, i.e.
 * memory known to be corrupt or otherwise in need to be reserved on
 * specific platforms.
 *
 * If this gets used more widely it could use a real dispatch mechanism.
 */
static void __init trim_platform_memory_ranges(void)
{
	trim_snb_memory();
}

static void __init trim_bios_range(void)
{
	/*
	 * A special case is the first 4Kb of memory;
	 * This is a BIOS owned area, not kernel ram, but generally
	 * not listed as such in the E820 table.
	 *
	 * This typically reserves additional memory (64KiB by default)
	 * since some BIOSes are known to corrupt low memory.  See the
	 * Kconfig help text for X86_RESERVE_LOW.
	 */
	e820_update_range(0, PAGE_SIZE, E820_RAM, E820_RESERVED);

	/*
	 * special case: Some BIOSen report the PC BIOS
	 * area (640->1Mb) as ram even though it is not.
	 * take them out.
	 */
	e820_remove_range(BIOS_BEGIN, BIOS_END - BIOS_BEGIN, E820_RAM, 1);

	sanitize_e820_map(e820.map, ARRAY_SIZE(e820.map), &e820.nr_map);
}

/* called before trim_bios_range() to spare extra sanitize */
static void __init e820_add_kernel_range(void)
{
	u64 start = __pa_symbol(_text);
	u64 size = __pa_symbol(_end) - start;

	/*
	 * Complain if .text .data and .bss are not marked as E820_RAM and
	 * attempt to fix it by adding the range. We may have a confused BIOS,
	 * or the user may have used memmap=exactmap or memmap=xxM$yyM to
	 * exclude kernel range. If we really are running on top non-RAM,
	 * we will crash later anyways.
	 */
	if (e820_all_mapped(start, start + size, E820_RAM))
		return;

	pr_warn(".text .data .bss are not marked as E820_RAM!\n");
	e820_remove_range(start, size, E820_RAM, 0);
	e820_add_region(start, size, E820_RAM);
}

static unsigned reserve_low = CONFIG_X86_RESERVE_LOW << 10;

static int __init parse_reservelow(char *p)
{
	unsigned long long size;

	if (!p)
		return -EINVAL;

	size = memparse(p, &p);

	if (size < 4096)
		size = 4096;

	if (size > 640*1024)
		size = 640*1024;

	reserve_low = size;

	return 0;
}

early_param("reservelow", parse_reservelow);

static void __init trim_low_memory_range(void)
{
	memblock_reserve(0, ALIGN(reserve_low, PAGE_SIZE));
}

/*
 * Determine if we were loaded by an EFI loader.  If so, then we have also been
 * passed the efi memmap, systab, etc., so we should use these data structures
 * for initialization.  Note, the efi init code path is determined by the
 * global efi_enabled. This allows the same kernel image to be used on existing
 * systems (with a traditional BIOS) as well as on EFI systems.
 */
/*
 * setup_arch - architecture-specific boot-time initializations
 *
 * Note: On x86_64, fixmaps are ready for use even before this is called.
 */

void __init setup_arch(char **cmdline_p)
{
	memblock_reserve(__pa_symbol(_text),
			 (unsigned long)__bss_stop - (unsigned long)_text);

	early_reserve_initrd();

	/*
	 * At this point everything still needed from the boot loader
	 * or BIOS or kernel text should be early reserved or marked not
	 * RAM in e820. All other memory is free game.
	 */

#ifdef CONFIG_X86_32
	memcpy(&boot_cpu_data, &new_cpu_data, sizeof(new_cpu_data));
	visws_early_detect();

	/*
	 * copy kernel address range established so far and switch
	 * to the proper swapper page table
	 */
	clone_pgd_range(swapper_pg_dir     + KERNEL_PGD_BOUNDARY,
			initial_page_table + KERNEL_PGD_BOUNDARY,
			KERNEL_PGD_PTRS);

	load_cr3(swapper_pg_dir);
	__flush_tlb_all();
#else
	printk(KERN_INFO "Command line: %s\n", boot_command_line);
#endif

	/*
	 * If we have OLPC OFW, we might end up relocating the fixmap due to
	 * reserve_top(), so do this before touching the ioremap area.
	 */
	olpc_ofw_detect();

	early_trap_init();
	early_cpu_init();
	early_ioremap_init();

	setup_olpc_ofw_pgd();

	ROOT_DEV = old_decode_dev(boot_params.hdr.root_dev);
	screen_info = boot_params.screen_info;
	edid_info = boot_params.edid_info;
#ifdef CONFIG_X86_32
	apm_info.bios = boot_params.apm_bios_info;
	ist_info = boot_params.ist_info;
	if (boot_params.sys_desc_table.length != 0) {
		machine_id = boot_params.sys_desc_table.table[0];
		machine_submodel_id = boot_params.sys_desc_table.table[1];
		BIOS_revision = boot_params.sys_desc_table.table[2];
	}
#endif
	saved_video_mode = boot_params.hdr.vid_mode;
	bootloader_type = boot_params.hdr.type_of_loader;
	if ((bootloader_type >> 4) == 0xe) {
		bootloader_type &= 0xf;
		bootloader_type |= (boot_params.hdr.ext_loader_type+0x10) << 4;
	}
	bootloader_version  = bootloader_type & 0xf;
	bootloader_version |= boot_params.hdr.ext_loader_ver << 4;

#ifdef CONFIG_BLK_DEV_RAM
	rd_image_start = boot_params.hdr.ram_size & RAMDISK_IMAGE_START_MASK;
	rd_prompt = ((boot_params.hdr.ram_size & RAMDISK_PROMPT_FLAG) != 0);
	rd_doload = ((boot_params.hdr.ram_size & RAMDISK_LOAD_FLAG) != 0);
#endif
#ifdef CONFIG_EFI
	if (!strncmp((char *)&boot_params.efi_info.efi_loader_signature,
		     "EL32", 4)) {
		set_bit(EFI_BOOT, &x86_efi_facility);
	} else if (!strncmp((char *)&boot_params.efi_info.efi_loader_signature,
		     "EL64", 4)) {
		set_bit(EFI_BOOT, &x86_efi_facility);
		set_bit(EFI_64BIT, &x86_efi_facility);
	}

	if (efi_enabled(EFI_BOOT))
		efi_memblock_x86_reserve_range();
#endif

	x86_init.oem.arch_setup();

	iomem_resource.end = (1ULL << boot_cpu_data.x86_phys_bits) - 1;
	setup_memory_map();
	parse_setup_data();
	/* update the e820_saved too */
	e820_reserve_setup_data();

	copy_edd();

	if (!boot_params.hdr.root_flags)
		root_mountflags &= ~MS_RDONLY;
	init_mm.start_code = (unsigned long) _text;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = _brk_end;

	code_resource.start = __pa_symbol(_text);
	code_resource.end = __pa_symbol(_etext)-1;
	data_resource.start = __pa_symbol(_etext);
	data_resource.end = __pa_symbol(_edata)-1;
	bss_resource.start = __pa_symbol(__bss_start);
	bss_resource.end = __pa_symbol(__bss_stop)-1;

#ifdef CONFIG_CMDLINE_BOOL
#ifdef CONFIG_CMDLINE_OVERRIDE
	strlcpy(boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
#else
	if (builtin_cmdline[0]) {
		/* append boot loader cmdline to builtin */
		strlcat(builtin_cmdline, " ", COMMAND_LINE_SIZE);
		strlcat(builtin_cmdline, boot_command_line, COMMAND_LINE_SIZE);
		strlcpy(boot_command_line, builtin_cmdline, COMMAND_LINE_SIZE);
	}
#endif
#endif

	strlcpy(command_line, boot_command_line, COMMAND_LINE_SIZE);
	*cmdline_p = command_line;

	/*
	 * x86_configure_nx() is called before parse_early_param() to detect
	 * whether hardware doesn't support NX (so that the early EHCI debug
	 * console setup can safely call set_fixmap()). It may then be called
	 * again from within noexec_setup() during parsing early parameters
	 * to honor the respective command line option.
	 */
	x86_configure_nx();

	parse_early_param();

	x86_report_nx();

	/* after early param, so could get panic from serial */
	memblock_x86_reserve_range_setup_data();

	if (acpi_mps_check()) {
#ifdef CONFIG_X86_LOCAL_APIC
		disable_apic = 1;
#endif
		setup_clear_cpu_cap(X86_FEATURE_APIC);
	}

#ifdef CONFIG_PCI
	if (pci_early_dump_regs)
		early_dump_pci_devices();
#endif

	finish_e820_parsing();

	if (efi_enabled(EFI_BOOT))
		efi_init();

	dmi_scan_machine();
	dmi_set_dump_stack_arch_desc();

	/*
	 * VMware detection requires dmi to be available, so this
	 * needs to be done after dmi_scan_machine, for the BP.
	 */
	init_hypervisor_platform();

	x86_init.resources.probe_roms();

	/* after parse_early_param, so could debug it */
	insert_resource(&iomem_resource, &code_resource);
	insert_resource(&iomem_resource, &data_resource);
	insert_resource(&iomem_resource, &bss_resource);

	e820_add_kernel_range();
	trim_bios_range();
#ifdef CONFIG_X86_32
	if (ppro_with_ram_bug()) {
		e820_update_range(0x70000000ULL, 0x40000ULL, E820_RAM,
				  E820_RESERVED);
		sanitize_e820_map(e820.map, ARRAY_SIZE(e820.map), &e820.nr_map);
		printk(KERN_INFO "fixed physical RAM map:\n");
		e820_print_map("bad_ppro");
	}
#else
	early_gart_iommu_check();
#endif

	/*
	 * partially used pages are not usable - thus
	 * we are rounding upwards:
	 */
	max_pfn = e820_end_of_ram_pfn();

	/* update e820 for memory not covered by WB MTRRs */
	mtrr_bp_init();
	if (mtrr_trim_uncached_memory(max_pfn))
		max_pfn = e820_end_of_ram_pfn();

#ifdef CONFIG_X86_32
	/* max_low_pfn get updated here */
	find_low_pfn_range();
#else
	num_physpages = max_pfn;

	check_x2apic();

	/* How many end-of-memory variables you have, grandma! */
	/* need this before calling reserve_initrd */
	if (max_pfn > (1UL<<(32 - PAGE_SHIFT)))
		max_low_pfn = e820_end_of_low_ram_pfn();
	else
		max_low_pfn = max_pfn;

	high_memory = (void *)__va(max_pfn * PAGE_SIZE - 1) + 1;
#endif

	/*
	 * Find and reserve possible boot-time SMP configuration:
	 */
	find_smp_config();

	reserve_ibft_region();

	early_alloc_pgt_buf();

	/*
	 * Need to conclude brk, before memblock_x86_fill()
	 *  it could use memblock_find_in_range, could overlap with
	 *  brk area.
	 */
	reserve_brk();

	cleanup_highmap();

	memblock.current_limit = ISA_END_ADDRESS;
	memblock_x86_fill();

	/*
	 * The EFI specification says that boot service code won't be called
	 * after ExitBootServices(). This is, in fact, a lie.
	 */
	if (efi_enabled(EFI_MEMMAP))
		efi_reserve_boot_services();

	/* preallocate 4k for mptable mpc */
	early_reserve_e820_mpc_new();

#ifdef CONFIG_X86_CHECK_BIOS_CORRUPTION
	setup_bios_corruption_check();
#endif

#ifdef CONFIG_X86_32
	printk(KERN_DEBUG "initial memory mapped: [mem 0x00000000-%#010lx]\n",
			(max_pfn_mapped<<PAGE_SHIFT) - 1);
#endif

	reserve_real_mode();

	trim_platform_memory_ranges();
	trim_low_memory_range();

	init_mem_mapping();

	early_trap_pf_init();

	setup_real_mode();

	memblock.current_limit = get_max_mapped();
	dma_contiguous_reserve(0);

	/*
	 * NOTE: On x86-32, only from this point on, fixmaps are ready for use.
	 */

#ifdef CONFIG_PROVIDE_OHCI1394_DMA_INIT
	if (init_ohci1394_dma_early)
		init_ohci1394_dma_on_all_controllers();
#endif
	/* Allocate bigger log buffer */
	setup_log_buf(1);

	reserve_initrd();

#if defined(CONFIG_ACPI) && defined(CONFIG_BLK_DEV_INITRD)
	acpi_initrd_override((void *)initrd_start, initrd_end - initrd_start);
#endif

	reserve_crashkernel();

	vsmp_init();

	io_delay_init();

	/*
	 * Parse the ACPI tables for possible boot-time SMP configuration.
	 */
	acpi_boot_table_init();

	early_acpi_boot_init();

	initmem_init();
	memblock_find_dma_reserve();

#ifdef CONFIG_KVM_GUEST
	kvmclock_init();
#endif

	x86_init.paging.pagetable_init();

	if (boot_cpu_data.cpuid_level >= 0) {
		/* A CPU has %cr4 if and only if it has CPUID */
		mmu_cr4_features = read_cr4();
		if (trampoline_cr4_features)
			*trampoline_cr4_features = mmu_cr4_features;
	}

#ifdef CONFIG_X86_32
	/* sync back kernel address range */
	clone_pgd_range(initial_page_table + KERNEL_PGD_BOUNDARY,
			swapper_pg_dir     + KERNEL_PGD_BOUNDARY,
			KERNEL_PGD_PTRS);
#endif

	tboot_probe();

#ifdef CONFIG_X86_64
	map_vsyscall();
#endif

	generic_apic_probe();

	early_quirks();

	/*
	 * Read APIC and some other early information from ACPI tables.
	 */
	acpi_boot_init();
	sfi_init();
	x86_dtb_init();

	/*
	 * get boot-time SMP configuration:
	 */
	if (smp_found_config)
		get_smp_config();

	prefill_possible_map();

	init_cpu_to_node();

	init_apic_mappings();
	if (x86_io_apic_ops.init)
		x86_io_apic_ops.init();

	kvm_guest_init();

	e820_reserve_resources();
	e820_mark_nosave_regions(max_low_pfn);

	x86_init.resources.reserve_resources();

	e820_setup_gap();

#ifdef CONFIG_VT
#if defined(CONFIG_VGA_CONSOLE)
	if (!efi_enabled(EFI_BOOT) || (efi_mem_type(0xa0000) != EFI_CONVENTIONAL_MEMORY))
		conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif
	x86_init.oem.banner();

	x86_init.timers.wallclock_init();

	mcheck_init();

	arch_init_ideal_nops();

	register_refined_jiffies(CLOCK_TICK_RATE);

#ifdef CONFIG_EFI
	/* Once setup is done above, unmap the EFI memory map on
	 * mismatched firmware/kernel archtectures since there is no
	 * support for runtime services.
	 */
	if (efi_enabled(EFI_BOOT) && !efi_is_native()) {
		pr_info("efi: Setup done, disabling due to 32/64-bit mismatch\n");
		efi_unmap_memmap();
	}
#endif
}

#ifdef CONFIG_X86_32

static struct resource video_ram_resource = {
	.name	= "Video RAM area",
	.start	= 0xa0000,
	.end	= 0xbffff,
	.flags	= IORESOURCE_BUSY | IORESOURCE_MEM
};

void __init i386_reserve_resources(void)
{
	request_resource(&iomem_resource, &video_ram_resource);
	reserve_standard_io_resources();
}

#endif /* CONFIG_X86_32 */
