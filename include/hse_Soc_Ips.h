/**
*   @file    Soc_Ips.h
*   @version 0.4.0
*
*   @brief   AUTOSAR Base - IP module versions.
*   @details The IP modules versions used on ARM hardware platform and 
*            IP specific defines
*
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

/*==================================================================================================
==================================================================================================*/
/** 
* @page misra_violations MISRA-C:2004 violations
* @section Soc_Ips_h_REF_1
* Violates MISRA 2004 Required Rule 19.15, Repeated include file 
* This comes from the order of includes in the .c file and from include dependencies. As a safe 
* approach, any file must include all its dependencies. Header files are already protected against 
* double inclusions.
* @section [global]
* Violates MISRA 2004 Required Rule 5.1, Identifiers (internal and external) shall not rely on the
* significance of more than 31 characters. The used compilers use more than 31 chars for identifiers
* @section [global]
* Violates MISRA 2004 Required Rule 20.2, The names of standard library macros, objects and functions
* shall not be reused. The driver needs to use defines starting with letter E.
* @section Soc_Ips_h_REF_2
*     Violates MISRA 2004 Advisory Rule 19.7, A function should be used in preference to a function-
*     like macro.
*     This violation is due to function like macros defined for register operations. Function like
*     macros are used to reduce code complexity.
*/

#ifndef HSE_SOC_IPS_H
#define HSE_SOC_IPS_H

#ifdef __cplusplus
extern "C"{
#endif


/*==================================================================================================
*                                         INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/
/** 
* @file        Soc_Ips.h
* @brief Include platform types
* @violates @ref Soc_Ips_h_REF_1 MISRA 2004 Required Rule 19.15, Repeated include file
*/
#include "hse_Platform_Types.h"

/*==================================================================================================
*                               SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define HSE_SOC_IPS_VENDOR_ID                    43
#define HSE_SOC_IPS_MODULE_ID                    0
#define HSE_PLATFORM_AR_RELEASE_MAJOR_VERSION    4
#define HSE_PLATFORM_AR_RELEASE_MAJOR_VERSION       4
#define HSE_SOC_IPS_AR_RELEASE_MAJOR_VERSION     4
#define HSE_SOC_IPS_AR_RELEASE_MINOR_VERSION     2
#define HSE_SOC_IPS_AR_RELEASE_REVISION_VERSION  2
#define HSE_SOC_IPS_SW_MAJOR_VERSION             0
#define HSE_SOC_IPS_SW_MINOR_VERSION             4
#define HSE_SOC_IPS_SW_PATCH_VERSION             0

/*==================================================================================================
*                                      FILE VERSION CHECKS
==================================================================================================*/
#ifndef HSE_DISABLE_MCAL_INTERMODULE_ASR_CHECK
    /* Check if source file and Platform_Types.h header file are of the same Autosar version */
    #if ((HSE_SOC_IPS_AR_RELEASE_MAJOR_VERSION != HSE_PLATFORM_AR_RELEASE_MAJOR_VERSION) || \
         (HSE_SOC_IPS_AR_RELEASE_MINOR_VERSION != HSE_PLATFORM_AR_RELEASE_MINOR_VERSION))
        #error "AutoSar Version Numbers of Soc_Ips.h and Platform_Types.h are different"
    #endif
#endif

/*==================================================================================================
*                                           CONSTANTS
==================================================================================================*/

/**
* @brief SAR ADC IP Version: ADC_12b1MSPS_SAR v00.00.01.02
* @details S32GXX IP Versions
*/
#ifndef IPV_ADCDIG
#define IPV_ADCDIG            (0x00000102UL)
#endif /* IPV_ADCDIG */

/**
* @brief DMAMUX IP Version: DMAMUX v06.01.03.00
* @details S32GXX IP Versions
*/
#ifndef IPV_DMAMUX
#define IPV_DMAMUX            (0x06010300UL)
#endif /* IPV_DMAMUX */

/**
* @brief DSPI IP Version: DSPI v06.01.08.03
* @details S32GXX IP Versions
*/
#ifndef IPV_DSPI
#define IPV_DSPI               (0x06010803UL)
#endif /* IPV_DSPI */

/**
* @brief WKUP IP Version: WKPU v01.00.14.01
* @details S32GXX IP Versions
*/
#ifndef IPV_WKPU
#define IPV_WKPU               (0x01001401UL)
#endif /* IPV_WKPU */

/**
* @brief CTU2 IP Version: CTU2 v00.00.16.00
* @details S32GXX IP Versions
*/
#ifndef IPV_CTU
#define IPV_CTU               (0x00001600UL)
#endif /* IPV_CTU */

/**
* @brief PIT_RTI IP Version: PIT_RTI v05.00.06.09
* @details S32GXX IP Versions
*/
#ifndef IPV_PIT
#define IPV_PIT              (0x05000609UL)
#endif /* IPV_PIT */

/**
* @brief PLL DIG IP Version: PLL DIG R02.00.04.01
* @details S32GXX IP Versions
*/
#ifndef IPV_PLLDIG
#define IPV_PLLDIG           (0x02000401UL)
#endif /* IPV_PLLDIG */

/**
* @brief SIUL2 IP Version: SIUL2 v1.3.c
* @details S32GXX IP Versions
*/
#ifndef IPV_SIULV2
#define IPV_SIULV2             (0x00010300UL)
#endif /* IPV_SIULV2 */

/**
* @brief SSCM IP Version: SSCM 06.00.00.06
* @details S32GXX IP Versions
*/
#ifndef IPV_SSCM
#define IPV_SSCM                 (0x06000006UL)
#endif /* IPV_SSCM */

/**
* @brief LINFLEX IP Version: LINFLEX v5.4.hbs1
* @details S32GXX IP Versions
*/
#ifndef IPV_LINFLEX
#define IPV_LINFLEX           (0x00050400UL)
#endif /* IPV_LINFLEX */

/**
* @brief STCU IP Version: STA1_STCU2_IPS@v1.0.a_0.01
* @details S32GXX IP Versions
*/
#ifndef IPV_STCU2
#define IPV_STCU2             (0x01006101UL)
#endif /* IPV_STCU2 */

/**
* @brief MCv2 IP Version: D_IP_magic_carpet_SYN [v04.03.00.00]
* @details S32GXX IP Versions
*/
#ifndef IPV_MC
#define IPV_MC                (0x04060500UL)
#endif /* IPV_MC */





/*==================================================================================================
*                              Software Erratas for Hardware Erratas
==================================================================================================*/


/*==================================================================================================
*                                       DEFINES AND MACROS
==================================================================================================*/
/**
* @brief Enable CACHE management feature
* @details Global define to enable CACHE management at runtime
*/
#define HSE_MCAL_CACHE_RUNTIME_MNGMNT (STD_OFF)


/*==================================================================================================
*                                             ENUMS
==================================================================================================*/


/*==================================================================================================
*                                 STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/


/*==================================================================================================
*                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/

/**
* @brief User Mode feature is enabled
* @details User Mode feature - MCAL is configured to run in supervisor mode, by default.
*/

#ifdef HSE_MCAL_ENABLE_SUPERVISOR_MODE
    #undef HSE_MCAL_ENABLE_USER_MODE_SUPPORT
#endif

/*==================================================================================================
*                                     FUNCTION PROTOTYPES
==================================================================================================*/

#ifdef __cplusplus
}
#endif


#endif /* #ifndef HSE_SOC_IPS_H */

/** @} */
