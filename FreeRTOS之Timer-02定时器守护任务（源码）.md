# FreeRTOS之Timer-02定时器守护任务（源码）

什么是定时器的守护任务？ 当我们要使用FreeRTOS的定时器功能时，调度器开始时会自动创建两个任务，一个是空闲任务，一个是定时器的守护任务。守护任务做了三件事1. 处理一个用户定义函数；2.检查是否有定时器到期；3. 处理定时器命令。要注意的是空闲任务优先级最低，守护任务优先级最高。

接下来通过源码看看守护任务长什么样子，它做了什么。

## 创建守护任务

守护任务的创建调用了 *xTimerCreateTimerTask* 函数，先给出源码，然后解释（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）：

```c
// 函数：创建FreeRTOS软件定时器的“定时器服务任务”（仅当启用软件定时器时调用）
BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdFAIL;  // 函数返回值（默认失败，创建成功后改为pdPASS）

    /* 注释说明：
     * 当configUSE_TIMERS（软件定时器功能开关）设为1时，此函数在调度器启动时被调用；
     * 作用是检查定时器服务任务依赖的基础结构（如定时器队列、定时器链表）是否已初始化；
     * 若定时器服务任务已创建过，基础结构应已完成初始化，无需重复操作。 */
    prvCheckForValidListAndQueue();  // 内部函数：检查定时器队列、定时器链表是否有效

    // 若定时器队列（xTimerQueue）已初始化（非NULL），则开始创建定时器服务任务
    if( xTimerQueue != NULL )
    {
        // 分支1：多核系统（核心数>1）且启用核心亲和性（任务绑定特定核心）
        #if ( ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) )
            /*       本文不考虑多核         */
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
    {}
    return xReturn;  // 返回创建结果（pdPASS成功，pdFAIL失败）
}
```

这个函数主要完成了两项工作：1. 检查定时器相关链表和队列；2. 调用创建任务函数创建守护任务。

**1.检查定时器相关链表和队列**

哪里来的链表？

在 [FreeRTOS之Timer-01创建定时器（API+源码）](./FreeRTOS之Timer-01创建定时器（API+源码）.md) 我们提到过，简单说一下，FreeRTOS系统里有两个存放定时器的链表，当定时器启动时会按定时器唤醒时间（到期时间）有序的排在链表中，通过检查这个链表就可以知道哪个定时器应该唤醒执行相关任务了。

```c
PRIVILEGED_DATA static List_t xActiveTimerList1;  //定时器活动链表1
PRIVILEGED_DATA static List_t xActiveTimerList2;  //定时器活动链表1
PRIVILEGED_DATA static List_t * pxCurrentTimerList;  //当前定时器链表
PRIVILEGED_DATA static List_t * pxOverflowTimerList;  //定时器溢出链表
```

哪里来的队列？

当我们控制定时器开启关闭时，其实是向定时器发送以一个命令，这个命令首先会放在队列里，然后被守护任务读取并执行对应指令。

```c
PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;  //定时器队列
```

为什么要检查？

因为如果我们没有创建定时器，不会自动创建定时器队列、定时器链表。如果之前没有创建，这里创建。

**2. 调用创建任务函数创建守护任务**

调用 *xTaskCreate* 或者 *xTaskCreateStatic* 创建定时器任务（详见 [FreeRTOS之Task-01任务创建（API+源码）](./FreeRTOS之Task-01任务创建（API+源码）.md)）。

这里主要看一下三个参数：任务名称、任务优先级、定时器服务任务函数。

**任务名称：** *configTIMER_SERVICE_TASK_NAME*

```c
#ifndef configTIMER_SERVICE_TASK_NAME
    #define configTIMER_SERVICE_TASK_NAME    "Tmr Svc"
#endif
```

默认为"*Tmr Svc*"。

**任务优先级：** *(( UBaseType_t ) configTIMER_TASK_PRIORITY )| portPRIVILEGE_BIT*

*portPRIVILEGE_BIT* 是什么？

```c
#ifndef portPRIVILEGE_BIT
   #define portPRIVILEGE_BIT    ( ( UBaseType_t ) 0x00 )
#endif
```

默认情况下为0（暂时不知道怎么用），所以任务的优先级就是 *configTIMER_TASK_PRIORITY* 

```c
#define configTIMER_TASK_PRIORITY       ( configMAX_PRIORITIES - 1 )
```

守护任务优先级为最大优先级。

**定时器服务任务函数：** *prvTimerTask*

找到它我们就找到了守护任务怎么定义的。

```c
static portTASK_FUNCTION( prvTimerTask, pvParameters )
{
  /*    守护任务内容      */
}
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )
```

## 守护任务函数

下面看看源码：

```c
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
```

三件事：1. 处理一个用户定义函数；2.检查到期定时器；3. 处理定时器控制命令

### **1. 处理一个用户定义函数**  *vApplicationDaemonTaskStartupHook*

```c
#if ( configUSE_DAEMON_TASK_STARTUP_HOOK != 0 )
/**
 *  timers.h
 * @code{c}
 * void vApplicationDaemonTaskStartupHook( void );
 * @endcode
 *
 * 此钩子函数（Hook Function）在定时器任务（守护任务）首次开始运行时被调用，仅执行一次。
 */
    void vApplicationDaemonTaskStartupHook( void );
#endif
```

使用需要设置宏 *configUSE_DAEMON_TASK_STARTUP_HOOK* 

我们自己定义就好。

### **2.检查**到期**定时器**

**首先**调用 *prvGetNextExpireTime* 获取下一个定时器的到期时间，函数源码如下：

```c
// 静态内部函数：获取下一个定时器的到期时间，并判断当前活动链表是否为空
// 返回值：下一个定时器的到期时间（节拍值）；若链表为空，返回0
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
    TickType_t xNextExpireTime;  // 存储下一个定时器的到期时间

    /* 定时器按到期时间排序存储，链表头部是最早到期的定时器。
     * 获取最近到期的定时器的到期时间。若没有活动定时器，
     * 则将下一个到期时间设为0，这会导致本任务在节拍计数器溢出时解除阻塞，
     * 此时时定时器链表会切换，下一个到期时间可重新评估。 */
    
    // 判断当前活动链表（pxCurrentTimerList）是否为空，结果存入输出参数
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );

    // 若链表非空（存在活动定时器）
    if( *pxListWasEmpty == pdFALSE )
    {
        // 获取链表头部节点的到期时间（即最早到期的定时器的时间）
        // listGET_ITEM_VALUE_OF_HEAD_ENTRY 宏用于提取链表头部节点的xItemValue成员
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else
    {
        /* 确保任务在节拍计数器溢出时解除阻塞（因0是最小的节拍值，必然小于等于溢出后的节拍值）。 */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    return xNextExpireTime;  // 返回下一个到期时间
}
```

获取下一个定时器的到期时间使用过读取上面我们说的定时器链表来实现的，看看链表是否为空，不为空就读取下一个定时器到期时间，为空就赋值下一个定时器的到期时间为0（这是为了之后不阻塞，继续运行守护任务）。

关于链表怎么操作，在 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 中有介绍。

**然后**处理这个到期的定时器，调用 *prvProcessTimerOrBlockTask* 源码如下：

```c
// 静态内部函数：处理已到期的定时器或阻塞定时器服务任务
// 根据下一个到期时间和链表状态，决定是处理超时还是进入阻塞状态
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime,  // 下一个定时器的到期时间
                                            BaseType_t xListWasEmpty )        // 当前活动链表是否为空（pdTRUE=空）
    {
        TickType_t xTimeNow;                  // 当前系统节拍值
        BaseType_t xTimerListsWereSwitched;   // 标记获取当前时间时是否发生了定时器链表切换（因节拍溢出）

        // 挂起所有任务调度（进入临界区，确保时间采样和链表操作的原子性）
        vTaskSuspendAll();
        {
            /* 获取当前时间，判断定时器是否已到期。
             * 若获取时间时发生了链表切换（节拍溢出），则不处理当前定时器，
             * 因为链表切换时，原链表中剩余的定时器已在prvSampleTimeNow()中处理完毕。 */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            // 情况1：未发生链表切换（无节拍溢出）
            if( xTimerListsWereSwitched == pdFALSE )
            {
                /* 节拍计数器未溢出，检查定时器是否已到期？ */
                // 若链表非空且下一个到期时间 <= 当前时间（定时器已到期）
                if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
                {
                    // 恢复任务调度（退出临界区）
                    ( void ) xTaskResumeAll();
                    // 处理已到期的定时器（触发回调、自动重载等）
                    prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
                }
                // 若定时器未到期或链表为空
                else
                {
                    /* 节拍计数器未溢出，且下一个到期时间尚未到达。
                     * 因此，本任务应阻塞等待，直到下一个到期时间或收到命令（以先到者为准）。
                     * 除非当前定时器链表为空，否则以下代码仅在xNextExpireTime > xTimeNow时执行。 */
                    
                    if( xListWasEmpty != pdFALSE )
                    {
                        /* 当前活动链表为空，检查溢出链表是否也为空？
                         * （用于后续判断是否需要无限阻塞） */
                        xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
                    }

                    // 阻塞等待命令队列消息，超时时间为（下一个到期时间 - 当前时间）
                    // 若两个链表都为空（xListWasEmpty=pdTRUE），则超时时间为portMAX_DELAY（无限阻塞）
                    vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

                    // 恢复任务调度，返回是否有更高优先级任务就绪
                    if( xTaskResumeAll() == pdFALSE )
                    {
                        /* 主动让出CPU，等待命令到达或阻塞超时。
                         * 若在临界区退出到让出CPU期间有命令到达，则让出操作不会导致任务阻塞。 */
                        taskYIELD_WITHIN_API();
                    }
                    else
                    {}
                }
            }
            // 情况2：发生了链表切换（因节拍溢出）
            else
            {
                // 恢复任务调度（退出临界区），无需额外处理（溢出时的定时器已在prvSampleTimeNow中处理）
                ( void ) xTaskResumeAll();
            }
        }
    }
```

两件事：1.获取当前时间；2. 比较定时器是否到期（当前定时器链表）

**获取当前时间：** **prvSampleTimeNow**

```c
// 静态内部函数：获取当前系统节拍时间，并检测是否发生节拍溢出（导致定时器链表切换）
// 返回值：当前系统节拍值
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )  // 输出参数：是否发生链表切换（pdTRUE=是）
    {
        TickType_t xTimeNow;  // 当前系统节拍值
        // 静态变量：保存上一次采样的节拍值（初始为0），用于检测节拍溢出
        PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U;

        // 获取当前系统节拍值（xTaskGetTickCount()返回自系统启动后的总节拍数）
        xTimeNow = xTaskGetTickCount();

        // 检查是否发生节拍溢出：当前时间 < 上一次时间（因无符号整数特性，溢出后会从最大值回到0）
        if( xTimeNow < xLastTime )
        {
            // 发生溢出：切换定时器双链表（当前链表与溢出链表交换）
            prvSwitchTimerLists();
            // 标记发生了链表切换
            *pxTimerListsWereSwitched = pdTRUE;
        }
        else
        {
            // 未发生溢出：标记未切换链表
            *pxTimerListsWereSwitched = pdFALSE;
        }

        // 更新上一次采样的节拍值，为下一次检测做准备
        xLastTime = xTimeNow;

        // 返回当前系统节拍值
        return xTimeNow;
    }
```

当前时间调用 *xTaskGetTickCount* 直接读系统的全局计数 *xTickCount* 就好了（不给出源码了，就是赋值）

检查溢出比较当前时间 *xTickCount* 和上一次时间  *xLastTime* 就好，溢出时要切换定时器链表（本文上面提到的两个链表）。之前除了两个链表还有两个链表的指针，这里交换一下指针指向就好（不给出源码了）。

**比较定时器是否到期：** *( xListWasEmpty == pdFALSE )&&( xNextExpireTime <= xTimeNow )*

若链表非空且下一个到期时间 <= 当前时间（定时器已到期），那就要处理到期的定时器了，调用*prvProcessExpiredTimer：*

```c
// 静态内部函数：处理已到期的定时器（触发回调并根据模式决定是否重载）
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime,  // 定时器的到期时间（节拍值）
                                        const TickType_t xTimeNow )     // 当前系统节拍值
    {
        // 获取当前活动链表头部的定时器（链表按到期时间排序，头部是最早到期的定时器）
        // listGET_OWNER_OF_HEAD_ENTRY 宏返回链表项的所有者（即定时器结构体指针）
        Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* 从活动定时器链表中移除该定时器。
         * 调用此函数前已确保链表不为空（由调用者检查）。 */
        // uxListRemove 移除链表项并返回剩余节点数（返回值未使用，强制转换为void避免警告）
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );

        /* 若定时器是自动重载模式，则计算下一次到期时间并重新插入活动链表。 */
        // 检查状态位中的“自动重载”标志（tmrSTATUS_IS_AUTORELOAD）
        if( ( pxTimer->ucStatus & tmrSTATUS_IS_AUTORELOAD ) != 0U )
        {
            // 调用prvReloadTimer处理重载逻辑（计算新到期时间并插入链表）
            prvReloadTimer( pxTimer, xNextExpireTime, xTimeNow );
        }
        else
        {
            // 单次触发模式：清除“活动”状态位（tmrSTATUS_IS_ACTIVE），标记为已停止
            pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
        }

        /* 调用定时器回调函数（通知应用层定时器已超时） */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );  // 传入定时器句柄作为参数
    }
```

这里把定时器从定时器链表里拿出来，然后执行相应的定时器任务。如果定时器时自动重装载模式，那就重新计算唤醒时间，放回定时器链表里。这就处理完了到期的定时器，继续去处理定时器命令。

那如果没有到期的定时器呢？（我把上面相关代码拿过来）

```c
/* 节拍计数器未溢出，且下一个到期时间尚未到达。
 * 因此，本任务应阻塞等待，直到下一个到期时间或收到命令（以先到者为准）。
 * 除非当前定时器链表为空，否则以下代码仅在xNextExpireTime > xTimeNow时执行。 */
                    
if( xListWasEmpty != pdFALSE )
{
    /* 当前活动链表为空，检查溢出链表是否也为空？
     * （用于后续判断是否需要无限阻塞） */
    xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
}

// 阻塞等待命令队列消息，超时时间为（下一个到期时间 - 当前时间）
// 若两个链表都为空（xListWasEmpty=pdTRUE），则超时时间为portMAX_DELAY（无限阻塞）
vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

// 恢复任务调度，返回是否有更高优先级任务就绪
if( xTaskResumeAll() == pdFALSE )
{
     /* 主动让出CPU，等待命令到达或阻塞超时。
      * 若在临界区退出到让出CPU期间有命令到达，则让出操作不会导致任务阻塞。 */
     taskYIELD_WITHIN_API();
}
else
{}
```

判断是否完全无定时器（检查定时器的溢出链表）->计算阻塞时长->进入阻塞等待

*vQueueWaitForMessageRestricted*  将定时器守护任务放到定时器队列等待命令链表里，然后看看有没有其他任务要执行，进行上下文切换。

![img](https://picx.zhimg.com/80/v2-758b4dc59cab31e2cf9b335078f1e493_1440w.png?source=ccfced1a)





 

实际的队列和链表结构没这么简单 ，详见 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) [FreeRTOS之队列（源码）](./FreeRTOS之队列（源码）.md)

### **3. 处理定时器命令** *prvProcessReceivedCommands*

```c
// 静态内部函数：处理定时器命令队列中所有接收到的命令（如启动、停止、重置定时器等）
static void prvProcessReceivedCommands( void )
{
    DaemonTaskMessage_t xMessage = { 0 };  // 存储从命令队列接收的消息
    Timer_t * pxTimer;                     // 指向命令对应的定时器结构体
    BaseType_t xTimerListsWereSwitched;    // 标记采样时间时是否发生链表切换（未使用，仅为函数调用参数）
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

                /* 执行挂起的回调函数，传入预设参数 */
                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }else{}
        }
        #endif /* INCLUDE_xTimerPendFunctionCall */

        /* 正的消息ID表示是定时器命令（如启动、停止、修改周期等） */
        if( xMessage.xMessageID >= ( BaseType_t ) 0 )
        {
            // 从消息中获取定时器参数，指向目标定时器结构体
            pxTimer = xMessage.u.xTimerParameters.pxTimer;

            // 检查定时器是否已在某个链表中（若在，则先移除，避免重复管理）
            if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
            {
                /* 定时器在链表中，将其从链表移除（返回值未使用，强制转换为void避免警告） */
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
                    /* 启动/重置定时器：标记为活动状态，计算新到期时间并插入活动链表 */
                    pxTimer->ucStatus |= ( uint8_t ) tmrSTATUS_IS_ACTIVE;

                    // 调用prvInsertTimerInActiveList插入链表，判断是否需要立即处理超时
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
                    /* 定时器已从链表中移除（前面的代码），此处仅需清除活动状态 */
                    pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
                    break;

                // 命令4：修改定时器周期（从任务或中断中发送）
                case tmrCOMMAND_CHANGE_PERIOD:
                case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR:
                    /* 修改周期：标记为活动状态，更新周期值，重新插入活动链表 */
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
                        /* 定时器已从链表中移除，若为动态分配，释放内存；否则仅清除活动状态 */
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
```

在说命令前，我们首先看一下这个消息结构体。

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

都有什么命令呢？

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

接下来一个一个看这些命令都干什么

**从中断中触发执行回调函数、触发执行回调函数都一样。**

执行回调函数即可。

```c
            if( xMessage.xMessageID < ( BaseType_t ) 0 )
            {
                // 获取消息中的回调参数结构体（存储函数指针和参数）
                const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                /* 定时器通过xCallbackParameters成员请求执行回调，需确保回调函数非空 */
                configASSERT( pxCallback );

                /* 执行挂起的回调函数，传入预设参数 */
                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }
```

**启动定时器、从中断中触发启动定时器、重置定时器、从中断中触发重置定时器都一样。**

标记为活动状态，重新插入活动链表就好（把节点插入链表函数不多说了）。插入失败要处理回调函数，如果是是自动重载模式，根据新的事件重新插入链表，如果不是自动重载模式清除活动状态即可，定时器已从链表中移除（前面的代码）。

```c
/* 启动/重置定时器：标记为活动状态，计算新到期时间并插入活动链表 */
pxTimer->ucStatus |= ( uint8_t ) tmrSTATUS_IS_ACTIVE;

// 调用prvInsertTimerInActiveList插入链表，判断是否需要立即处理超时
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
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}else{}
```

*prvReloadTimer*

```c
// 静态内部函数：重载定时器（用于自动重载定时器超时后重新计算到期时间并插入活动链表）
static void prvReloadTimer( Timer_t * const pxTimer,          // 要重载的定时器结构体指针
                                TickType_t xExpiredTime,       // 定时器本次到期的系统节拍值
                                const TickType_t xTimeNow )    // 当前系统节拍值
    {
        /* 将定时器插入到下一次到期时间对应的活动链表中。
         * 若下一次到期时间已过（因系统繁忙等原因导致处理延迟），
         * 则更新到期时间、调用回调函数，并重新尝试插入。 */
        // 循环调用prvInsertTimerInActiveList，直到成功将定时器插入活动链表（返回pdFALSE）
        // 插入时的新到期时间 = 本次到期时间 + 定时器周期（xTimerPeriodInTicks）
        while( prvInsertTimerInActiveList( pxTimer, ( xExpiredTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xExpiredTime ) != pdFALSE )
        {
            /* 更新到期时间：累加一个周期（处理超时情况） */
            xExpiredTime += pxTimer->xTimerPeriodInTicks;

            pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );  // 传入定时器句柄作为参数
        }
    }
```

**停止定时器、从中断中触发停止定时器一样。**清除活动状态即可 ，定时器已从链表中移除（前面的代码）。

```c
pxTimer->ucStatus &= ( ( uint8_t ) ~tmrSTATUS_IS_ACTIVE );
```

**更改定时器周期、从中断中触发更改定时器周期都一样。**标记为活动状态，更新周期值，重新插入活动链表。

```c
pxTimer->ucStatus |= ( uint8_t ) tmrSTATUS_IS_ACTIVE;
pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;

( void ) prvInsertTimerInActiveList( pxTimer, 
                                    ( xTimeNow + pxTimer->xTimerPeriodInTicks ), 
                                     xTimeNow, 
                                     xTimeNow );
```

**删除定时器。**定时器已从链表中移除，若为动态分配，释放内存；否则仅清除活动状态 

```c
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
{
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
```