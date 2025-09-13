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
 * A sample implementation of pvPortMalloc() and vPortFree() that combines
 * (coalescences) adjacent memory blocks as they are freed, and in so doing
 * limits memory fragmentation.
 *
 * See heap_1.c, heap_2.c and heap_3.c for alternative implementations, and the
 * memory management pages of https://www.FreeRTOS.org for more information.
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

/* 块大小不能太小。 */
#define heapMINIMUM_BLOCK_SIZE    ( ( size_t ) ( xHeapStructSize << 1 ) )

/* 假设是8位字节！ */
#define heapBITS_PER_BYTE         ( ( size_t ) 8 )

/* 适合 size_t 类型的最大值。 */
#define heapSIZE_MAX              ( ~( ( size_t ) 0 ) )

/* 检查 a 和 b 相乘是否会导致溢出。 */
#define heapMULTIPLY_WILL_OVERFLOW( a, b )     ( ( ( a ) > 0 ) && ( ( b ) > ( heapSIZE_MAX / ( a ) ) ) )

/* 检查 a 和 b 相加是否会导致溢出。 */
#define heapADD_WILL_OVERFLOW( a, b )          ( ( a ) > ( heapSIZE_MAX - ( b ) ) )

/* 检查减法操作 (a - b) 是否会导致下溢。 */
#define heapSUBTRACT_WILL_UNDERFLOW( a, b )    ( ( a ) < ( b ) )

/* BlockLink_t 结构体的 xBlockSize 成员的最高位（MSB）用于跟踪块的分配状态。
 * 当 BlockLink_t 结构体的 xBlockSize 成员的最高位被设置时，该块属于应用程序（已分配）。
 * 当该位未被设置时，该块仍属于空闲堆空间（未分配）。*/
#define heapBLOCK_ALLOCATED_BITMASK    ( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 ) )
#define heapBLOCK_SIZE_IS_VALID( xBlockSize )    ( ( ( xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) == 0 )
#define heapBLOCK_IS_ALLOCATED( pxBlock )        ( ( ( pxBlock->xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) != 0 )
#define heapALLOCATE_BLOCK( pxBlock )            ( ( pxBlock->xBlockSize ) |= heapBLOCK_ALLOCATED_BITMASK )
#define heapFREE_BLOCK( pxBlock )                ( ( pxBlock->xBlockSize ) &= ~heapBLOCK_ALLOCATED_BITMASK )

/*-----------------------------------------------------------*/

/* 为堆分配内存。 */
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )

/* 应用程序编写者已经定义了用于RTOS堆的数组——可能是为了将其放置在特殊的段或地址中。 */
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    PRIVILEGED_DATA static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif /* configAPPLICATION_ALLOCATED_HEAP */

/* 定义链表结构。这用于按内存地址顺序链接空闲块。 */
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /**< 链表中的下一个空闲块。 */
    size_t xBlockSize;                     /**< 空闲块的大小。 */
} BlockLink_t;

/* 将 configENABLE_HEAP_PROTECTOR 设为1可启用堆块指针保护，
 * 使用应用程序提供的金丝雀值（canary）来捕获堆缓冲区溢出导致的堆损坏。
 */
#if ( configENABLE_HEAP_PROTECTOR == 1 )

/**
 * @brief 应用程序提供的函数，用于获取随机值作为金丝雀（canary）。
 *
 * @param pxHeapCanary [out] 输出参数，用于返回金丝雀值。
 */
    extern void vApplicationGetRandomHeapCanary( portPOINTER_SIZE_TYPE * pxHeapCanary );

/* 用于保护内部堆指针的金丝雀值。 */
    PRIVILEGED_DATA static portPOINTER_SIZE_TYPE xHeapCanary;

/* 用于向内存加载/存储 BlockLink_t 指针的宏。通过将指针与随机金丝雀值异或，
 * 堆溢出将导致不可预测的随机指针值，这些值会被 heapVALIDATE_BLOCK_POINTER 断言捕获。 */
    #define heapPROTECT_BLOCK_POINTER( pxBlock )    ( ( BlockLink_t * ) ( ( ( portPOINTER_SIZE_TYPE ) ( pxBlock ) ) ^ xHeapCanary ) )
#else

    #define heapPROTECT_BLOCK_POINTER( pxBlock )    ( pxBlock )

#endif /* configENABLE_HEAP_PROTECTOR */

/* 断言堆块指针在堆的边界内。 */
#define heapVALIDATE_BLOCK_POINTER( pxBlock )                          \
    configASSERT( ( ( uint8_t * ) ( pxBlock ) >= &( ucHeap[ 0 ] ) ) && \
                  ( ( uint8_t * ) ( pxBlock ) <= &( ucHeap[ configTOTAL_HEAP_SIZE - 1 ] ) ) )

/*-----------------------------------------------------------*/

/*
 * 将待释放的内存块插入到空闲内存块链表的正确位置。
 * 若待释放的块与前面的块和/或后面的块在内存上相邻，
 * 则会将这些块合并为一个更大的块。
 */
static void prvInsertBlockIntoFreeList( BlockLink_t * pxBlockToInsert ) PRIVILEGED_FUNCTION;

/*
 * 首次调用 pvPortMalloc() 时自动调用，用于初始化所需的堆结构。
 */
static void prvHeapInit( void ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

/* 放置在每个已分配内存块开头的结构体（块头）必须满足正确的字节对齐要求。 */
static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );

/* 创建两个链表节点，用于标记链表的起始和结束。 */
PRIVILEGED_DATA static BlockLink_t xStart;
PRIVILEGED_DATA static BlockLink_t * pxEnd = NULL;

/* 跟踪内存分配和释放的调用次数，以及剩余的空闲字节数，
 * 但不反映内存碎片情况。 */
PRIVILEGED_DATA static size_t xFreeBytesRemaining = ( size_t ) 0U;
PRIVILEGED_DATA static size_t xMinimumEverFreeBytesRemaining = ( size_t ) 0U;
PRIVILEGED_DATA static size_t xNumberOfSuccessfulAllocations = ( size_t ) 0U;
PRIVILEGED_DATA static size_t xNumberOfSuccessfulFrees = ( size_t ) 0U;

/*-----------------------------------------------------------*/

void * pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t * pxBlock;          // 指向当前遍历的空闲块
    BlockLink_t * pxPreviousBlock;  // 指向当前块的前一个空闲块
    BlockLink_t * pxNewBlockLink;   // 指向拆分后新生成的空闲块
    void * pvReturn = NULL;         // 最终返回给用户的内存地址
    size_t xAdditionalRequiredSize; // 字节对齐所需的补充大小

    // 检查请求的大小是否有效（非0）
    if( xWantedSize > 0 )
    {
        /* 需增加请求大小，使其除了包含用户请求的字节数外，还能容纳一个 BlockLink_t 结构体（块头） */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )
        {
            xWantedSize += xHeapStructSize;

            /* 确保块始终满足所需的字节对齐要求 */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
            {
                /* 需要字节对齐：计算补充大小 */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                // 检查补充后是否溢出
                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )
                {
                    xWantedSize += xAdditionalRequiredSize;
                }
                else
                {
                    xWantedSize = 0; // 溢出则标记为无效大小
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // 覆盖率测试标记（无实际逻辑）
            }
        }
        else
        {
            xWantedSize = 0; // 溢出则标记为无效大小
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    // 挂起所有任务，确保内存分配操作的原子性（避免多任务竞争）
    vTaskSuspendAll();
    {
        /* 若首次调用 malloc，堆需要初始化以搭建空闲块链表 */
        if( pxEnd == NULL )
        {
            prvHeapInit();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        /* 检查待分配的块大小是否有效：不能占用 BlockLink_t 结构体 xBlockSize 成员的最高位（该位用于标记分配状态） */
        if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
        {
            // 检查大小是否有效且剩余空闲空间足够
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* 从链表起始位置（低地址）遍历，找到第一个大小足够的块 */
                pxPreviousBlock = &xStart;
                pxBlock = heapPROTECT_BLOCK_POINTER( xStart.pxNextFreeBlock ); // 解密指针（若启用堆保护）
                heapVALIDATE_BLOCK_POINTER( pxBlock ); // 验证指针是否在堆范围内

                // 遍历条件：当前块大小不足，且未到链表末尾
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != heapPROTECT_BLOCK_POINTER( NULL ) ) )
                {
                    pxPreviousBlock = pxBlock;
                    pxBlock = heapPROTECT_BLOCK_POINTER( pxBlock->pxNextFreeBlock ); // 解密下一个块指针
                    heapVALIDATE_BLOCK_POINTER( pxBlock );
                }

                /* 若未遍历到末尾（pxEnd），说明找到足够大的块 */
                if( pxBlock != pxEnd )
                {
                    /* 返回用户可用地址：跳过块头（BlockLink_t 结构体） */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxPreviousBlock->pxNextFreeBlock ) ) + xHeapStructSize );
                    heapVALIDATE_BLOCK_POINTER( pvReturn );

                    /* 将当前块从空闲链表中移除（分配给用户） */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* 若当前块远大于请求大小，将其拆分为“用户分配块”和“新空闲块” */
                    configASSERT( heapSUBTRACT_WILL_UNDERFLOW( pxBlock->xBlockSize, xWantedSize ) == 0 ); // 断言无下溢

                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                    {
                        /* 计算新空闲块的地址：当前块地址 + 用户请求的总大小（含块头） */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                        configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 ); // 断言新块地址对齐

                        /* 计算拆分后两个块的大小 */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize; // 新空闲块大小
                        pxBlock->xBlockSize = xWantedSize; // 用户分配块大小（含块头）

                        /* 将新空闲块插入空闲链表（按大小顺序） */
                        pxNewBlockLink->pxNextFreeBlock = pxPreviousBlock->pxNextFreeBlock;
                        pxPreviousBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxNewBlockLink ); // 加密指针（若启用堆保护）
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    // 更新剩余空闲字节数
                    xFreeBytesRemaining -= pxBlock->xBlockSize;

                    // 更新历史最小剩余空闲字节数
                    if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                    {
                        xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 标记当前块为已分配（设置 xBlockSize 最高位），且已分配块无“下一个块”指针 */
                    heapALLOCATE_BLOCK( pxBlock );
                    pxBlock->pxNextFreeBlock = NULL;
                    xNumberOfSuccessfulAllocations++; // 成功分配次数+1
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // 未找到足够大的块
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // 大小无效或空闲空间不足
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); // 块大小无效（占用了最高位）
        }

        traceMALLOC( pvReturn, xWantedSize ); // 调试跟踪：记录分配的地址和大小
    }
    ( void ) xTaskResumeAll(); // 恢复任务调度

    // 若启用“分配失败钩子函数”，且分配失败，则调用钩子函数
    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* if ( configUSE_MALLOC_FAILED_HOOK == 1 ) */

    // 断言返回的地址满足字节对齐要求
    configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
    return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree( void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;  // 将用户传入的内存指针转换为字节指针
    BlockLink_t * pxLink;              // 用于指向待释放块的块头（BlockLink_t 结构体）

    // 检查待释放的指针是否有效（非 NULL）
    if( pv != NULL )
    {
        /* 被释放的内存块前面，必然存在一个 BlockLink_t 结构体（块头） */
        puc -= xHeapStructSize;  // 从用户指针回退 xHeapStructSize 字节，定位到块头地址

        /* 强制转换是为了避免编译器发出警告 */
        pxLink = ( void * ) puc;  // 将块头地址转换为 BlockLink_t* 类型

        // 验证块头指针是否在堆的合法范围内（防止非法地址释放）
        heapVALIDATE_BLOCK_POINTER( pxLink );
        // 断言：待释放的块必须处于“已分配”状态（避免重复释放或释放空闲块）
        configASSERT( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 );
        // 断言：已分配块的“下一个空闲块指针”必须为 NULL（已分配块不参与空闲链表）
        configASSERT( pxLink->pxNextFreeBlock == NULL );

        // 再次检查块是否处于已分配状态（双重保险，避免断言被关闭时出错）
        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            // 检查已分配块的“下一个指针”是否为 NULL（确保块结构未被破坏）
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* 块即将归还给堆 —— 标记为“空闲”状态 */
                heapFREE_BLOCK( pxLink );  // 清除块头 xBlockSize 的最高位（标记为空闲）

                #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )  // 若启用“释放时清零内存”配置
                {
                    /* 检查是否会下溢（防止块大小被篡改导致非法清零） */
                    if( heapSUBTRACT_WILL_UNDERFLOW( pxLink->xBlockSize, xHeapStructSize ) == 0 )
                    {
                        // 将用户数据区清零：从块头结束地址开始，长度为“块总大小 - 块头大小”
                        ( void ) memset( puc + xHeapStructSize, 0, pxLink->xBlockSize - xHeapStructSize );
                    }
                }
                #endif

                // 挂起所有任务，确保释放操作的原子性（避免多任务竞争空闲链表）
                vTaskSuspendAll();
                {
                    /* 将空闲块加入空闲链表 */
                    xFreeBytesRemaining += pxLink->xBlockSize;  // 更新剩余空闲字节数
                    traceFREE( pv, pxLink->xBlockSize );        // 调试跟踪：记录释放的地址和大小
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );  // 插入空闲链表
                    xNumberOfSuccessfulFrees++;                 // 成功释放次数 +1
                }
                ( void ) xTaskResumeAll();  // 恢复任务调度
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
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

void vPortInitialiseBlocks( void )
{
    /* This just exists to keep the linker quiet. */
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

static void prvHeapInit( void ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxFirstFreeBlock;       // 指向初始的单个空闲块
    portPOINTER_SIZE_TYPE uxStartAddress; // 堆的实际起始地址（对齐后）
    portPOINTER_SIZE_TYPE uxEndAddress;   // 堆的实际结束地址（对齐后）
    size_t xTotalHeapSize = configTOTAL_HEAP_SIZE; // 堆的总大小（用户配置）

    /* 确保堆的起始地址满足正确的字节对齐要求 */
    uxStartAddress = ( portPOINTER_SIZE_TYPE ) ucHeap;

    // 若起始地址未对齐，调整为对齐地址
    if( ( uxStartAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
    {
        uxStartAddress += ( portBYTE_ALIGNMENT - 1 );  // 先增加补偿值，确保后续能对齐
        uxStartAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ); // 清除低n位，实现向上对齐
        xTotalHeapSize -= ( size_t ) ( uxStartAddress - ( portPOINTER_SIZE_TYPE ) ucHeap ); // 调整堆总大小（减去未对齐的部分）
    }

    #if ( configENABLE_HEAP_PROTECTOR == 1 )
    {
        // 若启用堆保护，获取随机金丝雀值（用于加密堆指针）
        vApplicationGetRandomHeapCanary( &( xHeapCanary ) );
    }
    #endif

    /* xStart 用于存储空闲块链表的表头指针（头哨兵）。
     * 强制转换为 void* 是为了避免编译器警告。 */
    xStart.pxNextFreeBlock = ( void * ) heapPROTECT_BLOCK_POINTER( uxStartAddress ); // 头哨兵指向第一个空闲块（对齐后的起始地址）
    xStart.xBlockSize = ( size_t ) 0; // 头哨兵的大小设为0（仅作链表标记，无实际内存）

    /* pxEnd 用于标记空闲块链表的末尾（尾哨兵），放置在堆空间的末尾。 */
    uxEndAddress = uxStartAddress + ( portPOINTER_SIZE_TYPE ) xTotalHeapSize; // 堆的原始结束地址（起始地址+总大小）
    uxEndAddress -= ( portPOINTER_SIZE_TYPE ) xHeapStructSize; // 减去一个块头大小（尾哨兵需占用一个BlockLink_t的空间）
    uxEndAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ); // 确保尾哨兵地址对齐
    pxEnd = ( BlockLink_t * ) uxEndAddress; // 尾哨兵指针指向调整后的结束地址
    pxEnd->xBlockSize = 0; // 尾哨兵的大小设为0（仅作链表标记）
    pxEnd->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL ); // 尾哨兵的下一个指针设为NULL（链表结束标志）

    /* 初始状态下，堆中只有一个空闲块，占用除尾哨兵外的全部堆空间。 */
    pxFirstFreeBlock = ( BlockLink_t * ) uxStartAddress; // 第一个空闲块的起始地址就是堆的对齐后起始地址
    // 计算第一个空闲块的大小：尾哨兵地址 - 第一个空闲块地址（即堆可用空间总大小）
    pxFirstFreeBlock->xBlockSize = ( size_t ) ( uxEndAddress - ( portPOINTER_SIZE_TYPE ) pxFirstFreeBlock );
    pxFirstFreeBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd ); // 第一个空闲块的下一个指针指向尾哨兵

    /* 初始时只有一个空闲块，其大小就是堆的全部可用空间，因此剩余空闲字节数和历史最小空闲字节数均设为该大小。 */
    xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( BlockLink_t * pxBlockToInsert ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxIterator;  // 遍历空闲链表的迭代器指针
    uint8_t * puc;             // 用于地址计算的字节指针

    /* 遍历链表，直到找到地址高于待插入块的块（确定待插入块的位置） */
    for( pxIterator = &xStart; 
         heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) < pxBlockToInsert; 
         pxIterator = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        /* 无需额外操作，仅遍历到正确位置即可 */
    }

    // 若迭代器不是头哨兵，验证其指针是否在堆范围内
    if( pxIterator != &xStart )
    {
        heapVALIDATE_BLOCK_POINTER( pxIterator );
    }

    /* 待插入块与它前面的块（迭代器指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxIterator;

    // 前面块的结束地址（前面块地址 + 前面块大小）是否等于待插入块的起始地址
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        // 合并：前面块的大小 += 待插入块的大小
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        // 待插入块指向合并后的块（后续操作基于合并后的块）
        pxBlockToInsert = pxIterator;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
    }

    /* 待插入块与它后面的块（迭代器下一个指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxBlockToInsert;

    // 待插入块的结束地址（待插入块地址 + 待插入块大小）是否等于后面块的起始地址
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        // 确保后面的块不是尾哨兵（尾哨兵无实际内存，无需合并）
        if( heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) != pxEnd )
        {
            /* 将两个块合并为一个大块 */
            // 待插入块的大小 += 后面块的大小
            pxBlockToInsert->xBlockSize += heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->xBlockSize;
            // 待插入块的下一个指针指向后面块的下一个指针（跳过后面块）
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->pxNextFreeBlock;
        }
        else
        {
            // 若后面是尾哨兵，待插入块的下一个指针直接指向尾哨兵
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );
        }
    }
    else
    {
        // 无法合并，待插入块的下一个指针指向迭代器原本的下一个块
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 若待插入块同时与前面块和后面块合并（填补了中间的空隙），
     * 则它的 pxNextFreeBlock 指针已在前面的步骤中设置，
     * 此处无需重复设置（否则会导致指针指向自身）。 */
    if( pxIterator != pxBlockToInsert )
    {
        // 迭代器的下一个指针指向待插入块（完成插入）
        pxIterator->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxBlockToInsert );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
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

                if( pxBlock->xBlockSize < xMinSize )
                {
                    xMinSize = pxBlock->xBlockSize;
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
}
/*-----------------------------------------------------------*/
