/* ****************************************************************
 * Portions Copyright 2004 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * linux_module_heap.c --
 *
 *	This file enables a module to have its own private heap, replete with
 *	posion-value checks, etc. to guarantee that modules and other kernel
 *	code don't step on each other's toes. This functionality is available to
 *	any module that compiles this code and links against it. Options
 *	specifying the size of the private heap, among other things, must be
 *	present within the module's Makefile, providing defined preprocessing
 *	values to the compiler. Thos preprocessing defines should be:
 *
 *	Required:
 *	
 *	- LINUX_MODULE_HEAP_INITIAL=<number-in-bytes-guaranteed-allocated>
 *	- LINUX_MODULE_HEAP_MAX=<number-in-bytes-of-maximum-allocation>
 *	- LINUX_MODULE_HEAP_NAME=<string>
 *
 *	Optional:
 *	
 *	- LINUX_MODULE_HEAP_NO_AUTO_LOAD
 *
 *	The optional preprocessing define should only be used in very rare
 *	cases. Those cases would be modules that want to perform additional
 *	operations inside vmk_early_init_module and vmk_late_cleanup_module. The
 *	first module that actually does this should most likely create a
 *	linux_module_heap.h file that exports the linux_module_heap_init and
 *	cleanup functions. That module should make sure to specify
 *	LINUX_MODULE_HEAP_NO_AUTO_LOAD in its Makefile and link against the
 *	newly created header file as well as its own .c file that contains new
 *	vmk_early_init_module and cleanup functions (that still call the
 *	linux_module_heap_init and cleanup functions).
 */

#ifndef LINUX_MODULE_HEAP_INITIAL
#error
#endif

#ifndef LINUX_MODULE_HEAP_MAX
#error
#endif

#ifndef LINUX_MODULE_HEAP_NAME
#error
#endif

#if LINUX_MODULE_HEAP_INITIAL > LINUX_MODULE_HEAP_MAX
#error
#endif

#if LINUX_MODULE_HEAP_INITIAL < 0
#error
#endif

#include <asm/page.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/module.h>

#include "vmkapi.h"

static vmk_HeapID moduleHeap = VMK_INVALID_HEAP_ID;

/*
 * The following functions - kfree and kmalloc, are implemented both here and in
 * vmklinux/linux_stubs.c. Most likely, if there are any changes made to these,
 * the changes should be propagated to the corresponding functions in vmklinux.
 * We need to define these functions here so that the drivers are able to
 * allocate memory from the appropriate driver heap memory.
 *
 * NOTE: We could have eliminated these functions and used the versions defined
 * in linux_stubs.c, but we need to distinguish between the memory allocated
 * for the drivers vs the memory allocated for vmklinux's internal use - say,
 * the call sequence is driver->vmklinux->kmalloc(), here the kmalloc() call
 * is on behalf of vmklinux, but there is no way to determine this from the
 * World->modStk since we don't push vmklinux's module id on to the stack.
 */

void
kfree(const void *p)
{
   if (p != NULL) {
      vmk_HeapFree(moduleHeap, (void *)p);
   }
}

void *
kmalloc(size_t size, int priority)
{
   size_t actual = 1;
   void *d;

   /*
    * Allocate blocks in powers-of-2 (like Linux), so we tolerate any
    * driver bugs that don't show up because of the large allocation size.
    */
   while (actual < size) {
      actual *= 2;
   }
   d = vmk_HeapAllocWithRA(moduleHeap, actual, __builtin_return_address(0));
   return d;
}

void*
kmalloc_align(size_t size, int priority, size_t align)
{
   size_t actual = 1;
   void *d;

   /*
    * Allocate blocks in powers-of-2 (like Linux), so we tolerate any
    * driver bugs that don't show up because of the large allocation size.
    */
   while (actual < size) {
      actual *= 2;
   }
   d = vmk_HeapAlignWithRA(moduleHeap, actual, align,
                           __builtin_return_address(0));
   return d;
}

/* kmem_cache implementation from Mike's infiniband port */

#define KMEM_CACHE_MAGIC	0xfa4b9c23

typedef struct kmem_cache_element {
   unsigned int magic;
   struct kmem_cache_element *next;
} kmem_cache_element;

struct kmem_cache_s {
   unsigned int size;
   kmem_cache_element *list;
   spinlock_t lock;
   void (*ctor)(void *, kmem_cache_t *, unsigned long);
};

/*
 *----------------------------------------------------------------------
 *
 * kmem_cache_create
 *
 *      Create a memory cache that will hold elements of a given size.
 *
 * Arguments
 *      ctor is a constructor that will initialize a cache element
 *           when allocated from the pool
 *      dtor is a destructor that would normally be used on an
 *           element before it is returned to the pool, but we do
 *           currently not implement it
 *
 * Results:
 *      A pointer to the cache to be used on subsequent calls to
 *           kmem_cache_{alloc, free, destroy} or NULL on error
 *
 * Side effects:
 *      A kmem cache heap object is allocated
 *
 *----------------------------------------------------------------------
 */

kmem_cache_t *
kmem_cache_create(const char *name , 
		  size_t size, size_t offset, unsigned long flags,
		  void (*ctor)(void *, kmem_cache_t *, unsigned long),
		  void (*dtor)(void *, kmem_cache_t *, unsigned long))
{
   kmem_cache_t *cache;

   if (dtor != NULL) {
      vmk_WarningMessage("kmem_cache_create: dtor != NULL\n");
      return NULL;
   }

   if (offset != 0) {
      vmk_WarningMessage("kmem_cache_create: offset = %d\n", offset);
      return NULL;
   }

   cache = (kmem_cache_t *)vmk_HeapAllocWithRA(moduleHeap, sizeof(kmem_cache_t),
                                               __builtin_return_address(0));
   if (cache == NULL) {
      vmk_WarningMessage("kmem_cache_create: out of memory\n");
      return NULL;
   }

   cache->size = size;
   cache->list = NULL;
   cache->ctor = ctor;

   // 
   // This lock can be grabbed with a linux spin lock held so we have to
   // be at least one higher in rank.
   //
   spin_lock_init(&cache->lock);

   return cache;
}

/*
 *----------------------------------------------------------------------
 *
 * kmem_cache_destroy
 *
 *      Deallocate all elements in a kmem cache and destroy the
 *      kmem_cache object itself
 *
 * Results:
 *      Always 0
 *
 * Side effects:
 *      All cached objects and the kmem_cache object itself are freed
 *
 *----------------------------------------------------------------------
 */

int 
kmem_cache_destroy(kmem_cache_t *cache)
{
   unsigned long cpu_flags;

   spin_lock_irqsave(&cache->lock, cpu_flags);
   while (cache->list != NULL) {
      kmem_cache_element *head = cache->list;
      cache->list = head->next;
      vmk_HeapFree(moduleHeap, head);
   }
   spin_unlock_irqrestore(&cache->lock, cpu_flags);
   spin_lock_destroy(&cache->lock);
   vmk_HeapFree(moduleHeap, cache);
   return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * kmem_cache_alloc
 *
 *      Allocate an element from the cache pool. If no free elements
 *      exist a new one will be allocated from the heap.
 *
 * Results:
 *      A pointer to the allocated object or NULL on error
 *
 * Side effects:
 *      Cache list may change or a new object is allocated from the heap
 *
 *----------------------------------------------------------------------
 */

void *
kmem_cache_alloc(kmem_cache_t *cache, int flags)
{
   void *res;
   kmem_cache_element *el;
   unsigned long cpu_flags;

   spin_lock_irqsave(&cache->lock, cpu_flags);

   if (cache->list == NULL) {
      el = (kmem_cache_element *)vmk_HeapAllocWithRA(moduleHeap, cache->size +
                                                     sizeof(kmem_cache_element),
                                                     __builtin_return_address(0));
      if (el == NULL) {
         vmk_WarningMessage("kmem_cache_alloc: out of memory\n");
         spin_unlock_irqrestore(&cache->lock, cpu_flags);
         return NULL;
      }
      el->magic = KMEM_CACHE_MAGIC;
   } else {
      el = cache->list;
      cache->list = el->next;
   }
   spin_unlock_irqrestore(&cache->lock, cpu_flags);

   res = (char *)el + sizeof(kmem_cache_element);
   if (cache->ctor != NULL) {
      cache->ctor(res, cache, 0);
   }

   return res;
}

/*
 *----------------------------------------------------------------------
 *
 * kmem_cache_free
 *
 *      Release an element to the object cache. The memory will not be
 *      freed until the cache is destroyed
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Cache list will change
 *
 *----------------------------------------------------------------------
 */

void 
kmem_cache_free(kmem_cache_t *cache, void *item)
{
   unsigned long flags;

   kmem_cache_element *el = 
      (kmem_cache_element *)((char *)item - sizeof(kmem_cache_element));

   if (el->magic != KMEM_CACHE_MAGIC) {
      panic("kmem_cache_free: Bad magic\n");
   }

   spin_lock_irqsave(&cache->lock, flags);

   el->next = cache->list;
   cache->list = el;

   spin_unlock_irqrestore(&cache->lock, flags);
}

#ifdef MODULE
static int heap_initial = LINUX_MODULE_HEAP_INITIAL;
MODULE_PARM(heap_initial, "i");
MODULE_PARM_DESC(heap_initial, "Initial heap size allocated for the driver.");

static int heap_max = LINUX_MODULE_HEAP_MAX;
MODULE_PARM(heap_max, "i");
MODULE_PARM_DESC(heap_max, "Maximum attainable heap size for the driver.");
#else
#error "You can only compile and link linux_module_heap with modules, which" \
       "means that MODULE has to be defined when compiling it..."
#endif

/*
 * The following two functions are used to create and destroy the private module
 * heap. The init function MUST be called before any other code in the module
 * has a chance to call kfree, kmalloc, etc. And the destroy function MUST be
 * called after those operations are all finished. 
 *
 */

int
linux_module_heap_init(void)
{
   vmk_uint32 moduleID;
   VMK_ReturnStatus status;

   moduleID = vmk_ModuleStackTop();

   if (heap_initial > heap_max) {
      heap_initial = heap_max;
      vmk_WarningMessage("Initial heap size > max. Limiting to max!!!\n");
   }

   vmk_LogMessage("Initial heap size : %d, max heap size: %d\n",
                  heap_initial, heap_max);
   status = vmk_HeapCreate(LINUX_MODULE_HEAP_NAME,
                           VMK_HEAP_LOW_MEM,
                           heap_initial,
                           heap_max,
                           &moduleHeap);

   if (status != VMK_OK) {
      vmk_LogMessage("vmk_HeapCreate failed \n");
      return -1;
   }

   vmk_ModuleSetHeapID(moduleID, moduleHeap);
   return 0;
}
 
int
linux_module_heap_cleanup(void)
{
   vmk_HeapDestroy(moduleHeap); 

   moduleHeap = VMK_INVALID_HEAP_ID;

   return 0;
}

/*
 * Modules can have the preceding two functions called automatically at the
 * right time by compiling and linking normally. To disable this automatic
 * functionality, you can specify LINUX_MODULE_HEAP_NO_AUTO_LOAD in your
 * Makefile, which will disable the following two functions. See the top of this
 * file for an explanation of why this might be useful.
 */

#ifndef LINUX_MODULE_HEAP_NO_AUTO_LOAD
int
vmk_early_init_module(void)
{
   return linux_module_heap_init();
}

int
vmk_late_cleanup_module(void)
{
   return linux_module_heap_cleanup();
}
#endif // LINUX_MODULE_HEAP_NO_AUTO_LOAD
