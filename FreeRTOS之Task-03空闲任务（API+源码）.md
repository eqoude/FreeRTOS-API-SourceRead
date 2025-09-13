# FreeRTOS之Task-03空闲任务（API+源码）

文章涉及内容：空闲任务是什么；空闲任务的API；空闲任务做了什么（实现）；

API：主要关注接口参数和使用

源码：给出源码->分析->总结（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

## 空闲任务是什么？

空闲任务是调度器在启动时自动创建的一个任务，它通常执行删除任务操作、调用用户定义的钩子函数、进入低功耗模式，它的优先级是最低的。

> 任务删除不是调用*vTaskDelete*就删除了吗？ 这可不一定啊，如果任务正在运行或即将触发上下文切换（就是切换到其他任务），是无法立即删除的，需交由空闲任务后续处理。

## 空闲任务的API

其实关于空闲任务，没有什么需要我们直接调用的，不过FreeRTOS里有一个接口 *vApplicationIdleHook* 供我们使用，怎么用呢，看看FreeRTOS是怎么定义的这个函数的吧，

**vApplicationIdleHook**

```c
#if ( configUSE_IDLE_HOOK == 1 )

/**
 * task.h
 * @code{c}
 * void vApplicationIdleHook( void );
 * @endcode
 *
 * 应用空闲钩子函数（vApplicationIdleHook）由空闲任务（idle task）调用。
 * 这使得应用设计者能够添加后台功能，且无需额外任务的开销。
 * 注意：在任何情况下，vApplicationIdleHook () 都不得调用可能导致阻塞（block）的函数。
 */
    /* MISRA Ref 8.6.1 [External linkage] */
    /* More details at: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-86 */
    /* coverity[misra_c_2012_rule_8_6_violation] */
    void vApplicationIdleHook( void );

#endif
```

只有一个声明，自己定义即可。（仔细看一下注释）

## 空闲任务做了什么（源码实现）

（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

### 空闲任务的创建

```c
// 静态函数：创建FreeRTOS的空闲任务（每个核心对应一个空闲任务，调度器启动前必须创建）
static BaseType_t prvCreateIdleTasks( void )
{
    BaseType_t xReturn = pdPASS;          // 函数返回值（pdPASS表示创建成功，pdFAIL表示失败）
    BaseType_t xCoreID;                   // 核心ID（用于遍历所有核心，为每个核心创建空闲任务）
    char cIdleName[ configMAX_TASK_NAME_LEN ];  // 空闲任务名称缓冲区（长度由配置宏定义）
    TaskFunction_t pxIdleTaskFunction = NULL;   // 指向空闲任务函数的指针（单核/多核函数不同）
    BaseType_t xIdleTaskNameIndex;        // 空闲任务名称的索引（用于拼接名称后缀）

    // 第一步：初始化空闲任务的基础名称（从配置宏configIDLE_TASK_NAME复制）
    for( xIdleTaskNameIndex = ( BaseType_t ) 0; xIdleTaskNameIndex < ( BaseType_t ) configMAX_TASK_NAME_LEN; xIdleTaskNameIndex++ )
    {
        // 将配置的空闲任务名称（如"IDLE"）逐字符复制到缓冲区
        cIdleName[ xIdleTaskNameIndex ] = configIDLE_TASK_NAME[ xIdleTaskNameIndex ];

        /* 若名称提前结束（遇到字符串结束符'\0'），则停止复制
         * 避免复制超出名称长度的无效内存（极端情况下内存不可访问） */
        if( cIdleName[ xIdleTaskNameIndex ] == ( char ) 0x00 )
        {
            break;
        }
        else
        {}
    }

    /* 为每个核心创建空闲任务，空闲任务优先级固定为最低（tskIDLE_PRIORITY，默认0） */
    // 第二步：遍历所有核心，为每个核心创建对应的空闲任务
    for( xCoreID = ( BaseType_t ) 0; xCoreID < ( BaseType_t ) configNUMBER_OF_CORES; xCoreID++ )
    {
        // 选择当前核心对应的空闲任务函数（单核与多核不同）
        #if ( configNUMBER_OF_CORES == 1 )  // 单核系统
        {
            // 单核仅需一个空闲任务，函数为prvIdleTask（处理内存回收等核心逻辑）
            pxIdleTaskFunction = prvIdleTask;
        }
        #else /* 多核系统（configNUMBER_OF_CORES > 1） */
        #endif /* configNUMBER_OF_CORES == 1 */

        // 多核系统：为空闲任务名称添加核心编号后缀（如"IDLE0"、"IDLE1"），区分不同核心的空闲任务
        #if ( configNUMBER_OF_CORES > 1 )
        #endif /* configNUMBER_OF_CORES > 1 */

        // 分支1：使用静态内存分配创建空闲任务（configSUPPORT_STATIC_ALLOCATION == 1）
        #if ( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            StaticTask_t * pxIdleTaskTCBBuffer = NULL;  // 空闲任务TCB（任务控制块）的静态内存缓冲区
            StackType_t * pxIdleTaskStackBuffer = NULL;  // 空闲任务栈的静态内存缓冲区
            configSTACK_DEPTH_TYPE uxIdleTaskStackSize;  // 空闲任务栈的大小

            /* 空闲任务使用应用层提供的静态内存，先获取内存地址，再创建任务 */
            #if ( configNUMBER_OF_CORES == 1 )  // 单核系统
            {
                // 调用应用层实现的函数，获取空闲任务的TCB、栈缓冲区和栈大小
                vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &uxIdleTaskStackSize );
            }
            #else  // 多核系统
            {}
            #endif /* configNUMBER_OF_CORES == 1 */

            // 调用静态内存创建任务的API（xTaskCreateStatic），创建空闲任务
            xIdleTaskHandles[ xCoreID ] = xTaskCreateStatic( 
                pxIdleTaskFunction,    // 空闲任务函数（prvIdleTask或prvPassiveIdleTask）
                cIdleName,             // 空闲任务名称（如"IDLE0"）
                uxIdleTaskStackSize,   // 空闲任务栈大小（应用层指定）
                ( void * ) NULL,       // 任务参数（空闲任务无需参数，传NULL）
                portPRIVILEGE_BIT,     // 任务优先级（最低优先级0 + 特权模式位，确保空闲任务为特权态）
                pxIdleTaskStackBuffer, // 任务栈静态缓冲区
                pxIdleTaskTCBBuffer    // 任务TCB静态缓冲区
            );

            // 检查空闲任务是否创建成功（xTaskCreateStatic返回非NULL表示成功）
            if( xIdleTaskHandles[ xCoreID ] != NULL )
            {
                xReturn = pdPASS;  // 标记当前核心任务创建成功
            }
            else
            {
                xReturn = pdFAIL;  // 标记当前核心任务创建失败
            }
        }
        #else /* 分支2：使用动态内存分配创建空闲任务（configSUPPORT_STATIC_ALLOCATION == 0） */
        {
            /* 空闲任务使用FreeRTOS堆的动态内存，调用动态创建任务的API（xTaskCreate） */
            xReturn = xTaskCreate(
                pxIdleTaskFunction,    // 空闲任务函数
                cIdleName,             // 空闲任务名称
                configMINIMAL_STACK_SIZE,  // 空闲任务栈大小（使用配置的最小栈大小）
                ( void * ) NULL,       // 任务参数（NULL）
                portPRIVILEGE_BIT,     // 任务优先级（最低优先级0 + 特权模式位）
                &xIdleTaskHandles[ xCoreID ]  // 输出参数：存储空闲任务句柄的数组
            );
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* 若任意一个核心的空闲任务创建失败，立即退出循环（无需继续创建其他核心的任务） */
        if( xReturn == pdFAIL )
        {
            break;
        }
        else
        {
            #if ( configNUMBER_OF_CORES == 1 )  // 单核系统无需绑定核心
            {}
            #else  // 多核系统：将空闲任务绑定到对应的核心（调度器启动前完成绑定）
            #endif
        }
    }

    return xReturn;  // 返回创建结果（pdPASS表示所有核心任务创建成功，pdFAIL表示至少一个失败）
}
```

可以大概看一下注释，其实它也没有什么啊，就是获得任务名，然后创建任务就好了，接下来说一些细节：

首先，任务名是什么，源码里是这样定义的

```c
//分配给空闲任务（Idle task）的名称。可以通过在 FreeRTOSConfig.h 中定义 configIDLE_TASK_NAME 来重写此名称。
#ifndef configIDLE_TASK_NAME
    #define configIDLE_TASK_NAME    "IDLE"
#endif
```

完全可以自己顺便写一个。

然后，这里有个函数

```c
// 调用应用层实现的函数，获取空闲任务的TCB、栈缓冲区和栈大小
vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &uxIdleTaskStackSize );
```

这个好像之前没有见过，来看一看源码：

```c
    void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                        StackType_t ** ppxIdleTaskStackBuffer,
                                        configSTACK_DEPTH_TYPE * puxIdleTaskStackSize )
    {
        static StaticTask_t xIdleTaskTCB;
        static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

        *ppxIdleTaskTCBBuffer = &( xIdleTaskTCB );
        *ppxIdleTaskStackBuffer = &( uxIdleTaskStack[ 0 ] );
        *puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
    }

    #if ( configNUMBER_OF_CORES > 1 )
    #endif /* #if ( configNUMBER_OF_CORES > 1 ) */

#endif /* #if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configKERNEL_PROVIDED_STATIC_MEMORY == 1 ) && ( portUSING_MPU_WRAPPERS == 0 ) ) */
```

这里给空闲任务的TCB、栈分配了空间。

最后，就是调用 *xTaskCreate* 或者 *xTaskCreateStatic* 创建任务了。

这样空闲任务就创建好了，不过不用自己来创建，在开启调度器的时候会自动调用 *prvCreateIdleTasks* 。

那空闲任务在哪啊？ 创建任务的时候，任务名是*pxIdleTaskFunction ；pxIdleTaskFunction = prvIdleTask*  *，*源码里这样定义：

```c
static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters ) PRIVILEGED_FUNCTION;
```

*portTASK_FUNCTION_PROTO*又是什么呢，继续看：

在（ARM Cortex-M3）移植版本下是这样的

```c
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )
```

在<task.c>中可以找到

```c
 // 定义空闲任务函数（static修饰：仅在当前文件可见；portTASK_FUNCTION宏适配端口特性）
static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
   /*  任务内容   */
}
```

ok，找到了，下面就看看里面是什么吧

### 空闲任务函数

```c
// 定义空闲任务函数（static修饰：仅在当前文件可见；portTASK_FUNCTION宏适配端口特性）
static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
    /* 消除编译器未使用参数警告 */
    ( void ) pvParameters;

    /** 这是RTOS空闲任务——调度器启动时会自动创建。**/

    #if ( configNUMBER_OF_CORES > 1 )  // SMP多核心场景
    #endif /* #if ( configNUMBER_OF_CORES > 1 ) */

    // 空闲任务主循环（configCONTROL_INFINITE_LOOP()是端口无关的无限循环宏，通常为for(;;)）
    for( ; configCONTROL_INFINITE_LOOP(); )
    {
         /* 检查是否有任务自删除——若有，空闲任务需负责释放被删除任务的TCB（任务控制块）和栈内存。
         * （注：任务调用vTaskDelete()后不会立即释放资源，仅标记为“待清理”，由空闲任务异步处理） */
        prvCheckTasksWaitingTermination();

        #if ( configUSE_PREEMPTION == 0 )
        {
            /* 若禁用抢占，需主动触发任务切换以检查是否有其他任务就绪。
             * 若启用抢占，就绪任务会自动抢占CPU，无需此操作。 */
            taskYIELD();
        }
        #endif /* configUSE_PREEMPTION */

        #if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) // 启用抢占且空闲任务需让步
        {
            /* 启用抢占时，同优先级任务会按时间片调度。
             * 若有其他同优先级（空闲优先级）的任务就绪，空闲任务应在当前时间片结束前主动让步。
             *
             * 此处无需临界区：仅读取就绪链表长度，偶发的错误值不影响整体逻辑。
             * 若空闲优先级就绪链表的长度 > 核心数（每个核心对应1个空闲任务），
             * 说明存在非空闲任务就绪，需触发切换。 */
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) configNUMBER_OF_CORES )
            {
                taskYIELD();
            }
            else
            {}
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) */

        #if ( configUSE_IDLE_HOOK == 1 )
        {
            /* 在空闲任务中调用用户定义的钩子函数。
             * （注：钩子函数需轻量化，不可阻塞或调用可能导致阻塞的API，如vTaskDelay()） */
            vApplicationIdleHook();
        }
        #endif /* configUSE_IDLE_HOOK */

        /* 条件编译使用“!=0”而非“==1”，确保当用户自定义低功耗模式需将configUSE_TICKLESS_IDLE设为
         * 非1值时，仍能调用portSUPPRESS_TICKS_AND_SLEEP()。 */
        #if ( configUSE_TICKLESS_IDLE != 0 )
        {
            TickType_t xExpectedIdleTime;

            /* 不希望在空闲任务每次循环都挂起/恢复调度器，因此先在调度器未挂起时
             * 初步计算预计空闲时间（此结果可能因任务就绪而失效）。 */
            xExpectedIdleTime = prvGetExpectedIdleTime();

            // 若预计空闲时间 >= 进入低功耗前的最小空闲时间阈值
            if( xExpectedIdleTime >= ( TickType_t ) configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
            {
                vTaskSuspendAll();
                {
                    xExpectedIdleTime = prvGetExpectedIdleTime();

                    /* 若应用不希望调用portSUPPRESS_TICKS_AND_SLEEP()，可定义此宏将xExpectedIdleTime设为0。
                     * （用于自定义低功耗触发逻辑） */
                    configPRE_SUPPRESS_TICKS_AND_SLEEP_PROCESSING( xExpectedIdleTime );

                    // 再次确认预计空闲时间满足阈值
                    if( xExpectedIdleTime >= ( TickType_t ) configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
                    {
                        portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime ); // 进入低功耗模式（端口实现）
                    }
                    else
                    {}
                }
                ( void ) xTaskResumeAll();
            }
            else
            {}
        }
        #endif /* configUSE_TICKLESS_IDLE */

        #if ( ( configNUMBER_OF_CORES > 1 ) && ( configUSE_PASSIVE_IDLE_HOOK == 1 ) ) // SMP且启用被动空闲钩子
        #endif /* #if ( ( configNUMBER_OF_CORES > 1 ) && ( configUSE_PASSIVE_IDLE_HOOK == 1 ) ) */
    }
}
```

空闲任务主要干了四件事：任务删除，切换任务（上下文切换），执行用户自定义操作，低功耗模式

第一件事：任务删除

```c
         /* 检查是否有任务自删除——若有，空闲任务需负责释放被删除任务的TCB（任务控制块）和栈内存。
         * （注：任务调用vTaskDelete()后不会立即释放资源，仅标记为“待清理”，由空闲任务异步处理） */
        prvCheckTasksWaitingTermination();
```

下面看看函数源码：

```c
// 检查并清理“待终止任务链表”中的任务（释放TCB和栈内存）
static void prvCheckTasksWaitingTermination( void )
{
     /** 此函数由RTOS空闲任务调用 **/

    #if ( INCLUDE_vTaskDelete == 1 )
    {
        TCB_t * pxTCB;

        /* uxDeletedTasksWaitingCleanUp用于记录待清理任务数量，避免在空闲任务中过于频繁地进入临界区 */
        while( uxDeletedTasksWaitingCleanUp > ( UBaseType_t ) 0U )
        {
            #if ( configNUMBER_OF_CORES == 1 )
            {
                taskENTER_CRITICAL();
                {
                    {
                        // 从“待终止任务链表”头部获取第一个待清理任务的TCB
                        pxTCB = listGET_OWNER_OF_HEAD_ENTRY( ( &xTasksWaitingTermination ) );
                        // 将该任务从链表中移除（xStateListItem是任务在状态链表中的节点）
                        ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                        --uxCurrentNumberOfTasks;  // 系统总任务数减1
                        --uxDeletedTasksWaitingCleanUp;  // 待清理任务数减1
                    }
                }
                taskEXIT_CRITICAL();

                prvDeleteTCB( pxTCB );  // 释放TCB和栈内存（核心清理函数）
            }
            #else /* #if( configNUMBER_OF_CORES == 1 ) */
            #endif /* 多核已经省略 */
        }
    }
    #endif /* INCLUDE_vTaskDelete */
}
```

[FreeRTOS之Task-01任务创建（API+源码）](./FreeRTOS之Task-01任务创建（API+源码）.md) 里有提到过FreeRTOS里有很多链表，当调用*vTaskDelete* 去删除一个任务时，当时可能因为一些情况删不了，那就会把任务放到“待终止任务链表”里（ xTasksWaitingTermination），那空闲任务从这个链表去找到任务后完成删除就好了。

现在来看上面的代码，*uxDeletedTasksWaitingCleanUp* 是一个全局变量，记录了待清理任务数量，有任务就开始清除，把任务从链表中移除，调用 *prvDeleteTCB* 释放任务空间就好。

至于这两个关于链表的函数，之前 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 提到过了。

到这，就完成了任务的删除。

第二件事：切换任务（上下文切换）

```c
        #if ( configUSE_PREEMPTION == 0 )
        {
            /* 若禁用抢占，需主动触发任务切换以检查是否有其他任务就绪。
             * 若启用抢占，就绪任务会自动抢占CPU，无需此操作。 */
            taskYIELD();
        }
        #endif /* configUSE_PREEMPTION */

        #if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) // 启用抢占且空闲任务需让步
        {
            /* 启用抢占时，同优先级任务会按时间片调度。
             * 若有其他同优先级（空闲优先级）的任务就绪，空闲任务应在当前时间片结束前主动让步。
             *
             * 此处无需临界区：仅读取就绪链表长度，偶发的错误值不影响整体逻辑。
             * 若空闲优先级就绪链表的长度 > 核心数（每个核心对应1个空闲任务），
             * 说明存在非空闲任务就绪，需触发切换。 */
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) configNUMBER_OF_CORES )
            {
                taskYIELD();
            }
            else
            {}
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) */
```

这里涉及到任务调度相关，本文不多说（之后的文章说明），简单说就是判断是否有其他任务要运行，有的话把CPU让出来。

第三件事：执行用户自定义操作

```c
        #if ( configUSE_IDLE_HOOK == 1 )
        {
            /* 在空闲任务中调用用户定义的钩子函数。
             * （注：钩子函数需轻量化，不可阻塞或调用可能导致阻塞的API，如vTaskDelay()） */
            vApplicationIdleHook();
        }
        #endif /* configUSE_IDLE_HOOK */
```

第三件事：低功耗模式

判断时间是否充足，太频繁的进入出来低功耗模式也不太好，关于低功耗模式这里也不多说，有机会后面分析低功耗模式再说。

**结尾：**

空闲任务是调度器在启动时自动创建，它做了三件事任务删除，执行用户自定义操作，进入低功耗模式。我们可以定义 *vApplicationIdleHook* 函数去完成我们想在空闲任务里做的操作。