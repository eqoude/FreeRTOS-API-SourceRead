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

/* Standard includes. */
#include <stdlib.h>

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 可防止 task.h 重新定义所有 API 函数以使用 MPU 包装函数。
 * 仅当从应用程序文件中包含 task.h 时，才应执行（重新定义 API 函数使用 MPU 包装函数）这一操作。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* 若已开启 xTimerPendFunctionCall 功能（INCLUDE_xTimerPendFunctionCall == 1），但未启用定时器功能（configUSE_TIMERS == 0），
   则触发编译错误，提示“需将 configUSE_TIMERS 设置为 1，才能使用 xTimerPendFunctionCall() 函数”。 */
#if ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 0 )
    #error configUSE_TIMERS must be set to 1 to make the xTimerPendFunctionCall() function available.
#endif

/* 对于支持 MPU（内存保护单元）的端口，为确保生成正确的特权模式/非特权模式链接属性及代码放置位置，
   上述头文件（指当前代码之前包含的头文件，如 task.h 等）需要定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，
   但本文件中无需定义该宏，因此此处将其取消定义。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE


/* 若应用程序未配置为包含软件定时器功能，则将跳过整个源文件。此 #if 指令将在本文件的最末尾处闭合。
 * 若您希望包含软件定时器功能，请确保在 FreeRTOSConfig.h 中将 configUSE_TIMERS 设置为 1。 */
#if ( configUSE_TIMERS == 1 )

/* 其他杂项定义 */
    #define tmrNO_DELAY                    ( ( TickType_t ) 0U )
    #define tmrMAX_TIME_BEFORE_OVERFLOW    ( ( TickType_t ) -1 )

/* 分配给定时器服务任务的名称。可以通过在 FreeRTOSConfig.h 中定义 configTIMER_SERVICE_TASK_NAME 来重写此名称。 */
    #ifndef configTIMER_SERVICE_TASK_NAME
        #define configTIMER_SERVICE_TASK_NAME    "Tmr Svc"
    #endif

    #if ( ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) ) //configUSE_CORE_AFFINITY == 1：启用了任务核心亲和性配置功能

/* 在SMP（对称多处理器）系统上分配给定时器服务任务的核心亲和性。
 * 可以通过在FreeRTOSConfig.h中定义configTIMER_SERVICE_TASK_CORE_AFFINITY来重写此设置。 */
        #ifndef configTIMER_SERVICE_TASK_CORE_AFFINITY
            #define configTIMER_SERVICE_TASK_CORE_AFFINITY    tskNO_AFFINITY
        #endif
    #endif /* #if ( ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) ) */

/* 用于定时器结构体中 ucStatus 成员的位定义 */
    #define tmrSTATUS_IS_ACTIVE                  ( 0x01U ) /* 定时器处于激活状态 */
    #define tmrSTATUS_IS_STATICALLY_ALLOCATED    ( 0x02U ) /* 定时器由静态内存分配 */
    #define tmrSTATUS_IS_AUTORELOAD              ( 0x04U ) /* 定时器为自动重载模式 */

/* The definition of the timers themselves. */
    typedef struct tmrTimerControl                                               /* 使用旧的命名约定是为了避免破坏内核感知调试器 */
    {
        const char * pcTimerName;                                                /**< 文本名称。内核不使用此名称，仅为方便调试而包含 */
        ListItem_t xTimerListItem;                                               /**< 所有内核功能用于事件管理的标准链表项 */
        TickType_t xTimerPeriodInTicks;                                          /**< 定时器的过期速度和周期 */
        void * pvTimerID;                                                        /**< 用于标识定时器的ID。当多个定时器使用相同的回调函数时，可通过此ID区分 */
        portTIMER_CALLBACK_ATTRIBUTE TimerCallbackFunction_t pxCallbackFunction; /**< 定时器过期时将被调用的函数 */
        #if ( configUSE_TRACE_FACILITY == 1 )
            UBaseType_t uxTimerNumber;                                           /**< 由FreeRTOS+Trace等跟踪工具分配的ID */
        #endif
        uint8_t ucStatus;                                                        /**< 包含位信息，用于表示定时器是否为静态分配以及是否处于活动状态 */
    } xTIMER;

/* 上面保留了旧的 xTIMER 名称，然后在下面将其类型定义为新的 Timer_t 名称，以支持旧版的内核感知调试器。 */
    typedef xTIMER Timer_t;

/* 可在定时器队列上发送和接收的消息定义。
 * 队列中可存在两种类型的消息：一种是用于操作软件定时器的消息，
 * 另一种是用于请求执行非定时器相关回调函数的消息。
 * 这两种消息类型分别定义在两个独立的结构体中，即 xTimerParametersType 和 xCallbackParametersType。 */
    typedef struct tmrTimerParameters
    {
        TickType_t xMessageValue; /**< 可选值，供部分命令使用，例如在更改定时器周期时。 */
        Timer_t * pxTimer;        /**< 要应用命令的目标定时器。 */
    } TimerParameter_t;


    typedef struct tmrCallbackParameters
    {
        portTIMER_CALLBACK_ATTRIBUTE
        PendedFunction_t pxCallbackFunction; /* << 要执行的回调函数 */
        void * pvParameter1;                 /* << 用作回调函数第一个参数的值 */
        uint32_t ulParameter2;               /* << 用作回调函数第二个参数的值 */
    } CallbackParameters_t;

/* 该结构体包含两种消息类型，以及一个用于确定哪种种消息类型有效的标识符。 */
    typedef struct tmrTimerQueueMessage
    {
        BaseType_t xMessageID; /**< 发送给定时器服务任务的命令。 */
        union
        {
            TimerParameter_t xTimerParameters;

            /* 如果不打算使用 xCallbackParameters，则不包含它，
             * 因为这会增大结构体（进而增大定时器队列）的大小。 */
            #if ( INCLUDE_xTimerPendFunctionCall == 1 )
                CallbackParameters_t xCallbackParameters;
            #endif /* INCLUDE_xTimerPendFunctionCall */
        } u;
    } DaemonTaskMessage_t;

/* 存储活动定时器的列表。定时器按到期时间排序，最近要到期的定时器位于列表前端。
 * 只有定时器服务任务被允许访问这些列表。
 * xActiveTimerList1 和 xActiveTimerList2 本可以放在函数作用域内，但这样会破坏
 * 一些内核感知调试器，以及依赖移除 static 限定符的调试器。 */
    PRIVILEGED_DATA static List_t xActiveTimerList1;
    PRIVILEGED_DATA static List_t xActiveTimerList2;
    PRIVILEGED_DATA static List_t * pxCurrentTimerList;
    PRIVILEGED_DATA static List_t * pxOverflowTimerList;

/* 用于向定时器服务任务发送命令的队列。 */
    PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;
    PRIVILEGED_DATA static TaskHandle_t xTimerTaskHandle = NULL;

/*-----------------------------------------------------------*/

/*
 * 如果定时器服务任务所使用的基础架构尚未初始化，则对其进行初始化。
 */
    static void prvCheckForValidListAndQueue( void ) PRIVILEGED_FUNCTION;

/*
 * 定时器服务任务（后台任务）。定时器功能由此任务控制。
 * 其他任务通过 xTimerQueue 队列与定时器服务任务通信。
 */
    static portTASK_FUNCTION_PROTO( prvTimerTask, pvParameters ) PRIVILEGED_FUNCTION;

/*
 * 由定时器服务任务调用，用于解析和处理其在定时器队列上接收的命令。
 */
    static void prvProcessReceivedCommands( void ) PRIVILEGED_FUNCTION;

/*
 * 根据过期时间是否会导致定时器计数器溢出，将定时器插入 xActiveTimerList1 或 xActiveTimerList2。
 */
    static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer,
                                                  const TickType_t xNextExpiryTime,
                                                  const TickType_t xTimeNow,
                                                  const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/*
 * 重新加载指定的自动重载定时器。如果重载操作存在积压，
 * 则清除积压，为每一次额外的重载调用回调函数。
 * 当此函数返回时，下一次过期时间将在 xTimeNow 之后。
 */
    static void prvReloadTimer( Timer_t * const pxTimer,
                                TickType_t xExpiredTime,
                                const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * 活动定时器已达到其过期时间。如果是自动重载定时器，则重新加载，然后调用其回调函数。
 */
    static void prvProcessExpiredTimer( const TickType_t xNextExpireTime,
                                        const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * 节拍计数已溢出。在确保当前定时器列表不再引用任何定时器后，切换定时器列表。
 */
    static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/*
 * 获取当前节拍计数，如果自上次调用 prvSampleTimeNow() 以来发生了节拍计数溢出，
 * 则将 *pxTimerListsWereSwitched 设置为 pdTRUE。
 */
    static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/*
 * 如果定时器列表包含任何活动定时器，则返回最早将要过期的定时器的过期时间，
 * 并将 *pxListWasEmpty 设置为 false。如果定时器列表不包含任何定时器，
 * 则返回 0 并将 *pxListWasEmpty 设置为 pdTRUE。
 */
    static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * 如果有定时器已过期，则处理它。否则，阻塞定时器服务任务，
 * 直到有定时器过期或收到命令为止。
 */
    static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime,
                                            BaseType_t xListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * 在 Timer_t 结构体被静态或动态分配后调用，用于填充结构体的成员。
 */
    static void prvInitialiseNewTimer( const char * const pcTimerName,
                                       const TickType_t xTimerPeriodInTicks,
                                       const BaseType_t xAutoReload,
                                       void * const pvTimerID,
                                       TimerCallbackFunction_t pxCallbackFunction,
                                       Timer_t * pxNewTimer ) PRIVILEGED_FUNCTION;
/*-----------------------------------------------------------*/

// 函数：创建FreeRTOS软件定时器的“定时器服务任务”（仅当启用软件定时器时调用）
BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdFAIL;  // 函数返回值（默认失败，创建成功后改为pdPASS）

    traceENTER_xTimerCreateTimerTask();  // 调试跟踪：函数入口

    /* 注释说明：
     * 当configUSE_TIMERS（软件定时器功能开关）设为1时，此函数在调度器启动时被调用；
     * 作用是检查定时器服务任务依赖的基础结构（如定时器队列、定时器列表）是否已初始化；
     * 若定时器服务任务已创建过，基础结构应已完成初始化，无需重复操作。 */
    prvCheckForValidListAndQueue();  // 内部函数：检查定时器队列、定时器列表是否有效

    // 若定时器队列（xTimerQueue）已初始化（非NULL），则开始创建定时器服务任务
    if( xTimerQueue != NULL )
    {
        // 分支1：多核系统（核心数>1）且启用核心亲和性（任务绑定特定核心）
        #if ( ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) )
        {
            // 子分支1.1：使用静态内存分配创建定时器服务任务
            #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                StaticTask_t * pxTimerTaskTCBBuffer = NULL;  // 定时器任务TCB（任务控制块）的静态内存缓冲区
                StackType_t * pxTimerTaskStackBuffer = NULL;  // 定时器任务栈的静态内存缓冲区
                configSTACK_DEPTH_TYPE uxTimerTaskStackSize;  // 定时器任务栈的大小

                // 调用应用层实现的函数，获取定时器任务的静态内存（TCB、栈缓冲区、栈大小）
                vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &uxTimerTaskStackSize );
                
                // 调用支持核心亲和性的静态创建API，创建定时器服务任务
                xTimerTaskHandle = xTaskCreateStaticAffinitySet( 
                    prvTimerTask,                          // 定时器服务任务的核心函数（处理定时器超时）
                    configTIMER_SERVICE_TASK_NAME,         // 任务名称（由配置宏定义，如"TimerService"）
                    uxTimerTaskStackSize,                  // 任务栈大小（应用层指定）
                    NULL,                                  // 任务参数（定时器任务无需参数，传NULL）
                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  // 任务优先级（配置宏指定 + 特权模式位）
                    pxTimerTaskStackBuffer,                // 任务栈静态缓冲区
                    pxTimerTaskTCBBuffer,                  // 任务TCB静态缓冲区
                    configTIMER_SERVICE_TASK_CORE_AFFINITY // 核心亲和性掩码（指定任务绑定的核心）
                );

                // 若任务句柄非NULL，说明创建成功，更新返回值
                if( xTimerTaskHandle != NULL )
                {
                    xReturn = pdPASS;
                }
            }
            #else /* 子分支1.2：使用动态内存分配创建定时器服务任务 */
            {
                // 调用支持核心亲和性的动态创建API，创建定时器服务任务
                xReturn = xTaskCreateAffinitySet(
                    prvTimerTask,                          // 定时器服务任务函数
                    configTIMER_SERVICE_TASK_NAME,         // 任务名称
                    configTIMER_TASK_STACK_DEPTH,          // 任务栈大小（配置宏指定的默认深度）
                    NULL,                                  // 任务参数
                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  // 任务优先级 + 特权模式位
                    configTIMER_SERVICE_TASK_CORE_AFFINITY, // 核心亲和性掩码
                    &xTimerTaskHandle                      // 输出参数：存储定时器任务句柄
                );
            }
            #endif /* configSUPPORT_STATIC_ALLOCATION */
        }
        #else /* 分支2：单核系统，或多核但未启用核心亲和性 */
        {
            // 子分支2.1：使用静态内存分配创建定时器服务任务
            #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                StaticTask_t * pxTimerTaskTCBBuffer = NULL;  // 定时器任务TCB静态缓冲区
                StackType_t * pxTimerTaskStackBuffer = NULL;  // 定时器任务栈静态缓冲区
                configSTACK_DEPTH_TYPE uxTimerTaskStackSize;  // 定时器任务栈大小

                // 调用应用层函数，获取定时器任务的静态内存
                vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &uxTimerTaskStackSize );
                
                // 调用普通静态创建API，创建定时器服务任务
                xTimerTaskHandle = xTaskCreateStatic(
                    prvTimerTask,                          // 定时器服务任务函数
                    configTIMER_SERVICE_TASK_NAME,         // 任务名称
                    uxTimerTaskStackSize,                  // 任务栈大小
                    NULL,                                  // 任务参数
                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  // 任务优先级 + 特权模式位
                    pxTimerTaskStackBuffer,                // 任务栈静态缓冲区
                    pxTimerTaskTCBBuffer                   // 任务TCB静态缓冲区
                );

                // 检查创建结果，更新返回值
                if( xTimerTaskHandle != NULL )
                {
                    xReturn = pdPASS;
                }
            }
            #else /* 子分支2.2：使用动态内存分配创建定时器服务任务 */
            {
                // 调用普通动态创建API，创建定时器服务任务
                xReturn = xTaskCreate(
                    prvTimerTask,                          // 定时器服务任务函数
                    configTIMER_SERVICE_TASK_NAME,         // 任务名称
                    configTIMER_TASK_STACK_DEPTH,          // 任务栈大小（配置宏指定）
                    NULL,                                  // 任务参数
                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT,  // 任务优先级 + 特权模式位
                    &xTimerTaskHandle                      // 输出参数：存储任务句柄
                );
            }
            #endif /* configSUPPORT_STATIC_ALLOCATION */
        }
        #endif /* ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) */
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（定时器队列为NULL时执行，无实际逻辑）
    }

    configASSERT( xReturn );  // 断言：确保定时器服务任务创建成功（失败会触发断言，提示错误）

    traceRETURN_xTimerCreateTimerTask( xReturn );  // 调试跟踪：函数返回，携带创建结果

    return xReturn;  // 返回创建结果（pdPASS成功，pdFAIL失败）
}
/*-----------------------------------------------------------*/

    #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

        TimerHandle_t xTimerCreate( const char * const pcTimerName,
                                    const TickType_t xTimerPeriodInTicks,
                                    const BaseType_t xAutoReload,
                                    void * const pvTimerID,
                                    TimerCallbackFunction_t pxCallbackFunction )
        {
            Timer_t * pxNewTimer;

            traceENTER_xTimerCreate( pcTimerName, xTimerPeriodInTicks, xAutoReload, pvTimerID, pxCallbackFunction );

            /* MISRA Ref 11.5.1 [Malloc memory assignment] */
            /* More details at: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
            /* coverity[misra_c_2012_rule_11_5_violation] */
            pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) );

            if( pxNewTimer != NULL )
            {
                /* 定时器状态初始化为0：
                 * 1. 非静态创建（无静态内存标记）
                 * 2. 尚未启动（无“已启动”标记）
                 * 3. 自动重载标记将在prvInitialiseNewTimer中根据xAutoReload参数设置
                 */
                pxNewTimer->ucStatus = 0x00;
                prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, xAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );
            }

            traceRETURN_xTimerCreate( pxNewTimer );

            return pxNewTimer;
        }

    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

// 若配置支持静态内存分配（configSUPPORT_STATIC_ALLOCATION == 1）
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

        // 函数：创建软件定时器（静态内存分配方式），返回定时器句柄（TimerHandle_t）
        TimerHandle_t xTimerCreateStatic( const char * const pcTimerName,          // 定时器名称（字符串常量）
                                          const TickType_t xTimerPeriodInTicks,    // 定时器周期（单位：系统时钟节拍）
                                          const BaseType_t xAutoReload,            // 是否自动重载（pdTRUE=自动，pdFALSE=单次）
                                          void * const pvTimerID,                  // 定时器ID（用于区分多个定时器）
                                          TimerCallbackFunction_t pxCallbackFunction,  // 定时器超时回调函数
                                          StaticTimer_t * pxTimerBuffer )          // 静态分配的定时器内存缓冲区（由应用层提供）
        {
            Timer_t * pxNewTimer;  // 指向内部定时器结构体（Timer_t）的指针（TimerHandle_t本质是该结构体指针别名）

            // 跟踪函数进入（调试/性能分析用，记录静态创建的参数）
            traceENTER_xTimerCreateStatic( pcTimerName, xTimerPeriodInTicks, xAutoReload, pvTimerID, pxCallbackFunction, pxTimerBuffer );

            // 若启用断言（configASSERT_DEFINED == 1），进行内存大小校验
            #if ( configASSERT_DEFINED == 1 )
            {
                /* 合理性检查：确保应用层提供的StaticTimer_t结构体大小，
                 * 与内核内部真实的Timer_t结构体大小完全一致（避免内存越界）。 */
                volatile size_t xSize = sizeof( StaticTimer_t );  // 计算StaticTimer_t的大小
                configASSERT( xSize == sizeof( Timer_t ) );       // 断言检查大小是否匹配
                ( void ) xSize; /* 当configASSERT未定义时，避免“未使用变量”编译警告 */
            }
            #endif /* configASSERT_DEFINED */

            /* 必须由应用层提供指向StaticTimer_t结构体的指针（静态内存缓冲区），否则断言触发 */
            configASSERT( pxTimerBuffer );
            
            /* MISRA 规则 11.3.1 引用 [ 未对齐访问 ] */
            /* 详细说明见：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-113 */
            /* coverity[misra_c_2012_rule_11_3_violation] */  // 告知代码检查工具此处为功能必需的规则例外
            // 将应用层提供的StaticTimer_t指针，强制转换为内核内部的Timer_t指针（内存大小已通过断言确保一致）
            pxNewTimer = ( Timer_t * ) pxTimerBuffer;

            // 若静态缓冲区指针有效（pxNewTimer不为NULL，实际由configASSERT保障）
            if( pxNewTimer != NULL )
            {
                /* 初始化定时器状态位：
                 * 1. 置“静态分配”标志（tmrSTATUS_IS_STATICALLY_ALLOCATED），标记此定时器内存由应用层提供，
                 *    避免后续删除时误调用vPortFree释放内存（静态内存无需动态释放）；
                 * 2. 自动重载标志将在prvInitialiseNewTimer中根据xAutoReload参数补充设置。
                 */
                pxNewTimer->ucStatus = ( uint8_t ) tmrSTATUS_IS_STATICALLY_ALLOCATED;

                // 调用内部初始化函数，填充定时器核心参数（与动态创建共用同一初始化逻辑，保证一致性）
                prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, xAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );
            }

            // 跟踪函数返回（调试/性能分析用，记录返回的定时器句柄）
            traceRETURN_xTimerCreateStatic( pxNewTimer );

            // 返回定时器句柄（若pxTimerBuffer无效则返回NULL，实际由configASSERT保障有效）
            return pxNewTimer;
        }

    #endif /* configSUPPORT_STATIC_ALLOCATION */  // 静态内存分配配置的条件编译结束
/*-----------------------------------------------------------*/

// 静态内部函数：初始化新创建的定时器结构体（Timer_t）
// 仅在FreeRTOS内核内部调用，不对外暴露接口
static void prvInitialiseNewTimer( const char * const pcTimerName,          // 定时器名称（字符串常量）
                                       const TickType_t xTimerPeriodInTicks,    // 定时器周期（单位：系统时钟节拍）
                                       const BaseType_t xAutoReload,            // 是否自动重载（pdTRUE=自动，pdFALSE=单次）
                                       void * const pvTimerID,                  // 定时器ID（用于区分多个定时器）
                                       TimerCallbackFunction_t pxCallbackFunction,  // 定时器超时回调函数
                                       Timer_t * pxNewTimer )                    // 待初始化的定时器结构体指针
    {
        /* 定时器周期（xTimerPeriodInTicks）不能为0，否则定时器无意义 */
        configASSERT( ( xTimerPeriodInTicks > 0 ) );  // 断言检查：强制确保周期合法，非法时触发调试断言

        /* 确保定时器服务任务依赖的基础架构（活动定时器列表、定时器队列）已创建/初始化 */
        prvCheckForValidListAndQueue();  // 调用内部检查函数，若基础架构未初始化则自动初始化

        /* 使用传入的函数参数，逐一初始化定时器结构体的成员变量 */
        pxNewTimer->pcTimerName = pcTimerName;  // 赋值定时器名称（仅用于调试识别）
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;  // 赋值定时器周期（核心参数）
        pxNewTimer->pvTimerID = pvTimerID;  // 赋值定时器ID（回调函数中可通过API获取此ID）
        pxNewTimer->pxCallbackFunction = pxCallbackFunction;  // 赋值超时回调函数（定时器到期后执行）
        vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );  // 初始化定时器的列表项（用于加入FreeRTOS列表管理）

        // 若配置为自动重载定时器（xAutoReload不为pdFALSE）
        if( xAutoReload != pdFALSE )
        {
            // 在定时器状态位（ucStatus）中置“自动重载”标志
            // tmrSTATUS_IS_AUTORELOAD是预定义的位掩码，确保仅修改对应状态位，不影响其他位
            pxNewTimer->ucStatus |= ( uint8_t ) tmrSTATUS_IS_AUTORELOAD;
        }

        // 跟踪事件：记录“定时器初始化完成”的调试信息（需配合跟踪工具使用，如FreeRTOS+Trace）
        traceTIMER_CREATE( pxNewTimer );
    }
/*-----------------------------------------------------------*/

// 函数：从任务上下文向定时器服务任务发送通用命令（如启动、停止、重置定时器）
// 返回值：pdPASS表示命令发送成功，pdFAIL表示失败
BaseType_t xTimerGenericCommandFromTask( TimerHandle_t xTimer,                  // 目标定时器句柄（操作的定时器）
                                             const BaseType_t xCommandID,          // 命令ID（指定要执行的操作，如启动、停止）
                                             const TickType_t xOptionalValue,      // 命令可选参数（如重置定时器时的新周期）
                                             BaseType_t * const pxHigherPriorityTaskWoken,  // 任务上下文未使用，仅为接口统一（置NULL即可）
                                             const TickType_t xTicksToWait )       // 发送队列时的阻塞超时时间（单位：时钟节拍）
    {
        BaseType_t xReturn = pdFAIL;  // 初始化返回值为失败
        DaemonTaskMessage_t xMessage; // 定时器服务任务消息结构体（用于封装命令数据）

        ( void ) pxHigherPriorityTaskWoken;  // 未使用参数，强制转换为void避免编译警告

        // 跟踪函数进入（调试/跟踪用，记录命令发送的参数）
        traceENTER_xTimerGenericCommandFromTask( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait );

        // 断言检查：目标定时器句柄必须有效（非NULL）
        configASSERT( xTimer );

        /* 向定时器服务任务发送消息，指示其对特定定时器执行特定操作。
         * （定时器服务任务是唯一有权修改定时器状态的任务，确保线程安全） */
        if( xTimerQueue != NULL )  // 先检查定时器队列（用于通信）是否已初始化
        {
            /* 封装命令消息：将命令ID、可选参数、目标定时器句柄填入消息结构体 */
            xMessage.xMessageID = xCommandID;  // 命令类型（如tmrCOMMAND_START、tmrCOMMAND_STOP）
            xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;  // 命令的附加参数
            xMessage.u.xTimerParameters.pxTimer = xTimer;  // 要操作的目标定时器

            // 断言检查：当前命令必须是“任务上下文专属命令”（不能是中断上下文命令）
            configASSERT( xCommandID < tmrFIRST_FROM_ISR_COMMAND );

            // 确认命令属于任务上下文（非中断上下文）
            if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
            {
                // 检查调度器是否已启动
                if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
                {
                    // 调度器已启动：将消息发送到定时器队列尾部，阻塞xTicksToWait个节拍（等待队列有空闲空间）
                    xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
                }
                else
                {
                    // 调度器未启动：无需阻塞（队列无其他任务使用），立即发送消息
                    xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
                }
            }

            // 跟踪命令发送结果（调试/跟踪用，记录命令是否发送成功）
            traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记（队列未初始化的分支）
        }

        // 跟踪函数返回（调试/跟踪用，记录返回结果）
        traceRETURN_xTimerGenericCommandFromTask( xReturn );

        return xReturn;  // 返回命令发送结果（pdPASS/pdFAIL）
    }
/*-----------------------------------------------------------*/

    BaseType_t xTimerGenericCommandFromISR( TimerHandle_t xTimer,
                                            const BaseType_t xCommandID,
                                            const TickType_t xOptionalValue,
                                            BaseType_t * const pxHigherPriorityTaskWoken,
                                            const TickType_t xTicksToWait )
    {
        BaseType_t xReturn = pdFAIL;
        DaemonTaskMessage_t xMessage;

        ( void ) xTicksToWait;

        traceENTER_xTimerGenericCommandFromISR( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait );

        configASSERT( xTimer );

        /* Send a message to the timer service task to perform a particular action
         * on a particular timer definition. */
        if( xTimerQueue != NULL )
        {
            /* Send a command to the timer service task to start the xTimer timer. */
            xMessage.xMessageID = xCommandID;
            xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;
            xMessage.u.xTimerParameters.pxTimer = xTimer;

            configASSERT( xCommandID >= tmrFIRST_FROM_ISR_COMMAND );

            if( xCommandID >= tmrFIRST_FROM_ISR_COMMAND )
            {
                xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
            }

            traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        traceRETURN_xTimerGenericCommandFromISR( xReturn );

        return xReturn;
    }
/*-----------------------------------------------------------*/

// 函数：获取定时器守护任务（即定时器服务任务）的句柄
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
    // 跟踪函数进入（调试/跟踪用，记录函数调用事件）
    traceENTER_xTimerGetTimerDaemonTaskHandle();

    /* 若在调度器启动前调用xTimerGetTimerDaemonTaskHandle()，
     * 则xTimerTaskHandle（定时器服务任务句柄）会是NULL（因服务任务尚未创建）。 */
    // 断言检查：确保调用此函数时，定时器服务任务句柄已有效（非NULL）
    configASSERT( ( xTimerTaskHandle != NULL ) );

    // 跟踪函数返回（调试/跟踪用，记录返回的任务句柄）
    traceRETURN_xTimerGetTimerDaemonTaskHandle( xTimerTaskHandle );

    // 返回定时器服务任务的句柄（xTimerTaskHandle是全局变量，在xTimerCreateTimerTask中初始化）
    return xTimerTaskHandle;
}

/*-----------------------------------------------------------*/

    TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
    {
        Timer_t * pxTimer = xTimer;

        traceENTER_xTimerGetPeriod( xTimer );

        configASSERT( xTimer );

        traceRETURN_xTimerGetPeriod( pxTimer->xTimerPeriodInTicks );

        return pxTimer->xTimerPeriodInTicks;
    }
/*-----------------------------------------------------------*/

// 函数：设置定时器的重载模式（自动重载或单次触发）
void vTimerSetReloadMode( TimerHandle_t xTimer,          // 目标定时器句柄
                              const BaseType_t xAutoReload )  // 重载模式：pdTRUE=自动重载，pdFALSE=单次触发
    {
        Timer_t * pxTimer = xTimer;  // 将定时器句柄转换为内部定时器结构体指针（TimerHandle_t本质是Timer_t*的别名）

        // 跟踪函数进入（调试/跟踪用，记录函数调用参数）
        traceENTER_vTimerSetReloadMode( xTimer, xAutoReload );

        // 断言检查：目标定时器句柄必须有效（非NULL）
        configASSERT( xTimer );

        // 进入临界区：确保修改状态位的操作是原子的（不受其他任务或中断干扰）
        taskENTER_CRITICAL();
        {
            // 若设置为自动重载模式（xAutoReload不为pdFALSE）
            if( xAutoReload != pdFALSE )
            {
                // 在状态位（ucStatus）中置“自动重载”标志（使用按位或操作，不影响其他状态位）
                pxTimer->ucStatus |= ( uint8_t ) tmrSTATUS_IS_AUTORELOAD;
            }
            else
            {
                // 若设置为单次触发模式：清除“自动重载”标志（使用按位与+取反操作，仅清除目标位）
                pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_AUTORELOAD );
            }
        }
        // 退出临界区：恢复任务调度和中断响应
        taskEXIT_CRITICAL();

        // 跟踪函数返回（调试/跟踪用）
        traceRETURN_vTimerSetReloadMode();
    }
/*-----------------------------------------------------------*/

    BaseType_t xTimerGetReloadMode( TimerHandle_t xTimer )
    {
        Timer_t * pxTimer = xTimer;
        BaseType_t xReturn;

        traceENTER_xTimerGetReloadMode( xTimer );

        configASSERT( xTimer );
        taskENTER_CRITICAL();
        {
            if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) == 0U )
            {
                /* Not an auto-reload timer. */
                xReturn = pdFALSE;
            }
            else
            {
                /* Is an auto-reload timer. */
                xReturn = pdTRUE;
            }
        }
        taskEXIT_CRITICAL();

        traceRETURN_xTimerGetReloadMode( xReturn );

        return xReturn;
    }

    UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer )
    {
        UBaseType_t uxReturn;

        traceENTER_uxTimerGetReloadMode( xTimer );

        uxReturn = ( UBaseType_t ) xTimerGetReloadMode( xTimer );

        traceRETURN_uxTimerGetReloadMode( uxReturn );

        return uxReturn;
    }
/*-----------------------------------------------------------*/

// 函数：获取定时器的到期时间（下次超时的系统时钟节拍值）
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
    Timer_t * pxTimer = xTimer;  // 将定时器句柄转换为内部定时器结构体指针
    TickType_t xReturn;          // 存储返回的到期时间

    // 跟踪函数进入（调试/跟踪用，记录被查询的定时器句柄）
    traceENTER_xTimerGetExpiryTime( xTimer );

    // 断言检查：目标定时器句柄必须有效（非NULL）
    configASSERT( xTimer );

    // 获取定时器列表项中存储的到期时间值
    // listGET_LIST_ITEM_VALUE 是宏，用于提取ListItem_t中的xItemValue成员（存储到期节拍值）
    xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );

    // 跟踪函数返回（调试/跟踪用，记录返回的到期时间）
    traceRETURN_xTimerGetExpiryTime( xReturn );

    // 返回定时器的到期时间（单位：系统时钟节拍）
    return xReturn;
}
/*-----------------------------------------------------------*/

// 若配置支持静态内存分配（configSUPPORT_STATIC_ALLOCATION == 1）
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
        // 函数：获取静态分配定时器的内存缓冲区指针
        // 返回值：pdTRUE表示成功（定时器为静态分配），pdFALSE表示失败（定时器为动态分配）
        BaseType_t xTimerGetStaticBuffer( TimerHandle_t xTimer,
                                          StaticTimer_t ** ppxTimerBuffer )  // 输出参数：用于存储静态缓冲区指针的二级指针
        {
            BaseType_t xReturn;  // 存储返回结果
            Timer_t * pxTimer = xTimer;  // 将定时器句柄转换为内部定时器结构体指针

            // 跟踪函数进入（调试/跟踪用，记录函数调用参数）
            traceENTER_xTimerGetStaticBuffer( xTimer, ppxTimerBuffer );

            // 断言检查：输出参数指针（ppxTimerBuffer）必须有效（非NULL），否则无法返回缓冲区地址
            configASSERT( ppxTimerBuffer != NULL );

            // 检查定时器状态位：判断是否为静态分配（是否设置了tmrSTATUS_IS_STATICALLY_ALLOCATED标志）
            if( ( pxTimer->ucStatus & tmrSTATUS_IS_STATICALLY_ALLOCATED ) != 0U )
            {
                /* MISRA 规则 11.3.1 引用 [ 未对齐访问 ] */
                /* 详细说明见：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-113 */
                /* coverity[misra_c_2012_rule_11_3_violation] */  // 告知代码检查工具此处为功能必需的规则例外
                // 将内部Timer_t指针强制转换为StaticTimer_t指针（两者大小一致，由之前的断言保障），并通过输出参数返回
                *ppxTimerBuffer = ( StaticTimer_t * ) pxTimer;
                xReturn = pdTRUE;  // 标记成功：定时器为静态分配，已获取缓冲区指针
            }
            else
            {
                xReturn = pdFALSE;  // 标记失败：定时器为动态分配，无静态缓冲区
            }

            // 跟踪函数返回（调试/跟踪用，记录返回结果）
            traceRETURN_xTimerGetStaticBuffer( xReturn );

            return xReturn;  // 返回获取结果
        }
    #endif /* configSUPPORT_STATIC_ALLOCATION */  // 静态内存分配配置的条件编译结束
/*-----------------------------------------------------------*/

    const char * pcTimerGetName( TimerHandle_t xTimer )
    {
        Timer_t * pxTimer = xTimer;

        traceENTER_pcTimerGetName( xTimer );

        configASSERT( xTimer );

        traceRETURN_pcTimerGetName( pxTimer->pcTimerName );

        return pxTimer->pcTimerName;
    }
/*-----------------------------------------------------------*/

// 静态内部函数：重载定时器（用于自动重载定时器超时后重新计算到期时间并插入活动列表）
static void prvReloadTimer( Timer_t * const pxTimer,          // 要重载的定时器结构体指针
                                TickType_t xExpiredTime,       // 定时器本次到期的系统节拍值
                                const TickType_t xTimeNow )    // 当前系统节拍值
    {
        /* 将定时器插入到下一次到期时间对应的活动列表中。
         * 若下一次到期时间已过（因系统繁忙等原因导致处理延迟），
         * 则更新到期时间、调用回调函数，并重新尝试插入。 */
        // 循环调用prvInsertTimerInActiveList，直到成功将定时器插入活动列表（返回pdFALSE）
        // 插入时的新到期时间 = 本次到期时间 + 定时器周期（xTimerPeriodInTicks）
        while( prvInsertTimerInActiveList( pxTimer, ( xExpiredTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xExpiredTime ) != pdFALSE )
        {
            /* 更新到期时间：累加一个周期（处理超时情况） */
            xExpiredTime += pxTimer->xTimerPeriodInTicks;

            /* 调用定时器回调函数（通知应用层定时器超时） */
            traceTIMER_EXPIRED( pxTimer );  // 跟踪定时器超时事件（调试用）
            pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );  // 传入定时器句柄作为参数
        }
    }
/*-----------------------------------------------------------*/

// 静态内部函数：处理已到期的定时器（触发回调并根据模式决定是否重载）
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime,  // 定时器的到期时间（节拍值）
                                        const TickType_t xTimeNow )     // 当前系统节拍值
    {
        /* MISRA 规则 11.5.3 引用 [ 空指针赋值 ] */
        /* 详细说明见：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
        /* coverity[misra_c_2012_rule_11_5_violation] */  // 告知代码检查工具此处为功能必需的规则例外
        // 获取当前活动列表头部的定时器（列表按到期时间排序，头部是最早到期的定时器）
        // listGET_OWNER_OF_HEAD_ENTRY 宏返回列表项的所有者（即定时器结构体指针）
        Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* 从活动定时器列表中移除该定时器。
         * 调用此函数前已确保列表不为空（由调用者检查）。 */
        // uxListRemove 移除列表项并返回剩余节点数（返回值未使用，强制转换为void避免警告）
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );

        /* 若定时器是自动重载模式，则计算下一次到期时间并重新插入活动列表。 */
        // 检查状态位中的“自动重载”标志（tmrSTATUS_IS_AUTORELOAD）
        if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0U )
        {
            // 调用prvReloadTimer处理重载逻辑（计算新到期时间并插入列表）
            prvReloadTimer( pxTimer, xNextExpireTime, xTimeNow );
        }
        else
        {
            // 单次触发模式：清除“活动”状态位（tmrSTATUS_IS_ACTIVE），标记为已停止
            pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
        }

        /* 调用定时器回调函数（通知应用层定时器已超时） */
        traceTIMER_EXPIRED( pxTimer );  // 跟踪定时器超时事件（调试用）
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );  // 传入定时器句柄作为参数
    }
/*-----------------------------------------------------------*/

// 静态任务函数：定时器服务任务（也称为守护任务）的主函数
// 此任务负责处理定时器命令（启动/停止/重置等）和超时事件
static portTASK_FUNCTION( prvTimerTask, pvParameters )
{
    TickType_t xNextExpireTime;  // 下一个定时器的到期时间（节拍值）
    BaseType_t xListWasEmpty;    // 标记活动列表是否为空（pdTRUE=空，pdFALSE=非空）

    /* 仅用于避免编译器警告（参数未使用） */
    ( void ) pvParameters;

    // 若启用守护任务启动钩子函数（configUSE_DAEMON_TASK_STARTUP_HOOK == 1）
    #if ( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
    {
        /* 允许应用开发者在该任务开始执行时，在其上下文中运行一些代码。
         * 这在应用包含需要在调度器启动后执行的初始化代码时非常有用。 */
        vApplicationDaemonTaskStartupHook();
    }
    #endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

    // 无限循环（FreeRTOS任务的标准形式）
    for( ; configCONTROL_INFINITE_LOOP(); )
    {
        /* 查询定时器列表，判断是否包含任何定时器；若包含，获取下一个定时器的到期时间。 */
        xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

        /* 若有定时器已到期，则处理它；否则，阻塞此任务直到：
         * 1. 某个定时器到期，或
         * 2. 收到新的命令（如启动定时器）。 */
        prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

        /* 处理命令队列中所有接收到的命令（如启动、停止、重置定时器）。 */
        prvProcessReceivedCommands();
    }
}
/*-----------------------------------------------------------*/

// 静态内部函数：处理已到期的定时器或阻塞定时器服务任务
// 根据下一个到期时间和列表状态，决定是处理超时还是进入阻塞状态
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime,  // 下一个定时器的到期时间
                                            BaseType_t xListWasEmpty )        // 当前活动列表是否为空（pdTRUE=空）
    {
        TickType_t xTimeNow;                  // 当前系统节拍值
        BaseType_t xTimerListsWereSwitched;   // 标记获取当前时间时是否发生了定时器列表切换（因节拍溢出）

        // 挂起所有任务调度（进入临界区，确保时间采样和列表操作的原子性）
        vTaskSuspendAll();
        {
            /* 获取当前时间，判断定时器是否已到期。
             * 若获取时间时发生了列表切换（节拍溢出），则不处理当前定时器，
             * 因为列表切换时，原列表中剩余的定时器已在prvSampleTimeNow()中处理完毕。 */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            // 情况1：未发生列表切换（无节拍溢出）
            if( xTimerListsWereSwitched == pdFALSE )
            {
                /* 节拍计数器未溢出，检查定时器是否已到期？ */
                // 若列表非空且下一个到期时间 <= 当前时间（定时器已到期）
                if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
                {
                    // 恢复任务调度（退出临界区）
                    ( void ) xTaskResumeAll();
                    // 处理已到期的定时器（触发回调、自动重载等）
                    prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
                }
                // 若定时器未到期或列表为空
                else
                {
                    /* 节拍计数器未溢出，且下一个到期时间尚未到达。
                     * 因此，本任务应阻塞等待，直到下一个到期时间或收到命令（以先到者为准）。
                     * 除非当前定时器列表为空，否则以下代码仅在xNextExpireTime > xTimeNow时执行。 */
                    
                    if( xListWasEmpty != pdFALSE )
                    {
                        /* 当前活动列表为空，检查溢出列表是否也为空？
                         * （用于后续判断是否需要无限阻塞） */
                        xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
                    }

                    // 阻塞等待命令队列消息，超时时间为（下一个到期时间 - 当前时间）
                    // 若两个列表都为空（xListWasEmpty=pdTRUE），则超时时间为portMAX_DELAY（无限阻塞）
                    vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

                    // 恢复任务调度，返回是否有更高优先级任务就绪
                    if( xTaskResumeAll() == pdFALSE )
                    {
                        /* 主动让出CPU，等待命令到达或阻塞超时。
                         * 若在临界区退出到让出CPU期间有命令到达，则让出操作不会导致任务阻塞。 */
                        taskYIELD_WITHIN_API();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记（此分支未被执行）
                    }
                }
            }
            // 情况2：发生了列表切换（因节拍溢出）
            else
            {
                // 恢复任务调度（退出临界区），无需额外处理（溢出时的定时器已在prvSampleTimeNow中处理）
                ( void ) xTaskResumeAll();
            }
        }
    }
/*-----------------------------------------------------------*/

// 静态内部函数：获取下一个定时器的到期时间，并判断当前活动列表是否为空
// 返回值：下一个定时器的到期时间（节拍值）；若列表为空，返回0
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
    TickType_t xNextExpireTime;  // 存储下一个定时器的到期时间

    /* 定时器按到期时间排序存储，列表头部是最早到期的定时器。
     * 获取最近到期的定时器的到期时间。若没有活动定时器，
     * 则将下一个到期时间设为0，这会导致本任务在节拍计数器溢出时解除阻塞，
     * 此时时定时器列表会切换，下一个到期时间可重新评估。 */
    
    // 判断当前活动列表（pxCurrentTimerList）是否为空，结果存入输出参数
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );

    // 若列表非空（存在活动定时器）
    if( *pxListWasEmpty == pdFALSE )
    {
        // 获取列表头部节点的到期时间（即最早到期的定时器的时间）
        // listGET_ITEM_VALUE_OF_HEAD_ENTRY 宏用于提取列表头部节点的xItemValue成员
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else
    {
        /* 确保任务在节拍计数器溢出时解除阻塞（因0是最小的节拍值，必然小于等于溢出后的节拍值）。 */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    return xNextExpireTime;  // 返回下一个到期时间
}
/*-----------------------------------------------------------*/

// 静态内部函数：获取当前系统节拍时间，并检测是否发生节拍溢出（导致定时器列表切换）
// 返回值：当前系统节拍值
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )  // 输出参数：是否发生列表切换（pdTRUE=是）
    {
        TickType_t xTimeNow;  // 当前系统节拍值
        // 静态变量：保存上一次采样的节拍值（初始为0），用于检测节拍溢出
        PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U;

        // 获取当前系统节拍值（xTaskGetTickCount()返回自系统启动后的总节拍数）
        xTimeNow = xTaskGetTickCount();

        // 检查是否发生节拍溢出：当前时间 < 上一次时间（因无符号整数特性，溢出后会从最大值回到0）
        if( xTimeNow < xLastTime )
        {
            // 发生溢出：切换定时器双列表（当前列表与溢出列表交换）
            prvSwitchTimerLists();
            // 标记发生了列表切换
            *pxTimerListsWereSwitched = pdTRUE;
        }
        else
        {
            // 未发生溢出：标记未切换列表
            *pxTimerListsWereSwitched = pdFALSE;
        }

        // 更新上一次采样的节拍值，为下一次检测做准备
        xLastTime = xTimeNow;

        // 返回当前系统节拍值
        return xTimeNow;
    }
/*-----------------------------------------------------------*/

// 静态内部函数：将定时器插入活动列表，并判断是否需要立即处理超时
// 返回值：pdTRUE表示需要立即处理超时，pdFALSE表示成功插入列表等待到期
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer,          // 要插入的定时器结构体指针
                                                  const TickType_t xNextExpiryTime,  // 定时器下一次的到期时间（节拍值）
                                                  const TickType_t xTimeNow,         // 当前系统节拍值
                                                  const TickType_t xCommandTime )    // 触发插入操作的命令（如启动/重置）发出时的节拍值
    {
        BaseType_t xProcessTimerNow = pdFALSE;  // 标记是否需要立即处理超时，初始为不需要

        // 设置定时器列表项的到期时间值（用于列表排序）
        listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
        // 设置列表项的所有者（指向定时器本身，便于从列表项反向找到定时器）
        listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

        // 情况1：下一次到期时间已小于等于当前时间（理论上已超时）
        if( xNextExpiryTime <= xTimeNow )
        {
            /* 检查从“启动/重置定时器命令发出”到“命令被处理”的时间间隔内，
             * 定时器是否已经超时？ */
            // 计算命令发出到当前的时间差（考虑节拍溢出的安全计算方式）
            if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks )
            {
                /* 命令发出到处理的时间差已超过定时器周期，说明定时器已至少超时一次，
                 * 需要立即处理（由调用者触发回调）。 */
                xProcessTimerNow = pdTRUE;
            }
            else
            {
                // 时间差未超过周期：可能因系统轻微延迟导致，将定时器插入溢出列表（等待统一处理）
                vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
            }
        }
        // 情况2：下一次到期时间在当前时间之后（未超时）
        else
        {
            // 检查是否发生了节拍溢出（命令发出时的节拍 > 当前节拍，说明中间发生了溢出）
            // 且下一次到期时间 >= 命令发出时的节拍（说明到期时间在溢出前，实际已超时）
            if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
            {
                /* 若命令发出后系统节拍发生了溢出，但到期时间未溢出，
                 * 则定时器实际已超时，需要立即处理。 */
                xProcessTimerNow = pdTRUE;
            }
            else
            {
                // 正常情况：将定时器插入当前活动列表（按到期时间排序，等待到期）
                vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
            }
        }

        // 返回是否需要立即处理超时的标志
        return xProcessTimerNow;
    }
/*-----------------------------------------------------------*/

// 静态内部函数：处理定时器命令队列中所有接收到的命令（如启动、停止、重置定时器等）
static void prvProcessReceivedCommands( void )
{
    DaemonTaskMessage_t xMessage = { 0 };  // 存储从命令队列接收的消息
    Timer_t * pxTimer;                     // 指向命令对应的定时器结构体
    BaseType_t xTimerListsWereSwitched;    // 标记采样时间时是否发生列表切换（未使用，仅为函数调用参数）
    TickType_t xTimeNow;                   // 当前系统节拍值

    // 循环从命令队列（xTimerQueue）接收消息，直到队列空（超时时间为0，非阻塞）
    while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL )
    {
        // 若启用了定时器挂起函数调用功能（INCLUDE_xTimerPendFunctionCall == 1）
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
        {
            /* 负的消息ID表示是“挂起函数调用”命令，而非定时器命令。
             * （xTimerPendFunctionCall通过发送负ID消息，借助定时器队列触发函数回调） */
            if( xMessage.xMessageID < ( BaseType_t ) 0 )
            {
                // 获取消息中的回调参数结构体（存储函数指针和参数）
                const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                /* 定时器通过xCallbackParameters成员请求执行回调，需确保回调函数非空 */
                configASSERT( pxCallback );

                /* 执行挂起的回调函数，传入预设参数 */
                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记（此分支未被执行）
            }
        }
        #endif /* INCLUDE_xTimerPendFunctionCall */

        /* 正的消息ID表示是定时器命令（如启动、停止、修改周期等） */
        if( xMessage.xMessageID >= ( BaseType_t ) 0 )
        {
            // 从消息中获取定时器参数，指向目标定时器结构体
            pxTimer = xMessage.u.xTimerParameters.pxTimer;

            // 检查定时器是否已在某个列表中（若在，则先移除，避免重复管理）
            if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
            {
                /* 定时器在列表中，将其从列表移除（返回值未使用，强制转换为void避免警告） */
                ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记（此分支未被执行）
            }

            // 跟踪命令接收事件（调试用，记录定时器、命令ID和命令值）
            traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

            /* 此处xTimerListsWereSwitched参数未使用，但prvSampleTimeNow需传入该参数。
             * 必须在接收消息后采样时间，避免高优先级任务在采样后、处理前修改时间导致偏差 */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            // 根据消息ID（命令类型）执行对应操作
            switch( xMessage.xMessageID )
            {
                // 命令1：启动定时器（从任务或中断中发送）
                case tmrCOMMAND_START:
                case tmrCOMMAND_START_FROM_ISR:
                // 命令2：重置定时器（从任务或中断中发送）
                case tmrCOMMAND_RESET:
                case tmrCOMMAND_RESET_FROM_ISR:
                    /* 启动/重置定时器：标记为活动状态，计算新到期时间并插入活动列表 */
                    pxTimer->ucStatus |= ( uint8_t ) tmrSTATUS_IS_ACTIVE;

                    // 调用prvInsertTimerInActiveList插入列表，判断是否需要立即处理超时
                    // 新到期时间 = 命令发送时的时间（xMessageValue） + 定时器周期
                    if( prvInsertTimerInActiveList( pxTimer, 
                                                   xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, 
                                                   xTimeNow, 
                                                   xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
                    {
                        /* 插入失败：新到期时间已过，需立即处理超时 */
                        if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0U )
                        {
                            // 自动重载模式：调用prvReloadTimer处理超时和重新插入
                            prvReloadTimer( pxTimer, 
                                           xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, 
                                           xTimeNow );
                        }
                        else
                        {
                            // 单次触发模式：清除活动状态（处理后停止）
                            pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
                        }

                        /* 触发定时器回调函数（通知应用层超时） */
                        traceTIMER_EXPIRED( pxTimer );
                        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();  // 插入成功，无需额外处理
                    }
                    break;

                // 命令3：停止定时器（从任务或中断中发送）
                case tmrCOMMAND_STOP:
                case tmrCOMMAND_STOP_FROM_ISR:
                    /* 定时器已从列表中移除（前面的代码），此处仅需清除活动状态 */
                    pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
                    break;

                // 命令4：修改定时器周期（从任务或中断中发送）
                case tmrCOMMAND_CHANGE_PERIOD:
                case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR:
                    /* 修改周期：标记为活动状态，更新周期值，重新插入活动列表 */
                    pxTimer->ucStatus |= ( uint8_t ) tmrSTATUS_IS_ACTIVE;
                    pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
                    configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );  // 确保周期非0

                    /* 新周期无参考时间（可长可短），命令时间设为当前时间。
                     * 因周期非0，新到期时间必在未来，无需处理插入失败（无pdTRUE分支） */
                    ( void ) prvInsertTimerInActiveList( pxTimer, 
                                                       ( xTimeNow + pxTimer->xTimerPeriodInTicks ), 
                                                       xTimeNow, 
                                                       xTimeNow );
                    break;

                // 命令5：删除定时器
                case tmrCOMMAND_DELETE:
                    #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
                    {
                        /* 定时器已从列表中移除，若为动态分配，释放内存；否则仅清除活动状态 */
                        if( ( pxTimer->ucStatus & tmrSTATUS_IS_STATICALLY_ALLOCATED ) == ( uint8_t ) 0 )
                        {
                            // 动态分配：释放定时器结构体内存
                            vPortFree( pxTimer );
                        }
                        else
                        {
                            // 静态分配：清除活动状态（内存由应用层管理）
                            pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
                        }
                    }
                    #else /* configSUPPORT_DYNAMIC_ALLOCATION == 0 */
                    {
                        /* 未启用动态分配，内存不可能是动态的，仅清除活动状态 */
                        pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
                    }
                    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
                    break;

                // 默认情况：不处理未知命令
                default:
                    /* 不应进入此分支 */
                    break;
            }
        }
    }
}
/*-----------------------------------------------------------*/

// 静态内部函数：切换定时器双列表（当前列表与溢出列表），仅在系统节拍溢出时调用
static void prvSwitchTimerLists( void )
    {
        TickType_t xNextExpireTime;  // 当前列表中最早到期的定时器的到期时间
        List_t * pxTemp;             // 临时指针，用于交换双列表

        /* 系统节拍已溢出，必须切换定时器列表。
         * 若当前定时器列表中仍有定时器引用，则这些定时器必然已到期，
         * 需在切换列表前处理完毕（避免溢出后时间判断混乱）。 */
        
        // 循环处理当前列表中所有剩余的定时器（直到列表为空）
        while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
        {
            // 获取当前列表头部节点的到期时间（最早到期的定时器）
            xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

            /* 处理已到期的定时器。对于自动重载定时器，需注意：
             * 仅处理当前列表中已到期的事件，后续的到期事件需等待列表切换后再处理（避免跨列表时间冲突）。 */
            // tmrMAX_TIME_BEFORE_OVERFLOW 是节拍溢出前的最大值（如0xFFFFFFFF），作为当前时间传入
            prvProcessExpiredTimer( xNextExpireTime, tmrMAX_TIME_BEFORE_OVERFLOW );
        }

        // 交换当前列表（pxCurrentTimerList）和溢出列表（pxOverflowTimerList）的指针
        pxTemp = pxCurrentTimerList;
        pxCurrentTimerList = pxOverflowTimerList;
        pxOverflowTimerList = pxTemp;
    }
/*-----------------------------------------------------------*/

// 静态函数：检查并初始化定时器所需的列表和队列
static void prvCheckForValidListAndQueue( void )
{
    /* 检查用于引用活动定时器的列表以及用于与定时器服务通信的队列是否已初始化。 */
    taskENTER_CRITICAL();  // 进入临界区（禁止任务调度和中断）
    {
        // 如果定时器队列尚未初始化
        if( xTimerQueue == NULL )
        {
            // 初始化两个活动定时器列表
            vListInitialise( &xActiveTimerList1 );
            vListInitialise( &xActiveTimerList2 );
            
            // 设置当前定时器列表和溢出定时器列表的初始指向
            pxCurrentTimerList = &xActiveTimerList1;
            pxOverflowTimerList = &xActiveTimerList2;

            // 如果支持静态内存分配
            #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                /* 为防止configSUPPORT_DYNAMIC_ALLOCATION为0的情况，定时器队列采用静态分配。 */
                PRIVILEGED_DATA static StaticQueue_t xStaticTimerQueue;  // 静态队列结构体
                // 静态队列存储区，大小为队列长度乘以每个消息的大小
                PRIVILEGED_DATA static uint8_t ucStaticTimerQueueStorage[ ( size_t ) configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ];

                // 创建静态队列
                xTimerQueue = xQueueCreateStatic( 
                    ( UBaseType_t ) configTIMER_QUEUE_LENGTH,  // 队列长度
                    ( UBaseType_t ) sizeof( DaemonTaskMessage_t ),  // 每个消息的大小
                    &( ucStaticTimerQueueStorage[ 0 ] ),  // 队列存储区
                    &xStaticTimerQueue  // 静态队列结构体
                );
            }
            #else  // 不支持静态内存分配，使用动态分配
            {
                // 创建动态队列
                xTimerQueue = xQueueCreate( 
                    ( UBaseType_t ) configTIMER_QUEUE_LENGTH,  // 队列长度
                    ( UBaseType_t ) sizeof( DaemonTaskMessage_t )  // 每个消息的大小
                );
            }
            #endif /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */

            // 如果配置了队列注册表且队列创建成功
            #if ( configQUEUE_REGISTRY_SIZE > 0 )
            {
                if( xTimerQueue != NULL )
                {
                    // 将定时器队列添加到注册表，名称为"TmrQ"
                    vQueueAddToRegistry( xTimerQueue, "TmrQ" );
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记
                }
            }
            #endif /* configQUEUE_REGISTRY_SIZE */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  // 测试覆盖率标记（队列已初始化的情况）
        }
    }
    taskEXIT_CRITICAL();  // 退出临界区（恢复任务调度和中断）
}
/*-----------------------------------------------------------*/

    BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer )
    {
        BaseType_t xReturn;
        Timer_t * pxTimer = xTimer;

        traceENTER_xTimerIsTimerActive( xTimer );

        configASSERT( xTimer );

        /* Is the timer in the list of active timers? */
        taskENTER_CRITICAL();
        {
            if( ( pxTimer->ucStatus & tmrSTATUS_IS_ACTIVE ) == 0U )
            {
                xReturn = pdFALSE;
            }
            else
            {
                xReturn = pdTRUE;
            }
        }
        taskEXIT_CRITICAL();

        traceRETURN_xTimerIsTimerActive( xReturn );

        return xReturn;
    }
/*-----------------------------------------------------------*/

    void * pvTimerGetTimerID( const TimerHandle_t xTimer )
    {
        Timer_t * const pxTimer = xTimer;
        void * pvReturn;

        traceENTER_pvTimerGetTimerID( xTimer );

        configASSERT( xTimer );

        taskENTER_CRITICAL();
        {
            pvReturn = pxTimer->pvTimerID;
        }
        taskEXIT_CRITICAL();

        traceRETURN_pvTimerGetTimerID( pvReturn );

        return pvReturn;
    }
/*-----------------------------------------------------------*/

    void vTimerSetTimerID( TimerHandle_t xTimer,
                           void * pvNewID )
    {
        Timer_t * const pxTimer = xTimer;

        traceENTER_vTimerSetTimerID( xTimer, pvNewID );

        configASSERT( xTimer );

        taskENTER_CRITICAL();
        {
            pxTimer->pvTimerID = pvNewID;
        }
        taskEXIT_CRITICAL();

        traceRETURN_vTimerSetTimerID();
    }
/*-----------------------------------------------------------*/

    #if ( INCLUDE_xTimerPendFunctionCall == 1 )

        BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend,
                                                  void * pvParameter1,
                                                  uint32_t ulParameter2,
                                                  BaseType_t * pxHigherPriorityTaskWoken )
        {
            DaemonTaskMessage_t xMessage;
            BaseType_t xReturn;

            traceENTER_xTimerPendFunctionCallFromISR( xFunctionToPend, pvParameter1, ulParameter2, pxHigherPriorityTaskWoken );

            /* Complete the message with the function parameters and post it to the
             * daemon task. */
            xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR;
            xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
            xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
            xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

            xReturn = xQueueSendFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );

            tracePEND_FUNC_CALL_FROM_ISR( xFunctionToPend, pvParameter1, ulParameter2, xReturn );
            traceRETURN_xTimerPendFunctionCallFromISR( xReturn );

            return xReturn;
        }

    #endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

    #if ( INCLUDE_xTimerPendFunctionCall == 1 )

        BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend,
                                           void * pvParameter1,
                                           uint32_t ulParameter2,
                                           TickType_t xTicksToWait )
        {
            DaemonTaskMessage_t xMessage;
            BaseType_t xReturn;

            traceENTER_xTimerPendFunctionCall( xFunctionToPend, pvParameter1, ulParameter2, xTicksToWait );

            /* This function can only be called after a timer has been created or
             * after the scheduler has been started because, until then, the timer
             * queue does not exist. */
            configASSERT( xTimerQueue );

            /* Complete the message with the function parameters and post it to the
             * daemon task. */
            xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK;
            xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
            xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
            xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

            xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );

            tracePEND_FUNC_CALL( xFunctionToPend, pvParameter1, ulParameter2, xReturn );
            traceRETURN_xTimerPendFunctionCall( xReturn );

            return xReturn;
        }

    #endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

    #if ( configUSE_TRACE_FACILITY == 1 )

        UBaseType_t uxTimerGetTimerNumber( TimerHandle_t xTimer )
        {
            traceENTER_uxTimerGetTimerNumber( xTimer );

            traceRETURN_uxTimerGetTimerNumber( ( ( Timer_t * ) xTimer )->uxTimerNumber );

            return ( ( Timer_t * ) xTimer )->uxTimerNumber;
        }

    #endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

    #if ( configUSE_TRACE_FACILITY == 1 )

        void vTimerSetTimerNumber( TimerHandle_t xTimer,
                                   UBaseType_t uxTimerNumber )
        {
            traceENTER_vTimerSetTimerNumber( xTimer, uxTimerNumber );

            ( ( Timer_t * ) xTimer )->uxTimerNumber = uxTimerNumber;

            traceRETURN_vTimerSetTimerNumber();
        }

    #endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/*
 * 重置本文件中的全局状态。这些状态通常在系统启动时初始化。
 * 应用程序在重新启动调度器（scheduler）之前，必须调用此函数。
 */
void vTimerResetState( void )
{
    // 将定时器命令队列句柄置空（表示队列未创建或已重置）
    xTimerQueue = NULL;
    // 将定时器服务任务句柄置空（表示任务未创建或已重置）
    xTimerTaskHandle = NULL;
}
/*-----------------------------------------------------------*/

/* This entire source file will be skipped if the application is not configured
 * to include software timer functionality.  If you want to include software timer
 * functionality then ensure configUSE_TIMERS is set to 1 in FreeRTOSConfig.h. */
#endif /* configUSE_TIMERS == 1 */
