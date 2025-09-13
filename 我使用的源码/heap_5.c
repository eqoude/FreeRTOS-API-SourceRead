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
 * pvPortMalloc() 的示例实现，支持将堆定义在多个非连续的内存块上，
 * 并且在内存块被释放时会将相邻的块合并（ coalescences ）。
 *
 * 可参考 heap_1.c、heap_2.c、heap_3.c 和 heap_4.c 以获取其他实现版本，
 * 更多信息请查阅 https://www.FreeRTOS.org 的内存管理相关页面。
 *
 * 使用说明：
 *
 * 1. 调用 pvPortMalloc() 之前，***必须***先调用 vPortDefineHeapRegions()。
 * 2. 若创建任何任务对象（任务、队列、事件组等），都会触发 pvPortMalloc() 调用，
 *    因此 vPortDefineHeapRegions() ***必须***在所有其他对象定义之前调用。
 *
 * vPortDefineHeapRegions() 接收一个参数，该参数是 HeapRegion_t 结构体数组。
 * HeapRegion_t 在 portable.h 中定义如下：
 *
 * typedef struct HeapRegion
 * {
 *  uint8_t *pucStartAddress; // 属于堆的某块内存的起始地址
 *  size_t xSizeInBytes;      // 该块内存的大小（字节数）
 * } HeapRegion_t;
 *
 * 数组需用“空地址+零大小”的区域定义作为结束标志，且数组中定义的内存区域
 * ***必须***按地址从低到高的顺序排列。以下是该函数的有效使用示例：
 *
 * HeapRegion_t xHeapRegions[] =
 * {
 *  { ( uint8_t * ) 0x80000000UL, 0x10000 }, // 定义一块起始地址为 0x80000000、大小为 0x10000 字节的内存
 *  { ( uint8_t * ) 0x90000000UL, 0xa0000 }, // 定义一块起始地址为 0x90000000、大小为 0xa0000 字节的内存
 *  { NULL, 0 }                               // 数组结束标志
 * };
 *
 * vPortDefineHeapRegions( xHeapRegions ); // 将数组传入 vPortDefineHeapRegions()
 *
 * 注意：0x80000000 是较低地址，因此在数组中排在前面。
 *
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

/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE    ( ( size_t ) ( xHeapStructSize << 1 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE         ( ( size_t ) 8 )

/* Max value that fits in a size_t type. */
#define heapSIZE_MAX              ( ~( ( size_t ) 0 ) )

/* Check if multiplying a and b will result in overflow. */
#define heapMULTIPLY_WILL_OVERFLOW( a, b )     ( ( ( a ) > 0 ) && ( ( b ) > ( heapSIZE_MAX / ( a ) ) ) )

/* Check if adding a and b will result in overflow. */
#define heapADD_WILL_OVERFLOW( a, b )          ( ( a ) > ( heapSIZE_MAX - ( b ) ) )

/* Check if the subtraction operation ( a - b ) will result in underflow. */
#define heapSUBTRACT_WILL_UNDERFLOW( a, b )    ( ( a ) < ( b ) )

/* MSB of the xBlockSize member of an BlockLink_t structure is used to track
 * the allocation status of a block.  When MSB of the xBlockSize member of
 * an BlockLink_t structure is set then the block belongs to the application.
 * When the bit is free the block is still part of the free heap space. */
#define heapBLOCK_ALLOCATED_BITMASK    ( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 ) )
#define heapBLOCK_SIZE_IS_VALID( xBlockSize )    ( ( ( xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) == 0 )
#define heapBLOCK_IS_ALLOCATED( pxBlock )        ( ( ( pxBlock->xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) != 0 )
#define heapALLOCATE_BLOCK( pxBlock )            ( ( pxBlock->xBlockSize ) |= heapBLOCK_ALLOCATED_BITMASK )
#define heapFREE_BLOCK( pxBlock )                ( ( pxBlock->xBlockSize ) &= ~heapBLOCK_ALLOCATED_BITMASK )

/* Setting configENABLE_HEAP_PROTECTOR to 1 enables heap block pointers
 * protection using an application supplied canary value to catch heap
 * corruption should a heap buffer overflow occur.
 */
#if ( configENABLE_HEAP_PROTECTOR == 1 )

/* Macro to load/store BlockLink_t pointers to memory. By XORing the
 * pointers with a random canary value, heap overflows will result
 * in randomly unpredictable pointer values which will be caught by
 * heapVALIDATE_BLOCK_POINTER assert. */
    #define heapPROTECT_BLOCK_POINTER( pxBlock )    ( ( BlockLink_t * ) ( ( ( portPOINTER_SIZE_TYPE ) ( pxBlock ) ) ^ xHeapCanary ) )

/* Assert that a heap block pointer is within the heap bounds. */
    #define heapVALIDATE_BLOCK_POINTER( pxBlock )                       \
    configASSERT( ( pucHeapHighAddress != NULL ) &&                     \
                  ( pucHeapLowAddress != NULL ) &&                      \
                  ( ( uint8_t * ) ( pxBlock ) >= pucHeapLowAddress ) && \
                  ( ( uint8_t * ) ( pxBlock ) < pucHeapHighAddress ) )

#else /* if ( configENABLE_HEAP_PROTECTOR == 1 ) */

    #define heapPROTECT_BLOCK_POINTER( pxBlock )    ( pxBlock )

    #define heapVALIDATE_BLOCK_POINTER( pxBlock )

#endif /* configENABLE_HEAP_PROTECTOR */

/*-----------------------------------------------------------*/

/* Define the linked list structure.  This is used to link free blocks in order
 * of their memory address. */
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /**< The next free block in the list. */
    size_t xBlockSize;                     /**< The size of the free block. */
} BlockLink_t;

/*-----------------------------------------------------------*/

/*
 * 将待释放的内存块插入到空闲内存块链表的正确位置。
 * 若待释放块与它前面的块和/或后面的块在内存地址上相邻，
 * 则会将这些块合并为一个连续的内存块。
 */
static void prvInsertBlockIntoFreeList( BlockLink_t * pxBlockToInsert ) PRIVILEGED_FUNCTION;

// 定义堆区域（用于 heap_5.c，支持多非连续内存块）
void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions ) PRIVILEGED_FUNCTION;

#if ( configENABLE_HEAP_PROTECTOR == 1 )

/**
 * @brief 应用程序提供的函数，用于获取随机值作为堆保护的金丝雀值。
 *
 * @param pxHeapCanary [out] 输出参数，用于返回金丝雀值。
 */
    extern void vApplicationGetRandomHeapCanary( portPOINTER_SIZE_TYPE * pxHeapCanary );
#endif /* configENABLE_HEAP_PROTECTOR */

/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
 * block must by correctly byte aligned. */
static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );

/* Create a couple of list links to mark the start and end of the list. */
PRIVILEGED_DATA static BlockLink_t xStart;
PRIVILEGED_DATA static BlockLink_t * pxEnd = NULL;

/* Keeps track of the number of calls to allocate and free memory as well as the
 * number of free bytes remaining, but says nothing about fragmentation. */
PRIVILEGED_DATA static size_t xFreeBytesRemaining = ( size_t ) 0U;
PRIVILEGED_DATA static size_t xMinimumEverFreeBytesRemaining = ( size_t ) 0U;
PRIVILEGED_DATA static size_t xNumberOfSuccessfulAllocations = ( size_t ) 0U;
PRIVILEGED_DATA static size_t xNumberOfSuccessfulFrees = ( size_t ) 0U;

#if ( configENABLE_HEAP_PROTECTOR == 1 )

/* Canary value for protecting internal heap pointers. */
    PRIVILEGED_DATA static portPOINTER_SIZE_TYPE xHeapCanary;

/* Highest and lowest heap addresses used for heap block bounds checking. */
    PRIVILEGED_DATA static uint8_t * pucHeapHighAddress = NULL;
    PRIVILEGED_DATA static uint8_t * pucHeapLowAddress = NULL;

#endif /* configENABLE_HEAP_PROTECTOR */

/*-----------------------------------------------------------*/

void * pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t * pxBlock;          // 指向当前遍历的空闲块
    BlockLink_t * pxPreviousBlock;  // 指向当前块的前一个空闲块
    BlockLink_t * pxNewBlockLink;   // 指向拆分后新生成的空闲块
    void * pvReturn = NULL;         // 最终返回给用户的内存地址
    size_t xAdditionalRequiredSize; // 字节对齐所需的补充大小

    /* 首次调用 pvPortMalloc() 之前，堆必须已完成初始化。 */
    configASSERT( pxEnd );  // 断言堆已初始化（pxEnd 是堆尾哨兵，初始化后非NULL）

    // 检查请求的内存大小是否有效（非0）
    if( xWantedSize > 0 )
    {
        /* 需增加请求大小，使其除了包含用户所需字节数外，还能容纳一个 BlockLink_t 结构体（块头） */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )  // 检查“加块头”是否溢出
        {
            xWantedSize += xHeapStructSize;  // 加上块头大小，得到总需求大小

            /* 确保块始终满足所需的字节对齐要求（避免CPU访问未对齐地址报错） */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )  // 检查是否未对齐
            {
                /* 需要补充字节以实现对齐 */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                // 检查“加对齐补充”是否溢出
                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )
                {
                    xWantedSize += xAdditionalRequiredSize;  // 加上补充大小，完成对齐
                }
                else
                {
                    xWantedSize = 0;  // 溢出则标记为无效大小，分配失败
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
            }
        }
        else
        {
            xWantedSize = 0;  // 溢出则标记为无效大小，分配失败
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    // 挂起所有任务，确保内存分配操作的原子性（避免多任务竞争堆链表）
    vTaskSuspendAll();
    {
        /* 检查待分配的块大小是否有效：不能占用 BlockLink_t 结构体 xBlockSize 成员的最高位。
         * 该最高位用于标记块的归属（应用程序/内核），因此必须为0（空闲块标记）。 */
        if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
        {
            // 检查大小有效且剩余空闲空间足够分配
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* 从链表起始位置（低地址）遍历，找到第一个大小足够的空闲块（首次适配算法） */
                pxPreviousBlock = &xStart;  // xStart 是堆头哨兵
                pxBlock = heapPROTECT_BLOCK_POINTER( xStart.pxNextFreeBlock );  // 解密指针（若启用堆保护）
                heapVALIDATE_BLOCK_POINTER( pxBlock );  // 验证指针是否在堆合法范围内

                // 遍历条件：当前块大小不足，且未到链表末尾（尾哨兵）
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != heapPROTECT_BLOCK_POINTER( NULL ) ) )
                {
                    pxPreviousBlock = pxBlock;
                    pxBlock = heapPROTECT_BLOCK_POINTER( pxBlock->pxNextFreeBlock );  // 解密下一个块指针
                    heapVALIDATE_BLOCK_POINTER( pxBlock );
                }

                /* 若未遍历到尾哨兵（pxEnd），说明找到大小足够的空闲块 */
                if( pxBlock != pxEnd )
                {
                    /* 返回用户可用地址：跳过块头（BlockLink_t 结构体），指向数据区起始位置 */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxPreviousBlock->pxNextFreeBlock ) ) + xHeapStructSize );
                    heapVALIDATE_BLOCK_POINTER( pvReturn );  // 验证返回地址合法性

                    /* 当前块已分配给用户，需从空闲链表中移除 */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* 若当前块远大于需求大小，将其拆分为“用户分配块”和“新空闲块” */
                    configASSERT( heapSUBTRACT_WILL_UNDERFLOW( pxBlock->xBlockSize, xWantedSize ) == 0 );  // 断言拆分无下溢

                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )  // 剩余空间足够生成新块
                    {
                        /* 计算新空闲块的地址：当前块地址 + 用户总需求大小（含块头） */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                        configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );  // 断言新块地址对齐

                        /* 计算拆分后两个块的大小 */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;  // 新空闲块大小
                        pxBlock->xBlockSize = xWantedSize;  // 用户分配块大小（含块头）

                        /* 将新空闲块插入空闲链表（保持地址升序） */
                        pxNewBlockLink->pxNextFreeBlock = pxPreviousBlock->pxNextFreeBlock;
                        pxPreviousBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxNewBlockLink );  // 加密指针（若启用堆保护）
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    // 更新剩余空闲字节数（减去分配块的大小）
                    xFreeBytesRemaining -= pxBlock->xBlockSize;

                    // 更新历史最小剩余空闲字节数（记录堆曾达到的最小空闲空间）
                    if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                    {
                        xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 标记当前块为已分配：设置 xBlockSize 最高位，且已分配块无“下一个块”指针 */
                    heapALLOCATE_BLOCK( pxBlock );
                    pxBlock->pxNextFreeBlock = NULL;
                    xNumberOfSuccessfulAllocations++;  // 成功分配次数统计+1
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  // 未找到足够大的块，分配失败
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 大小无效或空闲空间不足，分配失败
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  // 块大小无效（占用最高位），分配失败
        }

        traceMALLOC( pvReturn, xWantedSize );  // 调试跟踪：记录分配的地址和大小（供工具链调试用）
    }
    ( void ) xTaskResumeAll();  // 恢复任务调度，结束原子操作

    // 若启用“分配失败钩子函数”，且分配失败（pvReturn为NULL），则调用钩子函数
    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();  // 应用程序可在该钩子中处理分配失败（如打印日志、复位）
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* if ( configUSE_MALLOC_FAILED_HOOK == 1 ) */

    // 断言返回的用户地址满足字节对齐要求（确保用户使用时无对齐问题）
    configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
    return pvReturn;  // 返回用户数据区地址（NULL表示分配失败）
}
/*-----------------------------------------------------------*/

void vPortFree( void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;  // 将用户传入的内存指针转换为字节指针（方便地址偏移）
    BlockLink_t * pxLink;              // 指向待释放块的块头（BlockLink_t 结构体，存储堆管理元信息）

    // 检查待释放的指针是否有效（非 NULL，避免释放空指针）
    if( pv != NULL )
    {
        /* 被释放的内存块前方，必然存在一个 BlockLink_t 结构体（即块头，记录块大小、链表指针等） */
        puc -= xHeapStructSize;  // 从用户数据区指针回退 xHeapStructSize 字节，定位到块头起始地址

        /* 强制转换是为了避免编译器因类型不匹配发出警告 */
        pxLink = ( void * ) puc;  // 将块头地址转换为 BlockLink_t* 类型，用于操作堆管理元信息

        // 验证块头指针是否在堆的合法地址范围内（防止释放堆外非法地址）
        heapVALIDATE_BLOCK_POINTER( pxLink );
        // 断言：待释放的块必须处于“已分配”状态（避免重复释放或释放空闲块）
        configASSERT( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 );
        // 断言：已分配块的“下一个空闲块指针”必须为 NULL（已分配块不参与空闲链表，此指针无效）
        configASSERT( pxLink->pxNextFreeBlock == NULL );

        // 再次检查块是否处于已分配状态（双重保险，即使断言被关闭也能避免错误）
        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            // 确认已分配块的“下一个指针”为 NULL（防止块结构被篡改，如内存越界导致指针异常）
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* 该块即将归还给堆 —— 标记为“空闲”状态 */
                heapFREE_BLOCK( pxLink );  // 宏操作：清除块头 xBlockSize 的最高位（释放状态标记位）

                #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )  // 若启用“释放时清零内存”配置（增强数据安全性）
                {
                    /* 检查“块总大小 - 块头大小”是否会下溢（防止块大小被篡改导致非法内存操作） */
                    if( heapSUBTRACT_WILL_UNDERFLOW( pxLink->xBlockSize, xHeapStructSize ) == 0 )
                    {
                        // 将用户数据区清零：从块头结束地址（puc + xHeapStructSize）开始，长度为“块总大小 - 块头大小”
                        ( void ) memset( puc + xHeapStructSize, 0, pxLink->xBlockSize - xHeapStructSize );
                    }
                }
                #endif

                // 挂起所有任务，确保释放操作的原子性（避免多任务同时操作空闲链表导致指针混乱）
                vTaskSuspendAll();
                {
                    /* 将空闲块加入空闲链表（并合并相邻空闲块，减少内存碎片） */
                    xFreeBytesRemaining += pxLink->xBlockSize;  // 更新堆剩余空闲字节数（加上当前释放块的大小）
                    traceFREE( pv, pxLink->xBlockSize );        // 调试跟踪：记录释放的用户指针和块总大小（供调试工具使用）
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );  // 核心操作：插入空闲链表并合并相邻块
                    xNumberOfSuccessfulFrees++;                 // 成功释放次数统计+1（用于堆状态监控）
                }
                ( void ) xTaskResumeAll();  // 恢复任务调度，结束原子操作
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际业务逻辑，仅用于测试工具统计代码覆盖率）
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize( void )
{
    return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize( void )
{
    return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

void * pvPortCalloc( size_t xNum,
                     size_t xSize )
{
    void * pv = NULL;

    if( heapMULTIPLY_WILL_OVERFLOW( xNum, xSize ) == 0 )
    {
        pv = pvPortMalloc( xNum * xSize );

        if( pv != NULL )
        {
            ( void ) memset( pv, 0, xNum * xSize );
        }
    }

    return pv;
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( BlockLink_t * pxBlockToInsert ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxIterator;  // 遍历空闲链表的迭代器指针（用于定位插入位置）
    uint8_t * puc;             // 字节类型指针（用于计算内存地址，判断块是否连续）

    /* 遍历空闲链表，直到找到地址高于待插入块的空闲块（确定待插入块的前驱位置） */
    for( pxIterator = &xStart;  // 从链表头哨兵（xStart）开始遍历
         heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) < pxBlockToInsert;  // 下一个块地址 < 待插入块地址时继续遍历
         pxIterator = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )  // 迭代器移动到下一个块
    {
        /* 无需额外操作，仅通过循环定位到正确的插入位置 */
    }

    // 若迭代器不指向头哨兵（xStart），验证迭代器指向的块地址是否在堆合法范围内
    if( pxIterator != &xStart )
    {
        heapVALIDATE_BLOCK_POINTER( pxIterator );
    }

    /* 待插入块与它的前驱块（迭代器指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxIterator;  // 将前驱块指针转换为字节指针，用于地址计算

    // 前驱块的结束地址（前驱块地址 + 前驱块总大小）是否等于待插入块的起始地址
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        // 合并前驱块与待插入块：前驱块总大小 += 待插入块总大小
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        // 待插入块指针指向合并后的块（后续操作基于合并后的大块，而非原待插入块）
        pxBlockToInsert = pxIterator;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际业务逻辑，仅用于测试工具统计）
    }

    /* 待插入块（可能已合并前驱块）与它的后继块（迭代器原本的下一个块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxBlockToInsert;  // 将待插入块（或合并后的块）指针转换为字节指针

    // 待插入块的结束地址（待插入块地址 + 待插入块总大小）是否等于后继块的起始地址
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        // 确保后继块不是尾哨兵（pxEnd 无实际内存，无需合并）
        if( heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) != pxEnd )
        {
            /* 将待插入块与后继块合并为一个大块 */
            // 待插入块总大小 += 后继块总大小
            pxBlockToInsert->xBlockSize += heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->xBlockSize;
            // 待插入块的下一个指针指向后继块的下一个指针（跳过后继块，完成合并）
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->pxNextFreeBlock;
        }
        else
        {
            // 若后继块是尾哨兵，待插入块的下一个指针直接指向尾哨兵
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );
        }
    }
    else
    {
        // 无法与后继块合并，待插入块的下一个指针指向迭代器原本的下一个块（保持链表顺序）
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 若待插入块同时合并了前驱块和后继块（填补了中间空隙），
     * 则它的 pxNextFreeBlock 指针已在前面的步骤中设置，
     * 此处无需重复赋值（否则会导致指针指向自身，破坏链表）。 */
    if( pxIterator != pxBlockToInsert )  // 若未合并前驱块（迭代器与待插入块指针不同）
    {
        // 迭代器的下一个指针指向待插入块，将待插入块接入空闲链表
        pxIterator->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxBlockToInsert );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
/*-----------------------------------------------------------*/

void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxFirstFreeBlockInRegion = NULL;  // 指向当前区域的初始空闲块
    BlockLink_t * pxPreviousFreeBlock;              // 指向前一个区域的尾哨兵（用于链接多区域）
    portPOINTER_SIZE_TYPE xAlignedHeap;             // 当前区域对齐后的起始地址
    size_t xTotalRegionSize, xTotalHeapSize = 0;    // 当前区域总大小、所有区域总空闲大小
    BaseType_t xDefinedRegions = 0;                 // 已处理的区域计数（从0开始）
    portPOINTER_SIZE_TYPE xAddress;                 // 临时地址变量（用于计算）
    const HeapRegion_t * pxHeapRegion;              // 指向当前正在处理的 HeapRegion_t 结构体

    /* 该函数只能调用一次！（堆初始化后不可重复定义） */
    configASSERT( pxEnd == NULL );  // 断言堆未初始化（pxEnd 是尾哨兵，初始为NULL）

    #if ( configENABLE_HEAP_PROTECTOR == 1 )  // 若启用堆保护
    {
        // 获取随机金丝雀值（用于加密堆指针）
        vApplicationGetRandomHeapCanary( &( xHeapCanary ) );
    }
    #endif

    // 从第一个区域开始处理（xDefinedRegions 初始为0）
    pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );

    // 遍历所有区域，直到遇到“空地址+零大小”的终止区域
    while( pxHeapRegion->xSizeInBytes > 0 )
    {
        xTotalRegionSize = pxHeapRegion->xSizeInBytes;  // 当前区域的原始大小

        /* 确保当前区域的起始地址满足字节对齐要求（避免未对齐访问） */
        xAddress = ( portPOINTER_SIZE_TYPE ) pxHeapRegion->pucStartAddress;  // 当前区域原始起始地址

        if( ( xAddress & portBYTE_ALIGNMENT_MASK ) != 0 )  // 检查是否未对齐
        {
            xAddress += ( portBYTE_ALIGNMENT - 1 );  // 增加补偿值，确保后续能对齐
            xAddress &= ~( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK;  // 清除低n位，实现向上对齐
            // 调整区域总大小：减去未对齐部分的长度（对齐后可用空间减少）
            xTotalRegionSize -= ( size_t ) ( xAddress - ( portPOINTER_SIZE_TYPE ) pxHeapRegion->pucStartAddress );
        }

        xAlignedHeap = xAddress;  // 保存对齐后的区域起始地址

        /* 若处理的是第一个区域，初始化堆的头哨兵（xStart） */
        if( xDefinedRegions == 0 )
        {
            /* xStart 用于存储空闲块链表的表头指针（头哨兵）。
             * 强制转换是为了避免编译器警告。 */
            xStart.pxNextFreeBlock = ( BlockLink_t * ) heapPROTECT_BLOCK_POINTER( xAlignedHeap );
            xStart.xBlockSize = ( size_t ) 0;  // 头哨兵大小设为0（仅作链表标记）
        }
        else  // 处理非第一个区域
        {
            // 断言：非第一个区域时，前一个区域的尾哨兵已初始化（pxEnd 非NULL）
            configASSERT( pxEnd != heapPROTECT_BLOCK_POINTER( NULL ) );
            // 断言：区域必须按地址从低到高排列（当前区域地址 > 前一个区域尾哨兵地址）
            configASSERT( ( size_t ) xAddress > ( size_t ) pxEnd );
        }

        #if ( configENABLE_HEAP_PROTECTOR == 1 )  // 若启用堆保护
        {
            // 更新堆的最低地址（取所有区域的最小起始地址）
            if( ( pucHeapLowAddress == NULL ) ||
                ( ( uint8_t * ) xAlignedHeap < pucHeapLowAddress ) )
            {
                pucHeapLowAddress = ( uint8_t * ) xAlignedHeap;
            }
        }
        #endif /* configENABLE_HEAP_PROTECTOR */

        // 保存前一个区域的尾哨兵（用于链接当前区域）
        pxPreviousFreeBlock = pxEnd;

        /* 初始化当前区域的尾哨兵（pxEnd）：标记当前区域的空闲链表末尾，放置在区域末尾 */
        xAddress = xAlignedHeap + ( portPOINTER_SIZE_TYPE ) xTotalRegionSize;  // 当前区域原始结束地址
        xAddress -= ( portPOINTER_SIZE_TYPE ) xHeapStructSize;  // 减去尾哨兵的块头大小（BlockLink_t）
        xAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK );  // 确保尾哨兵地址对齐
        pxEnd = ( BlockLink_t * ) xAddress;  // 尾哨兵指针指向调整后的地址
        pxEnd->xBlockSize = 0;  // 尾哨兵大小设为0
        pxEnd->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL );  // 尾哨兵下一个指针设为NULL（链表结束）

        /* 初始化当前区域的初始空闲块：
         * 初始状态下，当前区域只有一个空闲块，占用除尾哨兵外的全部可用空间 */
        pxFirstFreeBlockInRegion = ( BlockLink_t * ) xAlignedHeap;  // 初始空闲块起始地址 = 区域对齐后起始地址
        // 初始空闲块大小 = 尾哨兵地址 - 初始空闲块地址（即区域可用空间总大小）
        pxFirstFreeBlockInRegion->xBlockSize = ( size_t ) ( xAddress - ( portPOINTER_SIZE_TYPE ) pxFirstFreeBlockInRegion );
        pxFirstFreeBlockInRegion->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );  // 初始空闲块下一个指针指向尾哨兵

        /* 若当前区域不是第一个区域，将前一个区域的尾哨兵与当前区域的初始空闲块链接 */
        if( pxPreviousFreeBlock != NULL )
        {
            pxPreviousFreeBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxFirstFreeBlockInRegion );
        }

        // 累加所有区域的总空闲大小
        xTotalHeapSize += pxFirstFreeBlockInRegion->xBlockSize;

        #if ( configENABLE_HEAP_PROTECTOR == 1 )  // 若启用堆保护
        {
            // 更新堆的最高地址（取所有区域的最大结束地址）
            if( ( pucHeapHighAddress == NULL ) ||
                ( ( ( ( uint8_t * ) pxFirstFreeBlockInRegion ) + pxFirstFreeBlockInRegion->xBlockSize ) > pucHeapHighAddress ) )
            {
                pucHeapHighAddress = ( ( uint8_t * ) pxFirstFreeBlockInRegion ) + pxFirstFreeBlockInRegion->xBlockSize;
            }
        }
        #endif

        // 处理下一个区域
        xDefinedRegions++;
        pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );
    }

    // 初始化堆状态变量：历史最小空闲字节数 = 总空闲大小（初始无分配）
    xMinimumEverFreeBytesRemaining = xTotalHeapSize;
    // 剩余空闲字节数 = 总空闲大小
    xFreeBytesRemaining = xTotalHeapSize;

    // 断言：至少定义了一个有效区域（避免堆总大小为0）
    configASSERT( xTotalHeapSize );
}
/*-----------------------------------------------------------*/

void vPortGetHeapStats( HeapStats_t * pxHeapStats )
{
    BlockLink_t * pxBlock;
    size_t xBlocks = 0, xMaxSize = 0, xMinSize = portMAX_DELAY; /* portMAX_DELAY used as a portable way of getting the maximum value. */

    vTaskSuspendAll();
    {
        pxBlock = heapPROTECT_BLOCK_POINTER( xStart.pxNextFreeBlock );

        /* pxBlock will be NULL if the heap has not been initialised.  The heap
         * is initialised automatically when the first allocation is made. */
        if( pxBlock != NULL )
        {
            while( pxBlock != pxEnd )
            {
                /* Increment the number of blocks and record the largest block seen
                 * so far. */
                xBlocks++;

                if( pxBlock->xBlockSize > xMaxSize )
                {
                    xMaxSize = pxBlock->xBlockSize;
                }

                /* Heap five will have a zero sized block at the end of each
                 * each region - the block is only used to link to the next
                 * heap region so it not a real block. */
                if( pxBlock->xBlockSize != 0 )
                {
                    if( pxBlock->xBlockSize < xMinSize )
                    {
                        xMinSize = pxBlock->xBlockSize;
                    }
                }

                /* Move to the next block in the chain until the last block is
                 * reached. */
                pxBlock = heapPROTECT_BLOCK_POINTER( pxBlock->pxNextFreeBlock );
            }
        }
    }
    ( void ) xTaskResumeAll();

    pxHeapStats->xSizeOfLargestFreeBlockInBytes = xMaxSize;
    pxHeapStats->xSizeOfSmallestFreeBlockInBytes = xMinSize;
    pxHeapStats->xNumberOfFreeBlocks = xBlocks;

    taskENTER_CRITICAL();
    {
        pxHeapStats->xAvailableHeapSpaceInBytes = xFreeBytesRemaining;
        pxHeapStats->xNumberOfSuccessfulAllocations = xNumberOfSuccessfulAllocations;
        pxHeapStats->xNumberOfSuccessfulFrees = xNumberOfSuccessfulFrees;
        pxHeapStats->xMinimumEverFreeBytesRemaining = xMinimumEverFreeBytesRemaining;
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*
 * Reset the state in this file. This state is normally initialized at start up.
 * This function must be called by the application before restarting the
 * scheduler.
 */
void vPortHeapResetState( void )
{
    pxEnd = NULL;

    xFreeBytesRemaining = ( size_t ) 0U;
    xMinimumEverFreeBytesRemaining = ( size_t ) 0U;
    xNumberOfSuccessfulAllocations = ( size_t ) 0U;
    xNumberOfSuccessfulFrees = ( size_t ) 0U;

    #if ( configENABLE_HEAP_PROTECTOR == 1 )
        pucHeapHighAddress = NULL;
        pucHeapLowAddress = NULL;
    #endif /* #if ( configENABLE_HEAP_PROTECTOR == 1 ) */
}
/*-----------------------------------------------------------*/
