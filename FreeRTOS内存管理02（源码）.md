# FreeRTOS内存管理02（源码）

之前已经看过<heap1.c><heap2.c><heap3.c>（[FreeRTOS内存管理01（源码）](./FreeRTOS内存管理01（源码）.md)），接下来继续，仍然是给出源码->分析->总结。（给出的源码并不完整，因为我省略了一些有关调试的内容）

## <heap4.c>

### 先来看 *pvPortMalloc()*  申请空间。

```c
void * pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t * pxBlock;          // 指向当前遍历的空闲块
    BlockLink_t * pxPreviousBlock;  // 指向当前块的前一个空闲块
    BlockLink_t * pxNewBlockLink;   // 指向拆分后新生成的空闲块
    void * pvReturn = NULL;         // 最终返回给用户的内存地址
    size_t xAdditionalRequiredSize; // 字节对齐所需的补充大小

    // 检查请求的大小是否有效（非0）
    if( xWantedSize > 0 )
    {
        /* 需增加请求大小，使其除了包含用户请求的字节数外，还能容纳一个 BlockLink_t 结构体（块头） */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )
        {
            xWantedSize += xHeapStructSize;

            /* 确保块始终满足所需的字节对齐要求 */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
            {
                /* 需要字节对齐：计算补充大小 */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                // 检查补充后是否溢出
                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )
                {
                    xWantedSize += xAdditionalRequiredSize;
                }
                else
                {
                    xWantedSize = 0; // 溢出则标记为无效大小
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // 覆盖率测试标记（无实际逻辑）
            }
        }
        else
        {
            xWantedSize = 0; // 溢出则标记为无效大小
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    // 挂起所有任务，确保内存分配操作的原子性（避免多任务竞争）
    vTaskSuspendAll();
    {
        /* 若首次调用 malloc，堆需要初始化以搭建空闲块链表 */
        if( pxEnd == NULL )
        {
            prvHeapInit();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        /* 检查待分配的块大小是否有效：不能占用 BlockLink_t 结构体 xBlockSize 成员的最高位（该位用于标记分配状态） */
        if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
        {
            // 检查大小是否有效且剩余空闲空间足够
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* 从链表起始位置（低地址）遍历，找到第一个大小足够的块 */
                pxPreviousBlock = &xStart;
                pxBlock = heapPROTECT_BLOCK_POINTER( xStart.pxNextFreeBlock ); // 解密指针（若启用堆保护）
                heapVALIDATE_BLOCK_POINTER( pxBlock ); // 验证指针是否在堆范围内

                // 遍历条件：当前块大小不足，且未到链表末尾
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != heapPROTECT_BLOCK_POINTER( NULL ) ) )
                {
                    pxPreviousBlock = pxBlock;
                    pxBlock = heapPROTECT_BLOCK_POINTER( pxBlock->pxNextFreeBlock ); // 解密下一个块指针
                    heapVALIDATE_BLOCK_POINTER( pxBlock );
                }

                /* 若未遍历到末尾（pxEnd），说明找到足够大的块 */
                if( pxBlock != pxEnd )
                {
                    /* 返回用户可用地址：跳过块头（BlockLink_t 结构体） */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxPreviousBlock->pxNextFreeBlock ) ) + xHeapStructSize );
                    heapVALIDATE_BLOCK_POINTER( pvReturn );

                    /* 将当前块从空闲链表中移除（分配给用户） */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* 若当前块远大于请求大小，将其拆分为“用户分配块”和“新空闲块” */
                    configASSERT( heapSUBTRACT_WILL_UNDERFLOW( pxBlock->xBlockSize, xWantedSize ) == 0 ); // 断言无下溢

                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
                    {
                        /* 计算新空闲块的地址：当前块地址 + 用户请求的总大小（含块头） */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                        configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 ); // 断言新块地址对齐

                        /* 计算拆分后两个块的大小 */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize; // 新空闲块大小
                        pxBlock->xBlockSize = xWantedSize; // 用户分配块大小（含块头）

                        /* 将新空闲块插入空闲链表（按大小顺序） */
                        pxNewBlockLink->pxNextFreeBlock = pxPreviousBlock->pxNextFreeBlock;
                        pxPreviousBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxNewBlockLink ); // 加密指针（若启用堆保护）
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    // 更新剩余空闲字节数
                    xFreeBytesRemaining -= pxBlock->xBlockSize;

                    // 更新历史最小剩余空闲字节数
                    if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                    {
                        xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 标记当前块为已分配（设置 xBlockSize 最高位），且已分配块无“下一个块”指针 */
                    heapALLOCATE_BLOCK( pxBlock );
                    pxBlock->pxNextFreeBlock = NULL;
                    xNumberOfSuccessfulAllocations++; // 成功分配次数+1
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // 未找到足够大的块
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // 大小无效或空闲空间不足
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); // 块大小无效（占用了最高位）
        }

        traceMALLOC( pvReturn, xWantedSize ); // 调试跟踪：记录分配的地址和大小
    }
    ( void ) xTaskResumeAll(); // 恢复任务调度

    // 若启用“分配失败钩子函数”，且分配失败，则调用钩子函数
    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* if ( configUSE_MALLOC_FAILED_HOOK == 1 ) */

    // 断言返回的地址满足字节对齐要求
    configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
    return pvReturn;
}
```

#### 第一步 申请空间的内存对齐

这一步与之前<heap2.c>的申请空间的对齐基本一致，不多说了。

#### 第二步 首次调用分配，堆需要初始化

堆的初始化调用了一个函数 p*rvHeapInit()  ，*下面给出它的源码。

```c
static void prvHeapInit( void )
{
    BlockLink_t * pxFirstFreeBlock;       // 指向初始的单个空闲块
    portPOINTER_SIZE_TYPE uxStartAddress; // 堆的实际起始地址（对齐后）
    portPOINTER_SIZE_TYPE uxEndAddress;   // 堆的实际结束地址（对齐后）
    size_t xTotalHeapSize = configTOTAL_HEAP_SIZE; // 堆的总大小（用户配置）

    /* 确保堆的起始地址满足正确的字节对齐要求 */
    uxStartAddress = ( portPOINTER_SIZE_TYPE ) ucHeap;

    // 若起始地址未对齐，调整为对齐地址
    if( ( uxStartAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
    {
        uxStartAddress += ( portBYTE_ALIGNMENT - 1 );  // 先增加补偿值，确保后续能对齐
        uxStartAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ); // 清除低n位，实现向上对齐
        xTotalHeapSize -= ( size_t ) ( uxStartAddress - ( portPOINTER_SIZE_TYPE ) ucHeap ); // 调整堆总大小（减去未对齐的部分）
    }

    /* xStart 用于存储空闲块链表的表头指针（头哨兵）。
     * 强制转换为 void* 是为了避免编译器警告。 */
    xStart.pxNextFreeBlock = ( void * ) heapPROTECT_BLOCK_POINTER( uxStartAddress ); // 头哨兵指向第一个空闲块（对齐后的起始地址）
    xStart.xBlockSize = ( size_t ) 0; // 头哨兵的大小设为0（仅作链表标记，无实际内存）

    /* pxEnd 用于标记空闲块链表的末尾（尾哨兵），放置在堆空间的末尾。 */
    uxEndAddress = uxStartAddress + ( portPOINTER_SIZE_TYPE ) xTotalHeapSize; // 堆的原始结束地址（起始地址+总大小）
    uxEndAddress -= ( portPOINTER_SIZE_TYPE ) xHeapStructSize; // 减去一个块头大小（尾哨兵需占用一个BlockLink_t的空间）
    uxEndAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ); // 确保尾哨兵地址对齐
    pxEnd = ( BlockLink_t * ) uxEndAddress; // 尾哨兵指针指向调整后的结束地址
    pxEnd->xBlockSize = 0; // 尾哨兵的大小设为0（仅作链表标记）
    pxEnd->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL ); // 尾哨兵的下一个指针设为NULL（链表结束标志）

    /* 初始状态下，堆中只有一个空闲块，占用除尾哨兵外的全部堆空间。 */
    pxFirstFreeBlock = ( BlockLink_t * ) uxStartAddress; // 第一个空闲块的起始地址就是堆的对齐后起始地址
    // 计算第一个空闲块的大小：尾哨兵地址 - 第一个空闲块地址（即堆可用空间总大小）
    pxFirstFreeBlock->xBlockSize = ( size_t ) ( uxEndAddress - ( portPOINTER_SIZE_TYPE ) pxFirstFreeBlock );
    pxFirstFreeBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd ); // 第一个空闲块的下一个指针指向尾哨兵

    /* 初始时只有一个空闲块，其大小就是堆的全部可用空间，因此剩余空闲字节数和历史最小空闲字节数均设为该大小。 */
    xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
}
```

##### 第一步 堆的对齐

这里的对齐和<heap1.c><heap2.c>提到过的是一样的，不多说，这里主要看一下堆空间

*uxStartAddress=(portPOINTER_SIZE_TYPE)ucHeap*   //*ucHeap是一个数组，堆起始地址就是数组的起始地址*

```c
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    PRIVILEGED_DATA static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif 
```

通过设置宏 *configAPPLICATION_ALLOCATED_HEAP* 可以用户自己定义这个堆空间，否则使用默认的空间。

##### 第二步 初始化链表

<heap4.c>也是用链表来管理的，这里初始化原始列表。（链表节点与<heap2.c>相同，一个xBlockSize 空间大小，一个pxNextFreeBlock 指向下一个节点）

初始化 xStart （首节点）：

```
    xStart.pxNextFreeBlock = ( void * ) heapPROTECT_BLOCK_POINTER( uxStartAddress ); // 头哨兵指向第一个空闲块（对齐后的起始地址）
    xStart.xBlockSize = ( size_t ) 0; // 头哨兵的大小设为0（仅作链表标记，无实际内存）
```

*heapPROTECT_BLOCK_POINTER（uxStartAddress）*默认情况下就是 *uxStartAddress*

![img](https://pic1.zhimg.com/80/v2-59ccc12f5d03eae1fa5a744a4daf37ab_1440w.png?source=ccfced1a)

初始化 pxEnd（尾节点）：

```c
    uxEndAddress = uxStartAddress + ( portPOINTER_SIZE_TYPE ) xTotalHeapSize; // 堆的原始结束地址（起始地址+总大小）
    uxEndAddress -= ( portPOINTER_SIZE_TYPE ) xHeapStructSize; // 减去一个块头大小（尾哨兵需占用一个BlockLink_t的空间）
    uxEndAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ); // 确保尾哨兵地址对齐
    pxEnd = ( BlockLink_t * ) uxEndAddress; // 尾哨兵指针指向调整后的结束地址
    pxEnd->xBlockSize = 0; // 尾哨兵的大小设为0（仅作链表标记）
    pxEnd->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL ); // 尾哨兵的下一个指针设为NULL（链表结束标志）
```

*pxEnd* 的起始地址 *uxEndAddress* 首先指向堆空间的末尾（*uxStartAddress +xTotalHeapSize*），在末尾挖出一个节点大小 （*uxEndAddress -=( portPOINTER_SIZE_TYPE ) xHeapStructSize*），然后确保内存对齐，给尾节点使用。

![img](https://pic1.zhimg.com/80/v2-af7193d2765f2840a58d78910096c8e8_1440w.png?source=ccfced1a)

初始化 pxFirstFreeBlock （链表中除哨兵节点的第一个节点）：

```c
    pxFirstFreeBlock = ( BlockLink_t * ) uxStartAddress; // 第一个空闲块的起始地址就是堆的对齐后起始地址
    // 计算第一个空闲块的大小：尾哨兵地址 - 第一个空闲块地址（即堆可用空间总大小）
    pxFirstFreeBlock->xBlockSize = ( size_t ) ( uxEndAddress - ( portPOINTER_SIZE_TYPE ) pxFirstFreeBlock );
    pxFirstFreeBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd ); // 第一个空闲块的下一个指针指向尾哨兵
```

![img](https://pica.zhimg.com/80/v2-0623d187a5f02e82373769832b30c996_1440w.png?source=ccfced1a)

​                                                                                                         初始链表

最后初始化一些关于链表状态的全局量

```c
    xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize; //堆的全部可用空间为剩余空闲字节数
    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize; //堆的全部可用空间为历史最小空闲字节数
```

这样链表的初始化就完成了。

返回去继续看 *pvPortMalloc()* 

#### 第三步 把堆的空间分配出去

分配前首先检查待分配的块大小是否有效：*if(heapBLOCK_SIZE_IS_VALID( xWantedSize )!=0)* 

如果你还记得<heap2.c>里 BlockLink_t 的有一个位说明这一块是否已经别分配出去了，这里也是一样的。块大小的最高位是分配状态标记。(*heapBLOCK_SIZE_IS_VALID来判断)*

> \#define heapBLOCK_ALLOCATED_BITMASK           \                                                        
> 						 ( ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 ) ) 
> #define heapBLOCK_SIZE_IS_VALID( xBlockSize )    \                                                         
> 						( ( ( xBlockSize ) & heapBLOCK_ALLOCATED_BITMASK ) == 0 )

检查大小是否有效且剩余空闲空间足够:

*if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )*

接下来就可以开始分配了，这里直接可以看一看源码，和之前的<heap2.c>大致一样（从链表中找到一个有足够大小的空闲块，分出用户要求大小，把剩余的空闲空间放回链表中），分出来的空闲块插入的操作也是一样的（从小到大）。

```c
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
```

![img](https://pic1.zhimg.com/80/v2-6160e542e99ead240013bfc16eb3442b_1440w.png?source=ccfced1a)

​																							第一次分配后

看完了申请空间，接下来看看是怎么释放的。

### 释放的函数 vPortFree():

```c
void vPortFree( void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;  // 将用户传入的内存指针转换为字节指针
    BlockLink_t * pxLink;              // 用于指向待释放块的块头（BlockLink_t 结构体）

    // 检查待释放的指针是否有效（非 NULL）
    if( pv != NULL )
    {
        /* 被释放的内存块前面，必然存在一个 BlockLink_t 结构体（块头） */
        puc -= xHeapStructSize;  // 从用户指针回退 xHeapStructSize 字节，定位到块头地址

        /* 强制转换是为了避免编译器发出警告 */
        pxLink = ( void * ) puc;  // 将块头地址转换为 BlockLink_t* 类型

        // 验证块头指针是否在堆的合法范围内（防止非法地址释放）
        heapVALIDATE_BLOCK_POINTER( pxLink );

        // 再次检查块是否处于已分配状态（双重保险，避免断言被关闭时出错）
        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            // 检查已分配块的“下一个指针”是否为 NULL（确保块结构未被破坏）
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* 块即将归还给堆 —— 标记为“空闲”状态 */
                heapFREE_BLOCK( pxLink );  // 清除块头 xBlockSize 的最高位（标记为空闲）

                #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )  // 若启用“释放时清零内存”配置
                {
                    /* 检查是否会下溢（防止块大小被篡改导致非法清零） */
                    if( heapSUBTRACT_WILL_UNDERFLOW( pxLink->xBlockSize, xHeapStructSize ) == 0 )
                    {
                        // 将用户数据区清零：从块头结束地址开始，长度为“块总大小 - 块头大小”
                        ( void ) memset( puc + xHeapStructSize, 0, pxLink->xBlockSize - xHeapStructSize );
                    }
                }
                #endif

                // 挂起所有任务，确保释放操作的原子性（避免多任务竞争空闲链表）
                vTaskSuspendAll();
                {
                    /* 将空闲块加入空闲链表 */
                    xFreeBytesRemaining += pxLink->xBlockSize;  // 更新剩余空闲字节数
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );  // 插入空闲链表
                    xNumberOfSuccessfulFrees++;                 // 成功释放次数 +1
                }
                ( void ) xTaskResumeAll();  // 恢复任务调度
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
}
```

这里的释放与之前<heap2.c>的基本一致，大概是找到释放内存块前面的节点，检查块状态是否正确，将块加入空闲链表。但是，这里有一个很不一样的地方，就是插入空闲链表这个函数  *prvInsertBlockIntoFreeList((( BlockLink_t \*) pxLink ))*

有什么不同呢，下面先给出源码，然后解释。

```c
static void prvInsertBlockIntoFreeList( BlockLink_t * pxBlockToInsert ) 
{
    BlockLink_t * pxIterator;  // 遍历空闲链表的迭代器指针
    uint8_t * puc;             // 用于地址计算的字节指针

    /* 遍历链表，直到找到地址高于待插入块的块（确定待插入块的位置） */
    for( pxIterator = &xStart; 
         heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) < pxBlockToInsert; 
         pxIterator = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        /* 无需额外操作，仅遍历到正确位置即可 */
    }

    // 若迭代器不是头哨兵，验证其指针是否在堆范围内
    if( pxIterator != &xStart )
    {
        heapVALIDATE_BLOCK_POINTER( pxIterator );
    }

    /* 待插入块与它前面的块（迭代器指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxIterator;

    // 前面块的结束地址（前面块地址 + 前面块大小）是否等于待插入块的起始地址
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        // 合并：前面块的大小 += 待插入块的大小
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        // 待插入块指向合并后的块（后续操作基于合并后的块）
        pxBlockToInsert = pxIterator;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
    }

    /* 待插入块与它后面的块（迭代器下一个指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxBlockToInsert;

    // 待插入块的结束地址（待插入块地址 + 待插入块大小）是否等于后面块的起始地址
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        // 确保后面的块不是尾哨兵（尾哨兵无实际内存，无需合并）
        if( heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) != pxEnd )
        {
            /* 将两个块合并为一个大块 */
            // 待插入块的大小 += 后面块的大小
            pxBlockToInsert->xBlockSize += heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->xBlockSize;
            // 待插入块的下一个指针指向后面块的下一个指针（跳过后面块）
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->pxNextFreeBlock;
        }
        else
        {
            // 若后面是尾哨兵，待插入块的下一个指针直接指向尾哨兵
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );
        }
    }
    else
    {
        // 无法合并，待插入块的下一个指针指向迭代器原本的下一个块
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 若待插入块同时与前面块和后面块合并（填补了中间的空隙），
     * 则它的 pxNextFreeBlock 指针已在前面的步骤中设置，
     * 此处无需重复设置（否则会导致指针指向自身）。 */
    if( pxIterator != pxBlockToInsert )
    {
        // 迭代器的下一个指针指向待插入块（完成插入）
        pxIterator->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxBlockToInsert );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
```

这里做了什么呢，可以先看看注释。

#### 第一步，找到地址高于待插入块的块（之前<heap2.c>是按照大小来的）。

```
    /* 遍历链表，直到找到地址高于待插入块的块（确定待插入块的位置） */
    for( pxIterator = &xStart; 
         heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) < pxBlockToInsert; 
         pxIterator = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        /* 无需额外操作，仅遍历到正确位置即可 */
    }

    // 若迭代器不是头哨兵，验证其指针是否在堆范围内
    if( pxIterator != &xStart )
    {
        heapVALIDATE_BLOCK_POINTER( pxIterator );
    }
```

#### 第二步，判断待插入块与它前面的块（迭代器指向的块）是否能组成连续的内存块？

```c
    /* 待插入块与它前面的块（迭代器指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxIterator;

    // 前面块的结束地址（前面块地址 + 前面块大小）是否等于待插入块的起始地址
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        // 合并：前面块的大小 += 待插入块的大小
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        // 待插入块指向合并后的块（后续操作基于合并后的块）
        pxBlockToInsert = pxIterator;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
    }
```

这里做了什么呢？ 还记得之前的<heap2.c>，被分配了的空间会一直保持分配的大小，不合并相邻块，会出现内存碎片。那这里通过判断释放的块和前面的空闲块是不是挨着，挨着就合并，这样下次分配的时候会更高效。下面举个例子：

> 比如先分配 100Byte、再分配 200Byte，之后释放，空闲链表中会有 “100Byte ” 和 “200Byte ”空闲块，此时如果需要两个“150Byte ” <heap2.c>只能从“200Byte ”空闲块分配出一个“150Byte ”，剩下 “100Byte ” 和 “50Byte ”空闲块，分不出一个完整的“150Byte ” <heap4.c>释放时会合并空闲块，释放后空闲链表中会有 “300Byte ”空闲块，可以分出两个完整的“150Byte ”

#### 第三步，判断待插入块与它后面的块（迭代器指向的块）是否能组成连续的内存块？

这里和上面一样，第二步和前面合并，第三步和后面合并：

```c
    /* 待插入块与它后面的块（迭代器下一个指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxBlockToInsert;

    // 待插入块的结束地址（待插入块地址 + 待插入块大小）是否等于后面块的起始地址
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        // 确保后面的块不是尾哨兵（尾哨兵无实际内存，无需合并）
        if( heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) != pxEnd )
        {
            /* 将两个块合并为一个大块 */
            // 待插入块的大小 += 后面块的大小
            pxBlockToInsert->xBlockSize += heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->xBlockSize;
            // 待插入块的下一个指针指向后面块的下一个指针（跳过后面块）
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->pxNextFreeBlock;
        }
        else
        {
            // 若后面是尾哨兵，待插入块的下一个指针直接指向尾哨兵
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );
        }
    }
    else
    {
        // 无法合并，待插入块的下一个指针指向迭代器原本的下一个块
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }
```

<heap2.c>和<heap4.c>对比

释放前：

![img](https://picx.zhimg.com/80/v2-5e60531ae52c0ed18c197fbf157ad894_1440w.png?source=ccfced1a)

释放后：

![img](https://picx.zhimg.com/80/v2-abb71557d03b46d3b7d6a1fa1148a420_1440w.png?source=ccfced1a)



​																						heap2.c

![img](https://picx.zhimg.com/80/v2-6a11b58090f14ef71506e5f9632b7ed3_1440w.png?source=ccfced1a)





​																						heap4.c

**总结：**

<heap4.c>利用链表管理空间，基本与<heap2.c>一致，但是释放时会合并相邻空闲块，提高了利用率。

## <heap5.c>

如果你有看过之前的<heap1.c><heap2.c><heap3.c><heap4.c>，那你完全可以自己看<heap5.c>的源码（可以忽略断言、堆保护、调试的一些函数，他们不影响分配方式），和<heap4.c>基本一致。

```c
void * pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t * pxBlock;          // 指向当前遍历的空闲块
    BlockLink_t * pxPreviousBlock;  // 指向当前块的前一个空闲块
    BlockLink_t * pxNewBlockLink;   // 指向拆分后新生成的空闲块
    void * pvReturn = NULL;         // 最终返回给用户的内存地址
    size_t xAdditionalRequiredSize; // 字节对齐所需的补充大小

    /* 首次调用 pvPortMalloc() 之前，堆必须已完成初始化。 */
    configASSERT( pxEnd );  // 断言堆已初始化（pxEnd 是堆尾哨兵，初始化后非NULL）

    // 检查请求的内存大小是否有效（非0）
    if( xWantedSize > 0 )
    {
        /* 需增加请求大小，使其除了包含用户所需字节数外，还能容纳一个 BlockLink_t 结构体（块头） */
        if( heapADD_WILL_OVERFLOW( xWantedSize, xHeapStructSize ) == 0 )  // 检查“加块头”是否溢出
        {
            xWantedSize += xHeapStructSize;  // 加上块头大小，得到总需求大小

            /* 确保块始终满足所需的字节对齐要求（避免CPU访问未对齐地址报错） */
            if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )  // 检查是否未对齐
            {
                /* 需要补充字节以实现对齐 */
                xAdditionalRequiredSize = portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK );

                // 检查“加对齐补充”是否溢出
                if( heapADD_WILL_OVERFLOW( xWantedSize, xAdditionalRequiredSize ) == 0 )
                {
                    xWantedSize += xAdditionalRequiredSize;  // 加上补充大小，完成对齐
                }
                else
                {
                    xWantedSize = 0;  // 溢出则标记为无效大小，分配失败
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际逻辑）
            }
        }
        else
        {
            xWantedSize = 0;  // 溢出则标记为无效大小，分配失败
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    // 挂起所有任务，确保内存分配操作的原子性（避免多任务竞争堆链表）
    vTaskSuspendAll();
    {
        /* 检查待分配的块大小是否有效：不能占用 BlockLink_t 结构体 xBlockSize 成员的最高位。
         * 该最高位用于标记块的归属（应用程序/内核），因此必须为0（空闲块标记）。 */
        if( heapBLOCK_SIZE_IS_VALID( xWantedSize ) != 0 )
        {
            // 检查大小有效且剩余空闲空间足够分配
            if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
            {
                /* 从链表起始位置（低地址）遍历，找到第一个大小足够的空闲块（首次适配算法） */
                pxPreviousBlock = &xStart;  // xStart 是堆头哨兵
                pxBlock = heapPROTECT_BLOCK_POINTER( xStart.pxNextFreeBlock );  // 解密指针（若启用堆保护）
                heapVALIDATE_BLOCK_POINTER( pxBlock );  // 验证指针是否在堆合法范围内

                // 遍历条件：当前块大小不足，且未到链表末尾（尾哨兵）
                while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != heapPROTECT_BLOCK_POINTER( NULL ) ) )
                {
                    pxPreviousBlock = pxBlock;
                    pxBlock = heapPROTECT_BLOCK_POINTER( pxBlock->pxNextFreeBlock );  // 解密下一个块指针
                    heapVALIDATE_BLOCK_POINTER( pxBlock );
                }

                /* 若未遍历到尾哨兵（pxEnd），说明找到大小足够的空闲块 */
                if( pxBlock != pxEnd )
                {
                    /* 返回用户可用地址：跳过块头（BlockLink_t 结构体），指向数据区起始位置 */
                    pvReturn = ( void * ) ( ( ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxPreviousBlock->pxNextFreeBlock ) ) + xHeapStructSize );
                    heapVALIDATE_BLOCK_POINTER( pvReturn );  // 验证返回地址合法性

                    /* 当前块已分配给用户，需从空闲链表中移除 */
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    /* 若当前块远大于需求大小，将其拆分为“用户分配块”和“新空闲块” */
                    configASSERT( heapSUBTRACT_WILL_UNDERFLOW( pxBlock->xBlockSize, xWantedSize ) == 0 );  // 断言拆分无下溢

                    if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )  // 剩余空间足够生成新块
                    {
                        /* 计算新空闲块的地址：当前块地址 + 用户总需求大小（含块头） */
                        pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                        configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );  // 断言新块地址对齐

                        /* 计算拆分后两个块的大小 */
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;  // 新空闲块大小
                        pxBlock->xBlockSize = xWantedSize;  // 用户分配块大小（含块头）

                        /* 将新空闲块插入空闲链表（保持地址升序） */
                        pxNewBlockLink->pxNextFreeBlock = pxPreviousBlock->pxNextFreeBlock;
                        pxPreviousBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxNewBlockLink );  // 加密指针（若启用堆保护）
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    // 更新剩余空闲字节数（减去分配块的大小）
                    xFreeBytesRemaining -= pxBlock->xBlockSize;

                    // 更新历史最小剩余空闲字节数（记录堆曾达到的最小空闲空间）
                    if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
                    {
                        xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 标记当前块为已分配：设置 xBlockSize 最高位，且已分配块无“下一个块”指针 */
                    heapALLOCATE_BLOCK( pxBlock );
                    pxBlock->pxNextFreeBlock = NULL;
                    xNumberOfSuccessfulAllocations++;  // 成功分配次数统计+1
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();  // 未找到足够大的块，分配失败
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 大小无效或空闲空间不足，分配失败
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  // 块大小无效（占用最高位），分配失败
        }

        traceMALLOC( pvReturn, xWantedSize );  // 调试跟踪：记录分配的地址和大小（供工具链调试用）
    }
    ( void ) xTaskResumeAll();  // 恢复任务调度，结束原子操作

    // 若启用“分配失败钩子函数”，且分配失败（pvReturn为NULL），则调用钩子函数
    #if ( configUSE_MALLOC_FAILED_HOOK == 1 )
    {
        if( pvReturn == NULL )
        {
            vApplicationMallocFailedHook();  // 应用程序可在该钩子中处理分配失败（如打印日志、复位）
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* if ( configUSE_MALLOC_FAILED_HOOK == 1 ) */

    // 断言返回的用户地址满足字节对齐要求（确保用户使用时无对齐问题）
    configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
    return pvReturn;  // 返回用户数据区地址（NULL表示分配失败）
}
/*-----------------------------------------------------------*/

void vPortFree( void * pv )
{
    uint8_t * puc = ( uint8_t * ) pv;  // 将用户传入的内存指针转换为字节指针（方便地址偏移）
    BlockLink_t * pxLink;              // 指向待释放块的块头（BlockLink_t 结构体，存储堆管理元信息）

    // 检查待释放的指针是否有效（非 NULL，避免释放空指针）
    if( pv != NULL )
    {
        /* 被释放的内存块前方，必然存在一个 BlockLink_t 结构体（即块头，记录块大小、链表指针等） */
        puc -= xHeapStructSize;  // 从用户数据区指针回退 xHeapStructSize 字节，定位到块头起始地址

        /* 强制转换是为了避免编译器因类型不匹配发出警告 */
        pxLink = ( void * ) puc;  // 将块头地址转换为 BlockLink_t* 类型，用于操作堆管理元信息

        // 验证块头指针是否在堆的合法地址范围内（防止释放堆外非法地址）
        heapVALIDATE_BLOCK_POINTER( pxLink );
        // 断言：待释放的块必须处于“已分配”状态（避免重复释放或释放空闲块）
        configASSERT( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 );
        // 断言：已分配块的“下一个空闲块指针”必须为 NULL（已分配块不参与空闲链表，此指针无效）
        configASSERT( pxLink->pxNextFreeBlock == NULL );

        // 再次检查块是否处于已分配状态（双重保险，即使断言被关闭也能避免错误）
        if( heapBLOCK_IS_ALLOCATED( pxLink ) != 0 )
        {
            // 确认已分配块的“下一个指针”为 NULL（防止块结构被篡改，如内存越界导致指针异常）
            if( pxLink->pxNextFreeBlock == NULL )
            {
                /* 该块即将归还给堆 —— 标记为“空闲”状态 */
                heapFREE_BLOCK( pxLink );  // 宏操作：清除块头 xBlockSize 的最高位（释放状态标记位）

                #if ( configHEAP_CLEAR_MEMORY_ON_FREE == 1 )  // 若启用“释放时清零内存”配置（增强数据安全性）
                {
                    /* 检查“块总大小 - 块头大小”是否会下溢（防止块大小被篡改导致非法内存操作） */
                    if( heapSUBTRACT_WILL_UNDERFLOW( pxLink->xBlockSize, xHeapStructSize ) == 0 )
                    {
                        // 将用户数据区清零：从块头结束地址（puc + xHeapStructSize）开始，长度为“块总大小 - 块头大小”
                        ( void ) memset( puc + xHeapStructSize, 0, pxLink->xBlockSize - xHeapStructSize );
                    }
                }
                #endif

                // 挂起所有任务，确保释放操作的原子性（避免多任务同时操作空闲链表导致指针混乱）
                vTaskSuspendAll();
                {
                    /* 将空闲块加入空闲链表（并合并相邻空闲块，减少内存碎片） */
                    xFreeBytesRemaining += pxLink->xBlockSize;  // 更新堆剩余空闲字节数（加上当前释放块的大小）
                    traceFREE( pv, pxLink->xBlockSize );        // 调试跟踪：记录释放的用户指针和块总大小（供调试工具使用）
                    prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );  // 核心操作：插入空闲链表并合并相邻块
                    xNumberOfSuccessfulFrees++;                 // 成功释放次数统计+1（用于堆状态监控）
                }
                ( void ) xTaskResumeAll();  // 恢复任务调度，结束原子操作
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际业务逻辑，仅用于测试工具统计代码覆盖率）
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( BlockLink_t * pxBlockToInsert ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxIterator;  // 遍历空闲链表的迭代器指针（用于定位插入位置）
    uint8_t * puc;             // 字节类型指针（用于计算内存地址，判断块是否连续）

    /* 遍历空闲链表，直到找到地址高于待插入块的空闲块（确定待插入块的前驱位置） */
    for( pxIterator = &xStart;  // 从链表头哨兵（xStart）开始遍历
         heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) < pxBlockToInsert;  // 下一个块地址 < 待插入块地址时继续遍历
         pxIterator = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )  // 迭代器移动到下一个块
    {
        /* 无需额外操作，仅通过循环定位到正确的插入位置 */
    }

    // 若迭代器不指向头哨兵（xStart），验证迭代器指向的块地址是否在堆合法范围内
    if( pxIterator != &xStart )
    {
        heapVALIDATE_BLOCK_POINTER( pxIterator );
    }

    /* 待插入块与它的前驱块（迭代器指向的块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxIterator;  // 将前驱块指针转换为字节指针，用于地址计算

    // 前驱块的结束地址（前驱块地址 + 前驱块总大小）是否等于待插入块的起始地址
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
    {
        // 合并前驱块与待插入块：前驱块总大小 += 待插入块总大小
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        // 待插入块指针指向合并后的块（后续操作基于合并后的大块，而非原待插入块）
        pxBlockToInsert = pxIterator;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（无实际业务逻辑，仅用于测试工具统计）
    }

    /* 待插入块（可能已合并前驱块）与它的后继块（迭代器原本的下一个块）是否能组成连续的内存块？ */
    puc = ( uint8_t * ) pxBlockToInsert;  // 将待插入块（或合并后的块）指针转换为字节指针

    // 待插入块的结束地址（待插入块地址 + 待插入块总大小）是否等于后继块的起始地址
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) )
    {
        // 确保后继块不是尾哨兵（pxEnd 无实际内存，无需合并）
        if( heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock ) != pxEnd )
        {
            /* 将待插入块与后继块合并为一个大块 */
            // 待插入块总大小 += 后继块总大小
            pxBlockToInsert->xBlockSize += heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->xBlockSize;
            // 待插入块的下一个指针指向后继块的下一个指针（跳过后继块，完成合并）
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxIterator->pxNextFreeBlock )->pxNextFreeBlock;
        }
        else
        {
            // 若后继块是尾哨兵，待插入块的下一个指针直接指向尾哨兵
            pxBlockToInsert->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );
        }
    }
    else
    {
        // 无法与后继块合并，待插入块的下一个指针指向迭代器原本的下一个块（保持链表顺序）
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 若待插入块同时合并了前驱块和后继块（填补了中间空隙），
     * 则它的 pxNextFreeBlock 指针已在前面的步骤中设置，
     * 此处无需重复赋值（否则会导致指针指向自身，破坏链表）。 */
    if( pxIterator != pxBlockToInsert )  // 若未合并前驱块（迭代器与待插入块指针不同）
    {
        // 迭代器的下一个指针指向待插入块，将待插入块接入空闲链表
        pxIterator->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxBlockToInsert );
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
```

那这个<heap5.c>和之前的有什么不同呢，在<heap5.c>源码开头有一段注释

```c
/* 使用说明：
 *
 * 1. 调用 pvPortMalloc() 之前，***必须***先调用 vPortDefineHeapRegions()。
 * 2. 若创建任何任务对象（任务、队列、事件组等），都会触发 pvPortMalloc() 调用，
 *    因此 vPortDefineHeapRegions() ***必须***在所有其他对象定义之前调用。
 *
 * vPortDefineHeapRegions() 接收一个参数，该参数是 HeapRegion_t 结构体数组。
 * HeapRegion_t 在 portable.h 中定义如下：
 *
 * typedef struct HeapRegion
 * {
 *  uint8_t *pucStartAddress; // 属于堆的某块内存的起始地址
 *  size_t xSizeInBytes;      // 该块内存的大小（字节数）
 * } HeapRegion_t;
 *
 * 数组需用“空地址+零大小”的区域定义作为结束标志，且数组中定义的内存区域
 * ***必须***按地址从低到高的顺序排列。以下是该函数的有效使用示例：
 *
 * HeapRegion_t xHeapRegions[] =
 * {
 *  { ( uint8_t * ) 0x80000000UL, 0x10000 }, // 定义一块起始地址为 0x80000000、大小为 0x10000 字节的内存
 *  { ( uint8_t * ) 0x90000000UL, 0xa0000 }, // 定义一块起始地址为 0x90000000、大小为 0xa0000 字节的内存
 *  { NULL, 0 }                               // 数组结束标志
 * };
 *
 * vPortDefineHeapRegions( xHeapRegions ); // 将数组传入 vPortDefineHeapRegions()
 *
 * 注意：0x80000000 是较低地址，因此在数组中排在前面。
 *
 */
```

怎么多出了一个 *HeapRegion_t*  结构体，这是因为<heap5.c>的堆空间和之前不一样了，之前堆空间是这样定义的：

```c
/* 为堆分配内存。 */
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )
    /* 应用程序编写者已经定义了用于RTOS堆的数组——可能是为了将其放置在特殊的段或地址中。 */
    extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
    PRIVILEGED_DATA static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif /* configAPPLICATION_ALLOCATED_HEAP */
```

是一个uint8_t数组，但是在<heap5.c>里的堆空间定义如上面的注释所示，是一个 HeapRegion_t 数组。在<heap5.c>里可没有默认的堆空间，要记得自己定义。那这个堆空间是什么样子的？可以自己看一看上面的注释使用说明写到：

```c
HeapRegion_t xHeapRegions[] =
{
 { ( uint8_t * ) 0x80000000UL, 0x10000 }, // 定义一块起始地址为 0x80000000、大小为 0x10000 字节的内存
 { ( uint8_t * ) 0x90000000UL, 0xa0000 }, // 定义一块起始地址为 0x90000000、大小为 0xa0000 字节的内存
 { NULL, 0 }                               // 数组结束标志
};
```

这不再是一段连续的空间了，之前的uint8_t数组是一段连续的空间。可以想象这里的堆空间样子如下：

![img](https://picx.zhimg.com/80/v2-b7ef5dc978a417fe5096b1cda8679867_1440w.png?source=ccfced1a)

*vPortDefineHeapRegions( xHeapRegions )*  对堆空间进行了初始化，源码如下：

```c
void vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions ) /* PRIVILEGED_FUNCTION */
{
    BlockLink_t * pxFirstFreeBlockInRegion = NULL;  // 指向当前区域的初始空闲块
    BlockLink_t * pxPreviousFreeBlock;              // 指向前一个区域的尾哨兵（用于链接多区域）
    portPOINTER_SIZE_TYPE xAlignedHeap;             // 当前区域对齐后的起始地址
    size_t xTotalRegionSize, xTotalHeapSize = 0;    // 当前区域总大小、所有区域总空闲大小
    BaseType_t xDefinedRegions = 0;                 // 已处理的区域计数（从0开始）
    portPOINTER_SIZE_TYPE xAddress;                 // 临时地址变量（用于计算）
    const HeapRegion_t * pxHeapRegion;              // 指向当前正在处理的 HeapRegion_t 结构体

    /* 该函数只能调用一次！（堆初始化后不可重复定义） */
    configASSERT( pxEnd == NULL );  // 断言堆未初始化（pxEnd 是尾哨兵，初始为NULL）

    #if ( configENABLE_HEAP_PROTECTOR == 1 )  // 若启用堆保护
    {
        // 获取随机金丝雀值（用于加密堆指针）
        vApplicationGetRandomHeapCanary( &( xHeapCanary ) );
    }
    #endif

    // 从第一个区域开始处理（xDefinedRegions 初始为0）
    pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );

    // 遍历所有区域，直到遇到“空地址+零大小”的终止区域
    while( pxHeapRegion->xSizeInBytes > 0 )
    {
        xTotalRegionSize = pxHeapRegion->xSizeInBytes;  // 当前区域的原始大小

        /* 确保当前区域的起始地址满足字节对齐要求（避免未对齐访问） */
        xAddress = ( portPOINTER_SIZE_TYPE ) pxHeapRegion->pucStartAddress;  // 当前区域原始起始地址

        if( ( xAddress & portBYTE_ALIGNMENT_MASK ) != 0 )  // 检查是否未对齐
        {
            xAddress += ( portBYTE_ALIGNMENT - 1 );  // 增加补偿值，确保后续能对齐
            xAddress &= ~( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK;  // 清除低n位，实现向上对齐
            // 调整区域总大小：减去未对齐部分的长度（对齐后可用空间减少）
            xTotalRegionSize -= ( size_t ) ( xAddress - ( portPOINTER_SIZE_TYPE ) pxHeapRegion->pucStartAddress );
        }

        xAlignedHeap = xAddress;  // 保存对齐后的区域起始地址

        /* 若处理的是第一个区域，初始化堆的头哨兵（xStart） */
        if( xDefinedRegions == 0 )
        {
            /* xStart 用于存储空闲块链表的表头指针（头哨兵）。
             * 强制转换是为了避免编译器警告。 */
            xStart.pxNextFreeBlock = ( BlockLink_t * ) heapPROTECT_BLOCK_POINTER( xAlignedHeap );
            xStart.xBlockSize = ( size_t ) 0;  // 头哨兵大小设为0（仅作链表标记）
        }
        else  // 处理非第一个区域
        {
            // 断言：非第一个区域时，前一个区域的尾哨兵已初始化（pxEnd 非NULL）
            configASSERT( pxEnd != heapPROTECT_BLOCK_POINTER( NULL ) );
            // 断言：区域必须按地址从低到高排列（当前区域地址 > 前一个区域尾哨兵地址）
            configASSERT( ( size_t ) xAddress > ( size_t ) pxEnd );
        }

        #if ( configENABLE_HEAP_PROTECTOR == 1 )  // 若启用堆保护
        {
            // 更新堆的最低地址（取所有区域的最小起始地址）
            if( ( pucHeapLowAddress == NULL ) ||
                ( ( uint8_t * ) xAlignedHeap < pucHeapLowAddress ) )
            {
                pucHeapLowAddress = ( uint8_t * ) xAlignedHeap;
            }
        }
        #endif /* configENABLE_HEAP_PROTECTOR */

        // 保存前一个区域的尾哨兵（用于链接当前区域）
        pxPreviousFreeBlock = pxEnd;

        /* 初始化当前区域的尾哨兵（pxEnd）：标记当前区域的空闲链表末尾，放置在区域末尾 */
        xAddress = xAlignedHeap + ( portPOINTER_SIZE_TYPE ) xTotalRegionSize;  // 当前区域原始结束地址
        xAddress -= ( portPOINTER_SIZE_TYPE ) xHeapStructSize;  // 减去尾哨兵的块头大小（BlockLink_t）
        xAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK );  // 确保尾哨兵地址对齐
        pxEnd = ( BlockLink_t * ) xAddress;  // 尾哨兵指针指向调整后的地址
        pxEnd->xBlockSize = 0;  // 尾哨兵大小设为0
        pxEnd->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL );  // 尾哨兵下一个指针设为NULL（链表结束）

        /* 初始化当前区域的初始空闲块：
         * 初始状态下，当前区域只有一个空闲块，占用除尾哨兵外的全部可用空间 */
        pxFirstFreeBlockInRegion = ( BlockLink_t * ) xAlignedHeap;  // 初始空闲块起始地址 = 区域对齐后起始地址
        // 初始空闲块大小 = 尾哨兵地址 - 初始空闲块地址（即区域可用空间总大小）
        pxFirstFreeBlockInRegion->xBlockSize = ( size_t ) ( xAddress - ( portPOINTER_SIZE_TYPE ) pxFirstFreeBlockInRegion );
        pxFirstFreeBlockInRegion->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );  // 初始空闲块下一个指针指向尾哨兵

        /* 若当前区域不是第一个区域，将前一个区域的尾哨兵与当前区域的初始空闲块链接 */
        if( pxPreviousFreeBlock != NULL )
        {
            pxPreviousFreeBlock->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxFirstFreeBlockInRegion );
        }

        // 累加所有区域的总空闲大小
        xTotalHeapSize += pxFirstFreeBlockInRegion->xBlockSize;

        #if ( configENABLE_HEAP_PROTECTOR == 1 )  // 若启用堆保护
        {
            // 更新堆的最高地址（取所有区域的最大结束地址）
            if( ( pucHeapHighAddress == NULL ) ||
                ( ( ( ( uint8_t * ) pxFirstFreeBlockInRegion ) + pxFirstFreeBlockInRegion->xBlockSize ) > pucHeapHighAddress ) )
            {
                pucHeapHighAddress = ( ( uint8_t * ) pxFirstFreeBlockInRegion ) + pxFirstFreeBlockInRegion->xBlockSize;
            }
        }
        #endif

        // 处理下一个区域
        xDefinedRegions++;
        pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );
    }

    // 初始化堆状态变量：历史最小空闲字节数 = 总空闲大小（初始无分配）
    xMinimumEverFreeBytesRemaining = xTotalHeapSize;
    // 剩余空闲字节数 = 总空闲大小
    xFreeBytesRemaining = xTotalHeapSize;

    // 断言：至少定义了一个有效区域（避免堆总大小为0）
    configASSERT( xTotalHeapSize );
}
```

我们直接来看while循环里面，看看是怎么对每一个区域初始化的。

第一步，是内存对齐，之前已经说过了，确保堆空间首地址是对齐的

```c
xTotalRegionSize = pxHeapRegion->xSizeInBytes;  // 当前区域的原始大小

/* 确保当前区域的起始地址满足字节对齐要求（避免未对齐访问） */
xAddress = ( portPOINTER_SIZE_TYPE ) pxHeapRegion->pucStartAddress;  // 当前区域原始起始地址

if( ( xAddress & portBYTE_ALIGNMENT_MASK ) != 0 )  // 检查是否未对齐
{
    xAddress += ( portBYTE_ALIGNMENT - 1 );  // 增加补偿值，确保后续能对齐
    xAddress &= ~( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK;  // 清除低n位，实现向上对齐
    // 调整区域总大小：减去未对齐部分的长度（对齐后可用空间减少）
    xTotalRegionSize -= ( size_t ) ( xAddress - ( portPOINTER_SIZE_TYPE ) pxHeapRegion->pucStartAddress );
}
xAlignedHeap = xAddress;  // 保存对齐后的区域起始地址
```

第二步，初始化堆的头哨兵（xStart）

```c
xAlignedHeap = xAddress;  // 保存对齐后的区域起始地址
/* 若处理的是第一个区域，初始化堆的头哨兵（xStart） */
if( xDefinedRegions == 0 )
{
    /* xStart 用于存储空闲块链表的表头指针（头哨兵）。
    * 强制转换是为了避免编译器警告。 */
    xStart.pxNextFreeBlock = ( BlockLink_t * ) heapPROTECT_BLOCK_POINTER( xAlignedHeap );
    xStart.xBlockSize = ( size_t ) 0;  // 头哨兵大小设为0（仅作链表标记）
}
```

![img](https://picx.zhimg.com/80/v2-bb906a7f4388afc7e2d0bbb96ecb0de0_1440w.png?source=ccfced1a)

第三步，初始化当前区域的尾哨兵（pxEnd ）（和<heap4.c>一样，从尾部挖出pxEnd 空间）

```
// 保存前一个区域的尾哨兵（用于链接当前区域）
pxPreviousFreeBlock = pxEnd;

/* 初始化当前区域的尾哨兵（pxEnd）：标记当前区域的空闲链表末尾，放置在区域末尾 */
xAddress = xAlignedHeap + ( portPOINTER_SIZE_TYPE ) xTotalRegionSize;  // 当前区域原始结束地址
xAddress -= ( portPOINTER_SIZE_TYPE ) xHeapStructSize;  // 减去尾哨兵的块头大小（BlockLink_t）
xAddress &= ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK );  // 确保尾哨兵地址对齐
pxEnd = ( BlockLink_t * ) xAddress;  // 尾哨兵指针指向调整后的地址
pxEnd->xBlockSize = 0;  // 尾哨兵大小设为0
pxEnd->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( NULL );  // 尾哨兵下一个指针设为NULL（链表结束）
```

![img](https://picx.zhimg.com/80/v2-48d33fec3b3436d6a1146e930e74c498_1440w.png?source=ccfced1a)

初始化当前区域的初始空闲块

```c
/* 初始化当前区域的初始空闲块：
 * 初始状态下，当前区域只有一个空闲块，占用除尾哨兵外的全部可用空间 */
pxFirstFreeBlockInRegion = ( BlockLink_t * ) xAlignedHeap;  // 初始空闲块起始地址 = 区域对齐后起始地址
// 初始空闲块大小 = 尾哨兵地址 - 初始空闲块地址（即区域可用空间总大小）
pxFirstFreeBlockInRegion->xBlockSize = ( size_t ) ( xAddress - ( portPOINTER_SIZE_TYPE ) pxFirstFreeBlockInRegion );
pxFirstFreeBlockInRegion->pxNextFreeBlock = heapPROTECT_BLOCK_POINTER( pxEnd );  // 初始空闲块下一个指针指向尾哨兵
```

![img](https://picx.zhimg.com/80/v2-77666953728a91f3325bc747d5ec1354_1440w.png?source=ccfced1a)

那后面的空间怎么初始化？可以自己看一看上面源码，其实和<heap4.c>是一样的，不过这里空间不是连续的了。

![img](https://picx.zhimg.com/80/v2-45ca4bf7c16f6bfde50924d231114b94_1440w.png?source=ccfced1a)

最后一个pxEND是真正的结尾

**总结：**

<heap5.c>和<heap4.c>一样，不过堆空间不是连续的，需要自己定义好。