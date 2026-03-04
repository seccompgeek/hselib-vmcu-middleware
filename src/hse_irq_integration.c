/**
 *   @file    hse_irq_integration.c
 *
 *   @brief   ISR wrappers and NVIC setup for HSE MU interrupts.
 *   @details This file is picked up automatically by the HSELib CMakeLists
 *            glob (src/[all .c files]) and compiled into libhselib.a.
 *
 *            It provides:
 *              - Four thin ISR wrappers that call HSE_ReceiveInterruptHandler()
 *                or HSE_GeneralPurposeInterruptHandler() for MU0 and MU1.
 *              - HSE_RegisterMuInterrupts() which directly writes the Cortex-M7
 *                NVIC ISER and IP registers — no dependency on any project-
 *                specific nvic.c.
 *
 *   @addtogroup HSE_Integration
 *   @{
 */
/*==================================================================================================
*   Copyright 2022-2025 NXP.
*   Source code licensed under applicable license terms.
==================================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/
#include "hse_irq_integration.h"
#include "hse_host.h"              /* HSE_ReceiveInterruptHandler, HSE_GeneralPurposeInterruptHandler */
#include "hse_common_types.h"      /* uint8_t, uint32_t via hse_std_typedefs                        */

/*==================================================================================================
*                                       LOCAL MACROS
* Direct Cortex-M7 NVIC register access.
* Using volatile pointer casts avoids any dependency on the application's
* nvic.c or HSELib's excluded nvic.c.
*
*  NVIC_ISER[n] at 0xE000E100 + 4n  – Interrupt Set Enable Register (1 bit/IRQ)
*  NVIC_IP[n]   at 0xE000E400 + n   – Interrupt Priority Register   (1 byte/IRQ)
*    On S32K344: 4 implemented priority bits live in bits [7:4] of each byte.
==================================================================================================*/
#define HSE_NVIC_ISER_BASE   ((volatile uint32_t *)0xE000E100UL)
#define HSE_NVIC_IP_BASE     ((volatile  uint8_t *)0xE000E400UL)

/** Enable a peripheral IRQ in the NVIC. */
static inline void hse_nvic_irq_enable(uint8_t irq)
{
    HSE_NVIC_ISER_BASE[irq >> 5U] = (uint32_t)1U << (irq & 0x1FU);
}

/**
 * Set the priority of a peripheral IRQ.
 * @param priority  Value 0-15; stored in the upper nibble of the IP byte.
 */
static inline void hse_nvic_irq_set_priority(uint8_t irq, uint8_t priority)
{
    /* S32K344: 4 priority bits live in bits [7:4] */
    HSE_NVIC_IP_BASE[irq] = (uint8_t)(((uint32_t)priority & 0x0FU) << 4U);
}

/*==================================================================================================
*                                       ISR WRAPPERS
*
* These functions are placed in the vector table (Vector_Table.s) at the
* entries for IRQ 193, 194, 196 and 197 respectively.  They simply delegate
* to the HSELib host-layer handlers.
==================================================================================================*/

/**
 * @brief IRQ handler for HSE_MU0_RX_IRQn (193).
 *        Wire into Vector_Table.s at the IRQ-193 entry.
 */
void HSE_MU0_RxIRQHandler(void)
{
    HSE_ReceiveInterruptHandler(0U);
}

/**
 * @brief IRQ handler for HSE_MU0_ORED_IRQn (194).
 *        Wire into Vector_Table.s at the IRQ-194 entry.
 */
void HSE_MU0_OredIRQHandler(void)
{
    HSE_GeneralPurposeInterruptHandler(0U);
}

/**
 * @brief IRQ handler for HSE_MU1_RX_IRQn (196).
 *        Wire into Vector_Table.s at the IRQ-196 entry.
 */
void HSE_MU1_RxIRQHandler(void)
{
    HSE_ReceiveInterruptHandler(1U);
}

/**
 * @brief IRQ handler for HSE_MU1_ORED_IRQn (197).
 *        Wire into Vector_Table.s at the IRQ-197 entry.
 */
void HSE_MU1_OredIRQHandler(void)
{
    HSE_GeneralPurposeInterruptHandler(1U);
}

/*==================================================================================================
*                                   NVIC INITIALISATION
==================================================================================================*/

/**
 * @brief   Enable the HSE MU interrupts in the Cortex-M7 NVIC.
 * @details Writes directly to NVIC_ISER and NVIC_IP so that no project-
 *          specific nvic.c symbols are required.
 *
 *          Call once before the first HSE service using interrupt-driven mode.
 *          The VTOR must already point to the RAM vector table populated by
 *          startup_cm7.s (i.e. after SET_VTOR_TCM in the startup sequence).
 *
 *          MU instances compiled in are controlled by HSE_NUM_OF_MU_INSTANCES
 *          (defined via hse_config.h / compile flags).
 */
void HSE_RegisterMuInterrupts(void)
{
    /* --- MU0: always present on S32K344 --- */
    hse_nvic_irq_set_priority((uint8_t)HSE_MU0_RX_IRQn,   (uint8_t)HSE_MU_IRQ_PRIORITY);
    hse_nvic_irq_enable      ((uint8_t)HSE_MU0_RX_IRQn);

    hse_nvic_irq_set_priority((uint8_t)HSE_MU0_ORED_IRQn, (uint8_t)HSE_MU_IRQ_PRIORITY);
    hse_nvic_irq_enable      ((uint8_t)HSE_MU0_ORED_IRQn);

#if (HSE_NUM_OF_MU_INSTANCES > 1)
    /* --- MU1: present when HSE_NUM_OF_MU_INSTANCES > 1 --- */
    hse_nvic_irq_set_priority((uint8_t)HSE_MU1_RX_IRQn,   (uint8_t)HSE_MU_IRQ_PRIORITY);
    hse_nvic_irq_enable      ((uint8_t)HSE_MU1_RX_IRQn);

    hse_nvic_irq_set_priority((uint8_t)HSE_MU1_ORED_IRQn, (uint8_t)HSE_MU_IRQ_PRIORITY);
    hse_nvic_irq_enable      ((uint8_t)HSE_MU1_ORED_IRQn);
#endif
}

/*==================================================================================================
*                                   HSE BOOT WAIT
==================================================================================================*/

/**
 * @brief   Spin until HSE firmware is ready to accept service requests.
 *
 * @details HSE firmware runs its own boot sequence after reset (integrity
 *          checks, key-catalog loading, optional secure-boot verification).
 *          Until it completes, the MU is disabled and every service request
 *          returns an error.
 *
 *          This function polls MU0 FSR[31:16] until #HSE_STATUS_INIT_OK
 *          (bit 8) is set.
 *
 * @return  The HSE status word at the moment INIT_OK became set.
 *          Check additional flags before proceeding:
 *
 * @code
 *          hseStatus_t sts = HSE_WaitForInitOk();
 *          if (0U == (sts & HSE_STATUS_INSTALL_OK))
 *          {
 *              // First run: NVM and RAM key catalogs are unformatted.
 *              FormatKeyCatalogs();
 *          }
 * @endcode
 *
 * @note    This function never times out.  Add a watchdog reset if required.
 */
hseStatus_t HSE_WaitForInitOk(void)
{
    hseStatus_t status;
    do
    {
        status = HSE_MU_GetHseStatus(0U);
    } while (0U == (status & HSE_STATUS_INIT_OK));
    return status;
}

#ifdef __cplusplus
}
#endif

/** @} */
