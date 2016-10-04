#ifndef __A_OUT_GNU_H__
#define __A_OUT_GNU_H__

#include <uapi/linux/a.out.h>

#ifndef __ASSEMBLY__
#if defined (M_OLDSUN2)
#else
#endif
#if defined (M_68010)
#else
#endif
#if defined (M_68020)
#else
#endif
#if defined (M_SPARC)
#else
#endif
#ifdef linux
#include <asm/page.h>
#if defined(__i386__) || defined(__mc68000__)
#else
#ifndef SEGMENT_SIZE
#define SEGMENT_SIZE	PAGE_SIZE
#endif
#endif
#endif
#if !defined (N_RELOCATION_INFO_DECLARED)
#ifdef NS32K
#else
#endif
#endif /* no N_RELOCATION_INFO_DECLARED.  */
#endif /*__ASSEMBLY__ */
#endif /* __A_OUT_GNU_H__ */
