/*
 * Copyright (c) 2013 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <sys/types.h>

#include <aprinter/base/JoinTokens.h>

#include "stm32f4_support.h"

#ifdef APRINTER_ENABLE_USB
#include <usbd_core.h>
#include "usbd_desc.h"
#endif

static void init_clock (void);
static void init_usb (void);
static void init_final_usb (void);

#ifdef APRINTER_ENABLE_USB
extern "C" PCD_HandleTypeDef hpcd;
USBD_HandleTypeDef USBD_Device;
#endif

extern "C" {
    void NMI_Handler (void)
    {
    }
    
    void HardFault_Handler (void)
    {
        while (1);
    }
    
    void MemManage_Handler (void)
    {
        while (1);
    }
    
    void BusFault_Handler (void)
    {
        while (1);
    }
    
    void UsageFault_Handler (void)
    {
        while (1);
    }
    
    void SVC_Handler (void)
    {
    }
    
    void DebugMon_Handler (void)
    {
    }
    
    void PendSV_Handler (void)
    {
    }
    
    void SysTick_Handler (void)
    {
        HAL_IncTick();
    }
    
#ifdef APRINTER_ENABLE_USB
#ifdef USE_USB_FS
    void OTG_FS_IRQHandler(void)
    {
        HAL_PCD_IRQHandler(&hpcd);
    }
#endif

#ifdef USE_USB_HS
    void OTG_HS_IRQHandler(void)
    {
        HAL_PCD_IRQHandler(&hpcd);
    }
#endif
#endif
}

void platform_init (void)
{
    HAL_Init();
    init_clock();
    init_usb();
}

void platform_init_final (void)
{
    init_final_usb();
}

static void init_clock (void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    HAL_StatusTypeDef ret = HAL_OK;

    // Enable Power Control clock.
    __HAL_RCC_PWR_CLK_ENABLE();

    // Enable voltage scaling.
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Enable the HSE oscillator and activate the PLL with the HSE.
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = PLL_M_VALUE;
    RCC_OscInitStruct.PLL.PLLN = PLL_N_VALUE;
    RCC_OscInitStruct.PLL.PLLP = PLL_P_DIV_VALUE;
    RCC_OscInitStruct.PLL.PLLQ = PLL_Q_DIV_VALUE;
    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (ret != HAL_OK) while (1);
    
    // Select PLL as the system clock source and configure the HCLK, PCLK1 and PCLK2
    // clock dividers.
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = APRINTER_JOIN(RCC_HCLK_DIV, APB1_PRESC_DIV);
    RCC_ClkInitStruct.APB2CLKDivider = APRINTER_JOIN(RCC_HCLK_DIV, APB2_PRESC_DIV);
    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
    if (ret != HAL_OK) while (1);
}

static void init_usb (void)
{
#ifdef APRINTER_ENABLE_USB
    if (USBD_Init(&USBD_Device, &VCP_Desc, 0) != USBD_OK) {
        while (1);
    }
#endif
}

static void init_final_usb (void)
{
#ifdef APRINTER_ENABLE_USB
    if (USBD_Start(&USBD_Device) != USBD_OK) {
        while (1);
    }
#endif
}
