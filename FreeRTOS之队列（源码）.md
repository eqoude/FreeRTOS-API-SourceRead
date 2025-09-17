# FreeRTOS之队列（源码）

之前有提到过FreeRTOS里的链表 [FreeRTOS之链表（源码） ](./FreeRTOS之链表（源码）.md) ，其实还有一种数据结构-队列。

FreeRTOS与队列有关吗？

《Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0》中这样说到

> 『Queues』 provide a task-to-task, task-to-interrupt, and interrupt-to-task communication mechanism. “队列”提供了一种任务到任务、任务到中断以及中断到任务的通信机制。

![img](https://picx.zhimg.com/80/v2-2d27dff56a5bb1a187ccf043809b4272_1440w.png?source=ccfced1a)





创建一个队列，以便任务A和任务B进行通信。该队列最多可容纳5个整数。创建队列时，它不包含任何值，因此为空。

![img](https://picx.zhimg.com/80/v2-d0c769d60327a1c05a581c5d9d97557d_1440w.png?source=ccfced1a)





任务A将一个局部变量的值写入（发送）到队列末尾。由于队列之前为空，写入的值现在是队列中的唯一一项，因此它既是队列末尾的值，也是队列开头的值。

![img](https://picx.zhimg.com/80/v2-8a2b00d5381addde8f838441a5c8d45e_1440w.png?source=ccfced1a)





任务A在将局部变量的值再次写入队列之前更改了该值。现在队列中包含写入队列的两个值的副本。最先写入的值仍在队列的前端，新值被插入到队列的末尾。队列还剩下三个空位。

![img](https://picx.zhimg.com/80/v2-6a69119b41c6292355de24d710895f8d_1440w.png?source=ccfced1a)





任务B从队列中读取（接收）数据到一个不同的变量中。任务B接收到的值是队列头部的值，也就是任务A写入队列的第一个值。

![img](https://pic1.zhimg.com/80/v2-23c4ec96a1d723b876f990a60ef07c2c_1440w.png?source=ccfced1a)





任务B移除了一个项目，队列中仅剩下任务A写入的第二个值。如果任务B再次从队列中读取，这将是它接下来会收到的值。现在队列还剩下四个空位。

上图均来源于《Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0》。

此外，<queue.c>里有这么一组宏定义：

```c
/* 仅用于内部使用。这些定义*必须*与 queue.c 中的定义保持一致。 */
#define queueQUEUE_TYPE_BASE                  ( ( uint8_t ) 0U )  // 基础队列类型（普通数据队列）
#define queueQUEUE_TYPE_SET                   ( ( uint8_t ) 0U )  // 队列集类型（与基础队列共享类型值，通过其他成员区分）
#define queueQUEUE_TYPE_MUTEX                 ( ( uint8_t ) 1U )  // 互斥锁类型
#define queueQUEUE_TYPE_COUNTING_SEMAPHORE    ( ( uint8_t ) 2U )  // 计数信号量类型
#define queueQUEUE_TYPE_BINARY_SEMAPHORE      ( ( uint8_t ) 3U )  // 二进制信号量类型
#define queueQUEUE_TYPE_RECURSIVE_MUTEX       ( ( uint8_t ) 4U )  // 递归互斥锁类型
```

互斥锁、计数信号量、二进制信号量、递归互斥锁这些都与队列有关，实际上互斥锁、信号量都是基于队列这个数据结构的。

接下来就从源码来看看FreeRTOS里的队列是什么样子

### FreeRTOS里的队列

先给出源码，慢慢解释：

```c
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
    List_t  xTasksWaitingToReceive;          /**< 因阻塞等待从此队列接收数据而被阻塞的任务列表。按优先级顺序存储。 */

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

typedef xQUEUE Queue_t;
```

（这里不看队列集和调试相关功能）

![img](https://picx.zhimg.com/80/v2-e0bfeb40fcaa0ddcc1824723b710fe8f_1440w.png?source=ccfced1a)

我们从下往上看

*ucStaticallyAllocated ：* 静态或动态分配 

> 为什么这里开启这个值的判断条件是这样的？ #if(( configSUPPORT_STATIC_ALLOCATION ==1)&&( configSUPPORT_DYNAMIC_ALLOCATION ==1)) 因为如果其中只有一个为 1 ，那就不用判断队列是动态还是静态分配了，哪个是1，那个就是队列的分配方式

*cRxLock ：* 存储队列锁定期间从队列接收（移除）的项目数量 *cTxLock ：*  存储队列锁定期间发送到队列（添加）的项目数量

> 这是什么意思呢？ 1. 在当前队列在任务A在发送数据时，A要想这个队列进行读写操作（A占有了这个队列（资源）），如果此时有另一个任务B也开始使用队列，那B可能会破坏A想要读的数据，此时通过 *cTxLock* 锁定队列，让B等队列不被其他任务使用时才使用队列。 2.当A使用队列时，B、C也想用，B就让这个值 +1 ，C也 +1，这样等A不用这个队列（解锁）的时候，A就知道有两个任务等着用呢，处理这两个任务的请求。

*uxItemSize ：* 队列将容纳的每个项目的大小

> 队列中可以容纳的数据有很多类型，这里可以调整队列中数据大小，容纳不同数据。

*uxLength ：* 队列的长度

> 这个长度可不是 *Queue_t* 这个结构体的长度，它是存放数据的队列的长度

*uxMessagesWaiting ：* 当前在队列中的项目数量

*xTasksWaitingToSend ：* 等待发送链表

*xTasksWaitingToReceive ：* 等待接收链表

> 之前我们说，当A使用队列时，B、C也想用，等A不用这个队列（解锁）的时候，要处理这两个任务的请求。 怎么知道等待的任务是B、C呢？ 把他们任务放到这两个链表里就好了，检查这两个链表里有没有任务在等待。

QueuePointers_t xQueue;                      /*< 当此结构用作队列时专用的数据。 */          
SemaphoreData_t xSemaphore;            /*< 当此结构用作信号量时专用的数据。 */

> 这两个结构体定义如下

```c
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
    UBaseType_t uxRecursiveCallCount; /**< 当结构体用作递归互斥锁时，记录递归「获取」互斥锁的次数。 */
} SemaphoreData_t;
```

这里就不多说了，具体不同在使用时再说明

pcHead ：  指向队列存储区域的起始位置 pcWriteTo ：  指向存储区域中下一个可用的空闲位置

> 说了这么多都没说到队列在哪里，数据放在哪里，这两个指针指向了存放数据的空间，此空间在创建队列时申请。

因为这里队列在不同用法时内容是不一样的，就先简单介绍这些，在介绍信号量、互斥量的时候再进一步说明。

![img](https://pic1.zhimg.com/80/v2-3083c2e1bba2182b879d1ff88a719146_1440w.png?source=ccfced1a)





