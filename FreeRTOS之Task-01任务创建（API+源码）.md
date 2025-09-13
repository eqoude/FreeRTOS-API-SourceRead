# FreeRTOS之Task-01任务创建（API+源码）

文章涉及内容：任务创建API介绍及使用示例、任务源码分析（任务在内存中是什么样的，任务初始化后是什么样，任务创建都做了哪些工作）

API：主要关注接口参数和使用

源码：给出源码->分析->总结（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

## **任务创建相关API**

### *(API)xTaskCreate*  动态分配创建任务

```c
// 创建一个新任务（动态分配）    
BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,                       //任务入口函数的指针
                            const char * const pcName,                   //任务的描述性名称，主要用于调试
                            const configSTACK_DEPTH_TYPE uxStackDepth,   //任务栈的大小，以栈能容纳的变量数量表示，而非字节数
                            void * const pvParameters,                   //指向将作为所创建任务参数的指针
                            UBaseType_t uxPriority,                      //任务运行的优先级
                            TaskHandle_t * const pxCreatedTask ) PRIVILEGED_FUNCTION;   //用于传回一个句柄，通过该句柄可以引用所创建的任务
/* 使用示例：
 * // 要创建的任务
 * void vTaskCode( void * pvParameters )
 * {
 *   for( ;; )
 *   {
 *       // 任务代码放在这里
 *   }
 * }
 *
 * // 创建任务的函数
 * void vOtherFunction( void )
 * {
 * static uint8_t ucParameterToPass;
 * TaskHandle_t xHandle = NULL;
 *
 *   // 创建任务，存储句柄。注意，传递的参数ucParameterToPass
 *   // 必须在任务的生命周期内存在，因此在这种情况下被声明为static。
 *   // 如果它只是一个自动栈变量，那么当新任务尝试访问它时，它可能已经不存在，或者至少已被损坏。
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, &ucParameterToPass, tskIDLE_PRIORITY, &xHandle );
 *   configASSERT( xHandle );
 *
 *   // 使用句柄删除任务
 *   if( xHandle != NULL )
 *   {
 *      vTaskDelete( xHandle );
 *   }
 * }
 */
```

仿照上面的示例就可以创建一个任务了，这里说一下任务要注意的参数：

*TaskFunction_t pxTaskCode  //任务入口函数的指针*

> 看了上面的代码你应该知道此参数就是一个任务指针，用的时候写个函数，然后把函数名放过来就好了，但是这里参数的数据类型是 TaskFunction_t ，它在<projdefs.h>中，定义如下 
>
> *typedef void (\* TaskFunction_t)( void \* arg );* 
>
> 这是一个带有 *void ** 类型的参数、无返回值的函数指针，这说明任务函数定义必须使用这样的格式。

*const configSTACK_DEPTH_TYPE uxStackDepth  //任务栈的大小，以栈能容纳的变量数量表示，而非字节数*

> 我们不是动态分配吗，这个栈是怎么回事？ 这个栈用来储存函数中的变量，关于一个任务要占用多少内存，在内存中是什么样子，下面再说明 configSTACK_DEPTH_TYPE *栈的数据类型：* 
>
> #define     configSTACK_DEPTH_TYPE     size_t 
>
> size_t是什么？ 默认情况下的定义如下（FreeRTOS.h） 
>
> #ifndef portPOINTER_SIZE_TYPE      
> 	#define portPOINTER_SIZE_TYPE uint32_t 
> #endif 
>
> 使用者可以自己定义它的大小

*UBaseType_t uxPriority    //任务运行的优先级*

> *UBaseType_t是什么？* 我使用的移植版本是（ARM Cortex-M3），在<portmacro.h>中定义如下 
>
> *typedef unsigned long UBaseType_t;*

*TaskHandle_t \*const   pxCreatedTask   //任务句柄，通过该句柄可以引用所创建的任务*

> 这个句柄有什么用，从上面的示例可以看到，删除任务时，传入句柄删除 *TaskHandle_t* 是什么类型呢？（<Task.h>里有说明） struct   tskTaskControlBlock;    // 一个任务TCB控制块，之后源码会说明TCB长什么样子 
> typedef   struct   tskTaskControlBlock       * TaskHandle_t;       // 任务句柄类型（可修改的任务引用）

### *(API)xTaskCreateAffinitySet* 多核系统且支持核心亲和性才使用，本文不关注

### *(API)xTaskCreateStatic* 静态分配创建任务

```c
    TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,
                                    const char * const pcName,
                                    const configSTACK_DEPTH_TYPE uxStackDepth,
                                    void * const pvParameters,
                                    UBaseType_t uxPriority,
                                    StackType_t * const puxStackBuffer,
                                    StaticTask_t * const pxTaskBuffer )
/* 使用示例：
 *
 *  // 待创建任务将使用的栈缓冲区大小
 *  // 注意：这是栈能容纳的“变量数量”，而非字节数。例如，若每个栈元素为32位（4字节），
 *  // 此处设为100，则需分配400字节（100 × 32位）的栈空间。
 *  #define STACK_SIZE 200
 *
 *  // 用于存储待创建任务TCB的结构体
 *  StaticTask_t xTaskBuffer;
 *
 *  // 待创建任务将使用的栈缓冲区。注意这是一个StackType_t类型的数组，
 *  // StackType_t的大小由RTOS移植层（port）决定。
 *  StackType_t xStack[ STACK_SIZE ];
 *
 *  // 待创建任务的实现函数
 *  void vTaskCode( void * pvParameters )
 *  {
 *      // 预期参数值为1，因为在调用xTaskCreateStatic()时，pvParameters参数传入的是1
 *      configASSERT( ( uint32_t ) pvParameters == 1U );
 *
 *      for( ;; )
 *      {
 *          // 任务代码写在此处
 *      }
 *  }
 *
 *  // 创建任务的函数
 *  void vOtherFunction( void )
 *  {
 *      TaskHandle_t xHandle = NULL;
 *
 *      // 不使用任何动态内存分配创建任务
 *      xHandle = xTaskCreateStatic(
 *                    vTaskCode,       // 实现任务的函数
 *                    "NAME",          // 任务的文本名称
 *                    STACK_SIZE,      // 栈大小（按变量数量，非字节）
 *                    ( void * ) 1,    // 传入任务的参数
 *                    tskIDLE_PRIORITY,// 任务创建时的优先级
 *                    xStack,          // 用作任务栈的数组
 *                    &xTaskBuffer );  // 用于存储任务数据结构的变量
 *
 *      // puxStackBuffer和pxTaskBuffer均非NULL，因此任务已创建成功，
 *      // xHandle即为任务句柄。可通过该句柄挂起任务。
 *      vTaskSuspend( xHandle );
 *  }
 * 
 */
```

这里说一下任务要注意的参数

*StackType_t \*const   puxStackBuffer*    //任务的栈

> 因为是静态分配，需要我们手动给出一个空间 注意：该值必须指向一个至少包含uxStackDepth个元素的StackType_t数组，该数组将作为任务的栈，从而避免栈的动态分配

### *(API)xTaskCreateStaticAffinitySet* 多核系统且支持核心亲和性才使用，本文不关注 

### *(API)xTaskCreateRestricted* 启用 MPU 封装功能可以使用，本文不关注 

### *(API)xTaskCreateRestrictedAffinitySet* 启用 MPU 封装功能，多核系统且支持核心亲和性才使用，本文不关注

### *(API)xTaskCreateRestrictedStatic*   本文不关注

###  *(API)xTaskCreateRestrictedStaticAffinitySet* 本文不关注

## 任务长什么样

在讲解任务分配源码前，有必要先看一下任务长什么样子，看过之前的API的参数，我们知道任务有个TCB控制块（任务句柄就是个TCB指针），还有个栈。

接下来，先给出源码，看一看任务的TCB长什么样：（我省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

```c
typedef struct tskTaskControlBlock 
{
    /**< 指向任务栈中最后放入的项的位置。这必须是TCB结构体的第一个成员。 */
    volatile StackType_t * pxTopOfStack;

    /**< 任务的状态列表项所引用的列表表示该任务的状态（就绪、阻塞、挂起）。 */
    ListItem_t xStateListItem;                  
    /**< 用于从事件列表中引用任务。 */
    ListItem_t xEventListItem;                  
    /**< 任务的优先级。0是最低优先级。 */
    UBaseType_t uxPriority;                  
    /**< 指向栈的起始位置。 */
    StackType_t * pxStack;                   
    /**< 创建任务时赋予的描述性名称。仅用于调试。 */
    char pcTaskName[ configMAX_TASK_NAME_LEN ]; 

    #if ( configUSE_TASK_PREEMPTION_DISABLE == 1 )
        BaseType_t xPreemptionDisable; 
    	/**< 用于防止任务被抢占。 */
    #endif

    #if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
        StackType_t * pxEndOfStack; 
    	/**< 指向栈的最高有效地址。 */
    #endif

    #if ( portCRITICAL_NESTING_IN_TCB == 1 )
    	/**< 用于在移植层不维护自己的计数的端口中，保存临界区嵌套深度。 */
        UBaseType_t uxCriticalNesting; 
    #endif

    #if ( configUSE_MUTEXES == 1 )
        /**< 任务原来的优先级——用于优先级继承机制。 */
        UBaseType_t uxBasePriority; 
    	/**< 上次分配给任务的优先级——用于优先级继承机制。 */
        UBaseType_t uxMutexesHeld;
    #endif

    //任务通知相关
    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
        volatile uint32_t ulNotifiedValue[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
        volatile uint8_t ucNotifyState[ configTASK_NOTIFICATION_ARRAY_ENTRIES ];
    #endif

    #if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        uint8_t ucStaticallyAllocated; 
    	/**< 如果任务是静态分配的，则设置为pdTRUE，以确保不会尝试释放该内存。 */
    #endif

    #if ( INCLUDE_xTaskAbortDelay == 1 )
        uint8_t ucDelayAborted;
    #endif

    #if ( configUSE_POSIX_ERRNO == 1 )
        int iTaskErrno;
    #endif
} tskTCB;

/* 上面保留了旧的tskTCB名称，然后在下面将其重定义为新的TCB_t名称，
 * 以支持较旧的内核感知调试器。 */
typedef tskTCB TCB_t;
```

这是已经减少了一部分了，但是还是有点多，我们主要关注下面图里内容就好

![img](https://pic1.zhimg.com/80/v2-5642b925d076a0ebbf8f680b412ef8ba_1440w.png?source=ccfced1a)

从下往上看：

静态or动态分配：这个好理解，就是表示任务空间采用的是什么分配方式

任务通知：任务通知用于将事件直接从一个任务发送到另一个任务，这个我们在任务通知里说明，本文不再关注。

互斥量相关：这里主要是在“优先级翻转的情况下要使用”，这个我们在互斥量里说明，本文不再关注。

任务名：这个就简单了，之前创建任务的API里不是要传入一个任务名吗，就是写在这里。

栈的起始位置和栈顶：之前创建任务的API里有一个栈深度，这两个和栈深度有关。

状态链表项和事件链表项：如果之前看过[FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md)，你就知道了，状态链表项是表示任务在那个状态（就绪、挂起、删除......）的关键，事件链表项是任务控制块（TCB）中用于将任务挂载到特定事件等待链表的关键结构。

在《Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0》里有这样一幅图：

![img](https://picx.zhimg.com/80/v2-2695b44697f38587045ab0aef1e7841f_1440w.png?source=ccfced1a)

一个任务有一个TCB和栈。

## **任务创建相关源码**

### *(源码)xTaskCreate*  动态分配创建任务

源码不全，留下了关键的

```c
    BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                            const char * const pcName,
                            const configSTACK_DEPTH_TYPE uxStackDepth,
                            void * const pvParameters,
                            UBaseType_t uxPriority,
                            TaskHandle_t * const pxCreatedTask )
    {
        TCB_t * pxNewTCB;
        BaseType_t xReturn;

        pxNewTCB = prvCreateTask( pxTaskCode, pcName, uxStackDepth, pvParameters, uxPriority, pxCreatedTask );

        if( pxNewTCB != NULL )
        {
            prvAddNewTaskToReadyList( pxNewTCB );  //将刚刚创建任务放入就绪列表中
            xReturn = pdPASS;
        }
        else
        {
            xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
        }

        return xReturn;
    }
```

主要调用了两个函数 *prvCreateTask* 创建和 *prvAddNewTaskToReadyList* 将刚刚创建任务放入就绪列表中。

下面分别给出源码和解释

*prvCreateTask*

我使用的是 （*ARM Cortex-M3*）移植版本  *portSTACK_GROWTH = -1* 直接看栈向下增长的情况。

```c
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    // 静态函数：创建任务的内部实现，返回新任务的TCB（任务控制块）指针
    static TCB_t * prvCreateTask( TaskFunction_t pxTaskCode,
                                  const char * const pcName,
                                  const configSTACK_DEPTH_TYPE uxStackDepth,
                                  void * const pvParameters,
                                  UBaseType_t uxPriority,
                                  TaskHandle_t * const pxCreatedTask )
    {
        TCB_t * pxNewTCB;  // 指向新任务控制块的指针

        /* 如果栈是向上增长的，则先分配TCB再分配栈，避免栈溢出覆盖TCB；
         * 如果栈是向下增长的，则先分配栈再分配TCB，同样避免栈溢出问题 */
        #if ( portSTACK_GROWTH > 0 )  // 栈向上增长（从低地址到高地址）
        {
           /* 我使用的是 （ARM Cortex-M3）移植版本  portSTACK_GROWTH = -1 直接看下面 */
        }
        #else /* portSTACK_GROWTH <= 0，栈向下增长（从高地址到低地址） */
        {
            StackType_t * pxStack;  // 指向栈内存的指针

            /* 先为任务栈分配内存 */
            pxStack = pvPortMallocStack( 
                ( ( ( size_t ) uxStackDepth ) * sizeof( StackType_t ) )
            );

            if( pxStack != NULL )  // 栈分配成功
            {
                /* 再为TCB分配内存 */
                pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

                if( pxNewTCB != NULL )  // TCB分配成功
                {
                    // 初始化TCB为全0
                    ( void ) memset( ( void * ) pxNewTCB, 0x00, sizeof( TCB_t ) );
                    // 将栈地址存储到TCB中
                    pxNewTCB->pxStack = pxStack;
                }
                else  // TCB分配失败
                {
                    // 释放已分配的栈内存
                    vPortFreeStack( pxStack );
                }
            }
            else  // 栈分配失败
            {
                pxNewTCB = NULL;  // 标记创建失败
            }
        }
        #endif /* portSTACK_GROWTH */

        if( pxNewTCB != NULL )  // TCB和栈均分配成功
        {
            #if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
            {
                /* 任务可静态或动态创建，此处标记为动态分配（栈和TCB均动态分配），
                 * 以便后续删除任务时正确释放内存 */
                pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB;
            }
            #endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */

            // 初始化新任务的其他属性（如任务函数、名称、优先级等）
            prvInitialiseNewTask( pxTaskCode, pcName, uxStackDepth, pvParameters, 
                                 uxPriority, pxCreatedTask, pxNewTCB, NULL );
        }

        return pxNewTCB;  // 返回新任务的TCB指针（失败则返回NULL）
    }
```

我们分两步，为任务栈和TCB分配内存 和 初始化任务栈和TCB。

**第一步，为任务栈和TCB分配内存**

为任务栈分配内存，调用 pvPortMallocStack 

为TCB分配内存，调用 pvPortMalloc 

在[FreeRTOS内存管理01（源码）](./FreeRTOS内存管理01（源码）.md) 和 [FreeRTOS内存管理02（源码）](./FreeRTOS内存管理02（源码）.md) 里，已经知道了动态分配是怎么分配的，但是之前好像没有提到过 *pvPortMallocStack* 这个函数，在<portable.h>里有关于函数的定义：

```c
#if ( configSTACK_ALLOCATION_FROM_SEPARATE_HEAP == 1 )
    void * pvPortMallocStack( size_t xSize ) PRIVILEGED_FUNCTION;
    void vPortFreeStack( void * pv ) PRIVILEGED_FUNCTION;
#else
    #define pvPortMallocStack    pvPortMalloc
    #define vPortFreeStack       vPortFree
#endif
```

可以看到，默认情况下 pvPortMallocStack 就是 pvPortMalloc，与原来这两个一样，不过也可以选择自己定义一个。

这里通过调用两个函数，就完成了分配内存。（TCB和栈是分开申请的，是两块区域）

**第二步，初始化任务栈和TCB**

这里首先对TCB里 静态or动态分配 这个地方进行了初始化，相关的宏如下

```c
/* 用于记录任务的栈和 TCB（任务控制块）分配方式的位定义。 */
#define tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB    ( ( uint8_t ) 0 ) // 栈和 TCB 均为动态分配
#define tskSTATICALLY_ALLOCATED_STACK_ONLY        ( ( uint8_t ) 1 ) // 仅栈为静态分配，TCB 为动态分配
#define tskSTATICALLY_ALLOCATED_STACK_AND_TCB     ( ( uint8_t ) 2 ) // 栈和 TCB 均为静态分配
```

然后调用了一个函数 *prvInitialiseNewTask* 去初始化剩下的内容，下面是这个函数的源码：（源码相当长了，我这里省略了好多本文不关注的内容）

```c
// 静态函数：初始化新任务的核心属性（TCB配置、栈初始化、任务状态设置等）
static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
                                  const char * const pcName,
                                  const configSTACK_DEPTH_TYPE uxStackDepth,
                                  void * const pvParameters,
                                  UBaseType_t uxPriority,
                                  TaskHandle_t * const pxCreatedTask,
                                  TCB_t * pxNewTCB,
                                  const MemoryRegion_t * const xRegions )
{
    StackType_t * pxTopOfStack;  // 指向任务栈顶的指针
    UBaseType_t x;               // 循环计数器
    // ===================== 模块1：MPU特权模式判断（仅启用MPU封装时生效） =====================
    // ===================== 模块2：栈内存初始化（调试辅助） =====================
    /* 仅在需要时使用memset，避免不必要的依赖 */
    #if ( tskSET_NEW_STACKS_TO_KNOWN_VALUE == 1 )
    {
        // 用已知值（tskSTACK_FILL_BYTE）填充栈内存，便于调试时观察栈使用情况（如栈溢出检测）
        ( void ) memset( pxNewTCB->pxStack, 
                        ( int ) tskSTACK_FILL_BYTE, 
                        ( size_t ) uxStackDepth * sizeof( StackType_t )  // 栈总字节数
                      );
    }
    #endif /* tskSET_NEW_STACKS_TO_KNOWN_VALUE == 1 */


    // ===================== 模块3：计算栈顶地址（适配不同栈增长方向） =====================
    /* 栈顶地址的计算依赖于栈的增长方向（从高地址到低地址，或反之）
     * portSTACK_GROWTH 用于控制地址计算的正负方向，适配不同CPU架构 */
    #if ( portSTACK_GROWTH < 0 )  // 栈向下增长（如ARM Cortex-M系列，从高地址→低地址）
    {
        // 栈向下增长时，栈顶是栈数组的“最后一个元素地址”（初始状态栈为空，栈顶即栈的最高地址）
        pxTopOfStack = &( pxNewTCB->pxStack[ uxStackDepth - ( configSTACK_DEPTH_TYPE ) 1 ] );
        // 栈顶地址对齐处理（确保符合CPU要求的内存对齐规则，如4字节/8字节对齐）
        pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) 
                                          & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );

        // 断言：验证栈顶地址对齐是否正确（若失败则触发调试断言）
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0U ) );

        #if ( configRECORD_STACK_HIGH_ADDRESS == 1 )
        {
            // 记录栈的最高地址（用于调试，辅助定位栈溢出问题）
            pxNewTCB->pxEndOfStack = pxTopOfStack;
        }
        #endif /* configRECORD_STACK_HIGH_ADDRESS */
    }
    #else /* portSTACK_GROWTH >= 0 ：栈向上增长（如x86，从低地址→高地址） */
    {
    }
    #endif /* portSTACK_GROWTH */


    // ===================== 模块4：任务名称写入TCB =====================
    if( pcName != NULL )  // 任务名称不为空
    {
        // 循环将任务名称复制到TCB的pcTaskName数组中（最大长度由configMAX_TASK_NAME_LEN限制）
        for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
        {
            pxNewTCB->pcTaskName[ x ] = pcName[ x ];

            // 若名称长度小于最大限制，遇到字符串结束符（'\0'）则提前退出循环
            if( pcName[ x ] == ( char ) 0x00 )
            {
                break;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
            }
        }

        // 确保任务名称始终以'\0'结尾（即使名称长度等于最大限制，避免字符串越界）
        pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1U ] = '\0';
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }


    // ===================== 模块5：任务优先级配置 =====================
    // 断言：确保任务优先级不超过系统最大支持优先级（避免数组越界）
    configASSERT( uxPriority < configMAX_PRIORITIES );

    // 若优先级超出最大限制，强制设为“最大优先级-1”（容错处理）
    if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
    {
        uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    // 将最终优先级写入TCB
    pxNewTCB->uxPriority = uxPriority;
    #if ( configUSE_MUTEXES == 1 )  // 启用互斥锁功能
    {
        // 记录“基础优先级”（互斥锁优先级继承机制需用到，避免优先级反转）
        pxNewTCB->uxBasePriority = uxPriority;
    }
    #endif /* configUSE_MUTEXES */


    // ===================== 模块6：TCB列表项初始化（任务调度核心） =====================
    // 初始化“任务状态列表项”（用于加入就绪/阻塞/挂起等状态列表）
    vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
    // 初始化“任务事件列表项”（用于等待事件时加入事件列表，如信号量/队列）
    vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

    /* 为列表项设置“所属TCB”的回指指针
     * 作用：从通用列表项（ListItem_t）反向找到对应的TCB，便于调度器操作任务 */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* 事件列表始终按优先级排序：
     * 列表项“值”设为（最大优先级 - 任务优先级），确保高优先级任务的列表项值更小，
     * 排序时排在列表前端，实现“高优先级任务优先被唤醒”的逻辑 */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), 
                            ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority );
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

    // ===================== 模块7：MPU内存区域配置（仅启用MPU时生效） =====================
    // ===================== 模块8：C运行时TLS初始化（线程局部存储） =====================
    // ===================== 模块9：栈初始化（模拟任务被中断的上下文） =====================
    /* 初始化栈内容，使其看起来像“任务已运行但被调度器中断”的状态
     * 栈中会预置任务入口函数地址、参数、寄存器值等，确保任务首次运行时能正确执行 */
    #if ( portUSING_MPU_WRAPPERS == 1 )  // 启用MPU
    {
    }
    #else  // 未启用MPU
    {
        #if ( portHAS_STACK_OVERFLOW_CHECKING == 1 )  // 启用栈溢出检测，不关注这个
        {
        }
        #else  // 未启用栈溢出检测
        {
            pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, 
                                                           pxTaskCode, 
                                                           pvParameters );
        }
        #endif
    }
    #endif /* portUSING_MPU_WRAPPERS */

    // ===================== 模块10：多核系统任务状态初始化 =====================
    // ===================== 模块11：返回任务句柄 =====================
    if( pxCreatedTask != NULL )  // 任务句柄指针不为空（需要返回句柄）
    {
        /* 将TCB指针包装为“任务句柄”（TaskHandle_t）返回给用户
         * 用户可通过句柄操作任务（如修改优先级、删除任务等） */
        *pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
```

这些模块是把源码扔给AI，AI分的，正好就按照这些模块来说把。

**模块2：栈内存初始化**

```c
    /* 仅在需要时使用memset，避免不必要的依赖 */
    #if ( tskSET_NEW_STACKS_TO_KNOWN_VALUE == 1 )
    {
        // 用已知值（tskSTACK_FILL_BYTE）填充栈内存，便于调试时观察栈使用情况（如栈溢出检测）
        ( void ) memset( pxNewTCB->pxStack, 
                        ( int ) tskSTACK_FILL_BYTE, 
                        ( size_t ) uxStackDepth * sizeof( StackType_t )  // 栈总字节数
                      );
    }
    #endif /* tskSET_NEW_STACKS_TO_KNOWN_VALUE == 1 */
```

就是给栈空间赋初值，通过宏 *tskSET_NEW_STACKS_TO_KNOWN_VALUE* 可以选择关闭，

*tskSTACK_FILL_BYTE*    这个初始值，定义如下：

```c
/*任务创建时用于填充任务栈的值。这一值纯粹用于检查任务的高水位线（栈使用峰值）。*/
#define tskSTACK_FILL_BYTE                        ( 0xa5U )
```

"用于检查任务的高水位线（栈使用峰值）"这是什么意思？ 任务创建API要求我们传入一个参数（栈深度），不知道你有没有想过栈深度的值怎么给，一般来说，去看教程都有一个推荐的值，但是实际上我们可以通过调试去找到一个相对合理的值，提高利用率，不过本文不对此做详细说明。

*(size_t) uxStackDepth \*sizeof( StackType_t )*   栈总字节数

前面有说过，传入的栈深度 *uxStackDepth* 不是字节，所以要乘一个数，*StackType_t*  好像之前没有说过，每个移植版本可能有所不同，我使用的版本是（ARM Cortex-M3），定义如下：

```c
#define portSTACK_TYPE    uint32_t
typedef portSTACK_TYPE   StackType_t;
```

*StackType_t* 和 *size_t*  都是  *uint32_t*

**模块3：计算栈顶地址**

```c
    #if ( portSTACK_GROWTH < 0 )  // 栈向下增长（如ARM Cortex-M系列，从高地址→低地址）
    {
        // 栈向下增长时，栈顶是栈数组的“最后一个元素地址”（初始状态栈为空，栈顶即栈的最高地址）
        pxTopOfStack = &( pxNewTCB->pxStack[ uxStackDepth - ( configSTACK_DEPTH_TYPE ) 1 ] );
        // 栈顶地址对齐处理（确保符合CPU要求的内存对齐规则，如4字节/8字节对齐）
        pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) 
                                          & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );

        // 断言：验证栈顶地址对齐是否正确（若失败则触发调试断言）
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0U ) );

        #if ( configRECORD_STACK_HIGH_ADDRESS == 1 )
        {
            // 记录栈的最高地址（用于调试，辅助定位栈溢出问题）
            pxNewTCB->pxEndOfStack = pxTopOfStack;
        }
        #endif /* configRECORD_STACK_HIGH_ADDRESS */
    }
```

直接看向下增长，因为（ARM Cortex-M3）移植版本中是这么定义的：

```
#define portSTACK_GROWTH      ( -1 )
```

剩下的就很简单，可以直接看源码，就是把栈的高低地址初始化（写到TCB里），关于内存对齐之前看 [FreeRTOS内存管理01（源码）](./FreeRTOS内存管理01（源码）.md) 的时候有说过。

**模块4：任务名称写入TCB**

```c
        // 循环将任务名称复制到TCB的pcTaskName数组中（最大长度由configMAX_TASK_NAME_LEN限制）
        for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
        {
            pxNewTCB->pcTaskName[ x ] = pcName[ x ];

            // 若名称长度小于最大限制，遇到字符串结束符（'\0'）则提前退出循环
            if( pcName[ x ] == ( char ) 0x00 )
            {
                break;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
            }
        }
```

通过循环一个字节一个字节的写到TCB里

**模块5：任务优先级配置**

```c
// 若优先级超出最大限制，强制设为“最大优先级-1”（容错处理）
    if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
    {
        uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
    // 将最终优先级写入TCB
    pxNewTCB->uxPriority = uxPriority;
    #if ( configUSE_MUTEXES == 1 )  // 启用互斥锁功能
    {
        // 记录“基础优先级”（互斥锁优先级继承机制需用到，避免优先级反转）
        pxNewTCB->uxBasePriority = uxPriority;
    }
    #endif /* configUSE_MUTEXES */
```

直接对TCB里的优先级赋值，“基础优先级”（uxBasePriority）有什么用，本文不讨论，之后再说。

**模块6：TCB列表项初始化**

```c
    // 初始化“任务状态列表项”（用于加入就绪/阻塞/挂起等状态列表）
    vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
    // 初始化“任务事件列表项”（用于等待事件时加入事件列表，如信号量/队列）
    vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

    /* 为列表项设置“所属TCB”的回指指针
     * 作用：从通用列表项（ListItem_t）反向找到对应的TCB，便于调度器操作任务 */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* 事件列表始终按优先级排序：
     * 列表项“值”设为（最大优先级 - 任务优先级），确保高优先级任务的列表项值更小，
     * 排序时排在列表前端，实现“高优先级任务优先被唤醒”的逻辑 */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), 
                            ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority );
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );
```

（上面的函数源码 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 里有）关于链表的初始化，之前已经讨论过 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) ，但是当时说了，初始化并不完整，这里是比较完整的初始化，调用*vListInitialiseItem* 初始化只是这个样子：

![img](https://picx.zhimg.com/80/v2-ef7eef007f37cbe970cc03ccce85db9f_1440w.png?source=ccfced1a)

```c
listSET_LIST_ITEM_OWNER(&( pxNewTCB->xStateListItem ), pxNewTCB ); //设置列表项pvOwner
listSET_LIST_ITEM_OWNER(&( pxNewTCB->xEventListItem ), pxNewTCB ); //设置列表项pvOwner
listSET_LIST_ITEM_VALUE(&( pxNewTCB->xEventListItem ),( TickType_t ) configMAX_PRIORITIES -( TickType_t ) uxPriority );  //设置列表项 xltemValue
```

这里没有初始化xStateListItem 的 xltemValue，不知道为什么，初始化后大概是这样：

![img](https://pic1.zhimg.com/80/v2-bd0bbb0c19422886f89c199cba01ebbd_1440w.png?source=ccfced1a)

​																			图太乱了，大概看一下初始化了哪些

**模块9：栈初始化**

```c
            pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, 
                                                           pxTaskCode, 
                                                           pvParameters );
```

*pxPortInitialiseStack* 和移植版本有关， ARM CM3（ARM Cortex-M3）移植版本是这样定义的：

```c
// 函数功能：初始化任务栈，构建模拟中断上下文的栈帧，并返回初始化后的栈顶指针
// 参数：
//   pxTopOfStack：任务栈的初始栈顶指针（栈从高地址向低地址生长，初始指向栈的最高地址）
//   pxCode：任务入口函数（任务首次运行时要执行的函数）
//   pvParameters：传递给任务入口函数的参数（通过 R0 寄存器传递）
// 返回值：初始化后的新栈顶指针（任务运行时，CPU 会从该指针开始加载寄存器）
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    /* 模拟“上下文切换中断”产生的栈帧结构（Cortex-M3 中断会自动将部分寄存器压栈，此处需手动构建完整栈帧）。 */
    pxTopOfStack--;                                                      /* 栈指针减1：适配 MCU 中断进入/退出时的栈操作方式（Cortex-M3 中断压栈后栈指针自动递减） */
    *pxTopOfStack = portINITIAL_XPSR;                                    /* 栈帧第1项：xPSR 寄存器（程序状态寄存器），初始值为 portINITIAL_XPSR（0x01000000，标识 Thumb 指令集） */
    pxTopOfStack--;
    *pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK; /* 栈帧第2项：PC 寄存器（程序计数器），存储任务入口函数地址，并用掩码清除第0位（符合 Thumb 指令集地址要求） */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS;             /* 栈帧第3项：LR 寄存器（链接寄存器），存储任务退出后的返回地址（默认是 prvTaskExitError 函数） */
    pxTopOfStack -= 5;                                                   /* 栈指针减5：跳过 R12、R3、R2、R1 这4个寄存器的位置（初始值无需显式设置，用栈默认值即可） */
    *pxTopOfStack = ( StackType_t ) pvParameters;                        /* 栈帧第8项：R0 寄存器，存储传递给任务入口函数的参数（pvParameters），符合 Cortex-M 函数调用约定（第1个参数通过 R0 传递） */
    pxTopOfStack -= 8;                                                   /* 栈指针减8：跳过 R11~R4 这8个寄存器的位置（初始值无需显式设置，用栈默认值即可） */

    return pxTopOfStack;  // 返回初始化后的新栈顶指针（后续任务切换时，CPU 会从该指针加载寄存器状态）
}
```

这里不看也没关系，之后的文章讲调度器的时候再说这件事。

这里涉及到了ARM Cortex-M3/M4内核架构中断的相关内容，可以看看这篇文章[ARM Cortex-M3/M4内核架构：中断处理过程_arm中断处理流程-CSDN博客](https://blog.csdn.net/2301_80348599/article/details/142852513?spm=1001.2101.3001.6650.2&utm_medium=distribute.pc_relevant.none-task-blog-2~default~BlogCommendFromBaidu~Ctr-2-142852513-blog-140059999.235^v43^pc_blog_bottom_relevance_base8&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2~default~BlogCommendFromBaidu~Ctr-2-142852513-blog-140059999.235^v43^pc_blog_bottom_relevance_base8&utm_relevant_index=4)

中断触发时，需要保护现场，这里涉及到了16个寄存器，之后讲中断的时候会具体说明怎么保护、这些寄存器作用。这里初始化了任务栈，构建模拟中断上下文的栈帧。（说到调度器的时候细说）

![img](https://pic1.zhimg.com/80/v2-b41e9a989696c050aa296b6e1e4b28fe_1440w.png?source=ccfced1a)

初始化栈后

![img](https://picx.zhimg.com/80/v2-03da48674eb792be5fc9bca3cfd71027_1440w.png?source=ccfced1a)

**模块11：返回任务句柄**

```c
    // ===================== 模块11：返回任务句柄 =====================
    if( pxCreatedTask != NULL )  // 任务句柄指针不为空（需要返回句柄）
    {
        /* 将TCB指针包装为“任务句柄”（TaskHandle_t）返回给用户
         * 用户可通过句柄操作任务（如修改优先级、删除任务等） */
        *pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
    }
```

初始化后TCB大概长这个样子

![img](https://picx.zhimg.com/80/v2-7b062999459c46bd3b375038e4169f9b_1440w.png?source=ccfced1a)

![img](https://pica.zhimg.com/80/v2-869c69d9f82f642f6ec73b75be34f0da_1440w.png?source=ccfced1a)

这只是完成了任务的初始化，任务的创建还有一步（可以返回上面的源码看一看）

*prvAddNewTaskToReadyList( pxNewTCB ) //将初始化后的任务加入到就绪链表中*

接下来看看这是怎么实现的，先给出源码（单核版本，本文不考虑多核）：

```c
static void prvAddNewTaskToReadyList( TCB_t * pxNewTCB )
// 解析：该函数是FreeRTOS内部函数，负责将新创建的任务添加到就绪列表，并处理相关初始化逻辑
{
    /* 确保在更新任务列表时，中断不会访问这些列表 */
    // 进入临界区（保护任务列表操作不被中断或其他任务干扰）
    taskENTER_CRITICAL();
    {
        // 增加系统当前任务总数（uxCurrentNumberOfTasks是全局变量）
        uxCurrentNumberOfTasks = ( UBaseType_t ) ( uxCurrentNumberOfTasks + 1U );

        // 如果当前没有正在运行的任务（pxCurrentTCB为NULL）
        if( pxCurrentTCB == NULL )
        {
            /* There are no other tasks, or all the other tasks are in
             * the suspended state - make this the current task. */
            // 翻译：/* 没有其他任务，或所有其他任务都处于挂起状态 - 将此任务设为当前任务 */
            pxCurrentTCB = pxNewTCB;

            // 如果这是系统创建的第一个任务
            if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
            {
                /* 这是第一个被创建的任务，因此执行必要的初步初始化。
                //        如果此调用失败，我们无法恢复，但会报告失败 */
                /* 第一个任务创建：执行任务列表初始化 */
                prvInitialiseTaskLists(); // 初始化FreeRTOS所有任务列表（就绪、挂起、阻塞等）
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // 测试覆盖率标记，实际无功能
            }
        }
        else
        {
            /* 如果调度器尚未运行，且此任务是目前创建的优先级最高的任务，则将其设为当前任务 */
            // 调度器未启动时（xSchedulerRunning是标记调度器状态的全局变量）
            if( xSchedulerRunning == pdFALSE )
            {
                /* 调度器未启动：若新任务优先级 ≥ 当前任务，更新当前任务 */
                if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
                {
                    pxCurrentTCB = pxNewTCB; // 新任务优先级更高或相同，设为当前任务
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

        // 任务编号自增（用于标识任务创建顺序）
        uxTaskNumber++;

        // 将新任务添加到就绪列表（内部函数，根据任务优先级插入对应就绪链表）
        prvAddTaskToReadyList( pxNewTCB );

        // 移植层宏：执行与硬件相关的TCB初始化（如栈指针调整等平台相关操作）
        portSETUP_TCB( pxNewTCB ); // 默认情况下 #define portSETUP_TCB( pxTCB )    ( void ) ( pxTCB )
    }
    // 退出临界区（恢复中断）
    taskEXIT_CRITICAL();

    // 如果调度器已经在运行
    if( xSchedulerRunning != pdFALSE )
    {
        /* 如果创建的任务优先级高于当前任务，则应立即运行 */
        /* 若新任务优先级高于当前任务，触发任务切换 */
        taskYIELD_ANY_CORE_IF_USING_PREEMPTION( pxNewTCB ); //本文不讨论调度器相关内容
        // 上述宏的作用：如果新任务优先级更高，触发任务切换（在多核系统中会选择合适的核）
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
```

在 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 中有说到，在FreeRTOS中任务的就绪、挂起........都是一个链表，有就绪链表、挂起链表.......让一个人任务变成就绪态就是把相应的任务放到这个链表中，比如下面这一个

```c
/**< 按优先级排序的就绪链表。 */
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ]; 
```

之前给出了定义，但是还没初始化，这些链表都是在这里，也就是创建第一个任务时初始化的，调用了*prvInitialiseTaskLists* 下面给出源码：

```c
// 初始化FreeRTOS所有核心任务列表（就绪列表、延迟列表、挂起列表等）
static void prvInitialiseTaskLists( void )
{
    UBaseType_t uxPriority;

    // 1. 初始化“就绪任务列表”：为每个优先级创建一个就绪列表
    for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
    {
        // 初始化当前优先级对应的就绪列表（pxReadyTasksLists是数组，每个元素对应一个优先级的就绪列表）
        vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
    }

    // 2. 初始化“延迟任务列表”：2个延迟列表用于处理xTickCount溢出（双缓冲机制）
    vListInitialise( &xDelayedTaskList1 );
    vListInitialise( &xDelayedTaskList2 );
    // 3. 初始化“挂起就绪列表”：临时存储调度器挂起期间被唤醒的任务
    vListInitialise( &xPendingReadyList );

    #if ( INCLUDE_vTaskDelete == 1 ) // 若启用任务删除功能
    {
        // 4. 初始化“待终止任务列表”：存储调用vTaskDelete()后待清理的任务
        vListInitialise( &xTasksWaitingTermination );
    }
    #endif /* INCLUDE_vTaskDelete */

    #if ( INCLUDE_vTaskSuspend == 1 )  // 若启用任务挂起功能
    {
        // 5. 初始化“挂起任务列表”：存储调用vTaskSuspend()显式挂起的任务
        vListInitialise( &xSuspendedTaskList );
    }
    #endif /* INCLUDE_vTaskSuspend */

    /* 初始化延迟列表指针：
     * - pxDelayedTaskList：当前活跃的延迟列表（用于添加/查询延迟任务）
     * - pxOverflowDelayedTaskList：溢出备用延迟列表（xTickCount溢出时切换）
     * 初始状态下，活跃列表使用xDelayedTaskList1，备用列表使用xDelayedTaskList2 */
    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
```

所有的链表都通过 vListInitialise 初始化，在[FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 中也已经说过。

> 这里有几个要注意的地方： 1.上面说的就绪链表pxReadyTasksLists是数组，一个优先级一个就绪链表 2.为什么延迟任务链表有两个，还有两个指针？这与定时器有关，本文先不说，之后说到定时器的时候再说。

这是链表的初始化，选择返回去继续看怎么把任务放到就绪链表中

又调用了一个函数 ：

```c
        // 将新任务添加到就绪列表（内部函数，根据任务优先级插入对应就绪链表）
        prvAddTaskToReadyList( pxNewTCB );
```

*prvAddTaskToReadyList* 下面给出源码：

```c
/* 将 pxTCB 所代表的任务放入该任务对应的就绪列表中。插入到列表的末尾。 */
#define prvAddTaskToReadyList( pxTCB )                                     \
    do {                                                                   \
        taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );               /* 更新最高就绪优先级 */
        /* 插入任务到就绪列表 */                                              
        listINSERT_END( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); 
    } while( 0 )
/* uxTopReadyPriority 存储最高优先级就绪状态任务的优先级。 */
    #define taskRECORD_READY_PRIORITY( uxPriority ) \
    do {                                            \
        if( ( uxPriority ) > uxTopReadyPriority )   \
        {                                           \
            uxTopReadyPriority = ( uxPriority );    \
        }                                           \
    } while( 0 ) /* taskRECORD_READY_PRIORITY */
```

实际上是调用了 *listINSERT_END* 函数把任务放到就绪链表中，关于此函数在 [FreeRTOS之链表（源码）](./FreeRTOS之链表（源码）.md) 有给出源码，就是把节点插入链表最后一个节点（哨兵节点）的前面，很简单。

到此为止，动态分配创建任务就说完了。

**小结：**

创建任务函数申请了TCB和任务的栈的空间、对它们进行了初始化，然后初始化了FreeRTOS里会用到的链表，将新创建的任务放入了就绪链表。

最后给一个初始化后的图（有点乱，看源码自己想清楚就好）

![img](https://picx.zhimg.com/80/v2-41a3ee85a0fb69681a7972dc1cb2f8a5_1440w.png?source=ccfced1a)

![img](https://picx.zhimg.com/80/v2-22ca1f39ff3287f74a53f099567e4960_1440w.png?source=ccfced1a)

为什么有5个就绪链表呢，因为默认情况下最大优先级为5，每个优先级一个就绪链表，最大优先级的定义在<configFreeRTOS.h>里，如下：

*#define    configMAX_PRIORITIES      5*

至于其他的几个链表，之后再说明，本文不关注。

### *(源码)xTaskCreateStatic* 静态分配创建任务

```c
    TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,
                                    const char * const pcName,
                                    const configSTACK_DEPTH_TYPE uxStackDepth,
                                    void * const pvParameters,
                                    UBaseType_t uxPriority,
                                    StackType_t * const puxStackBuffer,
                                    StaticTask_t * const pxTaskBuffer )
    {
        TaskHandle_t xReturn = NULL;
        TCB_t * pxNewTCB;

        pxNewTCB = prvCreateStaticTask( pxTaskCode, pcName, uxStackDepth, pvParameters, uxPriority, puxStackBuffer, pxTaskBuffer, &xReturn );

        if( pxNewTCB != NULL )
        {
            prvAddNewTaskToReadyList( pxNewTCB );/*在新任务被创建和初始化后调用，用于将任务置于调度器的控制之下。*/
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        return xReturn;
    }
```

和动态创建差不多，首先初始化（不过这里调用的函数是 *prvCreateStaticTask* ），然后把任务加入就绪链表（*prvAddNewTaskToReadyList* 这个到是和动态一样）

那就来看看静态时是怎么初始化的，下面是 *prvCreateStaticTask* 源码（不全，只有关键的）

```c
    // 静态函数：创建静态任务（使用用户预分配的内存），返回新任务的TCB指针
    static TCB_t * prvCreateStaticTask( TaskFunction_t pxTaskCode,  // 任务入口函数指针
                                        const char * const pcName,  // 任务名称
                                        const configSTACK_DEPTH_TYPE uxStackDepth,  // 栈深度（变量数量）
                                        void * const pvParameters,  // 传递给任务的参数
                                        UBaseType_t uxPriority,     // 任务优先级
                                        StackType_t * const puxStackBuffer,  // 用户预分配的静态栈缓冲区
                                        StaticTask_t * const pxTaskBuffer,   // 用户预分配的静态TCB缓冲区
                                        TaskHandle_t * const pxCreatedTask ) // 输出参数：任务句柄
    {
        TCB_t * pxNewTCB;  // 指向新任务TCB的指针

        // 若栈缓冲区和TCB缓冲区均有效
        if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
        {
            pxNewTCB = ( TCB_t * ) pxTaskBuffer;  // 将用户提供的StaticTask_t指针转为TCB_t指针
            ( void ) memset( ( void * ) pxNewTCB, 0x00, sizeof( TCB_t ) );  // 初始化TCB为全0
            pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;  // 绑定用户提供的栈缓冲区

            #if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )  // 内核同时支持静态和动态分配
            {
                /* 标记任务为“静态分配”（栈和TCB均由用户提供），
                 * 便于后续删除任务时正确处理内存（静态分配的内存无需内核释放） */
                pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
            }
            #endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */
            
            // 调用通用初始化函数，填充TCB的其他成员（如任务函数、优先级、栈顶地址等）
            prvInitialiseNewTask( pxTaskCode, pcName, uxStackDepth, pvParameters, 
                                 uxPriority, pxCreatedTask, pxNewTCB, NULL );
        }
        else
        {
            pxNewTCB = NULL;  // 内存缓冲区无效，返回NULL表示创建失败
        }

        return pxNewTCB;  // 返回新任务的TCB指针（失败则为NULL）
    }
```

可以先扫一眼注释，其实也就是分配空间时代码不一样，TCB的初始化还是 *prvInitialiseNewTask* 函数，下面我把分配的代码拿出来：

```c
            pxNewTCB = ( TCB_t * ) pxTaskBuffer;  // 将用户提供的StaticTask_t指针转为TCB_t指针
            ( void ) memset( ( void * ) pxNewTCB, 0x00, sizeof( TCB_t ) );  // 初始化TCB为全0
            pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;  // 绑定用户提供的栈缓冲区

            #if ( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )  // 内核同时支持静态和动态分配
            {
                /* 标记任务为“静态分配”（栈和TCB均由用户提供），
                 * 便于后续删除任务时正确处理内存（静态分配的内存无需内核释放） */
                pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
            }
            #endif /* tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE */
```

之前动态分配的时候申请的空间是通过 *pvPortMalloc* 函数，现在不一样了，TCB和栈的空间需要我们自己定义，怎么使用在介绍API时已经说过了，然后剩下的和动态时差不多，只是TCB里有个*ucStaticallyAllocated*用来标记我们用的是动态还是静态分配，这个不一样，下面需要说一下这个标记。

*ucStaticallyAllocated* 有三个值可取：

```
/* 用于记录任务的栈和 TCB（任务控制块）分配方式的位定义。 */
#define tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB    ( ( uint8_t ) 0 ) // 栈和 TCB 均为动态分配
#define tskSTATICALLY_ALLOCATED_STACK_ONLY        ( ( uint8_t ) 1 ) // 仅栈为静态分配，TCB 为动态分配
#define tskSTATICALLY_ALLOCATED_STACK_AND_TCB     ( ( uint8_t ) 2 ) // 栈和 TCB 均为静态分配
```

动态和静态都好理解，怎么还有个又用静态又用动态呢？ 此时栈为静态分配，TCB 为动态分配。

ok，关于任务创建就看这么多，源码其实还有好多内容，不过有的要放到之后去说。

关于任务删除不会马上提到，因为它涉及到了很多别的东西，最后再看。