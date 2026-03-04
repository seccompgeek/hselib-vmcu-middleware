/**
 *   @file    hse_irq_integration.h
 *
 *   @brief   HSE MU interrupt integration helpers for AUTOSAR RTD-style projects.
 *   @details Provides:
 *              - Named ISR entry-point functions to wire into Vector_Table.s
 *                for the S32K344 HSE MU0/MU1 RX and ORED interrupt lines.
 *              - HSE_RegisterMuInterrupts() to enable and prioritise those
 *                lines in the NVIC.
 *
 *            Minimal integration steps
 *            -------------------------
 *            1. In Vector_Table.s replace the four `undefined_handler` entries
 *               at IRQ 193, 194, 196 and 197 with the ISR names declared here.
 *               See the comment block below for the exact lines to change.
 *
 *            2. Call HSE_RegisterMuInterrupts() once before using any HSE
 *               service in interrupt-driven (asynchronous or sync-with-irq)
 *               mode.  An appropriate place is at the top of main().
 *               No separate #include is needed — this header is already
 *               pulled in by hselib.h.
 *
 *   @addtogroup HSE_Integration
 *   @{
 */
/*==================================================================================================
*   Copyright 2022-2025 NXP.
*   Source code licensed under applicable license terms.
==================================================================================================*/

#ifndef HSE_IRQ_INTEGRATION_H
#define HSE_IRQ_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                         INCLUDE FILES
==================================================================================================*/
#include "hse_S32K344_COMMON.h"   /* HSE_MU0_RX_IRQn / HSE_MU0_ORED_IRQn etc. */
#include "hse_common_types.h"     /* uint8_t via hse_std_typedefs               */

/*==================================================================================================
*                                       DEFINES AND MACROS
==================================================================================================*/

/**
 * @brief Default NVIC priority assigned to all HSE MU interrupts.
 *        Override before including this header if another priority is needed.
 *        Allowed range: 0 (highest) … 15 (lowest) on S32K344.
 */
#ifndef HSE_MU_IRQ_PRIORITY
#define HSE_MU_IRQ_PRIORITY  (1U)
#endif

/*==================================================================================================
*                               ISR ENTRY-POINT DECLARATIONS
*
* Wire these symbols into Vector_Table.s by replacing the four
* 'undefined_handler' entries that correspond to IRQ 193-194 (MU0) and
* IRQ 196-197 (MU1).
*
* The block in the peripheral section of Vector_Table.s looks like this
* (IRQ numbers shown as comments; the file itself has no IRQ annotations
* beyond the grouped (NNN) markers):
*
*   .long undefined_handler          <- IRQ 190  (leave unchanged)
*   .long undefined_handler          <- IRQ 191  (leave unchanged)
*   .long undefined_handler          <- IRQ 192  (leave unchanged)
*   .long HSE_MU0_RxIRQHandler       <- IRQ 193  HSE_MU0_RX_IRQn
*   .long HSE_MU0_OredIRQHandler     <- IRQ 194  HSE_MU0_ORED_IRQn
*   .long undefined_handler          <- IRQ 195  (leave unchanged)
*   .long HSE_MU1_RxIRQHandler       <- IRQ 196  HSE_MU1_RX_IRQn
*   .long HSE_MU1_OredIRQHandler     <- IRQ 197  HSE_MU1_ORED_IRQn
*   .long undefined_handler          <- IRQ 198  (leave unchanged)
*   .long undefined_handler          <- IRQ 199  (leave unchanged)
*   .long undefined_handler          <- IRQ 200  (leave unchanged)
*
* Also add these four lines near the top of Vector_Table.s where the other
* .globl directives live:
*
*   .globl HSE_MU0_RxIRQHandler
*   .globl HSE_MU0_OredIRQHandler
*   .globl HSE_MU1_RxIRQHandler
*   .globl HSE_MU1_OredIRQHandler
*
==================================================================================================*/

/** @brief ISR for HSE_MU0_RX_IRQn  (193) – HSE MU-0 receive (response ready). */
void HSE_MU0_RxIRQHandler(void);

/** @brief ISR for HSE_MU0_ORED_IRQn (194) – HSE MU-0 general-purpose / event. */
void HSE_MU0_OredIRQHandler(void);

/** @brief ISR for HSE_MU1_RX_IRQn  (196) – HSE MU-1 receive (response ready). */
void HSE_MU1_RxIRQHandler(void);

/** @brief ISR for HSE_MU1_ORED_IRQn (197) – HSE MU-1 general-purpose / event. */
void HSE_MU1_OredIRQHandler(void);

/*==================================================================================================
*                                     FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief   Enable and prioritise the HSE MU interrupts in the NVIC.
 * @details Sets HSE_MU_IRQ_PRIORITY for all active MU RX and ORED interrupt
 *          lines and enables them.  Call once before using HSE services in
 *          interrupt-driven mode.  The VTOR must already point to a valid
 *          vector table containing the HSE ISR entry points declared above.
 *
 *          Safe to call from privileged Thread or Handler mode (no SVC needed).
 */
void HSE_RegisterMuInterrupts(void);

#ifdef __cplusplus
}
#endif

#endif /* HSE_IRQ_INTEGRATION_H */

/** @} */
