/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright(c) 2019 Nuvoton Technology Corp. All rights reserved.                                         */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/

    .syntax unified
    .cpu cortex-m0
    .thumb

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    .section .stack, "w"
    .align 3
#ifdef Stack_Size
    .equ Stack_Size, Stack_Size
#else
    .equ Stack_Size, 0x00000800
#endif
    .globl __initial_sp
    .space Stack_Size
__initial_sp:

    .section .heap, "w"
    .align 3
#ifdef Heap_Size
    .equ Heap_Size, Heap_Size
#else
    .equ Heap_Size, 0x00000000
#endif
    .globl __heap_base
__heap_base:
    .space Heap_Size
__heap_limit:

    .section .isr_vector, "a"
    .align 2
    .globl __Vectors
__Vectors:
    .word __initial_sp              @ Top of Stack
    .word Reset_Handler             @ Reset Handler
    .word NMI_Handler               @ NMI Handler
    .word HardFault_Handler         @ Hard Fault Handler
    .word 0                         @ Reserved
    .word 0                         @ Reserved
    .word 0                         @ Reserved
    .word 0                         @ Reserved
    .word 0                         @ Reserved
    .word 0                         @ Reserved
    .word 0                         @ Reserved
    .word SVC_Handler               @ SVCall Handler
    .word 0                         @ Reserved
    .word 0                         @ Reserved
    .word PendSV_Handler            @ PendSV Handler
    .word SysTick_Handler           @ SysTick Handler

    @ External Interrupts
    .word BOD_IRQHandler            @ 0  BOD
    .word WDT_IRQHandler            @ 1  WDT
    .word EINT0_IRQHandler          @ 2  EINT0
    .word EINT1_IRQHandler          @ 3  EINT1
    .word GPAB_IRQHandler           @ 4  GPIO PA/PB
    .word GPCDF_IRQHandler          @ 5  GPIO PC/PD/PF
    .word PWMA_IRQHandler           @ 6  PWMA
    .word 0                         @ 7  Reserved
    .word TMR0_IRQHandler           @ 8  TIMER0
    .word TMR1_IRQHandler           @ 9  TIMER1
    .word TMR2_IRQHandler           @ 10 TIMER2
    .word TMR3_IRQHandler           @ 11 TIMER3
    .word UART0_IRQHandler          @ 12 UART0
    .word UART1_IRQHandler          @ 13 UART1
    .word SPI0_IRQHandler           @ 14 SPI0
    .word SPI1_IRQHandler           @ 15 SPI1
    .word SPI2_IRQHandler           @ 16 SPI2
    .word 0                         @ 17 Reserved
    .word I2C0_IRQHandler           @ 18 I2C0
    .word I2C1_IRQHandler           @ 19 I2C1
    .word CAN0_IRQHandler           @ 20 CAN0
    .word CAN1_IRQHandler           @ 21 CAN1
    .word 0                         @ 22 Reserved
    .word USBD_IRQHandler           @ 23 USBD
    .word PS2_IRQHandler            @ 24 PS2
    .word 0                         @ 25 Reserved
    .word PDMA_IRQHandler           @ 26 PDMA
    .word I2S_IRQHandler            @ 27 I2S
    .word PWRWU_IRQHandler          @ 28 PWRWU
    .word ADC_IRQHandler            @ 29 ADC
    .word IRC_IRQHandler            @ 30 IRC

    .section .text
    .thumb

    .globl Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    @ Unlock Register
    ldr r0, =0x50000100
    movs r1, #0x59
    str r1, [r0]
    movs r1, #0x16
    str r1, [r0]
    movs r1, #0x88
    str r1, [r0]

    @ Init POR
    ldr r2, =0x50000024
    ldr r1, =0x5AA5
    str r1, [r2]

    @ Lock register
    movs r1, #0
    str r1, [r0]

    @ Copy .data section from LDROM to SRAM
    ldr r0, =_sidata      @ Load address of .data in LDROM
    ldr r1, =_sdata       @ Runtime address of .data in SRAM
    ldr r2, =_edata       @ End address of .data in SRAM
copy_loop:
    cmp r1, r2
    beq copy_done
    ldr r3, [r0]
    str r3, [r1]
    adds r0, r0, #4
    adds r1, r1, #4
    b copy_loop
copy_done:

    @ Zero .bss
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r0, #0
    cmp r1, r2
    beq bss_done
bss_loop:
    str r0, [r1]
    adds r1, r1, #4
    cmp r1, r2
    bcc bss_loop
bss_done:

    @ Call SystemInit
    bl SystemInit

    @ Call main
    bl main

    @ Trap if main returns
    b .

    .size Reset_Handler, .-Reset_Handler

@ Dummy Exception Handlers (infinite loops)

    .weak NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler

    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler

    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler

    .weak BOD_IRQHandler
    .thumb_set BOD_IRQHandler, Default_Handler

    .weak WDT_IRQHandler
    .thumb_set WDT_IRQHandler, Default_Handler

    .weak EINT0_IRQHandler
    .thumb_set EINT0_IRQHandler, Default_Handler

    .weak EINT1_IRQHandler
    .thumb_set EINT1_IRQHandler, Default_Handler

    .weak GPAB_IRQHandler
    .thumb_set GPAB_IRQHandler, Default_Handler

    .weak GPCDF_IRQHandler
    .thumb_set GPCDF_IRQHandler, Default_Handler

    .weak PWMA_IRQHandler
    .thumb_set PWMA_IRQHandler, Default_Handler

    .weak TMR0_IRQHandler
    .thumb_set TMR0_IRQHandler, Default_Handler

    .weak TMR1_IRQHandler
    .thumb_set TMR1_IRQHandler, Default_Handler

    .weak TMR2_IRQHandler
    .thumb_set TMR2_IRQHandler, Default_Handler

    .weak TMR3_IRQHandler
    .thumb_set TMR3_IRQHandler, Default_Handler

    .weak UART0_IRQHandler
    .thumb_set UART0_IRQHandler, Default_Handler

    .weak UART1_IRQHandler
    .thumb_set UART1_IRQHandler, Default_Handler

    .weak SPI0_IRQHandler
    .thumb_set SPI0_IRQHandler, Default_Handler

    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler

    .weak SPI2_IRQHandler
    .thumb_set SPI2_IRQHandler, Default_Handler

    .weak I2C0_IRQHandler
    .thumb_set I2C0_IRQHandler, Default_Handler

    .weak I2C1_IRQHandler
    .thumb_set I2C1_IRQHandler, Default_Handler

    .weak CAN0_IRQHandler
    .thumb_set CAN0_IRQHandler, Default_Handler

    .weak CAN1_IRQHandler
    .thumb_set CAN1_IRQHandler, Default_Handler

    .weak USBD_IRQHandler
    .thumb_set USBD_IRQHandler, Default_Handler

    .weak PS2_IRQHandler
    .thumb_set PS2_IRQHandler, Default_Handler

    .weak PDMA_IRQHandler
    .thumb_set PDMA_IRQHandler, Default_Handler

    .weak I2S_IRQHandler
    .thumb_set I2S_IRQHandler, Default_Handler

    .weak PWRWU_IRQHandler
    .thumb_set PWRWU_IRQHandler, Default_Handler

    .weak ADC_IRQHandler
    .thumb_set ADC_IRQHandler, Default_Handler

    .weak IRC_IRQHandler
    .thumb_set IRC_IRQHandler, Default_Handler

    .globl Default_Handler
    .type Default_Handler, %function
Default_Handler:
    b .
    .size Default_Handler, .-Default_Handler

    .align 2