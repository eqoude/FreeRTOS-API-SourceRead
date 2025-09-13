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


#ifndef QUEUE_H
#define QUEUE_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h" must appear in source files before "include queue.h"
#endif

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

#include "task.h"

/**
 * 用于引用队列的类型。例如，调用 xQueueCreate() 会返回一个 QueueHandle_t 变量，
 * 该变量随后可作为参数传递给 xQueueSend()、xQueueReceive() 等函数。
 */
struct QueueDefinition; /* 使用旧的命名约定，以避免破坏内核感知调试器。 */
typedef struct QueueDefinition   * QueueHandle_t;  // 队列句柄类型：指向 QueueDefinition 结构体的指针

/**
 * 用于引用队列集的类型。例如，调用 xQueueCreateSet() 会返回一个 QueueSetHandle_t 变量，
 * 该变量随后可作为参数传递给 xQueueSelectFromSet()、xQueueAddToSet() 等函数。
 */
typedef struct QueueDefinition   * QueueSetHandle_t;  // 队列集句柄类型：指向 QueueDefinition 结构体的指针

/**
 * 队列集可以包含队列和信号量，因此 QueueSetMemberHandle_t 被定义为一种通用类型，
 * 用于参数或返回值既可以是 QueueHandle_t（队列句柄）也可以是 SemaphoreHandle_t（信号量句柄）的场景。
 */
typedef struct QueueDefinition   * QueueSetMemberHandle_t;  // 队列集成员句柄类型：指向 QueueDefinition 结构体的指针

/* 仅用于内部使用。 */
#define queueSEND_TO_BACK                     ( ( BaseType_t ) 0 )  // 发送到队列尾部（FIFO模式）
#define queueSEND_TO_FRONT                    ( ( BaseType_t ) 1 )  // 发送到队列头部（优先级模式）
#define queueOVERWRITE                        ( ( BaseType_t ) 2 )  // 覆盖队列中已有数据（仅适用于长度为1的队列）

/* 仅用于内部使用。这些定义*必须*与 queue.c 中的定义保持一致。 */
#define queueQUEUE_TYPE_BASE                  ( ( uint8_t ) 0U )  // 基础队列类型（普通数据队列）
#define queueQUEUE_TYPE_SET                   ( ( uint8_t ) 0U )  // 队列集类型（与基础队列共享类型值，通过其他成员区分）
#define queueQUEUE_TYPE_MUTEX                 ( ( uint8_t ) 1U )  // 互斥锁类型
#define queueQUEUE_TYPE_COUNTING_SEMAPHORE    ( ( uint8_t ) 2U )  // 计数信号量类型
#define queueQUEUE_TYPE_BINARY_SEMAPHORE      ( ( uint8_t ) 3U )  // 二进制信号量类型
#define queueQUEUE_TYPE_RECURSIVE_MUTEX       ( ( uint8_t ) 4U )  // 递归互斥锁类型

/**
 * queue.h
 * @code{c}
 * QueueHandle_t xQueueCreate(
 *                            UBaseType_t uxQueueLength,
 *                            UBaseType_t uxItemSize
 *                        );
 * @endcode
 *
 * 创建一个新的队列实例，并返回一个句柄，通过该句柄可以引用新创建的队列。
 *
 * 在 FreeRTOS 实现内部，队列使用两块内存。第一块用于存储队列的数据结构。
 * 第二块用于存储放入队列的项目。如果使用 xQueueCreate() 创建队列，那么这两块内存
 * 都会在 xQueueCreate() 函数内部自动进行动态分配（参见 https://www.FreeRTOS.org/a00111.html）。
 * 如果使用 xQueueCreateStatic() 创建队列，则应用程序编写者必须提供队列将要使用的内存。
 * 因此，xQueueCreateStatic() 允许在不使用任何动态内存分配的情况下创建队列。
 *
 * 更多信息：https://www.FreeRTOS.org/Embedded-RTOS-Queues.html
 *
 * @param uxQueueLength 队列可以容纳的最大项目数量。
 *
 * @param uxItemSize 队列中每个项目所需的字节数。项目通过复制方式入队，而非引用，
 * 因此这是每个入队项目将要复制的字节数。队列中的每个项目必须具有相同的大小。
 *
 * @return 如果队列创建成功，则返回指向新创建队列的句柄。如果队列无法创建，则返回 0。
 *
 * 使用示例：
 * @code{c}
 * struct AMessage
 * {
 *  char ucMessageID;
 *  char ucData[ 20 ];
 * };
 *
 * void vATask( void *pvParameters )
 * {
 * QueueHandle_t xQueue1, xQueue2;
 *
 *  // 创建一个能够容纳 10 个 uint32_t 类型值的队列。
 *  xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );
 *  if( xQueue1 == 0 )
 *  {
 *      // 队列未创建，不得使用。
 *  }
 *
 *  // 创建一个能够容纳 10 个指向 AMessage 结构体的指针的队列。
 *  // 这些指针应通过指针传递，因为它们包含大量数据。
 *  xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );
 *  if( xQueue2 == 0 )
 *  {
 *      // 队列未创建，不得使用。
 *  }
 *
 *  // ... 任务的其余代码。
 * }
 * @endcode
 * \defgroup xQueueCreate xQueueCreate
 * \ingroup QueueManagement
 */
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    #define xQueueCreate( uxQueueLength, uxItemSize )    xQueueGenericCreate( ( uxQueueLength ), ( uxItemSize ), ( queueQUEUE_TYPE_BASE ) )
#endif

/**
 * queue.h
 * @code{c}
 * QueueHandle_t xQueueCreateStatic(
 *                            UBaseType_t uxQueueLength,
 *                            UBaseType_t uxItemSize,
 *                            uint8_t *pucQueueStorage,
 *                            StaticQueue_t *pxQueueBuffer
 *                        );
 * @endcode
 *
 * 创建一个新的队列实例，并返回一个句柄，通过该句柄可引用新创建的队列。
 *
 * 在 FreeRTOS 实现内部，队列使用两块内存：第一块用于存储队列的数据结构（控制信息），
 * 第二块用于存储放入队列的实际项目数据。若使用 xQueueCreate() 创建队列，这两块内存
 * 会在函数内部自动通过动态内存分配获取（详见 https://www.FreeRTOS.org/a00111.html）；
 * 若使用 xQueueCreateStatic()，则需由应用程序开发者手动提供队列所需的内存，
 * 因此该函数可在不依赖任何动态内存分配的场景下创建队列。
 *
 * 更多参考：https://www.FreeRTOS.org/Embedded-RTOS-Queues.html
 *
 * @param uxQueueLength 队列可容纳的最大项目数量（队列深度）。
 *
 * @param uxItemSize 队列中每个项目所需的字节数。项目通过“复制”方式入队（而非引用），
 * 因此该参数表示每个入队项目需要复制的字节数。队列中所有项目的大小必须一致。
 *
 * @param pucQueueStorage 若 uxItemSize 不为 0（即队列需存储实际数据），则该参数必须指向
 * 一个 uint8_t 类型数组，数组大小至少为 (uxQueueLength * uxItemSize) 字节（确保能容纳
 * 队列最大容量的所有项目）；若 uxItemSize 为 0（如队列仅用作同步，无需存储数据），则该参数可设为 NULL。
 *
 * @param pxQueueBuffer 必须指向一个 StaticQueue_t 类型的变量，该变量将用于存储队列的数据结构
 * （如队列长度、项目大小、阻塞任务列表等控制信息）。
 *
 * @return 若队列创建成功，返回指向该队列的句柄（QueueHandle_t 类型）；若 pxQueueBuffer 为 NULL，
 * 则返回 NULL（因无法存储队列数据结构，创建失败）。
 *
 * 使用示例：
 * @code{c}
 * struct AMessage
 * {
 *  char ucMessageID;  // 消息ID
 *  char ucData[ 20 ]; // 消息数据
 * };
 *
 #define QUEUE_LENGTH 10          // 队列最大容量（10个项目）
 #define ITEM_SIZE sizeof( uint32_t ) // 每个项目大小（uint32_t 类型，4字节）
 *
 * // xQueueBuffer：用于存储队列的数据结构（控制信息）
 * StaticQueue_t xQueueBuffer;
 *
 * // ucQueueStorage：用于存储队列的实际项目数据，大小 = 队列长度 × 项目大小
 * uint8_t ucQueueStorage[ QUEUE_LENGTH * ITEM_SIZE ];
 *
 * void vATask( void *pvParameters )
 * {
 *  QueueHandle_t xQueue1;
 *
 *  // 创建一个可容纳 10 个 uint32_t 类型数据的队列
 *  xQueue1 = xQueueCreateStatic( 
 *                          QUEUE_LENGTH,          // 队列最大容量
 *                          ITEM_SIZE,             // 每个项目大小
 *                          &( ucQueueStorage[ 0 ] ), // 项目数据存储缓冲区
 *                          &xQueueBuffer );       // 队列数据结构存储区
 *
 *  // 因未使用动态内存分配，只要 pxQueueBuffer 和 pucQueueStorage 有效，队列必创建成功
 *  // 因此 xQueue1 此时是指向有效队列的句柄
 *
 *  // ... 任务其余逻辑代码
 * }
 * @endcode
 * \defgroup xQueueCreateStatic xQueueCreateStatic
 * \ingroup QueueManagement
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    // 静态创建队列的宏定义：实际调用通用静态创建函数 xQueueGenericCreateStatic，
    // 指定队列类型为基础队列（queueQUEUE_TYPE_BASE）
    #define xQueueCreateStatic( uxQueueLength, uxItemSize, pucQueueStorage, pxQueueBuffer )    \
        xQueueGenericCreateStatic( ( uxQueueLength ), ( uxItemSize ), ( pucQueueStorage ), ( pxQueueBuffer ), ( queueQUEUE_TYPE_BASE ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * queue.h
 * @code{c}
 * BaseType_t xQueueGetStaticBuffers( QueueHandle_t xQueue,
 *                                    uint8_t ** ppucQueueStorage,
 *                                    StaticQueue_t ** ppxStaticQueue );
 * @endcode
 *
 * 获取静态创建队列的“数据结构缓冲区”和“项目存储区缓冲区”的指针。
 * 这些缓冲区与队列创建时（通过xQueueCreateStatic）用户提供的缓冲区完全一致。
 *
 * @param xQueue 要获取缓冲区指针的目标队列（必须是静态创建的队列）。
 *
 * @param ppucQueueStorage 用于返回队列“项目存储区缓冲区”的指针（输出参数）。
 * 若队列创建时用户提供了存储区（uxItemSize≠0），则此指针指向该存储区；若uxItemSize=0（无存储区），则返回NULL。
 *
 * @param ppxStaticQueue 用于返回队列“数据结构缓冲区”的指针（输出参数）。
 * 此指针指向队列创建时用户提供的StaticQueue_t类型变量（即队列控制结构的存储区）。
 *
 * @return 若成功获取缓冲区指针，返回pdTRUE；若队列无效（如非静态创建、句柄为空），返回pdFALSE。
 *
 * \defgroup xQueueGetStaticBuffers xQueueGetStaticBuffers
 * \ingroup QueueManagement
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )  // 仅启用静态内存分配时，此API才有效
    // 宏定义：将接口封装为底层通用函数xQueueGenericGetStaticBuffers的调用
    #define xQueueGetStaticBuffers( xQueue, ppucQueueStorage, ppxStaticQueue )    \
        xQueueGenericGetStaticBuffers( ( xQueue ), ( ppucQueueStorage ), ( ppxStaticQueue ) )
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * queue.h
 * @code{c}
 * BaseType_t xQueueSendToFront(
 *                                 QueueHandle_t    xQueue,
 *                                 const void       *pvItemToQueue,
 *                                 TickType_t       xTicksToWait
 *                             );
 * @endcode
 *
 * 将一个项目发送到队列的**头部**（队首）。项目通过“复制”方式入队（而非引用），即把`pvItemToQueue`指向的数据
 * 复制到队列的存储区中。此函数**禁止在中断服务程序（ISR）中调用**，若需在ISR中发送项目，应使用`xQueueSendFromISR()`。
 *
 * @param xQueue 要发送项目的目标队列句柄（由`xQueueCreate()`或`xQueueCreateStatic()`创建）。
 *
 * @param pvItemToQueue 指向待入队项目的指针。队列可存储的项目大小在创建时已定义（`uxItemSize`），
 * 函数会从`pvItemToQueue`指向的地址复制`uxItemSize`字节的数据到队列存储区，因此需确保`pvItemToQueue`指向
 * 有效内存，且内存长度不小于`uxItemSize`。
 *
 * @param xTicksToWait 若队列已满，当前任务最多阻塞等待的“时钟节拍数”。
 * - 若设为0：队列满时立即返回，不阻塞；
 * - 若设为`portMAX_DELAY`：任务会一直阻塞，直到队列有空间（需确保未禁用任务阻塞，且调度器正常运行）；
 * - 其他值：阻塞`xTicksToWait`个节拍后，若队列仍无空间则返回失败。
 * 注：时钟节拍与实际时间的换算需使用`portTICK_PERIOD_MS`（如1个节拍=1ms，则10个节拍=10ms）。
 *
 * @return 若项目成功入队，返回`pdTRUE`；若队列已满且等待超时，返回`errQUEUE_FULL`。
 *
 * 使用示例：
 * @code{c}
 * // 定义一个消息结构体
 * struct AMessage
 * {
 *  char ucMessageID;  // 消息ID（用于标识消息类型）
 *  char ucData[ 20 ]; // 消息数据（20字节）
 * } xMessage;  // 定义一个消息实例
 *
 * uint32_t ulVar = 10U;  // 定义一个32位无符号整数（待入队的简单数据）
 *
 * void vATask( void *pvParameters )
 * {
 *  QueueHandle_t xQueue1, xQueue2;  // 定义两个队列句柄
 *  struct AMessage *pxMessage;      // 指向消息结构体的指针（用于传递复杂数据）
 *
 *  // 1. 创建队列1：可存储10个uint32_t类型数据（每个项目4字节）
 *  xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );
 *
 *  // 2. 创建队列2：可存储10个“指向AMessage结构体的指针”（每个项目4字节，32位系统）
 *  // 传递指针而非整个结构体，可避免大量数据复制，提升效率
 *  xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );
 *
 *  // ... 其他初始化逻辑
 *
 *  // 向队列1发送uint32_t类型数据
 *  if( xQueue1 != 0 )  // 先判断队列创建成功（句柄非空）
 *  {
 *      // 尝试发送ulVar到队列1头部，若队列满则阻塞等待10个节拍
 *      if( xQueueSendToFront( xQueue1, ( void * ) &ulVar, ( TickType_t ) 10 ) != pdPASS )
 *      {
 *          // 等待10个节拍后仍未成功入队（队列一直满），执行错误处理
 *      }
 *  }
 *
 *  // 向队列2发送“指向AMessage结构体的指针”
 *  if( xQueue2 != 0 )  // 判断队列创建成功
 *  {
 *      pxMessage = &xMessage;  // 指针指向已定义的消息实例
 *      // 尝试发送指针到队列2头部，队列满时不阻塞（等待0个节拍）
 *      xQueueSendToFront( xQueue2, ( void * ) &pxMessage, ( TickType_t ) 0 );
 *  }
 *
 *  // ... 任务其余逻辑代码
 * }
 * @endcode
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
// 宏定义：将xQueueSendToFront封装为通用发送函数xQueueGenericSend的调用
// 最后一个参数queueSEND_TO_FRONT指定“发送到队列头部”，是入队位置的标识
#define xQueueSendToFront( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_FRONT )

/**
 * queue.h
 * @code{c}
 * BaseType_t xQueueSendToBack(
 *                                 QueueHandle_t    xQueue,
 *                                 const void       *pvItemToQueue,
 *                                 TickType_t       xTicksToWait
 *                             );
 * @endcode
 *
 * 该函数是调用xQueueGenericSend()的宏定义，核心功能是**将项目发送到队列尾部**（遵循FIFO先进先出原则）。
 * 项目通过“复制”方式入队（非引用），即把pvItemToQueue指向的数据复制到队列存储区。
 * 此函数**禁止在中断服务程序（ISR）中调用**，ISR中需使用xQueueSendFromISR()。
 *
 * @param xQueue 目标队列句柄（由xQueueCreate()或xQueueCreateStatic()创建，需确保有效）。
 *
 * @param pvItemToQueue 待入队项目的指针：
 * 队列可存储的项目大小在创建时已通过uxItemSize定义，函数会从该指针指向的地址复制uxItemSize字节的数据到队列存储区；
 * 需确保指针指向有效内存（如全局变量、静态变量或堆分配内存），避免局部变量内存失效导致数据错误。
 *
 * @param xTicksToWait 队列满时，当前任务的最大阻塞等待节拍数：
 * - 设为0：队列满时立即返回，不阻塞；
 * - 设为portMAX_DELAY：任务持续阻塞，直到队列有空间（需确保调度器已启动且未禁用阻塞）；
 * - 其他值：阻塞xTicksToWait个节拍后，若队列仍无空间则返回失败；
 * 实际时间 = 节拍数 × portTICK_PERIOD_MS（如portTICK_PERIOD_MS=1时，10节拍=10ms）。
 *
 * @return 若项目成功入队，返回pdTRUE；若队列满且等待超时，返回errQUEUE_FULL。
 *
 * 使用示例：（同前序接口，略）
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
// 宏定义：调用通用发送函数xQueueGenericSend，指定入队位置为“队尾”（queueSEND_TO_BACK）
#define xQueueSendToBack( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * queue.h
 * @code{c}
 * BaseType_t xQueueSend(
 *                            QueueHandle_t xQueue,
 *                            const void * pvItemToQueue,
 *                            TickType_t xTicksToWait
 *                       );
 * @endcode
 *
 * 该函数是调用xQueueGenericSend()的宏定义，**仅为兼容FreeRTOS旧版本**（旧版本无xQueueSendToFront/Back）。
 * 功能与xQueueSendToBack()完全一致，即**将项目发送到队列尾部**，遵循FIFO原则。
 * 项目通过“复制”入队，禁止在ISR中调用，ISR需使用xQueueSendFromISR()。
 *
 * @param xQueue 目标队列句柄（有效且未被删除）。
 *
 * @param pvItemToQueue 待入队项目的指针（需指向有效内存，大小匹配队列的uxItemSize）。
 *
 * @param xTicksToWait 队列满时的最大阻塞等待节拍数（规则同xQueueSendToBack）。
 *
 * @return 成功入队返回pdTRUE，队列满且超时返回errQUEUE_FULL。
 *
 * 使用示例：（同xQueueSendToBack，略）
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
// 宏定义：调用xQueueGenericSend，入队位置为“队尾”，与xQueueSendToBack等价
#define xQueueSend( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

/**
 * queue.h
 * @code{c}
 * BaseType_t xQueueOverwrite(
 *                            QueueHandle_t xQueue,
 *                            const void * pvItemToQueue
 *                       );
 * @endcode
 *
 * 该函数**仅适用于长度为1的队列**（队列要么空、要么满），核心功能是：
 * - 若队列空：将项目正常入队；
 * - 若队列满：覆盖队列中已有的唯一项目（无需等待，直接替换）。
 * 项目通过“复制”入队，禁止在ISR中调用，ISR需使用xQueueOverwriteFromISR()。
 *
 * @param xQueue 目标队列句柄（必须是长度为1的队列，否则触发configASSERT断言）。
 *
 * @param pvItemToQueue 待入队/覆盖项目的指针（需指向有效内存，大小匹配队列的uxItemSize）。
 *
 * @return 该函数是调用xQueueGenericSend()的宏，理论上仅返回pdTRUE：
 * 因即使队列满，也会直接覆盖项目，不存在“队列满且超时”的情况；
 * 若队列无效（如句柄空、长度≠1），会触发断言或返回错误。
 *
 * 使用示例：（略）
 * \defgroup xQueueOverwrite xQueueOverwrite
 * \ingroup QueueManagement
 */
// 宏定义：调用xQueueGenericSend，入队位置为“覆盖”（queueOVERWRITE），等待时间设为0（无需阻塞）
#define xQueueOverwrite( xQueue, pvItemToQueue ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), 0, queueOVERWRITE )


/**
 * queue.h
 * @code{c}
 * BaseType_t xQueueGenericSend(
 *                                  QueueHandle_t xQueue,
 *                                  const void * pvItemToQueue,
 *                                  TickType_t xTicksToWait
 *                                  BaseType_t xCopyPosition
 *                              );
 * @endcode
 *
 * 该函数是队列发送的**底层统一实现**，推荐优先使用上层宏（xQueueSend()、xQueueSendToFront()等），而非直接调用。
 * 功能是根据xCopyPosition指定的位置，将项目发送到队列，支持队首、队尾、覆盖三种模式。
 * 项目通过“复制”入队，禁止在ISR中调用，ISR需使用xQueueSendFromISR()。
 *
 * @param xQueue 目标队列句柄（有效且未被删除）。
 *
 * @param pvItemToQueue 待入队项目的指针（需指向有效内存，大小匹配队列的uxItemSize）。
 *
 * @param xTicksToWait 队列满时的最大阻塞等待节拍数（规则同前序接口）。
 *
 * @param xCopyPosition 入队位置标识：
 * - queueSEND_TO_BACK：项目入队到尾部（FIFO模式）；
 * - queueSEND_TO_FRONT：项目入队到头部（LIFO/优先级模式）；
 * - queueOVERWRITE：覆盖队列中已有项目（仅单项目队列可用）。
 *
 * @return 成功入队返回pdTRUE，队列满且超时返回errQUEUE_FULL。
 *
 * 使用示例：（略）
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
// 函数声明：PRIVILEGED_FUNCTION表示该函数仅在特权模式下调用（部分架构支持特权/用户模式分离）
BaseType_t xQueueGenericSend( QueueHandle_t xQueue,
                              const void * const pvItemToQueue,
                              TickType_t xTicksToWait,
                              const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;

/**
 * queue.h  // 头文件名称，该函数声明所在的头文件
 * @code{c}  // Doxygen 标记，用于指定后续代码块为C语言语法，便于文档生成时高亮显示
 * BaseType_t xQueuePeek(  // 函数声明：返回类型为BaseType_t（FreeRTOS通用布尔/状态类型，通常是int），函数名xQueuePeek（队列查看函数）
 *                           QueueHandle_t xQueue,  // 第一个参数：队列句柄（QueueHandle_t类型，标识特定队列的唯一标识符）
 *                           void * const pvBuffer,  // 第二个参数：指向接收缓冲区的指针（void*类型支持任意数据类型，const表示指针本身不可修改）
 *                           TickType_t xTicksToWait  // 第三个参数：阻塞等待时间（TickType_t类型，以系统时钟周期为单位）
 *                       );
 * @endcode  // Doxygen 标记，结束C语言代码块
 */

// 函数功能描述
/**
 * Receive an item from a queue without removing the item from the queue.
 * （从队列中接收一个数据项，但不会将该数据项从队列中移除。）
 * 
 * The item is received by copy so a buffer of adequate size must be
 * provided.  The number of bytes copied into the buffer was defined when
 * the queue was created.
 * （数据项通过“复制”方式接收，因此必须提供大小足够的缓冲区。复制到缓冲区的字节数，在队列创建时就已定义。）
 * 
 * Successfully received items remain on the queue so will be returned again
 * by the next call, or a call to xQueueReceive().
 * （成功接收的数据项会保留在队列中，因此下一次调用本函数，或调用xQueueReceive()（队列接收函数）时，仍会返回该数据项。）
 * 
 * This macro must not be used in an interrupt service routine.  See
 * xQueuePeekFromISR() for an alternative that can be called from an interrupt
 * service routine.
 * （本函数（宏）不得在中断服务程序（ISR）中使用。若需在中断服务程序中实现类似功能，请参考替代函数xQueuePeekFromISR()。）
 * 
 * @param xQueue The handle to the queue from which the item is to be
 * received.
 * （@param 标记：参数说明。xQueue：要从中接收数据项的目标队列的句柄。）
 * 
 * @param pvBuffer Pointer to the buffer into which the received item will
 * be copied.
 * （@param 标记：参数说明。pvBuffer：指向接收缓冲区的指针，从队列中接收的数据项将被复制到该缓冲区。）
 * 
 * @param xTicksToWait The maximum amount of time the task should block
 * waiting for an item to receive should the queue be empty at the time
 * of the call. The time is defined in tick periods so the constant
 * portTICK_PERIOD_MS should be used to convert to real time if this is required.
 * xQueuePeek() will return immediately if xTicksToWait is 0 and the queue
 * is empty.
 * （@param 标记：参数说明。xTicksToWait：调用本函数时，若队列为空，任务阻塞等待数据项的最长时间。
 * 该时间以“系统时钟周期（tick）”为单位；若需转换为实际时间（如毫秒），需使用常量portTICK_PERIOD_MS（每个tick对应的毫秒数）。
 * 若xTicksToWait设为0且队列为空，xQueuePeek()会立即返回，不阻塞。）
 * 
 * @return pdTRUE if an item was successfully received from the queue,
 * otherwise pdFALSE.
 * （@return 标记：返回值说明。若成功从队列接收数据项，返回pdTRUE（FreeRTOS定义的“真”，通常为1）；否则返回pdFALSE（“假”，通常为0）。）
 */

// 函数示例代码（展示实际使用场景）
/**
 * Example usage:  // 示例用法说明
 * @code{c}  // Doxygen 标记，指定后续为C语言示例代码
 * struct AMessage  // 定义一个结构体AMessage，作为队列中传输的数据类型
 * {
 *  char ucMessageID;  // 结构体成员1：消息ID（unsigned char类型）
 *  char ucData[ 20 ];  // 结构体成员2：消息数据缓冲区（20字节的unsigned char数组）
 * } xMessage;  // 定义结构体变量xMessage
 *
 * QueueHandle_t xQueue;  // 定义队列句柄变量xQueue，用于后续标识创建的队列
 *
 * // Task to create a queue and post a value.  // 任务1：创建队列并向队列发送数据
 * void vATask( void *pvParameters )  // 任务函数vATask（FreeRTOS任务函数固定格式：返回值void，参数为void*）
 * {
 * struct AMessage *pxMessage;  // 定义指向AMessage结构体的指针pxMessage
 *
 *  // Create a queue capable of containing 10 pointers to AMessage structures.
 *  // These should be passed by pointer as they contain a lot of data.
 *  // （创建一个队列：队列可存储10个“指向AMessage结构体的指针”；
 *  // 由于结构体数据量较大，此处通过“指针传递”方式传输数据，而非直接传结构体本身）
 *  xQueue = xQueueCreate( 10, sizeof( struct AMessage * ) );  // 调用xQueueCreate创建队列，参数1：队列长度（10个元素），参数2：每个元素的大小（AMessage指针的字节数）
 *  if( xQueue == 0 )  // 判断队列是否创建成功（xQueueCreate返回0表示创建失败）
 *  {
 *      // Failed to create the queue.  // 队列创建失败的处理逻辑（此处仅为注释，实际需根据需求补充代码）
 *  }
 *
 *  // ...  // 省略其他业务逻辑代码
 *
 *  // Send a pointer to a struct AMessage object.  Don't block if the
 *  // queue is already full.
 *  // （向队列发送一个“指向AMessage结构体的指针”；若队列已满，不阻塞等待）
 *  pxMessage = &xMessage;  // 将结构体变量xMessage的地址赋值给指针pxMessage
 *  xQueueSend( xQueue, ( void * ) &pxMessage, ( TickType_t ) 0 );  // 调用xQueueSend发送数据：参数1（队列句柄）、参数2（要发送的数据地址，强制转为void*）、参数3（阻塞时间0）
 *
 *  // ... Rest of task code.  // 省略任务剩余业务逻辑代码
 * }
 *
 * // Task to peek the data from the queue.  // 任务2：从队列中“查看”数据（不移除数据）
 * void vADifferentTask( void *pvParameters )  // 任务函数vADifferentTask（FreeRTOS任务函数格式）
 * {
 * struct AMessage *pxRxedMessage;  // 定义指向AMessage结构体的指针pxRxedMessage，用于接收从队列查看的数据
 *
 *  if( xQueue != 0 )  // 判断队列句柄是否有效（即队列是否已成功创建）
 *  {
 *      // Peek a message on the created queue.  Block for 10 ticks if a
 *      // message is not immediately available.
 *      // （在已创建的队列中查看一条消息；若消息无法立即获取，阻塞等待10个时钟周期）
 *      if( xQueuePeek( xQueue, &( pxRxedMessage ), ( TickType_t ) 10 ) )  // 调用xQueuePeek查看数据：参数1（队列句柄）、参数2（接收缓冲区地址，即指针pxRxedMessage的地址）、参数3（阻塞时间10）
 *      {
 *          // pxRxedMessage now points to the struct AMessage variable posted
 *          // by vATask, but the item still remains on the queue.
 *          // （此时pxRxedMessage指向vATask任务发送的AMessage结构体变量，但该数据项仍保留在队列中，未被移除）
 *      }
 *  }
 *
 *  // ... Rest of task code.  // 省略任务剩余业务逻辑代码
 * }
 * @endcode  // Doxygen 标记，结束示例代码块
 */

// 函数最终声明（带PRIVILEGED_FUNCTION宏，FreeRTOS中标识该函数为“特权函数”，仅可在特权模式下调用）
BaseType_t xQueuePeek( QueueHandle_t xQueue,
                       void * const pvBuffer,
                       TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
                       
/**
 * queue.h  // 该函数声明所在的头文件名称
 * @code{c}  // Doxygen文档标记，用于指定后续代码块为C语言语法（便于文档生成时语法高亮）
 * BaseType_t xQueuePeekFromISR(  // 函数声明：返回类型为BaseType_t（FreeRTOS通用状态/布尔类型），函数名为xQueuePeekFromISR（中断安全版队列查看函数）
 *                                  QueueHandle_t xQueue,  // 第一个参数：队列句柄（标识要操作的特定队列）
 *                                  void *pvBuffer,        // 第二个参数：接收数据的缓冲区指针（支持任意数据类型）
 *                              );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 这是xQueuePeek函数的中断安全版本，可从中断服务程序（ISR）中调用。
 *
 * 从队列中接收一个数据项，但不会将该数据项从队列中移除。
 *
 * 数据项通过“复制”方式接收，因此必须提供大小足够的缓冲区。
 * 复制到缓冲区的字节数，在队列创建时就已经定义好了。
 *
 * 成功接收的数据项会保留在队列中，因此下一次调用本函数，或调用xQueueReceive函数（普通版队列接收函数）时，仍会返回该数据项。
 *
 * @param xQueue 要从中接收数据项的目标队列的句柄。
 *
 * @param pvBuffer 指向接收缓冲区的指针，从队列中接收的数据项会被复制到该缓冲区中。
 *
 * @return 如果成功从队列接收数据项，返回pdTRUE（FreeRTOS定义的“真”，通常为1）；否则返回pdFALSE（“假”，通常为0，常见原因是队列为空）。
 *
 * \defgroup xQueuePeekFromISR xQueuePeekFromISR  // Doxygen分组标记，将该函数归类到xQueuePeekFromISR分组
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组（便于文档分类）
 */

// 函数最终声明，末尾的PRIVILEGED_FUNCTION是FreeRTOS宏，标识该函数为“特权函数”（仅可在特权模式下调用）
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,
                              void * const pvBuffer ) PRIVILEGED_FUNCTION;

/**
 * queue.h  // 该函数声明所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时会高亮显示）
 * BaseType_t xQueueReceive(  // 函数声明：返回类型为BaseType_t（FreeRTOS通用状态/布尔类型），函数名为xQueueReceive（队列接收函数）
 *                               QueueHandle_t xQueue,  // 第一个参数：队列句柄（标识要从中接收数据的队列）
 *                               void *pvBuffer,        // 第二个参数：接收数据的缓冲区指针（支持任意数据类型）
 *                               TickType_t xTicksToWait // 第三个参数：阻塞等待时间（以系统时钟周期为单位）
 *                          );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 从队列中接收一个数据项。数据项通过“复制”方式接收，因此必须提供大小足够的缓冲区。
 * 复制到缓冲区的字节数，在队列创建时就已经定义好了。
 *
 * 成功接收的数据项会从队列中移除（区别于“查看”操作，接收后数据不再保留在队列中）。
 *
 * 本函数不得在中断服务程序（ISR）中使用。若需在中断中接收队列数据，请使用替代函数xQueueReceiveFromISR。
 *
 * @param xQueue 要从中接收数据项的目标队列的句柄。
 *
 * @param pvBuffer 指向接收缓冲区的指针，从队列中接收的数据项会被复制到该缓冲区中。
 *
 * @param xTicksToWait 当调用本函数时若队列为空，任务应阻塞等待数据项的最长时间。
 * 若xTicksToWait设为0且队列为空，xQueueReceive()会立即返回，不阻塞。
 * 该时间以“系统时钟周期（tick）”为单位，若需转换为实际时间（如毫秒），需使用常量portTICK_PERIOD_MS（每个tick对应的毫秒数）。
 *
 * @return 若成功从队列接收数据项，返回pdTRUE（FreeRTOS定义的“真”，通常为1）；否则返回pdFALSE（“假”，通常为0，常见原因是队列空且等待超时）。
 *
 * 示例用法：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 * // 定义一个结构体AMessage，作为队列中传输的数据类型
 * struct AMessage
 * {
 *  char ucMessageID;  // 结构体成员1：消息ID（unsigned char类型）
 *  char ucData[ 20 ]; // 结构体成员2：消息数据缓冲区（20字节的unsigned char数组）
 * } xMessage;  // 定义结构体变量xMessage
 *
 * QueueHandle_t xQueue;  // 定义队列句柄变量xQueue，用于后续标识创建的队列
 *
 * // 任务1：创建队列并向队列发送数据
 * void vATask( void *pvParameters )  // FreeRTOS任务函数固定格式：返回值void，参数为void*（支持任意类型参数传递）
 * {
 * struct AMessage *pxMessage;  // 定义指向AMessage结构体的指针pxMessage
 *
 *  // 创建一个队列：队列可存储10个“指向AMessage结构体的指针”
 *  // 由于结构体数据量较大，此处通过“指针传递”方式传输数据，而非直接传递结构体本身（节省内存和拷贝开销）
 *  xQueue = xQueueCreate( 10, sizeof( struct AMessage * ) );  // 调用xQueueCreate创建队列，参数1：队列长度（10个元素），参数2：每个元素的大小（AMessage指针的字节数）
 *  if( xQueue == 0 )  // 判断队列是否创建成功（xQueueCreate返回0表示创建失败，通常是内存不足）
 *  {
 *      // 队列创建失败的处理逻辑（此处仅为注释，实际项目中需根据需求补充代码，如报错、复位等）
 *  }
 *
 *  // ...  // 省略其他业务逻辑代码（如初始化结构体数据等）
 *
 *  // 向队列发送一个“指向AMessage结构体的指针”；若队列已满，不阻塞等待（等待时间设为0）
 *  pxMessage = &xMessage;  // 将结构体变量xMessage的地址赋值给指针pxMessage
 *  xQueueSend( xQueue, ( void * ) &pxMessage, ( TickType_t ) 0 );  // 调用xQueueSend发送数据，参数1：队列句柄，参数2：数据地址（强制转为void*），参数3：阻塞时间0
 *
 *  // ... 任务剩余业务逻辑代码（省略）
 * }
 *
 * // 任务2：从队列中接收数据
 * void vADifferentTask( void *pvParameters )  // FreeRTOS任务函数
 * {
 * struct AMessage *pxRxedMessage;  // 定义指向AMessage结构体的指针pxRxedMessage，用于接收从队列获取的数据
 *
 *  if( xQueue != 0 )  // 判断队列句柄是否有效（即队列是否已成功创建）
 *  {
 *      // 从已创建的队列中接收一条消息；若消息无法立即获取，阻塞等待10个时钟周期
 *      if( xQueueReceive( xQueue, &( pxRxedMessage ), ( TickType_t ) 10 ) )
 *      {
 *          // 此时pxRxedMessage指向vATask任务发送的AMessage结构体变量
 *          // （注：数据已从队列中移除，后续其他任务无法再接收该数据）
 *      }
 *  }
 *
 *  // ... 任务剩余业务逻辑代码（省略）
 * }
 * @endcode  // Doxygen文档标记，结束示例代码块
 *
 * \defgroup xQueueReceive xQueueReceive  // Doxygen分组标记，将该函数归类到xQueueReceive分组
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组（便于文档分类检索）
 */

// 函数最终声明，末尾的PRIVILEGED_FUNCTION是FreeRTOS宏，标识该函数为“特权函数”（仅可在特权模式下调用）
BaseType_t xQueueReceive( QueueHandle_t xQueue,
                          void * const pvBuffer,
                          TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * queue.h  // 该函数声明所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法
 * UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue );  // 函数声明：返回类型为UBaseType_t（无符号基础类型），函数名为uxQueueMessagesWaiting（查询队列消息数量）
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 返回存储在队列中的消息数量。
 *
 * @param xQueue 被查询队列的句柄。
 *
 * @return 队列中可用的消息数量。
 *
 * \defgroup uxQueueMessagesWaiting uxQueueMessagesWaiting  // Doxygen分组标记，将该函数归类到uxQueueMessagesWaiting分组
 * \ingroup QueueManagement  // Doxygen分组标记，属于队列管理分组
 */

// 函数最终声明，PRIVILEGED_FUNCTION宏标识为特权函数
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * queue.h  // 该函数声明所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时会对代码进行语法高亮）
 * UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue );  // 函数声明：返回类型为UBaseType_t（FreeRTOS无符号基础类型，通常用于计数），函数名为uxQueueSpacesAvailable（查询队列空闲空间数量）
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 返回队列中可用的空闲空间数量。
 * 该数量等于：在不移除队列中任何数据项的前提下，还能向队列发送的数据项总数（即队列填满前可新增的最大数据项数）。
 *
 * @param xQueue 被查询队列的句柄（需是之前通过xQueueCreate创建的有效队列句柄）。
 *
 * @return 队列中当前可用的空闲空间数量（返回0表示队列已满）。
 *
 * \defgroup uxQueueSpacesAvailable uxQueueSpacesAvailable  // Doxygen分组标记，将该函数归类到uxQueueSpacesAvailable分组（便于文档分类检索）
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组
 */

// 函数最终声明，末尾的PRIVILEGED_FUNCTION是FreeRTOS宏，标识该函数为“特权函数”（仅可在特权模式下调用，确保队列操作的安全性）
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * queue.h  // 该函数声明所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时会对代码进行语法高亮）
 * void vQueueDelete( QueueHandle_t xQueue );  // 函数声明：返回类型为void（无返回值），函数名为vQueueDelete（删除队列）
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 删除一个队列——释放为存储队列中数据项而分配的所有内存。
 * （注：队列删除后，其关联的内存会被回收，后续不可再使用该队列句柄操作队列）
 *
 * @param xQueue 待删除队列的句柄（需是之前通过xQueueCreate创建的有效队列句柄）。
 *
 * \defgroup vQueueDelete vQueueDelete  // Doxygen分组标记，将该函数归类到vQueueDelete分组（便于文档分类检索）
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组
 */

// 函数最终声明，末尾的PRIVILEGED_FUNCTION是FreeRTOS宏，标识该函数为“特权函数”（仅可在特权模式下调用，确保内存操作的安全性）
void vQueueDelete( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/**
 * queue.h  // 该宏定义所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时会对代码进行语法高亮）
 * BaseType_t xQueueSendToFrontFromISR(
 *                                       QueueHandle_t xQueue,  // 第一个参数：目标队列的句柄
 *                                       const void *pvItemToQueue,  // 第二个参数：要发送到队列的数据指针
 *                                       BaseType_t *pxHigherPriorityTaskWoken  // 第三个参数：指向“高优先级任务是否被唤醒”标记的指针
 *                                    );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 这是一个宏，其内部会调用xQueueGenericSendFromISR()函数（实际的中断安全版队列发送逻辑由该函数实现）。
 *
 * 将一个数据项发送到队列的“头部”（即新数据会成为队列中第一个被读取的数据）。
 * 该宏可安全地在中断服务程序（ISR）中使用（核心特性：中断安全）。
 *
 * 数据项通过“复制”方式存入队列，而非“引用”方式，因此建议仅发送小型数据项，
 * 尤其在从中断服务程序中调用时更应如此。多数情况下，更推荐将“数据项的指针”存入队列（减少复制开销）。
 *
 * @param xQueue 要向其发送数据项的目标队列的句柄（需是之前通过xQueueCreate创建的有效句柄）。
 *
 * @param pvItemToQueue 指向要存入队列的数据项的指针。队列可存储的数据项大小在创建队列时已定义，
 * 因此会从pvItemToQueue指向的地址复制“与队列元素大小相等的字节数”到队列的存储区域。
 *
 * @param pxHigherPriorityTaskWoken 该参数是一个指针，用于接收“高优先级任务是否被唤醒”的标记：
 * 当向队列发送数据后，若有因等待该队列空闲空间而阻塞的任务被唤醒，且被唤醒任务的优先级
 * 高于当前正在运行的任务（即中断服务程序所在的任务上下文），xQueueSendToFrontFromISR()会将
 * *pxHigherPriorityTaskWoken设为pdTRUE。若该宏将此值设为pdTRUE，
 * 则应在退出中断服务程序前请求一次上下文切换（确保高优先级任务能及时执行）。
 *
 * @return 若数据成功发送到队列，返回pdTRUE（FreeRTOS定义的“真”，通常为1）；
 * 若队列已满导致数据发送失败，返回errQUEUE_FULL（队列满错误标记）。
 *
 * 缓冲IO场景下的示例用法（中断服务程序中每次调用可处理多个数据值）：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 * // 中断服务程序：处理缓冲数据的接收与发送
 * void vBufferISR( void )
 * {
 * char cIn;  // 存储从硬件缓冲区读取的单个字节数据
 * BaseType_t xHigherPriorityTaskWoken;  // 标记高优先级任务是否被唤醒
 *
 *  // 中断服务程序开始时，初始化“高优先级任务未被唤醒”
 *  xHigherPriorityTaskWoken = pdFALSE;
 *
 *  // 循环读取硬件缓冲区，直到缓冲区为空
 *  do
 *  {
 *      // 从硬件接收寄存器（地址为RX_REGISTER_ADDRESS）读取一个字节
 *      cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );
 *
 *      // 将读取到的字节发送到xRxQueue队列的头部（中断安全版发送）
 *      xQueueSendToFrontFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );
 *
 *  // 检查硬件缓冲区计数寄存器（地址为BUFFER_COUNT），若不为0则缓冲区仍有数据，继续循环
 *  } while( portINPUT_BYTE( BUFFER_COUNT ) );
 *
 *  // 缓冲区已空，若高优先级任务被唤醒，则请求上下文切换
 *  if( xHigherPriorityTaskWoken )
 *  {
 *      // 触发上下文切换（中断服务程序中需用任务切换函数确保高优先级任务及时运行）
 *      taskYIELD ();
 *  }
 * }
 * @endcode  // Doxygen文档标记，结束示例代码块
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR  // Doxygen分组标记，将该宏归类到xQueueSendFromISR分组
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组
 */

// 宏定义：xQueueSendToFrontFromISR本质是对xQueueGenericSendFromISR的封装
// 最后一个参数queueSEND_TO_FRONT指定“数据发送到队列头部”，是区分“头部发送”与“尾部发送”的关键标识
#define xQueueSendToFrontFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_FRONT )

/**
 * queue.h  // 该宏定义所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时会对代码进行语法高亮）
 * BaseType_t xQueueSendToBackFromISR(
 *                                       QueueHandle_t xQueue,  // 第一个参数：目标队列的句柄
 *                                       const void *pvItemToQueue,  // 第二个参数：要发送到队列的数据指针
 *                                       BaseType_t *pxHigherPriorityTaskWoken  // 第三个参数：指向“高优先级任务是否被唤醒”标记的指针
 *                                    );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 这是一个宏，其内部会调用xQueueGenericSendFromISR()函数（中断安全版队列发送的核心逻辑由该函数实现）。
 *
 * 将一个数据项发送到队列的“尾部”（即新数据会成为队列中最后一个被读取的数据，遵循“先进先出”FIFO原则）。
 * 该宏可安全地在中断服务程序（ISR）中使用（核心特性：中断安全）。
 *
 * 数据项通过“复制”方式存入队列，而非“引用”方式，因此建议仅发送小型数据项，
 * 尤其在从中断服务程序中调用时更应如此——中断需快速执行，避免因大量数据复制延长中断耗时。
 * 多数情况下，更推荐将“数据项的指针”存入队列（大幅减少复制开销，提升中断处理效率）。
 *
 * @param xQueue 要向其发送数据项的目标队列的句柄（需是之前通过xQueueCreate创建的有效句柄，无效句柄可能触发断言）。
 *
 * @param pvItemToQueue 指向要存入队列的数据项的指针。队列可存储的数据项大小在创建时已定义，
 * 因此会从pvItemToQueue指向的地址，复制“与队列元素大小相等的字节数”到队列的存储区域（确保数据完整）。
 *
 * @param pxHigherPriorityTaskWoken 该参数是一个指针，用于接收“高优先级任务是否被唤醒”的状态：
 * 当向队列发送数据后，若存在因“等待队列空闲空间”而阻塞的任务，该任务会被唤醒；
 * 若被唤醒任务的优先级高于当前运行的任务（即中断服务程序所在的任务上下文），此宏会将*pxHigherPriorityTaskWoken设为pdTRUE。
 * 若该值被设为pdTRUE，必须在退出中断服务程序前请求一次上下文切换（确保高优先级任务能及时抢占CPU，符合实时性要求）。
 *
 * @return 若数据成功发送到队列，返回pdTRUE（FreeRTOS定义的“真”，通常为1）；
 * 若队列已满导致数据无法存入，返回errQUEUE_FULL（队列满错误标记）。
 *
 * 缓冲IO场景下的示例用法（中断服务程序中每次调用可处理多个数据值）：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 * // 中断服务程序：处理硬件缓冲区的数据接收与队列发送
 * void vBufferISR( void )
 * {
 * char cIn;  // 存储从硬件寄存器读取的单个字节数据
 * BaseType_t xHigherPriorityTaskWoken;  // 标记“高优先级任务是否被唤醒”
 *
 *  // 中断服务程序开始时，初始化标记为“未唤醒高优先级任务”
 *  xHigherPriorityTaskWoken = pdFALSE;
 *
 *  // 循环读取硬件缓冲区，直到缓冲区为空
 *  do
 *  {
 *      // 从硬件接收寄存器（地址为RX_REGISTER_ADDRESS）读取一个字节
 *      cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );
 *
 *      // 将读取到的字节发送到xRxQueue队列的尾部（中断安全版发送）
 *      xQueueSendToBackFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );
 *
 *  // 检查硬件缓冲区计数寄存器（地址为BUFFER_COUNT）：若值非0，说明缓冲区仍有数据，继续循环
 *  } while( portINPUT_BYTE( BUFFER_COUNT ) );
 *
 *  // 缓冲区已空，若高优先级任务被唤醒，则请求上下文切换
 *  if( xHigherPriorityTaskWoken )
 *  {
 *      // 触发上下文切换（确保高优先级任务能立即执行）
 *      taskYIELD ();
 *  }
 * }
 * @endcode  // Doxygen文档标记，结束示例代码块
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR  // Doxygen分组标记，将该宏归类到xQueueSendFromISR分组（便于文档分类检索）
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组
 */

// 宏定义：xQueueSendToBackFromISR本质是对xQueueGenericSendFromISR的封装
// 最后一个参数queueSEND_TO_BACK指定“数据发送到队列尾部”，是区分“尾部发送”与“头部发送”的核心标识
#define xQueueSendToBackFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )

/**
 * queue.h  // 该宏定义所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时会对代码进行语法高亮）
 * BaseType_t xQueueOverwriteFromISR(
 *                            QueueHandle_t xQueue,  // 第一个参数：目标队列的句柄
 *                            const void * pvItemToQueue,  // 第二个参数：要写入队列的数据指针
 *                            BaseType_t *pxHigherPriorityTaskWoken  // 第三个参数：指向“高优先级任务是否被唤醒”标记的指针
 *                       );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 这是xQueueOverwrite()函数的中断安全版本，可在中断服务程序（ISR）中使用。
 *
 * 仅适用于“仅能存储单个数据项的队列”——即队列要么为空，要么已满（队列容量固定为1）。
 *
 * 向队列中写入一个数据项。若队列已存满数据（因队列容量为1，存满即已有1个数据），则直接覆盖队列中已有的数据值。
 * 数据项通过“复制”方式存入队列，而非“引用”方式（即队列存储的是数据副本，而非数据指针）。
 *
 * @param xQueue 要向其写入数据项的目标队列的句柄（需是之前通过xQueueCreate创建的有效句柄，且队列容量必须为1）。
 *
 * @param pvItemToQueue 指向要写入队列的数据项的指针。队列可存储的数据项大小在创建时已定义，
 * 因此会从pvItemToQueue指向的地址，复制“与队列元素大小相等的字节数”到队列的存储区域。
 *
 * @param pxHigherPriorityTaskWoken 该参数是一个指针，用于接收“高优先级任务是否被唤醒”的状态：
 * 当向队列写入数据后，若存在因“等待队列数据”而阻塞的任务（如调用xQueueReceive()阻塞的任务），该任务会被唤醒；
 * 若被唤醒任务的优先级高于当前运行的任务（即中断服务程序所在的任务上下文），此宏会将*pxHigherPriorityTaskWoken设为pdTRUE。
 * 若该值被设为pdTRUE，必须在退出中断服务程序前请求一次上下文切换（确保高优先级任务能及时抢占CPU）。
 *
 * @return xQueueOverwriteFromISR()本质是调用xQueueGenericSendFromISR()的宏，因此返回值与xQueueSendToFrontFromISR()一致。
 * 但由于xQueueOverwriteFromISR()即使在队列已满时也会覆盖数据，因此实际仅可能返回pdTRUE（成功）——不会返回errQUEUE_FULL（队列满错误）。
 *
 * 示例用法：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 *
 * QueueHandle_t xQueue;  // 定义队列句柄变量xQueue
 *
 * // 任务函数：创建仅能存储单个数据项的队列
 * void vFunction( void *pvParameters )
 * {
 *  // 创建一个队列：容量为1（仅能存1个数据项），数据项类型为uint32_t（4字节无符号整数）。
 *  // 强烈建议不要在容量大于1的队列上使用xQueueOverwriteFromISR()，
 *  // 若开启configASSERT()断言功能，这种错误使用会触发断言失败。
 *  xQueue = xQueueCreate( 1, sizeof( uint32_t ) );
 * }
 *
 * // 中断处理函数：使用xQueueOverwriteFromISR()写入数据
 * void vAnInterruptHandler( void )
 * {
 * // xHigherPriorityTaskWoken必须初始化为pdFALSE后再使用
 * BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 * uint32_t ulVarToSend, ulValReceived;  // 定义要发送的数据变量和接收数据变量（接收变量仅作示例，未实际使用）
 *
 *  // 使用xQueueOverwriteFromISR()向队列写入值10
 *  ulVarToSend = 10;
 *  xQueueOverwriteFromISR( xQueue, &ulVarToSend, &xHigherPriorityTaskWoken );
 *
 *  // 此时队列已存满（容量为1），但再次调用xQueueOverwriteFromISR()仍会成功——队列中原有值会被新值覆盖
 *  ulVarToSend = 100;
 *  xQueueOverwriteFromISR( xQueue, &ulVarToSend, &xHigherPriorityTaskWoken );
 *
 *  // 此时从队列读取数据，会得到100（原有的10已被覆盖）
 *
 *  // ... （省略其他业务逻辑）
 *
 *  if( xHigherPrioritytaskWoken == pdTRUE )
 *  {
 *      // 向队列写入数据导致一个任务被唤醒，且被唤醒任务的优先级
 *      // 大于或等于当前执行任务（即被中断打断的任务）的优先级。
 *      // 执行上下文切换，使中断直接返回到被唤醒的任务（而非原被中断的任务）。
 *      // 具体使用的宏因端口而异，通常是portYIELD_FROM_ISR()或portEND_SWITCHING_ISR()，
 *      // 需参考所用端口的文档说明。
 *      portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
 *  }
 * }
 * @endcode  // Doxygen文档标记，结束示例代码块
 *
 * \defgroup xQueueOverwriteFromISR xQueueOverwriteFromISR  // Doxygen分组标记，将该宏归类到xQueueOverwriteFromISR分组
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组
 */

// 宏定义：xQueueOverwriteFromISR本质是对xQueueGenericSendFromISR的封装
// 最后一个参数queueOVERWRITE指定“覆盖模式”，是该宏与“头部/尾部发送”宏的核心区别
#define xQueueOverwriteFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueOVERWRITE )


/**
 * queue.h  // 该宏定义所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时会对代码进行语法高亮）
 * BaseType_t xQueueSendFromISR(
 *                                   QueueHandle_t xQueue,  // 第一个参数：目标队列的句柄
 *                                   const void *pvItemToQueue,  // 第二个参数：要发送到队列的数据指针
 *                                   BaseType_t *pxHigherPriorityTaskWoken  // 第三个参数：指向“高优先级任务是否被唤醒”标记的指针
 *                              );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 这是一个宏，其内部会调用xQueueGenericSendFromISR()函数。
 * 该宏的存在是为了兼容早期版本的FreeRTOS——在那些版本中，尚未提供xQueueSendToBackFromISR()和xQueueSendToFrontFromISR()这两个宏。
 *
 * 将一个数据项发送到队列的“尾部”（遵循“先进先出”FIFO原则，新数据会成为队列中最后一个被读取的数据）。
 * 该宏可安全地在中断服务程序（ISR）中使用（核心特性：中断安全）。
 *
 * 数据项通过“复制”方式存入队列，而非“引用”方式，因此建议仅发送小型数据项，
 * 尤其在从中断服务程序中调用时更应如此——中断需快速执行，大量数据复制会延长中断耗时，影响系统响应。
 * 多数情况下，更推荐将“数据项的指针”存入队列（大幅减少复制开销，提升中断处理效率）。
 *
 * @param xQueue 要向其发送数据项的目标队列的句柄（需是之前通过xQueueCreate创建的有效句柄，无效句柄可能触发断言）。
 *
 * @param pvItemToQueue 指向要存入队列的数据项的指针。队列可存储的数据项大小在创建时已定义，
 * 因此会从pvItemToQueue指向的地址，复制“与队列元素大小相等的字节数”到队列的存储区域（确保数据完整存入）。
 *
 * @param pxHigherPriorityTaskWoken 该参数是一个指针，用于接收“高优先级任务是否被唤醒”的状态：
 * 当向队列发送数据后，若存在因“等待队列空闲空间”而阻塞的任务（如调用xQueueSend()时队列满而阻塞的任务），该任务会被唤醒；
 * 若被唤醒任务的优先级高于当前运行的任务（即中断服务程序所打断的任务），此宏会将*pxHigherPriorityTaskWoken设为pdTRUE。
 * 若该值被设为pdTRUE，必须在退出中断服务程序前请求一次上下文切换（确保高优先级任务能及时抢占CPU，符合实时系统要求）。
 *
 * @return 若数据成功发送到队列，返回pdTRUE（FreeRTOS定义的“真”，通常为1）；
 * 若队列已满导致数据无法存入，返回errQUEUE_FULL（队列满错误标记）。
 *
 * 缓冲IO场景下的示例用法（中断服务程序中每次调用可处理多个数据值）：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 * // 中断服务程序：处理硬件缓冲区的数据接收与队列发送
 * void vBufferISR( void )
 * {
 * char cIn;  // 存储从硬件寄存器读取的单个字节数据
 * BaseType_t xHigherPriorityTaskWoken;  // 标记“高优先级任务是否被唤醒”
 *
 *  // 中断服务程序开始时，初始化标记为“未唤醒高优先级任务”
 *  xHigherPriorityTaskWoken = pdFALSE;
 *
 *  // 循环读取硬件缓冲区，直到缓冲区为空
 *  do
 *  {
 *      // 从硬件接收寄存器（地址为RX_REGISTER_ADDRESS）读取一个字节
 *      cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );
 *
 *      // 将读取到的字节通过xQueueSendFromISR()发送到xRxQueue队列
 *      xQueueSendFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );
 *
 *  // 检查硬件缓冲区计数寄存器（地址为BUFFER_COUNT）：若值非0，说明缓冲区仍有数据，继续循环
 *  } while( portINPUT_BYTE( BUFFER_COUNT ) );
 *
 *  // 缓冲区已空，若高优先级任务被唤醒，则请求上下文切换
 *  if( xHigherPriorityTaskWoken )
 *  {
 *       // 由于xHigherPriorityTaskWoken已设为pdTRUE，需请求上下文切换。
 *       // 具体使用的宏因端口而异，通常是portYIELD_FROM_ISR()或portEND_SWITCHING_ISR()，
 *       // 需参考所用端口的文档说明。
 *       portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
 *  }
 * }
 * @endcode  // Doxygen文档标记，结束示例代码块
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR  // Doxygen分组标记，将该宏归类到xQueueSendFromISR分组（便于文档分类检索）
 * \ingroup QueueManagement  // Doxygen分组标记，将上述分组归属于“队列管理”大分组
 */

// 宏定义：xQueueSendFromISR本质是对xQueueGenericSendFromISR的封装
// 最后一个参数queueSEND_TO_BACK指定“数据发送到队列尾部”，这意味着它的功能与xQueueSendToBackFromISR完全一致
#define xQueueSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )


/**
 * queue.h  // 函数声明所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时支持语法高亮）
 * BaseType_t xQueueGenericSendFromISR(
 *                                         QueueHandle_t    xQueue,  // 第1个参数：目标队列的句柄
 *                                         const    void    *pvItemToQueue,  // 第2个参数：要发送到队列的数据指针
 *                                         BaseType_t  *pxHigherPriorityTaskWoken,  // 第3个参数：指向“高优先级任务是否被唤醒”标记的指针
 *                                         BaseType_t  xCopyPosition  // 第4个参数：数据在队列中的存储位置（头部/尾部）
 *                                     );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 建议优先使用宏 xQueueSendFromISR()、xQueueSendToFrontFromISR() 和 xQueueSendToBackFromISR()，
 * 而非直接调用本函数。对于“无需实际复制数据的信号量”，其等效函数是 xQueueGiveFromISR()。
 *
 * 向队列中发送一个数据项。本函数可安全地在中断服务程序（ISR）中使用（核心特性：中断安全）。
 *
 * 数据项通过“复制”方式存入队列，而非“引用”方式，因此建议仅发送小型数据项，
 * 尤其在从中断服务程序中调用时更应如此——中断需快速执行，大量数据复制会延长中断耗时，影响系统实时性。
 * 多数情况下，更推荐将“数据项的指针”存入队列（大幅减少复制开销，提升中断处理效率）。
 *
 * @param xQueue 要向其发送数据项的目标队列的句柄（需是通过 xQueueCreate 创建的有效句柄，无效句柄可能触发断言）。
 *
 * @param pvItemToQueue 指向要存入队列的数据项的指针。队列可存储的数据项大小在创建时已定义，
 * 因此会从 pvItemToQueue 指向的地址，复制“与队列元素大小相等的字节数”到队列的存储区域（确保数据完整）。
 *
 * @param pxHigherPriorityTaskWoken 该参数是一个指针，用于接收“高优先级任务是否被唤醒”的状态：
 * 当向队列发送数据后，若存在因“等待队列空闲空间”而阻塞的任务（如调用 xQueueSend() 时队列满而阻塞的任务），该任务会被唤醒；
 * 若被唤醒任务的优先级高于当前运行的任务（即中断服务程序所打断的任务），本函数会将 *pxHigherPriorityTaskWoken 设为 pdTRUE。
 * 若该值被设为 pdTRUE，必须在退出中断服务程序前请求一次上下文切换（确保高优先级任务能及时抢占CPU）。
 *
 * @param xCopyPosition 用于指定数据在队列中的存储位置：
 * - 传入 queueSEND_TO_BACK：数据存入队列尾部（遵循“先进先出”FIFO原则，新数据最后被读取）；
 * - 传入 queueSEND_TO_FRONT：数据存入队列头部（适用于高优先级消息，新数据优先被读取）。
 *
 * @return 若数据成功发送到队列，返回 pdTRUE（FreeRTOS 定义的“真”，通常为1）；
 * 若队列已满导致数据无法存入，返回 errQUEUE_FULL（队列满错误标记）。
 *
 * 缓冲IO场景下的示例用法（中断服务程序中每次调用可处理多个数据值）：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 * // 中断服务程序：处理硬件缓冲区的数据接收与队列发送
 * void vBufferISR( void )
 * {
 * char cIn;  // 存储从硬件寄存器读取的单个字节数据
 * BaseType_t xHigherPriorityTaskWokenByPost;  // 标记“高优先级任务是否被唤醒”
 *
 *  // 中断服务程序开始时，初始化标记为“未唤醒高优先级任务”
 *  xHigherPriorityTaskWokenByPost = pdFALSE;
 *
 *  // 循环读取硬件缓冲区，直到缓冲区为空
 *  do
 *  {
 *      // 从硬件接收寄存器（地址为 RX_REGISTER_ADDRESS）读取一个字节
 *      cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );
 *
 *      // 将每个字节发送到队列尾部（通过 xQueueGenericSendFromISR 直接调用）
 *      xQueueGenericSendFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWokenByPost, queueSEND_TO_BACK );
 *
 *  // 检查硬件缓冲区计数寄存器（地址为 BUFFER_COUNT）：若值非0，说明缓冲区仍有数据，继续循环
 *  } while( portINPUT_BYTE( BUFFER_COUNT ) );
 *
 *  // 缓冲区已空，若高优先级任务被唤醒，则请求上下文切换
 *  if( xHigherPriorityTaskWokenByPost )
 *  {
 *       // 由于 xHigherPriorityTaskWokenByPost 已设为 pdTRUE，需请求上下文切换。
 *       // 具体使用的宏因端口而异，通常是 portYIELD_FROM_ISR() 或 portEND_SWITCHING_ISR()，
 *       // 需参考所用端口的文档说明。
 *       portYIELD_FROM_ISR( xHigherPriorityTaskWokenByPost );
 *  }
 * }
 * @endcode  // Doxygen文档标记，结束示例代码块
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR  // Doxygen分组标记，将函数归类到 xQueueSendFromISR 分组
 * \ingroup QueueManagement  // Doxygen分组标记，归属于“队列管理”大分组
 */

// 函数声明：中断安全版队列通用发送函数（PRIVILEGED_FUNCTION 宏标记为特权函数，仅可在特权模式调用）
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue,
                                     const void * const pvItemToQueue,
                                     BaseType_t * const pxHigherPriorityTaskWoken,
                                     const BaseType_t xCopyPosition ) PRIVILEGED_FUNCTION;

// 函数声明：中断安全版信号量“释放”函数（本质是无数据复制的队列发送，PRIVILEGED_FUNCTION 标记为特权函数）
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue,
                              BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

                              
/**
 * queue.h  // 函数声明所在的头文件名称
 * @code{c}  // Doxygen文档标记，指定后续代码块为C语言语法（文档生成时支持语法高亮）
 * BaseType_t xQueueReceiveFromISR(
 *                                     QueueHandle_t    xQueue,  // 第1个参数：要读取数据的目标队列句柄
 *                                     void             *pvBuffer,  // 第2个参数：存储读取数据的缓冲区指针
 *                                     BaseType_t       *pxTaskWoken  // 第3个参数：指向“高优先级任务是否被唤醒”标记的指针（文档此处参数名与声明一致，实际使用需注意统一）
 *                                 );
 * @endcode  // Doxygen文档标记，结束C语言代码块
 */

/**
 * 从队列中读取一个数据项。本函数可安全地在中断服务程序（ISR）中使用（核心特性：中断安全）。
 *
 * @param xQueue 要从中读取数据项的目标队列句柄（需是通过 xQueueCreate 创建的有效句柄，无效句柄可能触发断言）。
 *
 * @param pvBuffer 指向用于存储读取数据的缓冲区的指针。队列中数据项的大小在创建时已定义，
 * 因此会从队列的存储区域复制“与队列元素大小相等的字节数”到 pvBuffer 指向的缓冲区中（确保数据完整读取）。
 *
 * @param pxHigherPriorityTaskWoken （文档中参数名误写为 pxTaskWoken，以函数声明为准）
 * 可能存在任务因“队列满”而阻塞（例如调用 xQueueSend() 时队列无空闲空间），等待队列腾出空间后继续发送数据。
 * 若本次从队列读取数据的操作，导致这类阻塞任务解除阻塞（队列腾出了一个空间），则本函数会将 *pxHigherPriorityTaskWoken 设为 pdTRUE；
 * 若未唤醒任何任务，*pxHigherPriorityTaskWoken 保持原有值不变。
 *
 * @return 若成功从队列中读取到数据项，返回 pdTRUE（FreeRTOS 定义的“真”，通常为1）；
 * 若队列为空（无数据可读取），返回 pdFALSE（“假”，通常为0）。
 *
 * 示例用法：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 *
 * QueueHandle_t xQueue;  // 定义队列句柄变量xQueue（用于跨函数访问）
 *
 * // 任务函数：创建队列并向队列中发送数据
 * void vAFunction( void *pvParameters )
 * {
 * char cValueToPost;  // 存储要发送到队列的字符数据
 * const TickType_t xTicksToWait = ( TickType_t )0xff;  // 队列满时的阻塞等待时间（255个时钟节拍）
 *
 *  // 创建一个队列：容量为10（可存储10个数据项），每个数据项类型为char（1字节）
 *  xQueue = xQueueCreate( 10, sizeof( char ) );
 *  if( xQueue == 0 )  // 检查队列是否创建成功
 *  {
 *      // 队列创建失败（可能是内存不足等原因）
 *  }
 *
 *  // ... （省略其他业务逻辑）
 *
 *  // 向队列发送字符数据（供中断服务程序读取）。若队列已满，当前任务会阻塞xTicksToWait个时钟节拍
 *  cValueToPost = 'a';
 *  xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );
 *  cValueToPost = 'b';
 *  xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );
 *
 *  // ... （继续向队列发送字符数据 ... 当队列满时，当前任务会进入阻塞状态）
 *
 *  cValueToPost = 'c';
 *  xQueueSend( xQueue, ( void * ) &cValueToPost, xTicksToWait );
 * }
 *
 * // 中断服务程序：从队列中读取所有数据并输出
 * void vISR_Routine( void )
 * {
 * BaseType_t xTaskWokenByReceive = pdFALSE;  // 标记“读取操作是否唤醒了其他任务”，初始化为pdFALSE
 * char cRxedChar;  // 存储从队列读取的字符数据
 *
 *  // 循环从队列读取数据：只要读取成功（队列非空），就继续读取
 *  while( xQueueReceiveFromISR( xQueue, ( void * ) &cRxedChar, &xTaskWokenByReceive) )
 *  {
 *      // 成功读取到一个字符，立即输出该字符
 *      vOutputCharacter( cRxedChar );
 *
 *      // 说明：从队列移除数据后，若之前有任务因“队列满”阻塞（等待发送数据），
 *      // xTaskWokenByReceive 会被设为 pdTRUE。无论此循环执行多少次，
 *      // 最多只会唤醒一个等待发送的任务（FreeRTOS 队列唤醒逻辑：每次队列空间变化仅唤醒最高优先级的阻塞任务）。
 *  }
 *
 *  // 若读取操作唤醒了高优先级任务，请求上下文切换
 *  if( xTaskWokenByReceive != ( char ) pdFALSE )  // 文档中此处缺少右括号，修正后逻辑：判断标记是否为pdTRUE
 *  {
 *      taskYIELD();  // 触发上下文切换（注：中断中应使用 portYIELD_FROM_ISR()，示例此处为简化写法，实际需按端口要求调整）
 *  }
 * }
 * @endcode  // Doxygen文档标记，结束示例代码块
 *
 * \defgroup xQueueReceiveFromISR xQueueReceiveFromISR  // Doxygen分组标记，将函数归类到xQueueReceiveFromISR分组
 * \ingroup QueueManagement  // Doxygen分组标记，归属于“队列管理”大分组
 */

// 函数声明：中断安全版队列读取函数（PRIVILEGED_FUNCTION 宏标记为特权函数，仅可在特权模式调用）
// 注意：参数名 pxHigherPriorityTaskWoken 与文档中 pxTaskWoken 一致，实际使用需统一
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue,
                                 void * const pvBuffer,
                                 BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

/*
 * 用于查询队列状态的工具函数，支持在中断服务程序（ISR）中安全使用。
 * 这些工具函数仅应在以下场景中调用：
 * 1. 中断服务程序（ISR）内部；
 * 2. 临界区（critical section）内部。
 * （注：避免在非中断/非临界区调用，防止并发访问导致的状态查询错误）
 */

// 函数声明1：判断队列是否为空（中断安全版）
// 参数：xQueue - 要查询的目标队列句柄
// 返回值：pdTRUE 表示队列为空（无待读取数据）；pdFALSE 表示队列非空（有数据可读取）
BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

// 函数声明2：判断队列是否为满（中断安全版）
// 参数：xQueue - 要查询的目标队列句柄
// 返回值：pdTRUE 表示队列已满（无空闲空间接收新数据）；pdFALSE 表示队列未满（可发送新数据）
BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

// 函数声明3：获取队列中当前待读取的消息数（中断安全版）
// 参数：xQueue - 要查询的目标队列句柄
// 返回值：队列中当前存储的消息数量（UBaseType_t 为无符号类型，范围通常为 0 ~ 队列最大容量）
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

// 条件编译：仅当 configUSE_CO_ROUTINES（协程功能开关）设为1时，才编译以下协程专用队列函数
#if ( configUSE_CO_ROUTINES == 1 )

/*
 * 说明：上方定义的函数（如xQueueSendFromISR、xQueueReceiveFromISR等）用于
 * 任务（Task）之间或中断与任务之间传递数据；下方定义的函数是协程（Co-routine）场景的等效实现，
 * 用于协程之间或中断与协程之间传递数据。
 *
 * 注意：以下函数仅应在协程宏（croutine.h中定义）的内部实现中调用，
 * 不应直接从应用代码中调用。应用代码需使用 croutine.h 中定义的宏封装函数（如xCoRoutineSend）。
 */
    // 函数声明1：中断安全版协程专用队列发送函数
    // 功能：从中断中向队列发送数据，供协程读取
    // 参数：
    //   xQueue - 目标队列句柄
    //   pvItemToQueue - 待发送数据的指针
    //   xCoRoutinePreviouslyWoken - 标记“此前是否已唤醒过协程”（避免重复唤醒）
    // 返回值：pdTRUE表示发送成功；pdFALSE表示发送失败（如队列满）
    BaseType_t xQueueCRSendFromISR( QueueHandle_t xQueue,
                                    const void * pvItemToQueue,
                                    BaseType_t xCoRoutinePreviouslyWoken );

    // 函数声明2：中断安全版协程专用队列接收函数
    // 功能：从中断中从队列读取数据（通常是协程之前发送到队列的数据）
    // 参数：
    //   xQueue - 目标队列句柄
    //   pvBuffer - 存储读取数据的缓冲区指针
    //   pxTaskWoken - 标记“是否唤醒了高优先级任务”（协程与任务可能共享队列，需兼容任务唤醒）
    // 返回值：pdTRUE表示读取成功；pdFALSE表示读取失败（如队列为空）
    BaseType_t xQueueCRReceiveFromISR( QueueHandle_t xQueue,
                                       void * pvBuffer,
                                       BaseType_t * pxTaskWoken );

    // 函数声明3：协程专用队列发送函数（非中断版）
    // 功能：从协程中向队列发送数据，支持阻塞等待
    // 参数：
    //   xQueue - 目标队列句柄
    //   pvItemToQueue - 待发送数据的指针
    //   xTicksToWait - 队列满时的阻塞等待时间（单位：时钟节拍）
    // 返回值：pdTRUE表示发送成功；pdFALSE表示等待超时或发送失败
    BaseType_t xQueueCRSend( QueueHandle_t xQueue,
                             const void * pvItemToQueue,
                             TickType_t xTicksToWait );

    // 函数声明4：协程专用队列接收函数（非中断版）
    // 功能：从协程中从队列读取数据，支持阻塞等待
    // 参数：
    //   xQueue - 目标队列句柄
    //   pvBuffer - 存储读取数据的缓冲区指针
    //   xTicksToWait - 队列为空时的阻塞等待时间（单位：时钟节拍）
    // 返回值：pdTRUE表示读取成功；pdFALSE表示等待超时或读取失败
    BaseType_t xQueueCRReceive( QueueHandle_t xQueue,
                                void * pvBuffer,
                                TickType_t xTicksToWait );

#endif /* 结束 configUSE_CO_ROUTINES == 1 的条件编译 */

/*
 * 说明：以下函数仅用于FreeRTOS内部实现，不应从应用代码直接调用。
 * 应用代码需使用封装后的信号量API：
 * - 创建互斥锁：xSemaphoreCreateMutex()
 * - 创建计数信号量：xSemaphoreCreateCounting()
 * - 获取互斥锁持有者：xSemaphoreGetMutexHolder()
 */

// 函数声明1：创建互斥锁（底层实现，基于队列）
// 参数：ucQueueType - 队列类型（指定为互斥锁类型，如queueQUEUE_IS_MUTEX）
// 返回值：成功返回互斥锁句柄（本质是队列句柄）；失败返回NULL（如内存不足）
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;

// 条件编译1：仅当支持静态内存分配（configSUPPORT_STATIC_ALLOCATION == 1）时编译
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    // 函数声明2：静态创建互斥锁（底层实现，使用用户提供的静态内存）
    // 参数：
    //   ucQueueType - 队列类型（互斥锁类型）
    //   pxStaticQueue - 指向StaticQueue_t结构体的指针（用户提前分配的静态内存块）
    // 返回值：成功返回互斥锁句柄；失败返回NULL（如静态内存指针无效）
    QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType,
                                           StaticQueue_t * pxStaticQueue ) PRIVILEGED_FUNCTION;
#endif

// 条件编译2：仅当支持计数信号量（configUSE_COUNTING_SEMAPHORES == 1）时编译
#if ( configUSE_COUNTING_SEMAPHORES == 1 )
    // 函数声明3：创建计数信号量（底层实现，基于队列）
    // 参数：
    //   uxMaxCount - 信号量最大计数（队列最大容量）
    //   uxInitialCount - 信号量初始计数（队列初始消息数）
    // 返回值：成功返回计数信号量句柄；失败返回NULL
    QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount,
                                                 const UBaseType_t uxInitialCount ) PRIVILEGED_FUNCTION;
#endif

// 条件编译3：仅当同时支持计数信号量和静态内存分配时编译
#if ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
    // 函数声明4：静态创建计数信号量（底层实现，使用用户提供的静态内存）
    // 参数：
    //   uxMaxCount - 信号量最大计数
    //   uxInitialCount - 信号量初始计数
    //   pxStaticQueue - 指向StaticQueue_t结构体的静态内存指针
    // 返回值：成功返回计数信号量句柄；失败返回NULL
    QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount,
                                                       const UBaseType_t uxInitialCount,
                                                       StaticQueue_t * pxStaticQueue ) PRIVILEGED_FUNCTION;
#endif

// 函数声明5：获取信号量（底层实现，对应xSemaphoreTake()）
// 参数：
//   xQueue - 信号量句柄（本质是队列句柄）
//   xTicksToWait - 信号量不可用时的阻塞等待时间（单位：时钟节拍）
// 返回值：pdTRUE表示成功获取；pdFALSE表示等待超时或获取失败
BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue,
                                TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

// 条件编译4：仅当支持互斥锁且启用“获取互斥锁持有者”功能时编译
#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )
    // 函数声明6：获取互斥锁持有者（任务句柄，非中断版）
    // 参数：xSemaphore - 互斥锁句柄
    // 返回值：成功返回持有互斥锁的任务句柄；失败返回NULL（如互斥锁未被持有）
    TaskHandle_t xQueueGetMutexHolder( QueueHandle_t xSemaphore ) PRIVILEGED_FUNCTION;
    
    // 函数声明7：获取互斥锁持有者（任务句柄，中断安全版）
    // 参数：xSemaphore - 互斥锁句柄
    // 返回值：成功返回持有互斥锁的任务句柄；失败返回NULL
    TaskHandle_t xQueueGetMutexHolderFromISR( QueueHandle_t xSemaphore ) PRIVILEGED_FUNCTION;
#endif

/*
 * 仅供内部使用。应使用 xSemaphoreTakeRecursive() 或
 * xSemaphoreGiveRecursive() 而非直接调用这些函数。
 */
BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex,
                                     TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
BaseType_t xQueueGiveMutexRecursive( QueueHandle_t xMutex ) PRIVILEGED_FUNCTION;

/*
 * 将队列重置为初始的空状态。
 * 注：该函数的返回值目前已过时（obsolete），现在始终返回 pdPASS。
 */
#define xQueueReset( xQueue )    xQueueGenericReset( ( xQueue ), pdFALSE )

/*
 * 队列注册表（Queue Registry）的作用是：为“支持内核感知的调试器”（Kernel Aware Debugger）
 * 提供定位队列、信号量和互斥锁的途径。
 * 若你希望队列、信号量或互斥锁的句柄能被内核感知调试器识别，可调用 vQueueAddToRegistry()
 * 将其句柄添加到注册表中。若不使用内核感知调试器，则可忽略此函数。
 *
 * 1. 配置宏说明：
 *    - configQUEUE_REGISTRY_SIZE 定义了注册表可存储的“句柄最大数量”；
 *    - 若需启用注册表功能，必须在 FreeRTOSConfig.h 中设置 configQUEUE_REGISTRY_SIZE > 0；
 *    - 该宏的值不影响“可创建的队列、信号量、互斥锁总数”，仅限制“注册表可记录的句柄数”。
 *
 * 2. 重复添加规则：
 *    - 若对同一个 xQueue 参数（同一个句柄）多次调用 vQueueAddToRegistry()，
 *      注册表会存储“最近一次调用时传入的 pcQueueName 参数”（覆盖旧名称）。
 *
 * @参数 xQueue：要添加到注册表的队列/信号量/互斥锁句柄。
 *              - 队列句柄：由 xQueueCreate() 调用返回；
 *              - 信号量/互斥锁句柄：也可传入（因信号量/互斥锁本质是特殊队列）。
 *
 * @参数 pcQueueName：与句柄关联的“名称字符串”。
 *                    - 该名称会在“内核感知调试器”中显示，用于识别句柄；
 *                    - 注册表仅存储该字符串的“指针”，因此字符串必须是“持久化的”
 *                      （如全局变量或存储在 ROM/Flash 中的常量字符串），不可是栈上的临时字符串。
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )  // 仅当注册表容量配置>0时，才编译该函数声明
    void vQueueAddToRegistry( QueueHandle_t xQueue,
                              const char * pcQueueName ) PRIVILEGED_FUNCTION;  // 特权函数（仅内核/信任代码调用）
#endif

/*
 * 队列注册表（Queue Registry）的作用是：为“支持内核感知的调试器”（Kernel Aware Debugger）
 * 提供定位队列、信号量和互斥锁的途径。
 * 若你希望队列、信号量或互斥锁的句柄能被内核感知调试器识别，可调用 vQueueAddToRegistry()
 * 将其句柄添加到注册表中；若要从注册表中移除，可调用 vQueueUnregisterQueue()。
 * 若不使用内核感知调试器，则可忽略此函数。
 *
 * @参数 xQueue：要从注册表中移除的队列/信号量/互斥锁句柄。
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )  // 仅当注册表容量配置>0时，才编译该函数声明
    void vQueueUnregisterQueue( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;  // 特权函数（仅内核/信任代码调用）
#endif

/*
 * 队列注册表（Queue Registry）的作用是：为“支持内核感知的调试器”（Kernel Aware Debugger）
 * 提供定位队列、信号量和互斥锁的途径。
 * 调用 pcQueueGetName() 可通过队列句柄在队列注册表中查询并返回该队列的名称。
 *
 * @参数 xQueue：要查询名称的队列/信号量/互斥锁句柄。
 * @返回值：
 *   - 若该句柄在注册表中，则返回指向其名称字符串的指针；
 *   - 若该句柄不在注册表中，则返回 NULL。
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )  // 仅当注册表容量配置>0时，才编译该函数声明
    const char * pcQueueGetName( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;  // 特权函数（仅内核/信任代码调用）
#endif

/*
 * Generic version of the function used to create a queue using dynamic memory
 * allocation.  This is called by other functions and macros that create other
 * RTOS objects that use the queue structure as their base.
 */
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,
                                       const UBaseType_t uxItemSize,
                                       const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

/*
 * Generic version of the function used to create a queue using dynamic memory
 * allocation.  This is called by other functions and macros that create other
 * RTOS objects that use the queue structure as their base.
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength,
                                             const UBaseType_t uxItemSize,
                                             uint8_t * pucQueueStorage,
                                             StaticQueue_t * pxStaticQueue,
                                             const uint8_t ucQueueType ) PRIVILEGED_FUNCTION;
#endif

/*
 * Generic version of the function used to retrieve the buffers of statically
 * created queues. This is called by other functions and macros that retrieve
 * the buffers of other statically created RTOS objects that use the queue
 * structure as their base.
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    BaseType_t xQueueGenericGetStaticBuffers( QueueHandle_t xQueue,
                                              uint8_t ** ppucQueueStorage,
                                              StaticQueue_t ** ppxStaticQueue ) PRIVILEGED_FUNCTION;
#endif

/*
 * 队列集合（Queue Sets）提供了一种机制：允许任务在“从多个队列或信号量读取数据”的操作上
 * 同时阻塞（挂起）——即任务可等待多个同步对象，只要其中任意一个对象可读取（有数据/可获取），任务就会被唤醒。
 *
 * 有关该函数的使用示例，请参考 FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c。
 *
 * 队列集合的使用前提：
 * 1. 必须先通过调用 xQueueCreateSet() 显式创建队列集合；
 * 2. 创建后，可通过调用 xQueueAddToSet() 将标准 FreeRTOS 队列或信号量添加到集合中；
 * 3. 之后调用 xQueueSelectFromSet() 即可判断：集合中是否有队列/信号量处于“可读取/可获取”状态（即操作能成功），并返回对应的对象句柄。
 *
 * 注意事项：
 * 注1：参考官方文档 https://www.FreeRTOS.org/RTOS-queue-sets.html，了解为何实际应用中极少需要队列集合——
 *      因为存在更简单的方法实现“阻塞等待多个对象”（如使用事件组、任务通知等）。
 * 注2：若队列集合中包含互斥锁，任务阻塞等待该集合时，不会触发“互斥锁持有者的优先级继承”——
 *      即互斥锁的优先级继承机制在队列集合场景下失效，需谨慎使用。
 * 注3：每个被添加到队列集合的“队列/信号量”，其每个“存储单元”（队列的消息位、信号量的计数位）都需额外占用 4 字节 RAM；
 *      因此，最大计数值很高的计数信号量不应添加到队列集合中（会导致 RAM 开销过大）。
 * 注4：对于已添加到队列集合的成员（队列/信号量），禁止直接执行“读取（队列）”或“获取（信号量）”操作；
 *      必须先调用 xQueueSelectFromSet() 并获取到该成员的句柄后，才能对其执行操作（否则会破坏集合的事件跟踪逻辑）。
 *
 * @参数 uxEventQueueLength：队列集合用于存储“其成员（队列/信号量）产生的事件”，该参数指定“可同时排队的最大事件数”。
 *                          为确保事件不丢失，uxEventQueueLength 应设置为“所有添加到集合的成员的‘容量/最大计数’之和”，具体规则：
 *                          - 二进制信号量、互斥锁的“容量”视为 1；
 *                          - 队列的“容量”为其创建时指定的消息数（uxLength）；
 *                          - 计数信号量的“容量”为其创建时指定的最大计数值（uxMaxCount）。
 *                          示例：
 *                          1. 集合包含“长度5的队列 + 长度12的队列 + 二进制信号量”：uxEventQueueLength = 5 + 12 + 1 = 18；
 *                          2. 集合包含3个二进制信号量：uxEventQueueLength = 1 + 1 + 1 = 3；
 *                          3. 集合包含“最大计数5的计数信号量 + 最大计数3的计数信号量”：uxEventQueueLength = 5 + 3 = 8。
 *
 * @返回值：
 *   - 若队列集合创建成功，返回创建的队列集合句柄（QueueSetHandle_t 类型）；
 *   - 若创建失败（如动态内存不足），返回 NULL。
 */
#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )  // 需同时启用“队列集合”和“动态内存分配”
    QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength ) PRIVILEGED_FUNCTION;  // 特权函数（仅内核/信任代码调用）
#endif

/*
 * Adds a queue or semaphore to a queue set that was previously created by a
 * call to xQueueCreateSet().
 *
 * See FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c for an example using this
 * function.
 *
 * Note 1:  A receive (in the case of a queue) or take (in the case of a
 * semaphore) operation must not be performed on a member of a queue set unless
 * a call to xQueueSelectFromSet() has first returned a handle to that set member.
 *
 * @param xQueueOrSemaphore The handle of the queue or semaphore being added to
 * the queue set (cast to an QueueSetMemberHandle_t type).
 *
 * @param xQueueSet The handle of the queue set to which the queue or semaphore
 * is being added.
 *
 * @return If the queue or semaphore was successfully added to the queue set
 * then pdPASS is returned.  If the queue could not be successfully added to the
 * queue set because it is already a member of a different queue set then pdFAIL
 * is returned.
 */
#if ( configUSE_QUEUE_SETS == 1 )
    BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore,
                               QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;
#endif

/*
 * Removes a queue or semaphore from a queue set.  A queue or semaphore can only
 * be removed from a set if the queue or semaphore is empty.
 *
 * See FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c for an example using this
 * function.
 *
 * @param xQueueOrSemaphore The handle of the queue or semaphore being removed
 * from the queue set (cast to an QueueSetMemberHandle_t type).
 *
 * @param xQueueSet The handle of the queue set in which the queue or semaphore
 * is included.
 *
 * @return If the queue or semaphore was successfully removed from the queue set
 * then pdPASS is returned.  If the queue was not in the queue set, or the
 * queue (or semaphore) was not empty, then pdFAIL is returned.
 */
#if ( configUSE_QUEUE_SETS == 1 )
    BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore,
                                    QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;
#endif

/*
 * xQueueSelectFromSet() selects from the members of a queue set a queue or
 * semaphore that either contains data (in the case of a queue) or is available
 * to take (in the case of a semaphore).  xQueueSelectFromSet() effectively
 * allows a task to block (pend) on a read operation on all the queues and
 * semaphores in a queue set simultaneously.
 *
 * See FreeRTOS/Source/Demo/Common/Minimal/QueueSet.c for an example using this
 * function.
 *
 * Note 1:  See the documentation on https://www.FreeRTOS.org/RTOS-queue-sets.html
 * for reasons why queue sets are very rarely needed in practice as there are
 * simpler methods of blocking on multiple objects.
 *
 * Note 2:  Blocking on a queue set that contains a mutex will not cause the
 * mutex holder to inherit the priority of the blocked task.
 *
 * Note 3:  A receive (in the case of a queue) or take (in the case of a
 * semaphore) operation must not be performed on a member of a queue set unless
 * a call to xQueueSelectFromSet() has first returned a handle to that set member.
 *
 * @param xQueueSet The queue set on which the task will (potentially) block.
 *
 * @param xTicksToWait The maximum time, in ticks, that the calling task will
 * remain in the Blocked state (with other tasks executing) to wait for a member
 * of the queue set to be ready for a successful queue read or semaphore take
 * operation.
 *
 * @return xQueueSelectFromSet() will return the handle of a queue (cast to
 * a QueueSetMemberHandle_t type) contained in the queue set that contains data,
 * or the handle of a semaphore (cast to a QueueSetMemberHandle_t type) contained
 * in the queue set that is available, or NULL if no such queue or semaphore
 * exists before before the specified block time expires.
 */
#if ( configUSE_QUEUE_SETS == 1 )
    QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet,
                                                const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
#endif

/*
 * A version of xQueueSelectFromSet() that can be used from an ISR.
 */
#if ( configUSE_QUEUE_SETS == 1 )
    QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet ) PRIVILEGED_FUNCTION;
#endif

/* Not public API functions. */
void vQueueWaitForMessageRestricted( QueueHandle_t xQueue,
                                     TickType_t xTicksToWait,
                                     const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;
BaseType_t xQueueGenericReset( QueueHandle_t xQueue,
                               BaseType_t xNewQueue ) PRIVILEGED_FUNCTION;

#if ( configUSE_TRACE_FACILITY == 1 )
    void vQueueSetQueueNumber( QueueHandle_t xQueue,
                               UBaseType_t uxQueueNumber ) PRIVILEGED_FUNCTION;
#endif

#if ( configUSE_TRACE_FACILITY == 1 )
    UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
#endif

#if ( configUSE_TRACE_FACILITY == 1 )
    uint8_t ucQueueGetQueueType( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
#endif

UBaseType_t uxQueueGetQueueItemSize( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
UBaseType_t uxQueueGetQueueLength( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* QUEUE_H */
