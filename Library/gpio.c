/***************************************************************************//**
 * @file     gpio.c
 * @brief    NUC123 series GPIO driver source file
 * @version  1.0.0
 *
 * @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include "NUC123.h"
#include "gpio.h"

/**
 * @brief       Set GPIO pin mode
 *
 * @param[in]   port    GPIO port (PD, PB, etc.)
 * @param[in]   u32PinMask  Pin mask (BIT0-BIT15)
 * @param[in]   u32Mode Pin mode (INPUT, OUTPUT, QUASI, ANALOG)
 *
 * @return      None
 *
 * @details     Set the specified GPIO pins to the specified mode.
 */
void GPIO_SetMode(GPIO_T *port, uint32_t u32PinMask, uint32_t u32Mode)
{
    port->PMD = (port->PMD & ~u32PinMask) | (u32Mode << __builtin_ctz(u32PinMask) * 2);
}

/* GPIO_EnableInt not implemented - not needed for this project */
