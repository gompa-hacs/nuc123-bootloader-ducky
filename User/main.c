/***************************************************************************//**
 * @file     main.c
 * @brief    DFU bootloader for Ducky One 2 SF (DKON1967ST)
 *
 * @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include "NUC123.h"
#include "fmc_user.h"
#include "dfu_transfer.h"
#include "clk.h"
#include "usbd.h"

uint32_t g_romSize;
uint8_t g_reset = 0;

/* Verify after flash: openocd ... -c "mdw 0x00100D88 1"  (expect 0x466FC3D0) */
const uint32_t g_ldrom_build_tag __attribute__((used)) = 0x466fc3d0u;

uint8_t isLDROM(void)
{
    return !!(FMC->ISPCON & FMC_ISPCON_BS_Msk);
}

uint8_t isEscapePressed(void)
{
    uint8_t pressed;

    PD->PMD = (PD->PMD & ~(0x3UL << 22)) | (0x1UL << 22);
    PD->DOUT &= ~(1UL << 11);
    PB->PMD = (PB->PMD & ~(0x3UL << 20)) | (0x3UL << 20);
    CLK_SysTickDelay(500);
    pressed = !((PB->PIN >> 10) & 1);
    PD->PMD = (PD->PMD & ~(0x3UL << 22)) | (0x3UL << 22);
    PB->PMD = (PB->PMD & ~(0x3UL << 20)) | (0x3UL << 20);
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

    /* recovery.bin: always DFU. Normal build: Esc at plug-in or QMK system reset */
    if(isEscapePressed() || (SYS->RSTSRC & SYS_RSTSRC_RSTS_SYS_Msk)
#if defined(BOOTLOADER_FORCE_DFU)
       || 1
#endif
      )
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

    SYS->RSTSRC = (SYS_RSTSRC_RSTS_POR_Msk | SYS_RSTSRC_RSTS_RESET_Msk);
    FMC->ISPCON &= ~(FMC_ISPCON_ISPEN_Msk);
    FMC->ISPCON &= ~(FMC_ISPCON_BS_Msk);
    NVIC_SystemReset();
    while(1);
}
