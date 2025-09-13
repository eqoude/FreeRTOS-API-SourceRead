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

/*-----------------------------------------------------------
* Portable layer API.  Each function must be defined for each port.
*----------------------------------------------------------*/

#ifndef PORTABLE_H
#define PORTABLE_H

/* 每个FreeRTOS移植版本都有一个独特的portmacro.h头文件。最初，人们使用预处理器定义来确保预处理器能找到所使用移植版本对应的正确portmacro.h文件。
 *该方案已被弃用，取而代之的是设置编译器的包含路径，使其能找到正确的portmacro.h文件——这样就不再需要那个常量，并且允许portmacro.h文件相对于所使用的移植版本位于任何位置。
 *纯粹出于向后兼容的考虑，旧方法仍然有效，但为了明确新项目不应使用它，对移植版本特定常量的支持已移至deprecated_definitions.h头文件中。 */
#include "deprecated_definitions.h"

/* 如果 portENTER_CRITICAL 未定义，
 * 说明包含 deprecated_definitions.h 并未引入 portmacro.h 头文件——此时应在此处包含它。
 * 在这种情况下，必须在编译器的包含路径中设置正确的 portmacro.h 头文件的路径。 */
#ifndef portENTER_CRITICAL
    #include "portmacro.h"
#endif

#if portBYTE_ALIGNMENT == 32
    #define portBYTE_ALIGNMENT_MASK    ( 0x001f )  // 32字节对齐的掩码
#elif portBYTE_ALIGNMENT == 16
    #define portBYTE_ALIGNMENT_MASK    ( 0x000f )  // 16字节对齐的掩码
#elif portBYTE_ALIGNMENT == 8
    #define portBYTE_ALIGNMENT_MASK    ( 0x0007 )  // 8字节对齐的掩码
#elif portBYTE_ALIGNMENT == 4
    #define portBYTE_ALIGNMENT_MASK    ( 0x0003 )  // 4字节对齐的掩码
#elif portBYTE_ALIGNMENT == 2
    #define portBYTE_ALIGNMENT_MASK    ( 0x0001 )  // 2字节对齐的掩码
#elif portBYTE_ALIGNMENT == 1
    #define portBYTE_ALIGNMENT_MASK    ( 0x0000 )  // 1字节对齐的掩码（无需对齐）
#else /* if portBYTE_ALIGNMENT == 32 */
    #error "Invalid portBYTE_ALIGNMENT definition"  // 错误：portBYTE_ALIGNMENT定义无效
#endif /* if portBYTE_ALIGNMENT == 32 */

#ifndef portUSING_MPU_WRAPPERS
    #define portUSING_MPU_WRAPPERS    0  // 是否使用MPU封装器，默认禁用
#endif

#ifndef portNUM_CONFIGURABLE_REGIONS
    #define portNUM_CONFIGURABLE_REGIONS    1  // 可配置的内存保护区域数量，默认1个
#endif

#ifndef portHAS_STACK_OVERFLOW_CHECKING
    #define portHAS_STACK_OVERFLOW_CHECKING    0  // 是否支持栈溢出检查，默认禁用
#endif

#ifndef portARCH_NAME
    #define portARCH_NAME    NULL  // 目标架构名称，默认空
#endif

#ifndef configSTACK_DEPTH_TYPE
    #define configSTACK_DEPTH_TYPE    StackType_t  // 栈深度的类型，默认使用StackType_t
#endif

#ifndef configSTACK_ALLOCATION_FROM_SEPARATE_HEAP
    /* 为向后兼容，默认值设为0 */
    #define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP    0  // 是否从独立堆分配栈空间，默认禁用
#endif

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

#include "mpu_wrappers.h"

/*
 * 初始化新任务的栈，使其准备好被纳入调度器控制。
 * 寄存器值必须按照移植版本预期的顺序存入栈中。
 *
 */
#if ( portUSING_MPU_WRAPPERS == 1 )  // 若启用MPU封装器
    #if ( portHAS_STACK_OVERFLOW_CHECKING == 1 )  // 若启用栈溢出检查
        // 带MPU配置和栈溢出检查的栈初始化函数声明
        StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                             StackType_t * pxEndOfStack,
                                             TaskFunction_t pxCode,
                                             void * pvParameters,
                                             BaseType_t xRunPrivileged,
                                             xMPU_SETTINGS * xMPUSettings ) PRIVILEGED_FUNCTION;
    #else  // 未启用栈溢出检查
        // 带MPU配置、无栈溢出检查的栈初始化函数声明
        StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                             TaskFunction_t pxCode,
                                             void * pvParameters,
                                             BaseType_t xRunPrivileged,
                                             xMPU_SETTINGS * xMPUSettings ) PRIVILEGED_FUNCTION;
    #endif /* if ( portHAS_STACK_OVERFLOW_CHECKING == 1 ) */
#else /* if ( portUSING_MPU_WRAPPERS == 1 ) */  // 未启用MPU封装器
    #if ( portHAS_STACK_OVERFLOW_CHECKING == 1 )  // 若启用栈溢出检查
        // 无MPU配置、带栈溢出检查的栈初始化函数声明
        StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                             StackType_t * pxEndOfStack,
                                             TaskFunction_t pxCode,
                                             void * pvParameters ) PRIVILEGED_FUNCTION;
    #else  // 未启用栈溢出检查
        // 无MPU配置、无栈溢出检查的栈初始化函数声明
        StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                             TaskFunction_t pxCode,
                                             void * pvParameters ) PRIVILEGED_FUNCTION;
    #endif
#endif /* if ( portUSING_MPU_WRAPPERS == 1 ) */

/* 被 heap_5.c 用于定义每个内存区域的起始地址和大小，这些内存区域共同构成了 FreeRTOS 的总堆空间。 */
typedef struct HeapRegion
{
    uint8_t * pucStartAddress;  // 内存区域的起始地址
    size_t xSizeInBytes;        // 内存区域的大小（以字节为单位）
} HeapRegion_t;

/* 用于通过 vPortGetHeapStats() 函数传出堆的相关信息。 */
typedef struct xHeapStats
{
    size_t xAvailableHeapSpaceInBytes;      /* 当前可用的总堆空间大小 - 这是所有空闲块的总和，而非可分配的最大块大小。 */
    size_t xSizeOfLargestFreeBlockInBytes;  /* 调用 vPortGetHeapStats() 时，堆中所有空闲块的最大字节数。 */
    size_t xSizeOfSmallestFreeBlockInBytes; /* 调用 vPortGetHeapStats() 时，堆中所有空闲块的最小字节数。 */
    size_t xNumberOfFreeBlocks;             /* 调用 vPortGetHeapStats() 时，堆中的空闲内存块数量。 */
    size_t xMinimumEverFreeBytesRemaining;  /* 系统启动以来，堆中可用内存的最小总量（所有空闲块的总和）。 */
    size_t xNumberOfSuccessfulAllocations;  /* 成功返回有效内存块的 pvPortMalloc() 调用次数。 */
    size_t xNumberOfSuccessfulFrees;        /* 成功释放内存块的 vPortFree() 调用次数。 */
} HeapStats_t;

/*
 * 用于为 heap_5.c 定义多个堆内存区域。
 * 此函数必须在任何对 pvPortMalloc() 的调用之前调用——创建任务、队列、信号量、互斥锁、
 * 软件定时器、事件组等操作都会间接调用 pvPortMalloc()，因此必须在此类操作前调用本函数。
 *
 * pxHeapRegions 参数传入一个 HeapRegion_t 结构体数组——数组中的每个元素都定义了一块
 * 可作为堆使用的内存区域。数组必须以一个 size 为 0 的 HeapRegion_t 结构体作为结束标记。
 * 起始地址最低的内存区域必须放在数组的最前面。
 */
void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions ) PRIVILEGED_FUNCTION;

/*
 * Returns a HeapStats_t structure filled with information about the current
 * heap state.
 */
void vPortGetHeapStats( HeapStats_t * pxHeapStats );

/*
 * Map to the memory management routines required for the port.
 */
void * pvPortMalloc( size_t xWantedSize ) PRIVILEGED_FUNCTION;
void * pvPortCalloc( size_t xNum,
                     size_t xSize ) PRIVILEGED_FUNCTION;
void vPortFree( void * pv ) PRIVILEGED_FUNCTION;
void vPortInitialiseBlocks( void ) PRIVILEGED_FUNCTION;
size_t xPortGetFreeHeapSize( void ) PRIVILEGED_FUNCTION;
size_t xPortGetMinimumEverFreeHeapSize( void ) PRIVILEGED_FUNCTION;

#if ( configSTACK_ALLOCATION_FROM_SEPARATE_HEAP == 1 )
    void * pvPortMallocStack( size_t xSize ) PRIVILEGED_FUNCTION;
    void vPortFreeStack( void * pv ) PRIVILEGED_FUNCTION;
#else
    #define pvPortMallocStack    pvPortMalloc
    #define vPortFreeStack       vPortFree
#endif

/*
 * This function resets the internal state of the heap module. It must be called
 * by the application before restarting the scheduler.
 */
void vPortHeapResetState( void ) PRIVILEGED_FUNCTION;

#if ( configUSE_MALLOC_FAILED_HOOK == 1 )

/**
 * task.h
 * @code{c}
 * void vApplicationMallocFailedHook( void )
 * @endcode
 *
 * This hook function is called when allocation failed.
 */
    void vApplicationMallocFailedHook( void );
#endif

/*
 * 初始化硬件，为调度器接管系统控制权做好准备。
 * 通常包括：配置滴答定时器中断（tick interrupt），
 * 并将定时器设置为正确的滴答频率（确保任务调度周期准确）。
 */
BaseType_t xPortStartScheduler( void ) PRIVILEGED_FUNCTION;

/*
 * 撤销 xPortStartScheduler() 函数中对硬件/中断服务程序（ISR）的所有初始化操作，
 * 确保调度器停止运行后，硬件能恢复到其初始状态（未初始化调度器前的状态）。
 */
void vPortEndScheduler( void ) PRIVILEGED_FUNCTION;

/*
 * The structures and methods of manipulating the MPU are contained within the
 * port layer.
 *
 * Fills the xMPUSettings structure with the memory region information
 * contained in xRegions.
 */
#if ( portUSING_MPU_WRAPPERS == 1 )
    struct xMEMORY_REGION;
    void vPortStoreTaskMPUSettings( xMPU_SETTINGS * xMPUSettings,
                                    const struct xMEMORY_REGION * const xRegions,
                                    StackType_t * pxBottomOfStack,
                                    configSTACK_DEPTH_TYPE uxStackDepth ) PRIVILEGED_FUNCTION;
#endif

/**
 * @brief Checks if the calling task is authorized to access the given buffer.
 *
 * @param pvBuffer The buffer which the calling task wants to access.
 * @param ulBufferLength The length of the pvBuffer.
 * @param ulAccessRequested The permissions that the calling task wants.
 *
 * @return pdTRUE if the calling task is authorized to access the buffer,
 *         pdFALSE otherwise.
 */
#if ( portUSING_MPU_WRAPPERS == 1 )
    BaseType_t xPortIsAuthorizedToAccessBuffer( const void * pvBuffer,
                                                uint32_t ulBufferLength,
                                                uint32_t ulAccessRequested ) PRIVILEGED_FUNCTION;
#endif

/**
 * @brief Checks if the calling task is authorized to access the given kernel object.
 *
 * @param lInternalIndexOfKernelObject The index of the kernel object in the kernel
 *                                     object handle pool.
 *
 * @return pdTRUE if the calling task is authorized to access the kernel object,
 *         pdFALSE otherwise.
 */
#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) )

    BaseType_t xPortIsAuthorizedToAccessKernelObject( int32_t lInternalIndexOfKernelObject ) PRIVILEGED_FUNCTION;

#endif

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* PORTABLE_H */
