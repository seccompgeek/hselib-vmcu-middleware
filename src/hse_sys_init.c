/**
 *   @file    hse_sys_init.c
 *
 *   @brief   ARM Cortex-M7 system initialisation helpers required by hse_host.c.
 *   @details Provides weak default implementations of sys_disableAllInterrupts /
 *            sys_enableAllInterrupts using the PRIMASK register so that the
 *            library is self-contained.  Applications (or BSPs) that already
 *            define these symbols supply a strong override which the linker
 *            will prefer automatically.
 *
 *   @addtogroup hse_sys_init
 *   @{
 */
/*==================================================================================================
*
*   Copyright 2022 NXP.
*
*   This software is owned or controlled by NXP and may only be used strictly in accordance with
*   the applicable license terms. By expressly accepting such terms or by downloading, installing,
*   activating and/or otherwise using the software, you are agreeing that you have read, and that
*   you agree to comply with and are bound by, such license terms. If you do not agree to
*   be bound by the applicable license terms, then you may not retain, install, activate or
*   otherwise use the software.
==================================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/
#include "hse_sys_init.h"

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

/**
 * @brief   Disable all maskable interrupts (set PRIMASK).
 * @details Matches the declaration in hse_sys_init.h.
 *          Marked __attribute__((weak)) so application BSP can override.
 */
__attribute__((weak)) void sys_disableAllInterrupts(void)
{
    __asm volatile ("cpsid i" : : : "memory");
}

/**
 * @brief   Re-enable all maskable interrupts (clear PRIMASK).
 * @details Marked __attribute__((weak)) so application BSP can override.
 */
__attribute__((weak)) void sys_enableAllInterrupts(void)
{
    __asm volatile ("cpsie i" : : : "memory");
}

#ifdef __cplusplus
}
#endif

/** @} */
