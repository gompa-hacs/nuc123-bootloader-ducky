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

uint32_t command_Count = 0;
uint8_t manifest_state = MANIFEST_IN_PROGRESS;
dfu_status_struct dfu_status;
s_prog_struct prog_struct __attribute__((aligned(4), section(".bss"))) = {0};

void USBD_IRQHandler(void)
{
    uint32_t u32IntSts = USBD_GET_INT_FLAG();
    uint32_t u32State = USBD_GET_BUS_STATE();

    //------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_FLDET)
    {
        USBD_CLR_INT_FLAG(USBD_INTSTS_FLDET);

        if(USBD_IS_ATTACHED())
            USBD_ENABLE_USB();
        else
            USBD_DISABLE_USB();
    }

    //------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_BUS)
    {
        USBD_CLR_INT_FLAG(USBD_INTSTS_BUS);

        if(u32State & USBD_STATE_USBRST)
        {
            USBD_ENABLE_USB();
            USBD_SwReset();
        }

        if(u32State & USBD_STATE_SUSPEND)
            USBD_DISABLE_PHY();

        if(u32State & USBD_STATE_RESUME)
            USBD_ENABLE_USB();
    }

    //------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_WAKEUP)
        USBD_CLR_INT_FLAG(USBD_INTSTS_WAKEUP);

    //------------------------------------------------------------------
    if(u32IntSts & USBD_INTSTS_USB)
    {
        if(u32IntSts & USBD_INTSTS_SETUP)
        {
            /* SETUP also asserts EP1; ignore EP1 this pass (breaks 1024-byte DNLOAD) */
            USBD_CLR_INT_FLAG(USBD_INTSTS_SETUP);
            USBD_STOP_TRANSACTION(EP0);
            USBD_STOP_TRANSACTION(EP1);
            USBD_ProcessSetupPacket();
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

void DFU_Init(void)
{
    USBD->STBUFSEG = SETUP_BUF_BASE;

    USBD_CONFIG_EP(EP0, USBD_CFG_CSTALL | USBD_CFG_EPMODE_IN | 0);
    USBD_SET_EP_BUF_ADDR(EP0, EP0_BUF_BASE);

    USBD_CONFIG_EP(EP1, USBD_CFG_CSTALL | USBD_CFG_EPMODE_OUT | 0);
    USBD_SET_EP_BUF_ADDR(EP1, EP1_BUF_BASE);

    dfu_status.bStatus = STATUS_OK;
    dfu_status.bState = STATE_dfuIDLE;

    prog_struct.block_num = 0;
    prog_struct.data_len = 0;
    prog_struct.block_stride = TRANSFER_SIZE;
}

void DFU_ClassRequest(void)
{
    uint8_t buf[8];
    uint32_t wValue, wLength;

    USBD_GetSetupPacket(buf);

    wValue  = buf[3] << 8 | buf[2];
    wLength = buf[7] << 8 | buf[6];

    if(buf[0] & 0x80)
    {
        switch(buf[1])
        {
            case DFU_GETSTATUS:
            {
                if(dfu_status.bState == STATE_dfuDNLOAD_SYNC)
                {
                    command_Count++;
                    SET_POLLING_TIMEOUT(FLASH_WRITE_TIMEOUT);

                    if(command_Count == 5)
                    {
                        dfu_status.bState = STATE_dfuDNLOAD_IDLE;
                        {
                            uint32_t addr = (uint32_t)prog_struct.block_num * prog_struct.block_stride;
                            WriteData(addr, addr + prog_struct.data_len, (uint32_t *)prog_struct.buf);
                        }
                        command_Count = 0;
                    }
                }

                if(dfu_status.bState == STATE_dfuDNLOAD_IDLE)
                {
                    command_Count++;

                    if(command_Count == 5)
                    {
                        dfu_status.bState = STATE_dfuMANIFEST_SYNC;
                        command_Count = 0;
                    }
                }

                if(dfu_status.bState == STATE_dfuMANIFEST_SYNC)
                {
                    command_Count++;

                    if(command_Count == 5)
                    {
                        dfu_status.bState = STATE_dfuIDLE;
                        command_Count = 0;
                        manifest_state = MANIFEST_COMPLETE;
                    }
                }

                USBD_PrepareCtrlIn((uint8_t *)&dfu_status.bStatus, 6);
                USBD_PrepareCtrlOut(0, 0);
                break;
            }

            case DFU_GETSTATE:
                USBD_PrepareCtrlIn((uint8_t *)&dfu_status.bState, 1);
                USBD_PrepareCtrlOut(0, 0);
                break;

            case DFU_UPLOAD:
                if(dfu_status.bState == STATE_dfuIDLE || dfu_status.bState == STATE_dfuUPLOAD_IDLE)
                {
                    if(wLength <= 0)
                    {
                        dfu_status.bState = STATE_dfuIDLE;
                        return;
                    }

                    if(dfu_status.bState == STATE_dfuIDLE)
                        dfu_status.bState = STATE_dfuUPLOAD_IDLE;

                    if(wValue > APROM_BLOCK_NUM)
                    {
                        dfu_status.bState = STATE_dfuIDLE;
                        USBD_PrepareCtrlIn(0, 0);
                        USBD_PrepareCtrlOut(0, 0);
                        break;
                    }

                    ReadData(wValue * TRANSFER_SIZE, (wValue * TRANSFER_SIZE) + wLength,
                              (uint32_t *)prog_struct.buf);
                    USBD_PrepareCtrlIn((uint8_t *)prog_struct.buf, wLength);
                    USBD_PrepareCtrlOut(0, 0);
                }
                break;

            default:
                USBD_SetStall(0);
                break;
        }
    }
    else
    {
        switch(buf[1])
        {
            case DFU_DETACH:
                switch(dfu_status.bState)
                {
                    case STATE_dfuIDLE:
                    case STATE_dfuDNLOAD_SYNC:
                    case STATE_dfuDNLOAD_IDLE:
                    case STATE_dfuMANIFEST_SYNC:
                    case STATE_dfuUPLOAD_IDLE:
                        dfu_status.bStatus = STATUS_OK;
                        dfu_status.bState = STATE_dfuIDLE;
                        dfu_status.iString = 0;
                        prog_struct.block_num = 0;
                        prog_struct.data_len = 0;
                        g_reset = 1;
                        break;
                    default:
                        break;
                }
                USBD_PrepareCtrlIn(0, 0);
                break;

            case DFU_DNLOAD:
                switch(dfu_status.bState)
                {
                    case STATE_dfuIDLE:
                    case STATE_dfuDNLOAD_IDLE:
                        if(wLength > 0)
                        {
                            prog_struct.block_num = wValue;
                            prog_struct.data_len = wLength;
                            /* Lock stride on first block (e.g. dfu-util --transfer-size 64) */
                            if(dfu_status.bState == STATE_dfuIDLE)
                                prog_struct.block_stride = wLength;
                            dfu_status.bState = STATE_dfuDNLOAD_SYNC;
                        }
                        else
                        {
                            command_Count = 0;
                            manifest_state = MANIFEST_IN_PROGRESS;
                            dfu_status.bState = STATE_dfuMANIFEST_SYNC;
                        }

                        if(wLength > 0)
                        {
                            /* Status ZLP is sent from USBD_CtrlOut when OUT completes */
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

            case DFU_CLRSTATUS:
                dfu_status.bStatus = STATUS_OK;
                dfu_status.bState = STATE_dfuIDLE;
                dfu_status.iString = 0;
                USBD_PrepareCtrlIn(0, 0);
                break;

            case DFU_ABORT:
                switch(dfu_status.bState)
                {
                    case STATE_dfuIDLE:
                    case STATE_dfuDNLOAD_SYNC:
                    case STATE_dfuDNLOAD_IDLE:
                    case STATE_dfuMANIFEST_SYNC:
                    case STATE_dfuUPLOAD_IDLE:
                        dfu_status.bStatus = STATUS_OK;
                        dfu_status.bState = STATE_dfuIDLE;
                        dfu_status.iString = 0;
                        prog_struct.block_num = 0;
                        prog_struct.data_len = 0;
                        break;
                    default:
                        break;
                }
                USBD_PrepareCtrlIn(0, 0);
                break;

            default:
                USBD_SetStall(0);
                break;
        }
    }
}

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/
