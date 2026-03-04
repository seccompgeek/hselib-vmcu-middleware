/*==================================================================================================
*   Project              : M4_SRC_PROJECT_NAME
*   Platform             : M4_SRC_MCU_FAMILY
*   Peripheral           : M4_SRC_USED_PERIPHERAL
*   Dependencies         : M4_SRC_AR_MODULE_DEPENDENCY
*
*   Autosar Version      : M4_SRC_AR_SPEC_VERSION_MAJOR.M4_SRC_AR_SPEC_VERSION_MINOR.M4_SRC_AR_SPEC_VERSION_PATCH
*   Autosar Revision     : M4_SRC_AR_RELEASE_REVISION
*   Autosar Conf.Variant :
*   SW Version           : M4_SRC_SW_VERSION_MAJOR.M4_SRC_SW_VERSION_MINOR.M4_SRC_SW_VERSION_PATCH
*   Build Version        : M4_SRC_BUILD_ID
*
*   Copyright M4_SRC_YEAR_ID M4_SRC_COPYRIGHTED_TO
*
*   NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

/* User includes */
#include "Lpuart_Uart_Ip.h"
#include "Flexio_Uart_Ip.h"
#include "Lpuart_Uart_Ip_Irq.h"
#include "Flexio_Uart_Ip_Irq.h"
#include "Flexio_Mcl_Ip.h"
#include "Clock_Ip.h"
#include "IntCtrl_Ip.h"
#include "Siul2_Port_Ip.h"
#include "check_example.h"
#include "string.h"

///**** HSE ****/
#include "hselib.h"

/*
* ============================================================================
*                               GLOBAL VARIABLES
* ============================================================================
*/
volatile bool_t authentication_type[5U] = {TRUE, TRUE, TRUE, TRUE, TRUE};
volatile bool_t gADKPmasterbit = FALSE;
volatile bool_t debug_auth = FALSE;
volatile hseMUConfig_t WriteMu1Config = 0xFFU;
volatile hseMUConfig_t ReadMu1Config = 0xFFU;
/* variable to store HSE FW version number before and after requesting for HSE FW update*/
hseAttrFwVersion_t gOldHseFwVersion = {0};
volatile bool_t write_attr = FALSE;
volatile bool_t hsefwusageflag = 0x0UL;
volatile int8_t GetStartAsUserBit = 0x7F;
volatile int8_t GetEnableAdkmBit = 0x7F;
volatile bool_t ActivatePassiveBlock = FALSE;
volatile bool_t fwudpate_only = FALSE;

#define AESKEY_BYTE_LENGTH				(32U)

typedef struct{
	uint8_t isNVM;
	uint16_t keyBitLen;
	hseKeyFlags_t keyFlags;
	uint8_t *pKeyValue;
}AesKeyCfg;

const hseKeyGroupCfgEntry_t NVM_Catalog [] =
{
	{HSE_ALL_MU_MASK, HSE_KEY_OWNER_CUST, HSE_KEY_TYPE_AES, 10, 256},
	{HSE_ALL_MU_MASK, HSE_KEY_OWNER_CUST, HSE_KEY_TYPE_AES, 10, 256},
	{0, 0, 0, 0, 0}
};


const hseKeyGroupCfgEntry_t RAM_Catalog [] =
{
	{HSE_ALL_MU_MASK, HSE_KEY_OWNER_ANY, HSE_KEY_TYPE_AES, 10, 256},
	{HSE_ALL_MU_MASK, HSE_KEY_OWNER_ANY, HSE_KEY_TYPE_AES, 10, 256},
	{0, 0, 0, 0, 0}
};

uint32_t PlaintextLength = 16;

uint32_t Cmac_TagLength = 16;

const uint8_t Plaintext[16] =
{ 		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
};

uint8_t Cmac_Tag[16];

AesKeyCfg GeneratedAesKey = {
	.isNVM = 1,
	.keyBitLen = 256,
	.keyFlags = HSE_KF_USAGE_ENCRYPT | HSE_KF_USAGE_DECRYPT | HSE_KF_USAGE_SIGN | HSE_KF_USAGE_VERIFY,
	.pKeyValue = NULL
};

/* Welcome messages displayed at the console */
#define WELCOME_MSG_1 "Hello, This message is sent via Uart!\r\n"
#define WELCOME_MSG_2 "Have a nice day!\r\n"

/* Error message displayed at the console, in case data is received erroneously */
#define ERROR_MSG "An error occurred! The application will stop!\r\n"

/* Length of the message to be received from the console */
#define MSG_LEN  50U

/* Define channel */
#define UART_LPUART_INTERNAL_CHANNEL  3
#define UART_FLEXIO_TX_CHANNEL  0
#define UART_FLEXIO_RX_CHANNEL  1

volatile int exit_code = 0;


/* Define enum which module is receiver */
typedef enum
{
    LPUART_RECEIVER = 0,
    FLEXIO_RECEIVER = 1

} Receiver_Module_Type;

boolean User_Str_Cmp(const uint8 * pBuffer1, const uint8 * pBuffer2, const uint32 length)
{
    uint32 idx = 0;
    for (idx = 0; idx < length; idx++)
    {
        if(pBuffer1[idx] != pBuffer2[idx])
        {
            return FALSE;
        }
    }
    return TRUE;
}

boolean Send_Data(Receiver_Module_Type receiver, const uint8* pBuffer, uint32 length)
{
    volatile Lpuart_Uart_Ip_StatusType lpuartStatus = LPUART_UART_IP_STATUS_ERROR;
    volatile Flexio_Uart_Ip_StatusType flexioStatus = FLEXIO_UART_IP_STATUS_ERROR;
    uint32 remainingBytes;
    uint32 T_timeout = 0xFFFFFF;
    uint8 Rx_Buffer[MSG_LEN];

    memset(Rx_Buffer, 0 , MSG_LEN);

    if (receiver == FLEXIO_RECEIVER)
    {
        /* Uart_AsyncReceive transmit data */
        flexioStatus = Flexio_Uart_Ip_AsyncReceive(UART_FLEXIO_RX_CHANNEL, Rx_Buffer, length);
        if (FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus)
        {
            return FALSE;
        }
        /* Uart_AsyncSend transmit data */
        lpuartStatus = Lpuart_Uart_Ip_AsyncSend(UART_LPUART_INTERNAL_CHANNEL, pBuffer, length);
        if (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus)
        {
            return FALSE;
        }

        /* Check for no on-going transmission */
        do
        {
            lpuartStatus = Lpuart_Uart_Ip_GetTransmitStatus(UART_LPUART_INTERNAL_CHANNEL, &remainingBytes);
        } while (LPUART_UART_IP_STATUS_BUSY == lpuartStatus && 0 < T_timeout--);

        T_timeout = 0xFFFFFF;

        do
        {
            flexioStatus = Flexio_Uart_Ip_GetStatus(UART_FLEXIO_RX_CHANNEL, &remainingBytes);
        } while (FLEXIO_UART_IP_STATUS_BUSY == flexioStatus && 0 < T_timeout--);

        if ((FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus) || (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus))
        {
            return FALSE;
        }
    }
    else
    {
        /* Uart_AsyncReceive transmit data */
        lpuartStatus = Lpuart_Uart_Ip_AsyncReceive(UART_LPUART_INTERNAL_CHANNEL, Rx_Buffer, length);
        if (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus)
        {
            return FALSE;
        }
        /* Uart_AsyncSend transmit data */
        flexioStatus = Flexio_Uart_Ip_AsyncSend(UART_FLEXIO_TX_CHANNEL, pBuffer, length);
        if (FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus)
        {
            return FALSE;
        }

        /* Check for no on-going transmission */
        do
        {
            flexioStatus = Flexio_Uart_Ip_GetStatus(UART_FLEXIO_TX_CHANNEL, &remainingBytes);
        } while (FLEXIO_UART_IP_STATUS_BUSY == flexioStatus && 0 < T_timeout--);

        do
        {
            lpuartStatus = Lpuart_Uart_Ip_GetReceiveStatus(UART_LPUART_INTERNAL_CHANNEL, &remainingBytes);
        } while (LPUART_UART_IP_STATUS_BUSY == lpuartStatus && 0 < T_timeout--);

        if ((FLEXIO_UART_IP_STATUS_SUCCESS != flexioStatus) || (LPUART_UART_IP_STATUS_SUCCESS != lpuartStatus))
        {
            return FALSE;
        }
    }

    if(!User_Str_Cmp(pBuffer, (const uint8 *)Rx_Buffer, length))
    {
        return FALSE;
    }

    return TRUE;
}
/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void)
{
    /* Write your code here */
    volatile boolean isSendMsg1Success = FALSE;
    volatile boolean isSendMsg2Success = FALSE;

    hseSrvResponse_t HseResponse;
	hseKeyHandle_t GeneratedKeyHandle;

	 /*Check Fw Install Status*/
	WaitForHSEFWInitToFinish();

	/* Format Key Catalogs with the application's configuration */
	HseResponse = FormatKeyCatalogs(NVM_Catalog, RAM_Catalog);
	ASSERT(HSE_SRV_RSP_OK == HseResponse);

	/* Initialize HKF for Key management */
	HseResponse = HKF_Init(NVM_Catalog, RAM_Catalog);
	ASSERT(HSE_SRV_RSP_OK == HseResponse);

//	HseResponse = Load_Relevant_She_Keys();
//	ASSERT(HSE_SRV_RSP_OK == HseResponse);

    /* Init clock  */
    Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);

#if defined (FEATURE_CLOCK_IP_HAS_SPLL_CLK)
    while ( CLOCK_IP_PLL_LOCKED != Clock_Ip_GetPllStatus() )
    {
        /* Busy wait until the System PLL is locked */
    }

    Clock_Ip_DistributePll();
#endif
    /* Initialize all pins */
    Siul2_Port_Ip_Init(NUM_OF_CONFIGURED_PINS_PortContainer_0_VS_0, g_pin_mux_InitConfigArr_PortContainer_0_VS_0);

    /* Init Flexio common Mcl  */
    Flexio_Mcl_Ip_InitDevice(&Flexio_Ip_Sa_xFlexioInit_VS_0);

    /* Initialize IRQs */
    IntCtrl_Ip_Init(&IntCtrlConfig_0);
    //IntCtrl_Ip_ConfigIrqRouting(&intRouteConfig);
    /* Initializes an UART driver*/
    Lpuart_Uart_Ip_Init(UART_LPUART_INTERNAL_CHANNEL, &Lpuart_Uart_Ip_xHwConfigPB_3_VS_0);
    Flexio_Uart_Ip_Init(UART_FLEXIO_TX_CHANNEL, &Flexio_Uart_Ip_xHwConfigPB_0_VS_0);
    Flexio_Uart_Ip_Init(UART_FLEXIO_RX_CHANNEL, &Flexio_Uart_Ip_xHwConfigPB_1_VS_0);

    isSendMsg1Success = Send_Data(FLEXIO_RECEIVER, (const uint8 *)WELCOME_MSG_1, strlen(WELCOME_MSG_1));

    isSendMsg2Success = Send_Data(LPUART_RECEIVER, (const uint8 *)WELCOME_MSG_2, strlen(WELCOME_MSG_2));

    (void)Lpuart_Uart_Ip_Deinit(UART_LPUART_INTERNAL_CHANNEL);

    (void)Flexio_Uart_Ip_Deinit(UART_FLEXIO_RX_CHANNEL);
    (void)Flexio_Uart_Ip_Deinit(UART_FLEXIO_TX_CHANNEL);

    /* Deinit Flexio common Mcl  */
    Flexio_Mcl_Ip_DeinitDevice(&Flexio_Ip_Sa_xFlexioInit_VS_0);

    Exit_Example((isSendMsg1Success == TRUE) && (isSendMsg2Success == TRUE));
    return exit_code;
}

/** @} */
