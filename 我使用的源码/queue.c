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

#include <stdlib.h>
#include <string.h>

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 可以防止 task.h 重新定义
 * 所有 API 函数以使用 MPU 包装器。这种重新定义只应该在
 * 从应用程序文件中包含 task.h 时进行。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if ( configUSE_CO_ROUTINES == 1 )
    #include "croutine.h"
#endif

/* MPU 端口要求上面的头文件包含时定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，
 * 但在本文件中不需要，以生成正确的特权模式与非特权模式的链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE


/* 用于 cRxLock 和 cTxLock 结构体成员的常量。 */
#define queueUNLOCKED             ( ( int8_t ) -1 )  // 队列未锁定
#define queueLOCKED_UNMODIFIED    ( ( int8_t ) 0 )   // 队列已锁定且未修改
#define queueINT8_MAX             ( ( int8_t ) 127 ) // int8_t 类型的最大值

/* 当 Queue_t 结构体用于表示基础队列时，其 pcHead 和 pcTail 成员
 * 用作指向队列存储区域的指针。当 Queue_t 结构体用于表示互斥锁时，
 * pcHead 和 pcTail 指针不是必需的，此时 pcHead 指针被设置为 NULL，
 * 以指示该结构体转而持有指向互斥锁持有者（如果有的话）的指针。
 * 为 pcHead 和结构体成员映射替代名称，以确保代码的可读性。
 * QueuePointers_t 和 SemaphoreData_t 类型用于形成联合体，
 * 因为它们的使用是互斥的，取决于队列的用途。 */
#define uxQueueType               pcHead  // 将 pcHead 映射为 uxQueueType，用于标识队列类型
#define queueQUEUE_IS_MUTEX       NULL    // 定义标识"队列用作互斥锁"的值为 NULL

// 定义队列指针结构体，用于表示普通队列的数据指针信息
typedef struct QueuePointers
{
    int8_t * pcTail;     /**< 指向队列存储区域末尾的字节。实际分配的内存比存储队列项所需的多一个字节，此指针用作结束标记。 */
    int8_t * pcReadFrom; /**< 当结构体用作队列时，指向最后一次读取队列项的位置。 */
} QueuePointers_t;

// 定义信号量数据结构体，用于表示互斥锁的持有者和递归计数信息
typedef struct SemaphoreData
{
    TaskHandle_t xMutexHolder;        /**< 持有此互斥锁的任务句柄。 */
    UBaseType_t uxRecursiveCallCount; /**< 当结构体用作递归互斥锁时，记录递归"获取"互斥锁的次数。 */
} SemaphoreData_t;

/* 信号量实际上不存储或复制数据，因此其项大小为0。 */
#define queueSEMAPHORE_QUEUE_ITEM_LENGTH    ( ( UBaseType_t ) 0 )  // 信号量的队列项大小（0，因无需存储数据）
#define queueMUTEX_GIVE_BLOCK_TIME          ( ( TickType_t ) 0U )  // 释放互斥锁时的阻塞时间（0，即非阻塞）

#if ( configUSE_PREEMPTION == 0 )

/* 若使用协作式调度器，则不应仅因高优先级任务被唤醒而执行任务切换。 */
    #define queueYIELD_IF_USING_PREEMPTION()  // 协作式调度器下，空定义（不执行任务切换）
#else
    #if ( configNUMBER_OF_CORES == 1 )
        // 单核抢占式调度器：调用端口相关的API内任务切换函数
        #define queueYIELD_IF_USING_PREEMPTION()    portYIELD_WITHIN_API()
    #else /* #if ( configNUMBER_OF_CORES == 1 ) */
        // 多核抢占式调度器：调用任务API内的任务切换函数
        #define queueYIELD_IF_USING_PREEMPTION()    vTaskYieldWithinAPI()
    #endif /* #if ( configNUMBER_OF_CORES == 1 ) */
#endif

/*
 * 调度器所使用的队列的定义。
 * 队列中的项目通过复制方式入队，而非引用。有关基本原理请参见以下链接：
 * https://www.FreeRTOS.org/Embedded-RTOS-Queues.html
 */
typedef struct QueueDefinition /* 使用旧命名约定是为了避免破坏内核感知调试器。 */
{
    int8_t * pcHead;           /**< 指向队列存储区域的起始位置。 */
    int8_t * pcWriteTo;        /**< 指向存储区域中下一个可用的空闲位置。 */

    union
    {
        QueuePointers_t xQueue;     /**< 当此结构用作队列时专用的数据。 */
        SemaphoreData_t xSemaphore; /**< 当此结构用作信号量时专用的数据。 */
    } u;

    List_t xTasksWaitingToSend;             /**< 因阻塞等待向此队列发送数据而被阻塞的任务列表。按优先级顺序存储。 */
    List_t xTasksWaitingToReceive;          /**< 因阻塞等待从此队列接收数据而被阻塞的任务列表。按优先级顺序存储。 */

    volatile UBaseType_t uxMessagesWaiting; /**< 当前在队列中的项目数量。 */
    UBaseType_t uxLength;                   /**< 队列的长度，定义为它能容纳的项目数量，而非字节数。 */
    UBaseType_t uxItemSize;                 /**< 队列将容纳的每个项目的大小。 */

    volatile int8_t cRxLock;                /**< 存储队列锁定期间从队列接收（移除）的项目数量。队列未锁定时设置为queueUNLOCKED。 */
    volatile int8_t cTxLock;                /**< 存储队列锁定期间发送到队列（添加）的项目数量。队列未锁定时设置为queueUNLOCKED。 */

    #if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        uint8_t ucStaticallyAllocated; /**< 若队列使用的内存是静态分配的，则设置为pdTRUE，以确保不会尝试释放该内存。 */
    #endif

    #if ( configUSE_QUEUE_SETS == 1 )
        struct QueueDefinition * pxQueueSetContainer; /**< 指向此队列所属的队列集（如果有的话）。 */
    #endif

    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxQueueNumber; /**< 用于跟踪的队列编号。 */
        uint8_t ucQueueType;       /**< 标识队列类型（如普通队列、互斥锁等）。 */
    #endif
} xQUEUE;

/* 上面保留了旧的xQUEUE名称，然后在下面将其重定义为新的Queue_t名称，
 * 以支持旧版内核感知调试器。 */
typedef xQUEUE Queue_t;

/*-----------------------------------------------------------*/

/*
 * 队列注册表仅用于支持内核感知调试器定位队列结构体。
 * 它没有其他用途，因此是一个可选组件。
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )  // 仅当队列注册表大小配置大于0时，才编译以下代码

/* 存储在队列注册表数组中的数据类型。通过为每个队列分配名称，
 * 可让内核感知调试器的使用体验更友好（调试时能通过名称识别队列）。 */
    typedef struct QUEUE_REGISTRY_ITEM
    {
        const char * pcQueueName;  // 队列的名称（字符串指针，指向用户定义的队列名）
        QueueHandle_t xHandle;     // 对应的队列句柄（用于关联名称与实际队列结构体）
    } xQueueRegistryItem;

/* 上面保留了旧的xQueueRegistryItem名称，然后在下面将其重定义为新的QueueRegistryItem_t名称，
 * 以支持旧版内核感知调试器。 */
    typedef xQueueRegistryItem QueueRegistryItem_t;

/* 队列注册表本质是一个QueueRegistryItem_t类型的数组。
 * 数组中某结构体的pcQueueName成员为NULL时，表示该位置为空（未注册队列）。 */

/* MISRA规则8.4.2引用[声明应可见] */
/* 更多细节请参考：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-84 */
/* coverity[misra_c_2012_rule_8_4_violation] */  // 屏蔽Coverity工具对MISRA规则8.4的违规告警
    PRIVILEGED_DATA QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];  // 队列注册表数组（大小由配置决定）

#endif /* configQUEUE_REGISTRY_SIZE */  // 条件编译结束

/*
 * 解锁之前通过 prvLockQueue 调用锁定的队列。锁定队列并不会
 * 阻止中断服务程序（ISR）向队列添加或移除项目，但会阻止
 * ISR 从队列事件列表中移除任务。如果 ISR 发现队列已锁定，
 * 它会递增相应的队列锁定计数，以指示可能需要解除某个任务的阻塞。
 * 当队列解锁时，这些锁定计数会被检查，并采取相应的操作。
 */
static void prvUnlockQueue( Queue_t * const pxQueue ) PRIVILEGED_FUNCTION;

/*
 * 使用临界区判断队列中是否有数据。
 *
 * @return 如果队列为空则返回 pdTRUE，否则返回 pdFALSE。
 */
static BaseType_t prvIsQueueEmpty( const Queue_t * pxQueue ) PRIVILEGED_FUNCTION;

/*
 * 使用临界区判断队列中是否有空间。
 *
 * @return 如果队列已满则返回 pdTRUE，否则返回 pdFALSE。
 */
static BaseType_t prvIsQueueFull( const Queue_t * pxQueue ) PRIVILEGED_FUNCTION;

/*
 * 将一个项目复制到队列中，可选择复制到队列头部或队列尾部。
 */
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue,    // 目标队列的指针
                                      const void * pvItemToQueue, // 待入队项目的数据源指针（const表示数据源不可修改）
                                      const BaseType_t xPosition )// 入队位置：pdTRUE=头部，pdFALSE=尾部
                                      PRIVILEGED_FUNCTION;        // 标记为特权函数，仅内核可调用

/*
 * 从队列中复制一个项目到外部缓冲区。
 */
static void prvCopyDataFromQueue( Queue_t * const pxQueue,  // 目标队列的指针
                                  void * const pvBuffer )   // 存储出队项目的缓冲区指针
                                  PRIVILEGED_FUNCTION;      // 标记为特权函数，仅内核可调用

#if ( configUSE_QUEUE_SETS == 1 )  // 仅当启用队列集功能时，编译以下代码

/*
 * 检查队列是否属于某个队列集；若属于，则通知队列集该队列已包含数据（可被读取）。
 */
    static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue )  // 待检查的队列指针
                                                 PRIVILEGED_FUNCTION;              // 标记为特权函数，仅内核可调用
#endif

/*
 * 在 Queue_t 结构体通过静态或动态方式分配后调用，用于初始化结构体的成员。
 */
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength,    // 队列长度（可容纳的项目数量）
                        const UBaseType_t uxItemSize,      // 每个项目的大小（字节数）
                        uint8_t * pucQueueStorage,         // 指向队列存储区域的指针（用于存储队列项）
                        const uint8_t ucQueueType,         // 队列类型（如普通队列、互斥锁、计数信号量等）
                        Queue_t * pxNewQueue )             // 指向待初始化的 Queue_t 结构体
                        PRIVILEGED_FUNCTION;               // 标记为特权函数，仅内核可调用

/*
 * 互斥锁是一种特殊类型的队列。创建互斥锁时，首先创建队列，
 * 然后调用 prvInitialiseMutex() 将队列配置为互斥锁。
 */
#if ( configUSE_MUTEXES == 1 )  // 仅当启用互斥锁功能时编译
    static void prvInitialiseMutex( Queue_t * pxNewQueue )  // 指向待配置为互斥锁的队列结构体
                                   PRIVILEGED_FUNCTION;     // 标记为特权函数，仅内核可调用
#endif

#if ( configUSE_MUTEXES == 1 )  // 仅当启用互斥锁功能时编译

/*
 * 若等待互斥锁的任务导致互斥锁持有者继承了优先级，但等待任务超时，
 * 则持有者应取消继承该优先级——但仅降低到等待同一互斥锁的其他任务的最高优先级。
 * 此函数返回该优先级。
 */
    static UBaseType_t prvGetDisinheritPriorityAfterTimeout( const Queue_t * const pxQueue )  // 指向相关互斥锁的队列结构体
                                                            PRIVILEGED_FUNCTION;            // 标记为特权函数，仅内核可调用
#endif
/*-----------------------------------------------------------*/

/*
 * 用于将队列标记为锁定状态的宏。锁定队列可防止中断服务程序（ISR）
 * 访问队列的事件列表（即阻塞任务列表）。
 */
#define prvLockQueue( pxQueue )                            \
    taskENTER_CRITICAL();                                  \
    {                                                      \
        if( ( pxQueue )->cRxLock == queueUNLOCKED )        \
        {                                                  \
            ( pxQueue )->cRxLock = queueLOCKED_UNMODIFIED; \
        }                                                  \
        if( ( pxQueue )->cTxLock == queueUNLOCKED )        \
        {                                                  \
            ( pxQueue )->cTxLock = queueLOCKED_UNMODIFIED; \
        }                                                  \
    }                                                      \
    taskEXIT_CRITICAL()

/*
 * 用于增加队列数据结构中 cTxLock 成员（发送锁定计数）的宏。
 * 该计数的上限为系统中的任务总数——因为最多只能唤醒系统中所有的任务，
 * 无需记录超过任务总数的锁定期间操作次数。
 */
#define prvIncrementQueueTxLock( pxQueue, cTxLock )                           \
    do {                                                                      \
        // 获取当前系统中已创建的任务总数（UBaseType_t 为无符号基础类型，用于计数）
        const UBaseType_t uxNumberOfTasks = uxTaskGetNumberOfTasks();         \
        // 判断当前锁定计数（cTxLock）是否小于系统任务总数
        if( ( UBaseType_t ) ( cTxLock ) < uxNumberOfTasks )                   \
        {                                                                     \
            // 断言检查：确保当前锁定计数未达到 int8_t 类型的最大值（避免溢出）
            configASSERT( ( cTxLock ) != queueINT8_MAX );                     \
            // 锁定计数加 1：将 cTxLock 转换为 int8_t 类型后加 1，再赋值给队列的 cTxLock 成员
            ( pxQueue )->cTxLock = ( int8_t ) ( ( cTxLock ) + ( int8_t ) 1 ); \
        }                                                                     \
    } while( 0 )  // do-while(0) 结构确保宏在任何调用场景下都能正确展开（如单独一行、带分号等）

/*
 * Macro to increment cRxLock member of the queue data structure. It is
 * capped at the number of tasks in the system as we cannot unblock more
 * tasks than the number of tasks in the system.
 */
#define prvIncrementQueueRxLock( pxQueue, cRxLock )                           \
    do {                                                                      \
        const UBaseType_t uxNumberOfTasks = uxTaskGetNumberOfTasks();         \
        if( ( UBaseType_t ) ( cRxLock ) < uxNumberOfTasks )                   \
        {                                                                     \
            configASSERT( ( cRxLock ) != queueINT8_MAX );                     \
            ( pxQueue )->cRxLock = ( int8_t ) ( ( cRxLock ) + ( int8_t ) 1 ); \
        }                                                                     \
    } while( 0 )
/*-----------------------------------------------------------*/

// 通用队列重置函数：将队列恢复到初始状态（清空数据、重置指针、处理阻塞任务）
// 支持“新队列初始化”和“已有队列重置”两种场景
BaseType_t xQueueGenericReset( QueueHandle_t xQueue,  // 待重置的队列句柄
                               BaseType_t xNewQueue )  // 重置模式：pdTRUE=新队列初始化，pdFALSE=已有队列重置
{
    BaseType_t xReturn = pdPASS;  // 函数返回值，默认成功（pdPASS）
    Queue_t * const pxQueue = xQueue;  // 将队列句柄转换为内核内部控制结构指针（Queue_t*）

    // 跟踪函数入口：记录队列重置事件（供调试工具使用，如Trace Recorder）
    traceENTER_xQueueGenericReset( xQueue, xNewQueue );

    // 断言：确保队列句柄非空（避免无效指针操作）
    configASSERT( pxQueue );

    // 合法性校验：满足以下条件才执行重置操作
    if( ( pxQueue != NULL ) &&  // 1. 队列句柄有效
        ( pxQueue->uxLength >= 1U ) &&  // 2. 队列容量≥1（有效队列）
        /* 3. 检查“队列容量 × 项目大小”是否溢出：
           SIZE_MAX是系统最大可表示的字节数，若 SIZE_MAX / 容量 ≥ 项目大小，
           则容量×项目大小 ≤ SIZE_MAX，无溢出风险 */
        ( ( SIZE_MAX / pxQueue->uxLength ) >= pxQueue->uxItemSize ) )
    {
        // 进入临界区：禁止中断和任务切换，确保重置操作的原子性（避免多任务/中断干扰）
        taskENTER_CRITICAL();
        {
            // 1. 初始化队列存储区末尾指针（pcTail）：指向存储区最后一个字节的下一位（环形缓冲区结束标记）
            pxQueue->u.xQueue.pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize );
            
            // 2. 重置队列项目计数：当前队列空，无待处理项目
            pxQueue->uxMessagesWaiting = ( UBaseType_t ) 0U;
            
            // 3. 重置写入指针：初始写入位置为存储区起始地址（pcHead）
            pxQueue->pcWriteTo = pxQueue->pcHead;
            
            // 4. 重置读取指针：初始读取位置为“存储区末尾前一个项目”（环形缓冲区特性，确保首次读取从pcHead开始）
            pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - 1U ) * pxQueue->uxItemSize );
            
            // 5. 重置队列锁定状态：接收和发送方向均设为“未锁定”（queueUNLOCKED = -1）
            pxQueue->cRxLock = queueUNLOCKED;
            pxQueue->cTxLock = queueUNLOCKED;

            // 分支1：已有队列重置（xNewQueue = pdFALSE）——用于清空已有队列，保留阻塞任务列表逻辑
            if( xNewQueue == pdFALSE )
            {
                /* 逻辑说明：
                   - 等待读取的任务（xTasksWaitingToReceive）：重置后队列仍为空，任务继续阻塞，无需处理；
                   - 等待写入的任务（xTasksWaitingToSend）：重置后队列空（有空间），需唤醒一个任务来写入数据。 */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    // 从“等待发送”列表中移除一个最高优先级任务（唤醒任务）
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        // 若唤醒的任务优先级高于当前任务，触发任务切换（抢占式调度器生效）
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记：标记此分支已执行（单元测试用）
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  // 无等待发送的任务，标记分支执行
                }
            }
            // 分支2：新队列初始化（xNewQueue = pdTRUE）——用于新创建的队列，初始化阻塞任务列表
            else
            {
                // 初始化“等待发送”和“等待接收”列表为空（新队列无阻塞任务）
                vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
                vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
            }
        }
        // 退出临界区：恢复中断和任务切换
        taskEXIT_CRITICAL();
    }
    // 合法性校验失败（如队列无效、容量为0、内存溢出风险），返回失败
    else
    {
        xReturn = pdFAIL;
    }

    // 断言：确保重置成功（调试阶段暴露非法调用）
    configASSERT( xReturn != pdFAIL );

    // 跟踪函数返回：记录重置结果（供调试工具使用）
    traceRETURN_xQueueGenericReset( xReturn );

    // 返回重置结果：pdPASS=成功，pdFAIL=失败
    return xReturn;
}
/*-----------------------------------------------------------*/

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )  // 仅当启用静态内存分配时，编译此函数

    // 通用静态队列创建函数：支持创建不同类型的队列（普通队列、信号量等）
    QueueHandle_t xQueueGenericCreateStatic( const UBaseType_t uxQueueLength,    // 队列最大容量（项目数）
                                             const UBaseType_t uxItemSize,      // 每个项目的字节数
                                             uint8_t * pucQueueStorage,         // 项目数据存储缓冲区（静态分配）
                                             StaticQueue_t * pxStaticQueue,     // 队列控制结构存储区（静态分配）
                                             const uint8_t ucQueueType )        // 队列类型（如普通队列、互斥锁等）
    {
        Queue_t * pxNewQueue = NULL;  // 指向初始化后的队列控制结构，最终作为句柄返回

        // 跟踪函数入口：用于调试和性能分析（如记录队列创建事件）
        traceENTER_xQueueGenericCreateStatic( uxQueueLength, uxItemSize, pucQueueStorage, pxStaticQueue, ucQueueType );

        /* 静态队列的控制结构（pxStaticQueue）和数据存储区（pucQueueStorage，若需存储数据）必须由用户提供 */
        configASSERT( pxStaticQueue );  // 断言：确保pxStaticQueue非空（控制结构不可缺）

        // 合法性检查：满足以下所有条件才继续初始化
        if( ( uxQueueLength > ( UBaseType_t ) 0 ) &&  // 1. 队列容量必须大于0（至少能存1个项目）
            ( pxStaticQueue != NULL ) &&               // 2. 控制结构存储区非空
            /* 3. 数据存储区的合法性：
               - 若项目大小（uxItemSize）不为0（需存储数据），则pucQueueStorage必须非空；
               - 若项目大小为0（无需存储数据，如信号量），则pucQueueStorage必须为空 */
            ( !( ( pucQueueStorage != NULL ) && ( uxItemSize == 0U ) ) ) &&
            ( !( ( pucQueueStorage == NULL ) && ( uxItemSize != 0U ) ) ) )
        {
            #if ( configASSERT_DEFINED == 1 )  // 若启用断言功能，执行额外的合法性检查
            {
                /* 检查 StaticQueue_t 类型的大小是否与实际队列控制结构（Queue_t）一致：
                   因StaticQueue_t是Queue_t的别名（用于静态分配场景），确保两者大小相同，避免内存访问错误 */
                volatile size_t xSize = sizeof( StaticQueue_t );

                /* 此断言在单元测试中无法覆盖分支 */
                configASSERT( xSize == sizeof( Queue_t ) ); /* LCOV_EXCL_BR_LINE */
                ( void ) xSize;  // 防止configASSERT未定义时，xSize因未使用产生编译警告
            }
            #endif /* configASSERT_DEFINED */

            /* 将用户提供的静态控制结构（pxStaticQueue）强制转换为Queue_t指针：
               因StaticQueue_t本质是Queue_t的封装，用于区分静态/动态分配场景，实际内存布局一致 */
            /* MISRA规则11.3.1引用[未对齐访问]：因StaticQueue_t与Queue_t类型兼容，强制转换安全 */
            /* 更多细节：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-113 */
            /* coverity[misra_c_2012_rule_11_3_violation]：屏蔽Coverity工具的MISRA违规告警 */
            pxNewQueue = ( Queue_t * ) pxStaticQueue;

            #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )  // 若同时启用动态分配，标记队列的分配方式
            {
                /* 队列可静态或动态分配，此处标记为静态分配，避免后续删除队列时尝试释放动态内存 */
                pxNewQueue->ucStaticallyAllocated = pdTRUE;
            }
            #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

            // 调用队列初始化函数：填充队列控制结构的成员（如指针、计数、列表等），完成队列创建
            prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
        }
        else
        {
            // 若合法性检查失败，断言pxNewQueue为空（提示创建失败）
            configASSERT( pxNewQueue );
            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记：用于单元测试时标记此分支已执行
        }

        // 跟踪函数返回：记录队列创建结果（成功/失败）
        traceRETURN_xQueueGenericCreateStatic( pxNewQueue );

        // 返回队列句柄：成功则为非空指针，失败则为NULL
        return pxNewQueue;
    }

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )  // 仅启用静态内存分配时，编译此函数

    // 通用静态队列缓冲区查询函数：获取静态创建队列的项目存储区和控制结构缓冲区指针
    BaseType_t xQueueGenericGetStaticBuffers( QueueHandle_t xQueue,          // 目标队列句柄
                                              uint8_t ** ppucQueueStorage,    // 输出参数：用于接收项目存储区指针
                                              StaticQueue_t ** ppxStaticQueue )// 输出参数：用于接收控制结构缓冲区指针
    {
        BaseType_t xReturn;  // 函数返回值：pdTRUE=成功，pdFALSE=失败
        Queue_t * const pxQueue = xQueue;  // 将队列句柄转换为内核内部控制结构指针（Queue_t*）

        // 跟踪函数入口：记录缓冲区查询事件（供调试工具使用，如Trace Recorder）
        traceENTER_xQueueGenericGetStaticBuffers( xQueue, ppucQueueStorage, ppxStaticQueue );

        // 断言：确保队列句柄非空（避免无效指针操作）
        configASSERT( pxQueue );
        // 断言：确保控制结构缓冲区的输出参数非空（用户必须关心控制结构指针，或显式传入NULL）
        configASSERT( ppxStaticQueue );

        #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )  // 若同时启用动态内存分配
        {
            /* 先判断队列是否为静态分配：通过控制结构的ucStaticallyAllocated成员区分
               - pdTRUE：静态分配（用户提供内存），可查询缓冲区；
               - pdFALSE：动态分配（堆内存），不支持查询，返回失败。 */
            if( pxQueue->ucStaticallyAllocated == ( uint8_t ) pdTRUE )
            {
                // 若用户关心项目存储区指针（ppucQueueStorage非空），则赋值
                if( ppucQueueStorage != NULL )
                {
                    // pcHead指向项目存储区起始（静态创建时已初始化），强转为uint8_t*返回
                    *ppucQueueStorage = ( uint8_t * ) pxQueue->pcHead;
                }

                /* 将Queue_t*类型的控制结构指针，强转为StaticQueue_t*返回：
                   - 因StaticQueue_t是Queue_t的别名，内存布局完全一致，转换安全；
                   - MISRA规则11.3.1引用[未对齐访问]：此处转换无未对齐风险，符合规范。 */
                /* 更多细节：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-113 */
                /* coverity[misra_c_2012_rule_11_3_violation]：屏蔽Coverity工具的MISRA违规告警 */
                *ppxStaticQueue = ( StaticQueue_t * ) pxQueue;
                xReturn = pdTRUE;  // 查询成功
            }
            else
            {
                xReturn = pdFALSE;  // 队列是动态分配的，查询失败
            }
        }
        #else /* configSUPPORT_DYNAMIC_ALLOCATION == 0 */  // 仅启用静态分配（无动态分配）
        {
            /* 此时所有队列都是静态分配的（无动态分配选项），无需判断分配方式，直接返回缓冲区指针 */
            // 若用户关心项目存储区指针，赋值
            if( ppucQueueStorage != NULL )
            {
                *ppucQueueStorage = ( uint8_t * ) pxQueue->pcHead;
            }

            // 返回控制结构缓冲区指针
            *ppxStaticQueue = ( StaticQueue_t * ) pxQueue;
            xReturn = pdTRUE;  // 必然成功
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        // 跟踪函数返回：记录查询结果（成功/失败）
        traceRETURN_xQueueGenericGetStaticBuffers( xReturn );

        // 返回查询结果
        return xReturn;
    }

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )  // 仅当启用动态内存分配时，编译此函数

    // 通用动态队列创建函数：支持创建不同类型的队列（普通队列、信号量、互斥锁等）
    QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,    // 队列最大容量（项目数）
                                       const UBaseType_t uxItemSize,      // 每个项目的字节数
                                       const uint8_t ucQueueType )        // 队列类型（如普通队列、互斥锁等）
    {
        Queue_t * pxNewQueue = NULL;       // 指向动态分配的队列控制结构，最终作为句柄返回
        size_t xQueueSizeInBytes;          // 队列项目存储区的总字节数
        uint8_t * pucQueueStorage;         // 指向项目存储区的起始地址（从动态分配的内存块中拆分）

        // 跟踪函数入口：记录队列创建事件（供调试工具使用，如Trace Recorder）
        traceENTER_xQueueGenericCreate( uxQueueLength, uxItemSize, ucQueueType );

        // 合法性校验：满足以下所有条件才进行动态内存分配
        if( ( uxQueueLength > ( UBaseType_t ) 0 ) &&  // 1. 队列容量>0（有效队列）
            /* 2. 检查“容量×项目大小”是否溢出：避免计算存储区大小时整数溢出 */
            ( ( SIZE_MAX / uxQueueLength ) >= uxItemSize ) &&
            /* 3. 检查“控制结构大小+存储区大小”是否溢出：确保总内存不超过系统最大可分配字节数 */
            ( ( UBaseType_t ) ( SIZE_MAX - sizeof( Queue_t ) ) >= ( uxQueueLength * uxItemSize ) ) )
        {
            // 计算项目存储区的总字节数：容量 × 每个项目的字节数（若uxItemSize=0，存储区大小为0）
            xQueueSizeInBytes = ( size_t ) ( ( size_t ) uxQueueLength * ( size_t ) uxItemSize );

            /* 动态分配一块连续内存：大小 = 队列控制结构大小（Queue_t） + 项目存储区大小
               - 控制结构用于存储队列元数据（指针、计数、列表等）
               - 存储区用于存储实际入队的项目数据
               MISRA规则11.5.1引用[ malloc内存分配 ]：强制转换为Queue_t*，因pvPortMalloc返回void*
               更多细节：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
            /* coverity[misra_c_2012_rule_11_5_violation]：屏蔽Coverity工具的MISRA违规告警 */
            pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );

            // 若内存分配成功（pxNewQueue非空），继续初始化
            if( pxNewQueue != NULL )
            {
                /* 拆分动态内存块：项目存储区起始地址 = 控制结构地址 + 控制结构大小
                   - 动态内存块布局：[ Queue_t 控制结构 ][ 项目存储区 ]
                   - 这样设计可确保控制结构与存储区连续，减少内存碎片，且方便管理（释放时只需释放一块内存） */
                pucQueueStorage = ( uint8_t * ) pxNewQueue;
                pucQueueStorage += sizeof( Queue_t );

                #if ( configSUPPORT_STATIC_ALLOCATION == 1 )  // 若同时启用静态分配，标记队列的分配方式
                {
                    /* 队列可静态或动态分配，此处标记为动态分配（pdFALSE），
                       后续调用vQueueDelete时，会根据此标记释放动态内存 */
                    pxNewQueue->ucStaticallyAllocated = pdFALSE;
                }
                #endif /* configSUPPORT_STATIC_ALLOCATION */

                // 调用队列初始化函数：填充控制结构成员，完成队列功能初始化（与静态创建共用同一初始化逻辑）
                prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
            }
            // 内存分配失败（如堆内存不足）
            else
            {
                // 跟踪队列创建失败事件，供调试分析
                traceQUEUE_CREATE_FAILED( ucQueueType );
                mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记：标记此分支已执行（单元测试用）
            }
        }
        // 合法性校验失败（如容量为0、内存溢出风险）
        else
        {
            // 断言：提示参数非法（调试阶段暴露错误）
            configASSERT( pxNewQueue );
            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记
        }

        // 跟踪函数返回：记录队列创建结果（成功/失败）
        traceRETURN_xQueueGenericCreate( pxNewQueue );

        // 返回队列句柄：成功则为非空指针，失败则为NULL
        return pxNewQueue;
    }

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */  // 注意：原代码此处宏定义有误（应为configSUPPORT_DYNAMIC_ALLOCATION）
/*-----------------------------------------------------------*/

// 静态内部函数：初始化新创建的队列（无论是动态还是静态分配），填充队列控制结构的核心成员
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength,    // 队列最大容量（可存储的项目数）
                                   const UBaseType_t uxItemSize,      // 每个队列项目的字节数
                                   uint8_t * pucQueueStorage,         // 队列项目的存储缓冲区（静态分配时为用户提供，动态分配时为堆内存）
                                   const uint8_t ucQueueType,         // 队列类型（如普通队列、互斥锁、信号量等）
                                   Queue_t * pxNewQueue )             // 指向待初始化的队列控制结构（Queue_t）
{
    /* 若未启用跟踪功能（configUSE_TRACE_FACILITY == 0），ucQueueType参数未被使用，
     * 此语句用于消除编译器的“未使用参数”警告 */
    ( void ) ucQueueType;

    // 处理“无需存储项目数据”的场景（uxItemSize == 0，如信号量、互斥锁）
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        /* 此类场景无需为项目分配存储缓冲区，但pcHead不能设为NULL（因NULL用于标识队列是互斥锁）。
         * 因此将pcHead指向队列自身（pxNewQueue），使用一个已知在内存映射内的“无害值”，
         * 既避免NULL冲突，又确保指针非空（符合后续代码的指针判断逻辑）。 */
        pxNewQueue->pcHead = ( int8_t * ) pxNewQueue;
    }
    else
    {
        /* 处理“需要存储项目数据”的场景（如普通队列），将pcHead指向项目存储缓冲区的起始地址，
         * 后续通过pcHead定位队列存储区的基址，实现项目的读写。 */
        pxNewQueue->pcHead = ( int8_t * ) pucQueueStorage;
    }

    /* 按队列类型定义的规则，初始化队列核心成员 */
    pxNewQueue->uxLength = uxQueueLength;          // 记录队列最大容量
    pxNewQueue->uxItemSize = uxItemSize;          // 记录每个项目的字节数
    // 调用通用重置函数，初始化队列的读写指针、项目计数、阻塞列表等（pdTRUE表示“强制重置”，忽略当前状态）
    ( void ) xQueueGenericReset( pxNewQueue, pdTRUE );

    #if ( configUSE_TRACE_FACILITY == 1 )  // 若启用调试跟踪功能
    {
        // 记录队列类型（如普通队列=0、互斥锁=1），供调试工具（如Trace Recorder）识别队列功能
        pxNewQueue->ucQueueType = ucQueueType;
    }
    #endif /* configUSE_TRACE_FACILITY */

    #if ( configUSE_QUEUE_SETS == 1 )  // 若启用队列集功能
    {
        // 初始化队列集容器指针为NULL，表示当前队列暂未加入任何队列集
        pxNewQueue->pxQueueSetContainer = NULL;
    }
    #endif /* configUSE_QUEUE_SETS */

    // 触发队列创建的跟踪事件，记录队列创建的调试信息（如队列句柄、类型）
    traceQUEUE_CREATE( pxNewQueue );
}
/*-----------------------------------------------------------*/

// 条件编译：仅当启用互斥锁功能（configUSE_MUTEXES == 1）时，才编译该静态函数
#if ( configUSE_MUTEXES == 1 )

    // 静态函数定义：初始化互斥锁专属成员（仅在xQueueCreateMutex内部调用，不对外暴露）
    // 参数：pxNewQueue - 指向已通过xQueueGenericCreate创建的队列结构体指针（待初始化为互斥锁）
    static void prvInitialiseMutex( Queue_t * pxNewQueue )
    {
        if( pxNewQueue != NULL )  // 检查队列结构体指针是否有效（非NULL）
        {
            /* 说明：队列创建函数（xQueueGenericCreate）已为“通用队列”正确初始化了大部分结构体成员，
             * 但本函数的目标是将其初始化为“互斥锁”，因此需要覆盖那些与互斥锁特性相关的成员——
             * 尤其是优先级继承（priority inheritance）所需的信息。 */
            
            // 1. 初始化互斥锁持有者：设为NULL，表示初始状态下无任何任务持有该互斥锁
            pxNewQueue->u.xSemaphore.xMutexHolder = NULL;
            // 2. 标记队列类型为“互斥锁”：覆盖通用队列的类型，后续操作会识别为互斥锁并触发专属逻辑
            pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

            /* 3. 初始化递归调用计数（针对递归互斥锁）：
             * 即使是普通互斥锁，也初始化该成员为0（避免未初始化的随机值导致逻辑错误）；
             * 若为递归互斥锁，该计数记录同一任务获取互斥锁的次数（需对应次数释放）。 */
            pxNewQueue->u.xSemaphore.uxRecursiveCallCount = 0;

            // 跟踪互斥锁创建（调试用，记录“互斥锁专属初始化完成”）
            traceCREATE_MUTEX( pxNewQueue );

            /* 4. 将互斥锁初始化为“可用状态”：
             * 调用通用队列发送函数，向队列发送一个“空数据”（因元素大小为0，无需实际数据），
             * 使队列的消息数（uxMessagesWaiting）从0变为1——对应互斥锁“初始可用”的状态；
             * 等待时间设为0，表示若队列满则立即返回（此处队列容量为1，初始为空，必然发送成功）。 */
            ( void ) xQueueGenericSend( pxNewQueue, NULL, ( TickType_t ) 0U, queueSEND_TO_BACK );
        }
        else  // 队列结构体指针无效（NULL），互斥锁初始化失败
        {
            // 跟踪互斥锁创建失败（调试用，记录“因队列指针无效导致初始化失败”）
            traceCREATE_MUTEX_FAILED();
        }
    }

#endif /* 结束 configUSE_MUTEXES == 1 的条件编译 */
/*-----------------------------------------------------------*/

// 条件编译：仅当同时满足以下两个条件时，才编译该函数
// 1. configUSE_MUTEXES == 1：启用互斥锁功能
// 2. configSUPPORT_DYNAMIC_ALLOCATION == 1：启用动态内存分配（用于创建队列时分配内存）
#if ( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

    // 函数定义：创建互斥锁（底层实现，基于队列）
    // 参数：ucQueueType - 队列类型标记（必须传入“互斥锁类型”，如queueQUEUE_IS_MUTEX）
    // 返回值：成功返回互斥锁句柄（本质是队列句柄）；失败返回NULL（如动态内存分配失败）
    QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
    {
        QueueHandle_t xNewQueue;  // 存储创建的队列（互斥锁）句柄
        // 互斥锁的队列配置：
        // - uxMutexLength = 1：队列容量为1（互斥锁最多允许1个任务持有）
        // - uxMutexSize = 0：队列元素大小为0（互斥锁无需存储实际数据，仅需计数）
        const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

        // 跟踪函数进入（调试用，记录“开始创建互斥锁”）
        traceENTER_xQueueCreateMutex( ucQueueType );

        // 1. 调用通用队列创建函数，按互斥锁的配置创建队列
        // 队列容量=1、元素大小=0、类型=ucQueueType（互斥锁类型）
        xNewQueue = xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );
        // 2. 初始化互斥锁专属逻辑（如优先级继承相关成员）
        prvInitialiseMutex( ( Queue_t * ) xNewQueue );

        // 跟踪函数返回值（调试用，记录“互斥锁创建结束”）
        traceRETURN_xQueueCreateMutex( xNewQueue );

        return xNewQueue;  // 返回创建的互斥锁句柄（若xQueueGenericCreate失败，此处返回NULL）
    }

#endif /* 结束 configUSE_MUTEXES 和 configSUPPORT_DYNAMIC_ALLOCATION 的条件编译 */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

    QueueHandle_t xQueueCreateMutexStatic( const uint8_t ucQueueType,
                                           StaticQueue_t * pxStaticQueue )
    {
        QueueHandle_t xNewQueue;
        const UBaseType_t uxMutexLength = ( UBaseType_t ) 1, uxMutexSize = ( UBaseType_t ) 0;

        traceENTER_xQueueCreateMutexStatic( ucQueueType, pxStaticQueue );

        /* Prevent compiler warnings about unused parameters if
         * configUSE_TRACE_FACILITY does not equal 1. */
        ( void ) ucQueueType;

        xNewQueue = xQueueGenericCreateStatic( uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType );
        prvInitialiseMutex( ( Queue_t * ) xNewQueue );

        traceRETURN_xQueueCreateMutexStatic( xNewQueue );

        return xNewQueue;
    }

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

    TaskHandle_t xQueueGetMutexHolder( QueueHandle_t xSemaphore )
    {
        TaskHandle_t pxReturn;
        Queue_t * const pxSemaphore = ( Queue_t * ) xSemaphore;

        traceENTER_xQueueGetMutexHolder( xSemaphore );

        configASSERT( xSemaphore );

        /* This function is called by xSemaphoreGetMutexHolder(), and should not
         * be called directly.  Note:  This is a good way of determining if the
         * calling task is the mutex holder, but not a good way of determining the
         * identity of the mutex holder, as the holder may change between the
         * following critical section exiting and the function returning. */
        taskENTER_CRITICAL();
        {
            if( pxSemaphore->uxQueueType == queueQUEUE_IS_MUTEX )
            {
                pxReturn = pxSemaphore->u.xSemaphore.xMutexHolder;
            }
            else
            {
                pxReturn = NULL;
            }
        }
        taskEXIT_CRITICAL();

        traceRETURN_xQueueGetMutexHolder( pxReturn );

        return pxReturn;
    }

#endif /* if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

    TaskHandle_t xQueueGetMutexHolderFromISR( QueueHandle_t xSemaphore )
    {
        TaskHandle_t pxReturn;

        traceENTER_xQueueGetMutexHolderFromISR( xSemaphore );

        configASSERT( xSemaphore );

        /* Mutexes cannot be used in interrupt service routines, so the mutex
         * holder should not change in an ISR, and therefore a critical section is
         * not required here. */
        if( ( ( Queue_t * ) xSemaphore )->uxQueueType == queueQUEUE_IS_MUTEX )
        {
            pxReturn = ( ( Queue_t * ) xSemaphore )->u.xSemaphore.xMutexHolder;
        }
        else
        {
            pxReturn = NULL;
        }

        traceRETURN_xQueueGetMutexHolderFromISR( pxReturn );

        return pxReturn;
    }

#endif /* if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) ) */
/*-----------------------------------------------------------*/

// 条件编译：仅当启用递归互斥锁功能（configUSE_RECURSIVE_MUTEXES == 1）时，才编译该函数
#if ( configUSE_RECURSIVE_MUTEXES == 1 )

    // 函数定义：递归释放互斥锁（底层实现，对应应用层的xSemaphoreGiveRecursive()）
    // 参数：
    //   xMutex - 目标递归互斥锁句柄（本质是队列句柄，需确保是递归互斥锁类型）
    // 返回值：
    //   pdPASS - 释放操作成功（递归计数递减，或最终释放互斥锁）
    //   pdFAIL - 释放操作失败（当前任务不是互斥锁持有者，无权释放）
    BaseType_t xQueueGiveMutexRecursive( QueueHandle_t xMutex )
    {
        BaseType_t xReturn;  // 存储函数返回值（成功/失败）
        // 将递归互斥锁句柄强制转换为队列结构体指针（互斥锁本质是特殊配置的队列）
        Queue_t * const pxMutex = ( Queue_t * ) xMutex;

        // 跟踪函数进入（调试用，记录“开始递归释放互斥锁”）
        traceENTER_xQueueGiveMutexRecursive( xMutex );

        // 断言：检查递归互斥锁句柄是否有效（非NULL）
        configASSERT( pxMutex );

        /* 注释：关于互斥锁持有者的访问安全性说明：
         * 1. 若当前任务是持有者，xMutexHolder（持有者句柄）不会被其他任务修改（仅持有者能释放）；
         * 2. 若当前任务不是持有者，xMutexHolder 不可能恰好等于当前任务句柄（无并发冲突）；
         * 因此，无需临界区保护即可直接访问 xMutexHolder，避免不必要的性能开销。 */
        // 分支1：当前任务是互斥锁的持有者（有权释放）
        if( pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle() )
        {
            // 跟踪“递归释放互斥锁”操作（调试用）
            traceGIVE_MUTEX_RECURSIVE( pxMutex );

            /* 注释：关于递归计数的安全性说明：
             * 1. 只有持有者能修改 uxRecursiveCallCount（递归计数），无并发冲突；
             * 2. 若 xMutexHolder 是当前任务，计数至少为1（获取次数≥释放次数），无需检查下溢；
             * 因此，直接递减计数即可。 */
            ( pxMutex->u.xSemaphore.uxRecursiveCallCount )--;

            // 子分支1.1：递归计数减至0（所有获取操作已对应释放，需最终释放互斥锁）
            if( pxMutex->u.xSemaphore.uxRecursiveCallCount == ( UBaseType_t ) 0 )
            {
                /* 调用通用队列发送函数，真正释放互斥锁：
                 * 参数说明：
                 * - pxQueue=pxMutex：目标互斥锁对应的队列；
                 * - pvItemToQueue=NULL：互斥锁无需传递数据，传NULL；
                 * - xTicksToWait=queueMUTEX_GIVE_BLOCK_TIME：释放时永不阻塞（该宏定义为0）；
                 * - xCopyPosition=queueSEND_TO_BACK：无实际意义（互斥锁无数据队列）；
                 * 作用：将队列消息数（互斥锁计数）从0设为1，清空持有者，唤醒等待任务。 */
                ( void ) xQueueGenericSend( pxMutex, NULL, queueMUTEX_GIVE_BLOCK_TIME, queueSEND_TO_BACK );
            }
            else  // 子分支1.2：递归计数未减至0（仅完成一次递归释放，未真正释放互斥锁）
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际业务逻辑
            }

            xReturn = pdPASS;  // 释放操作成功，返回pdPASS
        }
        // 分支2：当前任务不是互斥锁持有者（无权释放，返回失败）
        else
        {
            xReturn = pdFAIL;  // 释放操作失败，返回pdFAIL

            // 跟踪“递归释放互斥锁失败”（调试用，记录释放失败）
            traceGIVE_MUTEX_RECURSIVE_FAILED( pxMutex );
        }

        // 跟踪函数返回值（调试用，记录“递归释放互斥锁的结果”）
        traceRETURN_xQueueGiveMutexRecursive( xReturn );

        return xReturn;  // 返回释放结果（pdPASS或pdFAIL）
    }

#endif /* 结束 configUSE_RECURSIVE_MUTEXES == 1 的条件编译 */
/*-----------------------------------------------------------*/

// 条件编译：仅当启用递归互斥锁功能（configUSE_RECURSIVE_MUTEXES == 1）时，才编译该函数
#if ( configUSE_RECURSIVE_MUTEXES == 1 )

    // 函数定义：递归获取互斥锁（底层实现，对应应用层的xSemaphoreTakeRecursive()）
    // 参数：
    //   xMutex - 目标递归互斥锁句柄（本质是队列句柄，需确保是递归互斥锁类型）
    //   xTicksToWait - 互斥锁不可用时的阻塞等待时间（单位：时钟节拍；portMAX_DELAY表示永久等待）
    // 返回值：
    //   pdPASS - 成功获取互斥锁（首次获取或递归获取）
    //   errQUEUE_EMPTY - 互斥锁被其他任务持有且等待超时/无等待时间（获取失败）
    BaseType_t xQueueTakeMutexRecursive( QueueHandle_t xMutex,
                                         TickType_t xTicksToWait )
    {
        BaseType_t xReturn;  // 存储函数返回值（成功/失败）
        // 将递归互斥锁句柄强制转换为队列结构体指针（互斥锁本质是特殊配置的队列）
        Queue_t * const pxMutex = ( Queue_t * ) xMutex;

        // 跟踪函数进入（调试用，记录“开始递归获取互斥锁”）
        traceENTER_xQueueTakeMutexRecursive( xMutex, xTicksToWait );

        // 断言：检查递归互斥锁句柄是否有效（非NULL）
        configASSERT( pxMutex );

        /* 注：关于互斥锁的互斥访问逻辑，参考xQueueGiveMutexRecursive()中的注释——
         * 核心是通过临界区保护“持有者”和“递归计数”，避免并发修改 */

        // 跟踪“递归获取互斥锁”操作（调试用）
        traceTAKE_MUTEX_RECURSIVE( pxMutex );

        // 分支1：当前任务已是互斥锁的持有者（递归获取场景）
        if( pxMutex->u.xSemaphore.xMutexHolder == xTaskGetCurrentTaskHandle() )
        {
            /* 递归计数递增：
             * uxRecursiveCallCount记录当前任务获取互斥锁的次数，
             * 每次递归获取都需递增，后续需对应次数的释放才能真正释放互斥锁 */
            ( pxMutex->u.xSemaphore.uxRecursiveCallCount )++;
            xReturn = pdPASS;  // 递归获取成功，返回pdPASS
        }
        // 分支2：当前任务不是互斥锁持有者（首次获取或其他任务持有）
        else
        {
            /* 调用普通信号量获取函数，按普通互斥锁逻辑尝试获取：
             * - 若互斥锁未被持有：获取成功，设置当前任务为持有者，计数初始化为1；
             * - 若互斥锁被其他任务持有：按xTicksToWait阻塞等待，超时后返回失败；
             * （递归互斥锁的首次获取逻辑与普通互斥锁完全一致，复用xQueueSemaphoreTake的实现） */
            xReturn = xQueueSemaphoreTake( pxMutex, xTicksToWait );

            /* pdPASS仅在互斥锁成功获取时返回：
             * 注意：调用xQueueSemaphoreTake后，当前任务可能因等待互斥锁进入过阻塞状态，
             * 唤醒后才执行到此处（需确保唤醒后仍能正确判断获取结果） */
            if( xReturn != pdFAIL )
            {
                /* 互斥锁获取成功，初始化递归计数为1：
                 * 首次获取时，uxRecursiveCallCount从0变为1，标记“当前任务已持有1次” */
                ( pxMutex->u.xSemaphore.uxRecursiveCallCount )++;
            }
            else
            {
                // 跟踪“递归获取互斥锁失败”（调试用，记录获取失败）
                traceTAKE_MUTEX_RECURSIVE_FAILED( pxMutex );
            }
        }

        // 跟踪函数返回值（调试用，记录“递归获取互斥锁的结果”）
        traceRETURN_xQueueTakeMutexRecursive( xReturn );

        return xReturn;  // 返回获取结果（pdPASS或errQUEUE_EMPTY）
    }

#endif /* 结束 configUSE_RECURSIVE_MUTEXES == 1 的条件编译 */
/*-----------------------------------------------------------*/

// 条件编译：仅当同时满足以下两个条件时，才编译该函数
// 1. configUSE_COUNTING_SEMAPHORES == 1：启用计数信号量功能
// 2. configSUPPORT_STATIC_ALLOCATION == 1：启用静态内存分配（用户需提前提供内存块）
#if ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

    // 函数定义：静态创建计数信号量（底层实现，基于静态队列，无动态内存分配）
    // 参数：
    //   uxMaxCount - 信号量的最大计数（对应队列的最大容量）
    //   uxInitialCount - 信号量的初始计数（对应队列的初始消息数）
    //   pxStaticQueue - 指向用户提前分配的 StaticQueue_t 结构体指针（存储队列核心数据的静态内存）
    // 返回值：成功返回计数信号量句柄（本质是队列句柄）；失败返回NULL（如参数无效、静态内存指针无效）
    QueueHandle_t xQueueCreateCountingSemaphoreStatic( const UBaseType_t uxMaxCount,
                                                       const UBaseType_t uxInitialCount,
                                                       StaticQueue_t * pxStaticQueue )
    {
        QueueHandle_t xHandle = NULL;  // 存储创建的计数信号量句柄（初始为NULL，标记未创建成功）

        // 跟踪函数进入（调试用，记录“开始静态创建计数信号量”）
        traceENTER_xQueueCreateCountingSemaphoreStatic( uxMaxCount, uxInitialCount, pxStaticQueue );

        // 第一步：参数合法性检查（确保计数信号量配置符合规则）
        // 1. 最大计数uxMaxCount不能为0（计数信号量需至少支持1个计数，否则无意义）
        // 2. 初始计数uxInitialCount不能超过最大计数（避免初始状态“超量”）
        if( ( uxMaxCount != 0U ) &&
            ( uxInitialCount <= uxMaxCount ) )
        {
            // 第二步：调用静态通用队列创建函数，按计数信号量配置创建静态队列
            // 队列参数说明：
            // - 容量=uxMaxCount：对应信号量最大计数
            // - 元素大小=queueSEMAPHORE_QUEUE_ITEM_LENGTH：信号量无需存储数据，该宏定义为0
            // - 数据缓冲区指针=NULL：因元素大小为0，无需额外数据缓冲区（普通队列需提供存储数据的缓冲区）
            // - 静态队列结构体指针=pxStaticQueue：用户提供的静态内存块，存储队列核心控制信息
            // - 队列类型=queueQUEUE_TYPE_COUNTING_SEMAPHORE：标记为计数信号量类型
            xHandle = xQueueGenericCreateStatic( uxMaxCount, 
                                                queueSEMAPHORE_QUEUE_ITEM_LENGTH, 
                                                NULL, 
                                                pxStaticQueue, 
                                                queueQUEUE_TYPE_COUNTING_SEMAPHORE );

            // 第三步：若静态队列创建成功，初始化信号量的“初始计数”
            if( xHandle != NULL )
            {
                // 将队列的“消息数”（uxMessagesWaiting）设为初始计数uxInitialCount
                // （消息数直接对应信号量计数：消息数>0表示信号量可用，消息数=0表示不可用）
                ( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

                // 跟踪计数信号量创建成功（调试用）
                traceCREATE_COUNTING_SEMAPHORE();
            }
            else  // 静态队列创建失败（如pxStaticQueue为NULL或无效指针）
            {
                // 跟踪计数信号量创建失败（调试用）
                traceCREATE_COUNTING_SEMAPHORE_FAILED();
            }
        }
        else  // 参数无效（如uxMaxCount=0或uxInitialCount>uxMaxCount）
        {
            // 断言失败：触发configASSERT（仅调试模式生效），提示参数配置错误
            configASSERT( xHandle );
            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际业务逻辑
        }

        // 跟踪函数返回值（调试用，记录“静态创建计数信号量结束”）
        traceRETURN_xQueueCreateCountingSemaphoreStatic( xHandle );

        return xHandle;  // 返回计数信号量句柄（成功为有效句柄，失败为NULL）
    }

#endif /* 结束条件编译：( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) */
/*-----------------------------------------------------------*/

// 条件编译：仅当同时满足以下两个条件时，才编译该函数
// 1. configUSE_COUNTING_SEMAPHORES == 1：启用计数信号量功能
// 2. configSUPPORT_DYNAMIC_ALLOCATION == 1：启用动态内存分配（用于创建队列时分配内存）
#if ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

    // 函数定义：创建计数信号量（底层实现，基于队列）
    // 参数：
    //   uxMaxCount - 信号量的最大计数（队列的最大容量，即最多可存储的“消息数”）
    //   uxInitialCount - 信号量的初始计数（队列创建后的初始“消息数”）
    // 返回值：成功返回计数信号量句柄（本质是队列句柄）；失败返回NULL（如参数无效、内存不足）
    QueueHandle_t xQueueCreateCountingSemaphore( const UBaseType_t uxMaxCount,
                                                 const UBaseType_t uxInitialCount )
    {
        QueueHandle_t xHandle = NULL;  // 存储创建的计数信号量句柄（初始化为NULL，表示未创建成功）

        // 跟踪函数进入（调试用，记录“开始创建计数信号量”）
        traceENTER_xQueueCreateCountingSemaphore( uxMaxCount, uxInitialCount );

        // 第一步：参数合法性检查（确保计数信号量的配置符合规则）
        // 1. 最大计数uxMaxCount不能为0（计数信号量至少允许1个计数，否则无意义）
        // 2. 初始计数uxInitialCount不能超过最大计数（避免初始状态就“超量”）
        if( ( uxMaxCount != 0U ) &&
            ( uxInitialCount <= uxMaxCount ) )
        {
            // 第二步：调用通用队列创建函数，按计数信号量的配置创建队列
            // 队列参数说明：
            // - 容量=uxMaxCount：对应信号量最大计数（最多允许uxMaxCount个“可用”状态）
            // - 元素大小=queueSEMAPHORE_QUEUE_ITEM_LENGTH：信号量无需存储数据，该宏定义为0
            // - 类型=queueQUEUE_TYPE_COUNTING_SEMAPHORE：标记为计数信号量类型，触发专属逻辑
            xHandle = xQueueGenericCreate( uxMaxCount, queueSEMAPHORE_QUEUE_ITEM_LENGTH, queueQUEUE_TYPE_COUNTING_SEMAPHORE );

            // 第三步：若队列创建成功，初始化计数信号量的“初始计数”
            if( xHandle != NULL )
            {
                // 将队列的“消息数”（uxMessagesWaiting）设为初始计数uxInitialCount
                // （队列消息数直接对应信号量计数：消息数>0表示信号量可用，消息数=0表示不可用）
                ( ( Queue_t * ) xHandle )->uxMessagesWaiting = uxInitialCount;

                // 跟踪计数信号量创建成功（调试用）
                traceCREATE_COUNTING_SEMAPHORE();
            }
            else  // 队列创建失败（如动态内存分配不足）
            {
                // 跟踪计数信号量创建失败（调试用）
                traceCREATE_COUNTING_SEMAPHORE_FAILED();
            }
        }
        else  // 参数无效（如uxMaxCount=0或uxInitialCount>uxMaxCount）
        {
            // 断言失败：触发configASSERT（仅在调试模式下生效），提示参数错误
            configASSERT( xHandle );
            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际业务逻辑
        }

        // 跟踪函数返回值（调试用，记录“计数信号量创建结束”）
        traceRETURN_xQueueCreateCountingSemaphore( xHandle );

        return xHandle;  // 返回计数信号量句柄（成功为有效句柄，失败为NULL）
    }

#endif /* 结束条件编译：( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) */
/*-----------------------------------------------------------*/

// 通用队列发送函数：支持向队列头部、尾部发送项目，或覆盖队列头部项目（适用于单项目队列）
// 是xQueueSendToFront、xQueueSendToBack、xQueueOverwrite的底层实现
BaseType_t xQueueGenericSend( QueueHandle_t xQueue,          // 目标队列句柄
                              const void * const pvItemToQueue,  // 待入队项目的指针（数据来源）
                              TickType_t xTicksToWait,           // 队列满时的阻塞等待节拍数
                              const BaseType_t xCopyPosition )   // 入队位置：queueSEND_TO_FRONT（队首）、queueSEND_TO_BACK（队尾）、queueOVERWRITE（覆盖）
{
    BaseType_t xEntryTimeSet = pdFALSE;  // 标记是否已初始化超时时间（避免重复设置）
    BaseType_t xYieldRequired = pdFALSE; // 标记是否需要触发任务切换（如唤醒高优先级任务）
    TimeOut_t xTimeOut;                  // 超时管理结构体（记录阻塞开始时间，用于判断超时）
    Queue_t * const pxQueue = xQueue;    // 将队列句柄转换为内核内部控制结构指针（Queue_t*）

    // 跟踪函数入口：记录发送事件（队列句柄、项目指针、等待时间、入队位置）
    traceENTER_xQueueGenericSend( xQueue, pvItemToQueue, xTicksToWait, xCopyPosition );

    // 断言防护（调试阶段暴露非法调用）
    configASSERT( pxQueue );  // 确保队列句柄非空
    // 确保“项目指针非空”与“项目大小非0”逻辑一致（需存储数据时，项目指针不能为NULL）
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    // 确保“覆盖模式（queueOVERWRITE）”仅用于单项目队列（uxLength=1）——覆盖模式只允许队列有一个项目
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        // 确保“调度器挂起时”不允许阻塞等待（调度器挂起后无法切换任务，阻塞会导致死锁）
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // 无限循环：直到项目成功入队或等待超时（循环内处理“检查-阻塞-重试”逻辑）
    for( ; ; )
    {
        // 进入临界区：禁止中断和任务切换，确保队列状态检查与修改的原子性
        taskENTER_CRITICAL();
        {
            /* 检查队列是否有空间（或是否允许覆盖）：
               - 普通模式：队列当前项目数 < 队列容量（uxMessagesWaiting < uxLength）；
               - 覆盖模式：无论队列是否满，都允许覆盖头部项目（仅单项目队列可用）。 */
            if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
            {
                // 跟踪“项目成功入队”事件
                traceQUEUE_SEND( pxQueue );

                #if ( configUSE_QUEUE_SETS == 1 )  // 若启用队列集功能
                {
                    UBaseType_t uxPreviousMessagesWaiting = pxQueue->uxMessagesWaiting;  // 记录入队前的项目数

                    // 核心操作：将数据从pvItemToQueue复制到队列存储区，返回是否需要任务切换
                    xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

                    // 若当前队列已加入某个队列集（pxQueueSetContainer非空）
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        // 覆盖模式且入队前队列非空：项目数未变化，无需通知队列集
                        if( ( xCopyPosition == queueOVERWRITE ) && ( uxPreviousMessagesWaiting != ( UBaseType_t ) 0 ) )
                        {
                            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记
                        }
                        // 通知队列集：队列有新项目，唤醒等待队列集的任务
                        else if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
                        {
                            // 若唤醒的任务优先级更高，触发任务切换（抢占式调度器）
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    // 队列未加入队列集：处理等待接收的任务
                    else
                    {
                        // 若有任务因“队列空”阻塞等待接收数据，唤醒一个最高优先级任务
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                // 唤醒的任务优先级更高，触发任务切换
                                queueYIELD_IF_USING_PREEMPTION();
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        // 若复制数据后需要任务切换（如释放了高优先级任务持有的互斥锁）
                        else if( xYieldRequired != pdFALSE )
                        {
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS == 0 */  // 未启用队列集
                {
                    // 复制数据到队列存储区，获取是否需要任务切换的标记
                    xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

                    // 若有任务阻塞等待接收数据，唤醒一个任务
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            // 唤醒高优先级任务，触发切换
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    // 若需要任务切换，触发切换
                    else if( xYieldRequired != pdFALSE )
                    {
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                #endif /* configUSE_QUEUE_SETS */

                // 退出临界区
                taskEXIT_CRITICAL();

                // 跟踪函数返回，返回成功
                traceRETURN_xQueueGenericSend( pdPASS );
                return pdPASS;
            }
            // 队列已满且不允许覆盖：处理阻塞等待或超时
            else
            {
                // 情况1：不阻塞等待（xTicksToWait=0），直接返回失败
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL();  // 退出临界区

                    traceQUEUE_SEND_FAILED( pxQueue );  // 跟踪“发送失败”事件
                    traceRETURN_xQueueGenericSend( errQUEUE_FULL );
                    return errQUEUE_FULL;
                }
                // 情况2：未初始化超时时间，初始化超时结构体（记录当前时间）
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;  // 标记已初始化
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  // 已初始化超时时间，仅标记分支
                }
            }
        }
        // 退出临界区：允许中断和任务切换（后续处理阻塞逻辑）
        taskEXIT_CRITICAL();

        /* 临界区已退出，其他任务或中断可操作队列 */

        // 挂起调度器：确保阻塞任务操作的原子性（避免任务在阻塞过程中被其他任务干扰）
        vTaskSuspendAll();
        // 锁定队列：标记队列正在被操作，防止中断或其他任务修改队列状态
        prvLockQueue( pxQueue );

        // 检查超时：判断阻塞等待是否已超时（xTicksToWait更新为剩余等待时间）
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 超时未发生：检查队列是否仍满（可能在临界区外被其他任务取走数据）
            if( prvIsQueueFull( pxQueue ) != pdFALSE )
            {
                // 跟踪“任务阻塞等待发送”事件
                traceBLOCKING_ON_QUEUE_SEND( pxQueue );
                // 将当前任务加入“等待发送”列表，进入阻塞状态（等待xTicksToWait个节拍）
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                // 解锁队列：允许队列事件（如其他任务取数据）修改等待列表
                prvUnlockQueue( pxQueue );

                // 恢复调度器：若当前任务已被唤醒（如其他任务取数据），会进入就绪列表
                if( xTaskResumeAll() == pdFALSE )
                {
                    // 若有高优先级任务进入就绪列表，触发任务切换
                    taskYIELD_WITHIN_API();
                }
            }
            // 队列已非满：无需阻塞，解锁队列并恢复调度器，回到循环重试入队
            else
            {
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        // 超时已发生：解锁队列、恢复调度器，返回失败
        else
        {
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            traceQUEUE_SEND_FAILED( pxQueue );
            traceRETURN_xQueueGenericSend( errQUEUE_FULL );
            return errQUEUE_FULL;
        }
    }
}
/*-----------------------------------------------------------*/

// 函数定义：中断安全版队列通用发送函数（支持发送到头部/尾部或覆盖模式）
// 参数：
//   xQueue - 目标队列句柄
//   pvItemToQueue - 要发送的数据指针
//   pxHigherPriorityTaskWoken - 高优先级任务是否被唤醒的标记指针
//   xCopyPosition - 数据存储位置（头部/尾部/覆盖）
// 返回值：pdPASS表示成功；errQUEUE_FULL表示队列满且非覆盖模式
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue,
                                     const void * const pvItemToQueue,
                                     BaseType_t * const pxHigherPriorityTaskWoken,
                                     const BaseType_t xCopyPosition )
{
    BaseType_t xReturn;                     // 函数返回值
    UBaseType_t uxSavedInterruptStatus;     // 保存的中断状态（用于临界区保护）
    Queue_t * const pxQueue = xQueue;       // 队列句柄转换为内部结构体指针

    // 跟踪函数进入（调试用）
    traceENTER_xQueueGenericSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken, xCopyPosition );

    // 断言检查：队列句柄必须有效
    configASSERT( pxQueue );
    // 断言检查：若队列元素大小不为0，则发送数据指针不能为NULL
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    // 断言检查：覆盖模式（queueOVERWRITE）仅允许用于容量为1的队列
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );

    /* 支持中断嵌套的RTOS端口有"最大系统调用中断优先级"的概念。
     * 高于此优先级的中断会永久使能，即使RTOS内核处于临界区，
     * 但这些中断不能调用任何FreeRTOS API函数。
     * 若在FreeRTOSConfig.h中定义了configASSERT()，则当从高于
     * 配置的最大系统调用优先级的中断中调用FreeRTOS API时，
     * portASSERT_IF_INTERRUPT_PRIORITY_INVALID()会触发断言失败。
     * 只有以FromISR结尾的FreeRTOS函数可在优先级小于等于
     * 最大系统调用优先级的中断中调用。FreeRTOS维护独立的中断安全API，
     * 以确保中断进入尽可能快速简单。
     * 更多信息（针对Cortex-M）：https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 与xQueueGenericSend类似，但队列满时不会阻塞。
     * 也不会直接唤醒因读取队列而阻塞的任务，而是返回一个标志表示
     * 是否需要上下文切换（即：此发送操作是否唤醒了优先级高于当前任务的任务）。 */
    /* MISRA参考 4.7.1 [返回值必须被检查] */
    /* 更多细节：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#dir-47 */
    /* coverity[misra_c_2012_directive_4_7_violation] */
    
    // 进入中断安全的临界区（保存中断状态）
    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        // 队列未满 或 处于覆盖模式（允许覆盖已有数据）
        if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
        {
            const int8_t cTxLock = pxQueue->cTxLock;              // 队列发送锁状态
            const UBaseType_t uxPreviousMessagesWaiting = pxQueue->uxMessagesWaiting;  // 发送前的消息数量

            // 跟踪队列从ISR发送操作（调试用）
            traceQUEUE_SEND_FROM_ISR( pxQueue );

            /* 信号量使用xQueueGiveFromISR()，因此pxQueue不会是信号量或互斥锁。
             * 这意味着prvCopyDataToQueue()不会导致任务解除优先级继承，
             * 因此即使解除继承的函数在访问就绪列表前不检查调度器是否挂起，
             * 也可在此处调用prvCopyDataToQueue()。 */
            // 将数据复制到队列（根据xCopyPosition指定位置）
            ( void ) prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

            /* 若队列被锁定，则不修改事件列表。
             * 这将在队列解锁时处理。 */
            if( cTxLock == queueUNLOCKED )  // 队列未被锁定
            {
                #if ( configUSE_QUEUE_SETS == 1 )  // 若启用队列集功能
                {
                    // 检查队列是否属于某个队列集
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        // 覆盖模式且队列原有消息数不为0（覆盖不改变消息数量）
                        if( ( xCopyPosition == queueOVERWRITE ) && ( uxPreviousMessagesWaiting != ( UBaseType_t ) 0 ) )
                        {
                            /* 不通知队列集，因为队列中已有项目被覆盖，
                             * 队列中的项目数量未改变。 */
                            mtCOVERAGE_TEST_MARKER();
                        }
                        // 通知队列集容器，返回是否唤醒了更高优先级任务
                        else if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
                        {
                            /* 队列是队列集的成员，向队列集发送数据导致
                             * 更高优先级任务解除阻塞。需要上下文切换。 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else  // 队列不属于任何队列集
                    {
                        // 检查是否有任务在等待接收此队列的数据
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            // 移除等待接收队列中的任务并唤醒它
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* 被唤醒的任务优先级更高，记录需要上下文切换 */
                                if( pxHigherPriorityTaskWoken != NULL )
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE;
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER();
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                }
                #else /* 未启用队列集功能 */
                {
                    // 检查是否有任务在等待接收此队列的数据
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        // 移除等待接收队列中的任务并唤醒它
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 被唤醒的任务优先级更高，记录需要上下文切换 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 此路径中未使用该变量 */
                    ( void ) uxPreviousMessagesWaiting;
                }
                #endif /* configUSE_QUEUE_SETS */
            }
            else  // 队列被锁定
            {
                /* 增加锁定计数，以便解锁队列的任务知道
                 * 在锁定期间有数据被发送。 */
                prvIncrementQueueTxLock( pxQueue, cTxLock );
            }

            // 发送成功
            xReturn = pdPASS;
        }
        else  // 队列已满且非覆盖模式
        {
            // 跟踪从ISR发送失败（调试用）
            traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
            xReturn = errQUEUE_FULL;
        }
    }
    // 退出中断安全的临界区（恢复中断状态）
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    // 跟踪函数返回值（调试用）
    traceRETURN_xQueueGenericSendFromISR( xReturn );

    return xReturn;
}
/*-----------------------------------------------------------*/

// 函数定义：中断安全版信号量“释放”函数（本质是无数据复制的队列发送，用于信号量同步）
// 参数：
//   xQueue - 目标信号量的句柄（FreeRTOS中信号量基于队列实现，句柄类型与队列一致）
//   pxHigherPriorityTaskWoken - 指向“高优先级任务是否被唤醒”标记的指针
// 返回值：pdPASS表示信号量释放成功；errQUEUE_FULL表示信号量已达最大计数（队列满）
BaseType_t xQueueGiveFromISR( QueueHandle_t xQueue,
                              BaseType_t * const pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;                     // 函数返回值
    UBaseType_t uxSavedInterruptStatus;     // 保存的中断状态（用于中断安全的临界区保护）
    Queue_t * const pxQueue = xQueue;       // 将信号量句柄转换为内部队列结构体指针（信号量基于队列实现）

    // 跟踪函数进入（调试用，记录“开始执行中断安全的信号量释放”）
    traceENTER_xQueueGiveFromISR( xQueue, pxHigherPriorityTaskWoken );

    /* 功能说明：类似xQueueGenericSendFromISR()，但专用于信号量——信号量的“元素大小”为0，无需复制数据。
     * 不直接唤醒因读取队列（获取信号量）而阻塞的任务，而是返回一个标志，
     * 表示是否需要上下文切换（即：此次释放操作是否唤醒了优先级高于当前任务的任务）。 */

    // 断言检查：确保信号量句柄有效（非NULL）
    configASSERT( pxQueue );

    /* 断言检查：若队列（信号量）的元素大小不为0，则应使用xQueueGenericSendFromISR()，而非本函数。
     * （信号量的核心是“计数”，无需存储实际数据，因此元素大小固定为0） */
    configASSERT( pxQueue->uxItemSize == 0 );

    /* 断言检查：通常不允许从中断中释放“已被任务持有”的互斥锁。
     * 原因：互斥锁涉及优先级继承，而中断无优先级，无法参与优先级继承逻辑；
     * 若互斥锁已有持有者（u.xSemaphore.xMutexHolder != NULL），且该互斥锁类型为queueQUEUE_IS_MUTEX，
     * 则触发断言失败（禁止这种非法操作）。 */
    configASSERT( !( ( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX ) && ( pxQueue->u.xSemaphore.xMutexHolder != NULL ) ) );

    /* 支持中断嵌套的RTOS端口有“最大系统调用中断优先级”的概念：
     * - 高于此优先级的中断会永久使能，即使内核处于临界区，也不能调用FreeRTOS API；
     * - 低于/等于此优先级的中断，仅可调用以FromISR结尾的API（如本函数）。
     * 若开启configASSERT()，当从高于最大系统调用优先级的中断中调用本函数时，
     * portASSERT_IF_INTERRUPT_PRIORITY_INVALID()会触发断言失败，提示非法调用。
     * 更多Cortex-M端口细节参考：https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* MISRA规范参考 4.7.1 [返回值必须被检查] */
    /* 详细说明：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#dir-47 */
    /* coverity[misra_c_2012_directive_4_7_violation] */
    
    // 进入中断安全的临界区（仅禁用低于“最大系统调用优先级”的中断，保证高优先级中断响应）
    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        // 记录当前信号量的“可用计数”（即队列中已有的“消息数”，对应信号量的计数）
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

        /* 当队列用于实现信号量时，无需实际传递数据，只需判断“队列是否有空闲空间”
         * （对应信号量是否未达最大计数：uxMessagesWaiting < 信号量最大计数）。 */
        if( uxMessagesWaiting < pxQueue->uxLength )  // 信号量计数未达上限（队列未满）
        {
            const int8_t cTxLock = pxQueue->cTxLock;  // 队列（信号量）的发送锁定状态

            // 跟踪信号量从ISR释放的操作（调试用）
            traceQUEUE_SEND_FROM_ISR( pxQueue );

            /* 仅当任务持有互斥锁时，才可能有“优先级继承”；
             * 而本函数是中断版，不允许释放已被持有互斥锁（已通过断言保证），
             * 因此无需处理“优先级解除继承”，直接将信号量计数加1即可。 */
            pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxMessagesWaiting + ( UBaseType_t ) 1 );

            /* 若队列（信号量）被锁定，则不修改事件列表（任务等待列表），
             * 待队列解锁后再统一处理唤醒逻辑。 */
            if( cTxLock == queueUNLOCKED )  // 信号量未被锁定
            {
                #if ( configUSE_QUEUE_SETS == 1 )  // 若启用队列集功能（信号量可加入队列集）
                {
                    // 检查当前信号量是否属于某个队列集
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        // 通知队列集：当前信号量已释放（有可用计数）
                        if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
                        {
                            /* 信号量是队列集的成员，通知队列集后唤醒了更高优先级任务，
                             * 需要标记“需上下文切换”。 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际逻辑
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else  // 信号量不属于任何队列集
                    {
                        // 检查是否有任务在等待获取该信号量（等待队列非空）
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            // 从等待列表中移除任务并唤醒它（允许任务获取信号量）
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                /* 被唤醒的任务优先级高于当前运行任务，标记“需上下文切换”。 */
                                if( pxHigherPriorityTaskWoken != NULL )
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE;
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER();
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                }
                #else /* 未启用队列集功能 */
                {
                    // 检查是否有任务在等待获取该信号量（等待队列非空）
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        // 从等待列表中移除任务并唤醒它
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 被唤醒的任务优先级更高，标记“需上下文切换”。 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                #endif /* configUSE_QUEUE_SETS */
            }
            else  // 信号量被锁定
            {
                /* 增加锁定计数：告知后续解锁信号量的任务，“锁定期间有信号量释放操作”，
                 * 解锁时需统一处理唤醒逻辑。 */
                prvIncrementQueueTxLock( pxQueue, cTxLock );
            }

            // 信号量释放成功
            xReturn = pdPASS;
        }
        else  // 信号量计数已达上限（队列满），释放失败
        {
            // 跟踪从ISR释放信号量失败的操作（调试用）
            traceQUEUE_SEND_FROM_ISR_FAILED( pxQueue );
            xReturn = errQUEUE_FULL;
        }
    }
    // 退出中断安全的临界区，恢复之前保存的中断状态
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    // 跟踪函数返回值（调试用，记录“中断安全的信号量释放操作结束”）
    traceRETURN_xQueueGiveFromISR( xReturn );

    return xReturn;
}
/*-----------------------------------------------------------*/

// 函数定义：从队列接收数据（接收后数据会从队列中移除）
// 参数：xQueue-队列句柄；pvBuffer-接收数据的缓冲区指针；xTicksToWait-等待超时时间（时钟周期）
// 返回值：pdPASS表示成功接收数据；errQUEUE_EMPTY表示队列空或超时
BaseType_t xQueueReceive( QueueHandle_t xQueue,
                          void * const pvBuffer,
                          TickType_t xTicksToWait )
{
    BaseType_t xEntryTimeSet = pdFALSE;  // 标记是否已设置超时起始时间（初始未设置）
    TimeOut_t xTimeOut;                  // 超时管理结构体（用于跟踪等待时间）
    Queue_t * const pxQueue = xQueue;    // 将队列句柄转换为内部队列结构体指针

    // 跟踪函数进入（用于调试和跟踪）
    traceENTER_xQueueReceive( xQueue, pvBuffer, xTicksToWait );

    // 断言检查：队列指针不能为空（确保参数有效性）
    configASSERT( ( pxQueue ) );

    // 断言检查：如果队列元素大小不为0，则接收缓冲区不能为NULL
    // （避免无缓冲区却要复制数据的错误）
    configASSERT( !( ( ( pvBuffer ) == NULL ) && ( ( pxQueue )->uxItemSize != ( UBaseType_t ) 0U ) ) );

    // 断言检查：调度器挂起时不能阻塞等待（防止死锁）
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // 无限循环：直到成功获取数据或超时退出
    for( ; ; )
    {
        // 进入临界区（禁用任务调度，保护队列操作）
        taskENTER_CRITICAL();
        {
            // 获取当前队列中的消息数量
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

            // 如果队列中有消息（且当前任务是最高优先级访问队列的任务）
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                // 数据可用，从队列复制数据到缓冲区
                prvCopyDataFromQueue( pxQueue, pvBuffer );
                // 跟踪队列接收操作（调试用）
                traceQUEUE_RECEIVE( pxQueue );
                // 消息数量减1（数据已被接收并移除）
                pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxMessagesWaiting - ( UBaseType_t ) 1 );

                // 队列现在有了空间，检查是否有任务在等待发送数据到该队列
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    // 移除等待发送队列中的任务并唤醒它
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        // 如果被唤醒的任务优先级更高，则触发任务切换
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        // 覆盖测试标记（用于代码覆盖率测试）
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                // 退出临界区
                taskEXIT_CRITICAL();

                // 跟踪函数返回（调试用）并返回成功
                traceRETURN_xQueueReceive( pdPASS );
                return pdPASS;
            }
            else  // 队列为空的情况
            {
                // 如果超时时间为0（不等待），直接退出
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    // 退出临界区
                    taskEXIT_CRITICAL();

                    // 跟踪接收失败（调试用）并返回队列空错误
                    traceQUEUE_RECEIVE_FAILED( pxQueue );
                    traceRETURN_xQueueReceive( errQUEUE_EMPTY );
                    return errQUEUE_EMPTY;
                }
                // 如果未设置超时起始时间，则初始化超时结构体
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );  // 设置超时起始状态
                    xEntryTimeSet = pdTRUE;  // 标记已设置超时时间
                }
                else
                {
                    // 已设置超时时间，覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        // 退出临界区（允许其他任务/中断访问队列）
        taskEXIT_CRITICAL();

        // 挂起所有任务调度（准备进入阻塞状态）
        vTaskSuspendAll();
        // 锁定队列（防止其他操作干扰）
        prvLockQueue( pxQueue );

        // 检查超时是否已发生（更新剩余等待时间）
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 超时未发生：检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                // 队列为空：将当前任务加入等待接收队列，进入阻塞状态
                traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );  // 跟踪阻塞操作
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // 解锁队列
                prvUnlockQueue( pxQueue );

                // 恢复任务调度，如果需要则触发任务切换
                if( xTaskResumeAll() == pdFALSE )
                {
                    taskYIELD_WITHIN_API();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 队列已有数据：不进入阻塞状态，解锁队列并恢复调度，重新尝试获取数据
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else  // 超时已发生
        {
            // 解锁队列并恢复任务调度
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            // 检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                // 队列仍为空：跟踪失败并返回超时错误
                traceQUEUE_RECEIVE_FAILED( pxQueue );
                traceRETURN_xQueueReceive( errQUEUE_EMPTY );
                return errQUEUE_EMPTY;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
}
/*-----------------------------------------------------------*/

// 函数定义：获取信号量（底层实现，对应应用层的xSemaphoreTake()）
// 参数：
//   xQueue - 目标信号量句柄（本质是队列句柄，需确保是信号量类型的队列）
//   xTicksToWait - 信号量不可用时的阻塞等待时间（单位：时钟节拍；portMAX_DELAY表示永久等待）
// 返回值：
//   pdPASS - 成功获取信号量（计数减1）
//   errQUEUE_EMPTY - 信号量不可用（计数为0）且等待超时/无等待时间
BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue,
                                TickType_t xTicksToWait )
{
    BaseType_t xEntryTimeSet = pdFALSE;  // 标记“是否已设置超时起始时间”（初始未设置）
    TimeOut_t xTimeOut;                  // 超时管理结构体（存储超时起始时间、溢出状态等）
    Queue_t * const pxQueue = xQueue;    // 将信号量句柄强制转换为队列结构体指针（信号量本质是队列）

    // 条件编译：若启用互斥锁功能，需记录“是否触发优先级继承”
    #if ( configUSE_MUTEXES == 1 )
        BaseType_t xInheritanceOccurred = pdFALSE;  // 标记“优先级继承是否发生”（初始未发生）
    #endif

    // 跟踪函数进入（调试用，记录“开始获取信号量”）
    traceENTER_xQueueSemaphoreTake( xQueue, xTicksToWait );

    /* 断言1：检查信号量句柄是否有效（非NULL） */
    configASSERT( ( pxQueue ) );

    /* 断言2：检查当前操作的是“信号量”——信号量的队列元素大小必须为0（无需存储数据） */
    configASSERT( pxQueue->uxItemSize == 0 );

    /* 断言3：调度器挂起时不能阻塞等待（调度器挂起后无法切换任务，阻塞会导致死锁） */
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // 无限循环：直到成功获取信号量或等待超时
    for( ; ; )
    {
        // 1. 进入临界区：保护队列状态（避免并发修改消息数、任务等待列表）
        taskENTER_CRITICAL();
        {
            /* 信号量本质是“元素大小为0的队列”，队列的“消息数”（uxMessagesWaiting）即信号量的“计数” */
            const UBaseType_t uxSemaphoreCount = pxQueue->uxMessagesWaiting;

            /* 分支1：信号量计数>0（可用），执行获取逻辑 */
            if( uxSemaphoreCount > ( UBaseType_t ) 0 )
            {
                // 跟踪“成功读取队列”（调试用，信号量获取对应队列读取）
                traceQUEUE_RECEIVE( pxQueue );

                /* 信号量计数减1：消息数-1即表示成功获取信号量 */
                pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxSemaphoreCount - ( UBaseType_t ) 1 );

                // 条件编译：若当前是互斥锁（而非普通信号量），需记录持有者信息
                #if ( configUSE_MUTEXES == 1 )
                {
                    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                    {
                        /* 记录互斥锁持有者：
                         * pvTaskIncrementMutexHeldCount()返回当前任务句柄，并递增任务的“持有互斥锁计数”
                         * （用于后续优先级继承、防止重复释放等逻辑） */
                        pxQueue->u.xSemaphore.xMutexHolder = pvTaskIncrementMutexHeldCount();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际逻辑
                    }
                }
                #endif /* configUSE_MUTEXES */

                /* 检查是否有任务阻塞等待“释放信号量”（即等待发送消息到队列）：
                 * 信号量获取后计数可能仍有剩余，需唤醒最高优先级的等待释放任务（若存在） */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    // 从“等待发送列表”中移除最高优先级任务并唤醒
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        // 若使用抢占式调度，唤醒的任务优先级可能更高，触发任务切换
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                // 退出临界区
                taskEXIT_CRITICAL();

                // 跟踪函数返回成功（调试用）
                traceRETURN_xQueueSemaphoreTake( pdPASS );

                return pdPASS;  // 成功获取信号量，返回pdPASS
            }
            /* 分支2：信号量计数=0（不可用），处理等待逻辑 */
            else
            {
                /* 子分支2.1：无等待时间（xTicksToWait=0），直接返回失败 */
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL();  // 退出临界区

                    traceQUEUE_RECEIVE_FAILED( pxQueue );  // 跟踪“队列读取失败”（调试用）
                    traceRETURN_xQueueSemaphoreTake( errQUEUE_EMPTY );

                    return errQUEUE_EMPTY;  // 信号量不可用，返回失败
                }
                /* 子分支2.2：有等待时间，但未设置超时起始时间，初始化超时结构体 */
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );  // 记录当前时间为超时起始点
                    xEntryTimeSet = pdTRUE;  // 标记“超时起始时间已设置”
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  // 已设置超时时间，无操作
                }
            }
        }
        // 退出临界区：此时其他任务/中断可修改信号量状态（如释放信号量）
        taskEXIT_CRITICAL();

        /* 2. 处理阻塞等待：信号量不可用时，将当前任务加入等待列表并挂起 */
        // 挂起调度器：避免在“检查超时→加入等待列表”过程中被其他任务抢占
        vTaskSuspendAll();
        // 锁定队列：标记队列正被操作，防止并发修改等待列表
        prvLockQueue( pxQueue );

        /* 检查超时状态：判断当前等待时间是否已过期 */
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            /* 子分支1：未超时，且信号量仍不可用（队列空=计数0），将任务加入等待列表 */
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                // 跟踪“任务阻塞等待队列读取”（调试用）
                traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );

                // 条件编译：若当前是互斥锁，触发优先级继承（关键！防止优先级反转）
                #if ( configUSE_MUTEXES == 1 )
                {
                    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                    {
                        taskENTER_CRITICAL();
                        {
                            /* 优先级继承：
                             * 若当前任务优先级 > 互斥锁持有者优先级，
                             * 则将持有者优先级临时提升到当前任务优先级，避免优先级反转 */
                            xInheritanceOccurred = xTaskPriorityInherit( pxQueue->u.xSemaphore.xMutexHolder );
                        }
                        taskEXIT_CRITICAL();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                #endif /* if ( configUSE_MUTEXES == 1 ) */

                // 将当前任务加入“等待接收列表”（等待信号量可用），并设置阻塞时间
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // 解锁队列：允许其他任务操作队列
                prvUnlockQueue( pxQueue );

                /* 恢复调度器：
                 * 若恢复后需要切换任务（如唤醒了更高优先级任务），则触发任务切换 */
                if( xTaskResumeAll() == pdFALSE )
                {
                    taskYIELD_WITHIN_API();  // 在API内部触发任务切换
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            /* 子分支2：未超时，但信号量已可用（其他任务/中断释放了信号量），重新进入循环尝试获取 */
            else
            {
                prvUnlockQueue( pxQueue );  // 解锁队列
                ( void ) xTaskResumeAll();  // 恢复调度器，无任务切换
            }
        }
        /* 子分支2：等待超时，处理超时逻辑 */
        else
        {
            prvUnlockQueue( pxQueue );  // 解锁队列
            ( void ) xTaskResumeAll();  // 恢复调度器

            /* 检查信号量是否仍不可用（队列空=计数0）：
             * 若超时后信号量仍不可用，返回失败；否则重新进入循环尝试获取 */
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                // 条件编译：若此前触发了优先级继承，超时后需恢复持有者的原始优先级
                #if ( configUSE_MUTEXES == 1 )
                {
                    if( xInheritanceOccurred != pdFALSE )
                    {
                        taskENTER_CRITICAL();
                        {
                            /* 步骤1：获取等待该互斥锁的最高优先级任务优先级 */
                            UBaseType_t uxHighestWaitingPriority = prvGetDisinheritPriorityAfterTimeout( pxQueue );

                            /* 步骤2：恢复互斥锁持有者的原始优先级：
                             * 将持有者优先级从“继承的高优先级”恢复到“原始优先级”，
                             * 但不低于“等待队列中最高优先级”（避免后续等待任务仍被阻塞） */
                            vTaskPriorityDisinheritAfterTimeout( pxQueue->u.xSemaphore.xMutexHolder, uxHighestWaitingPriority );
                        }
                        taskEXIT_CRITICAL();
                    }
                }
                #endif /* configUSE_MUTEXES */

                traceQUEUE_RECEIVE_FAILED( pxQueue );  // 跟踪“队列读取失败”（调试用）
                traceRETURN_xQueueSemaphoreTake( errQUEUE_EMPTY );

                return errQUEUE_EMPTY;  // 超时且信号量仍不可用，返回失败
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 超时后信号量已可用，重新进入循环尝试获取
            }
        }
    }
}
/*-----------------------------------------------------------*/

// 函数定义：从队列中查看数据（不删除数据）
// 参数：xQueue-队列句柄；pvBuffer-接收数据的缓冲区指针；xTicksToWait-等待超时时间（时钟周期）
// 返回值：pdPASS表示成功查看数据；errQUEUE_EMPTY表示队列空或超时
BaseType_t xQueuePeek( QueueHandle_t xQueue,
                       void * const pvBuffer,
                       TickType_t xTicksToWait )
{
    BaseType_t xEntryTimeSet = pdFALSE;  // 标记是否已设置超时起始时间（初始未设置）
    TimeOut_t xTimeOut;                  // 超时管理结构体（用于跟踪等待时间）
    int8_t * pcOriginalReadPosition;     // 保存原始读指针位置（用于查看后恢复）
    Queue_t * const pxQueue = xQueue;    // 将队列句柄转换为内部队列结构体指针

    // 跟踪函数进入（用于调试和跟踪）
    traceENTER_xQueuePeek( xQueue, pvBuffer, xTicksToWait );

    // 断言检查：队列指针不能为空（确保参数有效性）
    configASSERT( ( pxQueue ) );

    // 断言检查：如果队列元素大小不为0，则接收缓冲区不能为NULL
    // （避免无缓冲区却要复制数据的错误）
    configASSERT( !( ( ( pvBuffer ) == NULL ) && ( ( pxQueue )->uxItemSize != ( UBaseType_t ) 0U ) ) );

    // 断言检查：调度器挂起时不能阻塞等待（防止死锁）
    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // 无限循环：直到成功获取数据或超时退出
    for( ; ; )
    {
        // 进入临界区（禁用任务调度，保护队列操作）
        taskENTER_CRITICAL();
        {
            // 获取当前队列中的消息数量
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

            // 如果队列中有消息（且当前任务是最高优先级访问队列的任务）
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                // 保存原始读指针位置（因为查看操作不删除数据，后续需要恢复）
                pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;

                // 从队列复制数据到缓冲区（内部函数，处理实际数据拷贝）
                prvCopyDataFromQueue( pxQueue, pvBuffer );
                // 跟踪队列查看操作（调试用）
                traceQUEUE_PEEK( pxQueue );

                // 恢复读指针（因为是查看操作，不移动读指针）
                pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

                // 检查是否有其他任务在等待接收该队列的数据
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    // 将等待队列中的任务移除并唤醒（因为数据仍在队列中）
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        // 如果被唤醒的任务优先级更高，则触发任务切换
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        // 覆盖测试标记（用于代码覆盖率测试）
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                // 退出临界区
                taskEXIT_CRITICAL();

                // 跟踪函数返回（调试用）并返回成功
                traceRETURN_xQueuePeek( pdPASS );
                return pdPASS;
            }
            else  // 队列为空的情况
            {
                // 如果超时时间为0（不等待），直接退出
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    // 退出临界区
                    taskEXIT_CRITICAL();

                    // 跟踪查看失败（调试用）并返回队列空错误
                    traceQUEUE_PEEK_FAILED( pxQueue );
                    traceRETURN_xQueuePeek( errQUEUE_EMPTY );
                    return errQUEUE_EMPTY;
                }
                // 如果未设置超时起始时间，则初始化超时结构体
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );  // 设置超时起始状态
                    xEntryTimeSet = pdTRUE;  // 标记已设置超时时间
                }
                else
                {
                    // 已设置超时时间，覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        // 退出临界区（允许其他任务/中断访问队列）
        taskEXIT_CRITICAL();

        // 挂起所有任务调度（准备进入阻塞状态）
        vTaskSuspendAll();
        // 锁定队列（防止其他操作干扰）
        prvLockQueue( pxQueue );

        // 检查超时是否已发生（更新剩余等待时间）
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 超时未发生：检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                // 队列为空：将当前任务加入等待接收队列，进入阻塞状态
                traceBLOCKING_ON_QUEUE_PEEK( pxQueue );  // 跟踪阻塞操作
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // 解锁队列
                prvUnlockQueue( pxQueue );

                // 恢复任务调度，如果需要则触发任务切换
                if( xTaskResumeAll() == pdFALSE )
                {
                    taskYIELD_WITHIN_API();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 队列已有数据：不进入阻塞状态，解锁队列并恢复调度，重新尝试获取数据
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else  // 超时已发生
        {
            // 解锁队列并恢复任务调度
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            // 检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                // 队列仍为空：跟踪失败并返回超时错误
                traceQUEUE_PEEK_FAILED( pxQueue );
                traceRETURN_xQueuePeek( errQUEUE_EMPTY );
                return errQUEUE_EMPTY;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
}
/*-----------------------------------------------------------*/

// 函数定义：中断安全版队列读取函数（从队列头部读取数据，支持唤醒等待发送的高优先级任务）
// 参数：
//   xQueue - 目标队列句柄（需提前通过xQueueCreate创建）
//   pvBuffer - 存储读取数据的缓冲区指针（大小需≥队列元素大小）
//   pxHigherPriorityTaskWoken - 指向“高优先级任务是否被唤醒”标记的指针
// 返回值：pdPASS表示成功读取数据；pdFAIL表示队列为空，无数据可读
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue,
                                 void * const pvBuffer,
                                 BaseType_t * const pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;                     // 函数返回值（pdPASS/pdFAIL）
    UBaseType_t uxSavedInterruptStatus;     // 保存的中断状态（用于中断安全的临界区保护）
    Queue_t * const pxQueue = xQueue;       // 将队列句柄转换为内部结构体指针（FreeRTOS队列核心数据结构）

    // 跟踪函数进入（调试用，记录“中断安全队列读取操作开始”）
    traceENTER_xQueueReceiveFromISR( xQueue, pvBuffer, pxHigherPriorityTaskWoken );

    // 断言检查：队列句柄必须有效（非NULL）
    configASSERT( pxQueue );
    // 断言检查：若队列元素大小≠0（需存储实际数据），则缓冲区指针不可为NULL（避免内存访问错误）
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

    /* 支持中断嵌套的RTOS端口有“最大系统调用中断优先级”的概念：
     * - 高于此优先级的中断会永久使能，即使内核处于临界区，也禁止调用FreeRTOS API；
     * - 低于/等于此优先级的中断，仅可调用以FromISR结尾的API（如本函数）。
     * 若开启configASSERT()，当从高于最大系统调用优先级的中断中调用本函数时，
     * portASSERT_IF_INTERRUPT_PRIORITY_INVALID()会触发断言失败，提示非法调用。
     * 更多Cortex-M端口细节参考：https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* MISRA规范参考 4.7.1 [返回值必须被检查] */
    /* 详细说明：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#dir-47 */
    /* coverity[misra_c_2012_directive_4_7_violation] */
    
    // 进入中断安全的临界区：仅禁用低于“最大系统调用优先级”的中断，保证高优先级中断正常响应
    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        // 记录当前队列中的“消息数”（即待读取的数据项数量）
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

        /* 中断环境禁止阻塞，因此先检查队列是否有数据可读（消息数>0） */
        if( uxMessagesWaiting > ( UBaseType_t ) 0 )  // 队列非空，可读取数据
        {
            const int8_t cRxLock = pxQueue->cRxLock;  // 队列的“读取锁定状态”（区分发送锁定cTxLock）

            // 跟踪队列从ISR读取的操作（调试用，记录“成功进入数据读取流程”）
            traceQUEUE_RECEIVE_FROM_ISR( pxQueue );

            // 1. 从队列复制数据到缓冲区：按队列元素大小，将队列头部数据复制到pvBuffer
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            // 2. 减少队列消息数：读取成功后，队列中待读取的数据项数量减1（腾出1个空间）
            pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxMessagesWaiting - ( UBaseType_t ) 1 );

            /* 若队列被“读取锁定”，则不修改任务等待列表（事件列表），
             * 待队列解锁后，由解锁任务统一处理唤醒逻辑。 */
            if( cRxLock == queueUNLOCKED )  // 队列未被读取锁定，可正常处理任务唤醒
            {
                // 检查是否有任务因“队列满”阻塞（等待向队列发送数据）
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    // 从“等待发送任务列表”中移除任务并唤醒它（该任务现在可向队列发送数据）
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        /* 被唤醒的任务优先级高于当前运行任务（即被中断打断的任务），
                         * 标记“需要上下文切换”，告知中断服务程序退出前触发切换。 */
                        if( pxHigherPriorityTaskWoken != NULL )
                        {
                            *pxHigherPriorityTaskWoken = pdTRUE;
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际业务逻辑
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else  // 队列被读取锁定
            {
                /* 增加“读取锁定计数”：告知后续解锁队列的任务，
                 * “锁定期间有数据被读取，需处理等待发送的任务”。 */
                prvIncrementQueueRxLock( pxQueue, cRxLock );
            }

            // 数据读取成功，返回pdPASS
            xReturn = pdPASS;
        }
        else  // 队列为空，无数据可读取
        {
            xReturn = pdFAIL;  // 返回pdFAIL表示读取失败
            traceQUEUE_RECEIVE_FROM_ISR_FAILED( pxQueue );  // 跟踪读取失败（调试用）
        }
    }
    // 退出中断安全的临界区，恢复进入前的中断状态（确保不影响其他中断）
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    // 跟踪函数返回值（调试用，记录“中断安全队列读取操作结束”）
    traceRETURN_xQueueReceiveFromISR( xReturn );

    return xReturn;
}
/*-----------------------------------------------------------*/

// 函数定义：中断服务程序(ISR)中使用的队列查看函数（不删除数据）
// 参数：xQueue-队列句柄；pvBuffer-接收数据的缓冲区指针
// 返回值：pdPASS表示成功查看数据；pdFAIL表示失败（通常因为队列空）
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,
                              void * const pvBuffer )
{
    BaseType_t xReturn;                     // 函数返回值（成功/失败）
    UBaseType_t uxSavedInterruptStatus;     // 保存的中断状态（用于退出临界区时恢复）
    int8_t * pcOriginalReadPosition;        // 保存原始读指针位置（用于查看后恢复）
    Queue_t * const pxQueue = xQueue;       // 将队列句柄转换为内部队列结构体指针

    // 跟踪函数进入（用于调试和跟踪）
    traceENTER_xQueuePeekFromISR( xQueue, pvBuffer );

    // 断言检查：队列指针不能为空
    configASSERT( pxQueue );
    // 断言检查：如果队列元素大小不为0，则接收缓冲区不能为NULL
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );
    // 断言检查：不能对信号量使用peek操作（信号量没有数据项）
    configASSERT( pxQueue->uxItemSize != 0 ); /* 不能查看信号量 */

    /* 支持中断嵌套的RTOS端口有"最大系统调用中断优先级"的概念。
     * 高于此优先级的中断会始终保持使能，即使RTOS内核处于临界区，
     * 但这些中断不能调用任何FreeRTOS API函数。
     * 如果在FreeRTOSConfig.h中定义了configASSERT()，则当从高于
     * 配置的最大系统调用优先级的中断中调用FreeRTOS API函数时，
     * portASSERT_IF_INTERRUPT_PRIORITY_INVALID()会导致断言失败。
     * 只有以FromISR结尾的FreeRTOS函数可以从中断中调用，且这些中断的
     * 优先级必须小于等于最大系统调用中断优先级。
     * FreeRTOS维护独立的中断安全API，以确保中断处理尽可能快速简单。
     * 更多信息（针对Cortex-M）请参考：https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* MISRA规则4.7.1 [返回值必须被检查] */
    /* 更多细节：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#dir-47 */
    /* coverity[misra_c_2012_directive_4_7_violation] */
    // 进入中断安全的临界区，并保存当前中断状态
    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        // 中断中不能阻塞，因此直接检查队列中是否有数据
        if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            // 跟踪ISR中的队列查看操作（调试用）
            traceQUEUE_PEEK_FROM_ISR( pxQueue );

            // 保存原始读指针位置（因为是查看操作，不删除数据）
            pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;
            // 从队列复制数据到缓冲区
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            // 恢复读指针（不移动读指针，数据仍在队列中）
            pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

            // 标记成功
            xReturn = pdPASS;
        }
        else
        {
            // 队列为空，标记失败
            xReturn = pdFAIL;
            // 跟踪ISR中查看队列失败（调试用）
            traceQUEUE_PEEK_FROM_ISR_FAILED( pxQueue );
        }
    }
    // 退出中断安全的临界区，恢复之前保存的中断状态
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    // 跟踪函数返回（调试用）
    traceRETURN_xQueuePeekFromISR( xReturn );

    return xReturn;
}
/*-----------------------------------------------------------*/

UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;

    traceENTER_uxQueueMessagesWaiting( xQueue );

    configASSERT( xQueue );

    taskENTER_CRITICAL();
    {
        uxReturn = ( ( Queue_t * ) xQueue )->uxMessagesWaiting;
    }
    taskEXIT_CRITICAL();

    traceRETURN_uxQueueMessagesWaiting( uxReturn );

    return uxReturn;
}
/*-----------------------------------------------------------*/

UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;
    Queue_t * const pxQueue = xQueue;

    traceENTER_uxQueueSpacesAvailable( xQueue );

    configASSERT( pxQueue );

    taskENTER_CRITICAL();
    {
        uxReturn = ( UBaseType_t ) ( pxQueue->uxLength - pxQueue->uxMessagesWaiting );
    }
    taskEXIT_CRITICAL();

    traceRETURN_uxQueueSpacesAvailable( uxReturn );

    return uxReturn;
}
/*-----------------------------------------------------------*/

UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;
    Queue_t * const pxQueue = xQueue;

    traceENTER_uxQueueMessagesWaitingFromISR( xQueue );

    configASSERT( pxQueue );
    uxReturn = pxQueue->uxMessagesWaiting;

    traceRETURN_uxQueueMessagesWaitingFromISR( uxReturn );

    return uxReturn;
}
/*-----------------------------------------------------------*/

// 函数定义：删除队列并释放相关内存
// 参数：xQueue - 待删除队列的句柄
void vQueueDelete( QueueHandle_t xQueue )
{
    // 将队列句柄转换为内部队列结构体指针（便于访问队列的成员变量）
    Queue_t * const pxQueue = xQueue;

    // 跟踪函数进入（用于调试和代码跟踪，记录“开始删除队列”的操作）
    traceENTER_vQueueDelete( xQueue );

    // 断言检查：确保传入的队列句柄有效（非NULL），若无效则触发断言（需开启configASSERT）
    configASSERT( pxQueue );
    // 跟踪队列删除操作（调试用，记录“正在删除指定队列”的事件）
    traceQUEUE_DELETE( pxQueue );

    // 若配置了队列注册功能（configQUEUE_REGISTRY_SIZE > 0），先将队列从注册列表中移除
    #if ( configQUEUE_REGISTRY_SIZE > 0 )
    {
        // 调用vQueueUnregisterQueue，取消队列在注册中心的登记（避免后续通过注册名访问已删除的队列）
        vQueueUnregisterQueue( pxQueue );
    }
    #endif

    // 分支1：仅支持动态内存分配，不支持静态分配的场景
    #if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
    {
        /* 队列只能是动态分配的（因为不支持静态分配），因此直接释放其内存 */
        // 调用vPortFree（FreeRTOS封装的内存释放函数），释放队列结构体占用的动态内存
        vPortFree( pxQueue );
    }
    // 分支2：同时支持动态和静态内存分配的场景
    #elif ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
    {
        /* 队列可能是静态分配，也可能是动态分配，因此先判断再释放内存 */
        // 检查队列的“静态分配标记”：ucStaticallyAllocated为pdFALSE表示动态分配
        if( pxQueue->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
        {
            // 动态分配的队列，调用vPortFree释放内存
            vPortFree( pxQueue );
        }
        else
        {
            // 静态分配的队列（内存由用户预先分配，无需FreeRTOS释放），仅作覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    // 分支3：仅支持静态内存分配，不支持动态分配的场景
    #else /* if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) ) */
    {
        /* 队列只能是静态分配的，无需释放内存（内存由用户管理）。
         * 此处显式使用pxQueue参数，避免编译器报“未使用参数”的警告 */
        ( void ) pxQueue;
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

    // 跟踪函数返回（调试用，记录“队列删除操作结束”）
    traceRETURN_vQueueDelete();
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

    UBaseType_t uxQueueGetQueueNumber( QueueHandle_t xQueue )
    {
        traceENTER_uxQueueGetQueueNumber( xQueue );

        traceRETURN_uxQueueGetQueueNumber( ( ( Queue_t * ) xQueue )->uxQueueNumber );

        return ( ( Queue_t * ) xQueue )->uxQueueNumber;
    }

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

    void vQueueSetQueueNumber( QueueHandle_t xQueue,
                               UBaseType_t uxQueueNumber )
    {
        traceENTER_vQueueSetQueueNumber( xQueue, uxQueueNumber );

        ( ( Queue_t * ) xQueue )->uxQueueNumber = uxQueueNumber;

        traceRETURN_vQueueSetQueueNumber();
    }

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

    uint8_t ucQueueGetQueueType( QueueHandle_t xQueue )
    {
        traceENTER_ucQueueGetQueueType( xQueue );

        traceRETURN_ucQueueGetQueueType( ( ( Queue_t * ) xQueue )->ucQueueType );

        return ( ( Queue_t * ) xQueue )->ucQueueType;
    }

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

UBaseType_t uxQueueGetQueueItemSize( QueueHandle_t xQueue ) /* PRIVILEGED_FUNCTION */
{
    traceENTER_uxQueueGetQueueItemSize( xQueue );

    traceRETURN_uxQueueGetQueueItemSize( ( ( Queue_t * ) xQueue )->uxItemSize );

    return ( ( Queue_t * ) xQueue )->uxItemSize;
}
/*-----------------------------------------------------------*/

UBaseType_t uxQueueGetQueueLength( QueueHandle_t xQueue ) /* PRIVILEGED_FUNCTION */
{
    traceENTER_uxQueueGetQueueLength( xQueue );

    traceRETURN_uxQueueGetQueueLength( ( ( Queue_t * ) xQueue )->uxLength );

    return ( ( Queue_t * ) xQueue )->uxLength;
}
/*-----------------------------------------------------------*/

// 条件编译：仅当启用互斥锁功能（configUSE_MUTEXES == 1）时，才编译该静态函数
#if ( configUSE_MUTEXES == 1 )

    // 静态函数定义：获取互斥锁等待队列中最高优先级任务的优先级（用于超时后的优先级恢复）
    // 参数：pxQueue - 指向目标互斥锁对应的队列结构体指针（互斥锁本质是队列）
    // 返回值：等待该互斥锁的所有任务中，最高优先级的值；若无等待任务，返回空闲任务优先级（tskIDLE_PRIORITY）
    static UBaseType_t prvGetDisinheritPriorityAfterTimeout( const Queue_t * const pxQueue )
    {
        UBaseType_t uxHighestPriorityOfWaitingTasks;  // 存储等待任务的最高优先级

        /* 注释：若某任务因等待互斥锁而触发了“优先级继承”（互斥锁持有者优先级被提升），
         * 但该等待任务最终超时（未获取到互斥锁），则持有者需要“取消优先级继承”（恢复优先级）；
         * 但恢复后的优先级不能低于“其他仍在等待该互斥锁的任务中的最高优先级”——
         * 本函数的作用就是获取这个“其他等待任务的最高优先级”，作为持有者恢复优先级的下限。 */
        if( listCURRENT_LIST_LENGTH( &( pxQueue->xTasksWaitingToReceive ) ) > 0U )
        {
            /* 若等待列表（xTasksWaitingToReceive）中有任务：
             * 1. listGET_ITEM_VALUE_OF_HEAD_ENTRY：获取等待列表头部任务的“列表项值”（FreeRTOS中，列表项值存储的是“优先级的反转值”）；
             * 2. 计算逻辑：最高优先级 = 最大优先级数（configMAX_PRIORITIES） - 列表项值（反转值）；
             * （注：FreeRTOS列表按“优先级反转值”排序，头部是最高优先级任务，反转是为了让列表按升序存储，简化插入逻辑） */
            uxHighestPriorityOfWaitingTasks = ( UBaseType_t ) ( 
                ( UBaseType_t ) configMAX_PRIORITIES - 
                ( UBaseType_t ) listGET_ITEM_VALUE_OF_HEAD_ENTRY( &( pxQueue->xTasksWaitingToReceive ) ) 
            );
        }
        else
        {
            /* 若等待列表中无任务：
             * 持有者恢复优先级的下限为“空闲任务优先级”（tskIDLE_PRIORITY，通常为0），
             * 即持有者可直接恢复到自己的原始优先级。 */
            uxHighestPriorityOfWaitingTasks = tskIDLE_PRIORITY;
        }

        return uxHighestPriorityOfWaitingTasks;  // 返回计算出的“优先级恢复下限”
    }

#endif /* 结束 configUSE_MUTEXES == 1 的条件编译 */
/*-----------------------------------------------------------*/

// 静态函数：将数据复制到队列存储区的核心实现，根据入队位置（队首/队尾/覆盖）调整队列指针
// 仅在临界区内被xQueueGenericSend调用，确保队列状态操作的原子性
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue,    // 目标队列的内部控制结构指针
                                      const void * pvItemToQueue,  // 待复制的数据源指针
                                      const BaseType_t xPosition ) // 入队位置：queueSEND_TO_FRONT/queueSEND_TO_BACK/queueOVERWRITE
{
    BaseType_t xReturn = pdFALSE;  // 返回值：仅用于互斥锁场景（优先级继承相关）
    UBaseType_t uxMessagesWaiting;  // 记录当前队列中的项目数（用于覆盖模式下的计数修正）

    /* 注意：此函数仅在临界区内调用，无需额外加锁 */

    // 保存当前队列的项目数（后续覆盖模式需修正该值）
    uxMessagesWaiting = pxQueue->uxMessagesWaiting;

    // 分支1：项目大小为0（uxItemSize=0）——通常用于同步队列/互斥锁（无实际数据存储）
    if( pxQueue->uxItemSize == ( UBaseType_t ) 0 )
    {
        #if ( configUSE_MUTEXES == 1 )  // 仅启用互斥锁时处理
        {
            // 判断队列是否为互斥锁（FreeRTOS中互斥锁基于队列实现，类型为queueQUEUE_IS_MUTEX）
            if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
            {
                /* 互斥锁释放逻辑：
                   - 调用xTaskPriorityDisinherit：取消持有任务的优先级继承（若之前因互斥锁提升过优先级）
                   - 将互斥锁持有者设为NULL：标记互斥锁已释放
                   - 返回值xReturn：表示是否需要触发任务切换（优先级继承取消后可能需要） */
                xReturn = xTaskPriorityDisinherit( pxQueue->u.xSemaphore.xMutexHolder );
                pxQueue->u.xSemaphore.xMutexHolder = NULL;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记（非互斥锁场景）
            }
        }
        #endif /* configUSE_MUTEXES */
    }
    // 分支2：入队位置为队尾（queueSEND_TO_BACK）——FIFO模式
    else if( xPosition == queueSEND_TO_BACK )
    {
        // 核心操作：将数据源数据复制到队列的“写入指针（pcWriteTo）”位置
        ( void ) memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( size_t ) pxQueue->uxItemSize );
        
        // 更新写入指针：向后偏移一个项目大小（指向下次写入的位置）
        pxQueue->pcWriteTo += pxQueue->uxItemSize;

        // 边界处理：若写入指针超过队列尾部（pcTail），则回卷到队列头部（pcHead）——循环队列特性
        if( pxQueue->pcWriteTo >= pxQueue->u.xQueue.pcTail )
        {
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    // 分支3：入队位置为队首（queueSEND_TO_FRONT）或覆盖（queueOVERWRITE）
    else
    {
        // 核心操作：将数据源数据复制到队列的“读取指针（pcReadFrom）”位置
        // （队首入队/覆盖模式均从读取指针位置写入，因读取指针始终指向队首项目）
        ( void ) memcpy( ( void * ) pxQueue->u.xQueue.pcReadFrom, pvItemToQueue, ( size_t ) pxQueue->uxItemSize );
        
        // 更新读取指针：向前偏移一个项目大小（指向新的队首位置）
        pxQueue->u.xQueue.pcReadFrom -= pxQueue->uxItemSize; //怎么体现覆盖的呢：覆盖用于队列长度为1的场景

        // 边界处理：若读取指针低于队列头部（pcHead），则回卷到队列尾部前一个项目位置——循环队列特性
        if( pxQueue->u.xQueue.pcReadFrom < pxQueue->pcHead )
        {
            pxQueue->u.xQueue.pcReadFrom = ( pxQueue->u.xQueue.pcTail - pxQueue->uxItemSize );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        // 子分支：若为覆盖模式（queueOVERWRITE）——仅单项目队列可用
        if( xPosition == queueOVERWRITE )
        {
            // 若队列中已有项目（uxMessagesWaiting>0），则修正项目计数：
            // 覆盖模式是“替换”而非“新增”，因此先减1（抵消后续的加1操作），确保计数不变
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                --uxMessagesWaiting;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 队列空时覆盖=新增，无需修正
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  // 队首入队模式，无需修正计数
        }
    }

    // 统一更新队列项目数：无论哪种模式，最终计数=修正后的当前数+1（覆盖模式因提前减1，实际计数不变）
    pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxMessagesWaiting + ( UBaseType_t ) 1 );

    return xReturn;  // 仅互斥锁场景有效，其他场景返回pdFALSE（无意义）
}
/*-----------------------------------------------------------*/

static void prvCopyDataFromQueue( Queue_t * const pxQueue,
                                  void * const pvBuffer )
{
    if( pxQueue->uxItemSize != ( UBaseType_t ) 0 )
    {
        pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;

        if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
        {
            pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( size_t ) pxQueue->uxItemSize );
    }
}
/*-----------------------------------------------------------*/

static void prvUnlockQueue( Queue_t * const pxQueue )
{
    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED. */

    /* The lock counts contains the number of extra data items placed or
     * removed from the queue while the queue was locked.  When a queue is
     * locked items can be added or removed, but the event lists cannot be
     * updated. */
    taskENTER_CRITICAL();
    {
        int8_t cTxLock = pxQueue->cTxLock;

        /* See if data was added to the queue while it was locked. */
        while( cTxLock > queueLOCKED_UNMODIFIED )
        {
            /* Data was posted while the queue was locked.  Are any tasks
             * blocked waiting for data to become available? */
            #if ( configUSE_QUEUE_SETS == 1 )
            {
                if( pxQueue->pxQueueSetContainer != NULL )
                {
                    if( prvNotifyQueueSetContainer( pxQueue ) != pdFALSE )
                    {
                        /* The queue is a member of a queue set, and posting to
                         * the queue set caused a higher priority task to unblock.
                         * A context switch is required. */
                        vTaskMissedYield();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    /* Tasks that are removed from the event list will get
                     * added to the pending ready list as the scheduler is still
                     * suspended. */
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* The task waiting has a higher priority so record that a
                             * context switch is required. */
                            vTaskMissedYield();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            #else /* configUSE_QUEUE_SETS */
            {
                /* Tasks that are removed from the event list will get added to
                 * the pending ready list as the scheduler is still suspended. */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        /* The task waiting has a higher priority so record that
                         * a context switch is required. */
                        vTaskMissedYield();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    break;
                }
            }
            #endif /* configUSE_QUEUE_SETS */

            --cTxLock;
        }

        pxQueue->cTxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();

    /* Do the same for the Rx lock. */
    taskENTER_CRITICAL();
    {
        int8_t cRxLock = pxQueue->cRxLock;

        while( cRxLock > queueLOCKED_UNMODIFIED )
        {
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    vTaskMissedYield();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                --cRxLock;
            }
            else
            {
                break;
            }
        }

        pxQueue->cRxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

static BaseType_t prvIsQueueEmpty( const Queue_t * pxQueue )
{
    BaseType_t xReturn;

    taskENTER_CRITICAL();
    {
        if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
        {
            xReturn = pdTRUE;
        }
        else
        {
            xReturn = pdFALSE;
        }
    }
    taskEXIT_CRITICAL();

    return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue )
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = xQueue;

    traceENTER_xQueueIsQueueEmptyFromISR( xQueue );

    configASSERT( pxQueue );

    if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
    {
        xReturn = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    traceRETURN_xQueueIsQueueEmptyFromISR( xReturn );

    return xReturn;
}
/*-----------------------------------------------------------*/

// 静态函数：判断队列是否已满
// 参数：pxQueue - 指向要检查的队列控制结构体的指针
// 返回值：pdTRUE表示队列已满，pdFALSE表示队列未满
static BaseType_t prvIsQueueFull( const Queue_t * pxQueue )
{
    BaseType_t xReturn;  // 用于存储判断结果

    // 进入临界区，确保对队列状态的检查是原子操作
    taskENTER_CRITICAL();
    {
        // 核心判断逻辑：当前队列中的消息数是否等于队列的最大长度
        if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
        {
            xReturn = pdTRUE;  // 消息数等于最大长度，队列已满
        }
        else
        {
            xReturn = pdFALSE;  // 消息数小于最大长度，队列未满
        }
    }
    // 退出临界区，恢复中断和任务调度
    taskEXIT_CRITICAL();

    return xReturn;  // 返回判断结果
}
/*-----------------------------------------------------------*/

BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue )
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = xQueue;

    traceENTER_xQueueIsQueueFullFromISR( xQueue );

    configASSERT( pxQueue );

    if( pxQueue->uxMessagesWaiting == pxQueue->uxLength )
    {
        xReturn = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    traceRETURN_xQueueIsQueueFullFromISR( xReturn );

    return xReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

    BaseType_t xQueueCRSend( QueueHandle_t xQueue,
                             const void * pvItemToQueue,
                             TickType_t xTicksToWait )
    {
        BaseType_t xReturn;
        Queue_t * const pxQueue = xQueue;

        traceENTER_xQueueCRSend( xQueue, pvItemToQueue, xTicksToWait );

        /* If the queue is already full we may have to block.  A critical section
         * is required to prevent an interrupt removing something from the queue
         * between the check to see if the queue is full and blocking on the queue. */
        portDISABLE_INTERRUPTS();
        {
            if( prvIsQueueFull( pxQueue ) != pdFALSE )
            {
                /* The queue is full - do we want to block or just leave without
                 * posting? */
                if( xTicksToWait > ( TickType_t ) 0 )
                {
                    /* As this is called from a coroutine we cannot block directly, but
                     * return indicating that we need to block. */
                    vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToSend ) );
                    portENABLE_INTERRUPTS();
                    return errQUEUE_BLOCKED;
                }
                else
                {
                    portENABLE_INTERRUPTS();
                    return errQUEUE_FULL;
                }
            }
        }
        portENABLE_INTERRUPTS();

        portDISABLE_INTERRUPTS();
        {
            if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
            {
                /* There is room in the queue, copy the data into the queue. */
                prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );
                xReturn = pdPASS;

                /* Were any co-routines waiting for data to become available? */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    /* In this instance the co-routine could be placed directly
                     * into the ready list as we are within a critical section.
                     * Instead the same pending ready list mechanism is used as if
                     * the event were caused from within an interrupt. */
                    if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        /* The co-routine waiting has a higher priority so record
                         * that a yield might be appropriate. */
                        xReturn = errQUEUE_YIELD;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                xReturn = errQUEUE_FULL;
            }
        }
        portENABLE_INTERRUPTS();

        traceRETURN_xQueueCRSend( xReturn );

        return xReturn;
    }

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

    BaseType_t xQueueCRReceive( QueueHandle_t xQueue,
                                void * pvBuffer,
                                TickType_t xTicksToWait )
    {
        BaseType_t xReturn;
        Queue_t * const pxQueue = xQueue;

        traceENTER_xQueueCRReceive( xQueue, pvBuffer, xTicksToWait );

        /* If the queue is already empty we may have to block.  A critical section
         * is required to prevent an interrupt adding something to the queue
         * between the check to see if the queue is empty and blocking on the queue. */
        portDISABLE_INTERRUPTS();
        {
            if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
            {
                /* There are no messages in the queue, do we want to block or just
                 * leave with nothing? */
                if( xTicksToWait > ( TickType_t ) 0 )
                {
                    /* As this is a co-routine we cannot block directly, but return
                     * indicating that we need to block. */
                    vCoRoutineAddToDelayedList( xTicksToWait, &( pxQueue->xTasksWaitingToReceive ) );
                    portENABLE_INTERRUPTS();
                    return errQUEUE_BLOCKED;
                }
                else
                {
                    portENABLE_INTERRUPTS();
                    return errQUEUE_FULL;
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        portENABLE_INTERRUPTS();

        portDISABLE_INTERRUPTS();
        {
            if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                /* Data is available from the queue. */
                pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;

                if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
                {
                    pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                --( pxQueue->uxMessagesWaiting );
                ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

                xReturn = pdPASS;

                /* Were any co-routines waiting for space to become available? */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    /* In this instance the co-routine could be placed directly
                     * into the ready list as we are within a critical section.
                     * Instead the same pending ready list mechanism is used as if
                     * the event were caused from within an interrupt. */
                    if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        xReturn = errQUEUE_YIELD;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                xReturn = pdFAIL;
            }
        }
        portENABLE_INTERRUPTS();

        traceRETURN_xQueueCRReceive( xReturn );

        return xReturn;
    }

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

    BaseType_t xQueueCRSendFromISR( QueueHandle_t xQueue,
                                    const void * pvItemToQueue,
                                    BaseType_t xCoRoutinePreviouslyWoken )
    {
        Queue_t * const pxQueue = xQueue;

        traceENTER_xQueueCRSendFromISR( xQueue, pvItemToQueue, xCoRoutinePreviouslyWoken );

        /* Cannot block within an ISR so if there is no space on the queue then
         * exit without doing anything. */
        if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
        {
            prvCopyDataToQueue( pxQueue, pvItemToQueue, queueSEND_TO_BACK );

            /* We only want to wake one co-routine per ISR, so check that a
             * co-routine has not already been woken. */
            if( xCoRoutinePreviouslyWoken == pdFALSE )
            {
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        return pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        traceRETURN_xQueueCRSendFromISR( xCoRoutinePreviouslyWoken );

        return xCoRoutinePreviouslyWoken;
    }

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

    BaseType_t xQueueCRReceiveFromISR( QueueHandle_t xQueue,
                                       void * pvBuffer,
                                       BaseType_t * pxCoRoutineWoken )
    {
        BaseType_t xReturn;
        Queue_t * const pxQueue = xQueue;

        traceENTER_xQueueCRReceiveFromISR( xQueue, pvBuffer, pxCoRoutineWoken );

        /* We cannot block from an ISR, so check there is data available. If
         * not then just leave without doing anything. */
        if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            /* Copy the data from the queue. */
            pxQueue->u.xQueue.pcReadFrom += pxQueue->uxItemSize;

            if( pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail )
            {
                pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            --( pxQueue->uxMessagesWaiting );
            ( void ) memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->u.xQueue.pcReadFrom, ( unsigned ) pxQueue->uxItemSize );

            if( ( *pxCoRoutineWoken ) == pdFALSE )
            {
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    if( xCoRoutineRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        *pxCoRoutineWoken = pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            xReturn = pdPASS;
        }
        else
        {
            xReturn = pdFAIL;
        }

        traceRETURN_xQueueCRReceiveFromISR( xReturn );

        return xReturn;
    }

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

    void vQueueAddToRegistry( QueueHandle_t xQueue,
                              const char * pcQueueName )
    {
        UBaseType_t ux;
        QueueRegistryItem_t * pxEntryToWrite = NULL;

        traceENTER_vQueueAddToRegistry( xQueue, pcQueueName );

        configASSERT( xQueue );

        if( pcQueueName != NULL )
        {
            /* See if there is an empty space in the registry.  A NULL name denotes
             * a free slot. */
            for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
            {
                /* Replace an existing entry if the queue is already in the registry. */
                if( xQueue == xQueueRegistry[ ux ].xHandle )
                {
                    pxEntryToWrite = &( xQueueRegistry[ ux ] );
                    break;
                }
                /* Otherwise, store in the next empty location */
                else if( ( pxEntryToWrite == NULL ) && ( xQueueRegistry[ ux ].pcQueueName == NULL ) )
                {
                    pxEntryToWrite = &( xQueueRegistry[ ux ] );
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }

        if( pxEntryToWrite != NULL )
        {
            /* Store the information on this queue. */
            pxEntryToWrite->pcQueueName = pcQueueName;
            pxEntryToWrite->xHandle = xQueue;

            traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName );
        }

        traceRETURN_vQueueAddToRegistry();
    }

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

    const char * pcQueueGetName( QueueHandle_t xQueue )
    {
        UBaseType_t ux;
        const char * pcReturn = NULL;

        traceENTER_pcQueueGetName( xQueue );

        configASSERT( xQueue );

        /* Note there is nothing here to protect against another task adding or
         * removing entries from the registry while it is being searched. */

        for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
        {
            if( xQueueRegistry[ ux ].xHandle == xQueue )
            {
                pcReturn = xQueueRegistry[ ux ].pcQueueName;
                break;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }

        traceRETURN_pcQueueGetName( pcReturn );

        return pcReturn;
    }

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

    void vQueueUnregisterQueue( QueueHandle_t xQueue )
    {
        UBaseType_t ux;

        traceENTER_vQueueUnregisterQueue( xQueue );

        configASSERT( xQueue );

        /* See if the handle of the queue being unregistered in actually in the
         * registry. */
        for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
        {
            if( xQueueRegistry[ ux ].xHandle == xQueue )
            {
                /* Set the name to NULL to show that this slot if free again. */
                xQueueRegistry[ ux ].pcQueueName = NULL;

                /* Set the handle to NULL to ensure the same queue handle cannot
                 * appear in the registry twice if it is added, removed, then
                 * added again. */
                xQueueRegistry[ ux ].xHandle = ( QueueHandle_t ) 0;
                break;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }

        traceRETURN_vQueueUnregisterQueue();
    }

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

// 条件编译：仅当启用软件定时器功能（configUSE_TIMERS == 1）时，才编译该函数
#if ( configUSE_TIMERS == 1 )

    // 函数定义：受限的队列消息等待函数（内核内部使用，非应用层API）
    // 参数：
    //   xQueue - 目标队列句柄（等待该队列的消息）
    //   xTicksToWait - 阻塞等待时间（单位：时钟节拍；portMAX_DELAY表示永久等待）
    //   xWaitIndefinitely - 布尔值，标记是否“强制永久等待”（即使xTicksToWait非portMAX_DELAY，也会忽略超时时间）
    void vQueueWaitForMessageRestricted( QueueHandle_t xQueue,
                                         TickType_t xTicksToWait,
                                         const BaseType_t xWaitIndefinitely )
    {
        // 将队列句柄强制转换为队列结构体指针（操作底层队列数据）
        Queue_t * const pxQueue = xQueue;

        // 跟踪函数进入（调试用，记录“开始受限等待队列消息”）
        traceENTER_vQueueWaitForMessageRestricted( xQueue, xTicksToWait, xWaitIndefinitely );

        /* 注释：本函数禁止应用层代码调用（名称中的“Restricted”即表示“受限”），不属于公共API；
         * 设计用途：仅供内核代码（如软件定时器服务任务）使用，且有特殊调用要求：
         * 1. 调用时需锁定调度器（scheduler locked），但不能在临界区（critical section）内调用；
         * 2. 本函数可能调用vListInsert()向“仅含一个元素的列表”插入项，因此列表操作效率极高。 */

        /* 注释：仅当队列无消息时才执行操作：
         * 1. 本函数不会直接导致任务阻塞，仅将任务放入“阻塞列表”（xTasksWaitingToReceive）；
         * 2. 任务真正阻塞需等待调度器解锁——解锁时会触发任务切换（yield）；
         * 3. 若队列锁定期间有消息被添加，且当前任务已被放入阻塞列表，则调度器解锁时会立即唤醒该任务。 */
        // 锁定队列（禁止其他任务/中断修改队列状态，如添加/移除消息、操作等待列表）
        prvLockQueue( pxQueue );

        // 分支1：队列无消息（uxMessagesWaiting == 0），需将任务放入阻塞列表
        if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0U )
        {
            /* 调用受限的事件列表放置函数，将当前任务添加到队列的“接收等待列表”（xTasksWaitingToReceive）：
             * - 参数1：&pxQueue->xTasksWaitingToReceive：目标阻塞列表（等待该队列消息的任务列表）；
             * - 参数2：xTicksToWait：基础等待时间；
             * - 参数3：xWaitIndefinitely：强制永久等待标记（若为pdTRUE，xTicksToWait会被忽略，按永久等待处理）；
             * 作用：将任务按等待时间排序插入列表，设置任务的阻塞超时时间。 */
            vTaskPlaceOnEventListRestricted( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait, xWaitIndefinitely );
        }
        else  // 分支2：队列有消息，无需阻塞，仅标记覆盖测试
        {
            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际业务逻辑
        }

        // 解锁队列（恢复对队列状态的修改权限）
        prvUnlockQueue( pxQueue );

        // 跟踪函数返回（调试用，记录“受限等待队列消息结束”）
        traceRETURN_vQueueWaitForMessageRestricted();
    }

#endif /* 结束 configUSE_TIMERS == 1 的条件编译 */

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if ( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

    QueueSetHandle_t xQueueCreateSet( const UBaseType_t uxEventQueueLength )
    {
        QueueSetHandle_t pxQueue;

        traceENTER_xQueueCreateSet( uxEventQueueLength );

        pxQueue = xQueueGenericCreate( uxEventQueueLength, ( UBaseType_t ) sizeof( Queue_t * ), queueQUEUE_TYPE_SET );

        traceRETURN_xQueueCreateSet( pxQueue );

        return pxQueue;
    }

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

    BaseType_t xQueueAddToSet( QueueSetMemberHandle_t xQueueOrSemaphore,
                               QueueSetHandle_t xQueueSet )
    {
        BaseType_t xReturn;

        traceENTER_xQueueAddToSet( xQueueOrSemaphore, xQueueSet );

        taskENTER_CRITICAL();
        {
            if( ( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer != NULL )
            {
                /* Cannot add a queue/semaphore to more than one queue set. */
                xReturn = pdFAIL;
            }
            else if( ( ( Queue_t * ) xQueueOrSemaphore )->uxMessagesWaiting != ( UBaseType_t ) 0 )
            {
                /* Cannot add a queue/semaphore to a queue set if there are already
                 * items in the queue/semaphore. */
                xReturn = pdFAIL;
            }
            else
            {
                ( ( Queue_t * ) xQueueOrSemaphore )->pxQueueSetContainer = xQueueSet;
                xReturn = pdPASS;
            }
        }
        taskEXIT_CRITICAL();

        traceRETURN_xQueueAddToSet( xReturn );

        return xReturn;
    }

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

    BaseType_t xQueueRemoveFromSet( QueueSetMemberHandle_t xQueueOrSemaphore,
                                    QueueSetHandle_t xQueueSet )
    {
        BaseType_t xReturn;
        Queue_t * const pxQueueOrSemaphore = ( Queue_t * ) xQueueOrSemaphore;

        traceENTER_xQueueRemoveFromSet( xQueueOrSemaphore, xQueueSet );

        if( pxQueueOrSemaphore->pxQueueSetContainer != xQueueSet )
        {
            /* The queue was not a member of the set. */
            xReturn = pdFAIL;
        }
        else if( pxQueueOrSemaphore->uxMessagesWaiting != ( UBaseType_t ) 0 )
        {
            /* It is dangerous to remove a queue from a set when the queue is
             * not empty because the queue set will still hold pending events for
             * the queue. */
            xReturn = pdFAIL;
        }
        else
        {
            taskENTER_CRITICAL();
            {
                /* The queue is no longer contained in the set. */
                pxQueueOrSemaphore->pxQueueSetContainer = NULL;
            }
            taskEXIT_CRITICAL();
            xReturn = pdPASS;
        }

        traceRETURN_xQueueRemoveFromSet( xReturn );

        return xReturn;
    }

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

    QueueSetMemberHandle_t xQueueSelectFromSet( QueueSetHandle_t xQueueSet,
                                                TickType_t const xTicksToWait )
    {
        QueueSetMemberHandle_t xReturn = NULL;

        traceENTER_xQueueSelectFromSet( xQueueSet, xTicksToWait );

        ( void ) xQueueReceive( ( QueueHandle_t ) xQueueSet, &xReturn, xTicksToWait );

        traceRETURN_xQueueSelectFromSet( xReturn );

        return xReturn;
    }

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

    QueueSetMemberHandle_t xQueueSelectFromSetFromISR( QueueSetHandle_t xQueueSet )
    {
        QueueSetMemberHandle_t xReturn = NULL;

        traceENTER_xQueueSelectFromSetFromISR( xQueueSet );

        ( void ) xQueueReceiveFromISR( ( QueueHandle_t ) xQueueSet, &xReturn, NULL );

        traceRETURN_xQueueSelectFromSetFromISR( xReturn );

        return xReturn;
    }

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

    static BaseType_t prvNotifyQueueSetContainer( const Queue_t * const pxQueue )
    {
        Queue_t * pxQueueSetContainer = pxQueue->pxQueueSetContainer;
        BaseType_t xReturn = pdFALSE;

        /* This function must be called form a critical section. */

        /* The following line is not reachable in unit tests because every call
         * to prvNotifyQueueSetContainer is preceded by a check that
         * pxQueueSetContainer != NULL */
        configASSERT( pxQueueSetContainer ); /* LCOV_EXCL_BR_LINE */
        configASSERT( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength );

        if( pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength )
        {
            const int8_t cTxLock = pxQueueSetContainer->cTxLock;

            traceQUEUE_SET_SEND( pxQueueSetContainer );

            /* The data copied is the handle of the queue that contains data. */
            xReturn = prvCopyDataToQueue( pxQueueSetContainer, &pxQueue, queueSEND_TO_BACK );

            if( cTxLock == queueUNLOCKED )
            {
                if( listLIST_IS_EMPTY( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    if( xTaskRemoveFromEventList( &( pxQueueSetContainer->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        /* The task waiting has a higher priority. */
                        xReturn = pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                prvIncrementQueueTxLock( pxQueueSetContainer, cTxLock );
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        return xReturn;
    }

#endif /* configUSE_QUEUE_SETS */
