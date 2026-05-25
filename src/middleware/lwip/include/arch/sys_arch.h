/**
 * @file    sys_arch.h
 * @brief   LwIP OS abstraction layer — NO_SYS (raw API) mode
 *
 * When NO_SYS=1, LwIP runs without an OS and does not use semaphores,
 * mailboxes, or threads. This header provides stub definitions.
 */
#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "lwip/opt.h"

#if NO_SYS

/* Stub types for raw API mode */
typedef u8_t sys_prot_t;

#else

/* OS mode types (placeholder — extend when NO_SYS=0) */
typedef u32_t sys_prot_t;

struct _sys_mutex {
  void *mut;
};
typedef struct _sys_mutex sys_mutex_t;

struct _sys_sem {
  void *sem;
};
typedef struct _sys_sem sys_sem_t;

struct _sys_mbox {
  void *mbx;
};
typedef struct _sys_mbox sys_mbox_t;

struct _sys_thread {
  void *thread_handle;
};
typedef struct _sys_thread sys_thread_t;

#endif /* NO_SYS */

#endif /* LWIP_ARCH_SYS_ARCH_H */
