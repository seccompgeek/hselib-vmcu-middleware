/**
 *   @file    hse_host_boot.c
 *
 *   @brief   Wrappers for secure-boot configuration and boot-data image signing services.
 *   @details Implements HSE_InstallSmrEntry, HSE_InstallCoreResetEntry, HSE_SignBootImage,
 *            HSE_VerifyBootImage, smrVerifyTest, generateIvtSign, HSE_ReadAdkp, and
 *            HSE_ProgramAdkp.  All functions are marked __attribute__((weak)) so the
 *            application can supply stronger overrides when needed.
 *
 *   @addtogroup hse_host_boot
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
#include "hse_host_boot.h"
#include "hse_host.h"
#include "hse_demo_app_services.h"
#include "hse_host_flashSrv.h"
#include "hse_host_attrs.h"
#include "hse_global_variables.h"
#include <string.h>

/*==================================================================================================
*                                       LOCAL MACROS
==================================================================================================*/
/* BLOCK0_IVT_ADDRESS (0x00400000U) is defined in hse_demo_app_services.h */
#define IVT_FIXED_LENGTH      (0x100U)

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

/******************************************************************************
 * Function:    HSE_InstallSmrEntry
 * Description: Installs a Secure Memory Region entry for Advanced Secure Boot.
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t HSE_InstallSmrEntry(
    const uint8_t         entryIndex,
    const hseSmrEntry_t  *pSmrEntry,
    const uint8_t        *pData,
    const uint32_t        dataLen,
    const uint8_t        *pSign0,
    const uint8_t        *pSign1,
    const uint32_t        SignLen0,
    const uint32_t        SignLen1)
{
    uint8_t u8MuChannel = 1U;
    hseSrvDescriptor_t         *pHseSrvDesc;
    hseSmrEntryInstallSrv_t    *pSmrEntryInstall;
    hseSrvResponse_t            srvResponse = HSE_SRV_RSP_GENERAL_ERROR;

    pHseSrvDesc      = &gHseSrvDesc[MU0][u8MuChannel];
    pSmrEntryInstall = &(pHseSrvDesc->hseSrv.smrEntryInstallReq);
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));

    pHseSrvDesc->srvId                       = HSE_SRV_ID_SMR_ENTRY_INSTALL;
    pSmrEntryInstall->accessMode             = HSE_ACCESS_MODE_ONE_PASS;
    pSmrEntryInstall->entryIndex             = entryIndex;
    pSmrEntryInstall->pSmrEntry              = PTR_TO_HOST_ADDR(pSmrEntry);
    pSmrEntryInstall->pSmrData               = PTR_TO_HOST_ADDR(pData);
    pSmrEntryInstall->smrDataLength          = dataLen;
    pSmrEntryInstall->pAuthTag[0]            = PTR_TO_HOST_ADDR(pSign0);
    pSmrEntryInstall->pAuthTag[1]            = PTR_TO_HOST_ADDR(pSign1);
    pSmrEntryInstall->authTagLength[0]       = SignLen0;
    pSmrEntryInstall->authTagLength[1]       = SignLen1;

    srvResponse = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);
    return srvResponse;
}

/******************************************************************************
 * Function:    smrVerifyTest
 * Description: Requests on-demand verification of an SMR entry.
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t smrVerifyTest(uint32_t smrentry)
{
    uint8_t            u8MuChannel = 1U;
    hseSmrVerifySrv_t *pSmrVerifySrv;
    hseSrvResponse_t   hseStatus = HSE_SRV_RSP_GENERAL_ERROR;
    hseSrvDescriptor_t *pHseSrvDesc = &gHseSrvDesc[MU0][u8MuChannel];

    pSmrVerifySrv = &(pHseSrvDesc->hseSrv.smrVerifyReq);
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));
    pHseSrvDesc->srvId       = HSE_SRV_ID_SMR_VERIFY;
    pSmrVerifySrv->entryIndex = (uint8_t)smrentry;

    hseStatus = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);
    return hseStatus;
}

/******************************************************************************
 * Function:    HSE_InstallCoreResetEntry
 * Description: Installs a Core Reset entry for Advanced Secure Boot.
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t HSE_InstallCoreResetEntry(
    const uint8_t         entryIndex,
    const hseCrEntry_t   *pCrEntry)
{
    uint8_t                 u8MuChannel = 1U;
    hseSrvDescriptor_t     *pHseSrvDesc;
    hseCrEntryInstallSrv_t *pCrEntryInstallSrv;
    hseSrvResponse_t        srvResponse = HSE_SRV_RSP_GENERAL_ERROR;

    pHseSrvDesc          = &gHseSrvDesc[MU0][u8MuChannel];
    pCrEntryInstallSrv   = &(pHseSrvDesc->hseSrv.crEntryInstallReq);
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));

    pHseSrvDesc->srvId                 = HSE_SRV_ID_CORE_RESET_ENTRY_INSTALL;
    pCrEntryInstallSrv->crEntryIndex   = entryIndex;
    pCrEntryInstallSrv->pCrEntry       = PTR_TO_HOST_ADDR(pCrEntry);

    srvResponse = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);
    return srvResponse;
}

/******************************************************************************
 * Function:    HSE_SignBootImage
 * Description: Signs IVT/DCD/SelfTest or Application image for Basic Secure Boot.
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t HSE_SignBootImage(
    const uint8_t  *pInImage,
    const uint32_t  inTagLength,
    uint8_t        *pOutTagAddr)
{
    uint8_t                      u8MuChannel = 1U;
    hseSrvDescriptor_t          *pHseSrvDesc;
    hseBootDataImageSignSrv_t   *pBootDataImgSignSrv;
    hseSrvResponse_t             srvResponse = HSE_SRV_RSP_GENERAL_ERROR;

    pHseSrvDesc           = &gHseSrvDesc[MU0][u8MuChannel];
    pBootDataImgSignSrv   = &(pHseSrvDesc->hseSrv.bootDataImageSignReq);
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));

    pHseSrvDesc->srvId                         = HSE_SRV_ID_BOOT_DATA_IMAGE_SIGN;
    pBootDataImgSignSrv->pInImage              = PTR_TO_HOST_ADDR(pInImage);
    pBootDataImgSignSrv->inTagLength           = inTagLength;
    pBootDataImgSignSrv->pOutTagAddr           = PTR_TO_HOST_ADDR(pOutTagAddr);

    srvResponse = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);
    return srvResponse;
}

/******************************************************************************
 * Function:    HSE_VerifyBootImage
 * Description: Verifies the MAC generated over IVT/DCD/SelfTest/AppBSB images.
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t HSE_VerifyBootImage(const uint8_t *pInImage)
{
    uint8_t             u8MuChannel = 1U;
    hseSrvDescriptor_t *pHseSrvDesc;
    hseSrvResponse_t    srvResponse = HSE_SRV_RSP_GENERAL_ERROR;

    pHseSrvDesc = &gHseSrvDesc[MU0][u8MuChannel];
    memset(pHseSrvDesc, 0, sizeof(hseSrvDescriptor_t));

    pHseSrvDesc->srvId                                        = HSE_SRV_ID_BOOT_DATA_IMAGE_VERIFY;
    pHseSrvDesc->hseSrv.bootDataImageSigVerifyReq.pInImage    = PTR_TO_HOST_ADDR(pInImage);

    srvResponse = HSE_Send(MU0, u8MuChannel, gSyncTxOption, pHseSrvDesc);
    return srvResponse;
}

/******************************************************************************
 * Function:    generateIvtSign
 * Description: Generates and writes a GMAC over the IVT to flash (non-secure
 *              and secure backup copies).
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t generateIvtSign(void)
{
    hseSrvResponse_t srvResponse = HSE_SRV_RSP_GENERAL_ERROR;

    /* Copy IVT from flash to the global IVT buffer in SRAM */
    (void)memcpy(&IVT, (const void *)BLOCK0_IVT_ADDRESS, IVT_FIXED_LENGTH);

    /* Generate MAC over the non-secure IVT */
    srvResponse = HSE_SignBootImage(
        (uint8_t *)&IVT,
        BOOT_IMG_TAG_LEN,
        (uint8_t *)&IVT.GMAC[0]);
    ASSERT(HSE_SRV_RSP_OK == srvResponse);

    /* Write non-secure IVT backup to flash */
    ASSERT(HostFlash_Erase(
            HSE_HOST_BLOCK0_CODE_MEMORY,
            (BLOCK0_IVT_ADDRESS + NON_SECURE_IVT_BACKUPADDR_OFFSET),
            1U) == FLS_JOB_OK);
    ASSERT(HostFlash_Program(
            HSE_HOST_BLOCK0_CODE_MEMORY,
            (uint32_t)(BLOCK0_IVT_ADDRESS + NON_SECURE_IVT_BACKUPADDR_OFFSET),
            (uint8_t *)&IVT,
            (uint32_t)IVT_FIXED_LENGTH) == FLS_JOB_OK);

    /* Set the BOOT_SEQ bit and generate MAC over the secure IVT */
    IVT.bootCfgWord |= IVT_BOOT_CFG_WORD_BOOT_SEQ;
    srvResponse = HSE_SignBootImage(
        (uint8_t *)&IVT,
        BOOT_IMG_TAG_LEN,
        (uint8_t *)&IVT.GMAC[0]);
    ASSERT(HSE_SRV_RSP_OK == srvResponse);

    /* Write secure IVT backup to flash */
    ASSERT(HostFlash_Erase(
            HSE_HOST_BLOCK0_CODE_MEMORY,
            (BLOCK0_IVT_ADDRESS + SECURE_IVT_BACKUPADDR_OFFSET),
            1U) == FLS_JOB_OK);
    ASSERT(HostFlash_Program(
            HSE_HOST_BLOCK0_CODE_MEMORY,
            (uint32_t)(BLOCK0_IVT_ADDRESS + SECURE_IVT_BACKUPADDR_OFFSET),
            (uint8_t *)&IVT,
            (uint32_t)IVT_FIXED_LENGTH) == FLS_JOB_OK);

    return HSE_SRV_RSP_OK;
}

/******************************************************************************
 * Function:    HSE_ReadAdkp
 * Description: Reads the Application Debug Key/Password (ADKP) hash attribute
 *              from HSE.  Returns HSE_SRV_RSP_NOT_ALLOWED when no ADKP has
 *              been programmed yet; HSE_SRV_RSP_OK on success.
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t HSE_ReadAdkp(uint8_t *pDebugKey)
{
    hseSrvResponse_t srvResponse;
    srvResponse = Get_Attr(
            HSE_APP_DEBUG_KEY_ATTR_ID,
            sizeof(hseAttrApplDebugKey_t),
            (void *)pDebugKey);
    return srvResponse;
}

/******************************************************************************
 * Function:    HSE_ProgramAdkp
 * Description: Programs the Application Debug Key/Password (ADKP).
 *              WARNING: This operation is irreversible – one-time fuse write.
 ******************************************************************************/
__attribute__((weak))
hseSrvResponse_t HSE_ProgramAdkp(void)
{
    hseSrvResponse_t srvResponse;
    srvResponse = SetAttr(
            HSE_APP_DEBUG_KEY_ATTR_ID,
            sizeof(hseAttrApplDebugKey_t),
            (void *)&applicationDebugKeyPassword);
    ASSERT(HSE_SRV_RSP_OK == srvResponse);
    return srvResponse;
}

/******************************************************************************
 * Function:    check_debug_password_programmed_status
 * Description: Returns TRUE when an ADKP has already been programmed in fuses.
 ******************************************************************************/
__attribute__((weak))
bool_t check_debug_password_programmed_status(void)
{
    static uint8_t readback[16] = {0U};
    hseSrvResponse_t srvResponse = HSE_ReadAdkp(readback);
    return (HSE_SRV_RSP_OK == srvResponse) ? TRUE : FALSE;
}

#ifdef __cplusplus
}
#endif

/** @} */
