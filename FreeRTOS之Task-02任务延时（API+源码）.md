# FreeRTOS之Task-02任务延时（API+源码）

文章涉及内容：任务延时API介绍及使用示例、任务延时源码分析（延时怎么实现的、延时的两种实现）

API：主要关注接口参数和使用

源码：给出源码->分析->总结（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

### *(API) vTaskDelay* 任务延时(相对延时)

```c
// 声明 vTaskDelay 函数，PRIVILEGED_FUNCTION 表示仅能在特权模式下调用
void vTaskDelay( const TickType_t xTicksToDelay ) PRIVILEGED_FUNCTION;
/* 使用示例：
 *
 * void vTaskFunction( void * pvParameters )
 * {
 * // 计算500毫秒对应的节拍数（portTICK_PERIOD_MS为每个节拍的毫秒数）
 * const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
 *
 *   for( ;; )
 *   {
 *       // 每500毫秒翻转一次LED，两次翻转间阻塞任务
 *       vToggleLED();
 *       vTaskDelay( xDelay );
 *   }
 * }
 */
```

只有一个参数 *xTicksToDelay  // 延时时间/阻塞时间*

它的数据类型是 *TickType_t*  定义如下（我用的是ARM CM3（ARM Cortex-M3）移植版本）

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

默认是64位，但是ARM CM3（ARM Cortex-M3）移植版本 是不支持的，使用前要改成16或32位

它的使用大概就是上面的示例那样子，不过源码的注释有这样一段话，建议好好读一下

> vTaskDelay() 指定的是“相对阻塞时间”——即从调用该函数的时刻起，延迟指定节拍后解除阻塞。 例如，指定阻塞100个节拍，任务会在调用 vTaskDelay() 后的第100个节拍时解除阻塞。 因此，vTaskDelay() 不适合用于控制周期性任务的执行频率：因为代码执行路径、其他任务或中断活动， 都会影响 vTaskDelay() 的调用间隔，进而导致任务下次执行时间不确定。 若需实现固定频率的周期性任务，建议使用 xTaskDelayUntil()——该函数通过指定“绝对唤醒时间”（而非相对时间），确保任务按固定周期执行。

*xTaskDelayUntil*  怎么用，下面就介绍。

### *(API) xTaskDelayUntil*  任务延时（固定周期）

```
// 声明 xTaskDelayUntil 函数，支持返回阻塞状态
BaseType_t xTaskDelayUntil( TickType_t * const pxPreviousWakeTime,
                            const TickType_t xTimeIncrement ) PRIVILEGED_FUNCTION;
/* 使用示例：
 * @code{c}
 * // 每10个节拍执行一次任务（固定周期）
 * void vTaskFunction( void * pvParameters )
 * {
 * TickType_t xLastWakeTime;  // 存储上一次唤醒时间
 * const TickType_t xFrequency = 10;  // 任务周期（10个节拍）
 * BaseType_t xWasDelayed;    // 记录任务是否被实际阻塞
 *
 *     // 首次使用前，初始化 xLastWakeTime 为当前系统时间
 *     xLastWakeTime = xTaskGetTickCount ();
 *     for( ;; )
 *     {
 *         // 阻塞到下一个周期时间点（xLastWakeTime + xFrequency）
 *         xWasDelayed = xTaskDelayUntil( &xLastWakeTime, xFrequency );
 *
 *         // 此处执行周期性任务逻辑。xWasDelayed 可用于判断任务是否超时：
 *         // 若 xWasDelayed 为 pdFALSE，说明上一轮执行耗时超过周期，导致唤醒时间已过期
 *     }
 * }
 */
```

官方文档里是这么说的：

> vTaskDelay() 会 导致一个任务从调用 vTaskDelay() 起阻塞指定的滴答数，  而 xTaskDelayUntil() 将导致一个任务从 pxPreviousWakeTime 参数中指定的时间起阻塞指定的滴答数。

这是什么意思？ vTaskDelay 就是说任务运行到 vTaskDelay 的时候，任务阻塞，停下来让其他需要运行的任务运行，等 xTicksToDelay（vTaskDelay 的参数）个时间。 xTaskDelayUntil 就是说任务从 pxPreviousWakeTime（第一个参数）开始（这里可不是运行到xTaskDelayUntil 的时候）阻塞，等xTimeIncrement （第二个参数）个时间。

第二个参数还好说，和 vTaskDelay 的参数差不多一个意思，指等待的时间。第一个参数指的是某一个时刻，这个时间我们怎么赋值呢？看上面的使用示例，可以用*xTaskGetTickCount* 函数获得当前时间作为基准。

下面有两幅图，说明两种延时的区别：

![img](https://picx.zhimg.com/80/v2-b3d899b56718d805b501eda69860a611_1440w.png?source=ccfced1a)

![image-20250913123306117](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913123306117.png)

要注意，系统的运行不一定是这样的，和调度策略还有任务优先级有关（调度策略本文不涉及，之后有一章去说明）。idle是空闲任务，开启调度器时系统会自动创建空闲任务（空闲是干什么的？之后会有一章去说明)。

### *(源码) vTaskDelay* 任务延时 (相对延时)

（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

```c
// 仅当启用 vTaskDelay 功能时，定义该函数
#if ( INCLUDE_vTaskDelay == 1 )

    // 函数：将当前任务阻塞指定的节拍数（相对延迟）
    void vTaskDelay( const TickType_t xTicksToDelay )
    {
        BaseType_t xAlreadyYielded = pdFALSE;  // 标记调度器恢复时是否已触发上下文切换

        /* 若延迟节拍数为0，仅触发一次任务调度（不阻塞任务） */
        if( xTicksToDelay > ( TickType_t ) 0U )
        {
            // ===================== 模块1：挂起调度器，保护延迟链表操作 =====================
            vTaskSuspendAll();  // 挂起调度器（禁止其他任务修改任务列表，避免并发冲突）
            {

                /* 关键说明：
                 * 调度器挂起期间，从事件列表中移除的任务，需等到调度器恢复后才会被加入就绪链表/移出阻塞链表；
                 * 当前任务是正在执行的任务，不可能处于任何事件链表中（无需处理事件链表移除）。 */
                // 将当前任务加入“延迟链表”（xDelayedTaskList1 或 xDelayedTaskList2），阻塞 xTicksToDelay 个节拍
                prvAddCurrentTaskToDelayedList( xTicksToDelay, pdFALSE );
            }
            // 恢复调度器，返回值 xAlreadyYielded 表示恢复时是否已触发上下文切换
            xAlreadyYielded = xTaskResumeAll();
        }
        else
        { }

        // ===================== 模块2：确保任务调度（若未自动触发） =====================
        /* 若调度器恢复时未触发上下文切换（xAlreadyYielded == pdFALSE），
         * 需手动触发调度——因为当前任务可能已被加入延迟链表，需切换到其他就绪任务 */
        if( xAlreadyYielded == pdFALSE )
        {
            taskYIELD_WITHIN_API();  // 在API内部触发上下文切换
        }
        else
        { }
    }

#endif /* INCLUDE_vTaskDelay */
```

可以分为两步：第一步，将当前任务加入“延迟链表”；第二步，上下文切换（因为可能当前运行的任务需要从就绪放到延迟链表）

有关上下文切换，还有调度器相关的内容本文不会讨论，再后续的调度器章节再说明，所以这里我们关注怎么把将当前任务加入“延迟链表”就好了。

将当前任务加入“延迟链表”使用了一个函数 *prvAddCurrentTaskToDelayedList* 下面是它的源码：

```c
/* 静态函数：将当前任务添加到延迟链表或挂起链表
 * 参数1：xTicksToWait - 任务阻塞的超时时间（单位：系统节拍，portMAX_DELAY=永久阻塞）
 * 参数2：xCanBlockIndefinitely - 是否允许永久阻塞（pdTRUE=允许，pdFALSE=仅允许超时阻塞）
 * 功能：根据阻塞类型和节拍溢出状态，将任务从就绪链表移至对应阻塞链表，实现超时等待 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait,
                                            const BaseType_t xCanBlockIndefinitely )
{
    TickType_t xTimeToWake;
    const TickType_t xConstTickCount = xTickCount;
    List_t * const pxDelayedList = pxDelayedTaskList;
    List_t * const pxOverflowDelayedList = pxOverflowDelayedTaskList;

    // 若启用了“中止延迟”功能（INCLUDE_xTaskAbortDelay == 1）
    #if ( INCLUDE_xTaskAbortDelay == 1 )
    {
             /* 暂时不关注此功能 */
    }
    #endif

    /* 第一步：先将任务从当前链表中移除
     * 原因：任务的xStateListItem链表项既用于就绪链表，也用于阻塞链表，不能同时存在于两个链表 */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
    }
    else
    { }

    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        /* 判断是否满足“永久阻塞”条件：
         * 1. 超时时间为portMAX_DELAY（表示永久阻塞）；
         * 2. 允许永久阻塞（xCanBlockIndefinitely == pdTRUE） */
        if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
        {
            /* 满足永久阻塞条件：将任务添加到“挂起任务链表”
             * 挂起链表中的任务不会被超时唤醒，只能通过vTaskResume()等函数手动唤醒 */
            listINSERT_END( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* 不满足永久阻塞条件：按超时阻塞处理，计算任务唤醒时间 */
            xTimeToWake = xConstTickCount + xTicksToWait;

            /* 设置任务链表项的值为“唤醒时间”
             * 延迟链表按“唤醒时间升序”排序，确保最早唤醒的任务排在链表头部，便于调度器快速查找 */
            listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

            /* 判断唤醒时间是否溢出：
             * 若xTimeToWake < xConstTickCount，说明系统节拍计数溢出（如uint32_t从0xFFFFFFFF加1变为0） */
            if( xTimeToWake < xConstTickCount )
            {
                /* 唤醒时间溢出：将任务添加到“延迟溢出链表”
                 * 溢出链表专门处理节拍溢出后的超时任务，避免与未溢出任务的唤醒逻辑冲突 */
                vListInsert( pxOverflowDelayedList, &( pxCurrentTCB->xStateListItem ) );
            }
            else
            {
                /* 唤醒时间未溢出：将任务添加到“正常延迟链表” */
                traceMOVED_TASK_TO_DELAYED_LIST();
                vListInsert( pxDelayedList, &( pxCurrentTCB->xStateListItem ) );

                /* 若当前任务的唤醒时间早于“下一个任务唤醒时间”（xNextTaskUnblockTime）：
                 * 更新xNextTaskUnblockTime为当前任务的唤醒时间
                 * 目的：调度器可通过xNextTaskUnblockTime快速确定下一次需要唤醒任务的时间，减少轮询开销 */
                if( xTimeToWake < xNextTaskUnblockTime )
                {
                    xNextTaskUnblockTime = xTimeToWake;
                }
                else
                {}
            }
        }
    }
    #else /* INCLUDE_vTaskSuspend */
    {
        /* 未启用挂起功能，只能按超时阻塞处理，计算唤醒时间 */
        xTimeToWake = xConstTickCount + xTicksToWait;

        /* 设置链表项值为唤醒时间，用于延迟链表排序 */
        listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

        /* 判断唤醒时间是否溢出，分发到对应延迟链表 */
        if( xTimeToWake < xConstTickCount )
        {
            /* Wake time has overflowed.  Place this item in the overflow list. */
            vListInsert( pxOverflowDelayedList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* The wake time has not overflowed, so the current block list is used. */
            vListInsert( pxDelayedList, &( pxCurrentTCB->xStateListItem ) );

            /* If the task entering the blocked state was placed at the head of the
             * list of blocked tasks then xNextTaskUnblockTime needs to be updated
             * too. */
            if( xTimeToWake < xNextTaskUnblockTime )
            {
                xNextTaskUnblockTime = xTimeToWake;
            }
            else
            {}
        }
    }
    #endif /* INCLUDE_vTaskSuspend */
}
```

这里分类两种情况，一种是单纯延时（不允许挂起任务），第二种是允许任务是挂起状态

我们先来看看第一种情况（不支持把任务挂起）

第一步，把任务从当前链表中拿出来（把任务拿出来，才能放到其他链表啊）

```
    /* 第一步：先将任务从当前链表中移除
     * 原因：任务的xStateListItem链表项既用于就绪链表，也用于阻塞链表，不能同时存在于两个链表 */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
    }
```

拿出任务使用了 *uxListRemove* 函数（[FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 中有说明此函数），至于if里面的函数在默认情况下其实没有什么用，默认是个空的宏，无定义。

>  \#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )

第一步，把任务放到延迟链表里（有两个延迟链表，放到哪个里面呢？）

```c
       /* 未启用挂起功能，只能按超时阻塞处理，计算唤醒时间 */
        xTimeToWake = xConstTickCount + xTicksToWait;

        /* 设置链表项值为唤醒时间，用于延迟链表排序 */
        listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

        /* 判断唤醒时间是否溢出，分发到对应延迟链表 */
        if( xTimeToWake < xConstTickCount )
        {
            /* Wake time has overflowed.  Place this item in the overflow list. */
            vListInsert( pxOverflowDelayedList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* The wake time has not overflowed, so the current block list is used. */
            vListInsert( pxDelayedList, &( pxCurrentTCB->xStateListItem ) );

            /* If the task entering the blocked state was placed at the head of the
             * list of blocked tasks then xNextTaskUnblockTime needs to be updated
             * too. */
            if( xTimeToWake < xNextTaskUnblockTime )
            {
                xNextTaskUnblockTime = xTimeToWake;
            }
            else
            {}
        }
```

首先，要设置链表项值为唤醒时间，用于延迟链表排序（就是要把需要延时多长时间记录下来，毕竟后面还要唤醒，我们的知道它什么时候醒来）

我们看看这个数是怎么算的： *xTimeToWake = xConstTickCount + xTicksToWait*

*xTicksToWait* 就是 *vTaskDelay(x)* 的x，是我们要延迟多久。

*xConstTickCount = xTickCount*

这个 *xTickCount* 是一个全局变量，它的值就是系统当前的时间。

那我们链表里要记录唤醒的值就是 当前时间+我们要延迟的时间

然后，我们要判断唤醒的时间是否溢出

*vListInsert( pxOverflowDelayedList,&( pxCurrentTCB->xStateListItem ))* 溢出就放到溢出延迟链表

*vListInsert( pxDelayedList,&( pxCurrentTCB->xStateListItem ))* 未溢出就放到当前延迟链表

什么是溢出呢？ 我们记录系统时间的全局变量 *xTickCount*  它当然是有数据类型的，能存储的值有限的，可以肯定会溢出，溢出了那就从0 开始呗，那怎么记录系统时间的，其实还有一个全局变量记录溢出次数，这样系统时间就能完整记录下来了，记录溢出次数的全局变量当然也会溢出，不过这个时间应该是挺长的。

这就看出两个延迟链表的用处了，一个存当前*xTickCount* 没有溢出之前要唤醒的任务，一个存溢出以后要唤醒的任务。

最后，处理未溢出的时候还有几行代码：

```c
            if( xTimeToWake < xNextTaskUnblockTime )
            {
                xNextTaskUnblockTime = xTimeToWake;
            }
```

*xNextTaskUnblockTime* 这是下一个被延迟的任务要唤醒的时间，如果现在这个唤醒时间比上一次还要快，那就更新一下。

那 *vTaskDelay*   我们就看完了，当然还有一些东西没有提到，这些不是本文要关注的内容，之后的文章会提到。

### *(源码) xTaskDelayUntil*  任务延时（固定周期）

```c
// 仅当启用 xTaskDelayUntil 功能时，定义该函数
#if ( INCLUDE_xTaskDelayUntil == 1 )

    // 函数：将任务阻塞到指定的绝对时间（确保周期性执行）
    BaseType_t xTaskDelayUntil( TickType_t * const pxPreviousWakeTime,  // 指向“上一次唤醒时间”的指针
                                const TickType_t xTimeIncrement )       // 任务周期（单位：系统节拍数）
    {
        TickType_t xTimeToWake;                // 本次目标唤醒时间（绝对时间）
        BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE;  // xShouldDelay：标记是否需要阻塞任务

        // 挂起调度器（禁止任务切换，确保时间计算的原子性）
        vTaskSuspendAll();
        {
            /* 优化：当前代码块内，系统节拍计数（xTickCount）不会变化（因调度器已挂起） */
            const TickType_t xConstTickCount = xTickCount;  // 存储当前系统节拍数

            /* 计算本次目标唤醒时间 = 上一次唤醒时间 + 周期（确保固定周期） */
            xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

            // ===================== 核心逻辑：判断是否需要阻塞任务 =====================
            if( xConstTickCount < *pxPreviousWakeTime )
            {
                /* 场景1：系统节拍计数（xTickCount）已溢出（如从最大值回到0）
                 * 仅当“目标唤醒时间也溢出”且“目标唤醒时间 > 当前节拍数”时，才需要阻塞
                 * （此时相当于两者均未溢出，目标时间在未来） */
                if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xConstTickCount ) )
                {
                    xShouldDelay = pdTRUE;  // 需要阻塞
                }
                else
                {}
            }
            else
            {
                /* 场景2：系统节拍计数未溢出
                 * 若“目标唤醒时间溢出”（因加上周期后超过最大值）或“目标唤醒时间 > 当前节拍数”，则需要阻塞 */
                if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xConstTickCount ) )
                {
                    xShouldDelay = pdTRUE;  // 需要阻塞
                }
                else
                {}
            }

            /* 更新“上一次唤醒时间”为本次目标唤醒时间（供下次调用使用） */
            *pxPreviousWakeTime = xTimeToWake;

            // 若需要阻塞，将当前任务加入延迟链表
            if( xShouldDelay != pdFALSE )
            {
                /* prvAddCurrentTaskToDelayedList 需要“阻塞时长”而非“目标唤醒时间”，
                 * 因此用目标时间减去当前节拍数（xTimeToWake - xConstTickCount） */
                prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
            }
            else
            {}
        }
        // 恢复调度器，返回值表示恢复时是否已触发上下文切换
        xAlreadyYielded = xTaskResumeAll();

        /* 若调度器恢复时未触发切换，手动触发一次（确保任务切换到其他就绪任务） */
        if( xAlreadyYielded == pdFALSE )
        {
            taskYIELD_WITHIN_API();
        }
        else
        {}
        return xShouldDelay;  // 返回是否实际阻塞了任务
    }

#endif /* INCLUDE_xTaskDelayUntil */
```

这里其实和 *vTaskDelay* 一样，都是把任务放到延迟列表里，连函数都是一样的。不过，这里再加入链表前多了一些东西，看不懂源码，那就先看看下面的图吧。

![image-20250913123431568](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913123431568.png)

![image-20250913123455257](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913123455257.png)

看了这两幅图，就可以看懂上面的那段代码了。

**结尾：**

关于延迟函数，其实还有啦，比如 *xTaskAbortDelay*  不过他不在我的讨论范围内，我想知道上面两个应该就够了吧。