/***************************************************************************//**
 * @file     main.c
 * @brief    DFU bootloader for Ducky One 2 SF (DKON1967ST)
 *
 *           Hold Esc (PD11 + PB10) while plugging in for DFU.
 *           Build with -DBOOTLOADER_FORCE_DFU=1 (make dfu-always) to skip Esc.
 *
 * @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "NUC123.h"
#include "fmc_user.h"
#include "dfu_transfer.h"
#include "clk.h"
#include "usbd.h"

uint32_t g_romSize;
uint8_t g_reset = 0;
extern s_prog_struct prog_struct;
extern dfu_status_struct dfu_status;

uint32_t GetRomSize()
{
    uint32_t size = 0x800, data;
    int result;
    do
    {
        result = FMC_Read_User(size, &data);
        if(result < 0)
            return size;
        else
            size *= 2;
    }
    while(1);
}

uint8_t isLDROM(void)
{
    return !!(FMC->ISPCON & FMC_ISPCON_BS_Msk);
}

/* COL2ROW: row0 PD11 low, col0 PB10 low = Esc */
static uint8_t isEscapePressed(void)
{
    uint8_t pressed;

    PD->PMD = (PD->PMD & ~(3UL << 22)) | (1UL << 22);
    PD->DOUT &= ~(1UL << 11);

    PB->PMD = (PB->PMD & ~(3UL << 20)) | (3UL << 20);

    CLK_SysTickDelay(500);

    pressed = !((PB->PIN >> 10) & 1);

    PD->PMD = (PD->PMD & ~(3UL << 22)) | (3UL << 22);
    PB->PMD = (PB->PMD & ~(3UL << 20)) | (3UL << 20);

    return pressed;
}

void SYS_Init(void)
{
    SYS->GPA_MFPH = (SYS->GPA_MFPH & ~(SYS_GPA_MFPH_GPA14_MFP_Msk | SYS_GPA_MFPH_GPA15_MFP_Msk))
                  | (0x1UL << SYS_GPA_MFPH_GPA14_MFP_Pos)
                  | (0x1UL << SYS_GPA_MFPH_GPA15_MFP_Pos);

    CLK_EnableXtalRC(CLK_PWRCON_OSC22M_EN_Msk);
    CLK_WaitClockReady(CLK_CLKSTATUS_OSC22M_STB_Msk);

    CLK_EnablePLL(CLK_PLLCON_48MHz_HIRC, 48000000);
    CLK_WaitClockReady(CLK_CLKSTATUS_PLL_STB_Msk);

    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_PLL, CLK_CLKDIV_HCLK(1));
    CLK_SetModuleClock(USBD_MODULE, 0, CLK_CLKDIV_USB(1));
    CLK_EnableModuleClock(USBD_MODULE);

    SystemCoreClockUpdate();
}

void USBD_IRQHandler(void);

int main(void)
{
    SYS_UnlockReg();
    SYS_Init();

    /*
     * LDROM always runs DFU (matches old "if(1)" test build).
     * After flashing QMK via dfu-util, host DETACH resets to APROM.
     * To gate on Esc again: wrap USB block in if(isEscapePressed() || ...).
     */
    {
        CLK->AHBCLK |= CLK_AHBCLK_ISP_EN_Msk;
        FMC->ISPCON |= FMC_ISPCON_ISPEN_Msk | FMC_ISPCON_APUEN_Msk | FMC_ISPCON_ISPFF_Msk;

        g_romSize = 0x8000;

        USBD_Open(&gsInfo, DFU_ClassRequest, NULL);
        DFU_Init();
        USBD_ENABLE_USB();
        USBD->ATTR |= USBD_DPPU_EN;
        USBD_Start();

        while(!g_reset)
            USBD_IRQHandler();
    }

    SYS->RSTSRC = (SYS_RSTSRC_RSTS_POR_Msk | SYS_RSTSRC_RSTS_RESET_Msk | SYS_RSTSRC_RSTS_SYS_Msk);

    FMC->ISPCON &= ~(FMC_ISPCON_ISPEN_Msk);
    FMC->ISPCON &= ~(FMC_ISPCON_BS_Msk);

    NVIC_SystemReset();

    while(1);
}
