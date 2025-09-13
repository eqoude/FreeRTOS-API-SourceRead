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

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h" must appear in source files before "include semphr.h"
#endif

#include "queue.h"

typedef QueueHandle_t SemaphoreHandle_t;

// 二进制信号量的队列长度：固定为1（二进制信号量仅支持0/1两种状态）
#define semBINARY_SEMAPHORE_QUEUE_LENGTH    ( ( uint8_t ) 1U )

// 信号量的队列元素长度：固定为0（信号量无需存储实际数据，仅通过计数表示状态）
#define semSEMAPHORE_QUEUE_ITEM_LENGTH      ( ( uint8_t ) 0U )

// 信号量释放操作的阻塞时间：固定为0（释放信号量时永不阻塞）
#define semGIVE_BLOCK_TIME                  ( ( TickType_t ) 0U )


/**
 * semphr.h
 * @code{c}
 * vSemaphoreCreateBinary( SemaphoreHandle_t xSemaphore );
 * @endcode
 *
 * 在许多使用场景中，使用“直接任务通知”替代二进制信号量会更快且更节省内存！
 * 参考：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 旧版本的 vSemaphoreCreateBinary() 宏现已被弃用，推荐使用 xSemaphoreCreateBinary() 函数。
 * 注意：
 * - 通过 vSemaphoreCreateBinary() 宏创建的二进制信号量，初始状态为“可获取”（第一次调用 take 会成功）；
 * - 而通过 xSemaphoreCreateBinary() 函数创建的二进制信号量，初始状态为“不可获取”（必须先调用 give 才能 take）。
 *
 * 该宏通过“现有队列机制”实现信号量功能：
 * - 队列长度为 1（因为是二进制信号量）；
 * - 数据大小为 0（因为无需存储实际数据，仅通过队列空/满状态判断信号量状态）。
 *
 * 此类信号量可用于：
 * - 任务间的纯同步；
 * - 中断与任务间的同步。
 * 信号量获取后无需“归还”，因此一个任务/中断可以持续“给出”信号量，而另一个任务可以持续“获取”信号量。
 * 出于这个原因，此类信号量不使用优先级继承机制。如果需要优先级继承功能，请使用 xSemaphoreCreateMutex()。
 *
 * @参数 xSemaphore：创建的信号量句柄，类型应为 SemaphoreHandle_t。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore = NULL;
 *
 * void vATask( void * pvParameters )
 * {
 *  // 在调用 vSemaphoreCreateBinary() 之前，信号量不可使用。
 *  // 这是一个宏，因此直接传入变量。
 *  vSemaphoreCreateBinary( xSemaphore );
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 信号量创建成功，可以使用了。
 *  }
 * }
 * @endcode
 * \defgroup vSemaphoreCreateBinary vSemaphoreCreateBinary
 * \ingroup Semaphores
 */
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )  // 仅当启用动态内存分配时有效
    #define vSemaphoreCreateBinary( xSemaphore )                                                                                     \
    do {                                                                                                                             \
        // 1. 创建一个长度为1、元素大小为0的二进制信号量队列
        ( xSemaphore ) = xQueueGenericCreate( ( UBaseType_t ) 1, semSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_BINARY_SEMAPHORE ); \
        // 2. 若创建成功，立即给出信号量（初始化为“可获取”状态）
        if( ( xSemaphore ) != NULL )                                                                                                 \
        {                                                                                                                            \
            ( void ) xSemaphoreGive( ( xSemaphore ) );                                                                               \
        }                                                                                                                            \
    } while( 0 )
#endif

/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateBinary( void );
 * @endcode
 *
 * 创建一个新的二进制信号量实例，并返回一个句柄，通过该句柄可引用这个新信号量。
 *
 * 在许多使用场景中，使用“直接任务通知”替代二进制信号量会更快且更节省内存！
 * 参考：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 在 FreeRTOS 底层实现中，二进制信号量需要一块内存来存储其数据结构：
 * 1. 若使用 xSemaphoreCreateBinary() 创建二进制信号量，所需内存会在该函数内部自动通过动态内存分配获取
 *    （详见：https://www.FreeRTOS.org/a00111.html）；
 * 2. 若使用 xSemaphoreCreateBinaryStatic() 创建，则需由应用开发者手动提供内存；
 *    因此，xSemaphoreCreateBinaryStatic() 可在不使用任何动态内存分配的情况下创建二进制信号量。
 *
 * 旧版本的 vSemaphoreCreateBinary() 宏现已被弃用，推荐使用本函数（xSemaphoreCreateBinary()）。
 * 注意：
 * - 通过 vSemaphoreCreateBinary() 宏创建的二进制信号量，初始状态为“可获取”（第一次调用 take 会成功）；
 * - 而通过 xSemaphoreCreateBinary() 函数创建的二进制信号量，初始状态为“不可获取”（必须先调用 give 才能 take）。
 *
 * 此类信号量可用于：
 * - 任务间的纯同步；
 * - 中断与任务间的同步。
 * 信号量获取后无需“归还”，因此一个任务/中断可以持续“给出”信号量，而另一个任务可以持续“获取”信号量。
 * 出于这个原因，此类信号量不使用优先级继承机制。如果需要优先级继承功能，请使用 xSemaphoreCreateMutex()。
 *
 * @返回值：
 *   - 若创建成功，返回新信号量的句柄（SemaphoreHandle_t 类型）；
 *   - 若无法分配存储信号量数据结构所需的内存（动态内存不足），返回 NULL。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore = NULL;
 *
 * void vATask( void * pvParameters )
 * {
 *  // 在调用 xSemaphoreCreateBinary() 之前，信号量不可使用。
 *  xSemaphore = xSemaphoreCreateBinary();
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 信号量创建成功，可以使用了。
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreCreateBinary xSemaphoreCreateBinary
 * \ingroup Semaphores
 */
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )  // 仅当启用动态内存分配时，该宏才有效
    // 宏定义：通过调用通用队列创建函数，配置为二进制信号量的特性
    #define xSemaphoreCreateBinary()    xQueueGenericCreate( \
        ( UBaseType_t ) 1,                  // 队列长度=1（二进制信号量仅支持0/1状态） \
        semSEMAPHORE_QUEUE_ITEM_LENGTH,     // 元素大小=0（信号量无需存储实际数据） \
        queueQUEUE_TYPE_BINARY_SEMAPHORE    // 队列类型=二进制信号量（底层标记用途） \
    )
#endif

/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateBinaryStatic( StaticSemaphore_t *pxSemaphoreBuffer );
 * @endcode
 *
 * 创建一个新的二进制信号量实例，并返回一个句柄，通过该句柄可引用这个新信号量。
 *
 * 注意：在许多使用场景中，使用“直接任务通知”替代二进制信号量会更快且更节省内存！
 * 参考：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 在 FreeRTOS 底层实现中，二进制信号量需要一块内存来存储其数据结构：
 * 1. 若使用 xSemaphoreCreateBinary() 创建，所需内存会在函数内部自动通过动态内存分配获取；
 * 2. 若使用 xSemaphoreCreateBinaryStatic() 创建，则需由应用开发者手动提供内存；
 *    因此，xSemaphoreCreateBinaryStatic() 可在不使用任何动态内存分配的情况下创建二进制信号量。
 *
 * 此类信号量可用于：
 * - 任务间的纯同步；
 * - 中断与任务间的同步。
 * 信号量获取后无需“归还”，因此一个任务/中断可以持续“给出”信号量，而另一个任务可以持续“获取”信号量。
 * 出于这个原因，此类信号量不使用优先级继承机制。如果需要优先级继承功能，请使用 xSemaphoreCreateMutex()。
 *
 * @参数 pxSemaphoreBuffer：必须指向一个 StaticSemaphore_t 类型的变量，
 *        该变量将用于存储信号量的数据结构，从而避免动态内存分配。
 *
 * @返回值：
 *   - 若信号量创建成功，返回新信号量的句柄；
 *   - 若 pxSemaphoreBuffer 为 NULL，则返回 NULL。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore = NULL;
 * StaticSemaphore_t xSemaphoreBuffer;  // 静态内存缓冲区
 *
 * void vATask( void * pvParameters )
 * {
 *  // 在调用 xSemaphoreCreateBinaryStatic() 之前，信号量不可使用。
 *  // 信号量的数据结构将存储在 xSemaphoreBuffer 变量中，
 *  // 其地址被传入函数。由于参数不为 NULL，函数不会尝试动态内存分配，
 *  // 因此函数不会返回 NULL。
 *  xSemaphore = xSemaphoreCreateBinaryStatic( &xSemaphoreBuffer );
 *
 *  // 任务的其余代码放在这里。
 * }
 * @endcode
 * \defgroup xSemaphoreCreateBinaryStatic xSemaphoreCreateBinaryStatic
 * \ingroup Semaphores
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )  // 仅当启用静态内存分配时有效
    // 宏定义：通过静态队列创建函数，配置为二进制信号量特性
    #define xSemaphoreCreateBinaryStatic( pxStaticSemaphore )    xQueueGenericCreateStatic( \
        ( UBaseType_t ) 1,                  // 队列长度=1（二进制信号量特性） \
        semSEMAPHORE_QUEUE_ITEM_LENGTH,     // 元素大小=0（无需存储数据） \
        NULL,                               // 数据缓冲区=NULL（元素大小为0，无需缓冲区） \
        ( pxStaticSemaphore ),              // 静态内存块（存储信号量控制结构） \
        queueQUEUE_TYPE_BINARY_SEMAPHORE    // 队列类型=二进制信号量 \
    )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * semphr.h
 * @code{c}
 * xSemaphoreTake(
 *                   SemaphoreHandle_t xSemaphore,
 *                   TickType_t xBlockTime
 *               );
 * @endcode
 *
 * 用于“获取信号量”的宏。该信号量必须是此前通过 xSemaphoreCreateBinary()、
 * xSemaphoreCreateMutex() 或 xSemaphoreCreateCounting() 创建的实例。
 *
 * @参数 xSemaphore：要获取的信号量句柄——该句柄在信号量创建时获取。
 *
 * @参数 xBlockTime：等待信号量可用的时间（单位：时钟节拍）。
 *                   - 可使用宏 portTICK_PERIOD_MS 将其转换为实际时间（如 100ms = 100 / portTICK_PERIOD_MS 个节拍）；
 *                   - 阻塞时间设为 0 时，仅“轮询”信号量（不阻塞，立即返回结果）；
 *                   - 阻塞时间设为 portMAX_DELAY 时，会“永久阻塞”（直到信号量可用），但需确保 FreeRTOSConfig.h 中 INCLUDE_vTaskSuspend == 1。
 *
 * @返回值：
 *   - pdTRUE：成功获取信号量（信号量计数递减，或互斥锁所有权转移到当前任务）；
 *   - pdFALSE：等待超时（xBlockTime 时间内信号量始终不可用）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore = NULL;
 *
 * // 任务1：创建信号量
 * void vATask( void * pvParameters )
 * {
 *  // 创建二进制信号量（用于保护共享资源）
 *  xSemaphore = xSemaphoreCreateBinary();
 * }
 *
 * // 任务2：使用信号量
 * void vAnotherTask( void * pvParameters )
 * {
 *  // ... 执行其他操作 ...
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 尝试获取信号量：若不可用，等待10个时钟节拍
 *      if( xSemaphoreTake( xSemaphore, ( TickType_t ) 10 ) == pdTRUE )
 *      {
 *          // 成功获取信号量，可安全访问共享资源
 *
 *          // ... 访问共享资源的逻辑 ...
 *
 *          // 完成共享资源访问后，释放信号量
 *          xSemaphoreGive( xSemaphore );
 *      }
 *      else
 *      {
 *          // 等待超时，未能获取信号量，无法安全访问共享资源
 *      }
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreTake xSemaphoreTake
 * \ingroup Semaphores
 */
// 宏定义：将信号量获取操作映射到底层队列信号量获取函数
#define xSemaphoreTake( xSemaphore, xBlockTime )    xQueueSemaphoreTake( ( xSemaphore ), ( xBlockTime ) )

/**
 * semphr.h
 * @code{c}
 * xSemaphoreTakeRecursive(
 *                          SemaphoreHandle_t xMutex,
 *                          TickType_t xBlockTime
 *                        );
 * @endcode
 *
 * 用于“递归获取互斥锁”的宏。该互斥锁必须是此前通过 xSemaphoreCreateRecursiveMutex() 创建的实例。
 *
 * 需在 FreeRTOSConfig.h 中设置 configUSE_RECURSIVE_MUTEXES == 1，此宏才能生效。
 *
 * 该宏禁止用于通过 xSemaphoreCreateMutex() 创建的普通互斥锁（仅支持递归互斥锁）。
 *
 * 递归互斥锁的特性：
 * 持有者任务可多次“获取”该互斥锁，且仅当持有者针对每一次“获取”都调用 xSemaphoreGiveRecursive() 释放后，
 * 互斥锁才会重新变为可用状态。例如：
 * - 若任务成功获取同一递归互斥锁 5 次，则需调用 xSemaphoreGiveRecursive() 恰好 5 次，
 *   该互斥锁才会对其他任务可用。
 *
 * @参数 xMutex：要获取的递归互斥锁句柄——该句柄在调用 xSemaphoreCreateRecursiveMutex() 时返回。
 *
 * @参数 xBlockTime：等待互斥锁可用的时间（单位：时钟节拍）。
 *                   - 可使用宏 portTICK_PERIOD_MS 将其转换为实际时间（如 500ms = 500 / portTICK_PERIOD_MS 个节拍）；
 *                   - 阻塞时间设为 0 时，仅“轮询”互斥锁状态（不阻塞，立即返回结果）；
 *                   - 若当前任务已持有该互斥锁，无论 xBlockTime 取值如何，xSemaphoreTakeRecursive() 都会立即返回成功（无需等待）。
 *
 * @返回值：
 *   - pdTRUE：成功获取互斥锁（首次获取时转移所有权，递归获取时仅递增计数）；
 *   - pdFALSE：等待超时（xBlockTime 时间内互斥锁始终被其他任务持有，未成功获取）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xMutex = NULL;
 *
 * // 任务1：创建递归互斥锁
 * void vATask( void * pvParameters )
 * {
 *  // 创建递归互斥锁（用于保护共享资源）
 *  xMutex = xSemaphoreCreateRecursiveMutex();
 * }
 *
 * // 任务2：使用递归互斥锁
 * void vAnotherTask( void * pvParameters )
 * {
 *  // ... 执行其他操作 ...
 *
 *  if( xMutex != NULL )
 *  {
 *      // 尝试获取递归互斥锁：若不可用，等待10个时钟节拍
 *      if( xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 ) == pdTRUE )
 *      {
 *          // 成功获取互斥锁，可安全访问共享资源
 *
 *          // ... 业务逻辑 ...
 *          // 因代码逻辑需要（如嵌套调用），再次递归获取同一互斥锁
 *          // 实际代码中这些调用不会是顺序执行（无意义），而是嵌套在复杂调用结构中
 *          xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );
 *          xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );
 *
 *          // 互斥锁已被“获取”3次，需释放3次才对其他任务可用
 *          // 实际代码中释放调用也会嵌套在复杂结构中，此处仅为演示
 *          xSemaphoreGiveRecursive( xMutex );
 *          xSemaphoreGiveRecursive( xMutex );
 *          xSemaphoreGiveRecursive( xMutex );
 *
 *          // 此时互斥锁已完全释放，其他任务可获取
 *      }
 *      else
 *      {
 *          // 等待超时，未能获取互斥锁，无法安全访问共享资源
 *      }
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreTakeRecursive xSemaphoreTakeRecursive
 * \ingroup Semaphores
 */
#if ( configUSE_RECURSIVE_MUTEXES == 1 )  // 仅当启用递归互斥锁功能时，该宏才生效
    // 宏定义：将递归获取互斥锁的操作，映射到底层队列递归互斥锁获取函数
    #define xSemaphoreTakeRecursive( xMutex, xBlockTime )    xQueueTakeMutexRecursive( ( xMutex ), ( xBlockTime ) )
#endif

/**
 * semphr.h
 * @code{c}
 * xSemaphoreGive( SemaphoreHandle_t xSemaphore );
 * @endcode
 *
 * 用于“释放信号量”的宏。该信号量必须是此前通过 xSemaphoreCreateBinary()、
 * xSemaphoreCreateMutex() 或 xSemaphoreCreateCounting() 创建的实例，且已通过 xSemaphoreTake() 获取。
 *
 * 该宏禁止在中断服务函数（ISR）中使用。若需在中断中释放信号量，应使用替代接口 xSemaphoreGiveFromISR()。
 *
 * 该宏也禁止用于通过 xSemaphoreCreateRecursiveMutex() 创建的递归互斥锁（递归互斥锁需用 xSemaphoreGiveRecursive() 释放）。
 *
 * @参数 xSemaphore：要释放的信号量句柄——该句柄在信号量创建时返回。
 *
 * @返回值：
 *   - pdTRUE：信号量释放成功（二进制/计数信号量计数递增，普通互斥锁归还所有权）；
 *   - pdFALSE：释放失败（信号量未被当前任务获取、递归互斥锁误用该宏、或信号量计数已达最大值）。
 *   （信号量基于队列实现，释放失败本质是“队列无空间存储释放消息”，通常因信号量未正确获取导致）
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore = NULL;
 *
 * void vATask( void * pvParameters )
 * {
 *  // 创建二进制信号量（用于保护共享资源）
 *  xSemaphore = xSemaphoreCreateBinary();  // 注：示例中原vSemaphoreCreateBinary已弃用，此处修正为新版接口
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 尝试释放未获取的信号量——预期失败（未take直接give）
 *      if( xSemaphoreGive( xSemaphore ) != pdTRUE )
 *      {
 *          // 此调用应失败，因释放信号量前必须先获取它
 *      }
 *
 *      // 获取信号量——不阻塞（若无法立即获取则放弃）
 *      if( xSemaphoreTake( xSemaphore, ( TickType_t ) 0 ) )
 *      {
 *          // 成功获取信号量，可安全访问共享资源
 *
 *          // ... 访问共享资源的逻辑 ...
 *
 *          // 完成共享资源访问，释放信号量
 *          if( xSemaphoreGive( xSemaphore ) != pdTRUE )
 *          {
 *              // 此调用不应失败，因能执行到此处说明已成功获取信号量
 *          }
 *      }
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreGive xSemaphoreGive
 * \ingroup Semaphores
 */
// 宏定义：将信号量释放操作映射到底层通用队列发送函数
#define xSemaphoreGive( xSemaphore )    xQueueGenericSend( \
    ( QueueHandle_t ) ( xSemaphore ),  // 信号量句柄强制转为队列句柄（信号量本质是队列） \
    NULL,                              // 数据缓冲区=NULL（信号量无需存储数据） \
    semGIVE_BLOCK_TIME,                // 阻塞时间=0（释放信号量永不阻塞） \
    queueSEND_TO_BACK                  // 发送位置=队列尾部（无实际意义，因无数据） \
)

/**
 * semphr.h
 * @code{c}
 * xSemaphoreGiveRecursive( SemaphoreHandle_t xMutex );
 * @endcode
 *
 * 用于“递归释放互斥锁”的宏。该互斥锁必须是此前通过 xSemaphoreCreateRecursiveMutex() 创建的实例。
 *
 * 需在 FreeRTOSConfig.h 中设置 configUSE_RECURSIVE_MUTEXES == 1，此宏才能生效。
 *
 * 该宏禁止用于通过 xSemaphoreCreateMutex() 创建的普通互斥锁（仅支持递归互斥锁）。
 *
 * 递归互斥锁的特性：
 * 持有者任务可多次“获取”该互斥锁，且仅当持有者针对每一次“获取”都调用 xSemaphoreGiveRecursive() 释放后，
 * 互斥锁才会重新变为可用状态。例如：
 * - 若任务成功获取同一递归互斥锁 5 次，则需调用 xSemaphoreGiveRecursive() 恰好 5 次，
 *   该互斥锁才会对其他任务可用。
 *
 * @参数 xMutex：要释放的递归互斥锁句柄——注意：注释中“xSemaphoreCreateMutex() 返回”为笔误，
 * 正确应为“xSemaphoreCreateRecursiveMutex() 返回的句柄”。
 *
 * @返回值：
 *   - pdTRUE：释放操作成功（递归计数递减；若计数减至 0，互斥锁完全释放，所有权归还系统）；
 *   - （注：宏定义未明确提及 pdFALSE，但底层实现中，非持有者释放、释放次数超过获取次数时会返回 pdFALSE）
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xMutex = NULL;
 *
 * // 任务1：创建递归互斥锁
 * void vATask( void * pvParameters )
 * {
 *  // 创建递归互斥锁（用于保护共享资源）
 *  xMutex = xSemaphoreCreateRecursiveMutex();
 * }
 *
 * // 任务2：使用递归互斥锁
 * void vAnotherTask( void * pvParameters )
 * {
 *  // ... 执行其他操作 ...
 *
 *  if( xMutex != NULL )
 *  {
 *      // 尝试获取递归互斥锁：若不可用，等待10个时钟节拍
 *      if( xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 ) == pdTRUE )
 *      {
 *          // 成功获取互斥锁，可安全访问共享资源
 *
 *          // ... 业务逻辑 ...
 *          // 因代码逻辑需要（如嵌套调用），再次递归获取同一互斥锁
 *          // 实际代码中这些调用不会是顺序执行（无意义），而是嵌套在复杂调用结构中
 *          xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );
 *          xSemaphoreTakeRecursive( xMutex, ( TickType_t ) 10 );
 *
 *          // 互斥锁已被“获取”3次，需释放3次才对其他任务可用
 *          // 实际代码中释放调用通常随函数调用栈 unwind 执行（如子函数返回时释放），此处仅为演示
 *          xSemaphoreGiveRecursive( xMutex );
 *          xSemaphoreGiveRecursive( xMutex );
 *          xSemaphoreGiveRecursive( xMutex );
 *
 *          // 此时互斥锁已完全释放，其他任务可获取
 *      }
 *      else
 *      {
 *          // 等待超时，未能获取互斥锁，无法安全访问共享资源
 *      }
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreGiveRecursive xSemaphoreGiveRecursive
 * \ingroup Semaphores
 */
#if ( configUSE_RECURSIVE_MUTEXES == 1 )  // 仅当启用递归互斥锁功能时，该宏才生效
    // 宏定义：将递归释放互斥锁的操作，映射到底层队列递归互斥锁释放函数
    #define xSemaphoreGiveRecursive( xMutex )    xQueueGiveMutexRecursive( ( xMutex ) )
#endif

/**
 * semphr.h
 * @code{c}
 * xSemaphoreGiveFromISR(
 *                        SemaphoreHandle_t xSemaphore,
 *                        BaseType_t *pxHigherPriorityTaskWoken
 *                    );
 * @endcode
 *
 * 用于“从中断服务函数（ISR）中释放信号量”的宏。该信号量必须是此前通过 
 * xSemaphoreCreateBinary() 或 xSemaphoreCreateCounting() 创建的实例。
 *
 * 禁止将该宏用于“互斥锁类型的信号量”（即通过 xSemaphoreCreateMutex() 创建的互斥锁）。
 *
 * 该宏可在中断服务函数（ISR）中使用。
 *
 * @参数 xSemaphore：要释放的信号量句柄——该句柄在信号量创建时返回。
 *
 * @参数 pxHigherPriorityTaskWoken：输出参数（指针）。
 *        - 若“释放信号量”导致某个任务解除阻塞，且该解除阻塞的任务优先级**高于当前运行任务**，
 *          xSemaphoreGiveFromISR() 会将 *pxHigherPriorityTaskWoken 设为 pdTRUE；
 *        - 若该参数被设为 pdTRUE，需在中断退出前请求任务上下文切换（确保高优先级任务能立即运行）。
 *
 * @返回值：
 *   - pdTRUE：信号量成功释放（二进制信号量计数从0→1，计数信号量计数+1，未超过最大值）；
 *   - errQUEUE_FULL：释放失败（通常因计数信号量已达最大计数，无法继续递增）。
 *
 * 使用示例：
 * @code{c}
 #define LONG_TIME 0xffff
 #define TICKS_TO_WAIT 10
 * SemaphoreHandle_t xSemaphore = NULL;
 *
 * // 周期性任务
 * void vATask( void * pvParameters )
 * {
 *  for( ;; )
 *  {
 *      // 该任务需每10个定时器节拍运行一次，信号量已在任务启动前创建
 *
 *      // 阻塞等待信号量（永久等待，直到中断释放）
 *      if( xSemaphoreTake( xSemaphore, LONG_TIME ) == pdTRUE )
 *      {
 *          // 到执行时间，执行任务逻辑
 *
 *          // ... 任务业务逻辑 ...
 *
 *          // 任务执行完成，回到循环顶部，继续阻塞等待信号量（中断同步场景无需手动释放信号量）
 *      }
 *  }
 * }
 *
 * // 定时器中断服务函数
 * void vTimerISR( void * pvParameters )
 * {
 * static uint8_t ucLocalTickCount = 0;  // 静态变量，记录定时器节拍数
 * static BaseType_t xHigherPriorityTaskWoken;  // 标记是否需上下文切换
 *
 *  // 定时器节拍触发（中断发生）
 *
 *  // ... 执行其他定时器相关操作 ...
 *
 *  // 判断是否到任务运行时间（每10个节拍释放一次信号量）
 *  xHigherPriorityTaskWoken = pdFALSE;  // 初始化：默认无需切换
 *  ucLocalTickCount++;
 *  if( ucLocalTickCount >= TICKS_TO_WAIT )
 *  {
 *      // 释放信号量，解除任务阻塞
 *      xSemaphoreGiveFromISR( xSemaphore, &xHigherPriorityTaskWoken );
 *
 *      // 重置计数，确保下次10个节拍后再次释放
 *      ucLocalTickCount = 0;
 *  }
 *
 *  // 若需切换任务，执行中断上下文切换（语法需参考具体端口的FreeRTOS实现）
 *  if( xHigherPriorityTaskWoken != pdFALSE )
 *  {
 *      // 此处需调用端口专属的上下文切换函数，例如：
 *      // portYIELD_FROM_ISR();  // 多数ARM Cortex-M端口支持该宏
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreGiveFromISR xSemaphoreGiveFromISR
 * \ingroup Semaphores
 */
// 宏定义：将中断中释放信号量的操作，映射到底层中断队列释放函数
#define xSemaphoreGiveFromISR( xSemaphore, pxHigherPriorityTaskWoken )    xQueueGiveFromISR( \
    ( QueueHandle_t ) ( xSemaphore ),  // 信号量句柄强制转为队列句柄（信号量本质是队列） \
    ( pxHigherPriorityTaskWoken )      // 传递“高优先级任务唤醒标记”指针 \
)

/**
 * semphr.h
 * @code{c}
 * xSemaphoreTakeFromISR(
 *                        SemaphoreHandle_t xSemaphore,
 *                        BaseType_t *pxHigherPriorityTaskWoken
 *                    );
 * @endcode
 *
 * 用于“从中断服务函数（ISR）中获取信号量”的宏。该信号量必须是此前通过 
 * xSemaphoreCreateBinary() 或 xSemaphoreCreateCounting() 创建的实例。
 *
 * 禁止将该宏用于“互斥锁类型的信号量”（即通过 xSemaphoreCreateMutex() 创建的互斥锁）。
 *
 * 该宏可在中断服务函数（ISR）中使用，但“从中断中获取信号量”并非常见操作——
 * 仅在特定场景下有用，例如：中断从“资源池”中获取资源时（信号量计数表示可用资源数量），
 * 可通过获取计数信号量确认资源是否可用。
 *
 * @参数 xSemaphore：要获取的信号量句柄——该句柄在信号量创建时返回。
 *
 * @参数 pxHigherPriorityTaskWoken：输出参数（指针）。
 *        - 若“获取信号量”导致某个任务解除阻塞（通常因信号量是计数类型，获取后仍有剩余计数，唤醒等待释放的任务），
 *          且该解除阻塞的任务优先级**高于当前运行任务**，xSemaphoreTakeFromISR() 会将 *pxHigherPriorityTaskWoken 设为 pdTRUE；
 *        - 若该参数被设为 pdTRUE，需在中断退出前请求任务上下文切换。
 *
 * @返回值：
 *   - pdTRUE：信号量成功获取（二进制信号量计数从1→0，计数信号量计数-1）；
 *   - pdFALSE：获取失败（信号量计数为0，无可用资源）。
 */
// 宏定义：将中断中获取信号量的操作，映射到底层中断队列接收函数
#define xSemaphoreTakeFromISR( xSemaphore, pxHigherPriorityTaskWoken )    xQueueReceiveFromISR(  \
    ( QueueHandle_t ) ( xSemaphore ),  \// 信号量句柄强制转为队列句柄 
    NULL,                              \// 数据缓冲区=NULL（信号量无需存储数据） 
    ( pxHigherPriorityTaskWoken )      \// 传递“高优先级任务唤醒标记”指针 
)

/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateMutex( void );
 * @endcode
 *
 * 创建一个新的“互斥锁类型信号量”实例，并返回该互斥锁的引用句柄（后续通过句柄操作互斥锁）。
 *
 * 在 FreeRTOS 底层实现中，互斥锁信号量需要一块内存来存储其数据结构：
 * - 若使用 xSemaphoreCreateMutex() 创建互斥锁，所需内存会在该函数内部通过**动态内存分配**自动获取（参考：https://www.FreeRTOS.org/a00111.html）；
 * - 若使用 xSemaphoreCreateMutexStatic() 创建互斥锁，需由应用开发者手动提供内存（无动态内存分配）。
 *
 * 通过该函数创建的互斥锁，仅可使用 xSemaphoreTake() 和 xSemaphoreGive() 宏进行操作；
 * 禁止使用 xSemaphoreTakeRecursive() 和 xSemaphoreGiveRecursive() 宏（递归操作接口仅适用于递归互斥锁）。
 *
 * 此类互斥锁支持“优先级继承机制”，因此**获取互斥锁的任务必须在不再需要时释放互斥锁**（否则会导致死锁，其他任务永久无法获取）。
 *
 * 互斥锁类型信号量**禁止在中断服务函数（ISR）中使用**。
 *
 * 若需实现“纯同步场景”（如一个任务/中断负责释放信号量，另一个任务负责获取），或需在中断中使用信号量，
 * 可参考 xSemaphoreCreateBinary()（二进制信号量创建接口）。
 *
 * @返回值：
 *   - 成功：返回创建的互斥锁句柄（非 NULL）；
 *   - 失败：返回 NULL（因堆内存不足，无法分配互斥锁数据结构）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore;  // 定义互斥锁句柄
 *
 * void vATask( void * pvParameters )
 * {
 *  // 互斥锁使用前必须先创建，该宏直接传入句柄变量
 *  xSemaphore = xSemaphoreCreateMutex();
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 互斥锁创建成功，可开始使用（如 xSemaphoreTake() 获取、xSemaphoreGive() 释放）
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreCreateMutex xSemaphoreCreateMutex
 * \ingroup Semaphores
 */
#if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configUSE_MUTEXES == 1 ) )
    // 宏定义：动态创建互斥锁，映射到底层队列创建函数（指定队列类型为互斥锁）
    #define xSemaphoreCreateMutex()    xQueueCreateMutex( queueQUEUE_TYPE_MUTEX )
#endif

/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateMutexStatic( StaticSemaphore_t *pxMutexBuffer );
 * @endcode
 *
 * 创建一个新的“互斥锁类型信号量”实例，并返回该互斥锁的引用句柄（后续通过句柄操作互斥锁）。
 *
 * 在 FreeRTOS 底层实现中，互斥锁信号量需要一块内存来存储其数据结构：
 * - 若使用 xSemaphoreCreateMutex() 创建互斥锁，所需内存会在该函数内部通过动态内存分配自动获取（参考：https://www.FreeRTOS.org/a00111.html）；
 * - 若使用 xSemaphoreCreateMutexStatic() 创建互斥锁，需由应用开发者手动提供内存（通过 pxMutexBuffer 参数传入），因此**无需动态内存分配**。
 *
 * 通过该函数创建的互斥锁，仅可使用 xSemaphoreTake() 和 xSemaphoreGive() 宏进行操作；
 * 禁止使用 xSemaphoreTakeRecursive() 和 xSemaphoreGiveRecursive() 宏（递归操作接口仅适用于递归互斥锁）。
 *
 * 此类互斥锁支持“优先级继承机制”，因此**获取互斥锁的任务必须在不再需要时释放互斥锁**（否则会导致死锁，其他任务永久无法获取）。
 *
 * 互斥锁类型信号量**禁止在中断服务函数（ISR）中使用**。
 *
 * 若需实现“纯同步场景”（如一个任务/中断负责释放信号量，另一个任务负责获取），或需在中断中使用信号量，
 * 可参考 xSemaphoreCreateBinary()（二进制信号量创建接口）。
 *
 * @参数 pxMutexBuffer：必须指向一个 StaticSemaphore_t 类型的变量，该变量将用于存储互斥锁的数据结构，
 * 从而避免动态内存分配（开发者需确保该变量的生命周期覆盖互斥锁的使用周期）。
 *
 * @返回值：
 *   - 成功：返回创建的互斥锁句柄（非 NULL）；
 *   - 失败：返回 NULL（仅当 pxMutexBuffer 为 NULL 时，因无内存存储互斥锁结构）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore;        // 定义互斥锁句柄
 * StaticSemaphore_t xMutexBuffer;      // 静态分配互斥锁内存（全局/静态变量，避免栈溢出）
 *
 * void vATask( void * pvParameters )
 * {
 *  // 互斥锁使用前必须先创建，传入静态内存缓冲区地址，无动态内存分配
 *  xSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer );
 *
 *  // 因未使用动态内存分配，只要 pxMutexBuffer 非 NULL，xSemaphore 就不会为 NULL，无需额外检查
 * }
 * @endcode
 * \defgroup xSemaphoreCreateMutexStatic xSemaphoreCreateMutexStatic
 * \ingroup Semaphores
 */
#if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configUSE_MUTEXES == 1 ) )
    // 宏定义：静态创建互斥锁，映射到底层静态队列创建函数（指定队列类型+静态内存缓冲区）
    #define xSemaphoreCreateMutexStatic( pxMutexBuffer )    xQueueCreateMutexStatic( queueQUEUE_TYPE_MUTEX, ( pxMutexBuffer ) )
#endif


/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateRecursiveMutex( void );
 * @endcode
 *
 * 创建一个新的“递归互斥锁类型信号量”实例，并返回该递归互斥锁的引用句柄（后续通过句柄操作互斥锁）。
 *
 * 在 FreeRTOS 底层实现中，递归互斥锁需要一块内存来存储其数据结构：
 * - 若使用 xSemaphoreCreateRecursiveMutex() 创建，所需内存会在该函数内部通过**动态内存分配**自动获取（参考：https://www.FreeRTOS.org/a00111.html）；
 * - 若使用 xSemaphoreCreateRecursiveMutexStatic() 创建，需由应用开发者手动提供内存（无动态内存分配）。
 *
 * 通过该宏创建的递归互斥锁，仅可使用 xSemaphoreTakeRecursive() 和 xSemaphoreGiveRecursive() 宏进行操作；
 * 禁止使用 xSemaphoreTake() 和 xSemaphoreGive() 宏（普通互斥锁操作接口不支持递归计数）。
 *
 * 递归互斥锁的核心特性：持有者任务可多次“获取”该互斥锁，且仅当持有者针对每一次“获取”都调用 xSemaphoreGiveRecursive() 释放后，
 * 互斥锁才会重新变为可用状态。例如：
 * - 若任务成功获取同一递归互斥锁 5 次，则需调用 xSemaphoreGiveRecursive() 恰好 5 次，
 *   该互斥锁才会对其他任务可用。
 *
 * 此类互斥锁支持“优先级继承机制”，因此**获取互斥锁的任务必须在不再需要时完全释放互斥锁**（即“获取次数=释放次数”），
 * 否则会导致死锁（其他任务永久无法获取）。
 *
 * 递归互斥锁类型信号量**禁止在中断服务函数（ISR）中使用**。
 *
 * 若需实现“纯同步场景”（如一个任务/中断负责释放信号量，另一个任务负责获取），或需在中断中使用信号量，
 * 可参考 xSemaphoreCreateBinary()（二进制信号量创建接口）。
 *
 * @返回值：
 *   - 成功：返回创建的递归互斥锁句柄（非 NULL）；
 *   - 失败：返回 NULL（因堆内存不足，无法分配递归互斥锁数据结构）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore;  // 定义递归互斥锁句柄
 *
 * void vATask( void * pvParameters )
 * {
 *  // 递归互斥锁使用前必须先创建，该宏无需传入参数（动态分配内存）
 *  xSemaphore = xSemaphoreCreateRecursiveMutex();
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 递归互斥锁创建成功，可开始使用（如 xSemaphoreTakeRecursive() 获取、xSemaphoreGiveRecursive() 释放）
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreCreateRecursiveMutex xSemaphoreCreateRecursiveMutex
 * \ingroup Semaphores
 */
#if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configUSE_RECURSIVE_MUTEXES == 1 ) )
    // 宏定义：动态创建递归互斥锁，映射到底层队列创建函数（指定队列类型为递归互斥锁）
    #define xSemaphoreCreateRecursiveMutex()    xQueueCreateMutex( queueQUEUE_TYPE_RECURSIVE_MUTEX )
#endif

/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic( StaticSemaphore_t *pxMutexBuffer );
 * @endcode
 *
 * 创建一个新的“递归互斥锁类型信号量”实例，并返回该递归互斥锁的引用句柄（后续通过句柄操作互斥锁）。
 *
 * 在 FreeRTOS 底层实现中，递归互斥锁需要一块内存来存储其数据结构：
 * - 若使用 xSemaphoreCreateRecursiveMutex() 创建，所需内存会在该函数内部通过动态内存分配自动获取（参考：https://www.FreeRTOS.org/a00111.html）；
 * - 若使用 xSemaphoreCreateRecursiveMutexStatic() 创建，需由应用开发者手动提供内存（通过 pxMutexBuffer 参数传入），因此**无需动态内存分配**。
 *
 * 通过该宏创建的递归互斥锁，仅可使用 xSemaphoreTakeRecursive() 和 xSemaphoreGiveRecursive() 宏进行操作；
 * 禁止使用 xSemaphoreTake() 和 xSemaphoreGive() 宏（普通互斥锁操作接口不支持递归计数）。
 *
 * 递归互斥锁的核心特性：持有者任务可多次“获取”该互斥锁，且仅当持有者针对每一次“获取”都调用 xSemaphoreGiveRecursive() 释放后，
 * 互斥锁才会重新变为可用状态。例如：
 * - 若任务成功获取同一递归互斥锁 5 次，则需调用 xSemaphoreGiveRecursive() 恰好 5 次，
 *   该互斥锁才会对其他任务可用。
 *
 * 此类互斥锁支持“优先级继承机制”，因此**获取互斥锁的任务必须在不再需要时完全释放互斥锁**（即“获取次数=释放次数”），
 * 否则会导致死锁（其他任务永久无法获取）。
 *
 * 递归互斥锁类型信号量**禁止在中断服务函数（ISR）中使用**。
 *
 * 若需实现“纯同步场景”（如一个任务/中断负责释放信号量，另一个任务负责获取），或需在中断中使用信号量，
 * 可参考 xSemaphoreCreateBinary()（二进制信号量创建接口）。
 *
 * @参数 pxMutexBuffer：必须指向一个 StaticSemaphore_t 类型的变量，该变量将用于存储递归互斥锁的数据结构，
 * 从而避免动态内存分配（开发者需确保该变量的生命周期覆盖递归互斥锁的使用周期）。
 *
 * @返回值：
 *   - 成功：返回创建的递归互斥锁句柄（非 NULL）；
 *   - 失败：返回 NULL（仅当 pxMutexBuffer 为 NULL 时，因无内存存储递归互斥锁结构）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore;        // 定义递归互斥锁句柄
 * StaticSemaphore_t xMutexBuffer;      // 静态分配递归互斥锁内存（全局/静态变量，避免栈溢出）
 *
 * void vATask( void * pvParameters )
 * {
 *  // 递归互斥锁使用前必须先创建，传入静态内存缓冲区地址，无动态内存分配
 *  xSemaphore = xSemaphoreCreateRecursiveMutexStatic( &xMutexBuffer );
 *
 *  // 因未使用动态内存分配，只要 pxMutexBuffer 非 NULL，xSemaphore 就不会为 NULL，无需额外检查
 * }
 * @endcode
 * \defgroup xSemaphoreCreateRecursiveMutexStatic xSemaphoreCreateRecursiveMutexStatic
 * \ingroup Semaphores
 */
#if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configUSE_RECURSIVE_MUTEXES == 1 ) )
    // 宏定义：静态创建递归互斥锁，映射到底层静态队列创建函数（指定队列类型+静态内存缓冲区）
    #define xSemaphoreCreateRecursiveMutexStatic( pxStaticSemaphore )    xQueueCreateMutexStatic( queueQUEUE_TYPE_RECURSIVE_MUTEX, ( pxStaticSemaphore ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateCounting( UBaseType_t uxMaxCount, UBaseType_t uxInitialCount );
 * @endcode
 *
 * 创建一个新的“计数信号量”实例，并返回该计数信号量的引用句柄（后续通过句柄操作信号量）。
 *
 * 注意：在许多使用场景中，使用“任务直接通知（direct to task notification）”替代计数信号量，
 * 会更高效且更节省内存！参考文档：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 在 FreeRTOS 底层实现中，计数信号量需要一块内存来存储其数据结构：
 * - 若使用 xSemaphoreCreateCounting() 创建，所需内存会在该函数内部通过**动态内存分配**自动获取（参考：https://www.FreeRTOS.org/a00111.html）；
 * - 若使用 xSemaphoreCreateCountingStatic() 创建，应用开发者可手动提供内存（无动态内存分配）。
 *
 * 计数信号量通常用于两种场景：
 *
 * 1. 事件计数（Counting Events）
 *    - 场景逻辑：事件触发时，事件处理函数（如中断）调用“释放信号量”（递增计数）；
 *      处理任务调用“获取信号量”（递减计数），每次获取对应处理一个事件。
 *    - 计数含义：当前计数 = 已发生但未处理的事件数量。
 *    - 初始值建议：设为 0（创建时无未处理事件）。
 *
 * 2. 资源管理（Resource Management）
 *    - 场景逻辑：计数表示“可用资源数量”；任务获取信号量（递减计数）以占用资源，
 *      释放信号量（递增计数）以归还资源；计数为 0 时无可用资源。
 *    - 计数含义：当前计数 = 剩余可用资源数量。
 *    - 初始值建议：设为“最大计数”（创建时所有资源均可用）。
 *
 * @参数 uxMaxCount：信号量允许的**最大计数**。当计数达到该值时，无法继续释放信号量（释放会返回失败）。
 *
 * @参数 uxInitialCount：信号量创建时的**初始计数**。需满足 0 ≤ uxInitialCount ≤ uxMaxCount（否则创建失败）。
 *
 * @返回值：
 *   - 成功：返回计数信号量句柄（非 NULL），可用于后续 xSemaphoreTake()/xSemaphoreGive() 操作；
 *   - 失败：返回 NULL（因堆内存不足，或初始计数超出最大计数范围）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore;
 *
 * void vATask( void * pvParameters )
 * {
 *  SemaphoreHandle_t xSemaphore = NULL;
 *
 *  // 计数信号量使用前必须先创建：最大计数10，初始计数0（用于事件计数场景）
 *  xSemaphore = xSemaphoreCreateCounting( 10, 0 );
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 信号量创建成功，可开始使用
 *  }
 * }
 * @endcode
 * \defgroup xSemaphoreCreateCounting xSemaphoreCreateCounting
 * \ingroup Semaphores
 */
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    // 宏定义：动态创建计数信号量，映射到底层计数信号量队列创建函数
    #define xSemaphoreCreateCounting( uxMaxCount, uxInitialCount )    xQueueCreateCountingSemaphore( ( uxMaxCount ), ( uxInitialCount ) )
#endif

/**
 * semphr.h
 * @code{c}
 * SemaphoreHandle_t xSemaphoreCreateCountingStatic( UBaseType_t uxMaxCount, UBaseType_t uxInitialCount, StaticSemaphore_t *pxSemaphoreBuffer );
 * @endcode
 *
 * 创建一个新的“计数信号量”实例，并返回该计数信号量的引用句柄（后续通过句柄操作信号量）。
 *
 * 注意：在许多使用场景中，使用“任务直接通知（direct to task notification）”替代计数信号量，
 * 会更高效且更节省内存！参考文档：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 在 FreeRTOS 底层实现中，计数信号量需要一块内存来存储其数据结构：
 * - 若使用 xSemaphoreCreateCounting() 创建，所需内存会在该函数内部通过动态内存分配自动获取（参考：https://www.FreeRTOS.org/a00111.html）；
 * - 若使用 xSemaphoreCreateCountingStatic() 创建，需由应用开发者手动提供内存（通过 pxSemaphoreBuffer 参数传入），因此**无需动态内存分配**。
 *
 * 计数信号量通常用于两种场景（同动态创建宏）：
 * 1. 事件计数（Counting Events）；
 * 2. 资源管理（Resource Management）。
 *
 * @参数 uxMaxCount：信号量允许的**最大计数**。当计数达到该值时，无法继续释放信号量。
 *
 * @参数 uxInitialCount：信号量创建时的**初始计数**。需满足 0 ≤ uxInitialCount ≤ uxMaxCount（否则创建失败）。
 *
 * @参数 pxSemaphoreBuffer：必须指向一个 StaticSemaphore_t 类型的变量，该变量将用于存储计数信号量的数据结构，
 * 从而避免动态内存分配（开发者需确保该变量的生命周期覆盖信号量的使用周期）。
 *
 * @返回值：
 *   - 成功：返回计数信号量句柄（非 NULL）；
 *   - 失败：返回 NULL（仅当 pxSemaphoreBuffer 为 NULL，或初始计数超出最大计数范围时）。
 *
 * 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore;
 * StaticSemaphore_t xSemaphoreBuffer;  // 静态分配信号量内存（全局/静态变量）
 *
 * void vATask( void * pvParameters )
 * {
 *  SemaphoreHandle_t xSemaphore = NULL;
 *
 *  // 静态创建计数信号量：最大计数10，初始计数0，传入静态内存缓冲区地址
 *  xSemaphore = xSemaphoreCreateCountingStatic( 10, 0, &xSemaphoreBuffer );
 *
 *  // 因未使用动态内存分配，只要 pxSemaphoreBuffer 非 NULL 且参数合法，xSemaphore 就不会为 NULL，无需额外检查
 * }
 * @endcode
 * \defgroup xSemaphoreCreateCountingStatic xSemaphoreCreateCountingStatic
 * \ingroup Semaphores
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    // 宏定义：静态创建计数信号量，映射到底层静态计数信号量队列创建函数
    #define xSemaphoreCreateCountingStatic( uxMaxCount, uxInitialCount, pxSemaphoreBuffer )    xQueueCreateCountingSemaphoreStatic( ( uxMaxCount ), ( uxInitialCount ), ( pxSemaphoreBuffer ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * semphr.h
 * @code{c}
 * void vSemaphoreDelete( SemaphoreHandle_t xSemaphore );
 * @endcode
 *
 * 删除一个已创建的信号量。调用此函数时必须格外谨慎，例如：
 * 若互斥锁类型的信号量正被某个任务持有（未释放），则禁止删除该互斥锁（会导致死锁或资源泄漏）。
 *
 * @参数 xSemaphore：待删除的信号量句柄——该句柄是信号量创建时（如 xSemaphoreCreateMutex()、xSemaphoreCreateCounting()）返回的有效句柄。
 *
 * \defgroup vSemaphoreDelete vSemaphoreDelete
 * \ingroup Semaphores
 */
// 宏定义：将信号量删除操作映射到底层队列删除函数（信号量本质是特殊队列）
#define vSemaphoreDelete( xSemaphore )    vQueueDelete( ( QueueHandle_t ) ( xSemaphore ) )

/**
 * semphr.h
 * @code{c}
 * TaskHandle_t xSemaphoreGetMutexHolder( SemaphoreHandle_t xMutex );
 * @endcode
 *
 * 若 xMutex 确实是“互斥锁类型信号量”，则返回当前持有该互斥锁的任务句柄；
 * 若 xMutex 不是互斥锁类型信号量，或该互斥锁处于可用状态（无任务持有），则返回 NULL。
 *
 * 注意：此函数可用于判断“调用该函数的任务是否为互斥锁持有者”，但**不适合用于确定互斥锁持有者的具体身份**——
 * 因为在函数返回结果与对结果进行判断的这段时间内，互斥锁的持有者可能已发生变化（例如原持有者释放了互斥锁，新任务获取了互斥锁）。
 */
#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )
    // 宏定义：将“获取互斥锁持有者”操作映射到底层队列获取互斥锁持有者函数（互斥锁本质是特殊队列）
    #define xSemaphoreGetMutexHolder( xSemaphore )    xQueueGetMutexHolder( ( xSemaphore ) )
#endif

/**
 * semphr.h
 * @code{c}
 * TaskHandle_t xSemaphoreGetMutexHolderFromISR( SemaphoreHandle_t xMutex );
 * @endcode
 *
 * 若 xMutex 确实是“互斥锁类型信号量”，则返回当前持有该互斥锁的任务句柄；
 * 若 xMutex 不是互斥锁类型信号量，或该互斥锁处于可用状态（无任务持有），则返回 NULL。
 *
 * （注：此宏专为中断服务函数（ISR）设计，确保在中断上下文下安全获取互斥锁持有者信息，无任务上下文切换风险）
 */
#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )
    // 宏定义：将“从中断获取互斥锁持有者”操作映射到底层中断安全的队列获取互斥锁持有者函数
    #define xSemaphoreGetMutexHolderFromISR( xSemaphore )    xQueueGetMutexHolderFromISR( ( xSemaphore ) )
#endif

/**
 * semphr.h
 * @code{c}
 * UBaseType_t uxSemaphoreGetCount( SemaphoreHandle_t xSemaphore );
 * @endcode
 *
 * 若信号量是计数信号量，uxSemaphoreGetCount() 返回其当前计数；
 * 若信号量是二进制信号量，当信号量可用时返回 1，不可用时返回 0。
 *
 * （注：此宏适用于任务上下文，可查询各类信号量的当前状态）
 */
// 宏定义：将信号量计数查询映射到底层队列消息数查询（信号量本质是特殊队列，计数=消息数）
#define uxSemaphoreGetCount( xSemaphore )           uxQueueMessagesWaiting( ( QueueHandle_t ) ( xSemaphore ) )

/**
 * semphr.h
 * @code{c}
 * UBaseType_t uxSemaphoreGetCountFromISR( SemaphoreHandle_t xSemaphore );
 * @endcode
 *
 * 若信号量是计数信号量，uxSemaphoreGetCountFromISR() 返回其当前计数；
 * 若信号量是二进制信号量，当信号量可用时返回 1，不可用时返回 0。
 *
 * （注：此宏专为中断服务函数（ISR）设计，确保在中断上下文下安全查询信号量计数）
 */
// 宏定义：将中断中的信号量计数查询映射到底层中断安全的队列消息数查询
#define uxSemaphoreGetCountFromISR( xSemaphore )    uxQueueMessagesWaitingFromISR( ( QueueHandle_t ) ( xSemaphore ) )

/**
 * semphr.h
 * @code{c}
 * BaseType_t xSemaphoreGetStaticBuffer( SemaphoreHandle_t xSemaphore,
 *                                       StaticSemaphore_t ** ppxSemaphoreBuffer );
 * @endcode
 *
 * 获取静态创建的二进制信号量、计数信号量或互斥锁的底层数据结构缓冲区指针。
 * 该缓冲区与信号量创建时传入的静态内存缓冲区是同一个。
 *
 * @参数 xSemaphore：需要获取其缓冲区的信号量句柄（必须是静态创建的信号量）。
 *
 * @参数 ppxSemaphoreBuffer：用于返回信号量数据结构缓冲区的指针（二级指针，存储静态缓冲区地址）。
 *
 * @返回值：
 *   - pdTRUE：成功获取缓冲区指针（信号量为静态创建且句柄有效）；
 *   - pdFALSE：获取失败（信号量为动态创建、句柄无效或参数错误）。
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    // 宏定义：将静态信号量缓冲区查询映射到底层队列静态缓冲区查询函数
    // （信号量本质是特殊队列，静态缓冲区即队列的静态内存）
    #define xSemaphoreGetStaticBuffer( xSemaphore, ppxSemaphoreBuffer )    xQueueGenericGetStaticBuffers( ( QueueHandle_t ) ( xSemaphore ), NULL, ( ppxSemaphoreBuffer ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

#endif /* SEMAPHORE_H */
