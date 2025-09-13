# FreeRTOS内存管理01（源码）

FreeRTOS的内存分配分为静态和动态，静态分配需要我们手动预留存储空间，动态分配则由系统自动管理。动态分配有五种分配方式，本文从源码来看看内存是怎么管理的。（给出的源码并不完整，因为我省略了一些有关调试的内容）

每介绍一种分配方式，都会从源码开始，源码->解读->总结。

我会拿出一些关键代码去说明这种方式是怎么分配的，可以从官网下载源码，对着源码看本文，当然，不想看源码直接看本文也是可以的。

## ＜heap.1＞

首先来看 *pvPortMalloc()* 函数

```c
void * pvPortMalloc( size_t xWantedSize )
{
    void * pvReturn = NULL;  // 指向分配的内存块的指针，默认返回NULL（分配失败）
    static uint8_t * pucAlignedHeap = NULL;  // 指向对齐后的堆起始地址的指针

    /* 确保内存块始终按要求对齐。 */
    #if ( portBYTE_ALIGNMENT != 1 )  // 如果需要字节对齐（非1字节对齐）
    {
        if( xWantedSize & portBYTE_ALIGNMENT_MASK )  // 检查请求的大小是否未对齐
        {
            /* 需要字节对齐。检查是否会溢出。 */
            if( ( xWantedSize + ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) ) ) > xWantedSize )
            {
                // 调整大小以满足对齐要求
                xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
            }
            else
            {
                xWantedSize = 0;  // 溢出，标记为无效大小
            }
        }
    }
    #endif /* if ( portBYTE_ALIGNMENT != 1 ) */

    vTaskSuspendAll();  // 挂起所有任务，确保内存分配的原子性
    {
        if( pucAlignedHeap == NULL )
        {
            /* 确保堆的起始地址满足正确的对齐要求。 */
            pucAlignedHeap = ( uint8_t * ) ( ( ( portPOINTER_SIZE_TYPE ) & ucHeap[ portBYTE_ALIGNMENT - 1 ] ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );
        }

        /* 检查是否有足够的空间分配，并且验证参数有效性。 */
        if( ( xWantedSize > 0 ) &&                                /* 有效的大小 */
            ( ( xNextFreeByte + xWantedSize ) < configADJUSTED_HEAP_SIZE ) &&  /* 空间足够 */
            ( ( xNextFreeByte + xWantedSize ) > xNextFreeByte ) ) /* 检查是否溢出 */
        {
            /* 返回下一个空闲字节的地址，然后将索引递增以跳过此块。 */
            pvReturn = pucAlignedHeap + xNextFreeByte;
            xNextFreeByte += xWantedSize;
        }
    }
    ( void ) xTaskResumeAll();  // 恢复所有任务

    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )  // 如果启用了分配失败钩子函数
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();  // 调用分配失败钩子
        }
    }
    #endif
}
```

可以先看注释，这个函数大概进行了三步：

1. 首先确保要分配的内存块始终按要求对齐（我们申请的内存大小是对齐的）；
2. 然后对齐堆的起始地址（这是因为我们申请空间实际上是从`<heap1.h>`里定义的一个静态空间里申请的，这块空间的首地址不一定是对齐的，后面有说明）；
3. 计算下一个空闲字节的地址，返回首地址供我们使用。

### 第一步 申请空间的对齐

```c
    /* 确保内存块始终按要求对齐。 */
    #if ( portBYTE_ALIGNMENT != 1 )  // 如果需要字节对齐（非1字节对齐）
    {
        if( xWantedSize & portBYTE_ALIGNMENT_MASK )  // 检查请求的大小是否未对齐
        {
            /* 需要字节对齐。检查是否会溢出。 */
            if( ( xWantedSize + ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) ) ) > xWantedSize )
            {
                // 调整大小以满足对齐要求
                xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
            }
            else
            {
                xWantedSize = 0;  // 溢出，标记为无效大小
            }
        }
    }
    #endif /* if ( portBYTE_ALIGNMENT != 1 ) */
```

> *变量说明：*                                                                                                                                                                         *portBYTE_ALIGNMENT*  在 *<protable.h>*  里有说明，指按多少字节对齐（可以看作内存空间要求是多少字节的倍数） portBYTE_ALIGNMENT_MASK 是 *portBYTE_ALIGNMENT  对应的掩码* xWantedSize 是传入参数，指我们要申请空间的大小

```c
#if portBYTE_ALIGNMENT == 32
    #define portBYTE_ALIGNMENT_MASK    ( 0x001f )  // 32字节对齐的掩码
#elif portBYTE_ALIGNMENT == 16
    #define portBYTE_ALIGNMENT_MASK    ( 0x000f )  // 16字节对齐的掩码
#elif portBYTE_ALIGNMENT == 8
    #define portBYTE_ALIGNMENT_MASK    ( 0x0007 )  // 8字节对齐的掩码
#elif portBYTE_ALIGNMENT == 4
    #define portBYTE_ALIGNMENT_MASK    ( 0x0003 )  // 4字节对齐的掩码
#elif portBYTE_ALIGNMENT == 2
    #define portBYTE_ALIGNMENT_MASK    ( 0x0001 )  // 2字节对齐的掩码
#elif portBYTE_ALIGNMENT == 1
    #define portBYTE_ALIGNMENT_MASK    ( 0x0000 )  // 1字节对齐的掩码（无需对齐）
#else /* if portBYTE_ALIGNMENT == 32 */
    #error "Invalid portBYTE_ALIGNMENT definition"  // 错误：portBYTE_ALIGNMENT定义无效
#endif /* if portBYTE_ALIGNMENT == 32 */
```

首先检查请求的大小是否是对齐的 *if( xWantedSize & portBYTE_ALIGNMENT_MASK )*  查看低 n 位是否为0，低四位不为0，则需要对齐

![img](https://picx.zhimg.com/80/v2-b3b97f49896681a83ed4a732db1430fa_1440w.png?source=ccfced1a)





按位与，检查申请空间（xWantedSize）是否是要求对齐的整数倍

如果没有对齐，要对齐，对齐前检查是否会溢出 

```
if( ( xWantedSize + ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) ) ) > xWantedSize )
{
    // 调整大小以满足对齐要求
    xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
}
```

xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) )

> portBYTE_ALIGNMENT 前面提到过，指按多少字节对齐（可以看作内存空间要求是多少字节的倍数），portBYTE_ALIGNMENT_MASK 是按多少字节对齐的掩码 例如：portBYTE_ALIGNMENT = 4（0100），要求按4字节对齐；对应的portBYTE_ALIGNMENT_MASK = 0x0003 ；xWantedSize = 7（0111）。 ( xWantedSize & portBYTE_ALIGNMENT_MASK ) = 0111 & 0011 = 0011 ，计算申请空间溢出了多少 portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK )= 0100 - 0011 = 0001，这里其实就是计算我们申请的空间xWantedSize 差多少可以对齐（当前大小离下一个对齐边界还差多少字节） xWantedSize + 0001 = 0111 + 0001 = 1000 （十进位8）将申请空间扩大到对齐字节的整数倍，这样就完成对齐

为什么这里还要检查是否会溢出呢？

试想若 xWantedSize 是 8 位无符号 char 类型，最大值是 255，假设：    portBYTE_ALIGNMENT = 4（按 4 字节对齐）    xWantedSize = 254（已经非常接近 char 类型的最大值 255）    (xWantedSize & portBYTE_ALIGNMENT_MASK) = 254 & 3 = 2    需要补充的字节数 = 4 - 2 = 2    调整后大小 = 254 + 2 = 256 256 超过了 char 类型的最大值 255，会导致整数溢出（实际存储值会变成 0）

### 第二步 堆对齐

```
if( pucAlignedHeap == NULL )
{
    /* 确保堆的起始地址满足正确的对齐要求。 */
    pucAlignedHeap = ( uint8_t * ) ( ( ( portPOINTER_SIZE_TYPE ) & ucHeap[ portBYTE_ALIGNMENT - 1 ] ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );
 }
```

> *变量说明：* #define portPOINTER_SIZE_TYPE uint32_t 默认 portPOINTER_SIZE_TYPE  是 uint32_t ucHeap[ ]  *一个静态数组，这里就可以看出<heap1.c>分配的空间实际是从一个静态数组分配出来的*

```
/* 为堆分配内存。<heap1.c> */
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif
```

> 我们发现，通过 *configAPPLICATION_ALLOCATED_HEAP* 这个宏，我们可以选择负责分配的静态数组，可以使用默认的数组也可以我们自己定义一个数组。 数组的大小是 *configTOTAL_HEAP_SIZE  sizeof（uint8_t）  ，在<configFreeRTOS.h>定义了*  *#define configTOTAL_HEAP_SIZE  4096,*默认数组即堆的大小就是 8 *4096 个字节。

我们继续来看堆的地址是怎么对齐的

(~(( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK )) 我们前面提到 portBYTE_ALIGNMENT_MASK 是对齐的掩码（低 n 位为1，高位为0），~取反后 高位为1，低位为0。

& ucHeap[ portBYTE_ALIGNMENT -1]) 是堆空间的地址，但是为什么里面是 portBYTE_ALIGNMENT -1 呢？

**因为 ucHeap[] 首地址不一定是对齐的**，ucHeap[portBYTE_ALIGNMENT - 1]等价于ucHeap + (portBYTE_ALIGNMENT - 1)，即从原始堆地址向后偏移了 portBYTE_ALIGNMENT - 1 字节，为什么要偏移字节呢，假设没有偏移 &ucHeap[] & （~portBYTE_ALIGNMENT_MASK)，就有会将 &ucHeap[] 低n位抹去，这样不就超出了堆ucHeap[]的范围吗，所以要使用ucHeap[portBYTE_ALIGNMENT - 1]，为什么是 portBYTE_ALIGNMENT - 1，可以自己想一下，如果 &ucHeap[] 的低 n 位是 1、2、3........，会是什么情况。

可以看到实际堆的空间不是 8 * 4096，因为有内存对齐，所以实际要少一点。

![img](https://picx.zhimg.com/80/v2-752ea22ee137e83b6acf3b3bb8eef48e_1440w.png?source=ccfced1a)

### 第三步 分配申请空间

```
/* 检查是否有足够的空间分配，并且验证参数有效性。 */
if( ( xWantedSize > 0 ) &&                                /* 有效的大小 */
( ( xNextFreeByte + xWantedSize ) < configADJUSTED_HEAP_SIZE ) &&  /* 空间足够 */
( ( xNextFreeByte + xWantedSize ) > xNextFreeByte ) ) /* 检查是否溢出 */
{
    /* 返回下一个空闲字节的地址，然后将索引递增以跳过此块。 */
    pvReturn = pucAlignedHeap + xNextFreeByte;
    xNextFreeByte += xWantedSize;
}
```

> *变量说明：*  configADJUSTED_HEAP_SIZE ：堆实际空间 #define configADJUSTED_HEAP_SIZE    ( configTOTAL_HEAP_SIZE - portBYTE_ALIGNMENT ) 从上面可以看出因为有内存对齐，实际堆的空间不是 8 * 4096，这里按对齐可能浪费的最大空间来计算 xNextFreeByte ：指向下一个可分配地址

if ( )里面判断 xWantedSize 是有效的大小、当前堆的空闲空间足够、检查是否溢出，都很简单

*pvReturn = pucAlignedHeap + xNextFreeByte* 对齐后的首地址 + 下一个可分配地址 = 当前申请内存的地址 *xNextFreeByte += xWantedSize*   上次计算的下一个可分配地址 + 点钱申请的空间 =  下一个可分配地址

![image-20250913103513573](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913103513573.png)

接下来看一看*vPortFree()* 函数

```c
void vPortFree( void * pv )
{
    ( void ) pv;  // 未使用参数，避免编译器警告
}
```

不做任何事，<heap1.c>其实不允许释放

### 总结：

堆空间本质：ucHeap全局静态数组

<heap1.c>的分配是在一个事先定义好的静态空间中分配，不允许释放。

## ＜heap.2＞

首先来看 *pvPortMalloc()* 函数

```c
void * pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t * pxBlock;          // 指向当前遍历的空闲块
    BlockLink_t * pxPreviousBlock;  // 指向当前遍历块的前一个空闲块
    BlockLink_t * pxNewBlockLink;   // 指向拆分后新生成的空闲块
    void * pvReturn = NULL;         // 分配成功的内存地址（返回给用户）
    size_t xAdditionalRequiredSize; // 满足对齐所需的额外字节数

    if( xWantedSize > 0 )  // 检查请求的大小是否有效（非零）
    {
        /* 需增加请求大小，以容纳 BlockLink_t 结构体（块头）和用户请求的字节数 */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )  // 检查加法是否溢出
        {
            xWantedSize += xHeapStructSize;  // 加上块头大小
            /* 确保块大小始终满足平台要求的字节对齐 */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )  // 检查是否未对齐
            {
                /* 需要字节对齐，计算所需的额外字节数 */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )  // 检查溢出
                {
                    xWantedSize += xAdditionalRequiredSize;  // 调整为对齐大小
                }
                else
                {
                    xWantedSize = 0;  // 溢出，标记为无效大小
                }
            }
            else{}
        }
        else
        {
            xWantedSize = 0;  // 溢出，标记为无效大小
        }
    }
    else{}

    vTaskSuspendAll();  // 挂起所有任务，确保内存分配的原子性
    {
        /* 若首次调用 malloc，需初始化堆结构（构建空闲链表） */
        if( xHeapHasBeenInitialised == pdFALSE )
        {
            prvHeapInit();
            xHeapHasBeenInitialised = pdTRUE;
        }

        /* 检查待分配的块大小是否有效：块大小的最高位不能被设置（最高位是分配状态标记） */
        if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
        {
            /* 检查大小有效且堆中有足够空闲空间 */
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* 空闲链表按块大小升序排列，从链表头开始遍历，找到第一个足够大的块 */
                pxPreviousBlock = &xStart;  // 从哨兵节点 xStart 开始
                pxBlock = xStart.pxNextFreeBlock;  // 当前遍历的第一个空闲块

                // 遍历链表：直到找到大小≥请求值的块，或到达链表尾（xEnd）
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
                {
                    pxPreviousBlock = pxBlock;  // 记录前一个块
                    pxBlock = pxBlock->pxNextFreeBlock;  // 移动到下一个块
                }

                /* 若未到达链表尾（xEnd），说明找到了足够大的空闲块 */
                if( pxBlock != &xEnd )
                {
                    /* 返回给用户的地址 = 空闲块地址 + 块头大小（跳过 BlockLink_t 结构体） */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

                    /* 将当前块从空闲链表中移除（分配给用户） */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* 若当前块远大于请求大小，将其拆分为“用户分配块”和“新空闲块” */
                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                    {
                        /* 计算新空闲块的地址：当前块地址 + 用户请求的总大小（含块头） */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

                        /* 计算拆分后两个块的大小 */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;  // 新空闲块大小
                        pxBlock->xBlockSize = xWantedSize;  // 用户分配块大小（含块头）

                        /* 将新空闲块按大小顺序插入空闲链表 */
                        prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
                    }

                    xFreeBytesRemaining -= pxBlock->xBlockSize;  // 更新剩余空闲字节数

                    /* 标记当前块为“已分配”（设置块大小的最高位），并清空下一个块指针 */
                    heapALLOCATE_BLOCK( pxBlock );
                    pxBlock->pxNextFreeBlock = NULL;
                }
            }
        }
    }
    ( void ) xTaskResumeAll();  // 恢复所有任务调度

    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )  // 若启用分配失败钩子函数
    {
        if( pvReturn == NULL )  // 分配失败时调用钩子
        {
            vApplicationMallocFailedHook();
        }
    }
    #endif

    return pvReturn;  // 返回分配的内存地址（或 NULL 表示失败）
}
```

如果你手里面有源码，你可能注意到，这次多了一个结构体

```c
typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /*<< 链表中的下一个空闲块。 */
    size_t xBlockSize;                     /*<< 空闲块的大小。 */
} BlockLink_t;
```

我这里先给出定义，之后再做说明（如果你接触过内存管理，你可能已经知道这个结构体是用来干什么的了）

### 第一步 申请空间对齐

```c
    if( xWantedSize > 0 )  // 检查请求的大小是否有效（非零）
    {
        /* 需增加请求大小，以容纳 BlockLink_t 结构体（块头）和用户请求的字节数 */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )  // 检查 结构体+申请空间 是否溢出
        {
            xWantedSize += xHeapStructSize;  // 加上结构体大小
            /* 确保块大小始终满足平台要求的字节对齐 */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )  // 检查是否未对齐
            {
                /* 需要字节对齐，计算所需的额外字节数 */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )  // 检查溢出
                {
                    xWantedSize += xAdditionalRequiredSize;  // 调整为对齐大小
                }
                else
                {
                    xWantedSize = 0;  // 溢出，标记为无效大小
                }
            }
            else{}
        }
        else
        {
            xWantedSize = 0;  // 溢出，标记为无效大小
        }
    }
    else{}
```

> *变量、函数解释：（先前解释过的变量不再说明）* 
>
> *heapADD_WILL_OVERFLOW： 检查 a 和 b 相加是否会导致溢出* 
>     *#define heapADD_WILL_OVERFLOW( a, b )         ( ( a ) > ( heapSIZE_MAX - ( b ) ) )*
> *heapSIZE_MAX ： 适合 size_t 类型的最大值* 
> 	*#define heapSIZE_MAX                ( ~( ( size_t ) 0 ) )* 
> *xHeapStructSize ：链表 BlockLink_t 结构体的大小（满足平台的字节对齐要求）*  
> 	*static const size_t xHeapStructSize = ( ( sizeof( BlockLink_t ) + ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK ) );*      *这里的对齐与<heap1.c>里的堆对齐思路一样的*

上面的代码可以自己看一下，如果不懂可以返回去看一看<heap1.c>，都出现过的

总之这里计算了 申请空间 + 结构体 的大小，并且进行了对齐，最后计算出总的大小 *xWantedSize* 

### 第二步 首次调用 malloc，需初始化堆结构

```c
if( xHeapHasBeenInitialised == pdFALSE )
{
    prvHeapInit();
    xHeapHasBeenInitialised = pdTRUE;
}
```

来看看 *prvHeapInit()*

```c
/* 创建两个链表节点，用于标记链表的开头和结尾。 */
PRIVILEGED_DATA static BlockLink_t xStart, xEnd;

typedef struct A_BLOCK_LINK
{
    struct A_BLOCK_LINK * pxNextFreeBlock; /*<< 链表中的下一个空闲块。 */
    size_t xBlockSize;                     /*<< 空闲块的大小。 */
} BlockLink_t;

static void prvHeapInit( void ) 
{
    BlockLink_t * pxFirstFreeBlock;  // 指向第一个空闲块的指针
    uint8_t * pucAlignedHeap;        // 指向对齐后的堆起始地址

    /* 确保堆的起始地址满足正确的字节对齐要求。 */
    pucAlignedHeap = ( uint8_t * ) ( ( ( portPOINTER_SIZE_TYPE ) & ucHeap[ portBYTE_ALIGNMENT - 1 ] ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) );

    /* xStart 用于存储空闲块链表中第一个节点的指针。
     * 使用 void 强制转换是为了避免编译器警告。 */
    xStart.pxNextFreeBlock = ( void * ) pucAlignedHeap;
    xStart.xBlockSize = ( size_t ) 0;

    /* xEnd 用于标记空闲块链表的末尾。 */
    xEnd.xBlockSize = configADJUSTED_HEAP_SIZE;
    xEnd.pxNextFreeBlock = NULL;

    /* 初始化时，堆中只有一个空闲块，其大小等于整个堆的可用空间。 */
    pxFirstFreeBlock = ( BlockLink_t * ) pucAlignedHeap;
    pxFirstFreeBlock->xBlockSize = configADJUSTED_HEAP_SIZE;
    pxFirstFreeBlock->pxNextFreeBlock = &xEnd;
}
```

首先确保堆的起始地址（*`pucAlignedHeap`* ）满足正确的字节对齐要求

然后对两个全局的 `BlockLink_t` 进行初始化，*`xStart`* 第一个节点，指向堆开头，大小为零，*`xEnd`* 最后一个节点指向空（NULL），大小为堆大小。

为什么我这里把 `xStart`, `xEnd` 叫做节点呢，如果你了解过数据结构链表，*`BlockLink_t`* 结构体实际上就是链表的一个一个节点。如果你不知道链表，最好去了解一下。

最后，初始化堆的内容 *`pxFirstFreeBlock`* 指向堆开头，接下来设置 *`xBlockSize` 、`pxNextFreeBlock`* 。

现在这个链表就是这个样子

![img](https://pic1.zhimg.com/80/v2-a3da3762f19ebabb578347a88a1cef8c_1440w.png?source=ccfced1a)

堆空间大概就是这样的

![image-20250913103903077](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913103903077.png)

不过这里可要注意 *pxFirstFreeBlock* 可不是一个全局变量，实际上的 *pxFirstFreeBlock*  是写在堆空间里的

![img](https://pic1.zhimg.com/80/v2-212a4b4eca440520c96e761b640f0ec0_1440w.png?source=ccfced1a)

### 第三步 分配申请空间

```c
/* 检查待分配的块大小是否有效：块大小的最高位不能被设置（最高位是分配状态标记） */
if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
{
    /* 检查大小有效且堆中有足够空闲空间 */
    if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
    {
         /* 空闲链表按块大小升序排列，从链表头开始遍历，找到第一个足够大的块 */
         pxPreviousBlock = &xStart;  // 从哨兵节点 xStart 开始
         pxBlock = xStart.pxNextFreeBlock;  // 当前遍历的第一个空闲块

         // 遍历链表：直到找到大小≥请求值的块，或到达链表尾（xEnd）
         while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
         {
              pxPreviousBlock = pxBlock;  // 记录前一个块
              pxBlock = pxBlock->pxNextFreeBlock;  // 移动到下一个块
          }

          /* 若未到达链表尾（xEnd），说明找到了足够大的空闲块 */
          if( pxBlock != &xEnd )
          {
               /* 返回给用户的地址 = 空闲块地址 + 块头大小（跳过 BlockLink_t 结构体） */
               pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

               /* 将当前块从空闲链表中移除（分配给用户） */
               pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

               /* 若当前块远大于请求大小，将其拆分为“用户分配块”和“新空闲块” */
               if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
               {
                   /* 计算新空闲块的地址：当前块地址 + 用户请求的总大小（含块头） */
                   pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

                   /* 计算拆分后两个块的大小 */
                   pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;  // 新空闲块大小
                   pxBlock->xBlockSize = xWantedSize;  // 用户分配块大小（含块头）

                   /* 将新空闲块按大小顺序插入空闲链表 */
                   prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
               }

                xFreeBytesRemaining -= pxBlock->xBlockSize;  // 更新剩余空闲字节数

                /* 标记当前块为“已分配”（设置块大小的最高位），并清空下一个块指针 */
                heapALLOCATE_BLOCK( pxBlock );
                pxBlock->pxNextFreeBlock = NULL;
            }
        }
}
```

> *变量、函数解释：（先前解释过的变量不再说明）* 
>
> ```c
> heapBLOCK_SIZE_IS_VALID（）： 
>     #define heapBITS_PER_BYTE           ( ( size_t ) 8 )         //假设是8位字节！ 
>     #define heapBLOCK_ALLOCATED_BITMASK             \            //最高位
> 				 ( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 ) ) #define heapBLOCK_SIZE_IS_VALID( xBlockSize )               \            //判断最高位是否为0      
>                  ( ( ( xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) == 0 ) 
> xFreeBytesRemaining ： 跟踪剩余的空闲字节数，但不反映内存碎片情况。（内存碎片之后说明） 
>     PRIVILEGED_DATA static size_t xFreeBytesRemaining = configADJUSTED_HEAP_SIZE;
> configADJUSTED_HEAP_SIZE：
>     /* 为了字节对齐堆的起始地址，可能会损失几个字节。 */
> 	#define configADJUSTED_HEAP_SIZE    ( configTOTAL_HEAP_SIZE - portBYTE_ALIGNMENT )
> ```

首先判断块大小的最高位

> 为什么要先判断最高位是否为0？ `BlockLink_t` 结构体的 `xBlockSize` 成员的最高位用于跟踪块的分配状态。 当 `BlockLink_t` 结构体的 `xBlockSize` 成员的最高位被设置时，该块属于应用程序（已分配）。 当该位未设置时，该块仍属于空闲堆空间。

接下来我们直接跳过中间一段（中间的可以自己看一看，就是从链表中找到有足够空间的一个节点）我们看看找到之后是怎么处理的。

```c
/* 若未到达链表尾（xEnd），说明找到了足够大的空闲块 */
if( pxBlock != &xEnd )
{
    /* 返回给用户的地址 = 空闲块地址 + 块头大小（跳过 BlockLink_t 结构体） */
    pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

    /* 将当前块从空闲链表中移除（分配给用户） */
    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

     /* 若当前块远大于请求大小，将其拆分为“用户分配块”和“新空闲块” */
     if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
     {
          /* 计算新空闲块的地址：当前块地址 + 用户请求的总大小（含块头） */
          pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

          /* 计算拆分后两个块的大小 */
          pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;  // 新空闲块大小
          pxBlock->xBlockSize = xWantedSize;  // 用户分配块大小（含块头）

          /* 将新空闲块按大小顺序插入空闲链表 */
          prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
      }

      xFreeBytesRemaining -= pxBlock->xBlockSize;  // 更新剩余空闲字节数

      /* 标记当前块为“已分配”（设置块大小的最高位），并清空下一个块指针 */
      heapALLOCATE_BLOCK( pxBlock );
      pxBlock->pxNextFreeBlock = NULL;
}
```

刚开始说明了返回地址 返回给用户的地址 = 空闲块地址 + 块头大小（跳过 BlockLink_t 结构体），这里我们可以知道每次分配出去的空间前面有一个 BlockLink_t ，记录了当前块的信息，释放空间的时候需要读取这里的信息。

为什么不直接返回找到的空闲块呢？ 返回空闲块地址会覆盖块对应的结构体的信息。

![img](https://picx.zhimg.com/80/v2-3d11941a188868eefbd056aa7fbba973_1440w.png?source=ccfced1a)

接下来因为当前块被用户拿走了，所以将当前块从空闲链表中移除（分配给用户）

![img](https://pica.zhimg.com/80/v2-318b5c564997985325cb38860b932813_1440w.png?source=ccfced1a)

若当前块远大于请求大小，将其拆分为“用户分配块”和“新空闲块” *if(( pxBlock->xBlockSize - xWantedSize )> heapMINIMUM_BLOCK_SIZE )*

>  heapMINIMUM_BLOCK_SIZE ： 
>  	#define heapMINIMUM_BLOCK_SIZE    ( ( size_t ) ( xHeapStructSize * 2 ） 
>  xHeapStructSize ：BlockLink_t 对齐后大小

为什么是两倍的 *xHeapStructSize*  呢？ 因为要分出两个块，所以要两个。

接下来就很简单了，对链表指针进行操作就好了，都很简单，可以自己读源码，看看图：

原来的空闲块分了为两个

![image-20250913104100280](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913104100280.png)

分开后指向

![image-20250913104207466](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913104207466.png)

但是接下来还有两个函数需要说一下

> *prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );  /\* 将新空闲块按大小顺序插入空闲链表 \*/*

```c
/*
 * 将块插入空闲块链表中——链表按块大小排序。
 * 小 block 位于链表开头，大 block 位于链表末尾。
 */
#define prvInsertBlockIntoFreeList( pxBlockToInsert )         
    {                          
        BlockLink_t * pxIterator;      
        size_t xBlockSize;            
                                      
        xBlockSize = pxBlockToInsert->xBlockSize;
        
        /* 遍历链表，直到找到一个大小大于待插入块的块 */    
        for( pxIterator = &xStart; pxIterator->pxNextFreeBlock->xBlockSize < xBlockSize; pxIterator = pxIterator->pxNextFreeBlock )
        {                   
            /* 这里无需执行任何操作，只需迭代到正确位置即可。 */    
        }                                                                                                            
        /* 更新链表，将待插入块放入正确位置。 */      
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;   
        pxIterator->pxNextFreeBlock = pxBlockToInsert;         
    }
```

找到链表中的位置（按大小），插入。可以自己看看。

接下来看一看*vPortFree()* 函数

```c
void vPortFree( void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;  // 将用户传入的指针转换为字节指针
    BlockLink_t * pxLink;              // 用于指向块头的指针

    if( pv != NULL )  // 检查待释放的指针是否有效（非NULL）
    {
        /* 被释放的内存前会有一个 BlockLink_t 结构体（块头） */
        puc -= xHeapStructSize;  // 从用户指针回退到块头地址

        /* 这种强制转换是为了避免某些编译器byte alignment warnings. */
        pxLink = ( void * ) puc;  // 将块头地址转换为 BlockLink_t* 类型

        // 再次检查块是否处于已分配状态（双重保险，避免断言被关闭时出错）
        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            // 检查已分配块的下一个指针是否为NULL（确保块未被破坏）
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* 块即将归还给堆 - 标记为“未分配”状态 */
                heapFREE_BLOCK( pxLink );  // 清除最高位（标记为空闲）

                #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )  // 若启用“释放时清零”配置
                {
                    // 将用户数据区清零（从块头结束地址开始，长度为数据区大小C）
                    ( void ) memset( puc + xHeapStructSize, 0, pxLink->xBlockSize - xHeapStructSize );
                }
                #endif

                vTaskSuspendAll();  // 挂起所有任务，确保释放操作的原子性
                {
                    /* 将块添加到空闲块链表中 */
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
                    xFreeBytesRemaining += pxLink->xBlockSize;  // 更新剩余空闲字节数
                }
                ( void ) xTaskResumeAll();  // 恢复任务调度
            }
        }
    }
}
```

> *变量、函数解释：（先前解释过的变量不再说明）*   
> xHeapStructSize：之前说过，是BlockLink_t 对齐后的大小 
> heapBLOCK_IS_ALLOCATED（）： 
> 	#define heapBLOCK_ALLOCATED_BITMASK          \    // 还记得有一个位标记内存空间是否被分配吗                                            
> 					( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 ) ) 
> 	#define heapBLOCK_IS_ALLOCATED( pxBlock )        \   //判断是否是已分配的空间                                          
> 					( ( ( pxBlock->xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) != 0 )

还记得之前说过的每个分配出去的块起那面有一个 BlockLink_t 吗，它存放了当前块的信息，我们先去找到这个空闲块。

> *puc -= xHeapStructSize;// 从用户指针回退到块头地址*

然后对当前块做一个检查

> *if(heapBLOCK_IS_ALLOCATED( pxLink )!=0) // 检查块是否处于已分配状态* 
> *if( pxLink->pxNextFreeBlock ==NULL)  // 检查已分配块的下一个指针是否为NULL（确保块未被破坏）*

接下来就要把块归还给堆了，这里是直接把将块添加到空闲块链表中

> *prvInsertBlockIntoFreeList((( BlockLink_t \*) pxLink ));*

不知道你有没有注意到这样做可能会出现一些问题

1. 块会被分割，但是不会合并，块被分配后大小就固定了
2. 快递分配只和空闲空间大小有关，这样空间会很混乱

记不记得之前提到过的内存碎片。

> 内存碎片是指在内存分配过程中，由于多次分配和释放操作，原本连续的大块内存被分割成许多分散的、不连续的小空闲块。这些小空闲块虽然总容量可能满足分配需求，但由于彼此不连续，无法被用于分配大的内存块，从而导致内存利用率降低，甚至出现 “有内存但分配失败” 的情况。

比如之前的

![image-20250913104354712](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913104354712.png)



释放后就变成了

![image-20250913104414745](C:\Users\27258\AppData\Roaming\Typora\typora-user-images\image-20250913104414745.png)

注意：放回时链表插入是从小到大

**总结：**

内存块管理：每个内存块都有BlockLink_t头部，记录块大小和下一个空闲块地址，空闲块通过链表串联。

分配：找大小≥需求且最接近需求的块（按块大小找）。

相较于<heap1.c>的方式，利用链表管理，加入了释放空间的函数，但是会有内存碎片、且不合并相邻空闲块

## ＜heap.3＞

这个就简单了，分配和释放只是调用了 *malloc()* 和 *free()*

下面是源码

```c
void * pvPortMalloc( size_t xWantedSize )
{
    void * pvReturn;

    vTaskSuspendAll();
    {
        pvReturn = malloc( xWantedSize );
        traceMALLOC( pvReturn, xWantedSize );
    }
    ( void ) xTaskResumeAll();

    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();
        }
    }
    #endif

    return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree( void * pv )
{
    if( pv != NULL )
    {
        vTaskSuspendAll();
        {
            free( pv );
            traceFREE( pv, 0 );
        }
        ( void ) xTaskResumeAll();
    }
}
```

篇幅有些长了，下面的内容下一篇再说明

## ＜heap.4＞

## ＜heap.5＞