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
 * pvPortMalloc() 和 vPortFree() 的示例实现，支持释放已分配的块，
 * 但不将相邻的空闲块合并为一个更大的块（因此会产生内存碎片）。
 * 若需要支持将相邻块合并的等效实现，请参见 heap_4.c。
 *
 * 可参见 heap_1.c、heap_3.c 和 heap_4.c 获取其他实现，更多信息请参考
 * https://www.FreeRTOS.org 的内存管理页面。
 */
#include <stdlib.h>
#include <string.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
 * all the API functions to use the MPU wrappers.  That should only be done when
 * task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
    #error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

#ifndef configHEAP_CLEAR_MEMORY_ON_FREE
    #define configHEAP_CLEAR_MEMORY_ON_FREE    0
#endif

/* 为了字节对齐堆的起始地址，可能会损失几个字节。 */
#define configADJUSTED_HEAP_SIZE    ( configTOTAL_HEAP_SIZE - portBYTE_ALIGNMENT )

/* 假设是8位字节！ */
#define heapBITS_PER_BYTE           ( ( size_t ) 8 )

/* 适合 size_t 类型的最大值。 */
#define heapSIZE_MAX                ( ~( ( size_t ) 0 ) )

/* 检查 a 和 b 相乘是否会导致溢出。 */
#define heapMULTIPLY_WILL_OVERFLOW( a, b )    ( ( ( a ) > 0 ) && ( ( b ) > ( heapSIZE_MAX / ( a ) ) ) )

/* 检查 a 和 b 相加是否会导致溢出。 */
#define heapADD_WILL_OVERFLOW( a, b )         ( ( a ) > ( heapSIZE_MAX - ( b ) ) )

/* BlockLink_t 结构体的 xBlockSize 成员的最高位用于跟踪块的分配状态。
 * 当 BlockLink_t 结构体的 xBlockSize 成员的最高位被设置时，该块属于应用程序（已分配）。
 * 当该位未设置时，该块仍属于空闲堆空间。 */
#define heapBLOCK_ALLOCATED_BITMASK    ( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 ) )
#define heapBLOCK_SIZE_IS_VALID( xBlockSize )    ( ( ( xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) == 0 )
#define heapBLOCK_IS_ALLOCATED( pxBlock )        ( ( ( pxBlock->xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) != 0 )
#define heapALLOCATE_BLOCK( pxBlock )            ( ( pxBlock->xBlockSize ) |= heapBLOCK_ALLOCATED_BITMASK )
#define heapFREE_BLOCK( pxBlock )                ( ( pxBlock->xBlockSize ) &= ~heapBLOCK_ALLOCATED_BITMASK )

/*-----------------------------------------------------------*/

/* 为堆分配内存。 */
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )

/* 应用程序编写者已经定义了用于 RTOS 堆的数组——可能是为了将其放置在特殊的段或地址中。 */
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    PRIVILEGED_DATA static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif /* configAPPLICATION_ALLOCATED_HEAP */


/* 定义链表结构。用于按大小顺序链接空闲块。 */
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /*<< 链表中的下一个空闲块。 */
    size_t xBlockSize;                     /*<< 空闲块的大小。 */
} BlockLink_t;

/* 计算并确保 BlockLink_t 结构体的大小满足平台的字节对齐要求 */
static const size_t xHeapStructSize = ( ( sizeof( BlockLink_t ) + ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK ) );
#define heapMINIMUM_BLOCK_SIZE    ( ( size_t ) ( xHeapStructSize * 2 ) )

/* 创建两个链表节点，用于标记链表的开头和结尾。 */
PRIVILEGED_DATA static BlockLink_t xStart, xEnd;

/* 跟踪剩余的空闲字节数，但不反映内存碎片情况。 */
PRIVILEGED_DATA static size_t xFreeBytesRemaining = configADJUSTED_HEAP_SIZE;

/* 指示堆是否已初始化。 */
PRIVILEGED_DATA static BaseType_t xHeapHasBeenInitialised = pdFALSE;

/*-----------------------------------------------------------*/

/*
 * Initialises the heap structures before their first use.
 */
static void prvHeapInit( void ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

/* 静态函数被定义为宏，以最小化函数调用深度。 */

/*
 * 将块插入空闲块链表中——链表按块大小排序。
 * 小 block 位于链表开头，大 block 位于链表末尾。
 */
#define prvInsertBlockIntoFreeList( pxBlockToInsert )                                                                               \
    {                                                                                                                               \
        BlockLink_t * pxIterator;                                                                                                   \
        size_t xBlockSize;                                                                                                          \
                                                                                                                                    \
        xBlockSize = pxBlockToInsert->xBlockSize;                                                                                   \
                                                                                                                                    \
        /* 遍历链表，直到找到一个大小大于待插入块的块 */                                                                        \
        for( pxIterator = &xStart; pxIterator->pxNextFreeBlock->xBlockSize < xBlockSize; pxIterator = pxIterator->pxNextFreeBlock ) \
        {                                                                                                                           \
            /* 这里无需执行任何操作，只需迭代到正确位置即可。 */                                                                   \
        }                                                                                                                           \
                                                                                                                                    \
        /* 更新链表，将待插入块放入正确位置。 */                                                                                    \
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;                                                             \
        pxIterator->pxNextFreeBlock = pxBlockToInsert;                                                                              \
    }
/*-----------------------------------------------------------*/

void * pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t * pxBlock;          // 指向当前遍历的空闲块
    BlockLink_t * pxPreviousBlock;  // 指向当前遍历块的前一个空闲块
    BlockLink_t * pxNewBlockLink;   // 指向拆分后新生成的空闲块
    void * pvReturn = NULL;         // 分配成功的内存地址（返回给用户）
    size_t xAdditionalRequiredSize; // 满足对齐所需的额外字节数

    if( xWantedSize > 0 )  // 检查请求的大小是否有效（非零）
    {
        /* 需增加请求大小，以容纳 BlockLink_t 结构体（块头）和用户请求的字节数 */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )  // 检查加法是否溢出
        {
            xWantedSize += xHeapStructSize;  // 加上块头大小

            /* 确保块大小始终满足平台要求的字节对齐 */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )  // 检查是否未对齐
            {
                /* 需要字节对齐，计算所需的额外字节数 */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )  // 检查溢出
                {
                    xWantedSize += xAdditionalRequiredSize;  // 调整为对齐大小
                }
                else
                {
                    xWantedSize = 0;  // 溢出，标记为无效大小
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记（无实际逻辑）
            }
        }
        else
        {
            xWantedSize = 0;  // 溢出，标记为无效大小
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记
    }

    vTaskSuspendAll();  // 挂起所有任务，确保内存分配的原子性
    {
        /* 若首次调用 malloc，需初始化堆结构（构建空闲链表） */
        if( xHeapHasBeenInitialised == pdFALSE )
        {
            prvHeapInit();
            xHeapHasBeenInitialised = pdTRUE;
        }

        /* 检查待分配的块大小是否有效：块大小的最高位不能被设置（最高位是分配状态标记） */
        if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
        {
            /* 检查大小有效且堆中有足够空闲空间 */
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* 空闲链表按块大小升序排列，从链表头开始遍历，找到第一个足够大的块 */
                pxPreviousBlock = &xStart;  // 从哨兵节点 xStart 开始
                pxBlock = xStart.pxNextFreeBlock;  // 当前遍历的第一个空闲块

                // 遍历链表：直到找到大小≥请求值的块，或到达链表尾（xEnd）
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
                {
                    pxPreviousBlock = pxBlock;  // 记录前一个块
                    pxBlock = pxBlock->pxNextFreeBlock;  // 移动到下一个块
                }

                /* 若未到达链表尾（xEnd），说明找到了足够大的空闲块 */
                if( pxBlock != &xEnd )
                {
                    /* 返回给用户的地址 = 空闲块地址 + 块头大小（跳过 BlockLink_t 结构体） */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

                    /* 将当前块从空闲链表中移除（分配给用户） */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* 若当前块远大于请求大小，将其拆分为“用户分配块”和“新空闲块” */
                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                    {
                        /* 计算新空闲块的地址：当前块地址 + 用户请求的总大小（含块头） */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

                        /* 计算拆分后两个块的大小 */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;  // 新空闲块大小
                        pxBlock->xBlockSize = xWantedSize;  // 用户分配块大小（含块头）

                        /* 将新空闲块按大小顺序插入空闲链表 */
                        prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
                    }

                    xFreeBytesRemaining -= pxBlock->xBlockSize;  // 更新剩余空闲字节数

                    /* 标记当前块为“已分配”（设置块大小的最高位），并清空下一个块指针 */
                    heapALLOCATE_BLOCK( pxBlock );
                    pxBlock->pxNextFreeBlock = NULL;
                }
            }
        }

        traceMALLOC( pvReturn, xWantedSize );  // 调试跟踪：记录分配的地址和大小
    }
    ( void ) xTaskResumeAll();  // 恢复所有任务调度

    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )  // 若启用分配失败钩子函数
    {
        if( pvReturn == NULL )  // 分配失败时调用钩子
        {
            vApplicationMallocFailedHook();
        }
    }
    #endif

    return pvReturn;  // 返回分配的内存地址（或 NULL 表示失败）
}
/*-----------------------------------------------------------*/

void vPortFree( void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;  // 将用户传入的指针转换为字节指针
    BlockLink_t * pxLink;              // 用于指向块头的指针

    if( pv != NULL )  // 检查待释放的指针是否有效（非NULL）
    {
        /* 被释放的内存前会有一个 BlockLink_t 结构体（块头） */
        puc -= xHeapStructSize;  // 从用户指针回退到块头地址

        /* 这种强制转换是为了避免某些编译器byte alignment warnings. */
        pxLink = ( void * ) puc;  // 将块头地址转换为 BlockLink_t* 类型

        // 断言：确保待释放的块确实处于“已分配”状态
        configASSERT( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 );
        // 断言：确保已分配块的下一个指针为NULL（已分配块不参与链表）
        configASSERT( pxLink->pxNextFreeBlock == NULL );

        // 再次检查块是否处于已分配状态（双重保险，避免断言被关闭时出错）
        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            // 检查已分配块的下一个指针是否为NULL（确保块未被破坏）
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* 块即将归还给堆 - 标记为“未分配”状态 */
                heapFREE_BLOCK( pxLink );  // 清除最高位（标记为空闲）

                #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )  // 若启用“释放时清零”配置
                {
                    // 将用户数据区清零（从块头结束地址开始，长度为数据区大小）
                    ( void ) memset( puc + xHeapStructSize, 0, pxLink->xBlockSize - xHeapStructSize );
                }
                #endif

                vTaskSuspendAll();  // 挂起所有任务，确保释放操作的原子性
                {
                    /* 将块添加到空闲块链表中 */
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
                    xFreeBytesRemaining += pxLink->xBlockSize;  // 更新剩余空闲字节数
                    traceFREE( pv, pxLink->xBlockSize );  // 调试跟踪：记录释放的地址和大小
                }
                ( void ) xTaskResumeAll();  // 恢复任务调度
            }
        }
    }
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( void )
{
    return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( void )
{
    /* This just exists to keep the linker quiet. */
}
/*-----------------------------------------------------------*/

void * pvPortCalloc( size_t xNum,
                     size_t xSize )
{
    void * pv = NULL;

    // 检查 xNum * xSize 是否会溢出
    if( heapMULTIPLY_WILL_OVERFLOW( xNum, xSize ) == 0 )
    {
        // 分配 xNum * xSize 字节的内存
        pv = pvPortMalloc( xNum * xSize );

        // 若分配成功，将内存区域清零
        if( pv != NULL )
        {
            ( void ) memset( pv, 0, xNum * xSize );
        }
    }

    return pv;  // 返回分配并清零的内存地址（或 NULL 表示失败）
}
/*-----------------------------------------------------------*/

static void prvHeapInit( void ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxFirstFreeBlock;  // 指向第一个空闲块的指针
    uint8_t * pucAlignedHeap;        // 指向对齐后的堆起始地址

    /* 确保堆的起始地址满足正确的字节对齐要求。 */
    pucAlignedHeap = ( uint8_t * ) ( ( ( portPOINTER_SIZE_TYPE ) & ucHeap[ portBYTE_ALIGNMENT - 1 ] ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );

    /* xStart 用于存储空闲块链表中第一个节点的指针。
     * 使用 void 强制转换是为了避免编译器警告。 */
    xStart.pxNextFreeBlock = ( void * ) pucAlignedHeap;
    xStart.xBlockSize = ( size_t ) 0;

    /* xEnd 用于标记空闲块链表的末尾。 */
    xEnd.xBlockSize = configADJUSTED_HEAP_SIZE;
    xEnd.pxNextFreeBlock = NULL;

    /* 初始化时，堆中只有一个空闲块，其大小等于整个堆的可用空间。 */
    pxFirstFreeBlock = ( BlockLink_t * ) pucAlignedHeap;
    pxFirstFreeBlock->xBlockSize = configADJUSTED_HEAP_SIZE;
    pxFirstFreeBlock->pxNextFreeBlock = &xEnd;
}
/*-----------------------------------------------------------*/

/*
 * 重置此文件中的状态。此状态通常在启动时初始化。
 * 应用程序在重启调度器之前必须调用此函数。
 */
void vPortHeapResetState( void )
{
    xFreeBytesRemaining = configADJUSTED_HEAP_SIZE;  // 重置剩余空闲字节数为堆总大小

    xHeapHasBeenInitialised = pdFALSE;  // 标记堆未初始化
}
/*-----------------------------------------------------------*/
