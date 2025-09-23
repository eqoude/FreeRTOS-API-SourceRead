# FreeRTOS之资源管理（API+源码）

首先，用《Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0》中的两个例子了解下互斥。

**例1：**

![img](https://pic1.zhimg.com/80/v2-20aab8b6dbcaa5dcd8eb05d46447c870_1440w.png?source=ccfced1a)

> 考虑以下场景:有两个任务试图向液晶显示屏(LCD)写入数据。 1.任务A执行完毕后，开始将字符串“Hello world”写入液晶显示屏。 2.任务 A在输出字符串的开头部分“Hello w”之后，就被任务 B所抢先完成了。 3.任务 B在进入阻塞状态前会在液晶显示屏上显示“中止、重试、失败?”的字样。 4.任务 A从被中断的地方继续执行，并完成输出其字符串“world”剩余的字符。 现在的液晶显示屏上显示的是一段损坏的字符串“Hello wAbort,Retry,Fail?orld”

在任务A使用一个资源时，有其他任务抢断，占用了此任务，这是可能就会发生错误。

**例2：**

PORTA的值首先从内存读取到寄存器中，在寄存器内进行修改，然后再写回内存。

![img](https://picx.zhimg.com/80/v2-80bf4864238f6f1e2fff8018ba9eb130_1440w.png?source=ccfced1a)

> 1.任务 A将 PORTA 的值加载到一个寄存器中--这是操作的读取部分。 2.在任务 A完成相同操作的修改和写入部分之前，任务 B已抢先完成了这一任务 3.任务 B会更新 PORTA 的值，然后进入阻塞状态。 4.任务 A从其被中断的地方继续执行。它先在寄存器中修改已保存的 PORTA 值的副本，然后再将更新后的值写回 PORTA。

在这种情况下，任务A更新并写回一个过时的PORTA值。任务B在任务A复制PORTA值之后、任务A将其修改后的值写回到PORTA寄存器之前修改了PORTA。当任务A写入PORTA时，它会覆盖任务B已经执行的修改，从而破坏了PORTA寄存器的值。

看，这些是非原子操作的例子，如果这些操作被中断，可能会导致数据丢失或损坏。我们要说的互斥，就是处理这种情况的方法。

> 什么是非原子操作？ 原子操作是指在执行过程中不会被其他操作中断的操作，非原子操作就是指一个操作或一系列操作在执行过程中可能被中断，且可能出现部分执行、部分未执行的中间状态，不具备 “不可分割” 的特性。

互斥：

为确保始终保持数据一致性，对于任务之间或任务与中断之间共享的资源，必须使用“互斥”技术来管理对其的访问。目标是确保一旦某个任务开始访问一个不可重入且非线程安全的共享资源，在该资源恢复到一致状态之前，该任务对该资源拥有独占访问权。 

接下来就说一说FreeRTOS提供的可用于实现互斥功能的函数。

## 临界区与暂停调度器

> 临界区是干什么？ 在临界区域内，抢占式上下文文切换不会发生。

举例，之前的第一个例子，由于任务A被任务B抢断，打印出的数据不是我们想要的，现在，我们把打印的函数放到临界区内，这样就不会被B抢占了。（临界区必须保持非常短，否则它们会对中断响应时间产生不利影响）

```c
taskENTER_CRITICAL();

printf( "HELLO");

taskEXIT_CRITICAL();
```

### *（API）*基本临界区

*taskENTER_CRITICAL* 标记临界代码区域开始

*taskEXIT_CRITICAL* 标记临界代码区域结束

```c
#define taskENTER_CRITICAL()                 portENTER_CRITICAL()
#define taskEXIT_CRITICAL()                  portEXIT_CRITICAL()
```

无参数，无返回值。

### *（API）*基本临界区--中断安全版

*taskENTER_CRITICAL_FROM_ISR* 标记临界代码区域开始

*taskEXIT_CRITICAL_FROM_ISR* 标记临界代码区域结束

```c
#define taskENTER_CRITICAL_FROM_ISR()    portSET_INTERRUPT_MASK_FROM_ISR()
#define taskEXIT_CRITICAL_FROM_ISR( x )    portCLEAR_INTERRUPT_MASK_FROM_ISR( x )
```

参数x通常是进入临界区时保存的中断状态即*taskENTER_CRITICAL_FROM_ISR*的返回值，用于恢复

### *（API）*暂停（或锁定）调度器

临界区也可以通过暂停调度器来创建。

*vTaskSuspendAll* 暂停调度器

*xTaskResumeAll* 恢复调度器

```c
void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;
BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;
/*使用示例
 * void vTask1( void * pvParameters )
 * {
 *   for( ;; )
 *   {
 *      // 调用 vTaskSuspendAll() 暂停任务调度
 *      vTaskSuspendAll ();
 *
 *      // 执行需要独占处理器的操作：
 *      // 1. 期间不会被其他任务抢占（调度器已暂停）
 *      // 2. 中断仍可正常响应（如定时器 tick、外设中断等）
 *      // 3. 无需嵌套临界区，因为已无任务切换
 *      // ...
 *
 *      // 操作完成，调用 xTaskResumeAll() 恢复调度
 *      xTaskResumeAll ();
 *   }
 * }
 */
```

xTaskResumeAll恢复调度器是有返回值的：如果恢复调度器导致了上下文切换（上下文切换），则返回 pdTRUE；否则返回 pdFALSE。所以可以这样用

```c
void vTask1( void * pvParameters )
{
    for( ;; )
    {
        // 任务正常业务代码
        // ...

        // 调用 vTaskSuspendAll() 暂停调度器，确保当前任务不被切换
        vTaskSuspendAll ();

        // 执行长时间操作：
        // 1. 期间不会被其他任务打断（调度器暂停）
        // 2. 中断仍正常响应（如时钟 tick、外设事件）
        // 3. 无需额外加临界区，因为已无任务竞争
        // ...

        // 恢复调度器，并检查是否已发生任务切换
        if( !xTaskResumeAll () )  // xTaskResumeAll() 返回 pdFALSE 表示未切换
        {
            taskYIELD ();  // 主动触发一次任务切换
        }
    }
}
```

## 互斥锁（和二值信号量）

互斥锁是一种特殊类型的二值信号量，用于控制对两个或多个任务之间共享资源的访问。在互斥场景中使用时，互斥锁可以被视为与共享资源相关联的一个令牌。一个任务要合法访问该资源，必须首先成功“获取”令牌（成为令牌持有者）。当令牌持有者使用完资源后，必须“归还”令牌。只有在令牌被归还后，另一个任务才能成功获取令牌，进而安全地访问同一共享资源。除非持有令牌，否则任务不允许访问共享资源。

互斥锁和二值信号量是有不同的：用于互斥的信号量必须始终归还；用于同步的信号量通常会被丢弃，不会被返回。

### （API）创建互斥锁

```c
#if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configUSE_MUTEXES == 1 ) )
    // 宏定义：动态创建互斥锁，映射到底层队列创建函数（指定队列类型为互斥锁）
    #define xSemaphoreCreateMutex()    xQueueCreateMutex( queueQUEUE_TYPE_MUTEX )
#endif
/* 使用示例：
 * SemaphoreHandle_t xSemaphore;  // 定义互斥锁句柄
 *
 * void vATask( void * pvParameters )
 * {
 *  // 互斥锁使用前必须先创建，该宏直接传入句柄变量
 *  xSemaphore = xSemaphoreCreateMutex();
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 互斥锁创建成功，可开始使用（如 xSemaphoreTake() 获取、xSemaphoreGive() 释放）
 *  }
 * }
```

*return：*成功，返回创建的互斥锁句柄；失败，返回 NULL。

> 互斥锁句柄： 类型是*SemaphoreHandle_t* ，定义如下 *typedef  QueueHandle_t  SemaphoreHandle_t;* *QueueHandle_t*  是队列结构体，在[FreeRTOS之队列（源码）](./FreeRTOS之队列（源码）.md) 中说过上互斥锁、信号量都是基于FreeRTOS的队列结构体的。

```c
#if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configUSE_MUTEXES == 1 ) )
    // 宏定义：静态创建互斥锁，映射到底层静态队列创建函数（指定队列类型+静态内存缓冲区）
    #define xSemaphoreCreateMutexStatic( pxMutexBuffer )    xQueueCreateMutexStatic( queueQUEUE_TYPE_MUTEX, ( pxMutexBuffer ) )
#endif
/* 使用示例：
 * SemaphoreHandle_t xSemaphore;        // 定义互斥锁句柄
 * StaticSemaphore_t xMutexBuffer;      // 静态分配互斥锁内存（全局/静态变量，避免栈溢出）
 *
 * void vATask( void * pvParameters )
 * {
 *  // 互斥锁使用前必须先创建，传入静态内存缓冲区地址，无动态内存分配
 *  xSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer );
 *
 *  // 因未使用动态内存分配，只要 pxMutexBuffer 非 NULL，xSemaphore 就不会为 NULL，无需额外检查
 * }
```

*return：*成功，返回创建的互斥锁句柄；失败，返回 NULL。

### （API）获得互斥锁

```c
#define xSemaphoreTake( xSemaphore, xBlockTime )    xQueueSemaphoreTake( ( xSemaphore ), ( xBlockTime ) )
/* 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore = NULL;
 *
 * // 任务1：创建信号量
 * void vATask( void * pvParameters )
 * {
 *  // 创建二进制信号量（用于保护共享资源）
 *  xSemaphore = xSemaphoreCreateBinary();
 * }
 *
 * // 任务2：使用信号量
 * void vAnotherTask( void * pvParameters )
 * {
 *  // ... 执行其他操作 ...
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 尝试获取信号量：若不可用，等待10个时钟节拍
 *      if( xSemaphoreTake( xSemaphore, ( TickType_t ) 10 ) == pdTRUE )
 *      {
 *          // 成功获取信号量，可安全访问共享资源
 *
 *          // ... 访问共享资源的逻辑 ...
 *
 *          // 完成共享资源访问后，释放信号量
 *          xSemaphoreGive( xSemaphore );
 *      }
 *      else
 *      {
 *          // 等待超时，未能获取信号量，无法安全访问共享资源
 *      }
 *  }
 * }
 * }
 */
```

*param1：*要获取的互斥量句柄——该句柄在互斥量创建时获取

*param2：*等待信号量可用的时间（单位：时钟节拍）

*return：*成功返回 *pdTRUE*；失败返回 *pdFALSE*

### （API）释放互斥锁

```c
#define xSemaphoreGive( xSemaphore )    xQueueGenericSend( \
    ( QueueHandle_t ) ( xSemaphore ),               \ // 互斥量句柄强制转为队列句柄（信号量本质是队列） 
    NULL,                                           \ // 数据缓冲区=NULL（互斥量无需存储数据）
    semGIVE_BLOCK_TIME,                             \  // 阻塞时间=0（释放互斥量永不阻塞）
    queueSEND_TO_BACK                               \  // 发送位置=队列尾部（无实际意义，因无数据）
)
/* 使用示例：
 * @code{c}
 * SemaphoreHandle_t xSemaphore = NULL;
 *
 * void vATask( void * pvParameters )
 * {
 *  // 创建二进制信号量（用于保护共享资源）
 *  xSemaphore = xSemaphoreCreateBinary();  // 注：示例中原vSemaphoreCreateBinary已弃用，此处修正为新版接口
 *
 *  if( xSemaphore != NULL )
 *  {
 *      // 尝试释放未获取的信号量——预期失败（未take直接give）
 *      if( xSemaphoreGive( xSemaphore ) != pdTRUE )
 *      {
 *          // 此调用应失败，因释放信号量前必须先获取它
 *      }
 *
 *      // 获取信号量——不阻塞（若无法立即获取则放弃）
 *      if( xSemaphoreTake( xSemaphore, ( TickType_t ) 0 ) )
 *      {
 *          // 成功获取信号量，可安全访问共享资源
 *
 *          // ... 访问共享资源的逻辑 ...
 *
 *          // 完成共享资源访问，释放信号量
 *          if( xSemaphoreGive( xSemaphore ) != pdTRUE )
 *          {
 *              // 此调用不应失败，因能执行到此处说明已成功获取信号量
 *          }
 *      }
 *  }
 * }
 */
```

*param1：*要释放的互斥量句柄——该句柄在互斥量创建时返回

*return：*成功返回 *pdTRUE*；失败返回 *pdFALSE*

## 优先级反转

直接看《Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0》中的描述即可，现在有三个任务，HP高优先级任务，MP中等优先级任务，LP低优先级任务。

![img](https://picx.zhimg.com/80/v2-147be6a8b495630a0e28c54f0fbc7503_1440w.png?source=ccfced1a)

当LP正在使用资源，HP正等待获取资源时，MP进入就绪态，此时LP被MP抢占了，结果将会是高优先级任务在等待低优先级任务，而低优先级任务甚至都无法执行。怎么样去处理这个问题？

## 优先级继承

优先级继承的工作原理是，将互斥锁持有者的优先级暂时提升到试图获取同一互斥锁的最高优先级任务的优先级。持有互斥锁的低优先级任务 “继承” 等待该互斥锁的任务的优先级。

![img](https://pic1.zhimg.com/80/v2-5ff8978c521189d1617d1809951f80d9_1440w.png?source=ccfced1a)

LP使用资源时会继承HP优先级，不会被MP打断。

## **GCC** **内联汇编的标准格式**

在讲源码前有必要提一下本文涉及到的**GCC** **内联汇编的标准格式**，后面记得回来看看，理解下

```c
__asm volatile (
    "汇编指令1;"  // 第1段：要执行的汇编指令（必须有）
    "汇编指令2;"
    : [输出操作数列表]  // 第2段：汇编指令的“输出参数”（可选，无则留空）
    : [输入操作数列表]  // 第3段：汇编指令的“输入参数”（可选，无则留空）
    : [破坏描述符列表]  // 第4段：汇编指令可能“破坏”的资源（可选，无则留空）
);
```

使用%0、%1等占位符表示后续操作数列表中的变量（按顺序对应），不懂可以跟着下面理解

## 临界区与暂停调度器

### *（源码）*基本临界区

ARM CM3（ARM Cortex-M3）移植版本

先看一看怎么进入临界区

```c
#define taskENTER_CRITICAL()                 portENTER_CRITICAL()

#define portENTER_CRITICAL()                 vPortEnterCritical()

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;

    /* 这不是可用于中断上下文的进入临界区函数版本，因此
     * 如果从 interrupt 上下文调用此函数，assert() 会触发。
     * 只有以 "FromISR" 结尾的 API 函数才能在中断中使用。
     * 仅当临界区嵌套计数为 1 时才进行断言，以防止在
     * assert 函数本身也使用临界区时发生递归调用问题。 */
    if( uxCriticalNesting == 1 )
    {
        configASSERT( ( portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK ) == 0 );
    }
}
```

*taskENTER_CRITICAL*就是 *vPortEnterCritical*，

第一步调用了函数*portDISABLE_INTERRUPTS*

```c
#define portDISABLE_INTERRUPTS()                  vPortRaiseBASEPRI()
```

*portDISABLE_INTERRUPTS*就是 *vPortRaiseBASEPRI* ，*vPortRaiseBASEPRI()* 定义如下

```c
// 使用FORCE_INLINE 强制编译器内联这个函数，static 限制作用域
portFORCE_INLINE static void vPortRaiseBASEPRI( void )
{
    uint32_t ulNewBASEPRI;  // 用于存储要写入BASEPRI寄存器的值

    // 内联汇编块，使用GCC格式
    __asm volatile
    (
        // 将配置的最大系统调用中断优先级值移动到寄存器%0
        "   mov %0, %1                                              \n" \
        // 将值写入BASEPRI寄存器，该寄存器控制中断屏蔽
        "   msr basepri, %0                                         \n" \
        // 指令同步屏障，确保前面的指令执行完成
        "   isb                                                     \n" \
        // 数据同步屏障，确保内存操作完成
        "   dsb                                                     \n"
        // 输出操作数：将结果存入ulNewBASEPRI变量，使用寄存器传递
        : "=r" ( ulNewBASEPRI ) 
        // 输入操作数：使用立即数传递configMAX_SYSCALL_INTERRUPT_PRIORITY
        : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) 
        // 破坏描述符：通知编译器内存已被修改
        : "memory"
    );
}
```

参考上面提到过的**GCC 内联汇编的标准格式**。输入参数（*%1*） *configMAX_SYSCALL_INTERRUPT_PRIORITY ()，*输出参数（*%0*） *ulNewBASEPRI* 。

函数主要把*configMAX_SYSCALL_INTERRUPT_PRIORITY* 写到*basepri*寄存器中

> *configMAX_SYSCALL_INTERRUPT_PRIORITY* 是什么？ *#define   configMAX_SYSCALL_INTERRUPT_PRIORITY     0* 默认值被设置为最高中断优先级（0）。 *basepri*寄存器是什么？ BASEPRI是 ARM Cortex-M 系列处理器中的一个**中断优先级屏蔽寄存器,**当 BASEPRI 被设置为某个值 X 时，**所有优先级数值大于** **X** **的中断（即优先级更低）会被屏蔽**，无法响应；优先级数值优先级数值小于或等于 X 的中断（即优先级更高）不受影响，仍可正常响应；

此时*basepri*寄存器值为0，意味着所有优先级数值大于0的中断（即优先级更低）会被屏蔽，即所有中断均不被屏蔽。

进入临界区，主要是开启所有中断，使所有中断均不被屏蔽。

接下来看一看怎么退出临界区

```c
#define taskEXIT_CRITICAL()                  portEXIT_CRITICAL()

#define portEXIT_CRITICAL()                       vPortExitCritical()

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;

    if( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}
```

之前进入一次临界区将 *uxCriticalNesting++*，现在要退出一次就 *uxCriticalNesting--* ，当*uxCriticalNesting ==0*，说明对每次进入都执行了退出，然后调用了*vPortExitCritical*

```c
#define portENABLE_INTERRUPTS()                   vPortSetBASEPRI( 0 )

portFORCE_INLINE static void vPortSetBASEPRI( uint32_t ulNewMaskValue )
{
    __asm volatile
    (
        "   msr basepri, %0 " ::"r" ( ulNewMaskValue ) : "memory"
    );
}
```

完全退出临界区其实就是把basepri寄存器写入0，使所有中断均不被屏蔽。

默认情况下，进入退出临界区都是使所有中断均不被屏蔽。这样似乎没有什么用，不过，可以通过改写进入临界区提到的宏 *configMAX_SYSCALL_INTERRUPT_PRIORITY* ，使临界区内一些中断被屏蔽。

### *（源码）*基本临界区--中断安全版

先看进入

```c
#define taskENTER_CRITICAL_FROM_ISR()    portSET_INTERRUPT_MASK_FROM_ISR()

#define portSET_INTERRUPT_MASK_FROM_ISR()         ulPortRaiseBASEPRI()

// 强制内联的静态函数，返回值为uint32_t类型
portFORCE_INLINE static uint32_t ulPortRaiseBASEPRI( void )
{
    // 定义变量：存储原始BASEPRI值和新值
    uint32_t ulOriginalBASEPRI, ulNewBASEPRI;

    // GCC内联汇编块
    __asm volatile
    (
        // 读取当前BASEPRI寄存器的值到ulOriginalBASEPRI变量（%0）
        "   mrs %0, basepri                                         \n" \
        // 将配置的优先级值移动到ulNewBASEPRI变量对应的寄存器（%1）
        "   mov %1, %2                                              \n" \
        // 将新值写入BASEPRI寄存器，设置中断屏蔽阈值
        "   msr basepri, %1                                         \n" \
        // 指令同步屏障，确保指令执行顺序
        "   isb                                                     \n" \
        // 数据同步屏障，确保内存操作完成
        "   dsb                                                     \n"
        // 输出操作数：%0对应ulOriginalBASEPRI，%1对应ulNewBASEPRI
        : "=r" ( ulOriginalBASEPRI ), "=r" ( ulNewBASEPRI ) 
        // 输入操作数：%2对应配置的优先级值
        : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) 
        // 破坏描述符：通知编译器内存已被修改
        : "memory"
    );

    /* 这个返回值在某些场景下可能不会被使用，但为了防止编译器警告必须存在 */
    return ulOriginalBASEPRI;
}
```

其实和非中断安全版差不多，只是多了一个输出，把原来basepri寄存器的值输出了。

接下来看退出

```c
#define taskEXIT_CRITICAL_FROM_ISR( x )    portCLEAR_INTERRUPT_MASK_FROM_ISR( x )

#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )    vPortSetBASEPRI( x )

portFORCE_INLINE static void vPortSetBASEPRI( uint32_t ulNewMaskValue )
{
    __asm volatile
    (
        "   msr basepri, %0 " ::"r" ( ulNewMaskValue ) : "memory"
    );
}
```

和非中断安全版差不多。

### *（源码）*暂停（或锁定）调度器

从暂停调度器开始

```c
void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;

// 函数：挂起所有任务调度（挂起调度器，使任务切换暂时失效）
void vTaskSuspendAll( void )
{
    // 单核系统（核心数==1）的实现逻辑
    #if ( configNUMBER_OF_CORES == 1 )
    {
        /* 注释说明：
         * 1. 由于uxSchedulerSuspended是BaseType_t类型（原子操作可保证安全），此处无需进入临界区；
         * 2. 关于为何无需临界区的详细解释，可参考FreeRTOS论坛链接：https://goo.gl/wu4acr */

        /* portSOFTWARE_BARRIER()仅在“非实时的模拟/仿真平台”中实现，
         * 作用是阻止编译器对“屏障前后的代码”进行重排序（保证代码执行顺序）。 */
        portSOFTWARE_BARRIER();  // 软件屏障：防止编译器乱序优化

        /* 注释说明：
         * 1. 当uxSchedulerSuspended不为0时，表示调度器已被挂起；
         * 2. 使用“自增”操作支持嵌套调用（多次调用vTaskSuspendAll()，需对应次数的vTaskResumeAll()才能恢复）。 */
        // 调度器挂起计数器自增（支持嵌套挂起）
        uxSchedulerSuspended = ( UBaseType_t ) ( uxSchedulerSuspended + 1U );

        /* 对“需要实时行为的平台”，此宏用于：
         * 1. 阻止编译器重排序；
         * 2. 阻止CPU乱序执行（通过内存屏障指令），确保自增结果立即对其他代码可见。 */
        portMEMORY_BARRIER();  // 内存屏障：确保自增操作的结果立即生效
    }
    #else /* 多核系统（核心数>1）的实现逻辑 */
         /*       不考虑多核          */ 
    #endif /* configNUMBER_OF_CORES == 1 */

}
```

在<FreeRTOSconfig.h>中有这样的定义

```c
#ifndef portMEMORY_BARRIER
    #define portMEMORY_BARRIER()
#endif

#ifndef portSOFTWARE_BARRIER
    #define portSOFTWARE_BARRIER()
#endif
```

所以挂起调度器函数实际上就是

```c
// 函数：挂起所有任务调度（挂起调度器，使任务切换暂时失效）
void vTaskSuspendAll( void )
{
   uxSchedulerSuspended = ( UBaseType_t ) ( uxSchedulerSuspended + 1U );
}
```

给一个全局量标记+1，表示执行了一次挂起调度器操作。

接下来看看恢复调度器

```c
BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;

// 函数：恢复所有任务调度（与vTaskSuspendAll配对，解除调度器挂起状态）
BaseType_t xTaskResumeAll( void )
{
    TCB_t * pxTCB = NULL;               // 指向任务控制块（TCB）的指针，用于处理就绪任务
    BaseType_t xAlreadyYielded = pdFALSE;  // 返回值：标记是否已触发任务切换（pdTRUE=已切换）

    {
        /* 注释说明：
         * 1. 调度器挂起期间，中断可能导致任务从“事件等待链表”（如信号量、队列链表）中移除；
         * 2. 被移除的任务不会直接加入就绪链表，而是先放入“待处理就绪链表”（xPendingReadyList）；
         * 3. 调度器恢复后，需将待处理就绪链表中的任务移到正确的就绪链表，确保其能被调度。 */
        taskENTER_CRITICAL();  // 进入临界区，确保任务链表操作的原子性
        {
            BaseType_t xCoreID;  // 当前核心ID（用于多核场景下的核心专属操作）
            xCoreID = ( BaseType_t ) portGET_CORE_ID();  // 获取当前执行此函数的核心ID，单核时就是0

            // 调度器挂起计数器自减（嵌套挂起时，需减到0才真正恢复调度）
            uxSchedulerSuspended = ( UBaseType_t ) ( uxSchedulerSuspended - 1U );

            // 若计数器减为0，说明调度器要完全恢复运行（非嵌套挂起的中间状态）
            if( uxSchedulerSuspended == ( UBaseType_t ) 0U )
            {
                // 若系统中存在任务（uxCurrentNumberOfTasks > 0），才处理待就绪任务
                if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
                {
                    /* 第一步：将“待处理就绪链表”（xPendingReadyList）中的所有任务，移到正确的就绪链表 */
                    while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )  // 循环直到待处理链表为空
                    {
                        /* MISRA规则兼容注释：空指针赋值相关说明，详见链接 */
                        /* 从待处理就绪链表的头部获取任务的TCB */
                        pxTCB = listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
                        // 从任务的“事件链表项”（xEventListItem）所在链表中移除（解除事件等待关联）
                        listREMOVE_ITEM( &( pxTCB->xEventListItem ) );
                        // 从任务的“状态链表项”（xStateListItem）所在链表中移除（解除原状态关联）
                        listREMOVE_ITEM( &( pxTCB->xStateListItem ) );
                        // 将任务加入正确的就绪链表（根据任务优先级，放入对应的pxReadyTasksLists链表）
                        prvAddTaskToReadyList( pxTCB );

                        {
                            /* 若恢复的任务优先级 > 当前运行任务的优先级，需标记“待切换” */
                            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
                            {
                                xYieldPendings[ xCoreID ] = pdTRUE;  // 标记当前核心需要任务切换
                            }else{}
                        }
                    }

                    // 若有任务从待处理链表移出，需重置“下一个任务解阻塞时间”
                    if( pxTCB != NULL )
                    {
                        /* 注释说明：
                         * 调度器挂起期间，任务解除阻塞可能导致“下一个解阻塞时间”未及时更新；
                         * 此时重置该时间，确保低功耗Tickless模式下，系统不会因无效时间误唤醒，优化功耗。 */
                        prvResetNextTaskUnblockTime();
                    }

                    /* 第二步：处理调度器挂起期间积累的Tick（时钟节拍）
                     * 1. 确保Tick计数不丢失，避免任务延迟时间计算错误；
                     * 2. 触发延迟任务的超时唤醒（如vTaskDelay的任务到点恢复）。 */
                    {
                        TickType_t xPendedCounts = xPendedTicks; /* 复制积累的Tick数（避免volatile变量多次读取） */

                        // 若存在积累的Tick，逐个处理
                        if( xPendedCounts > ( TickType_t ) 0U )
                        {
                            do
                            {
                                /* 调用xTaskIncrementTick()处理单个Tick：
                                 * - 更新系统Tick计数；
                                 * - 检查延迟任务是否超时，超时则唤醒；
                                 * - 返回pdTRUE表示需要触发任务切换（如高优先级任务被唤醒）。 */
                                if( xTaskIncrementTick() != pdFALSE )
                                {
                                    /* 注释说明：多核场景下，xTaskIncrementTick()会中断其他核心；
                                     * 标记当前核心需要任务切换。 */
                                    xYieldPendings[ xCoreID ] = pdTRUE;
                                }else{}

                                xPendedCounts--;  // 处理完一个Tick，计数减1
                            } while( xPendedCounts > ( TickType_t ) 0U );

                            xPendedTicks = 0;  // 重置积累的Tick数
                        }else{}
                    }

                    /* 第三步：若当前核心标记了“待切换”，触发任务切换（仅抢占式调度有效） */
                    if( xYieldPendings[ xCoreID ] != pdFALSE )
                    {
                        // 若启用抢占式调度，标记“已触发切换”
                        #if ( configUSE_PREEMPTION != 0 )
                        {
                            xAlreadyYielded = pdTRUE;
                        }
                        #endif /* configUSE_PREEMPTION != 0 */

                        // 单核系统：触发当前核心的任务切换
                        #if ( configNUMBER_OF_CORES == 1 )
                        {
                            taskYIELD_TASK_CORE_IF_USING_PREEMPTION( pxCurrentTCB );
                        }
                        #endif /* configNUMBER_OF_CORES == 1 */
                    }else{}
                }
            }else{}
        }
        taskEXIT_CRITICAL();  // 退出临界区
   

    return xAlreadyYielded;  // 返回结果：pdTRUE=已触发任务切换，pdFALSE=未切换
}
```

之前挂起调度器时做了标记+1，现在先让标记-1。

```c
uxSchedulerSuspended = ( UBaseType_t ) ( uxSchedulerSuspended - 1U );
```

如果标记为0说明对每次挂起都执行了恢复，正式开始恢复：

 **第一步：将“待处理就绪链表”（xPendingReadyList）中的所有任务，移到正确的就绪链表** 

```c
while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )  // 循环直到待处理链表为空
{
    /* 从待处理就绪链表的头部获取任务的TCB */
    pxTCB = listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
    // 从任务的“事件链表项”（xEventListItem）所在链表中移除（解除事件等待关联）
    listREMOVE_ITEM( &( pxTCB->xEventListItem ) );
    // 从任务的“状态链表项”（xStateListItem）所在链表中移除（解除原状态关联）
    listREMOVE_ITEM( &( pxTCB->xStateListItem ) );
    // 将任务加入正确的就绪链表（根据任务优先级，放入对应的pxReadyTasksLists链表）
    prvAddTaskToReadyList( pxTCB );
    /* 若恢复的任务优先级 > 当前运行任务的优先级，需标记“待切换” */
    if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
         xYieldPendings[ xCoreID ] = pdTRUE;  // 标记当前核心需要任务切换
    }else{}
}
// 若有任务从待处理链表移出，需重置“下一个任务解阻塞时间”
if( pxTCB != NULL )
{
    /* 注释说明：
     * 调度器挂起期间，任务解除阻塞可能导致“下一个解阻塞时间”未及时更新；
     * 此时重置该时间，确保低功耗Tickless模式下，系统不会因无效时间误唤醒，优化功耗。 */
     prvResetNextTaskUnblockTime();
}
```

> 这里先来说说什么是待就绪链表。 这里可不是就绪链表，待就绪链表是在调度器挂起状态下（就是标记为0）有任务变成了就绪态，但是此时调度器是挂起的，先不应该去处理这些任务（那就是不应该放到就绪链表中），那就先放到待就绪链表，等我们恢复调度器时（也就是现在）再处理。

这里处理待就绪链表就是把任务从待就绪链表放到就绪链表，还要移除原来链表项与其他链表的关系。（其中用到的函数在本专栏其它地方提到过），最后标记是否需要任务切换（使恢复后处理高优先级任务）。

处理完“待处理就绪链表”中的所有任务，需重置“下一个任务解阻塞时间”

> 什么是下一个任务解阻塞时间？ 阻塞的任务会放在一个链表里，一直检查这个链表里的任务是不是该运行了那太费劲了，所以用了“下一个任务解阻塞时间”取记录下一个任务解阻塞的时间，检查这个值就好了。

实现如下

```c
static void prvResetNextTaskUnblockTime( void )
{
    if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
    {
        /* 当前延迟链表为空，将xNextTaskUnblockTime设为最大可能值（portMAX_DELAY）。
         * 这样可以确保：在延迟链表添加任务前，“xTickCount >= xNextTaskUnblockTime”的判断几乎不会成立，
         * 避免误唤醒无延迟任务的情况。 */
        xNextTaskUnblockTime = portMAX_DELAY;
    }
    else
    {
        /* 当前延迟链表非空，获取链表头部任务的“链表项值”。
         * 该值即为链表头部任务的唤醒时间——头部任务是最早需要从阻塞状态唤醒的任务。 */
        xNextTaskUnblockTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxDelayedTaskList );
    }
}
```

链表中的项是按解除阻塞的时刻来排的，检查头一个项就好，这就是最近要运行的任务。

**第二步：处理调度器挂起期间积累的Tick（时钟节拍）**

```c
/* 1. 确保Tick计数不丢失，避免任务延迟时间计算错误；
 * 2. 触发延迟任务的超时唤醒（如vTaskDelay的任务到点恢复）。 */
TickType_t xPendedCounts = xPendedTicks; /* 复制积累的Tick数（避免volatile变量多次读取） */

// 若存在积累的Tick，逐个处理
if( xPendedCounts > ( TickType_t ) 0U )
{
    do
    {
        /* 调用xTaskIncrementTick()处理单个Tick：
         * - 更新系统Tick计数；
         * - 检查延迟任务是否超时，超时则唤醒；
         * - 返回pdTRUE表示需要触发任务切换（如高优先级任务被唤醒）。 */
         if( xTaskIncrementTick() != pdFALSE )
         {
             /* 注释说明：多核场景下，xTaskIncrementTick()会中断其他核心；
             * 标记当前核心需要任务切换。 */
             xYieldPendings[ xCoreID ] = pdTRUE;
         }else{}

    	xPendedCounts--;  // 处理完一个Tick，计数减1
    } while( xPendedCounts > ( TickType_t ) 0U );

    xPendedTicks = 0;  // 重置积累的Tick数
}
```

> 先来说下什么是调度器挂起期间积累的Tick 就和之前说过的调度器挂起后不用就绪链表用待就绪链表一样，那调度器挂起后系统节拍是要暂停的（不然挂起调度器有什么用），这个时候节拍加到了积累的Tick（也就是xPendedTicks），现在就要把时间加过来，继续运行调度器了。

怎么把累计的时间加回来？ 这里通过一个while循环不断运行*xTaskIncrementTick，*知道累计时间减到0。

*xTaskIncrementTick*实现如下

```c
// 系统滴答计数递增函数，返回是否需要触发上下文切换（pdTRUE=需要切换，pdFALSE=无需切换）
BaseType_t xTaskIncrementTick( void )
{
    TCB_t * pxTCB;  // 指向任务控制块（TCB）的指针，用于处理超时任务
    TickType_t xItemValue;  // 存储任务的唤醒时间（从任务状态链表项中获取）
    BaseType_t xSwitchRequired = pdFALSE;  // 返回值：标记是否需要触发任务切换

    /* 注释说明：
     * 1. 每次滴答中断都需递增系统滴答计数；
     * 2. 仅核心0负责递增全局滴答计数（xTickCount）；
     * 3. 若调度器已挂起（uxSchedulerSuspended > 0），则不递增xTickCount，而是递增挂起滴答计数（xPendedTicks）；
     * 4. 挂起的滴答会在调度器恢复（xTaskResumeAll）时由调用恢复函数的核心统一处理。 */
    if( uxSchedulerSuspended == ( UBaseType_t ) 0U )  // 调度器未挂起：正常处理滴答
    {
        /* 小优化：将“当前滴答+1”的结果存入常量，避免后续代码重复计算 */
        const TickType_t xConstTickCount = xTickCount + ( TickType_t ) 1;

        /* 1. 递增全局滴答计数（xTickCount）；
         * 2. 若计数回绕到0（如32位TickType_t从0xFFFFFFFF变为0），需切换延迟任务链表，
         *    避免回绕后唤醒逻辑错误（通过taskSWITCH_DELAYED_LISTS()实现）。 */
        xTickCount = xConstTickCount;

        if( xConstTickCount == ( TickType_t ) 0U )  // 滴答计数回绕
        {
            // 切换延迟链表：将pxDelayedTaskList（当前延迟链表）与pxOverflowDelayedTaskList（溢出延迟链表）互换
            taskSWITCH_DELAYED_LISTS();
        }else{}

        /* 检查当前滴答是否达到“下一个任务解阻塞时间”（xNextTaskUnblockTime）：
         * 若达到，说明有延迟任务超时，需唤醒并移到就绪链表；
         * 延迟链表中的任务按唤醒时间升序排列，一旦遇到未超时任务，后续任务均未超时，可终止遍历。 */
        if( xConstTickCount >= xNextTaskUnblockTime )
        {
            for( ; ; )  // 无限循环，直到无超时任务或延迟链表为空
            {
                if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )  // 延迟链表为空
                {
                    /* 无延迟任务，将xNextTaskUnblockTime设为最大值（portMAX_DELAY），
                     * 确保下次滴答中断时，无需进入此遍历逻辑（优化性能） */
                    xNextTaskUnblockTime = portMAX_DELAY;
                    break;  // 退出循环
                }
                else  // 延迟链表非空，处理头部任务（唤醒时间最早的任务）
                {
                    /* 获取延迟链表头部任务的TCB（listGET_OWNER_OF_HEAD_ENTRY返回链表项的所有者，即TCB） */
                    pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
                    // 获取该任务的唤醒时间（存储在任务状态链表项xStateListItem的value字段中）
                    xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

                    if( xConstTickCount < xItemValue )  // 当前滴答＜任务唤醒时间（任务未超时）
                    {
                        /* 将此任务的唤醒时间设为“下一个解阻塞时间”，下次滴答直接判断此时间即可 */
                        xNextTaskUnblockTime = xItemValue;
                        break;  // 退出循环（后续任务唤醒时间更晚，无需处理）
                    }else  // 任务已超时，需从阻塞态唤醒
                    {}

                    // 步骤1：从延迟链表中移除任务的状态链表项（解除阻塞态关联）
                    listREMOVE_ITEM( &( pxTCB->xStateListItem ) );

                    // 步骤2：若任务同时在等待事件（如信号量、事件组），还需从事件链表中移除其事件链表项
                    if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                    {
                        listREMOVE_ITEM( &( pxTCB->xEventListItem ) );
                    }else{}

                    // 步骤3：将任务加入就绪链表（根据优先级放入对应的pxReadyTasksLists链表）
                    prvAddTaskToReadyList( pxTCB );

                    /* 若启用抢占式调度，判断是否需要触发任务切换：
                     * 仅当被唤醒任务优先级＞当前运行任务优先级时，才需要切换（同等优先级通过时间切片处理） */
                    #if ( configUSE_PREEMPTION == 1 )
                    {
                        #if ( configNUMBER_OF_CORES == 1 )  // 单核场景
                        {
                            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
                            {
                                xSwitchRequired = pdTRUE;  // 标记需要切换
                            }else{}
                        }
                        #else  // 多核场景：调用prvYieldForTask判断是否需要触发对应核心的切换
                        #endif /* configNUMBER_OF_CORES == 1 */
                    }
                    #endif /* configUSE_PREEMPTION == 1 */
                }
            }
        }

        /* 若启用“抢占式调度+时间切片”：
         * 同等优先级的任务会按滴答间隔轮流占用CPU（时间切片），每次滴答后需检查是否需要切换到同优先级其他任务 */
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
        {
            #if ( configNUMBER_OF_CORES == 1 )  // 单核场景
            {
                // 若当前优先级的就绪链表中任务数＞1（存在同优先级其他任务）
                if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > 1U )
                {
                    xSwitchRequired = pdTRUE;  // 标记需要切换（触发时间切片）
                }else{}
            }
            #else  // 多核场景：遍历每个核心，检查是否有同优先级任务需时间切片
            #endif /* configNUMBER_OF_CORES == 1 */
        }
        #endif /* ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) */

        /* 若启用滴答钩子函数（configUSE_TICK_HOOK == 1）：
         * 调用用户实现的vApplicationTickHook()，用于执行周期性任务（如系统监控、数据采样）；
         * 仅当无挂起滴答（xPendedTicks == 0）时调用，避免钩子函数在调度器不稳定状态下执行。 */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            if( xPendedTicks == ( TickType_t ) 0 )
            {
                vApplicationTickHook();
            }else{}
        }
        #endif /* configUSE_TICK_HOOK */

        /* 抢占式调度场景：汇总所有需要切换的标记，最终决定是否触发切换 */
        #if ( configUSE_PREEMPTION == 1 )
        {
            #if ( configNUMBER_OF_CORES == 1 )  // 单核场景
            {
                // 若存在挂起的切换请求（xYieldPendings[0]，如xTaskResume触发的切换）
                if( xYieldPendings[ 0 ] != pdFALSE )
                {
                    xSwitchRequired = pdTRUE;
                }else{}
            }
            #else  
            #endif /* configNUMBER_OF_CORES == 1 */
        }
        #endif /* configUSE_PREEMPTION == 1 */
    }
    else  // 调度器已挂起：不处理任务唤醒，仅递增挂起滴答计数
    {
        xPendedTicks += 1U;  // 挂起滴答计数递增，待调度器恢复后统一处理

        /* 即使调度器挂起，滴答钩子函数仍需按固定间隔调用（用户可能依赖其周期性执行） */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            vApplicationTickHook();
        }
        #endif
    }

    return xSwitchRequired;  // 返回结果：pdTRUE=需要触发任务切换，pdFALSE=无需切换
}
```

首先，递增全局滴答计数（累计时间加一个到系统时间），处理溢出（溢出要切换延迟链表）

```c
/* 小优化：将“当前滴答+1”的结果存入常量，避免后续代码重复计算 */
const TickType_t xConstTickCount = xTickCount + ( TickType_t ) 1;

/* 1. 递增全局滴答计数（xTickCount）；
 * 2. 若计数回绕到0（如32位TickType_t从0xFFFFFFFF变为0），需切换延迟任务链表，
 *    避免回绕后唤醒逻辑错误（通过taskSWITCH_DELAYED_LISTS()实现）。 */
xTickCount = xConstTickCount;

if( xConstTickCount == ( TickType_t ) 0U )  // 滴答计数回绕
{
    // 切换延迟链表：将pxDelayedTaskList（当前延迟链表）与pxOverflowDelayedTaskList（溢出延迟链表）互换
    taskSWITCH_DELAYED_LISTS();
}else{}
```

然后，如果现在的系统时间是一个任务的唤醒时间，那就要从延迟链表里拿出来，放到就绪链表中，更新下一个任务的唤醒时间（*xNextTaskUnblockTime* ），如果需要上下文切换（唤醒任务优先级高），做一个标记。

```c
if( xConstTickCount >= xNextTaskUnblockTime )
{
    for( ; ; )  // 无限循环，直到无超时任务或延迟链表为空
    {
        if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )  // 延迟链表为空
        {
            /* 无延迟任务，将xNextTaskUnblockTime设为最大值（portMAX_DELAY），
            * 确保下次滴答中断时，无需进入此遍历逻辑（优化性能） */
            xNextTaskUnblockTime = portMAX_DELAY;
            break;  // 退出循环
        }
        else  // 延迟链表非空，处理头部任务（唤醒时间最早的任务）
        {
            /* 获取延迟链表头部任务的TCB（listGET_OWNER_OF_HEAD_ENTRY返回链表项的所有者，即TCB） */
            pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
            // 获取该任务的唤醒时间（存储在任务状态链表项xStateListItem的value字段中）
            xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

            if( xConstTickCount < xItemValue )  // 当前滴答＜任务唤醒时间（任务未超时）
            {
                /* 将此任务的唤醒时间设为“下一个解阻塞时间”，下次滴答直接判断此时间即可 */
                xNextTaskUnblockTime = xItemValue;
                break;  // 退出循环（后续任务唤醒时间更晚，无需处理）
            }else  // 任务已超时，需从阻塞态唤醒
            {}

            // 步骤1：从延迟链表中移除任务的状态链表项（解除阻塞态关联）
            listREMOVE_ITEM( &( pxTCB->xStateListItem ) );

            // 步骤2：若任务同时在等待事件（如信号量、事件组），还需从事件链表中移除其事件链表项
            if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
            {
            	listREMOVE_ITEM( &( pxTCB->xEventListItem ) );
            }else{}

            // 步骤3：将任务加入就绪链表（根据优先级放入对应的pxReadyTasksLists链表）
            prvAddTaskToReadyList( pxTCB );

            /* 若启用抢占式调度，判断是否需要触发任务切换：
            * 仅当被唤醒任务优先级＞当前运行任务优先级时，才需要切换（同等优先级通过时间切片处理） */
            #if ( configUSE_PREEMPTION == 1 )
            {
                #if ( configNUMBER_OF_CORES == 1 )  // 单核场景
                {
                    if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
                    {
                    	xSwitchRequired = pdTRUE;  // 标记需要切换
                    }else{}
                }
                #else  // 多核场景：调用prvYieldForTask判断是否需要触发对应核心的切换
                #endif /* configNUMBER_OF_CORES == 1 */
            }
            #endif /* configUSE_PREEMPTION == 1 */
        }
    }
}
```

然后，针对“抢占式调度+时间切片”（这是什么，之后在调度策略说明），查看是否需要上下文切换。

```c
/* 若启用“抢占式调度+时间切片”：
 * 同等优先级的任务会按滴答间隔轮流占用CPU（时间切片），每次滴答后需检查是否需要切换到同优先级其他任务 */
#if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
{
    #if ( configNUMBER_OF_CORES == 1 )  // 单核场景
    {
        // 若当前优先级的就绪链表中任务数＞1（存在同优先级其他任务）
        if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > 1U )
        {
        	xSwitchRequired = pdTRUE;  // 标记需要切换（触发时间切片）
        }else{}
    }
    #else  // 多核场景：遍历每个核心，检查是否有同优先级任务需时间切片
    #endif /* configNUMBER_OF_CORES == 1 */
}
#endif /* ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) */
```

最后处理用户定义滴答钩子函数

```c
/* 若启用滴答钩子函数（configUSE_TICK_HOOK == 1）：
* 调用用户实现的vApplicationTickHook()，用于执行周期性任务（如系统监控、数据采样）；
* 仅当无挂起滴答（xPendedTicks == 0）时调用，避免钩子函数在调度器不稳定状态下执行。 */
#if ( configUSE_TICK_HOOK == 1 )
{
    if( xPendedTicks == ( TickType_t ) 0 )
    {
    	vApplicationTickHook();
    }else{}
}
#endif /* configUSE_TICK_HOOK */
```

### （*源码*）创建互斥锁

```c
#define xSemaphoreCreateMutex()    xQueueCreateMutex( queueQUEUE_TYPE_MUTEX )

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

    // 1. 调用通用队列创建函数，按互斥锁的配置创建队列
    // 队列容量=1、元素大小=0、类型=ucQueueType（互斥锁类型）
    xNewQueue = xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );
    // 2. 初始化互斥锁专属逻辑（如优先级继承相关成员）
    prvInitialiseMutex( ( Queue_t * ) xNewQueue );

    return xNewQueue;  // 返回创建的互斥锁句柄（若xQueueGenericCreate失败，此处返回NULL）
}
```

创建互斥锁其实就是创建了一个队列容量为1，队列元素大小为0的队列（FreeRTOS中的），互斥锁的初始化调用了*prvInitialiseMutex*  （队列相关函数不再介绍）

```c
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

        /* 4. 将互斥锁初始化为“可用状态”：
        * 调用通用队列发送函数，向队列发送一个“空数据”（因元素大小为0，无需实际数据），
        * 使队列的消息数（uxMessagesWaiting）从0变为1——对应互斥锁“初始可用”的状态；
        * 等待时间设为0，表示若队列满则立即返回（此处队列容量为1，初始为空，必然发送成功）。 */
        ( void ) xQueueGenericSend( pxNewQueue, NULL, ( TickType_t ) 0U, queueSEND_TO_BACK );
    }else  // 队列结构体指针无效（NULL），互斥锁初始化失败
    {}
}
```

![img](https://picx.zhimg.com/80/v2-2eaf4ea4b41c73e6c0cded76b66af5fd_1440w.png?source=ccfced1a)

### （*源码*）获得互斥锁

获得互斥锁和信号量是同一个函数。

```c
#define xSemaphoreTake( xSemaphore, xBlockTime )    xQueueSemaphoreTake( ( xSemaphore ), ( xBlockTime ) )

// 函数定义：获取信号量/互斥锁（底层实现，对应应用层的xSemaphoreTake()）
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
                    else{}
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
                    }else{}
                }else{}

                // 退出临界区
                taskEXIT_CRITICAL();

                return pdPASS;  // 成功获取信号量，返回pdPASS
            }
            /* 分支2：信号量计数=0（不可用），处理等待逻辑 */
            else
            {
                /* 子分支2.1：无等待时间（xTicksToWait=0），直接返回失败 */
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL();  // 退出临界区

                    return errQUEUE_EMPTY;  // 信号量不可用，返回失败
                }
                /* 子分支2.2：有等待时间，但未设置超时起始时间，初始化超时结构体 */
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );  // 记录当前时间为超时起始点
                    xEntryTimeSet = pdTRUE;  // 标记“超时起始时间已设置”
                }else{}
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
                    }else{}
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
                }else{}
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

                return errQUEUE_EMPTY;  // 超时且信号量仍不可用，返回失败
            }else{}
        }
    }
}
```

> 文中的注释是信号量，这是因为信号量和互斥量使用的都是这个函数

**第一步**：进入临界区，尝试获取互斥量。如果互斥量不为零（事实上只有1和0），此时的任务可以获得，标记互斥量为0（*uxMessagesWaiting* -1），让其他任务不能获取，

*pxQueue->uxMessagesWaiting =( UBaseType_t )( uxSemaphoreCount -( UBaseType_t )1);*

标记此时获得互斥量的任务。

*pxQueue->u.xSemaphore.xMutexHolder =pvTaskIncrementMutexHeldCount();*

然后，检查是否有任务阻塞等待“释放信号量”（这里这个不重要因为只能由一个任务获得，不会有其他任务还能发送），返回获取成功；

*return pdPASS;*

```c
        // 1. 进入临界区：保护队列状态（避免并发修改消息数、任务等待列表）
        taskENTER_CRITICAL();
        {
            /* 信号量本质是“元素大小为0的队列”，队列的“消息数”（uxMessagesWaiting）即信号量的“计数” */
            const UBaseType_t uxSemaphoreCount = pxQueue->uxMessagesWaiting;

            /* 分支1：信号量计数>0（可用），执行获取逻辑 */
            if( uxSemaphoreCount > ( UBaseType_t ) 0 )
            {
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
                    else{}
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
                    }else{}
                }else{}

                // 退出临界区
                taskEXIT_CRITICAL();

                return pdPASS;  // 成功获取信号量，返回pdPASS
            }
```

如果互斥量不能获取且等待时间为0，

*xTicksToWait ==( TickType_t )0*

直接返回获取失败；

*return errQUEUE_EMPTY;*

如果互斥量不能获取且等待时间不为0，那就记录当前时间，进入阻塞等待逻辑。

*vTaskInternalSetTimeOutState(&xTimeOut );// 记录当前时间为超时起始点*

```c
/* 分支2：信号量计数=0（不可用），处理等待逻辑 */
else
{
    /* 子分支2.1：无等待时间（xTicksToWait=0），直接返回失败 */
    if( xTicksToWait == ( TickType_t ) 0 )
    {
        taskEXIT_CRITICAL();  // 退出临界区

        return errQUEUE_EMPTY;  // 信号量不可用，返回失败
    }
    /* 子分支2.2：有等待时间，但未设置超时起始时间，初始化超时结构体 */
    else if( xEntryTimeSet == pdFALSE )
    {
        vTaskInternalSetTimeOutState( &xTimeOut );  // 记录当前时间为超时起始点
        xEntryTimeSet = pdTRUE;  // 标记“超时起始时间已设置”
    }else{}
}

// 退出临界区：此时其他任务/中断可修改信号量状态（如释放信号量）
taskEXIT_CRITICAL();
```

**第二步**：信号量不可用，阻塞等待处理。

用*xTaskCheckForTimeOut()* 检查是否超时，如果未超时且互斥量仍不可用，先优先级继承（就是修改任务优先级，不过任务还有一个基础优先级标记，后续恢复优先级用）

*xInheritanceOccurred =xTaskPriorityInherit( pxQueue->u.xSemaphore.xMutexHolder );*

然后将当前任务加入“等待接收列表”，阻塞等待；

*vTaskPlaceOnEventList(&( pxQueue->xTasksWaitingToReceive ), xTicksToWait );*

如果未超时且互斥量可用，就重新进入循环，获取互斥量

```c
        /* 检查超时状态：判断当前等待时间是否已过期 */
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            /* 子分支1：未超时，且信号量仍不可用（队列空=计数0），将任务加入等待列表 */
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
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
                    }else{}
                }
                #endif /* if ( configUSE_MUTEXES == 1 ) */

                // 将当前任务加入“等待接收链表”（等待信号量可用），并设置阻塞时间
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // 解锁队列：允许其他任务操作队列
                prvUnlockQueue( pxQueue );

                /* 恢复调度器：
                 * 若恢复后需要切换任务（如唤醒了更高优先级任务），则触发任务切换 */
                if( xTaskResumeAll() == pdFALSE )
                {
                    taskYIELD_WITHIN_API();  // 在API内部触发任务切换
                }else{}
            }
            /* 子分支2：未超时，但信号量已可用（其他任务/中断释放了信号量），重新进入循环尝试获取 */
            else
            {
                prvUnlockQueue( pxQueue );  // 解锁队列
                ( void ) xTaskResumeAll();  // 恢复调度器，无任务切换
            }
        }
```

如果超时，处理超时逻辑。如果超时后互斥量可用，尝试重新获取；如果超时后互斥量不可用，恢复原始优先级，返回获取失败。

```c
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

                return errQUEUE_EMPTY;  // 超时且信号量仍不可用，返回失败
            }else{}
        }
    }
```

### （源码）释放互斥锁

```c
#define xSemaphoreGive( xSemaphore )    xQueueGenericSend( \
    ( QueueHandle_t ) ( xSemaphore ),               \ // 互斥量句柄强制转为队列句柄（信号量本质是队列） 
    NULL,                                           \ // 数据缓冲区=NULL（互斥量无需存储数据）
    semGIVE_BLOCK_TIME,                             \  // 阻塞时间=0（释放互斥量永不阻塞）
    queueSEND_TO_BACK                               \  // 发送位置=队列尾部（无实际意义，因无数据）
)
```

释放互斥锁就是像这个长度为1 的队列发送一次数据，调用了 *xQueueGenericSend* 函数（之前[FreeRTOS之队列结构体用作队列（API+源码）](./FreeRTOS之队列结构体用作队列（API+源码）.md) 说过，这里不多说）。