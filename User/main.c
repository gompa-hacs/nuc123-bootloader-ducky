/***************************************************************************//**
 * @file     main.c
 * @brief    DFU bootloader for Ducky One 2 SF (DKON1967ST)
 *           Modified from Nuvoton DFU sample by giannello.
 *           Adapted for NUC123SD4AN0 / Ducky One 2 SF matrix pinout.
 *
 *           Enter DFU mode by holding ESCAPE while plugging in the keyboard.
 *           Escape key: row PD11 (row 0), col PB10 (col 0)
 *
 *           After flashing, the bootloader automatically boots APROM.
 *
 * @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "NUC123.h"
#include "fmc_user.h"
#include "dfu_transfer.h"
#include "clk.h"
#include "usbd.h"

#define PLLCON_SETTING    CLK_PLLCON_144MHz_HXT
#define PLL_CLOCK         144000000

/*
 * Ducky One 2 SF matrix pins (from QMK info.json):
 *   Rows: D11, B4, B5, B6, B7
 *   Cols: B10, B9, C13, C12, C11, C10, C9, C8, A15, A14, A13, D0, D1, D2, B15, B8
 *   Diode direction: COL2ROW
 *
 * Escape key: matrix [0,0] -> row 0 = PD11, col 0 = PB10
 *
 * GPIO PMD register: 2 bits per pin
 *   00 = input, 01 = output, 10 = open-drain, 11 = quasi-bidirectional (pull-up)
 * PD11 = bits [23:22] of PD->PMD
 * PB10 = bits [21:20] of PB->PMD
 */

uint32_t g_romSize;
uint8_t g_reset = 0;
extern volatile uint8_t g_write_pending;

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

uint8_t isLDROM()
{
    return !!(FMC->ISPCON & FMC_ISPCON_BS_Msk);
}

/*
 * Check if Escape is held using direct register access.
 * Avoids pulling in gpio.c to save space.
 *
 * COL2ROW: drive row PD11 low, read col PB10 with pull-up.
 * If pressed, PB10 reads low.
 */
uint8_t isEscapePressed(void)
{
    uint8_t pressed;

    /* Set PD11 as output (PMD bits [23:22] = 01) */
    PD->PMD = (PD->PMD & ~(0x3UL << 22)) | (0x1UL << 22);
    /* Drive PD11 low */
    PD->DOUT &= ~(1UL << 11);

    /* Set PB10 as quasi-bidirectional/pull-up (PMD bits [21:20] = 11) */
    PB->PMD = (PB->PMD & ~(0x3UL << 20)) | (0x3UL << 20);

    /* Settle delay */
    CLK_SysTickDelay(500);

    /* Read PB10 — low means key pressed */
    pressed = !((PB->PIN >> 10) & 1);

    /* Restore both pins to quasi-bidirectional */
    PD->PMD = (PD->PMD & ~(0x3UL << 22)) | (0x3UL << 22);
    PB->PMD = (PB->PMD & ~(0x3UL << 20)) | (0x3UL << 20);

    return pressed;
}

void SYS_Init(void)
{
    /* USB D- (PA.14) and D+ (PA.15) */
    SYS->GPA_MFPH = (SYS->GPA_MFPH & ~(SYS_GPA_MFPH_GPA14_MFP_Msk | SYS_GPA_MFPH_GPA15_MFP_Msk))
                  | (0x1UL << SYS_GPA_MFPH_GPA14_MFP_Pos)
                  | (0x1UL << SYS_GPA_MFPH_GPA15_MFP_Pos);

    /*
     * USB full-speed needs a 48 MHz USB clock.
     * Use HIRC -> PLL (~48 MHz) -> USB (divide-by-1), HCLK from PLL.
     */
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
    /* Unlock write-protected registers */
    SYS_UnlockReg();

    /* Init system and multi-function I/O */
    SYS_Init();

    /*
     * Enter DFU mode if:
     *   1. Escape is held at plug-in, OR
     *   2. We got here via software system reset (QK_BOOT from QMK)
     */
    // if (isEscapePressed() || (SYS->RSTSRC & SYS_RSTSRC_RSTS_SYS_Msk))
    if (1)
    {
        CLK->AHBCLK |= CLK_AHBCLK_ISP_EN_Msk;
        FMC->ISPCON |= FMC_ISPCON_ISPEN_Msk | FMC_ISPCON_APUEN_Msk | FMC_ISPCON_ISPFF_Msk;
        
        g_romSize = 0x8000;  /* 32KB APROM */

        USBD_Open(&gsInfo, DFU_ClassRequest, NULL);
        
        NVIC_EnableIRQ(USBD_IRQn);
        
        /* Enable USB controller with proper bit settings */
        USBD->ATTR = (USBD->ATTR & ~USBD_ATTR_DPPU_EN_Msk) | USBD_DPPU_EN;
        USBD_ENABLE_USB();
        
        /* Software reset USB controller to ensure clean state */
        USBD_SwReset();
        
        /* Re-enable USB after reset - SW reset clears ATTR */
        USBD->ATTR = (USBD->ATTR & ~USBD_ATTR_DPPU_EN_Msk) | USBD_DPPU_EN;
        USBD_ENABLE_USB();
        
        USBD_Start();
        
        /* Initialize endpoints AFTER USB is enabled */
        DFU_Init();
        /* Note: Control OUT is prepared in DFU_DNLOAD handler after SETUP packet arrives */

        /* Wait for reset signal - USB processing handled by interrupts */
        while(!g_reset)
        {
            if(g_write_pending)
            {
                g_write_pending = 0;
                /* Write data to flash */
                WriteData(prog_struct.block_num * TRANSFER_SIZE,
                        (prog_struct.block_num * TRANSFER_SIZE) + prog_struct.data_len,
                        (uint32_t *)prog_struct.buf);
                dfu_status.bStatus = STATUS_OK;
                dfu_status.bState  = STATE_dfuDNLOAD_IDLE;
                
                /* Send ZLP to signal completion - host will poll GETSTATUS next */
                // USBD_PrepareCtrlIn(0, 0);
            }
            
            /* Enter low power mode while waiting */
            __WFI();
        }
    }

    /* Clear reset source bits */
    SYS->RSTSRC = (SYS_RSTSRC_RSTS_POR_Msk | SYS_RSTSRC_RSTS_RESET_Msk);

    /* Disable ISP */
    FMC->ISPCON &= ~(FMC_ISPCON_ISPEN_Msk);

    /* Boot from APROM */
    FMC->ISPCON &= ~(FMC_ISPCON_BS_Msk);

    /* Reset into APROM */
    NVIC_SystemReset();

    while(1);
}