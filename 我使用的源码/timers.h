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


#ifndef TIMERS_H
#define TIMERS_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h must appear in source files before include timers.h"
#endif

#include "task.h"


/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

/*-----------------------------------------------------------
* MACROS AND DEFINITIONS
*----------------------------------------------------------*/

/* 可在定时器队列上发送/接收的命令ID。这些ID应仅通过构成公共软件定时器API的宏来使用，
 * 如下文所定义。从中断中发送的命令必须使用最大的编号，因为tmrFIRST_FROM_ISR_COMMAND用于
 * 用于确定应使用队列发送函数的任务版本还是中断中断版本。 */
#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR    ( ( BaseType_t ) -2 ) //从中断中触发执行回调函数
#define tmrCOMMAND_EXECUTE_CALLBACK             ( ( BaseType_t ) -1 ) //触发执行回调函数
#define tmrCOMMAND_START_DONT_TRACE             ( ( BaseType_t ) 0 )  //启动定时器，但不进行跟踪
#define tmrCOMMAND_START                        ( ( BaseType_t ) 1 )  //启动定时器
#define tmrCOMMAND_RESET                        ( ( BaseType_t ) 2 )  //重置定时器
#define tmrCOMMAND_STOP                         ( ( BaseType_t ) 3 )  //停止定时器
#define tmrCOMMAND_CHANGE_PERIOD                ( ( BaseType_t ) 4 )  //更改定时器周期
#define tmrCOMMAND_DELETE                       ( ( BaseType_t ) 5 )  //删除定时器

#define tmrFIRST_FROM_ISR_COMMAND               ( ( BaseType_t ) 6 )  //从中断中触发的第一个命令ID
#define tmrCOMMAND_START_FROM_ISR               ( ( BaseType_t ) 6 )  //从中断中触发启动定时器
#define tmrCOMMAND_RESET_FROM_ISR               ( ( BaseType_t ) 7 )  //从中断中触发重置定时器
#define tmrCOMMAND_STOP_FROM_ISR                ( ( BaseType_t ) 8 )  //从中断中触发停止定时器
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR       ( ( BaseType_t ) 9 )  //从中断中触发更改定时器周期


/**
 * 软件定时器的引用类型。例如，调用 xTimerCreate() 会返回一个 TimerHandle_t 变量，
 * 该变量可用于在其他软件定时器 API 函数（如 xTimerStart()、xTimerReset() 等）的调用中引用相应的定时器。
 */
struct tmrTimerControl; /* 使用旧的命名约定是为了避免破坏内核感知调试器 */
typedef struct tmrTimerControl * TimerHandle_t;

/*
 * 定义定时器回调函数必须遵循的函数原型。
 */
typedef void (* TimerCallbackFunction_t)( TimerHandle_t xTimer );

/*
 * 定义与 xTimerPendFunctionCallFromISR() 函数配合使用的函数必须遵循的函数原型。
 */
typedef void (* PendedFunction_t)( void * arg1,
                                   uint32_t arg2 );

/**
 * TimerHandle_t xTimerCreate(  const char * const pcTimerName,
 *                              TickType_t xTimerPeriodInTicks,
 *                              BaseType_t xAutoReload,
 *                              void * pvTimerID,
 *                              TimerCallbackFunction_t pxCallbackFunction );
 *
 * 创建一个新的软件定时器实例，并返回一个句柄，通过该句柄可引用所创建的软件定时器。
 *
 * 在 FreeRTOS 实现内部，软件定时器需要一块内存来存储定时器数据结构。若使用 xTimerCreate() 创建软件定时器，
 * 所需内存会在 xTimerCreate() 函数内部自动从动态堆中分配（参考：https://www.FreeRTOS.org/a00111.html）。
 * 若使用 xTimerCreateStatic() 创建软件定时器，则需由应用程序开发者自行提供该软件定时器所需的内存，
 * 因此 xTimerCreateStatic() 可在不使用任何动态内存分配的情况下创建软件定时器。
 *
 * 定时器创建后处于休眠状态（dormant state）。可通过 xTimerStart()、xTimerReset()、xTimerStartFromISR()、
 * xTimerResetFromISR()、xTimerChangePeriod() 和 xTimerChangePeriodFromISR() 等 API 函数，
 * 将定时器切换到激活状态。
 *
 * @param pcTimerName 分配给定时器的文本名称，仅用于辅助调试。内核仅通过定时器句柄引用定时器，从不通过名称引用。
 *
 * @param xTimerPeriodInTicks 定时器周期，单位为系统节拍数（tick）。可使用常量 portTICK_PERIOD_MS 将以毫秒为单位的时间
 * 转换为节拍数。例如：若定时器需在 100 个节拍后过期，xTimerPeriodInTicks 应设为 100；若定时器需在 500ms 后过期，
 * 且 configTICK_RATE_HZ（系统节拍频率）小于等于 1000，则 xTimerPeriodInTicks 可设为 (500 / portTICK_PERIOD_MS)。
 * 定时器周期必须大于 0。
 *
 * @param xAutoReload 若设为 pdTRUE，定时器将以 xTimerPeriodInTicks 设定的频率重复过期；
 * 若设为 pdFALSE，定时器为单次模式（one-shot timer），过期后自动进入休眠状态。
 *
 * @param pvTimerID 分配给所创建定时器的标识符。当多个定时器共用同一个回调函数时，通常通过该标识符在回调函数中区分
 * 哪个定时器已过期。
 *
 * @param pxCallbackFunction 定时器过期时调用的函数。回调函数必须符合 TimerCallbackFunction_t 定义的原型，
 * 即 “void vCallbackFunction( TimerHandle_t xTimer );”。
 *
 * @return 若定时器创建成功，返回指向新创建定时器的句柄；若因 FreeRTOS 堆内存不足，无法分配定时器数据结构，
 * 则返回 NULL。
 *
 * 示例用法：
 * @verbatim
 * #define NUM_TIMERS 5
 *
 * // 用于存储所创建定时器句柄的数组
 * TimerHandle_t xTimers[ NUM_TIMERS ];
 *
 * // 用于记录每个定时器过期次数的数组
 * int32_t lExpireCounters[ NUM_TIMERS ] = { 0 };
 *
 * // 定义一个供多个定时器实例共用的回调函数
 * // 该回调函数仅统计关联定时器的过期次数，当定时器过期 10 次后停止该定时器
 * void vTimerCallback( TimerHandle_t pxTimer )
 * {
 * int32_t lArrayIndex;
 * const int32_t xMaxExpiryCountBeforeStopping = 10;
 *
 *     // （可选）若 pxTimer 参数为 NULL，可执行相应处理
 *     configASSERT( pxTimer );
 *
 *     // 确定哪个定时器已过期
 *     lArrayIndex = ( int32_t ) pvTimerGetTimerID( pxTimer );
 *
 *     // 递增 pxTimer 对应的过期次数
 *     lExpireCounters[ lArrayIndex ] += 1;
 *
 *     // 若定时器已过期 10 次，则停止该定时器
 *     if( lExpireCounters[ lArrayIndex ] == xMaxExpiryCountBeforeStopping )
 *     {
 *         // 从定时器回调函数中调用定时器 API 函数时，不要指定阻塞时间，否则可能导致死锁！
 *         xTimerStop( pxTimer, 0 );
 *     }
 * }
 *
 * void main( void )
 * {
 * int32_t x;
 *
 *     // 创建并启动多个定时器。在调度器启动前启动定时器，意味着调度器一启动，这些定时器就会立即开始运行
 *     for( x = 0; x < NUM_TIMERS; x++ )
 *     {
 *         xTimers[ x ] = xTimerCreate(    "Timer",             // Just a text name, not used by the kernel.
 *                                         ( 100 * ( x + 1 ) ), // The timer period in ticks.
 *                                         pdTRUE,              // The timers will auto-reload themselves when they expire.
 *                                         ( void * ) x,        // Assign each timer a unique id equal to its array index.
 *                                         vTimerCallback       // Each timer calls the same callback when it expires.
 *                                     );
 *
 *         if( xTimers[ x ] == NULL )
 *         {
 *             // 定时器创建失败
 *         }
 *         else
 *         {
 *             // 启动定时器。此处不指定阻塞时间（即使指定，因调度器尚未启动，也会被忽略）
 *             if( xTimerStart( xTimers[ x ], 0 ) != pdPASS )
 *             {
 *                 // 定时器无法切换到激活状态
 *             }
 *         }
 *     }
 *
 *     // ...
 *     // 此处创建任务
 *     // ...
 *
 *     // 启动调度器后，已处于激活状态的定时器将开始运行
 *     vTaskStartScheduler();
 *
 *     // 正常情况下不会执行到此处
 *     for( ;; );
 * }
 * @endverbatim
 */
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    TimerHandle_t xTimerCreate( const char * const pcTimerName,
                                const TickType_t xTimerPeriodInTicks,
                                const BaseType_t xAutoReload,
                                void * const pvTimerID,
                                TimerCallbackFunction_t pxCallbackFunction ) PRIVILEGED_FUNCTION;
#endif

/**
 * TimerHandle_t xTimerCreateStatic(const char * const pcTimerName,
 *                                  TickType_t xTimerPeriodInTicks,
 *                                  BaseType_t xAutoReload,
 *                                  void * pvTimerID,
 *                                  TimerCallbackFunction_t pxCallbackFunction,
 *                                  StaticTimer_t *pxTimerBuffer );
 *
 * 创建一个新的软件定时器实例，并返回一个句柄，通过该句柄可引用所创建的软件定时器。
 *
 * 在 FreeRTOS 实现内部，软件定时器需要一块内存来存储定时器数据结构。若使用 xTimerCreate() 创建软件定时器，
 * 所需内存会在 xTimerCreate() 函数内部自动从动态堆中分配（参考：https://www.FreeRTOS.org/a00111.html）。
 * 若使用 xTimerCreateStatic() 创建软件定时器，则需由应用程序开发者自行提供该软件定时器所需的内存，
 * 因此 xTimerCreateStatic() 可在不使用任何动态内存分配的情况下创建软件定时器。
 *
 * 定时器创建后处于休眠状态（dormant state）。可通过 xTimerStart()、xTimerReset()、xTimerStartFromISR()、
 * xTimerResetFromISR()、xTimerChangePeriod() 和 xTimerChangePeriodFromISR() 等 API 函数，
 * 将定时器切换到激活状态。
 *
 * @param pcTimerName 分配给定时器的文本名称，仅用于辅助调试。内核仅通过定时器句柄引用定时器，从不通过名称引用。
 *
 * @param xTimerPeriodInTicks 定时器周期，单位为系统节拍数（tick）。可使用常量 portTICK_PERIOD_MS 将以毫秒为单位的时间
 * 转换为节拍数。例如：若定时器需在 100 个节拍后过期，xTimerPeriodInTicks 应设为 100；若定时器需在 500ms 后过期，
 * 且 configTICK_RATE_HZ（系统节拍频率）小于等于 1000，则 xTimerPeriodInTicks 可设为 (500 / portTICK_PERIOD_MS)。
 * 定时器周期必须大于 0。
 *
 * @param xAutoReload 若设为 pdTRUE，定时器将以 xTimerPeriodInTicks 设定的频率重复过期；
 * 若设为 pdFALSE，定时器为单次模式（one-shot timer），过期后自动进入休眠状态。
 *
 * @param pvTimerID 分配给所创建定时器的标识符。当多个定时器共用同一个回调函数时，通常通过该标识符在回调函数中区分
 * 哪个定时器已过期。
 *
 * @param pxCallbackFunction 定时器过期时调用的函数。回调函数必须符合 TimerCallbackFunction_t 定义的原型，
 * 即 “void vCallbackFunction( TimerHandle_t xTimer );”。
 *
 * @param pxTimerBuffer 必须指向一个 StaticTimer_t 类型的变量，该变量将用于存储软件定时器的数据结构，
 * 从而避免动态内存分配的需求。
 *
 * @return 若定时器创建成功，返回指向新创建定时器的句柄；若 pxTimerBuffer 为 NULL，则返回 NULL。
 *
 * 示例用法：
 * @verbatim
 *
 * // 用于存储软件定时器数据结构的缓冲区
 * static StaticTimer_t xTimerBuffer;
 *
 * // 由软件定时器回调函数递增的变量
 * UBaseType_t uxVariableToIncrement = 0;
 *
 * // 软件定时器回调函数：递增创建定时器时传入的变量，递增 5 次后停止定时器
 * static void prvTimerCallback( TimerHandle_t xExpiredTimer )
 * {
 *     UBaseType_t *puxVariableToIncrement;
 *     BaseType_t xReturned;
 *
 *     // 从定时器 ID 中获取要递增的变量地址
 *     puxVariableToIncrement = ( UBaseType_t * ) pvTimerGetTimerID( xExpiredTimer );
 *
 *     // 递增变量，以标识回调函数已执行
 *     ( *puxVariableToIncrement )++;
 *
 *     // 若回调函数已执行指定次数（5 次），则停止定时器
 *     if( *puxVariableToIncrement == 5 )
 *     {
 *         // 从定时器回调中调用 API，不可阻塞，因此阻塞时间设为 staticDONT_BLOCK
 *         xTimerStop( xExpiredTimer, staticDONT_BLOCK );
 *     }
 * }
 *
 *
 * void main( void )
 * {
 *     // 创建软件定时器。xTimerCreateStatic() 比 xTimerCreate() 多一个参数：
 *     // 指向 StaticTimer_t 结构体的指针（用于存储定时器数据结构）。
 *     // 若该参数为 NULL，则会像 xTimerCreate() 一样动态分配内存。
 *     xTimer = xTimerCreateStatic( "T1",             // 定时器文本名称，仅用于调试，FreeRTOS 不使用
 *                                  xTimerPeriod,     // 定时器周期（单位：节拍）
 *                                  pdTRUE,           // 自动重载模式
 *                                  ( void * ) &uxVariableToIncrement,    // 回调函数要递增的变量
 *                                  prvTimerCallback, // 定时器过期时执行的函数
 *                                  &xTimerBuffer );  // 存储定时器数据结构的缓冲区
 *
 *     // 调度器尚未启动，因此不使用阻塞时间
 *     xReturned = xTimerStart( xTimer, 0 );
 *
 *     // ...
 *     // 此处创建任务
 *     // ...
 *
 *     // 启动调度器后，已处于激活状态的定时器将开始运行
 *     vTaskStartScheduler();
 *
 *     // 正常情况下不会执行到此处
 *     for( ;; );
 * }
 * @endverbatim
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    TimerHandle_t xTimerCreateStatic( const char * const pcTimerName,
                                      const TickType_t xTimerPeriodInTicks,
                                      const BaseType_t xAutoReload,
                                      void * const pvTimerID,
                                      TimerCallbackFunction_t pxCallbackFunction,
                                      StaticTimer_t * pxTimerBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * void *pvTimerGetTimerID( TimerHandle_t xTimer );
 *
 * 返回分配给定时器的ID。
 *
 * 定时器的ID可通过两种方式设置：一是创建定时器时调用 xTimerCreate()（或 xTimerCreateStatic()）的 pvTimerID 参数；
 * 二是调用 vTimerSetTimerID() API 函数动态修改。
 *
 * 若多个定时器共用同一个回调函数，定时器ID可作为“定时器专属存储”（类似局部变量），用于在回调中区分不同定时器或传递自定义数据。
 *
 * @param xTimer 要查询的定时器（句柄）。
 *
 * @return 分配给目标定时器的ID（与设置时的类型一致，需根据实际场景强制转换为对应类型使用）。
 *
 * 示例用法：
 *
 * 参考 xTimerCreate() API 函数的示例场景（如下为简化示例）。
 */
void * pvTimerGetTimerID( const TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID );
 *
 * 为定时器设置新的ID。
 *
 * 定时器的初始ID通过创建定时器时调用 xTimerCreate()（或 xTimerCreateStatic()）的 pvTimerID 参数分配。
 *
 * 若多个定时器共用同一个回调函数，定时器ID可作为“定时器专属存储”（类似局部变量），用于区分不同定时器或传递自定义数据。
 *
 * @param xTimer 要更新ID的定时器（句柄）。
 *
 * @param pvNewID 要分配给定时器的新ID（可为任意类型的指针，需在使用时强制转换为对应类型）。
 *
 * 示例用法：
 *
 * 参考 xTimerCreate() API 函数的示例场景（如下为补充动态修改ID的示例）。
 */
void vTimerSetTimerID( TimerHandle_t xTimer,
                       void * pvNewID ) PRIVILEGED_FUNCTION;

/**
 * BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer );
 *
 * 查询定时器当前处于激活状态（active）还是休眠状态（dormant）。
 *
 * 定时器在以下情况为休眠状态：
 *     1) 已创建但未启动；
 *     2) 是单次模式（one-shot）定时器，且已过期但未重新启动。
 *
 * 定时器创建后默认处于休眠状态。可通过 xTimerStart()、xTimerReset()、xTimerStartFromISR()、
 * xTimerResetFromISR()、xTimerChangePeriod() 和 xTimerChangePeriodFromISR() 等 API 函数，
 * 将定时器切换到激活状态。
 *
 * @param xTimer 要查询的定时器（句柄）。
 *
 * @return 若定时器处于休眠状态，返回 pdFALSE；若处于激活状态，返回非 pdFALSE 的值（通常为 pdTRUE）。
 *
 * 示例用法：
 * @verbatim
 * // 此函数假设 xTimer 已创建
 * void vAFunction( TimerHandle_t xTimer )
 * {
 *     if( xTimerIsTimerActive( xTimer ) != pdFALSE ) // 或更简洁的写法：if( xTimerIsTimerActive( xTimer ) )
 *     {
 *         // xTimer 处于激活状态，执行对应逻辑
 *     }
 *     else
 *     {
 *         // xTimer 处于休眠状态，执行其他逻辑
 *     }
 * }
 * @endverbatim
 */
BaseType_t xTimerIsTimerActive( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * TaskHandle_t xTimerGetTimerDaemonTaskHandle( void );
 *
 * 直接返回定时器服务任务（timer service/daemon task）的句柄。
 * 注意：在调度器（scheduler）启动前调用 xTimerGetTimerDaemonTaskHandle() 是无效的。
 */
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void ) PRIVILEGED_FUNCTION;

/**
 * BaseType_t xTimerStart( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 定时器功能由定时器服务任务（timer service/daemon task）提供。许多 FreeRTOS 公共定时器 API 函数会通过
 * 一个名为“定时器命令队列”（timer command queue）的队列，向定时器服务任务发送命令。该队列是内核私有队列，
 * 应用代码无法直接访问。定时器命令队列的长度由配置常量 configTIMER_QUEUE_LENGTH 设定。
 *
 * xTimerStart() 用于启动一个此前通过 xTimerCreate() API 函数创建的定时器。若定时器已处于激活状态且已启动，
 * 则 xTimerStart() 的功能与 xTimerReset() API 函数等效（即重置定时器，重新开始计时）。
 *
 * 启动定时器会确保定时器进入激活状态。若在此期间未停止、删除或重置定时器，
 * 则定时器关联的回调函数会在 xTimerStart() 调用后 'n' 个节拍触发，其中 'n' 是定时器定义的周期。
 *
 * 在调度器启动前调用 xTimerStart() 是合法的，但此时定时器不会实际启动，需等到调度器启动后才开始运行；
 * 且定时器的过期时间是相对于“调度器启动时间”计算的，而非“xTimerStart() 调用时间”。
 *
 * 需将配置常量 configUSE_TIMERS 设为 1，xTimerStart() 才会生效。
 *
 * @param xTimer 要启动/重启的定时器句柄。
 *
 * @param xTicksToWait 若调用 xTimerStart() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数。
 * 若在调度器启动前调用 xTimerStart()，此参数会被忽略。
 *
 * @return 若等待 xTicksToWait 节拍后仍无法将启动命令发送到定时器命令队列，返回 pdFAIL；
 * 若命令成功发送到队列，返回 pdPASS。命令的实际处理时间取决于定时器服务任务的优先级（相对于系统中其他任务），
 * 但定时器的过期时间是相对于“xTimerStart() 实际执行时间”计算的。定时器服务任务的优先级由配置常量
 * configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 *
 * 参考 xTimerCreate() API 函数的示例场景。
 *
 */
#define xTimerStart( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerStop( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 定时器功能由定时器服务任务（timer service/daemon task）提供。许多 FreeRTOS 公共定时器 API 函数会通过
 * 一个名为“定时器命令队列”（timer command queue）的队列，向定时器服务任务发送命令。该队列是内核私有队列，
 * 应用代码无法直接访问。定时器命令队列的长度由配置常量 configTIMER_QUEUE_LENGTH 设定。
 *
 * xTimerStop() 用于停止一个此前通过 xTimerStart()、xTimerReset()、xTimerStartFromISR()、
 * xTimerResetFromISR()、xTimerChangePeriod() 或 xTimerChangePeriodFromISR() 等 API 启动的定时器。
 *
 * 停止定时器会确保定时器退出激活状态，进入休眠态（dormant state），不会再因周期到达触发回调（除非再次启动）。
 *
 * 需将配置常量 configUSE_TIMERS 设为 1，xTimerStop() 才会生效。
 *
 * @param xTimer 要停止的定时器句柄。
 *
 * @param xTicksToWait 若调用 xTimerStop() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数。
 * 若在调度器启动前调用 xTimerStop()，此参数会被忽略。
 *
 * @return 若等待 xTicksToWait 节拍后仍无法将停止命令发送到定时器命令队列，返回 pdFAIL；
 * 若命令成功发送到队列，返回 pdPASS。命令的实际处理时间取决于定时器服务任务的优先级（相对于系统中其他任务），
 * 定时器服务任务的优先级由配置常量 configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 *
 * 参考 xTimerCreate() API 函数的示例场景。
 *
 */
#define xTimerStop( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP, 0U, NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerChangePeriod(   TimerHandle_t xTimer,
 *                                  TickType_t xNewPeriod,
 *                                  TickType_t xTicksToWait );
 *
 * 定时器功能由定时器服务任务（timer service/daemon task）提供。许多 FreeRTOS 公共定时器 API 函数会通过
 * 一个名为“定时器命令队列”（timer command queue）的队列，向定时器服务任务发送命令。该队列是内核私有队列，
 * 应用代码无法直接访问。定时器命令队列的长度由配置常量 configTIMER_QUEUE_LENGTH 设定。
 *
 * xTimerChangePeriod() 用于修改此前通过 xTimerCreate() API 函数创建的定时器的周期。
 *
 * xTimerChangePeriod() 可对处于激活态（active）或休眠态（dormant）的定时器调用：
 * - 若定时器已激活：修改周期后，定时器会以新周期重新开始计时（等效于“修改周期+重置”）；
 * - 若定时器未激活：修改周期后，定时器仍处于休眠态，需调用 xTimerStart() 启动。
 *
 * 需将配置常量 configUSE_TIMERS 设为 1，xTimerChangePeriod() 才会生效。
 *
 * @param xTimer 要修改周期的定时器句柄。
 *
 * @param xNewPeriod 定时器的新周期，单位为系统节拍数（tick）。可使用常量 portTICK_PERIOD_MS 将毫秒时间转换为节拍数，
 * 例如：500ms 周期可表示为 (500 / portTICK_PERIOD_MS)（需确保 configTICK_RATE_HZ ≤ 1000）。新周期必须大于 0。
 *
 * @param xTicksToWait 若调用 xTimerChangePeriod() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数。
 * 若在调度器启动前调用 xTimerChangePeriod()，此参数会被忽略。
 *
 * @return 若等待 xTicksToWait 节拍后仍无法将修改周期命令发送到定时器命令队列，返回 pdFAIL；
 * 若命令成功发送到队列，返回 pdPASS。命令的实际处理时间取决于定时器服务任务的优先级（相对于系统中其他任务），
 * 定时器服务任务的优先级由配置常量 configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 * @verbatim
 * // 此函数假设 xTimer 已创建。若 xTimer 指向的定时器处于激活态，则删除该定时器；
 * // 若处于休眠态，则将其周期设为 500ms 并启动。
 * void vAFunction( TimerHandle_t xTimer )
 * {
 *     if( xTimerIsTimerActive( xTimer ) != pdFALSE ) // 或更简洁的写法：if( xTimerIsTimerActive( xTimer ) )
 *     {
 *         // xTimer 已激活，删除该定时器
 *         xTimerDelete( xTimer );
 *     }
 *     else
 *     {
 *         // xTimer 未激活，将其周期改为 500ms（此操作也会启动定时器）。
 *         // 若修改周期命令无法立即发送到队列，最多等待 100 个节拍。
 *         if( xTimerChangePeriod( xTimer, 500 / portTICK_PERIOD_MS, 100 ) == pdPASS )
 *         {
 *             // 命令发送成功
 *         }
 *         else
 *         {
 *             // 等待 100 个节拍后仍无法发送命令，此处需执行适当的错误处理
 *         }
 *     }
 * }
 * @endverbatim
 */
#define xTimerChangePeriod( xTimer, xNewPeriod, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD, ( xNewPeriod ), NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerDelete( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 定时器功能由定时器服务任务（timer service/daemon task）提供。许多 FreeRTOS 公共定时器 API 函数会通过
 * 一个名为“定时器命令队列”（timer command queue）的队列，向定时器服务任务发送命令。该队列是内核私有队列，
 * 应用代码无法直接访问。定时器命令队列的长度由配置常量 configTIMER_QUEUE_LENGTH 设定。
 *
 * xTimerDelete() 用于删除一个此前通过 xTimerCreate() API 函数创建的定时器。
 *
 * 需将配置常量 configUSE_TIMERS 设为 1，xTimerDelete() 才会生效。
 *
 * @param xTimer 要删除的定时器句柄。
 *
 * @param xTicksToWait 若调用 xTimerDelete() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数。
 * 若在调度器启动前调用 xTimerDelete()，此参数会被忽略。
 *
 * @return 若等待 xTicksToWait 节拍后仍无法将删除命令发送到定时器命令队列，返回 pdFAIL；
 * 若命令成功发送到队列，返回 pdPASS。命令的实际处理时间取决于定时器服务任务的优先级（相对于系统中其他任务），
 * 定时器服务任务的优先级由配置常量 configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 *
 * 参考 xTimerChangePeriod() API 函数的示例场景。
 *
 */
#define xTimerDelete( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_DELETE, 0U, NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerReset( TimerHandle_t xTimer, TickType_t xTicksToWait );
 *
 * 定时器功能由定时器服务任务（timer service/daemon task）提供。许多 FreeRTOS 公共定时器 API 函数会通过
 * 一个名为“定时器命令队列”（timer command queue）的队列，向定时器服务任务发送命令。该队列是内核私有队列，
 * 应用代码无法直接访问。定时器命令队列的长度由配置常量 configTIMER_QUEUE_LENGTH 设定。
 *
 * xTimerReset() 用于重启此前通过 xTimerCreate() API 函数创建的定时器。若定时器已启动且处于激活态，
 * xTimerReset() 会重新计算定时器的过期时间（以调用 xTimerReset() 的时间为基准）；若定时器处于休眠态，
 * xTimerReset() 的功能与 xTimerStart() API 函数等效（即启动定时器）。
 *
 * 重置定时器会确保定时器进入激活态。若在此期间未停止、删除或再次重置定时器，
 * 则定时器关联的回调函数会在 xTimerReset() 调用后 'n' 个节拍触发，其中 'n' 是定时器定义的周期。
 *
 * 在调度器启动前调用 xTimerReset() 是合法的，但此时定时器不会实际启动，需等到调度器启动后才开始运行；
 * 且定时器的过期时间是相对于“调度器启动时间”计算的，而非“xTimerReset() 调用时间”。
 *
 * 需将配置常量 configUSE_TIMERS 设为 1，xTimerReset() 才会生效。
 *
 * @param xTimer 要重置/启动/重启的定时器句柄。
 *
 * @param xTicksToWait 若调用 xTimerReset() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数。
 * 若在调度器启动前调用 xTimerReset()，此参数会被忽略。
 *
 * @return 若等待 xTicksToWait 节拍后仍无法将重置命令发送到定时器命令队列，返回 pdFAIL；
 * 若命令成功发送到队列，返回 pdPASS。命令的实际处理时间取决于定时器服务任务的优先级（相对于系统中其他任务），
 * 但定时器的过期时间是相对于“xTimerReset() 实际执行时间”计算的。定时器服务任务的优先级由配置常量
 * configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 * @verbatim
 * // 按下按键时，LCD 背光开启；若 5 秒内无按键按下，LCD 背光关闭（此处使用单次定时器）
 *
 * TimerHandle_t xBacklightTimer = NULL;
 *
 * // 分配给单次定时器的回调函数（本示例中未使用参数）
 * void vBacklightTimerCallback( TimerHandle_t pxTimer )
 * {
 *     // 定时器过期，说明 5 秒内无按键按下，关闭 LCD 背光
 *     vSetBacklightState( BACKLIGHT_OFF );
 * }
 *
 * // 按键按下事件处理函数
 * void vKeyPressEventHandler( char cKey )
 * {
 *     // 确保 LCD 背光开启，然后重置定时器（该定时器负责在无按键 5 秒后关闭背光）
 *     // 若命令无法立即发送，最多等待 100 个节拍
 *     vSetBacklightState( BACKLIGHT_ON );
 *     if( xTimerReset( xBacklightTimer, 100 ) != pdPASS )
 *     {
 *         // 重置命令执行失败，此处需执行适当的错误处理
 *     }
 *
 *     // 此处执行其他按键处理逻辑
 * }
 *
 * void main( void )
 * {
 *     int32_t x;
 *
 *     // 创建并启动单次定时器（负责在无按键 5 秒后关闭背光）
 *     xBacklightTimer = xTimerCreate( "BacklightTimer",           // 仅为文本名称，内核不使用
 *                                     ( 5000 / portTICK_PERIOD_MS), // 定时器周期（单位：节拍）
 *                                     pdFALSE,                    // 设为单次定时器
 *                                     0,                          // 回调函数未使用 ID，可设任意值
 *                                     vBacklightTimerCallback     // 关闭 LCD 背光的回调函数
 *                                   );
 *
 *     if( xBacklightTimer == NULL )
 *     {
 *         // 定时器创建失败
 *     }
 *     else
 *     {
 *         // 启动定时器。此处不指定阻塞时间（即使指定，因调度器尚未启动，也会被忽略）
 *         if( xTimerStart( xBacklightTimer, 0 ) != pdPASS )
 *         {
 *             // 定时器无法切换到激活态
 *         }
 *     }
 *
 *     // ...
 *     // 此处创建任务
 *     // ...
 *
 *     // 启动调度器后，已处于激活态的定时器将开始运行
 *     vTaskStartScheduler();
 *
 *     // 正常情况下不会执行到此处
 *     for( ;; );
 * }
 * @endverbatim
 */
#define xTimerReset( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )

/**
 * BaseType_t xTimerStartFromISR(   TimerHandle_t xTimer,
 *                                  BaseType_t *pxHigherPriorityTaskWoken );
 *
 * xTimerStart() 的中断安全版本，可在中断服务程序（ISR）中调用。
 *
 * @param xTimer 要启动/重启的定时器句柄。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务任务（timer service/daemon task）大部分时间处于阻塞态，
 * 等待定时器命令队列的消息。调用 xTimerStartFromISR() 会向队列写入消息，可能使服务任务退出阻塞态。
 * 若调用后服务任务退出阻塞态，且服务任务优先级大于等于当前被中断的任务优先级，则函数会将 *pxHigherPriorityTaskWoken 设为 pdTRUE。
 * 若该值被设为 pdTRUE，需在中断退出前执行任务上下文切换。
 *
 * @return 若启动命令无法发送到定时器命令队列，返回 pdFAIL；若命令成功发送，返回 pdPASS。
 * 命令实际处理时间取决于服务任务优先级（相对于系统其他任务），但定时器过期时间以 xTimerStartFromISR() 实际执行时间为基准。
 * 服务任务优先级由配置常量 configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 * @verbatim
 * // 此场景假设 xBacklightTimer 已创建。按键按下时开启 LCD 背光，若 5 秒无按键则关闭（单次定时器），
 * // 与 xTimerReset() 示例不同，本示例中按键事件处理函数为中断服务程序。
 *
 * // 分配给单次定时器的回调函数（本示例未使用参数）
 * void vBacklightTimerCallback( TimerHandle_t pxTimer )
 * {
 *     // 定时器过期，说明 5 秒无按键，关闭 LCD 背光
 *     vSetBacklightState( BACKLIGHT_OFF );
 * }
 *
 * // 按键按下中断服务程序
 * void vKeyPressEventInterruptHandler( void )
 * {
 *     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // 确保 LCD 背光开启，然后重启定时器（负责无按键 5 秒后关闭背光）
 *     // 中断中仅能调用以 "FromISR" 结尾的 FreeRTOS API
 *     vSetBacklightState( BACKLIGHT_ON );
 *
 *     // xTimerStartFromISR() 或 xTimerResetFromISR() 均可调用（二者均会让定时器重新计算过期时间）
 *     // xHigherPriorityTaskWoken 声明时初始化为 pdFALSE
 *     if( xTimerStartFromISR( xBacklightTimer, &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // 启动命令执行失败，此处需执行错误处理
 *     }
 *
 *     // 此处执行其他按键处理逻辑
 *
 *     // 若 xHigherPriorityTaskWoken 为 pdTRUE，需执行上下文切换
 *     // 中断中切换上下文的语法因 FreeRTOS 端口和编译器而异，需参考对应端口的示例代码
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // 调用中断安全的任务切换函数（具体函数取决于使用的 FreeRTOS 端口）
 *     }
 * }
 * @endverbatim
 */
#define xTimerStartFromISR( xTimer, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * BaseType_t xTimerStopFromISR(    TimerHandle_t xTimer,
 *                                  BaseType_t *pxHigherPriorityTaskWoken );
 *
 * xTimerStop() 的中断安全版本，可在中断服务程序（ISR）中调用。
 *
 * @param xTimer 要停止的定时器句柄。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务任务大部分时间处于阻塞态，等待命令队列消息。
 * 调用 xTimerStopFromISR() 会向队列写入消息，可能使服务任务退出阻塞态。
 * 若调用后服务任务退出阻塞态，且服务任务优先级大于等于当前被中断的任务优先级，则函数会将 *pxHigherPriorityTaskWoken 设为 pdTRUE。
 * 若该值被设为 pdTRUE，需在中断退出前执行任务上下文切换。
 *
 * @return 若停止命令无法发送到定时器命令队列，返回 pdFAIL；若命令成功发送，返回 pdPASS。
 * 命令实际处理时间取决于服务任务优先级，服务任务优先级由配置常量 configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 * @verbatim
 * // 此场景假设 xTimer 已创建并启动。中断触发时，仅需停止该定时器。
 *
 * // 停止定时器的中断服务程序
 * void vAnExampleInterruptServiceRoutine( void )
 * {
 *     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // 中断触发，停止定时器
 *     // xHigherPriorityTaskWoken 在函数内声明时初始化为 pdFALSE
 *     // 中断中仅能调用以 "FromISR" 结尾的 FreeRTOS API
 *     if( xTimerStopFromISR( xTimer, &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // 停止命令执行失败，此处需执行错误处理
 *     }
 *
 *     // 若 xHigherPriorityTaskWoken 为 pdTRUE，需执行上下文切换
 *     // 中断中切换上下文的语法因 FreeRTOS 端口和编译器而异，需参考对应端口的示例代码
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // 调用中断安全的任务切换函数（具体函数取决于使用的 FreeRTOS 端口）
 *     }
 * }
 * @endverbatim
 */
#define xTimerStopFromISR( xTimer, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP_FROM_ISR, 0, ( pxHigherPriorityTaskWoken ), 0U )

/**
 * BaseType_t xTimerChangePeriodFromISR( TimerHandle_t xTimer,
 *                                       TickType_t xNewPeriod,
 *                                       BaseType_t *pxHigherPriorityTaskWoken );
 *
 * xTimerChangePeriod() 的中断安全版本，可在中断服务程序（ISR）中调用。
 *
 * @param xTimer 要修改周期的定时器句柄。
 *
 * @param xNewPeriod 定时器的新周期，单位为系统节拍数（tick）。可使用常量 portTICK_PERIOD_MS 将毫秒时间转换为节拍数，
 * 例如：若定时器需 100 节拍后过期，xNewPeriod 设为 100；若需 500ms 后过期，且 configTICK_RATE_HZ ≤ 1000，
 * 则 xNewPeriod 可设为 (500 / portTICK_PERIOD_MS)。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务任务大部分时间处于阻塞态，等待命令队列消息。
 * 调用 xTimerChangePeriodFromISR() 会向队列写入消息，可能使服务任务退出阻塞态。
 * 若调用后服务任务退出阻塞态，且服务任务优先级大于等于当前被中断的任务优先级，则函数会将 *pxHigherPriorityTaskWoken 设为 pdTRUE。
 * 若该值被设为 pdTRUE，需在中断退出前执行任务上下文切换。
 *
 * @return 若修改周期命令无法发送到定时器命令队列，返回 pdFAIL；若命令成功发送，返回 pdPASS。
 * 命令实际处理时间取决于服务任务优先级，服务任务优先级由配置常量 configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 * @verbatim
 * // 此场景假设 xTimer 已创建并启动。中断触发时，将 xTimer 的周期改为 500ms。
 *
 * // 修改定时器周期的中断服务程序
 * void vAnExampleInterruptServiceRoutine( void )
 * {
 *     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // 中断触发，将 xTimer 周期改为 500ms
 *     // xHigherPriorityTaskWoken 在函数内声明时初始化为 pdFALSE
 *     // 中断中仅能调用以 "FromISR" 结尾的 FreeRTOS API
 *     if( xTimerChangePeriodFromISR( xTimer, pdMS_TO_TICKS(500), &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // 修改周期命令执行失败，此处需执行错误处理
 *     }
 *
 *     // 若 xHigherPriorityTaskWoken 为 pdTRUE，需执行上下文切换
 *     // 中断中切换上下文的语法因 FreeRTOS 端口和编译器而异，需参考对应端口的示例代码
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // 调用中断安全的任务切换函数（具体函数取决于使用的 FreeRTOS 端口）
 *     }
 * }
 * @endverbatim
 */
#define xTimerChangePeriodFromISR( xTimer, xNewPeriod, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD_FROM_ISR, ( xNewPeriod ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * BaseType_t xTimerResetFromISR(   TimerHandle_t xTimer,
 *                                  BaseType_t *pxHigherPriorityTaskWoken );
 *
 * xTimerReset() 的中断安全版本，可在中断服务程序（ISR）中调用。
 *
 * @param xTimer 要启动、重置或重启的定时器句柄。
 *
 * @param pxHigherPriorityTaskWoken 定时器服务任务大部分时间处于阻塞态，等待命令队列消息。
 * 调用 xTimerResetFromISR() 会向队列写入消息，可能使服务任务退出阻塞态。
 * 若调用后服务任务退出阻塞态，且服务任务优先级大于等于当前被中断的任务优先级，则函数会将 *pxHigherPriorityTaskWoken 设为 pdTRUE。
 * 若该值被设为 pdTRUE，需在中断退出前执行任务上下文切换。
 *
 * @return 若重置命令无法发送到定时器命令队列，返回 pdFAIL；若命令成功发送，返回 pdPASS。
 * 命令实际处理时间取决于服务任务优先级，定时器过期时间以 xTimerResetFromISR() 实际执行时间为基准。
 * 服务任务优先级由配置常量 configTIMER_TASK_PRIORITY 设定。
 *
 * 示例用法：
 * @verbatim
 * // 此场景假设 xBacklightTimer 已创建。按键按下时开启 LCD 背光，若 5 秒无按键则关闭（单次定时器），
 * // 与 xTimerReset() 示例不同，本示例中按键事件处理函数为中断服务程序。
 *
 * // 分配给单次定时器的回调函数（本示例未使用参数）
 * void vBacklightTimerCallback( TimerHandle_t pxTimer )
 * {
 *     // 定时器过期，说明 5 秒无按键，关闭 LCD 背光
 *     vSetBacklightState( BACKLIGHT_OFF );
 * }
 *
 * // 按键按下中断服务程序
 * void vKeyPressEventInterruptHandler( void )
 * {
 *     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
 *
 *     // 确保 LCD 背光开启，然后重置定时器（负责无按键 5 秒后关闭背光）
 *     // 中断中仅能调用以 "FromISR" 结尾的 FreeRTOS API
 *     vSetBacklightState( BACKLIGHT_ON );
 *
 *     // xTimerStartFromISR() 或 xTimerResetFromISR() 均可调用（二者均会让定时器重新计算过期时间）
 *     // xHigherPriorityTaskWoken 声明时初始化为 pdFALSE
 *     if( xTimerResetFromISR( xBacklightTimer, &xHigherPriorityTaskWoken ) != pdPASS )
 *     {
 *         // 重置命令执行失败，此处需执行错误处理
 *     }
 *
 *     // 此处执行其他按键处理逻辑
 *
 *     // 若 xHigherPriorityTaskWoken 为 pdTRUE，需执行上下文切换
 *     // 中断中切换上下文的语法因 FreeRTOS 端口和编译器而异，需参考对应端口的示例代码
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // 调用中断安全的任务切换函数（具体函数取决于使用的 FreeRTOS 端口）
 *     }
 * }
 * @endverbatim
 */
#define xTimerResetFromISR( xTimer, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )

/**
 * BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend,
 *                                          void *pvParameter1,
 *                                          uint32_t ulParameter2,
 *                                          BaseType_t *pxHigherPriorityTaskWoken );
 *
 * 用于在应用中断服务程序（ISR）中，将函数的执行延迟到 RTOS 守护任务（daemon task，即定时器服务任务，因此该函数
 * 在 timers.c 中实现且以“Timer”为前缀）的上下文执行。
 *
 * 理想情况下，中断服务程序应尽可能简短，但有时 ISR 可能需要执行大量处理，或需执行非确定性操作（如内存分配、阻塞操作）。
 * 这种场景下，可通过 xTimerPendFunctionCallFromISR() 将函数处理逻辑延迟到守护任务中执行，避免中断占用 CPU 过久。
 *
 * 该函数提供一种机制：中断可直接返回到后续将执行延迟函数的任务（守护任务）。这使得延迟函数的执行能与中断在时间上“连续衔接”，
 * 效果等同于函数在中断内执行，但实际运行在任务上下文（可安全调用非中断安全 API）。
 *
 * @param xFunctionToPend 要在定时器服务/守护任务中执行的函数。该函数必须符合 PendedFunction_t 函数原型（参数为 void* 和 uint32_t，无返回值）。
 *
 * @param pvParameter1 延迟函数（xFunctionToPend）的第一个参数。类型为 void*，可用于传递任意类型数据（如将无符号长整型强制转换为 void*，或传递结构体指针）。
 *
 * @param ulParameter2 延迟函数（xFunctionToPend）的第二个参数。类型为 uint32_t，适用于传递整数型数据（如设备编号、状态标记）。
 *
 * @param pxHigherPriorityTaskWoken 如前所述，调用该函数会向定时器守护任务的消息队列发送消息。若守护任务的优先级（由 FreeRTOSConfig.h 中的
 * configTIMER_TASK_PRIORITY 配置）高于当前被中断任务的优先级，则 xTimerPendFunctionCallFromISR() 会将
 * *pxHigherPriorityTaskWoken 设为 pdTRUE，表明需在中断退出前请求任务上下文切换。因此，调用前必须将 *pxHigherPriorityTaskWoken 初始化为 pdFALSE。
 *
 * @return 若消息成功发送到定时器守护任务的队列，返回 pdPASS；否则返回 pdFAIL（通常因队列满导致）。
 *
 * 示例用法：
 * @verbatim
 *
 *  // 要在守护任务上下文执行的回调函数（注意：所有延迟函数必须符合此原型）
 *  void vProcessInterface( void *pvParameter1, uint32_t ulParameter2 )
 *  {
 *      BaseType_t xInterfaceToService;
 *
 *      // 需要处理的接口编号通过第二个参数传递，本示例中不使用第一个参数
 *      xInterfaceToService = ( BaseType_t ) ulParameter2;
 *
 *      // ...此处执行具体的接口处理逻辑（如读取数据、解析协议等）...
 *  }
 *
 *  // 从多个接口接收数据包的中断服务程序
 *  void vAnISR( void )
 *  {
 *      BaseType_t xInterfaceToService, xHigherPriorityTaskWoken;
 *
 *      // 查询硬件，确定哪个接口需要处理（自定义函数 prvCheckInterfaces()）
 *      xInterfaceToService = prvCheckInterfaces();
 *
 *      // 实际处理逻辑延迟到任务中执行：请求执行 vProcessInterface() 回调函数，
 *      // 并传递需要处理的接口编号（通过第二个参数），本示例不使用第一个参数
 *      xHigherPriorityTaskWoken = pdFALSE; // 初始化标记变量
 *      xTimerPendFunctionCallFromISR( 
 *          vProcessInterface,       // 要延迟执行的函数
 *          NULL,                    // 第一个参数（未使用）
 *          ( uint32_t ) xInterfaceToService, // 第二个参数（接口编号）
 *          &xHigherPriorityTaskWoken // 任务切换标记
 *      );
 *
 *      // 若 xHigherPriorityTaskWoken 被设为 pdTRUE，需请求上下文切换
 *      // 具体使用的宏因端口而异，通常为 portYIELD_FROM_ISR() 或 portEND_SWITCHING_ISR()，
 *      // 需参考所用 FreeRTOS 端口的文档
 *      portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
 *
 *  }
 * @endverbatim
 */
#if ( INCLUDE_xTimerPendFunctionCall == 1 )
    BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend,
                                              void * pvParameter1,
                                              uint32_t ulParameter2,
                                              BaseType_t * pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#endif

/**
 * BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend,
 *                                    void *pvParameter1,
 *                                    uint32_t ulParameter2,
 *                                    TickType_t xTicksToWait );
 *
 * 用于将函数的执行延迟到 RTOS 守护任务（daemon task，即定时器服务任务，因此该函数
 * 在 timers.c 中实现且以“Timer”为前缀）的上下文执行。
 *
 * @param xFunctionToPend 要在定时器服务/守护任务中执行的函数。该函数必须符合 PendedFunction_t 函数原型
 * （参数为 void* 和 uint32_t，无返回值）。
 *
 * @param pvParameter1 延迟函数（xFunctionToPend）的第一个参数。类型为 void*，可用于传递任意类型数据
 * （如将无符号长整型强制转换为 void*，或传递结构体指针）。
 *
 * @param ulParameter2 延迟函数（xFunctionToPend）的第二个参数。类型为 uint32_t，适用于传递整数型数据
 * （如设备编号、状态标记）。
 *
 * @param xTicksToWait 调用该函数会向定时器守护任务的队列发送消息。若队列已满，xTicksToWait 表示调用任务
 * 应进入阻塞态等待队列空闲的最大节拍数（等待期间不占用 CPU 时间）。
 *
 * @return 若消息成功发送到定时器守护任务的队列，返回 pdPASS；否则返回 pdFAIL（通常因队列满且等待超时导致）。
 *
 */
#if ( INCLUDE_xTimerPendFunctionCall == 1 )
    BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend,
                                       void * pvParameter1,
                                       uint32_t ulParameter2,
                                       TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
#endif

/**
 * const char * const pcTimerGetName( TimerHandle_t xTimer );
 *
 * 返回定时器创建时分配的名称。
 *
 * @param xTimer 要查询的定时器句柄。
 *
 * @return 由 xTimer 参数指定的定时器的名称。
 */
const char * pcTimerGetName( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * void vTimerSetReloadMode( TimerHandle_t xTimer, const BaseType_t xAutoReload );
 *
 * 更新定时器为自动重载模式（每次过期后自动重置）或单次模式（仅过期一次，除非手动重启）。
 *
 * @param xTimer 要更新的定时器句柄。
 *
 * @param xAutoReload 若设为 pdTRUE，定时器将按其周期（见 xTimerCreate() 的 xTimerPeriodInTicks 参数）重复过期；
 * 若设为 pdFALSE，定时器将为单次模式，过期后进入休眠态。
 */
void vTimerSetReloadMode( TimerHandle_t xTimer,
                          const BaseType_t xAutoReload ) PRIVILEGED_FUNCTION;

/**
 * BaseType_t xTimerGetReloadMode( TimerHandle_t xTimer );
 *
 * 查询定时器当前的工作模式，判断其为自动重载模式（每次过期后自动重置）还是单次模式（仅过期一次，除非手动重启）。
 *
 * @param xTimer 要查询的定时器句柄。
 *
 * @return 若定时器为自动重载模式，返回 pdTRUE；否则返回 pdFALSE。
 */
BaseType_t xTimerGetReloadMode( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer );
 *
 * 查询定时器当前的工作模式，判断其为自动重载模式（每次过期后自动重置）还是单次模式（仅过期一次，除非手动重启）。
 *
 * @param xTimer 要查询的定时器句柄。
 *
 * @return 若定时器为自动重载模式，返回 pdTRUE；否则返回 pdFALSE。
 */
UBaseType_t uxTimerGetReloadMode( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * TickType_t xTimerGetPeriod( TimerHandle_t xTimer );
 *
 * 返回定时器的周期。
 *
 * @param xTimer 要查询的定时器句柄。
 *
 * @return 定时器的周期（单位：节拍）。
 */
TickType_t xTimerGetPeriod( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer );
 *
 * 返回定时器的过期时间（单位：节拍）。若该值小于当前节拍数，说明过期时间已从当前时间溢出（因节拍计数器循环）。
 *
 * @param xTimer 要查询的定时器句柄。
 *
 * @return 若定时器正在运行，返回其下一次过期的节拍时间；若定时器未运行，返回值未定义。
 */
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;

/**
 * BaseType_t xTimerGetStaticBuffer( TimerHandle_t xTimer,
 *                                   StaticTimer_t ** ppxTimerBuffer );
 *
 * 获取静态创建的定时器的数据结构缓冲区指针，该缓冲区是定时器创建时由用户提供的。
 *
 * @param xTimer 要获取缓冲区的定时器句柄（需为静态创建的定时器）。
 *
 * @param ppxTimerBuffer 用于返回定时器数据结构缓冲区的指针（输出参数，类型为 StaticTimer_t**）。
 *
 * @return 若成功获取缓冲区，返回 pdTRUE；否则返回 pdFALSE（如定时器为动态创建或句柄无效）。
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    BaseType_t xTimerGetStaticBuffer( TimerHandle_t xTimer,
                                      StaticTimer_t ** ppxTimerBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

/*
 * 此部分之后的函数不属于公共 API，仅用于内核内部。
 */
BaseType_t xTimerCreateTimerTask( void ) PRIVILEGED_FUNCTION;

/*
 * 将 xTimerGenericCommand 拆分为两个子函数并定义为宏，是为了避免在 ISR 中调用时产生递归路径。
 * 这主要针对 XCore XCC 端口——该端口在未拆分时会检测到递归路径，并在编译时抛出错误。
 */
BaseType_t xTimerGenericCommandFromTask( TimerHandle_t xTimer,
                                         const BaseType_t xCommandID,
                                         const TickType_t xOptionalValue,
                                         BaseType_t * const pxHigherPriorityTaskWoken,
                                         const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

BaseType_t xTimerGenericCommandFromISR( TimerHandle_t xTimer,
                                        const BaseType_t xCommandID,
                                        const TickType_t xOptionalValue,
                                        BaseType_t * const pxHigherPriorityTaskWoken,
                                        const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

#define xTimerGenericCommand( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait )         \
    ( ( xCommandID ) < tmrFIRST_FROM_ISR_COMMAND ?                                                                  \
      xTimerGenericCommandFromTask( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait ) : \
      xTimerGenericCommandFromISR( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait ) )
#if ( configUSE_TRACE_FACILITY == 1 )
    void vTimerSetTimerNumber( TimerHandle_t xTimer,
                               UBaseType_t uxTimerNumber ) PRIVILEGED_FUNCTION;
    UBaseType_t uxTimerGetTimerNumber( TimerHandle_t xTimer ) PRIVILEGED_FUNCTION;
#endif

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

/**
 * task.h
 * @code{c}
 * void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer, StackType_t ** ppxTimerTaskStackBuffer, configSTACK_DEPTH_TYPE * puxTimerTaskStackSize )
 * @endcode
 *
 * 当 configSUPPORT_STATIC_ALLOCATION 设为 1 时，需实现此函数，为 FreeRTOS 提供静态分配的内存块，用于存储定时器任务（Timer Task）的 TCB（任务控制块）。
 * 更多信息请参考：https://www.FreeRTOS.org/a00110.html#configSUPPORT_STATIC_ALLOCATION
 *
 * @param ppxTimerTaskTCBBuffer   指向静态分配的定时器任务 TCB 缓冲区的指针（输出参数）。
 * @param ppxTimerTaskStackBuffer 指向静态分配的定时器任务栈缓冲区的指针（输出参数）。
 * @param puxTimerTaskStackSize   指向栈缓冲区可容纳的元素数量的指针（输出参数，类型为 configSTACK_DEPTH_TYPE）。
 */
    void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                         StackType_t ** ppxTimerTaskStackBuffer,
                                         configSTACK_DEPTH_TYPE * puxTimerTaskStackSize );

#endif

#if ( configUSE_DAEMON_TASK_STARTUP_HOOK != 0 )

/**
 *  timers.h
 * @code{c}
 * void vApplicationDaemonTaskStartupHook( void );
 * @endcode
 *
 * 此钩子函数（Hook Function）在定时器任务（守护任务）首次开始运行时被调用，仅执行一次。
 */
    /* MISRA 参考 8.6.1 [外部链接] */
    /* 更多细节：https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-86 */
    /* coverity[misra_c_2012_rule_8_6_violation] */
    void vApplicationDaemonTaskStartupHook( void );

#endif

/*
 * 此函数用于重置定时器模块的内部状态，应用程序在重启调度器前必须调用该函数。
 */
void vTimerResetState( void ) PRIVILEGED_FUNCTION;

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */
#endif /* TIMERS_H */