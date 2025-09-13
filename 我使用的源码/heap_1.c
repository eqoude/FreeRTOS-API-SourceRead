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


/*
 * pvPortMalloc() 最简单的实现方式。注意，此实现
 * 不允许释放已分配的内存。
 *
 * 可参见 heap_2.c、heap_3.c 和 heap_4.c 以获取其他实现，
 * 更多信息请参考 https://www.FreeRTOS.org 的内存管理页面。
 */
#include <stdlib.h>  // 包含标准库头文件

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 可以防止 task.h 重新定义
 * 所有 API 函数以使用 MPU 封装器。这种重新定义只应在
 * 从应用程序文件中包含 task.h 时进行。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
    #error 当 configSUPPORT_DYNAMIC_ALLOCATION 为 0 时，禁止使用本文件
#endif

/* 为了字节对齐堆的起始地址，可能会损失几个字节。 */
#define configADJUSTED_HEAP_SIZE    ( configTOTAL_HEAP_SIZE - portBYTE_ALIGNMENT )

/* 为堆分配内存。 */
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )

/* 应用程序编写者已经定义了用于 RTOS 堆的数组——可能是为了将其放置在特殊的段或地址中。 */
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif /* configAPPLICATION_ALLOCATED_HEAP */

/* 用于 ucHeap 数组的索引。 */
static size_t xNextFreeByte = ( size_t ) 0U;

/*-----------------------------------------------------------*/

void * pvPortMalloc( size_t xWantedSize )
{
    void * pvReturn = NULL;  // 指向分配的内存块的指针，默认返回NULL（分配失败）
    static uint8_t * pucAlignedHeap = NULL;  // 指向对齐后的堆起始地址的指针

    /* 确保内存块始终按要求对齐。 */
    #if ( portBYTE_ALIGNMENT != 1 )  // 如果需要字节对齐（非1字节对齐）
    {
        if( xWantedSize & portBYTE_ALIGNMENT_MASK )  // 检查请求的大小是否未对齐
        {
            /* 需要字节对齐。检查是否会溢出。 */
            if( ( xWantedSize + ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) ) ) > xWantedSize )
            {
                // 调整大小以满足对齐要求
                xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
            }
            else
            {
                xWantedSize = 0;  // 溢出，标记为无效大小
            }
        }
    }
    #endif /* if ( portBYTE_ALIGNMENT != 1 ) */

    vTaskSuspendAll();  // 挂起所有任务，确保内存分配的原子性
    {
        if( pucAlignedHeap == NULL )
        {
            /* 确保堆的起始地址满足正确的对齐要求。 */
            pucAlignedHeap = ( uint8_t * ) ( ( ( portPOINTER_SIZE_TYPE ) & ucHeap[ portBYTE_ALIGNMENT - 1 ] ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );
        }

        /* 检查是否有足够的空间分配，并且验证参数有效性。 */
        if( ( xWantedSize > 0 ) &&                                /* 有效的大小 */
            ( ( xNextFreeByte + xWantedSize ) < configADJUSTED_HEAP_SIZE ) &&  /* 空间足够 */
            ( ( xNextFreeByte + xWantedSize ) > xNextFreeByte ) ) /* 检查是否溢出 */
        {
            /* 返回下一个空闲字节的地址，然后将索引递增以跳过此块。 */
            pvReturn = pucAlignedHeap + xNextFreeByte;
            xNextFreeByte += xWantedSize;
        }

        traceMALLOC( pvReturn, xWantedSize );  // 跟踪内存分配（用于调试）
    }
    ( void ) xTaskResumeAll();  // 恢复所有任务

    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )  // 如果启用了分配失败钩子函数
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();  // 调用分配失败钩子
        }
    }
    #endif

    return pvReturn;  // 返回分配的内存块指针（或NULL）
}
/*-----------------------------------------------------------*/

void vPortFree( void * pv )
{
    /* 此方案不支持内存释放。可参见 heap_2.c、heap_3.c 和
     * heap_4.c 获取其他实现，更多信息请参考
     * https://www.FreeRTOS.org 的内存管理页面。 */
    ( void ) pv;  // 未使用参数，避免编译器警告

    /* 强制断言，因为调用此函数是无效的。 */
    configASSERT( pv == NULL );
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( void )
{
    /* 仅在静态内存未被清除时需要调用。 */
    xNextFreeByte = ( size_t ) 0;
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( void )
{
    return( configADJUSTED_HEAP_SIZE - xNextFreeByte );
}

/*-----------------------------------------------------------*/

/*
 * 重置本文件中的状态。此状态通常在启动时初始化。
 * 应用程序在重启调度器之前必须调用此函数。
 */
void vPortHeapResetState( void )
{
    xNextFreeByte = ( size_t ) 0U;
}
/*-----------------------------------------------------------*/
