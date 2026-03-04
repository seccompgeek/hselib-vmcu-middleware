/**
*   @file    nvic.c
*
*   @brief   AUTOSAR M4_SRC_MODULE_NAME - nvic driver
*   @details Functions that manage interrupts and exceptions via the NVIC (adapted from CMSIS_core)

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
#include "hse_std_typedefs.h"
//#include "hse_StdRegMacros.h"
#include "hse_Std_Types.h"
#include "hse_nvic.h"
#include "hse_Mcal.h"
#include "hse_device_platform.h"

/*==================================================================================================
*                                        LOCAL MACROS
==================================================================================================*/

/*==================================================================================================
*                                      FILE VERSION CHECKS
==================================================================================================*/

/*==================================================================================================
*                          LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/

/*==================================================================================================
*                                       LOCAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                       GLOBAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                       GLOBAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                   LOCAL FUNCTION PROTOTYPES
==================================================================================================*/

/*==================================================================================================
*                                       LOCAL FUNCTIONS
==================================================================================================*/

/*================================================================================================*/
/** 
* @brief Set Priority Grouping
* @details The function sets the priority grouping field using the required unlock sequence.
*  The parameter PriorityGroup is assigned to the field SCB->AIRCR [10:8] PRIGROUP field.
*   Only values from 0..7 are used.
*   In case of a conflict between priority grouping and available
*   priority bits (__NVIC_PRIO_BITS), the smallest possible priority group is set
*/
/*================================================================================================*/
void NVIC_SetPriorityGrouping(uint32 PriorityGroup)
{
    /* Set the PRIGROUP[10:8] bits according to NVIC_PriorityGroup value */
    S32_SCB->AIRCR = (S32_SCB->AIRCR & (~S32_SCB_AIRCR_PRIGROUP_MASK)) | PriorityGroup;
}

/*================================================================================================*/
/** 
* @brief Enable External Interrupt
* @details The function enables a device-specific interrupt in the NVIC interrupt controller.
*/
/*================================================================================================*/
void NVIC_EnableIRQ(uint8 IRQn)
{
    #ifdef HSE_MCAL_ENABLE_USER_MODE_SUPPORT
        Mcal_goToSupervisor();
    #endif
        REG_BIT_SET32((HSE_NVIC_BASEADDR + HSE_NVIC_ISER_OFFSET(IRQn)), 1 << (IRQn % 32)); /* enable interrupt */
    #ifdef HSE_MCAL_ENABLE_USER_MODE_SUPPORT
        Mcal_goToUser();
    #endif
}

/*================================================================================================*/
/** 
* @brief Disable External Interrupt
* @details The function disables a device-specific interrupt in the NVIC interrupt controller
*/
/*================================================================================================*/
void NVIC_DisableIRQ(uint8 IRQn)
{
    #ifdef HSE_MCAL_ENABLE_USER_MODE_SUPPORT
        Mcal_goToSupervisor();
    #endif
        REG_WRITE32((HSE_NVIC_BASEADDR + HSE_NVIC_ICER_OFFSET(IRQn)), 1 << (IRQn % 32));
    #ifdef HSE_MCAL_ENABLE_USER_MODE_SUPPORT
        Mcal_goToUser();
    #endif
}

/*================================================================================================*/
/** 
* @brief Set Interrupt Priority
* @details The function sets the priority of an interrupt.
*/
/*================================================================================================*/
void NVIC_SetPriority(uint8 IRQn, uint8 priority)
{
    #ifdef HSE_MCAL_ENABLE_USER_MODE_SUPPORT
        Mcal_goToSupervisor();
    #endif
        REG_RMW32((HSE_NVIC_BASEADDR + HSE_NVIC_IPRO_OFFSET(IRQn)), HSE_NVIC_IPRO_MASK(IRQn), priority << ((IRQn % 4) * 8));
    #ifdef HSE_MCAL_ENABLE_USER_MODE_SUPPORT
        Mcal_goToUser();
    #endif
}
