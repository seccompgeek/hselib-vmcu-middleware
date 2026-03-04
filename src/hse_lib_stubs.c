/**
 *   @file    hse_lib_stubs.c
 *
 *   @brief   Weak default definitions for all application-supplied symbols used
 *            by HSELib source files.
 *   @details These stubs satisfy the declarations in HSELib headers so that
 *            libhselib.a can be archived without unresolved symbols.  Every
 *            definition is marked __attribute__((weak)) so the application
 *            binary can override them with real implementations at final link
 *            time.
 *
 *            Categories covered:
 *              1. Global variables: write_attr, WriteMu1Config, ReadMu1Config,
 *                 authentication_type[]
 *              2. Linker-script address symbols: FLASH_DRIVER_* and HSE_HOST_*
 *              3. Application-level service functions called from
 *                 hse_demo_app_config.c
 */
/*==================================================================================================
*   Copyright 2022 NXP.
*   Source code licensed under applicable license terms.
==================================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                        INCLUDE FILES
==================================================================================================*/
#include "hse_demo_app_services.h"  /* bool_t, void service prototypes             */
#include "hse_srv_attr.h"           /* hseMUConfig_t                               */

/*==================================================================================================
*                          SECTION 1: GLOBAL VARIABLE WEAK DEFAULTS
* These extern volatile variables are declared in the library source files but
* are intended to be provided by the application.  The weak defaults allow the
* library to archive without errors.
==================================================================================================*/

/** @brief Controls whether WriteMu1Config/ReadMu1Config are flushed to NVM.
 *  Application sets to TRUE before calling MU_EnablementService(). */
__attribute__((weak)) volatile bool_t write_attr = FALSE;

/** @brief Desired MU1 configuration value to be written by hse_mu_config.c. */
__attribute__((weak)) volatile hseMUConfig_t WriteMu1Config = HSE_MU_DEACTIVATED;

/** @brief Current MU1 configuration value read back by hse_mu_config.c. */
__attribute__((weak)) volatile hseMUConfig_t ReadMu1Config  = HSE_MU_DEACTIVATED;

/** @brief SMR authentication scheme flags, one per SMR slot (5 used by
 *  hse_secure_boot.c).  TRUE = entry requires authentication. */
__attribute__((weak)) volatile bool_t authentication_type[5] = { FALSE, FALSE, FALSE, FALSE, FALSE };

/*==================================================================================================
*                       SECTION 2: LINKER-SCRIPT ADDRESS SYMBOL DEFAULTS
* Declared as extern uint32_t FOO[] in hse_host_flashSrv.h / hse_she_api.h and
* normally assigned by the application linker script.  Providing weak 1-element
* arrays gives the linker a resolvable default; the application linker script
* absolute symbol overrides these at final link time.
==================================================================================================*/

/** @brief Target address of the Flash Driver image in HSE RAM. */
__attribute__((weak)) uint32_t FLASH_DRIVER_RAM_DST_START_ADDRESS[1]   = { 0U };

/** @brief Start address of the Flash Driver image in Flash. */
__attribute__((weak)) uint32_t FLASH_DRIVER_FLASH_SRC_START_ADDRESS[1] = { 0U };

/** @brief End address of the Flash Driver image in Flash. */
__attribute__((weak)) uint32_t FLASH_DRIVER_FLASH_SRC_END_ADDRESS[1]   = { 0U };

/** @brief Target address of the HSE Host code image in RAM. */
__attribute__((weak)) uint32_t HSE_HOST_RAM_DST_START_ADDR[1]          = { 0U };

/** @brief Start address of the HSE Host code image in Flash. */
__attribute__((weak)) uint32_t HSE_HOST_FLASH_SRC_START_ADDR[1]        = { 0U };

/** @brief End address of the HSE Host code image in Flash. */
__attribute__((weak)) uint32_t HSE_HOST_FLASH_SRC_END_ADDR[1]          = { 0U };

/*==================================================================================================
*                     SECTION 3: APPLICATION-LEVEL SERVICE FUNCTION STUBS
* These functions are called from hse_demo_app_config.c (part of the library)
* but their full implementations are application-specific (e.g. they depend on
* application key material or policy decisions at build time).
* Applications must provide a strong override; the weak no-ops below are only
* here so that the library itself and simple test applications can link.
==================================================================================================*/

/** @brief Programs the Application Debug Key / Password into HSE OTP fuses. */
__attribute__((weak)) void ProgramADKPService(void)                  { /* Application must override */ }

/** @brief Requests HSE to perform CHAL-RESP or password debug authorisation. */
__attribute__((weak)) void Debug_Auth_Service(void)                  { /* Application must override */ }

/** @brief Sets the ADKM bit and/or StartAsUser bit in the customer security policy. */
__attribute__((weak)) void ExtendCustomerSecurityPolicyService(void) { /* Application must override */ }

/** @brief Advances the NXP secure lifecycle (e.g. OEM_PROD → IN_FIELD). */
__attribute__((weak)) void Advance_LifeCycle_Service(void)           { /* Application must override */ }

/** @brief Enables IVT authentication: writes the BOOT_AUTH attribute to HSE. */
__attribute__((weak)) void IVT_Auth_Service(void)                    { /* Application must override */ }

/** @brief Enables the secondary MU interface (MU1) via HSE NVM attribute. */
__attribute__((weak)) void MU_EnablementService(void)                { /* Application must override */ }

/** @brief Triggers the HSE firmware update flow. */
__attribute__((weak)) void firmwareUpdateService(void)               { /* Application must override */ }

/** @brief Runs the full secure-boot configuration sequence. */
__attribute__((weak)) void SecureBootService(void)                   { /* Application must override */ }

#ifdef __cplusplus
}
#endif
