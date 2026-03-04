/**
 * @file    hselib.h
 * @brief   HSELib umbrella header — include this single header in your project
 *          to access all HSE Host-side APIs for S32K344.
 *
 * =============================================================================
 * HOW TO USE IN NXP S32DS (or any ARM GCC project)
 * =============================================================================
 *
 * 1. Add <HSELib_ROOT>/include  to your include paths  (Properties → C/C++ Build
 *    → Settings → Includes).  Add it AFTER the S32DS / SDK include paths so
 *    that S32DS platform headers take precedence for any shared files.
 *
 * 2. Link against <HSELib_ROOT>/build/libhselib.a
 *    (Properties → C/C++ Build → Settings → Libraries → -lhselib + library path)
 *
 * 3. In your source files:
 *        #include "hselib.h"
 *
 * =============================================================================
 * CONFLICT RESOLUTION
 * =============================================================================
 *
 * HSELib/include/ contains NXP AUTOSAR / CMSIS headers that are also shipped
 * with S32DS (Std_Types.h, Platform_Types.h, Compiler.h, core_cm7.h, …).
 * They are IDENTICAL in content — include guards (STD_TYPES_H, PLATFORM_TYPES_H,
 * __CORE_CM7_H_GENERIC, etc.) guarantee the second inclusion is a no-op.
 *
 * If you encounter redefinition errors for platform types despite the guards,
 * define the macro below BEFORE including this header (or add it as a global
 * compiler define: -DHSE_SKIP_PLATFORM_HEADERS):
 *
 *     #define HSE_SKIP_PLATFORM_HEADERS
 *     #include "hselib.h"
 *
 * This will suppress all AUTOSAR/CMSIS/device-register headers from HSELib
 * and rely entirely on the ones provided by S32DS / your project SDK.
 *
 * If you need to exclude only the S32K344 peripheral register headers:
 *     #define HSE_SKIP_DEVICE_HEADERS
 *
 * =============================================================================
 */

#ifndef HSELIB_H
#define HSELIB_H

/* =========================================================================
 * 1.  PLATFORM / AUTOSAR / CMSIS HEADERS
 *     Guarded by HSE_SKIP_PLATFORM_HEADERS.
 *     Each file already has its own include guard — so even without the
 *     macro these headers are safe to include alongside S32DS equivalents.
 * ========================================================================= */
#ifndef HSE_SKIP_PLATFORM_HEADERS

#ifndef HSE_SKIP_DEVICE_HEADERS
#include "hse_S32K344.h"
#include "hse_S32K344_COMMON.h"
#include "hse_S32K344_FLASH.h"
#include "hse_S32K344_MU.h"
#include "hse_S32K344_STM.h"
#include "hse_S32K344_DCM.h"
#include "hse_S32K344_DCM_GPR.h"
#include "hse_S32K344_PFLASH.h"
#include "hse_S32K344_MC_ME.h"
#include "hse_S32K344_MC_CGM.h"
#include "hse_S32K344_MC_RGM.h"
#include "hse_S32K344_PMC.h"
#include "hse_S32K344_MSCM.h"
#include "hse_S32K344_CONFIGURATION_GPR.h"
#endif /* HSE_SKIP_DEVICE_HEADERS */

#include "hse_Platform_Types.h"
#include "hse_Compiler.h"
#include "hse_Compiler_Cfg.h"
#include "hse_Std_Types.h"
#include "hse_Mcal.h"
#include "hse_core_cm7.h"
#include "hse_Soc_Ips.h"

#endif /* HSE_SKIP_PLATFORM_HEADERS */

/* =========================================================================
 * 2.  HSE INTERFACE — hardware / firmware protocol layer
 * ========================================================================= */
#include "hse_target.h"
#include "hse_platform.h"
#include "hse_compile_defs.h"
#include "hse_b_config.h"
#include "hse_defs.h"
#include "hse_common_types.h"
#include "hse_keymgmt_common_types.h"
#include "hse_gpr_status.h"
#include "hse_status_and_errors.h"
#include "hse_srv_responses.h"
#include "hse_interface.h"

/* =========================================================================
 * 3.  HSE SERVICE HEADERS (sym cipher, MAC, hash, key mgmt, etc.)
 * ========================================================================= */
#include "hse_srv_sym_cipher.h"
#include "hse_srv_mac.h"
#include "hse_srv_cmac_with_counter.h"
#include "hse_srv_hash.h"
#include "hse_srv_aead.h"
#include "hse_srv_sign.h"
#include "hse_srv_rsa_cipher.h"
#include "hse_srv_random.h"
#include "hse_srv_key_import_export.h"
#include "hse_srv_key_generate.h"
#include "hse_srv_key_derive.h"
#include "hse_srv_key_mgmt_utils.h"
#include "hse_srv_attr.h"
#include "hse_srv_monotonic_cnt.h"
#include "hse_srv_utils.h"
#include "hse_srv_self_test.h"
#include "hse_srv_bootdatasig.h"
#include "hse_srv_smr_install.h"
#include "hse_srv_sys_authorization.h"
#include "hse_srv_firmware_update.h"
#include "hse_srv_publish_sys_img.h"
#include "hse_srv_combined_auth_enc.h"
#include "hse_srv_she_cmds.h"
#include "hse_srv_siphash.h"
#include "hse_srv_msc_key_mgmt.h"
#include "hse_srv_sbaf_update.h"
#include "hse_srv_crc32.h"

/* =========================================================================
 * 4.  HOST DRIVER / COMMUNICATION LAYER
 * ========================================================================= */
#include "hse_mu.h"
#include "hse_host_stm.h"
#include "hse_nvic.h"
#include "hse_dcm_register.h"
#include "hse_host.h"
#include "hse_host_common.h"
#include "hse_host_compiler_api.h"
#include "hse_irq_integration.h"

/* =========================================================================
 * 5.  HOST HIGH-LEVEL API WRAPPERS
 * ========================================================================= */
#include "hse_host_cipher.h"
#include "hse_host_mac.h"
#include "hse_host_cmac_with_counter.h"
#include "hse_host_hash.h"
#include "hse_host_aead.h"
#include "hse_host_sign.h"
#include "hse_host_rsa_cipher.h"
#include "hse_host_rng.h"
#include "hse_host_import_key.h"
#include "hse_host_kdf.h"
#include "hse_host_ecc.h"
#include "hse_host_impex_stream.h"
#include "hse_host_format_key_catalogs.h"
#include "hse_host_attrs.h"
#include "hse_host_monotonic_counters.h"
#include "hse_host_utils.h"
#include "hse_host_boot.h"
#include "hse_host_wrappers.h"
#include "hse_host_test.h"
#include "hse_host_sys_authorization.h"
#include "hse_host_authenc.h"

/* =========================================================================
 * 6.  KEY MANAGEMENT
 * ========================================================================= */
#include "hse_keys_allocator.h"

/* =========================================================================
 * 7.  FLASH HOST SERVICES  (S32K3x4 / S32K344 variant)
 *
 *  flash.h / pflash.h use IP_FLASH / IP_PFLASH accessor names, which match
 *  the S32K344_FLASH.h / S32K344_PFLASH.h headers supplied in this library.
 *  Projects that were written against an older SDK using the bare names FLASH /
 *  PFLASH should remove their local Flash/ sources; the compiled versions in
 *  libhselib.a already use the correct IP_ accessors.
 * ========================================================================= */
#include "hse_fls_type.h"
#include "hse_fls_api.h"
#include "hse_host_soc_fls_type.h"
#include "hse_host_flash.h"
#include "hse_host_flashSrv.h"
#include "hse_flash.h"
#include "hse_pflash.h"

/* =========================================================================
 * 8.  DEMO APP SERVICES  (key catalog setup, global test vars, etc.)
 * ========================================================================= */
#include "hse_global_defs.h"
#include "hse_std_typedefs.h"
#include "hse_typedefs.h"
#include "hse_global_variables.h"
#include "hse_global_types.h"
#include "hse_config.h"
#include "hse_demo_app_services.h"
#include "hse_b_catalog_formatting.h"
#include "hse_monotonic_cnt.h"
#include "hse_she_api.h"
#include "hse_she_commands.h"
#include "hse_memory_update_protocol.h"

/* =========================================================================
 * 9.  COMPATIBILITY ALIAS
 *  <typedefs.h> (angle-bracket form) is also satisfied by typedefs.h above.
 *  hse_host_services.h (used in some projects as an umbrella) is completely
 *  covered by the individual hse_host_*.h includes in section 5.
 *  hse_common_types.h is included in section 2 above.
 * ========================================================================= */

#endif /* HSELIB_H */
