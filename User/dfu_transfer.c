/******************************************************************************//**
 * @file     dfu_transfer.c
 * @version  V1.00
 * @brief    NUC123 series USBD DFU transfer sample file
 *
 * @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/

/*!<Includes */
#include <stdio.h>
#include <string.h>
#include "NUC123.h"
#include "dfu_transfer.h"
#include "fmc_user.h"

extern uint32_t g_romSize;
extern uint8_t g_reset;
#define APROM_BLOCK_NUM         ((g_romSize/TRANSFER_SIZE)-1)

uint8_t manifest_state = MANIFEST_COMPLETE;
dfu_status_struct dfu_status;
s_prog_struct prog_struct __attribute__((aligned(4), section(".bss"))) = {0};

volatile uint8_t g_write_pending = 0;


void USBD_OnCtrlOutComplete(void)
{
    if(dfu_status.bState == STATE_dfuDNLOAD_SYNC)
    {
        /* Data is in prog_struct.buf — flag it for the main loop.
           Do NOT write flash here; STATUS ZLP must go out first. */
        g_write_pending = 1;
        /* Keep state as dfuDNLOAD_SYNC so GETSTATUS replies dfuDNBUSY */
    }
    else if(dfu_status.bState == STATE_dfuMANIFEST_SYNC)
    {
        /* Manifest phase complete */
        manifest_state = MANIFEST_COMPLETE;
        dfu_status.bState = STATE_dfuIDLE;
        g_reset = 1;
    }
}

void USBD_IRQHandler(void)
{
    uint32_t u32IntSts = USBD_GET_INT_FLAG();
    uint32_t u32State = USBD_GET_BUS_STATE();

    //------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_FLDET)
    {
        // Floating detect
        USBD_CLR_INT_FLAG(USBD_INTSTS_FLDET);

        if(USBD_IS_ATTACHED())
        {
            /* USB Plug In */
            USBD_ENABLE_USB();
        }
        else
        {
            /* USB Un-plug */
            USBD_DISABLE_USB();
        }
    }

    //------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_BUS)
    {
        /* Clear event flag */
        USBD_CLR_INT_FLAG(USBD_INTSTS_BUS);

        if(u32State & USBD_STATE_USBRST)
        {
            /* Bus reset */
            USBD_ENABLE_USB();
            USBD_SwReset();
            
            /* Reconfigure endpoints after reset - hardware clears endpoint config on bus reset */
            /* EP0 ==> control IN endpoint, address 0 */
            USBD_CONFIG_EP(EP0, USBD_CFG_CSTALL | USBD_CFG_EPMODE_IN | 0);
            USBD_SET_EP_BUF_ADDR(EP0, EP0_BUF_BASE);
            
            /* EP1 ==> control OUT endpoint, address 0 */
            USBD_CONFIG_EP(EP1, USBD_CFG_CSTALL | USBD_CFG_EPMODE_OUT | 0);
            USBD_SET_EP_BUF_ADDR(EP1, EP1_BUF_BASE);
            
            /* Unused endpoints must point to end of SRAM so EP1 gets a valid buffer size */
            USBD_SET_EP_BUF_ADDR(EP2, 512);
            USBD_SET_EP_BUF_ADDR(EP3, 512);
            USBD_SET_EP_BUF_ADDR(EP4, 512);
            USBD_SET_EP_BUF_ADDR(EP5, 512);
            USBD_SET_EP_BUF_ADDR(EP6, 512);
            USBD_SET_EP_BUF_ADDR(EP7, 512);

            /* Prepare to receive setup packet */
            USBD_SET_PAYLOAD_LEN(EP0, EP0_MAX_PKT_SIZE);
        }

        if(u32State & USBD_STATE_SUSPEND)
        {
            /* Enable USB but disable PHY */
            USBD_DISABLE_PHY();
        }

        if(u32State & USBD_STATE_RESUME)
        {
            /* Enable USB and enable PHY */
            USBD_ENABLE_USB();
        }

    }

    //------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_WAKEUP)
    {
        /* Clear event flag */
        USBD_CLR_INT_FLAG(USBD_INTSTS_WAKEUP);
    }

    //------------------------------------------------------------------
if(u32IntSts & USBD_INTSTS_USB)
{
    if(u32IntSts & USBD_INTSTS_SETUP)
    {
        /* Clear the setup flag and cancel any in-flight EP0/EP1 transactions.
           SETUP arrival also sets EP1 — we must NOT call USBD_CtrlOut()
           in the same ISR pass or the freshly-written MXPLD looks like
           received data, corrupting the OUT byte counter. */
        USBD_CLR_INT_FLAG(USBD_INTSTS_SETUP);
        USBD_STOP_TRANSACTION(EP0);
        USBD_STOP_TRANSACTION(EP1);
        USBD_ProcessSetupPacket();
        /* Fall through without processing EP0/EP1 this cycle */
    }
    else
    {
        if(u32IntSts & USBD_INTSTS_EP0)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP0);
            USBD_CtrlIn();
        }

        if(u32IntSts & USBD_INTSTS_EP1)
        {
            USBD_CLR_INT_FLAG(USBD_INTSTS_EP1);
            USBD_CtrlOut();
        }
    }
}
}


/*--------------------------------------------------------------------------*/
/**
  * @brief  USBD Endpoint Config.
  * @param  None.
  * @retval None.
  */

static void DFU_ReplyStatus(void)
{
    USBD_PrepareCtrlIn((uint8_t *)&dfu_status, 6);
    USBD_PrepareCtrlOut(0, 0);
}

void DFU_Init(void)
{
    /* Init setup packet buffer */
    /* Buffer range for setup packet -> [0 ~ 0x7] */
    USBD->STBUFSEG = SETUP_BUF_BASE;

    /*****************************************************/
    /* EP0 ==> control IN endpoint, address 0 */
    USBD_CONFIG_EP(EP0, USBD_CFG_CSTALL | USBD_CFG_EPMODE_IN | 0);
    /* Buffer range for EP0 */
    USBD_SET_EP_BUF_ADDR(EP0, EP0_BUF_BASE);

    /* EP1 ==> control OUT endpoint, address 0 */
    USBD_CONFIG_EP(EP1, USBD_CFG_CSTALL | USBD_CFG_EPMODE_OUT | 0);
    /* Buffer range for EP1 */
    USBD_SET_EP_BUF_ADDR(EP1, EP1_BUF_BASE);

    /* restore device default state */
    dfu_status.bStatus = STATUS_OK;
    dfu_status.bState = STATE_dfuIDLE;

    prog_struct.block_num = 0;
    prog_struct.data_len = 0;

}


void DFU_ClassRequest(void)
{
    uint8_t buf[8];
    uint32_t wValue, wLength;

    USBD_GetSetupPacket(buf);

    wValue  = buf[3] << 8 | buf[2];
    wLength = buf[7] << 8 | buf[6];

    if(buf[0] & 0x80)    /* request data transfer direction */
    {
        switch(buf[1])
        {
            case DFU_GETSTATUS:
            {
                if(dfu_status.bState == STATE_dfuDNLOAD_SYNC)
                {
                    /* Host polled before OUT data finished — still programming */
                    SET_POLLING_TIMEOUT(FLASH_WRITE_TIMEOUT);
                    dfu_status.bStatus = STATUS_OK;
                    dfu_status.bState = STATE_dfuDNBUSY;
                }
                else if(dfu_status.bState == STATE_dfuMANIFEST_SYNC)
                {
                    manifest_state = MANIFEST_COMPLETE;
                    dfu_status.bStatus = STATUS_OK;
                    dfu_status.bState = STATE_dfuIDLE;
                    g_reset = 1;
                }

                DFU_ReplyStatus();
                break;
            }

            case DFU_GETSTATE:
            {
                USBD_PrepareCtrlIn((uint8_t *)&dfu_status.bState, 1);
                USBD_PrepareCtrlOut(0, 0);
                break;
            }

            case DFU_UPLOAD:
            {
                if(dfu_status.bState == STATE_dfuIDLE || dfu_status.bState == STATE_dfuUPLOAD_IDLE)
                {
                    if(wLength <= 0)
                    {
                        dfu_status.bState = STATE_dfuIDLE;
                        return;
                    }
                    else
                    {
                        if(dfu_status.bState == STATE_dfuIDLE)
                        {
                            dfu_status.bState = STATE_dfuUPLOAD_IDLE;
                        }

                        if(wValue > APROM_BLOCK_NUM)
                        {
                            dfu_status.bState = STATE_dfuIDLE;
                            USBD_PrepareCtrlIn(0, 0);
                            USBD_PrepareCtrlOut(0, 0);
                            break;
                        }

                        ReadData(wValue * TRANSFER_SIZE, (wValue * TRANSFER_SIZE) + wLength, (uint32_t *)prog_struct.buf);
                        USBD_PrepareCtrlIn((uint8_t *)prog_struct.buf, wLength);
                    }

                    USBD_PrepareCtrlOut(0, 0);
                }

                break;
            }

            default:
            {
                // Stall
                /* Setup error, stall the device */
                USBD_SetStall(0);
                break;
            }
        }
    }
    else
    {
        switch(buf[1])
        {
            case DFU_DETACH:
            {
                switch(dfu_status.bState)
                {
                    case STATE_dfuIDLE:
                    case STATE_dfuDNLOAD_SYNC:
                    case STATE_dfuDNLOAD_IDLE:
                    case STATE_dfuMANIFEST_SYNC:
                    case STATE_dfuUPLOAD_IDLE:
                        dfu_status.bStatus = STATUS_OK;
                        dfu_status.bState = STATE_dfuIDLE;
                        dfu_status.iString = 0; /* iString */
                        prog_struct.block_num = 0;
                        prog_struct.data_len = 0;
                        g_reset = 1;
                        break;

                    default:
                        break;
                }
                USBD_PrepareCtrlIn(0, 0);
                break;
            }

            case DFU_DNLOAD:
            {
                switch(dfu_status.bState)
                {
                    case STATE_dfuIDLE:
                    case STATE_dfuDNLOAD_IDLE:
                        if(wLength > 0)
                        {
                            /* update the global length and block number */
                            prog_struct.block_num = wValue;
                            prog_struct.data_len = wLength;
                            dfu_status.bState = STATE_dfuDNLOAD_SYNC;

                        }
                        else
                        {
                            manifest_state = MANIFEST_IN_PROGRESS;
                            dfu_status.bState = STATE_dfuMANIFEST_SYNC;

                        }

                        if(wLength > 0)
                        {
                            USBD_PrepareCtrlOut((uint8_t *)prog_struct.buf, wLength);
                        }
                        else
                        {
                            USBD_PrepareCtrlIn(0, 0);
                        }
                        break;

                    default:
                        USBD_SetStall(0);
                        break;
                }

                break;
            }


            case DFU_CLRSTATUS:
            {
                dfu_status.bStatus = STATUS_OK;
                dfu_status.bState = STATE_dfuIDLE;
                dfu_status.iString = 0;
                USBD_PrepareCtrlIn(0, 0);
                break;
            }


            case DFU_ABORT:
            {
                switch(dfu_status.bState)
                {
                    case STATE_dfuIDLE:
                    case STATE_dfuDNLOAD_SYNC:
                    case STATE_dfuDNLOAD_IDLE:
                    case STATE_dfuMANIFEST_SYNC:
                    case STATE_dfuUPLOAD_IDLE:
                        dfu_status.bStatus = STATUS_OK;
                        dfu_status.bState = STATE_dfuIDLE;
                        dfu_status.iString = 0; /* iString: index = 0 */

                        prog_struct.block_num = 0;
                        prog_struct.data_len = 0;
                        break;

                    default:
                        break;
                }
                break;
            }


            default:
            {
                /* Setup error, stall the device */
                USBD_SetStall(0);
                break;
            }

        }

    }
}

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/
