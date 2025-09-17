# FreeRTOS之Timer-01创建定时器（API+源码）

文章涉及内容：创建定时器的API；定时器是什么样子；创建定时器（源码，实现）；

API：主要关注接口参数和使用

源码：给出源码->分析->总结（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

## 创建定时器的API

首先如果要使用定时器功能，要定义几个宏，《Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0》中描述如下

![img](https://pic1.zhimg.com/80/v2-caf99edd2394cc4112f26ec0cba95bc6_1440w.png?source=ccfced1a)

### *（API）xTimerCreate* 动态分配

```c
    TimerHandle_t xTimerCreate( const char * const pcTimerName,
                                const TickType_t xTimerPeriodInTicks,
                                const BaseType_t xAutoReload,
                                void * const pvTimerID,
                                TimerCallbackFunction_t pxCallbackFunction ) PRIVILEGED_FUNCTION;
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

上面创建了5个定时器，*xTimerCreate* 怎么用，按上面的示例来就好，我这里说一下传入参数：

*pcTimerName：* 定时器名称，字符数组

*xTimerPeriodInTicks：*定时器周期，单位为系统节拍数（tick）

> 一个节拍是多少呢？ 它的数据类型为 TickType_t，定义如下（我用的是ARM CM3（ARM Cortex-M3）移植版本）

```c
#if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS )
    typedef uint16_t     TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffff
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
    typedef uint32_t     TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffffffffUL
    #define portTICK_TYPE_IS_ATOMIC    1
#else
    #error configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width.
#endif

#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_64_BITS

/* configTICK_TYPE_WIDTH_IN_BITS 的可接受值。 */
#define TICK_TYPE_WIDTH_16_BITS    0  // 16位的节拍类型宽度
#define TICK_TYPE_WIDTH_32_BITS    1  // 32位的节拍类型宽度
#define TICK_TYPE_WIDTH_64_BITS    2  // 64位的节拍类型宽度
```

默认是64位，但是ARM CM3（ARM Cortex-M3）移植版本 是不支持的，使用前要改成16或32位。

*xAutoReload：*自动重装设置，就是要不要每次定时器结束都重新计数。*pdTRUE* 就是要，*pdFALSE*就是不*要*

*pvTimerID ：*分配给所创建定时器的标识符。当多个定时器共用同一个回调函数时，通常通过该标识符在回调函数中区分哪个定时器已过期。

*pxCallbackFunction ：*定时器过期时调用的函数，就是每个定时器周期我们要做的事

> 定时器任务函数怎么定义？ *pxCallbackFunction* 的数据类型是 *TimerCallbackFunction_t*   它的定义如下： typedef void (* TimerCallbackFunction_t)( TimerHandle_t xTimer ); 是一个带 TimerHandle_t 类型参数、无返回值的函数指针。 TimerHandle_t 又是什么？ 这就是定时器了，之前创建任务的时候也有一个任务句柄，任务句柄就是一个任务的TCB控制块指针，那这就是一个定时器指针了，定时器什么样子呢？之后说明--定时器是什么

所以定时器任务是有固定格式的，是一个带 TimerHandle_t 类型参数、无返回值的函数。

### *（API）*xTimerCreateStatic 静态分配

```c
    TimerHandle_t xTimerCreateStatic( const char * const pcTimerName,
                                      const TickType_t xTimerPeriodInTicks,
                                      const BaseType_t xAutoReload,
                                      void * const pvTimerID,
                                      TimerCallbackFunction_t pxCallbackFunction,
                                      StaticTimer_t * pxTimerBuffer ) PRIVILEGED_FUNCTION;
/* 示例用法：
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
 *     xTimer = xTimerCreateStatic( 「T1」,             // 定时器文本名称，仅用于调试，FreeRTOS 不使用
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
```

我们只看不一样的地方

*StaticTimer_t \* pxTimerBuffer*  这是一个 *StaticTimer_t* 类型的指针的，这是什么呢，看下面

```c
typedef struct xSTATIC_TIMER
{
    void * pvDummy1;
    StaticListItem_t xDummy2;
    TickType_t xDummy3;
    void * pvDummy5;
    TaskFunction_t pvDummy6;
    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxDummy7;
    #endif
    uint8_t ucDummy8;
} StaticTimer_t;
```

这又是什么，都是看不懂的东西，看官方注释的意思好像是这个东西的东西和定时器的大小一样的，那这其实也是定时器嘛，那定时器是什么？下面就说到

## 定时器是什么

前面说*TimerHandle_t* 是定时器指针，定义如下：

```c
struct tmrTimerControl; /* 使用旧的命名约定是为了避免破坏内核感知调试器 */
typedef struct tmrTimerControl * TimerHandle_t;
```

*struct   tmrTimerControl*  这就是定时器了，他长什么样子呢？

```c
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
```

下面就用 *Timer_t* 表示定时器了。

![img](https://pica.zhimg.com/80/v2-b263ae357664f25b28841969a55f185a_1440w.png?source=ccfced1a)

它的内容很好理解，名称、周期、ID、函数，这里说一下剩下两个，链表项和状态。

**链表项**（这是个链表的节点）：之前说过每个任务有两个链表项，原来定时器也有，看来定时器也会放到一个链表中，链表项在 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 中说过了，就是下面这个样子：

![img](https://picx.zhimg.com/80/v2-38c56de2eec0689c6b812ec0c47c146b_1440w.png?source=ccfced1a)

这个节点有什么用呢？

再定时器这里有两个全局链表和两个指向这些链表的指针：

```c
PRIVILEGED_DATA static List_t xActiveTimerList1;
PRIVILEGED_DATA static List_t xActiveTimerList2;
PRIVILEGED_DATA static List_t * pxCurrentTimerList;
PRIVILEGED_DATA static List_t * pxOverflowTimerList;
```

在任务创建时我们有见到过两个延迟链表和两个指向这两个链表的指针，其中一个链表放当前系统时间周期（就是全局计数*xTickCount*  溢出前）会运行的任务，一个放溢出以后会运行的任务，那定时器的这两个链表也一样，一个是当前周期内会触发的定时器的，一个是溢出后会触发的定时器。至于两个指针就是指出哪个是未溢出的，哪个是溢出后的。

**状态**：标志定时器的状态，下面是定时器可能的状态：

```c
/* 用于定时器结构体中 ucStatus 成员的位定义 */
    #define tmrSTATUS_IS_ACTIVE                  ( 0x01U ) /* 定时器处于激活状态 */
    #define tmrSTATUS_IS_STATICALLY_ALLOCATED    ( 0x02U ) /* 定时器由静态内存分配 */
    #define tmrSTATUS_IS_AUTORELOAD              ( 0x04U ) /* 定时器为自动重载模式 */
```

这三个状态可不是说定时器在激活状态时，就不在静态内存分配和自动重载模式。这里是三个位，每个位都有自己的含义。

## 定时器（源码，实现）

```c
    #if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

        TimerHandle_t xTimerCreate( const char * const pcTimerName,
                                    const TickType_t xTimerPeriodInTicks,
                                    const BaseType_t xAutoReload,
                                    void * const pvTimerID,
                                    TimerCallbackFunction_t pxCallbackFunction )
        {
            Timer_t * pxNewTimer;

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

            return pxNewTimer;
        }

    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
```

这个函数很简单了，申请空间、初始化状态、调用 *prvInitialiseNewTimer* 初始化定时器。

申请空间：调用 *pvPortMalloc* 这个和使用的哪个<heap.c>有关，就是从堆中拿出定时器的空间，具体在 [FreeRTOS内存管理01（源码）](./FreeRTOS内存管理01（源码）.md) 和 [FreeRTOS内存管理02（源码）](./FreeRTOS内存管理02（源码）.md) 。

初始化状态：赋初始值，三个位都是0，表示 未激活状态、动态分配、非自动重载模式（单次）模式。

接下来看看定时器初始化成什么样子了，下面是 *prvInitialiseNewTimer*  源码：

```c
// 静态内部函数：初始化新创建的定时器结构体（Timer_t）
// 仅在FreeRTOS内核内部调用，不对外暴露接口
static void prvInitialiseNewTimer( const char * const pcTimerName,          // 定时器名称（字符串常量）
                                       const TickType_t xTimerPeriodInTicks,    // 定时器周期（单位：系统时钟节拍）
                                       const BaseType_t xAutoReload,            // 是否自动重载（pdTRUE=自动，pdFALSE=单次）
                                       void * const pvTimerID,                  // 定时器ID（用于区分多个定时器）
                                       TimerCallbackFunction_t pxCallbackFunction,  // 定时器超时回调函数
                                       Timer_t * pxNewTimer )                    // 待初始化的定时器结构体指针
    {
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
    } 
```

我们先跳过 *prvCheckForValidListAndQueue* 这个函数，剩下的就很简单了，为刚刚已经申请好空间的定时器直接赋初值（名称、周期、ID、定时器回调函数、定时器状态），这些值都是我们使用者要定义出来的。

下面看看*prvCheckForValidListAndQueue* ：

```c
static void prvCheckForValidListAndQueue( void )
{
    /* 检查用于引用活动定时器的列表以及用于与定时器服务通信的队列是否已初始化。 */
    taskENTER_CRITICAL();  // 进入临界区（禁止任务调度和中断）
    {     // 如果定时器队列尚未初始化
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

            // 如果配置了队列注册表且队列创建成功，这里不关注
            #if ( configQUEUE_REGISTRY_SIZE > 0 )
            {
                if( xTimerQueue != NULL )
                {
                    // 将定时器队列添加到注册表，名称为「TmrQ」
                    vQueueAddToRegistry( xTimerQueue, "TmrQ" );
                }
                else
                {}
            }
            #endif /* configQUEUE_REGISTRY_SIZE */
        }
        else
        {}
    }
    taskEXIT_CRITICAL();  // 退出临界区（恢复任务调度和中断）
}
```

这里主要做了三件事，初始化两个定时器相关全局链表、创建定时器的队列。

这两个链表还好理解，这个队列是干什么的？ 这个队列是定时器的任务队列，比如你调用定时器相关 API（如*xTimerStart*、*xTimerStop* 、*xTimerReset*等）时，这些函数其实都是发送一个命令到队列里最后在定时器的“空闲任务”（守护任务）里处理。

具体怎么发送到队列、定时器守护任务做了什么在之后的文章再说明。

**小结：定时器创建函数做了什么**

1. 申请定时器的空间；2.初始化定时器结构体；3.初始化定时器相关链表和队列

![img](https://picx.zhimg.com/80/v2-a2ed5cc0e5e76989d5968803f2c7a5f8_1440w.png?source=ccfced1a)