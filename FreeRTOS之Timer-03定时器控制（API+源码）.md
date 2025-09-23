# FreeRTOS之Timer-03定时器控制（API+源码）

什么是定时器控制？

比如开启定时器、停止定时器、删除定时器等等。

在 [FreeRTOS之Timer-02定时器守护任务（源码）](./FreeRTOS之Timer-02定时器守护任务（源码）.md) 中提到守护任务做了三件事1. 处理一个用户定义函数；2.检查是否有定时器到期；3. 处理定时器命令。这里守护任务处理定时器命令就是从一个与定时器相关的队列中读取命令然后做相应的处理，那定时器控制就很好想了，就是向这个队列中发送命令，然后守护任务处理。

定时器相关控制命令如下：

```c
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
```

文章涉及内容：开启定时器；

API：主要关注接口参数和使用

源码：给出源码->分析->总结（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）（不考虑队列集）

## *（API）*开启定时器

```c
#define xTimerStart( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )
/* 示例用法：
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
 */
```

*param1：*要启动/重启的定时器句柄

*param2：*若调用 xTimerStart() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

**（API）****开启定时器--中断安全版**

```c
#define xTimerStartFromISR( xTimer, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )
/* 示例用法：
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
 *     // 中断中切换上下文的语法因 FreeRTOS 移植版本和编译器而异，需参考对应移植版本的示例代码
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // 调用中断安全的任务切换函数（具体函数取决于使用的 FreeRTOS 移植版本）
 *     }
 * }
 */
```

*param1：*要启动/重启的定时器句柄

*param2：*定时器服务任务（timer service/daemon task）大部分时间处于阻塞态，等待定时器命令队列的消息。调用 xTimerStartFromISR() 会向队列写入消息，可能使服务任务退出阻塞态。若调用后服务任务退出阻塞态，且服务任务优先级大于等于当前被中断的任务优先级，则函数会将 *pxHigherPriorityTaskWoken 设为 pdTRUE。若该值被设为 pdTRUE，需在中断退出前执行任务上下文切换。

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

## *（API）*停止定时器

```c
#define xTimerStop( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP, 0U, NULL, ( xTicksToWait ) )
/* 使用方法看xTimerStart */
```

*param1：*要停止的定时器句柄

*param2：*若调用 xTimerStop()时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

**（API）****停止定时器--中断安全版**

```c
#define xTimerStopFromISR( xTimer, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_STOP_FROM_ISR, 0, ( pxHigherPriorityTaskWoken ), 0U )
/* 示例用法：
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
 *     // 中断中切换上下文的语法因 FreeRTOS 移植版本和编译器而异，需参考对应移植版本的示例代码
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // 调用中断安全的任务切换函数（具体函数取决于使用的 FreeRTOS 移植版本）
 *     }
 * }
 */
```

*param1：*要停止的定时器句柄

*param2：*标记是否需要上下文切换

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

## *（API）*修改定时器周期

```c
#define xTimerChangePeriod( xTimer, xNewPeriod, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD, ( xNewPeriod ), NULL, ( xTicksToWait ) )
/* 示例用法：
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
 */
```

*param1：*要修改周期的定时器句柄

*param2：*定时器的新周期，单位为系统节拍数（tick）

> 可使用常量 portTICK_PERIOD_MS 将毫秒时间转换为节拍数

*param3：*若调用 xTimerChangePeriod() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

**（API）****修改定时器周期--中断安全版**

```c
#define xTimerChangePeriodFromISR( xTimer, xNewPeriod, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_CHANGE_PERIOD_FROM_ISR, ( xNewPeriod ), ( pxHigherPriorityTaskWoken ), 0U )
/* 示例用法：
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
 *     // 中断中切换上下文的语法因 FreeRTOS 移植版本和编译器而异，需参考对应移植版本的示例代码
 *     if( xHigherPriorityTaskWoken != pdFALSE )
 *     {
 *         // 调用中断安全的任务切换函数（具体函数取决于使用的 FreeRTOS 移植版本）
 *     }
 * }
 */
```

*param1：*要修改周期的定时器句柄

*param2：*定时器的新周期，单位为系统节拍数（tick）

*param3：*标记是否需要上下文切换

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

## *（API）*删除定时器

```c
#define xTimerDelete( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_DELETE, 0U, NULL, ( xTicksToWait ) )
/* 使用方法看xTimerChangePeriod */
```

*param1：*要删除的定时器句柄

*param2：*若调用 xTimerDelete() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

## （API）重置定时器

```c
#define xTimerReset( xTimer, xTicksToWait ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )
/* 示例用法：
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
 */
```

*param1：*要重启的定时器句柄

*param2：*若调用 xTimerReset() 时定时器命令队列已满，调用任务将进入阻塞状态等待的节拍数

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

**（API）****重置定时器--中断安全版**

```c
#define xTimerResetFromISR( xTimer, pxHigherPriorityTaskWoken ) \
    xTimerGenericCommand( ( xTimer ), tmrCOMMAND_RESET_FROM_ISR, ( xTaskGetTickCountFromISR() ), ( pxHigherPriorityTaskWoken ), 0U )
/* 示例用法：
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
 *      // 具体使用的宏因移植版本而异，通常为 portYIELD_FROM_ISR() 或 portEND_SWITCHING_ISR()，
 *      // 需参考所用 FreeRTOS 移植版本的文档
 *      portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
 *
 *  }
 */
```

*param1：*要重启的定时器句柄

*param2：*标记是否需要上下文切换

*return：*成功返回 *pdPASS*；失败返回 *pdFAIL*

## *（源码）*发送命令

我们可以看到，所有的定时器命令都是用了一个函数 *xTimerGenericCommand* ，它的定义如下：

```c
#define xTimerGenericCommand( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait )         \
    ( ( xCommandID ) < tmrFIRST_FROM_ISR_COMMAND ?                                                                  \
      xTimerGenericCommandFromTask( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait ) : \
      xTimerGenericCommandFromISR( xTimer, xCommandID, xOptionalValue, pxHigherPriorityTaskWoken, xTicksToWait ) )
```

接下来看一看这两个函数

首先来看*xTimerGenericCommandFromTask*

```c
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

        /* 向定时器服务任务发送消息，指示其对特定定时器执行特定操作。
         * （定时器服务任务是唯一有权修改定时器状态的任务，确保线程安全） */
        if( xTimerQueue != NULL )  // 先检查定时器队列（用于通信）是否已初始化
        {
            /* 封装命令消息：将命令ID、可选参数、目标定时器句柄填入消息结构体 */
            xMessage.xMessageID = xCommandID;  // 命令类型（如tmrCOMMAND_START、tmrCOMMAND_STOP）
            xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;  // 命令的附加参数
            xMessage.u.xTimerParameters.pxTimer = xTimer;  // 要操作的目标定时器

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
        }else{}

        return xReturn;  // 返回命令发送结果（pdPASS/pdFAIL）
    }
```

很简单两步，封装消息、发送到队列（直接看源码就好）

封装消息*xMessage*定义如下

```c
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
```

发送到队列*xQueueSendToBack* 在[FreeRTOS之队列结构体用作队列（API+源码）](./FreeRTOS之队列结构体用作队列（API+源码）.md) 中有说明。

没错，到这就结束了。其实还有一些检查定时器状态的函数：*xTimerIsTimerActive(*查询定时器当前处于激活状态(active)还是休眠状态(dormant))、*pcTimerGetName*(返回定时器创建时分配的名称)、*xTimerGetReloadMode*(查询定时器当前的工作模式，判断其为自动重载模式(每次过期后自动重置)还是单次模式(仅过期一次，除非手动重启))等等，其实都很简单，查看定时器的状态就好，定时器结构体如下

![image-20250923181004851](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250923181004851.png)

比如*pcTimerGetName，*读取*Timer_t*的*pcTimerName*就好