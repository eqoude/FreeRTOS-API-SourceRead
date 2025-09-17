# FreeRTOS之队列结构体用作队列（API+源码）

为什么这个标题叫队列结构体用做队列？

在上一篇 [FreeRTOS之队列（源码）](./FreeRTOS之队列（源码）.md) 里说到队列结构体长这样

![img](https://picx.zhimg.com/80/v2-e0bfeb40fcaa0ddcc1824723b710fe8f_1440w.png?source=ccfced1a)





添加图片注释，不超过 140 字（可选）

看其中的第三项，Queue用作队列或者信号量，在FreeRTOS里Queue_t这个结构体可以用作队列，也可以用作信号量，本文关注于Queue_t用作队列时的情况。

用作队列时第三项是这样定义的：

```c
typedef struct QueuePointers
{
    int8_t * pcTail;     /**< 指向队列存储区域末尾的字节。实际分配的内存比存储队列项所需的多一个字节，此指针用作结束标记。 */
    int8_t * pcReadFrom; /**< 当结构体用作队列时，指向最后一次读取队列项的位置。 */
} QueuePointers_t;
```

![img](https://pica.zhimg.com/80/v2-52ae5f9972ed1d56b7d36cf6d7a787f7_1440w.png?source=ccfced1a)

这有什么用？

任务间传递信息会用到，有一个定时器队列十分重要（定时器的命令，像是开启定时器、删除、重置等等这些命令都是通过发送到队列中，有一个任务叫定时器的守护任务会去处理这些命令）

文章涉及内容：创建队列的API；向队列发送、接收数据API；查看队列状态API

API：主要关注接口参数和使用

源码：给出源码->分析->总结（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）（不考虑队列集）

## *（API）*创建队列

### *xQueueCreate：*动态分配创建

```c
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    #define xQueueCreate( uxQueueLength, uxItemSize )    xQueueGenericCreate( ( uxQueueLength ), ( uxItemSize ), ( queueQUEUE_TYPE_BASE ) )
#endif
/* 使用示例：
 * struct AMessage
 * {
 *  char ucMessageID;
 *  char ucData[ 20 ];
 * };
 *
 * void vATask( void *pvParameters )
 * {
 *  QueueHandle_t xQueue1, xQueue2;
 *
 *  // 创建一个能够容纳 10 个 uint32_t 类型值的队列。
 *  xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );
 *  if( xQueue1 == 0 )
 *  {
 *      // 队列未创建，不得使用。
 *  }
 *
 *  // 创建一个能够容纳 10 个指向 AMessage 结构体的指针的队列。
 *  // 这些指针应通过指针传递，因为它们包含大量数据。
 *  xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );
 *  if( xQueue2 == 0 )
 *  {
 *      // 队列未创建，不得使用。
 *  }
 *
 *  // ... 任务的其余代码。
 * }
 */
```

这里封装了一层，我们只用 *xQueueCreate* 创建即可

> 为什么要用宏定义，不用 *xQueueGenericCreate* 呢？ 上面说过，Queue_t可以是队列，也可以是信号量，我们创建信号量的时候用的函数是 *vSemaphoreCreateBinary* ，其实也是调用了 *xQueueGenericCreate* ，这里是进行了一层封装，方便使用。仔细看 *xQueueGenericCreate* 的第三个参数，指明了是创建队列还是创建信号量。

至于两个参数，就是队列长度和队列中数据的大小。

> 队列中最大长度是多少呢？ 队列中最大长度也就是参数 *uxQueueLength* ，它的数据类型是 *UBaseType_t* ，在ARM CM3（ARM Cortex-M3）移植版本中定义如下： typedef   unsigned long   UBaseType_t; 那最大理论上就是(2^32 - 1）个，但是系统可不一定能容得下这么多数据。 第二个参数*uxItemSize* 应当传入所存数据类型所占字节数。 队列中数据 *uxItemSize* 的类型也是UBaseType_t。

*param1：uxQueueLength* 队列长度。

*param2：uxItemSize*  数据的大小。

*return：*队列句柄（队列结构体指针），用来给我们之后向队列发送数据，接收数据用。

### *xQueueCreateStatic：*静态分配创建

```c
#define xQueueCreateStatic( uxQueueLength, uxItemSize, pucQueueStorage, pxQueueBuffer )    \
    xQueueGenericCreateStatic( ( uxQueueLength ), ( uxItemSize ), ( pucQueueStorage ), ( pxQueueBuffer ), ( queueQUEUE_TYPE_BASE ) )
/* 使用示例：
 * @code{c}
 * struct AMessage
 * {
 *  char ucMessageID;  // 消息ID
 *  char ucData[ 20 ]; // 消息数据
 * };
 *
 * #define QUEUE_LENGTH 10          // 队列最大容量（10个项目）
 * #define ITEM_SIZE sizeof( uint32_t ) // 每个项目大小（uint32_t 类型，4字节）
 *
 * // xQueueBuffer：用于存储队列的数据结构（控制信息）
 * StaticQueue_t xQueueBuffer;
 *
 * // ucQueueStorage：用于存储队列的实际项目数据，大小 = 队列长度 × 项目大小
 * uint8_t ucQueueStorage[ QUEUE_LENGTH * ITEM_SIZE ];
 *
 * void vATask( void *pvParameters )
 * {
 *  QueueHandle_t xQueue1;
 *
 *  // 创建一个可容纳 10 个 uint32_t 类型数据的队列
 *  xQueue1 = xQueueCreateStatic( 
 *                          QUEUE_LENGTH,          // 队列最大容量
 *                          ITEM_SIZE,             // 每个项目大小
 *                          &( ucQueueStorage[ 0 ] ), // 项目数据存储缓冲区
 *                          &xQueueBuffer );       // 队列数据结构存储区
 *
 *  // 因未使用动态内存分配，只要 pxQueueBuffer 和 pucQueueStorage 有效，队列必创建成功
 *  // 因此 xQueue1 此时是指向有效队列的句柄
 *
 *  // ... 任务其余逻辑代码
 * }
 */
```

也是一个宏定义。

*param1：uxQueueLength* 队列长度（和动态时一样）。

*param2：uxItemSize*  数据的大小（和动态时一样）。

*param3：pucQueueStorage* 队列数据存储区，静态是需自己定义，定义方法参考示例。

> *uint8_t ucQueueStorage[ QUEUE_LENGTH \* ITEM_SIZE ];*

*param4：pxQueueBuffer*  队列结构体，本文开始提过队列结构体 *Queue_t* ，这里可以用 *StaticQueue_t*  定义一个区域供使用。

> *StaticQueue_t xQueueBuffer;*

*return：*队列句柄（队列结构体指针），用来给我们之后向队列发送数据，接收数据用。

## *（API）*将数据发送到队首

### xQueueSendToFront

```c
#define xQueueSendToFront( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_FRONT )
/* 使用示例：
 * // 定义一个消息结构体
 * struct AMessage
 * {
 *  char ucMessageID;  // 消息ID（用于标识消息类型）
 *  char ucData[ 20 ]; // 消息数据（20字节）
 * } xMessage;  // 定义一个消息实例
 *
 * uint32_t ulVar = 10U;  // 定义一个32位无符号整数（待入队的简单数据）
 *
 * void vATask( void *pvParameters )
 * {
 *  QueueHandle_t xQueue1, xQueue2;  // 定义两个队列句柄
 *  struct AMessage *pxMessage;      // 指向消息结构体的指针（用于传递复杂数据）
 *
 *  // 1. 创建队列1：可存储10个uint32_t类型数据（每个项目4字节）
 *  xQueue1 = xQueueCreate( 10, sizeof( uint32_t ) );
 *
 *  // 2. 创建队列2：可存储10个“指向AMessage结构体的指针”（每个项目4字节，32位系统）
 *  // 传递指针而非整个结构体，可避免大量数据复制，提升效率
 *  xQueue2 = xQueueCreate( 10, sizeof( struct AMessage * ) );
 *
 *  // ... 其他初始化逻辑
 *
 *  // 向队列1发送uint32_t类型数据
 *  if( xQueue1 != 0 )  // 先判断队列创建成功（句柄非空）
 *  {
 *      // 尝试发送ulVar到队列1头部，若队列满则阻塞等待10个节拍
 *      if( xQueueSendToFront( xQueue1, ( void * ) &ulVar, ( TickType_t ) 10 ) != pdPASS )
 *      {
 *          // 等待10个节拍后仍未成功入队（队列一直满），执行错误处理
 *      }
 *  }
 *
 *  // 向队列2发送“指向AMessage结构体的指针”
 *  if( xQueue2 != 0 )  // 判断队列创建成功
 *  {
 *      pxMessage = &xMessage;  // 指针指向已定义的消息实例
 *      // 尝试发送指针到队列2头部，队列满时不阻塞（等待0个节拍）
 *      xQueueSendToFront( xQueue2, ( void * ) &pxMessage, ( TickType_t ) 0 );
 *  }
 *
 *  // ... 任务其余逻辑代码
 * }
 */
```

也是封装了的，因为数据发送、发送到队尾、发送到队首，这些都是调用了 *xQueueGenericSend* 这个函数，此函数的第四个参数来控制发送到队首还是队尾。

*param1：xQueue* 要发送的队列，也就是之前创建队列时的指针。

*param2：pvItemToQueue*要发送的数据的地址，记得转换成( void * )。

*param3：xTicksToWait*  等待时间。

> 等待时间是什么意思？ 我们发送数据到队列时，这个队列可能现在正有其他任务在使用（发送、接收），此时，队列是锁定的（防止现在发送数据后，其他任务读不到正确数据），那我们就等一段时间再发送，这个参数 *xTicksToWait* 就是顶戴的时间（如果允许一直等待，此参数应设置成最大）

*return：*成功返回 pdTRUE ；失败返回 errQUEUE_FULL 

### *xQueueSendToFrontFromISR* 中断安全版

```c
#define xQueueSendToFrontFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_FRONT )
/* 缓冲IO场景下的示例用法（中断服务程序中每次调用可处理多个数据值）：
 * @code{c}  // Doxygen文档标记，指定后续为C语言示例代码
 * // 中断服务程序：处理缓冲数据的接收与发送
 * void vBufferISR( void )
 * {
 * char cIn;  // 存储从硬件缓冲区读取的单个字节数据
 * BaseType_t xHigherPriorityTaskWoken;  // 标记高优先级任务是否被唤醒
 *
 *  // 中断服务程序开始时，初始化“高优先级任务未被唤醒”
 *  xHigherPriorityTaskWoken = pdFALSE;
 *
 *  // 循环读取硬件缓冲区，直到缓冲区为空
 *  do
 *  {
 *      // 从硬件接收寄存器（地址为RX_REGISTER_ADDRESS）读取一个字节
 *      cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );
 *
 *      // 将读取到的字节发送到xRxQueue队列的头部（中断安全版发送）
 *      xQueueSendToFrontFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );
 *
 *  // 检查硬件缓冲区计数寄存器（地址为BUFFER_COUNT），若不为0则缓冲区仍有数据，继续循环
 *  } while( portINPUT_BYTE( BUFFER_COUNT ) );
 *
 *  // 缓冲区已空，若高优先级任务被唤醒，则请求上下文切换
 *  if( xHigherPriorityTaskWoken )
 *  {
 *      // 触发上下文切换（中断服务程序中需用任务切换函数确保高优先级任务及时运行）
 *      taskYIELD ();
 *  }
 * }
 */
```

*param1：xQueue* 要发送的队列，也就是之前创建队列时的指针。

*param2：pvItemToQueue*要发送的数据的地址，记得转换成( void * )。

*param3：pxHigherPriorityTaskWoken*  高优先级任务是否被唤醒”的标记。

> 为什么不是等待时间了？ 因为中断最好要快点，等待的话不安全 这个标记什么意思？ 假如在中断中发送前，有任务A,B因为等待队列数据（队列数据为空，A,B都被阻塞，放在队列的等发送列表），此时中断发送数据到队列里，队列有数据给A,B用了，那就标记一下，最后触发上下文切换，让A或B运行（看调度逻辑）。

*return：*成功返回 pdTRUE ；失败返回 errQUEUE_FULL 

## *（API）*将数据发送到队尾

### *xQueueSendToBack*

```c
#define xQueueSendToBack( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )
```

使用参考*（API）*将数据发送到队首-*xQueueSendToFront*

*param1：xQueue* 要发送的队列，也就是之前创建队列时的指针。（和发送队首时一样）

*param2：pvItemToQueue*要发送的数据的地址，记得转换成( void * )。（和发送队首时一样）

*param3：xTicksToWait*  等待时间。（和发送队首时一样）

*return：*成功返回 *pdTRUE* ；失败返回 *errQUEUE_FULL* 

### *xQueueSendToBackFromISR*  中断安全版

```c
#define xQueueSendToBackFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )
/* void vBufferISR( void )
 * {
 * char cIn;  // 存储从硬件寄存器读取的单个字节数据
 * BaseType_t xHigherPriorityTaskWoken;  // 标记“高优先级任务是否被唤醒”
 *
 *  // 中断服务程序开始时，初始化标记为“未唤醒高优先级任务”
 *  xHigherPriorityTaskWoken = pdFALSE;
 *
 *  // 循环读取硬件缓冲区，直到缓冲区为空
 *  do
 *  {
 *      // 从硬件接收寄存器（地址为RX_REGISTER_ADDRESS）读取一个字节
 *      cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );
 *
 *      // 将读取到的字节发送到xRxQueue队列的尾部（中断安全版发送）
 *      xQueueSendToBackFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );
 *
 *  // 检查硬件缓冲区计数寄存器（地址为BUFFER_COUNT）：若值非0，说明缓冲区仍有数据，继续循环
 *  } while( portINPUT_BYTE( BUFFER_COUNT ) );
 *
 *  // 缓冲区已空，若高优先级任务被唤醒，则请求上下文切换
 *  if( xHigherPriorityTaskWoken )
 *  {
 *      // 触发上下文切换（确保高优先级任务能立即执行）
 *      taskYIELD ();
 *  }
 * }
 */
```

*param1：xQueue* 要发送的队列，也就是之前创建队列时的指针。

*param2：pvItemToQueue*要发送的数据的地址，记得转换成( void * )。

*param3：pxHigherPriorityTaskWoken*  高优先级任务是否被唤醒”的标记。

*return：*成功返回 *pdTRUE* ；失败返回 *errQUEUE_FULL* 

### *xQueueSend*

```c
#define xQueueSend( xQueue, pvItemToQueue, xTicksToWait ) \
    xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )
```

*param1：xQueue* 要发送的队列，也就是之前创建队列时的指针。（和发送队首时一样）

*param2：pvItemToQueue*要发送的数据的地址，记得转换成( void * )。（和发送队首时一样）

*param3：xTicksToWait*  等待时间。（和发送队首时一样）

*return：*成功返回 *pdTRUE* ；失败返回 *errQUEUE_FULL* 

### xQueueSendFromISR  中断安全版

```c
#define xQueueSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) \
    xQueueGenericSendFromISR( ( xQueue ), ( pvItemToQueue ), ( pxHigherPriorityTaskWoken ), queueSEND_TO_BACK )
/* void vBufferISR( void )
 * {
 * char cIn;  // 存储从硬件寄存器读取的单个字节数据
 * BaseType_t xHigherPriorityTaskWoken;  // 标记“高优先级任务是否被唤醒”
 *
 *  // 中断服务程序开始时，初始化标记为“未唤醒高优先级任务”
 *  xHigherPriorityTaskWoken = pdFALSE;
 *
 *  // 循环读取硬件缓冲区，直到缓冲区为空
 *  do
 *  {
 *      // 从硬件接收寄存器（地址为RX_REGISTER_ADDRESS）读取一个字节
 *      cIn = portINPUT_BYTE( RX_REGISTER_ADDRESS );
 *
 *      // 将读取到的字节通过xQueueSendFromISR()发送到xRxQueue队列
 *      xQueueSendFromISR( xRxQueue, &cIn, &xHigherPriorityTaskWoken );
 *
 *  // 检查硬件缓冲区计数寄存器（地址为BUFFER_COUNT）：若值非0，说明缓冲区仍有数据，继续循环
 *  } while( portINPUT_BYTE( BUFFER_COUNT ) );
 *
 *  // 缓冲区已空，若高优先级任务被唤醒，则请求上下文切换
 *  if( xHigherPriorityTaskWoken )
 *  {
 *       // 由于xHigherPriorityTaskWoken已设为pdTRUE，需请求上下文切换。
 *       // 具体使用的宏因端口而异，通常是portYIELD_FROM_ISR()或portEND_SWITCHING_ISR()，
 *       // 需参考所用端口的文档说明。
 *       portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
 *  }
 * }
 */
```

*param1：xQueue* 要发送的队列，也就是之前创建队列时的指针。

*param2：pvItemToQueue*要发送的数据的地址，记得转换成( void * )。

*param3：pxHigherPriorityTaskWoken*  高优先级任务是否被唤醒”的标记。

*return：*成功返回 *pdTRUE* ；失败返回 *errQUEUE_FULL* 

## *（API）查看*队列数据，不移除

### *xQueuePeek*

```c
BaseType_t xQueuePeek( QueueHandle_t xQueue,
                       void * const pvBuffer,
                       TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
/*
 * // 示例用法说明
 * struct AMessage  // 定义一个结构体AMessage，作为队列中传输的数据类型
 * {
 *  char ucMessageID;  // 结构体成员1：消息ID（unsigned char类型）
 *  char ucData[ 20 ];  // 结构体成员2：消息数据缓冲区（20字节的unsigned char数组）
 * } xMessage;  // 定义结构体变量xMessage
 *
 * QueueHandle_t xQueue;  // 定义队列句柄变量xQueue，用于后续标识创建的队列
 *
 * // 任务1：创建队列并向队列发送数据
 * void vATask( void *pvParameters )  // 任务函数vATask（FreeRTOS任务函数固定格式：返回值void，参数为void*）
 * {
 * struct AMessage *pxMessage;  // 定义指向AMessage结构体的指针pxMessage
 *
 *  // （创建一个队列：队列可存储10个“指向AMessage结构体的指针”；
 *  // 由于结构体数据量较大，此处通过“指针传递”方式传输数据，而非直接传结构体本身）
 *  xQueue = xQueueCreate( 10, sizeof( struct AMessage * ) );  // 调用xQueueCreate创建队列，参数1：队列长度（10个元素），参数2：每个元素的大小（AMessage指针的字节数）
 *  if( xQueue == 0 )  // 判断队列是否创建成功（xQueueCreate返回0表示创建失败）
 *  {
 *      // Failed to create the queue.  // 队列创建失败的处理逻辑（此处仅为注释，实际需根据需求补充代码）
 *  }
 *
 *  // ...  // 省略其他业务逻辑代码
 *
 *  // （向队列发送一个“指向AMessage结构体的指针”；若队列已满，不阻塞等待）
 *  pxMessage = &xMessage;  // 将结构体变量xMessage的地址赋值给指针pxMessage
 *  xQueueSend( xQueue, ( void * ) &pxMessage, ( TickType_t ) 0 );  // 调用xQueueSend发送数据：参数1（队列句柄）、参数2（要发送的数据地址，强制转为void*）、参数3（阻塞时间0）
 *  // 省略任务剩余业务逻辑代码
 * }
 *
 *  // 任务2：从队列中“查看”数据（不移除数据）
 * void vADifferentTask( void *pvParameters )  // 任务函数vADifferentTask（FreeRTOS任务函数格式）
 * {
 * struct AMessage *pxRxedMessage;  // 定义指向AMessage结构体的指针pxRxedMessage，用于接收从队列查看的数据
 *
 *  if( xQueue != 0 )  // 判断队列句柄是否有效（即队列是否已成功创建）
 *  {
 *      // （在已创建的队列中查看一条消息；若消息无法立即获取，阻塞等待10个时钟周期）
 *      if( xQueuePeek( xQueue, &( pxRxedMessage ), ( TickType_t ) 10 ) )  // 调用xQueuePeek查看数据：参数1（队列句柄）、参数2（接收缓冲区地址，即指针pxRxedMessage的地址）、参数3（阻塞时间10）
 *      {
 *          // （此时pxRxedMessage指向vATask任务发送的AMessage结构体变量，但该数据项仍保留在队列中，未被移除）
 *      }
 *  }
 * }
```

队列是先进先出的，读数据就是读第一个送到队列里的数据。

*param1：*队列句柄，也就是之前创建队列时的指针。

*param2：pvBuffer* 接收的数据存放区的地址。

*param3：xTicksToWait*  等待时间。

*return：*成功返回 *pdTRUE* ；失败返回 *pdFALSE*

### *xQueuePeekFromISR* 中断安全版本

```c
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,
                              void * const pvBuffer ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*param2：pvBuffer* 接收的数据存放区的地址。

*return：*成功返回 *pdTRUE* ；失败返回 *pdFALSE*

## *（API）*读取队列数据，同时移除数据

### *xQueueReceive*

```c
BaseType_t xQueueReceive( QueueHandle_t xQueue,
                          void * const pvBuffer,
                          TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*param2：pvBuffer* 接收的数据存放区的地址。

*param3：xTicksToWait*  等待时间。

*return：*成功返回 *pdTRUE* ；失败返回 *pdFALSE*

### *xQueueReceiveFromISR*  中断安全版本

```c
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue,
                                 void * const pvBuffer,
                                 BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*param2：pvBuffer* 接收的数据存放区的地址。

*param3：pxHigherPriorityTaskWoken*  高优先级任务是否被唤醒”的标记。

*return：*成功返回 *pdTRUE* ；失败返回 *pdFALSE*

## *（API）*查看队列中消息数量

### *uxQueueMessagesWaiting*

```c
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*return：*队列中可用的消息数量，类型为 *UBaseType_t* 

### *uxQueueMessagesWaitingFromISR*

```c
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*return：*队列中可用的消息数量，类型为 *UBaseType_t* 

## *（API）*查看队列中可用的空闲空间数量

```c
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*return：*队列中可用的空闲空间数量，类型为 *UBaseType_t* 

## *（API）*查看队列中可用的空闲空间数量

```c
void vQueueDelete( QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

## *（API）*判断队列是否为满（中断安全版）

```c
BaseType_t xQueueIsQueueFullFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*return：pdTRUE* 表示队列已满（无空闲空间接收新数据）；*pdFALSE* 表示队列未满（可发送新数据）

## *（API）*判断队列是否为空（中断安全版）

```c
BaseType_t xQueueIsQueueEmptyFromISR( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*return：pdTRUE* 表示队列为空（无待读取数据）；*pdFALSE* 表示队列非空（有数据可读取）

## *（API）*重置队列为初始的空状态

```c
#define xQueueReset( xQueue )    xQueueGenericReset( ( xQueue ), pdFALSE )
```

*param1：*队列句柄，也就是之前创建队列时的指针。

*return：*始终返回 *pdPASS*

## *（源码）*创建队列

```c
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )  // 仅当启用动态内存分配时，编译此函数

    // 通用动态队列创建函数：支持创建不同类型的队列（普通队列、信号量、互斥锁等）
    QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength,    // 队列最大容量（项目数）
                                       const UBaseType_t uxItemSize,      // 每个项目的字节数
                                       const uint8_t ucQueueType )        // 队列类型（如普通队列、互斥锁等）
    {
        Queue_t * pxNewQueue = NULL;       // 指向动态分配的队列控制结构，最终作为句柄返回
        size_t xQueueSizeInBytes;          // 队列项目存储区的总字节数
        uint8_t * pucQueueStorage;         // 指向项目存储区的起始地址（从动态分配的内存块中拆分）

        // 合法性校验：满足以下所有条件才进行动态内存分配
        if( ( uxQueueLength > ( UBaseType_t ) 0 ) &&  // 1. 队列容量>0（有效队列）
            /* 2. 检查“容量×项目大小”是否溢出：避免计算存储区大小时整数溢出 */
            ( ( SIZE_MAX / uxQueueLength ) >= uxItemSize ) &&
            /* 3. 检查“控制结构大小+存储区大小”是否溢出：确保总内存不超过系统最大可分配字节数 */
            ( ( UBaseType_t ) ( SIZE_MAX - sizeof( Queue_t ) ) >= ( uxQueueLength * uxItemSize ) ) )
        {
            // 计算项目存储区的总字节数：容量 × 每个项目的字节数（若uxItemSize=0，存储区大小为0）
            xQueueSizeInBytes = ( size_t ) ( ( size_t ) uxQueueLength * ( size_t ) uxItemSize );

            /* 动态分配一块连续内存：大小 = 队列控制结构大小（Queue_t） + 项目存储区大小
               - 控制结构用于存储队列元数据（指针、计数、列表等）
               - 存储区用于存储实际入队的项目数据 */
            pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );

            // 若内存分配成功（pxNewQueue非空），继续初始化
            if( pxNewQueue != NULL )
            {
                /* 拆分动态内存块：项目存储区起始地址 = 控制结构地址 + 控制结构大小
                   - 动态内存块布局：[ Queue_t 控制结构 ][ 项目存储区 ]
                   - 这样设计可确保控制结构与存储区连续，减少内存碎片，且方便管理（释放时只需释放一块内存） */
                pucQueueStorage = ( uint8_t * ) pxNewQueue;
                pucQueueStorage += sizeof( Queue_t );

                #if ( configSUPPORT_STATIC_ALLOCATION == 1 )  // 若同时启用静态分配，标记队列的分配方式
                {
                    /* 队列可静态或动态分配，此处标记为动态分配（pdFALSE），
                       后续调用vQueueDelete时，会根据此标记释放动态内存 */
                    pxNewQueue->ucStaticallyAllocated = pdFALSE;
                }
                #endif /* configSUPPORT_STATIC_ALLOCATION */

                // 调用队列初始化函数：填充控制结构成员，完成队列功能初始化（与静态创建共用同一初始化逻辑）
                prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
            }
            // 内存分配失败（如堆内存不足）
            else
            {}
        }
        // 合法性校验失败（如容量为0、内存溢出风险）
        else
        {}

        // 返回队列句柄：成功则为非空指针，失败则为NULL
        return pxNewQueue;
    }

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
```

两件事：1.申请空间；2.初始化队列

**1.申请空间**

```c
//计算项目存储区的总字节数
xQueueSizeInBytes =(size_t)((size_t) uxQueueLength *(size_t) uxItemSize );
//调用pvPortMalloc分配空间
pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes )
```

*pvPortMalloc* 与堆分配策略有关详见 [FreeRTOS内存管理01（源码）](./FreeRTOS内存管理01（源码）.md) [FreeRTOS内存管理02（源码）](./FreeRTOS内存管理02（源码）.md)

这里分配的空间是 *sizeof( Queue_t )+ xQueueSizeInBytes* ，是一整块空间。

![img](https://picx.zhimg.com/80/v2-a8a83ffa1d2c502a0e9d95e2ee086f2d_1440w.png?source=ccfced1a)

**2.初始化队列**

```c
//计算储存区地址
pucQueueStorage = ( uint8_t * ) pxNewQueue;
pucQueueStorage += sizeof( Queue_t );
//设置动态分配标记
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )  // 若同时启用静态分配，标记队列的分配方式
{
    pxNewQueue->ucStaticallyAllocated = pdFALSE;
}
#endif 
//调用队列初始化函数
prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
```

接下来看看队列初始化函数 *prvInitialiseNewQueue* ：

```c
// 静态内部函数：初始化新创建的队列（无论是动态还是静态分配），填充队列控制结构的核心成员
static void prvInitialiseNewQueue( const UBaseType_t uxQueueLength,    // 队列最大容量（可存储的项目数）
                                   const UBaseType_t uxItemSize,      // 每个队列项目的字节数
                                   uint8_t * pucQueueStorage,         // 队列项目的存储缓冲区（静态分配时为用户提供，动态分配时为堆内存）
                                   const uint8_t ucQueueType,         // 队列类型（如普通队列、互斥锁、信号量等）
                                   Queue_t * pxNewQueue )             // 指向待初始化的队列控制结构（Queue_t）
{
    // 处理“无需存储项目数据”的场景（uxItemSize == 0，如信号量、互斥锁）
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        /* 用作队列时一般不考虑 uxItemSize == 0 的情况 */
    }
    else
    {
        /* 处理“需要存储项目数据”的场景（如普通队列），将pcHead指向项目存储缓冲区的起始地址，
         * 后续通过pcHead定位队列存储区的基址，实现项目的读写。 */
        pxNewQueue->pcHead = ( int8_t * ) pucQueueStorage;
    }

    /* 按队列类型定义的规则，初始化队列核心成员 */
    pxNewQueue->uxLength = uxQueueLength;          // 记录队列最大容量
    pxNewQueue->uxItemSize = uxItemSize;          // 记录每个项目的字节数
    // 调用通用重置函数，初始化队列的读写指针、项目计数、阻塞列表等（pdTRUE表示“强制重置”，忽略当前状态）
    ( void ) xQueueGenericReset( pxNewQueue, pdTRUE );
}
```

就是根据计算好的地址和用户给的值赋值就好，正好这里顺便看一下重置函数 *xQueueGenericReset*

```c
// 通用队列重置函数：将队列恢复到初始状态（清空数据、重置指针、处理阻塞任务）
// 支持“新队列初始化”和“已有队列重置”两种场景
BaseType_t xQueueGenericReset( QueueHandle_t xQueue,  // 待重置的队列句柄
                               BaseType_t xNewQueue )  // 重置模式：pdTRUE=新队列初始化，pdFALSE=已有队列重置
{
    BaseType_t xReturn = pdPASS;  // 函数返回值，默认成功（pdPASS）
    Queue_t * const pxQueue = xQueue;  // 将队列句柄转换为内核内部控制结构指针（Queue_t*）

    // 合法性校验：满足以下条件才执行重置操作
    if( ( pxQueue != NULL ) &&  // 1. 队列句柄有效
        ( pxQueue->uxLength >= 1U ) &&  // 2. 队列容量≥1（有效队列）
        /* 3. 检查“队列容量 × 项目大小”是否溢出：
           SIZE_MAX是系统最大可表示的字节数，若 SIZE_MAX / 容量 ≥ 项目大小，
           则容量×项目大小 ≤ SIZE_MAX，无溢出风险 */
        ( ( SIZE_MAX / pxQueue->uxLength ) >= pxQueue->uxItemSize ) )
    {
        // 进入临界区：禁止中断和任务切换，确保重置操作的原子性（避免多任务/中断干扰）
        taskENTER_CRITICAL();
        {
            // 1. 初始化队列存储区末尾指针（pcTail）：指向存储区最后一个字节的下一位（环形缓冲区结束标记）
            pxQueue->u.xQueue.pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize );
            
            // 2. 重置队列项目计数：当前队列空，无待处理项目
            pxQueue->uxMessagesWaiting = ( UBaseType_t ) 0U;
            
            // 3. 重置写入指针：初始写入位置为存储区起始地址（pcHead）
            pxQueue->pcWriteTo = pxQueue->pcHead;
            
            // 4. 重置读取指针：初始读取位置为“存储区末尾前一个项目”（环形缓冲区特性，确保首次读取从pcHead开始）
            pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - 1U ) * pxQueue->uxItemSize );
            
            // 5. 重置队列锁定状态：接收和发送方向均设为“未锁定”（queueUNLOCKED = -1）
            pxQueue->cRxLock = queueUNLOCKED;
            pxQueue->cTxLock = queueUNLOCKED;

            // 分支1：已有队列重置（xNewQueue = pdFALSE）——用于清空已有队列，保留阻塞任务列表逻辑
            if( xNewQueue == pdFALSE )
            {
                 /* 新队列创建不看这个  */
            }
            // 分支2：新队列初始化（xNewQueue = pdTRUE）——用于新创建的队列，初始化阻塞任务列表
            else
            {
                // 初始化“等待发送”和“等待接收”列表为空（新队列无阻塞任务）
                vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
                vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
            }
        }
        // 退出临界区：恢复中断和任务切换
        taskEXIT_CRITICAL();
    }
    // 合法性校验失败（如队列无效、容量为0、内存溢出风险），返回失败
    else
    {
        xReturn = pdFAIL;
    }
    // 返回重置结果：pdPASS=成功，pdFAIL=失败
    return xReturn;
}
```

也是赋值，然后初始化队列相关链表。

![img](https://picx.zhimg.com/80/v2-5801204ada2951ef3f950e47301f64d3_1440w.png?source=ccfced1a)

至于静态分配，和动态一样，只不过队列的空间需要用户给出。

## *（源码）*数据发送

把数据发送到队首、队尾都用的是一个函数 *xQueueGenericSend* ，中断版本是 *xQueueGenericSendFromISR*

先看 *xQueueGenericSend* 

```c
BaseType_t xQueueGenericSend( QueueHandle_t xQueue,          // 目标队列句柄
                              const void * const pvItemToQueue,  // 待入队项目的指针（数据来源）
                              TickType_t xTicksToWait,           // 队列满时的阻塞等待节拍数
                              const BaseType_t xCopyPosition )   // 入队位置：queueSEND_TO_FRONT（队首）、queueSEND_TO_BACK（队尾）、queueOVERWRITE（覆盖）
{
    BaseType_t xEntryTimeSet = pdFALSE;  // 标记是否已初始化超时时间（避免重复设置）
    BaseType_t xYieldRequired = pdFALSE; // 标记是否需要触发任务切换（如唤醒高优先级任务）
    TimeOut_t xTimeOut;                  // 超时管理结构体（记录阻塞开始时间，用于判断超时）
    Queue_t * const pxQueue = xQueue;    // 将队列句柄转换为内核内部控制结构指针（Queue_t*）

    // 无限循环：直到项目成功入队或等待超时（循环内处理“检查-阻塞-重试”逻辑）
    for( ; ; )
    {
        // 进入临界区：禁止中断和任务切换，确保队列状态检查与修改的原子性
        taskENTER_CRITICAL();
        {
            /* 检查队列是否有空间（或是否允许覆盖）：
               - 普通模式：队列当前项目数 < 队列容量（uxMessagesWaiting < uxLength）；
               - 覆盖模式：无论队列是否满，都允许覆盖头部项目（仅单项目队列可用）。 */
            if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
            {
                #if ( configUSE_QUEUE_SETS == 1 )  // 若启用队列集功能
                #else /* configUSE_QUEUE_SETS == 0 */  // 未启用队列集
                {
                    // 复制数据到队列存储区，获取是否需要任务切换的标记
                    xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

                    // 若有任务阻塞等待接收数据，唤醒一个任务
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            // 唤醒高优先级任务，触发切换
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else{}
                    }
                    // 若需要任务切换，触发切换
                    else if( xYieldRequired != pdFALSE )
                    {
                        queueYIELD_IF_USING_PREEMPTION();
                    }else{}
                }
                #endif /* configUSE_QUEUE_SETS */

                // 退出临界区
                taskEXIT_CRITICAL();
                return pdPASS;
            }
            // 队列已满且不允许覆盖：处理阻塞等待或超时
            else
            {
                // 情况1：不阻塞等待（xTicksToWait=0），直接返回失败
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL();  // 退出临界区
                    return errQUEUE_FULL;
                }
                // 情况2：未初始化超时时间，初始化超时结构体（记录当前时间）
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;  // 标记已初始化
                }else{}
            }
        }
        // 退出临界区：允许中断和任务切换（后续处理阻塞逻辑）
        taskEXIT_CRITICAL();

        /* 临界区已退出，其他任务或中断可操作队列 */

        // 挂起调度器：确保阻塞任务操作的原子性（避免任务在阻塞过程中被其他任务干扰）
        vTaskSuspendAll();
        // 锁定队列：标记队列正在被操作，防止中断或其他任务修改队列状态
        prvLockQueue( pxQueue );

        // 检查超时：判断阻塞等待是否已超时（xTicksToWait更新为剩余等待时间）
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 超时未发生：检查队列是否仍满（可能在临界区外被其他任务取走数据）
            if( prvIsQueueFull( pxQueue ) != pdFALSE )
            {
                // 将当前任务加入“等待发送”列表，进入阻塞状态（等待xTicksToWait个节拍）
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                // 解锁队列：允许队列事件（如其他任务取数据）修改等待列表
                prvUnlockQueue( pxQueue );

                // 恢复调度器：若当前任务已被唤醒（如其他任务取数据），会进入就绪列表
                if( xTaskResumeAll() == pdFALSE )
                {
                    // 若有高优先级任务进入就绪列表，触发任务切换
                    taskYIELD_WITHIN_API();
                }
            }
            // 队列已非满：无需阻塞，解锁队列并恢复调度器，回到循环重试入队
            else
            {
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        // 超时已发生：解锁队列、恢复调度器，返回失败
        else
        {
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();
            return errQUEUE_FULL;
        }
    }
}
```

我们分三种情况：1.队列有空间；2.队列无空间，不等待；3.队列无空间，等待；

**1.队列有空间**

首先，复制数据到队列存储区，获取是否需要任务切换的标记（此处的标记在互斥量下有用，这里标记始终为*pdFALSE*）

```c
xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );
```

*prvCopyDataToQueue* 实现如下

```c
// 静态函数：将数据复制到队列存储区的核心实现，根据入队位置（队首/队尾/覆盖）调整队列指针
// 仅在临界区内被xQueueGenericSend调用，确保队列状态操作的原子性
static BaseType_t prvCopyDataToQueue( Queue_t * const pxQueue,    // 目标队列的内部控制结构指针
                                      const void * pvItemToQueue,  // 待复制的数据源指针
                                      const BaseType_t xPosition ) // 入队位置：queueSEND_TO_FRONT/queueSEND_TO_BACK/queueOVERWRITE
{
    BaseType_t xReturn = pdFALSE;  // 返回值：仅用于互斥锁场景（优先级继承相关）
    UBaseType_t uxMessagesWaiting;  // 记录当前队列中的项目数（用于覆盖模式下的计数修正）

    // 保存当前队列的项目数（后续覆盖模式需修正该值）
    uxMessagesWaiting = pxQueue->uxMessagesWaiting;

    // 分支1：项目大小为0（uxItemSize=0）——通常用于同步队列/互斥锁（无实际数据存储） 我们这里不看这个
    if( pxQueue->uxItemSize == ( UBaseType_t ) 0 ){}
    // 分支2：入队位置为队尾（queueSEND_TO_BACK）——FIFO模式
    else if( xPosition == queueSEND_TO_BACK )
    {
        // 核心操作：将数据源数据复制到队列的“写入指针（pcWriteTo）”位置
        ( void ) memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, ( size_t ) pxQueue->uxItemSize );
        
        // 更新写入指针：向后偏移一个项目大小（指向下次写入的位置）
        pxQueue->pcWriteTo += pxQueue->uxItemSize;

        // 边界处理：若写入指针超过队列尾部（pcTail），则回卷到队列头部（pcHead）——循环队列特性
        if( pxQueue->pcWriteTo >= pxQueue->u.xQueue.pcTail )
        {
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }else{}
    }
    // 分支3：入队位置为队首（queueSEND_TO_FRONT）或覆盖（queueOVERWRITE）
    else
    {
        // 核心操作：将数据源数据复制到队列的“读取指针（pcReadFrom）”位置
        // （队首入队/覆盖模式均从读取指针位置写入，因读取指针始终指向队首项目）
        ( void ) memcpy( ( void * ) pxQueue->u.xQueue.pcReadFrom, pvItemToQueue, ( size_t ) pxQueue->uxItemSize );
        
        // 更新读取指针：向前偏移一个项目大小（指向新的队首位置）
        pxQueue->u.xQueue.pcReadFrom -= pxQueue->uxItemSize; 

        // 边界处理：若读取指针低于队列头部（pcHead），则回卷到队列尾部前一个项目位置——循环队列特性
        if( pxQueue->u.xQueue.pcReadFrom < pxQueue->pcHead )
        {
            pxQueue->u.xQueue.pcReadFrom = ( pxQueue->u.xQueue.pcTail - pxQueue->uxItemSize );
        }else{}
        // 子分支：若为覆盖模式（queueOVERWRITE）——仅单项目队列可用，我们这里不看这个
        if( xPosition == queueOVERWRITE ){}
    }

    // 统一更新队列项目数：无论哪种模式，最终计数=修正后的当前数+1（覆盖模式因提前减1，实际计数不变）
    pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxMessagesWaiting + ( UBaseType_t ) 1 );

    return xReturn;  // 仅互斥锁场景有效，其他场景返回pdFALSE（无意义）
}
```

> 从*prvCopyDataToQueue* 可以看出，队列里有两个指针 *pxQueue->pcWriteTo* 指向队列尾（不是实际空间的尾部，是逻辑上的），*pxQueue->u.xQueue.pcReadFrom* 指向队列头部（不是实际空间的尾部，是逻辑上的），写入头部就是向*pcReadFrom* 指向地方写，写入头部尾部就是向*pcWriteTo* 指向地方写。 最后更新一下指针指向和队列状态（数据数目）。

然后，若有任务阻塞等待接收数据，唤醒一个任务

> 怎么查看有任务阻塞等待接收数据？ 一个队列有两个链表，分别是等待接收链表和等待发送链表，当队列为空且有任务要读数据时，发现没有数据可读，那他就被放到等待接收链表，等有数据被发送到队列时（比如现在），就去看看等待接收链表有没有任务在等待，有的话就把它从等待接收链表移出来，放到就绪链表里，让它可以被执行。

```c
if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
{
    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
    {
     // 唤醒高优先级任务，触发切换
     queueYIELD_IF_USING_PREEMPTION();
    }
    else{}
}
```

若没有任务阻塞等待接收数据，检查是否要任务切换（这里始终为*pdFALSE* 互斥量下才有意义）

```c
else if( xYieldRequired != pdFALSE )
{
    queueYIELD_IF_USING_PREEMPTION();
}
else{}
```

**2.队列无空间，不等待**

```c
// 不阻塞等待（xTicksToWait=0），直接返回失败
if( xTicksToWait == ( TickType_t ) 0 )
{
    taskEXIT_CRITICAL();  // 退出临界区
    return errQUEUE_FULL;
}
```

*xTicksToWait ==( TickType_t )0*  表示不等待，此时没有空间直接发送失败。

**3.队列无空间，等待**

初始化超时结构体（我们计算任务等到多久要用）。

```c
vTaskInternalSetTimeOutState( &xTimeOut );
```

*vTaskInternalSetTimeOutState*实现如下

```c
// 内核内部专用的超时状态初始化函数（无临界区保护，仅用于内核代码）
void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut )
{
    /* 仅内核内部使用，因为此函数不包含临界区保护 */
    // 记录当前系统的溢出计数器值
    // xNumOfOverflows：系统节拍计数器(xTickCount)溢出的次数
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    
    // 记录进入超时状态时的系统节拍值
    // xTickCount：当前系统节拍计数器的值（每过一个时钟节拍加1）
    pxTimeOut->xTimeOnEntering = xTickCount;
}
```

> 通过检查系统节拍计数器溢出的次数和计数器的值，记录现在系统计数器状态。

然后，锁定队列，标记队列正在被操作，防止中断或其他任务修改队列状态

```c
prvLockQueue( pxQueue );
```

*prvLockQueue*实现如下

```c
/*
 * 用于将队列标记为锁定状态的宏。锁定队列可防止中断服务程序（ISR）
 * 访问队列的事件列表（即阻塞任务列表）。
 */
#define prvLockQueue( pxQueue )                            \
    taskENTER_CRITICAL();                                  \
    {                                                      \
        if( ( pxQueue )->cRxLock == queueUNLOCKED )        \
        {                                                  \
            ( pxQueue )->cRxLock = queueLOCKED_UNMODIFIED; \
        }                                                  \
        if( ( pxQueue )->cTxLock == queueUNLOCKED )        \
        {                                                  \
            ( pxQueue )->cTxLock = queueLOCKED_UNMODIFIED; \
        }                                                  \
    }                                                      \
    taskEXIT_CRITICAL()
```

> 队列里有两个标记*cRxLock* 和*cTxLock* ，一个是读数据的锁一个是发送数据的锁（这里看不出区别，同时加锁同时解锁），把这个状态位修改成锁定状态就好了。

然后，检查超时，判断阻塞等待是否已超时

```c
xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait )
```

*xTaskCheckForTimeOut*实现如下

```c
// 检查任务等待是否超时，更新剩余等待时间，返回超时状态
// 参数1：pxTimeOut - 超时状态结构体（需先通过vTaskSetTimeOutState()初始化）
// 参数2：pxTicksToWait - 指向剩余等待时间的指针（函数会更新该值）
// 返回值：pdTRUE - 已超时；pdFALSE - 未超时
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut,
                                 TickType_t * const pxTicksToWait )
{
    BaseType_t xReturn;

    taskENTER_CRITICAL();
    {
        /* 小优化：在临界区内，滴答计数（xTickCount）不会被中断修改，可缓存为常量 */
        const TickType_t xConstTickCount = xTickCount;
        // 计算从超时初始化到当前的已流逝时间（无符号减法，自动处理溢出）
        const TickType_t xElapsedTime = xConstTickCount - pxTimeOut->xTimeOnEntering;

        #if ( INCLUDE_xTaskAbortDelay == 1 ) // 若启用"延迟中止"功能 ，这里不考虑延迟终止
        #endif

        #if ( INCLUDE_vTaskSuspend == 1 )  // 若启用"任务挂起"功能
            if( *pxTicksToWait == portMAX_DELAY )
            {
                xReturn = pdFALSE;
            }
            else
        #endif

        // 场景1：处理xTickCount溢出导致的超时判断
        if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) )
        {
             /* 此时：
             * 1. 滴答计数器溢出次数已变化（xNumOfOverflows != 初始值）；
             * 2. 当前滴答计数 >= 初始滴答计数；
             * 说明xTickCount已溢出并完整循环一次，且已超过初始时间，判定为超时。 */
            xReturn = pdTRUE;
            *pxTicksToWait = ( TickType_t ) 0; // 剩余等待时间设为0
        }
        // 场景2：未超时，更新剩余等待时间
        else if( xElapsedTime < *pxTicksToWait )
        {
            /* 已流逝时间 < 剩余等待时间，未超时 */
            *pxTicksToWait -= xElapsedTime; // 更新剩余等待时间（减去已流逝部分）
            vTaskInternalSetTimeOutState( pxTimeOut ); // 重置超时状态（为下次检查做准备）
            xReturn = pdFALSE;
        }
        // 场景3：已超时（已流逝时间 >= 剩余等待时间）
        else
        {
            *pxTicksToWait = ( TickType_t ) 0; // 剩余等待时间设为0
            xReturn = pdTRUE;
        }
    }
    taskEXIT_CRITICAL();

    return xReturn;
}
```

> 当开始我们不是初始化了一个当前时间（*xTimeOut* ）吗，现在就比较一下新的当前时间（*xTickCount*）与*xTimeOut* 的差值不是不已经超过了我们指定的等待时间（*pxTicksToWait* ）

检查超时，如果超时了，那就返回*errQUEUE_FULL* 表示返回失败

```c
return errQUEUE_FULL;
```

检查超时，如果还没超时，那就放到“等待发送”列表，进入阻塞状态

```c
vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );
```

*vTaskPlaceOnEventList*实现如下

```c
// 将当前任务放入事件链表（等待事件发生），并设置等待超时时间
void vTaskPlaceOnEventList( List_t * const pxEventList,
                            const TickType_t xTicksToWait )
{
    // 将当前任务的事件链表项插入事件链表（按优先级排序）
    vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    // 将当前任务添加到延迟链表，设置超时时间（xTicksToWait）
    // 第二个参数pdTRUE表示：若超时，需从事件链表中移除任务
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
```

*prvAddCurrentTaskToDelayedList*实现如下

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

    // 若启用了“中止延迟”功能（INCLUDE_xTaskAbortDelay == 1），这里不考虑
    #if ( INCLUDE_xTaskAbortDelay == 1 )
    #endif

    /* 第一步：先将任务从就绪链表中移除
     * 原因：任务的xStateListItem链表项既用于就绪链表，也用于阻塞链表，不能同时存在于两个链表 */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        /* 若返回值为0，说明任务原本在就绪链表中（就绪链表移除成功）
         * 此时需重置“就绪优先级标记”（uxTopReadyPriority记录系统最高就绪优先级） */
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
    }else{}

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
                vListInsert( pxDelayedList, &( pxCurrentTCB->xStateListItem ) );

                /* 若当前任务的唤醒时间早于“下一个任务唤醒时间”（xNextTaskUnblockTime）：
                 * 更新xNextTaskUnblockTime为当前任务的唤醒时间
                 * 目的：调度器可通过xNextTaskUnblockTime快速确定下一次需要唤醒任务的时间，减少轮询开销 */
                if( xTimeToWake < xNextTaskUnblockTime )
                {
                    xNextTaskUnblockTime = xTimeToWake;
                }else{}
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
            }else{}
        }

        /* Avoid compiler warning when INCLUDE_vTaskSuspend is not 1. */
        ( void ) xCanBlockIndefinitely;
    }
    #endif /* INCLUDE_vTaskSuspend */
}
```

> 之前也说过这个函数，怎么添加到延迟列表里呢？从就绪列表里拿出来，然后放到延迟列表里就好了。不过有两个注意到地方：1. 放到延迟链表里后，可不是一直检测延迟链表有没有任务，任务什么时候唤醒，而是再放到延迟链表里的时候根据每个任务的延迟时间，计算下一个唤醒任务的时刻，看看到没到这个时刻就好了；2. 如果允许挂起的话延迟时间为portMAX_DELAY 表示挂起任务，要放到挂起链表里。

现在，我们已经把任务放到了等待发送链表（这里放的是任务的状态链表项），标记了延迟时间，放到了延迟链表里（这里放的是时间链表项）。接下来就解锁队列，看看有没有别的任务要执行，这样任务发送就处理完了。

## **（源码）****数据发送--中断安全版**

```c
// 函数定义：中断安全版队列通用发送函数（支持发送到头部/尾部或覆盖模式）
// 参数：
//   xQueue - 目标队列句柄
//   pvItemToQueue - 要发送的数据指针
//   pxHigherPriorityTaskWoken - 高优先级任务是否被唤醒的标记指针
//   xCopyPosition - 数据存储位置（头部/尾部/覆盖）
// 返回值：pdPASS表示成功；errQUEUE_FULL表示队列满且非覆盖模式
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue,
                                     const void * const pvItemToQueue,
                                     BaseType_t * const pxHigherPriorityTaskWoken,
                                     const BaseType_t xCopyPosition )
{
    BaseType_t xReturn;                     // 函数返回值
    UBaseType_t uxSavedInterruptStatus;     // 保存的中断状态（用于临界区保护）
    Queue_t * const pxQueue = xQueue;       // 队列句柄转换为内部结构体指针

    /* 与xQueueGenericSend类似，但队列满时不会阻塞。
     * 也不会直接唤醒因读取队列而阻塞的任务，而是返回一个标志表示
     * 是否需要上下文切换（即：此发送操作是否唤醒了优先级高于当前任务的任务）。 */
    
    // 进入中断安全的临界区（保存中断状态）
    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        // 队列未满 或 处于覆盖模式（允许覆盖已有数据）
        if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
        {
            const int8_t cTxLock = pxQueue->cTxLock;              // 队列发送锁状态
            const UBaseType_t uxPreviousMessagesWaiting = pxQueue->uxMessagesWaiting;  // 发送前的消息数量

            // 将数据复制到队列（根据xCopyPosition指定位置）
            ( void ) prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

            /* 若队列被锁定，则不修改事件列表。
             * 这将在队列解锁时处理。 */
            if( cTxLock == queueUNLOCKED )  // 队列未被锁定
            {
                #if ( configUSE_QUEUE_SETS == 1 )  // 若启用队列集功能
                #else /* 未启用队列集功能 */
                {
                    // 检查是否有任务在等待接收此队列的数据
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        // 移除等待接收队列中的任务并唤醒它
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* 被唤醒的任务优先级更高，记录需要上下文切换 */
                            if( pxHigherPriorityTaskWoken != NULL )
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }else{}
                        }else{}
                    }else{}

                    /* 此路径中未使用该变量 */
                    ( void ) uxPreviousMessagesWaiting;
                }
                #endif /* configUSE_QUEUE_SETS */
            }
            else  // 队列被锁定
            {
                /* 增加锁定计数，以便解锁队列的任务知道
                 * 在锁定期间有数据被发送。 */
                prvIncrementQueueTxLock( pxQueue, cTxLock );
            }

            // 发送成功
            xReturn = pdPASS;
        }
        else  // 队列已满且非覆盖模式
        {
            xReturn = errQUEUE_FULL;
        }
    }
    // 退出中断安全的临界区（恢复中断状态）
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
```

> 如果队列是满的，则不准写（与非中断版本不同，这里不等待），如果可以写，那就写，但是这里对于队列是否是锁定状态有处理，当队列未锁定时，那我们就可以对队列操作，这里我们去检查是否有任务可以接收。如果队列被锁定了，那就不能让其他任务来了，这里*prvIncrementQueueTxLock* 就是让发送锁/接收锁 +1，表示有数据写了，告诉解锁队列的任务知道。解锁队列的任务怎么处理？看函数*prvUnlockQueue* ，源码在下面

```c
static void prvUnlockQueue( Queue_t * const pxQueue )
{
    taskENTER_CRITICAL();
    {
        int8_t cTxLock = pxQueue->cTxLock;

        while( cTxLock > queueLOCKED_UNMODIFIED )
        {
            #if ( configUSE_QUEUE_SETS == 1 )
            #else /* configUSE_QUEUE_SETS */
            {
                /* Tasks that are removed from the event list will get added to
                 * the pending ready list as the scheduler is still suspended. */
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        /* The task waiting has a higher priority so record that
                         * a context switch is required. */
                        vTaskMissedYield();
                    }else{}
                }
                else
                {
                    break;
                }
            }
            #endif /* configUSE_QUEUE_SETS */

            --cTxLock;
        }

        pxQueue->cTxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();

    taskENTER_CRITICAL();
    {
        int8_t cRxLock = pxQueue->cRxLock;

        while( cRxLock > queueLOCKED_UNMODIFIED )
        {
            if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    vTaskMissedYield();
                }
                else{}

                --cRxLock;
            }
            else
            {
                break;
            }
        }

        pxQueue->cRxLock = queueUNLOCKED;
    }
    taskEXIT_CRITICAL();
}
```

> 这里很简单，如果*cTxLock > queueLOCKED_UNMODIFIED* 现在的锁大于我们加锁时候的值，那就说明有中断写了数据，那就一个一个处理，*cRxLock* 也一样。

## *（源码）*查看队列数据，不移除

```c
// 函数定义：从队列中查看数据（不删除数据）
// 参数：xQueue-队列句柄；pvBuffer-接收数据的缓冲区指针；xTicksToWait-等待超时时间（时钟周期）
// 返回值：pdPASS表示成功查看数据；errQUEUE_EMPTY表示队列空或超时
BaseType_t xQueuePeek( QueueHandle_t xQueue,
                       void * const pvBuffer,
                       TickType_t xTicksToWait )
{
    BaseType_t xEntryTimeSet = pdFALSE;  // 标记是否已设置超时起始时间（初始未设置）
    TimeOut_t xTimeOut;                  // 超时管理结构体（用于跟踪等待时间）
    int8_t * pcOriginalReadPosition;     // 保存原始读指针位置（用于查看后恢复）
    Queue_t * const pxQueue = xQueue;    // 将队列句柄转换为内部队列结构体指针

    // 无限循环：直到成功获取数据或超时退出
    for( ; ; )
    {
        // 进入临界区（禁用任务调度，保护队列操作）
        taskENTER_CRITICAL();
        {
            // 获取当前队列中的消息数量
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

            // 如果队列中有消息（且当前任务是最高优先级访问队列的任务）
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                // 保存原始读指针位置（因为查看操作不删除数据，后续需要恢复）
                pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;

                // 从队列复制数据到缓冲区（内部函数，处理实际数据拷贝）
                prvCopyDataFromQueue( pxQueue, pvBuffer );

                // 恢复读指针（因为是查看操作，不移动读指针）
                pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

                // 检查是否有其他任务在等待接收该队列的数据
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                {
                    // 将等待队列中的任务移除并唤醒（因为数据仍在队列中）
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        // 如果被唤醒的任务优先级更高，则触发任务切换
                        queueYIELD_IF_USING_PREEMPTION();
                    }else{}
                }else{}

                // 退出临界区
                taskEXIT_CRITICAL();

                return pdPASS;
            }
            else  // 队列为空的情况
            {
                // 如果超时时间为0（不等待），直接退出
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    // 退出临界区
                    taskEXIT_CRITICAL();

                    return errQUEUE_EMPTY;
                }
                // 如果未设置超时起始时间，则初始化超时结构体
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );  // 设置超时起始状态
                    xEntryTimeSet = pdTRUE;  // 标记已设置超时时间
                }else{}
            }
        }
        // 退出临界区（允许其他任务/中断访问队列）
        taskEXIT_CRITICAL();

        // 挂起所有任务调度（准备进入阻塞状态）
        vTaskSuspendAll();
        // 锁定队列（防止其他操作干扰）
        prvLockQueue( pxQueue );

        // 检查超时是否已发生（更新剩余等待时间）
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 超时未发生：检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // 解锁队列
                prvUnlockQueue( pxQueue );

                // 恢复任务调度，如果需要则触发任务切换
                if( xTaskResumeAll() == pdFALSE )
                {
                    taskYIELD_WITHIN_API();
                }else{}
            }
            else
            {
                // 队列已有数据：不进入阻塞状态，解锁队列并恢复调度，重新尝试获取数据
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else  // 超时已发生
        {
            // 解锁队列并恢复任务调度
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            // 检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                return errQUEUE_EMPTY;
            }else{}
        }
    }
}
```

大概看一下，这里的操作和发送时差不多，也是三类。

> 如果可以读，那就读取（不过这里要恢复读指针，因为我们只读不取），然后看看有没有任务等着取数据（等待读链表），有的话就唤醒它。 如果不能读，那就看看xTicksToWait 了，等于0说明不等待，现在读不到就算了。如果有时间，那就进入等待读的链表，标记延迟事件（放入延迟链表），然后该循环就循环，该切换任务就切换任务。

## *（源码）*查看队列数据，不移除--中断安全版

```c
// 函数定义：中断服务程序(ISR)中使用的队列查看函数（不删除数据）
// 参数：xQueue-队列句柄；pvBuffer-接收数据的缓冲区指针
// 返回值：pdPASS表示成功查看数据；pdFAIL表示失败（通常因为队列空）
BaseType_t xQueuePeekFromISR( QueueHandle_t xQueue,
                              void * const pvBuffer )
{
    BaseType_t xReturn;                     // 函数返回值（成功/失败）
    UBaseType_t uxSavedInterruptStatus;     // 保存的中断状态（用于退出临界区时恢复）
    int8_t * pcOriginalReadPosition;        // 保存原始读指针位置（用于查看后恢复）
    Queue_t * const pxQueue = xQueue;       // 将队列句柄转换为内部队列结构体指针

    // 进入中断安全的临界区，并保存当前中断状态
    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        // 中断中不能阻塞，因此直接检查队列中是否有数据
        if( pxQueue->uxMessagesWaiting > ( UBaseType_t ) 0 )
        {
            // 保存原始读指针位置（因为是查看操作，不删除数据）
            pcOriginalReadPosition = pxQueue->u.xQueue.pcReadFrom;
            // 从队列复制数据到缓冲区
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            // 恢复读指针（不移动读指针，数据仍在队列中）
            pxQueue->u.xQueue.pcReadFrom = pcOriginalReadPosition;

            // 标记成功
            xReturn = pdPASS;
        }
        else
        {
            // 队列为空，标记失败
            xReturn = pdFAIL;
        }
    }
    // 退出中断安全的临界区，恢复之前保存的中断状态
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
```

> 这里很简单，和之前差不多，没有加锁（因为没有取出数据）。

## *（源码）*读取队列数据，同时移除数据

如果看过前面的，这里很简单，一样的，不过之前是发送数据，现在是接收数据。

```c
// 函数定义：从队列接收数据（接收后数据会从队列中移除）
// 参数：xQueue-队列句柄；pvBuffer-接收数据的缓冲区指针；xTicksToWait-等待超时时间（时钟周期）
// 返回值：pdPASS表示成功接收数据；errQUEUE_EMPTY表示队列空或超时
BaseType_t xQueueReceive( QueueHandle_t xQueue,
                          void * const pvBuffer,
                          TickType_t xTicksToWait )
{
    BaseType_t xEntryTimeSet = pdFALSE;  // 标记是否已设置超时起始时间（初始未设置）
    TimeOut_t xTimeOut;                  // 超时管理结构体（用于跟踪等待时间）
    Queue_t * const pxQueue = xQueue;    // 将队列句柄转换为内部队列结构体指针

    // 无限循环：直到成功获取数据或超时退出
    for( ; ; )
    {
        // 进入临界区（禁用任务调度，保护队列操作）
        taskENTER_CRITICAL();
        {
            // 获取当前队列中的消息数量
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

            // 如果队列中有消息（且当前任务是最高优先级访问队列的任务）
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                // 数据可用，从队列复制数据到缓冲区
                prvCopyDataFromQueue( pxQueue, pvBuffer );
                // 消息数量减1（数据已被接收并移除）
                pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxMessagesWaiting - ( UBaseType_t ) 1 );

                // 队列现在有了空间，检查是否有任务在等待发送数据到该队列
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    // 移除等待发送队列中的任务并唤醒它
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        // 如果被唤醒的任务优先级更高，则触发任务切换
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else{}
                }else{}

                // 退出临界区
                taskEXIT_CRITICAL();

                return pdPASS;
            }
            else  // 队列为空的情况
            {
                // 如果超时时间为0（不等待），直接退出
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    // 退出临界区
                    taskEXIT_CRITICAL();

                    return errQUEUE_EMPTY;
                }
                // 如果未设置超时起始时间，则初始化超时结构体
                else if( xEntryTimeSet == pdFALSE )
                {
                    vTaskInternalSetTimeOutState( &xTimeOut );  // 设置超时起始状态
                    xEntryTimeSet = pdTRUE;  // 标记已设置超时时间
                }else{}
            }
        }
        // 退出临界区（允许其他任务/中断访问队列）
        taskEXIT_CRITICAL();

        // 挂起所有任务调度（准备进入阻塞状态）
        vTaskSuspendAll();
        // 锁定队列（防止其他操作干扰）
        prvLockQueue( pxQueue );

        // 检查超时是否已发生（更新剩余等待时间）
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 超时未发生：检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                // 队列为空：将当前任务加入等待接收队列，进入阻塞状态
                traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue );  // 跟踪阻塞操作
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // 解锁队列
                prvUnlockQueue( pxQueue );

                // 恢复任务调度，如果需要则触发任务切换
                if( xTaskResumeAll() == pdFALSE )
                {
                    taskYIELD_WITHIN_API();
                }
                else{}
            }
            else
            {
                // 队列已有数据：不进入阻塞状态，解锁队列并恢复调度，重新尝试获取数据
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeAll();
            }
        }
        else  // 超时已发生
        {
            // 解锁队列并恢复任务调度
            prvUnlockQueue( pxQueue );
            ( void ) xTaskResumeAll();

            // 检查队列是否仍为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                return errQUEUE_EMPTY;
            }else{}
        }
    }
}
```

## *（源码）*读取队列数据，同时移除数据--中断安全版本

一样的，之前是发送数据，现在是接收数据。

```c
BaseType_t xQueueReceiveFromISR( QueueHandle_t xQueue,
                                 void * const pvBuffer,
                                 BaseType_t * const pxHigherPriorityTaskWoken )
{
    BaseType_t xReturn;                     // 函数返回值（pdPASS/pdFAIL）
    UBaseType_t uxSavedInterruptStatus;     // 保存的中断状态（用于中断安全的临界区保护）
    Queue_t * const pxQueue = xQueue;       // 将队列句柄转换为内部结构体指针（FreeRTOS队列核心数据结构）
    
    // 进入中断安全的临界区：仅禁用低于“最大系统调用优先级”的中断，保证高优先级中断正常响应
    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        // 记录当前队列中的“消息数”（即待读取的数据项数量）
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting;

        /* 中断环境禁止阻塞，因此先检查队列是否有数据可读（消息数>0） */
        if( uxMessagesWaiting > ( UBaseType_t ) 0 )  // 队列非空，可读取数据
        {
            const int8_t cRxLock = pxQueue->cRxLock;  // 队列的“读取锁定状态”（区分发送锁定cTxLock）

            // 1. 从队列复制数据到缓冲区：按队列元素大小，将队列头部数据复制到pvBuffer
            prvCopyDataFromQueue( pxQueue, pvBuffer );
            // 2. 减少队列消息数：读取成功后，队列中待读取的数据项数量减1（腾出1个空间）
            pxQueue->uxMessagesWaiting = ( UBaseType_t ) ( uxMessagesWaiting - ( UBaseType_t ) 1 );

            /* 若队列被“读取锁定”，则不修改任务等待列表（事件列表），
             * 待队列解锁后，由解锁任务统一处理唤醒逻辑。 */
            if( cRxLock == queueUNLOCKED )  // 队列未被读取锁定，可正常处理任务唤醒
            {
                // 检查是否有任务因“队列满”阻塞（等待向队列发送数据）
                if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                {
                    // 从“等待发送任务列表”中移除任务并唤醒它（该任务现在可向队列发送数据）
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                    {
                        /* 被唤醒的任务优先级高于当前运行任务（即被中断打断的任务），
                         * 标记“需要上下文切换”，告知中断服务程序退出前触发切换。 */
                        if( pxHigherPriorityTaskWoken != NULL )
                        {
                            *pxHigherPriorityTaskWoken = pdTRUE;
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();  // 覆盖测试标记，无实际业务逻辑
                        }
                    }else{}
                }else{}
            }
            else  // 队列被读取锁定
            {
                /* 增加“读取锁定计数”：告知后续解锁队列的任务，
                 * “锁定期间有数据被读取，需处理等待发送的任务”。 */
                prvIncrementQueueRxLock( pxQueue, cRxLock );
            }

            // 数据读取成功，返回pdPASS
            xReturn = pdPASS;
        }
        else  // 队列为空，无数据可读取
        {
            xReturn = pdFAIL;  // 返回pdFAIL表示读取失败
        }
    }
    // 退出中断安全的临界区，恢复进入前的中断状态（确保不影响其他中断）
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
```

## *（源码）*查看队列中消息数量

```c
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;

    taskENTER_CRITICAL();
    {
        uxReturn = ( ( Queue_t * ) xQueue )->uxMessagesWaiting;
    }
    taskEXIT_CRITICAL();

    return uxReturn;
}
```

查看队列中消息数量直接读取队列结构体的参数就好了。

到此为止，其实已经说完了，之后没说的一些函数，都是读取结构体的参数然后判断一下就好。

![img](https://picx.zhimg.com/80/v2-55d26c5b002654a4ff7f2ccc29e0fcfc_1440w.png?source=ccfced1a)