/*
 * FreeRTOS Kernel V11.1.0
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef DEPRECATED_DEFINITIONS_H
#define DEPRECATED_DEFINITIONS_H


/* 每个FreeRTOS移植版本都有一个独特的portmacro.h头文件。
 * 最初，人们使用预处理器定义来确保预处理器能找到所使用移植版本对应的正确portmacro.h文件。
 * 该方案已被弃用，取而代之的是设置编译器的包含路径，使其能找到正确的portmacro.h文件——
 * 这样就不再需要那个常量，并且允许portmacro.h文件相对于所使用的移植版本位于任何位置。以下定义仅为向后兼容而保留在代码中。新项目不应使用它们。 */

#ifdef OPEN_WATCOM_INDUSTRIAL_PC_PORT
    #include "..\..\Source\portable\owatcom\16bitdos\pc\portmacro.h"
    typedef void ( __interrupt __far * pxISR )();
#endif

#ifdef OPEN_WATCOM_FLASH_LITE_186_PORT
    #include "..\..\Source\portable\owatcom\16bitdos\flsh186\portmacro.h"
    typedef void ( __interrupt __far * pxISR )();
#endif

#ifdef GCC_MEGA_AVR
    #include "../portable/GCC/ATMega323/portmacro.h"
#endif

#ifdef IAR_MEGA_AVR
    #include "../portable/IAR/ATMega323/portmacro.h"
#endif

#ifdef MPLAB_PIC24_PORT
    #include "../../Source/portable/MPLAB/PIC24_dsPIC/portmacro.h"
#endif

#ifdef MPLAB_DSPIC_PORT
    #include "../../Source/portable/MPLAB/PIC24_dsPIC/portmacro.h"
#endif

#ifdef MPLAB_PIC18F_PORT
    #include "../../Source/portable/MPLAB/PIC18F/portmacro.h"
#endif

#ifdef MPLAB_PIC32MX_PORT
    #include "../../Source/portable/MPLAB/PIC32MX/portmacro.h"
#endif

#ifdef _FEDPICC
    #include "libFreeRTOS/Include/portmacro.h"
#endif

#ifdef SDCC_CYGNAL
    #include "../../Source/portable/SDCC/Cygnal/portmacro.h"
#endif

#ifdef GCC_ARM7
    #include "../../Source/portable/GCC/ARM7_LPC2000/portmacro.h"
#endif

#ifdef GCC_ARM7_ECLIPSE
    #include "portmacro.h"
#endif

#ifdef ROWLEY_LPC23xx
    #include "../../Source/portable/GCC/ARM7_LPC23xx/portmacro.h"
#endif

#ifdef IAR_MSP430
    #include "..\..\Source\portable\IAR\MSP430\portmacro.h"
#endif

#ifdef GCC_MSP430
    #include "../../Source/portable/GCC/MSP430F449/portmacro.h"
#endif

#ifdef ROWLEY_MSP430
    #include "../../Source/portable/Rowley/MSP430F449/portmacro.h"
#endif

#ifdef ARM7_LPC21xx_KEIL_RVDS
    #include "..\..\Source\portable\RVDS\ARM7_LPC21xx\portmacro.h"
#endif

#ifdef SAM7_GCC
    #include "../../Source/portable/GCC/ARM7_AT91SAM7S/portmacro.h"
#endif

#ifdef SAM7_IAR
    #include "..\..\Source\portable\IAR\AtmelSAM7S64\portmacro.h"
#endif

#ifdef SAM9XE_IAR
    #include "..\..\Source\portable\IAR\AtmelSAM9XE\portmacro.h"
#endif

#ifdef LPC2000_IAR
    #include "..\..\Source\portable\IAR\LPC2000\portmacro.h"
#endif

#ifdef STR71X_IAR
    #include "..\..\Source\portable\IAR\STR71x\portmacro.h"
#endif

#ifdef STR75X_IAR
    #include "..\..\Source\portable\IAR\STR75x\portmacro.h"
#endif

#ifdef STR75X_GCC
    #include "..\..\Source\portable\GCC\STR75x\portmacro.h"
#endif

#ifdef STR91X_IAR
    #include "..\..\Source\portable\IAR\STR91x\portmacro.h"
#endif

#ifdef GCC_H8S
    #include "../../Source/portable/GCC/H8S2329/portmacro.h"
#endif

#ifdef GCC_AT91FR40008
    #include "../../Source/portable/GCC/ARM7_AT91FR40008/portmacro.h"
#endif

#ifdef RVDS_ARMCM3_LM3S102
    #include "../../Source/portable/RVDS/ARM_CM3/portmacro.h"
#endif

#ifdef GCC_ARMCM3_LM3S102
    #include "../../Source/portable/GCC/ARM_CM3/portmacro.h"
#endif

#ifdef GCC_ARMCM3
    #include "../../Source/portable/GCC/ARM_CM3/portmacro.h"
#endif

#ifdef IAR_ARM_CM3
    #include "../../Source/portable/IAR/ARM_CM3/portmacro.h"
#endif

#ifdef IAR_ARMCM3_LM
    #include "../../Source/portable/IAR/ARM_CM3/portmacro.h"
#endif

#ifdef HCS12_CODE_WARRIOR
    #include "../../Source/portable/CodeWarrior/HCS12/portmacro.h"
#endif

#ifdef MICROBLAZE_GCC
    #include "../../Source/portable/GCC/MicroBlaze/portmacro.h"
#endif

#ifdef TERN_EE
    #include "..\..\Source\portable\Paradigm\Tern_EE\small\portmacro.h"
#endif

#ifdef GCC_HCS12
    #include "../../Source/portable/GCC/HCS12/portmacro.h"
#endif

#ifdef GCC_MCF5235
    #include "../../Source/portable/GCC/MCF5235/portmacro.h"
#endif

#ifdef COLDFIRE_V2_GCC
    #include "../../../Source/portable/GCC/ColdFire_V2/portmacro.h"
#endif

#ifdef COLDFIRE_V2_CODEWARRIOR
    #include "../../Source/portable/CodeWarrior/ColdFire_V2/portmacro.h"
#endif

#ifdef GCC_PPC405
    #include "../../Source/portable/GCC/PPC405_Xilinx/portmacro.h"
#endif

#ifdef GCC_PPC440
    #include "../../Source/portable/GCC/PPC440_Xilinx/portmacro.h"
#endif

#ifdef _16FX_SOFTUNE
    #include "..\..\Source\portable\Softune\MB96340\portmacro.h"
#endif

#ifdef BCC_INDUSTRIAL_PC_PORT

/* 使用Borland编译器时，必须用短文件名替代常规的FreeRTOSConfig.h。 */
    #include "frconfig.h"
    #include "..\portable\BCC\16BitDOS\PC\prtmacro.h"
    typedef void ( __interrupt __far * pxISR )();
#endif

#ifdef BCC_FLASH_LITE_186_PORT

/* 使用Borland编译器时，必须用短文件名替代常规的FreeRTOSConfig.h。 */
    #include "frconfig.h"
    #include "..\portable\BCC\16BitDOS\flsh186\prtmacro.h"
    typedef void ( __interrupt __far * pxISR )();
#endif

#ifdef __GNUC__
    #ifdef __AVR32_AVR32A__
        #include "portmacro.h"
    #endif
#endif

#ifdef __ICCAVR32__
    #ifdef __CORE__
        #if __CORE__ == __AVR32A__
            #include "portmacro.h"
        #endif
    #endif
#endif

#ifdef __91467D
    #include "portmacro.h"
#endif

#ifdef __96340
    #include "portmacro.h"
#endif


#ifdef __IAR_V850ES_Fx3__
    #include "../../Source/portable/IAR/V850ES/portmacro.h"
#endif

#ifdef __IAR_V850ES_Jx3__
    #include "../../Source/portable/IAR/V850ES/portmacro.h"
#endif

#ifdef __IAR_V850ES_Jx3_L__
    #include "../../Source/portable/IAR/V850ES/portmacro.h"
#endif

#ifdef __IAR_V850ES_Jx2__
    #include "../../Source/portable/IAR/V850ES/portmacro.h"
#endif

#ifdef __IAR_V850ES_Hx2__
    #include "../../Source/portable/IAR/V850ES/portmacro.h"
#endif

#ifdef __IAR_78K0R_Kx3__
    #include "../../Source/portable/IAR/78K0R/portmacro.h"
#endif

#ifdef __IAR_78K0R_Kx3L__
    #include "../../Source/portable/IAR/78K0R/portmacro.h"
#endif

#endif /* DEPRECATED_DEFINITIONS_H */
