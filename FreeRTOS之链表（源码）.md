# FreeRTOS之链表（源码）

为什么我要从链表开始？

因为在FreeRTOS中任务的就绪、挂起........都是一个链表，有就绪链表、挂起链表.......让一个任务变成就绪态就是把相应的任务放到这个链表中，所以理解FreeRTOS的链表十分重要。

在说明链表前，我想拿出FreeRTOS源码里的定义，看看都有什么链表。

```c
/**< 按优先级排序的就绪链表。 */
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];              
/**< 延迟任务链表。 */
PRIVILEGED_DATA static List_t xDelayedTaskList1;      
/**< 延迟任务链表（使用两个链表 - 一个用于已溢出当前节拍计数的延迟任务）。 */
PRIVILEGED_DATA static List_t xDelayedTaskList2;                  
/**< 指向当前正在使用的延迟任务链表。 */         
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;        
/**< 指向当前用于存放已溢出当前节拍计数的任务的延迟任务链表。 */
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;     
/**< 调度器挂起时被就绪的任务。它们将在调度器恢复时移至就绪链表。 */
PRIVILEGED_DATA static List_t xPendingReadyList;  
/**< 已被删除但内存尚未释放的任务链表。 */
PRIVILEGED_DATA static List_t xTasksWaitingTermination; 
/**< 当前处于挂起状态的任务链表。 */
PRIVILEGED_DATA static List_t xSuspendedTaskList; 
/**< 当前活跃的定时器链表1。 */
PRIVILEGED_DATA static List_t xActiveTimerList1;
/**< 当前活跃的定时器链表2。 */
PRIVILEGED_DATA static List_t xActiveTimerList2;
/**< 指向当前正在使用的活跃定时器链表。 */
PRIVILEGED_DATA static List_t * pxCurrentTimerList;
/**< 指向溢出链表。 */
PRIVILEGED_DATA static List_t * pxOverflowTimerList;
```

这些链表数据类型都是 List_t ，List_t 也是一个节点。

那就下来就来看看源码吧，看看这些链表长什么样子，关于链表的定义和函数在<list.c>和<list.h>中。 依旧按给出源码->分析->总结的顺序来，这一节还是没有介绍供用户使用的接口，因为一般操作系统自动的完成链表的创建，不需要我们动手。

先看看 List_t 的定义吧：

```
typedef struct xLIST
{
    listFIRST_LIST_INTEGRITY_CHECK_VALUE      //（用于完整性校验）先不用管
    /**< 列表中包含的链表项数量 */
    configLIST_VOLATILE UBaseType_t uxNumberOfItems;
    /**< 用于遍历链表的索引指针，指向通过调用 listGET_OWNER_OF_NEXT_ENTRY () 函数返回的最后一个链表项。 */
    ListItem_t * configLIST_VOLATILE pxIndex; 
    /**< 包含最大可能值的链表项，这意味着它始终位于列表的末尾，因此被用作链表的标记项（哨兵节点）。即链表最后一个节点 */
    MiniListItem_t xListEnd;                
    listSECOND_LIST_INTEGRITY_CHECK_VALUE      //（用于完整性校验）先不用管
} List_t;
```

一个 List_t 大概长这个样子：

![img](https://picx.zhimg.com/80/v2-7a5b926146032c3ac37c3eb7c0f34f2a_1440w.png?source=ccfced1a)

可以把uxNumberOfItems 和 pxIndex 看作列表的状态信息，那就只剩下一个 xListEnd（MiniListItem_t），为什么我之前说 List_t 也是一个节点，因为可以把 List_t 看作一个 xListEnd （MiniListItem_t），类型为 MiniListItem_t 这是一个节点，把List_t 看作特殊节点（哨兵节点）。

下面是 MiniListItem_t（哨兵节点）的定义

```c
#if ( configUSE_MINI_LIST_ITEM == 1 )
    struct xMINI_LIST_ITEM
    {
        listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE       //（用于完整性校验）先不用管
            
        configLIST_VOLATILE TickType_t xItemValue;  // 节点的值（通常用于排序，如定时器到期时间）
        struct xLIST_ITEM * configLIST_VOLATILE pxNext;  // 指向链表中下一个节点的指针
        struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;  // 指向链表中上一个节点的指针
    };
    typedef struct xMINI_LIST_ITEM MiniListItem_t;
#else
    typedef struct xLIST_ITEM      MiniListItem_t;
#endif
```

默认情况下 *configUSE_MINI_LIST_ITEM == 1*

MiniListItem_t 大概长这个样子：

![img](https://picx.zhimg.com/80/v2-644e60ef237cdaea9bd39180aed03b3a_1440w.png?source=ccfced1a)

所以 List_t 应该是这个样子：

![img](https://pic1.zhimg.com/80/v2-c848bfd790bde3cda762b36b77f84a5e_1440w.png?source=ccfced1a)

哨兵节点知道了那普通的节点什么样子呢？

```c
struct xLIST_ITEM
{
    listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE      //（用于完整性校验）先不用管
    /*< 链表项中存储的值。在大多数情况下，该值用于将列表按升序排序。 */
    configLIST_VOLATILE TickType_t xItemValue;          
    /*< 指向链表中下一个 ListItem_t 的指针。 */
    struct xLIST_ITEM * configLIST_VOLATILE pxNext;     
    /*< 指向链表中上一个 ListItem_t 的指针。 */
    struct xLIST_ITEM * configLIST_VOLATILE pxPrevious; 
    /*<  指向包含此链表项的对象（通常是任务控制块 TCB）的指针。因此，包含链表项的对象与链表项本身之间存在双向链接。 */
    void * pvOwner;                                    
    /*< 指向此链表项所在列表（如果有的话）的指针。 */
    struct xLIST * configLIST_VOLATILE pxContainer;     
    listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE      //（用于完整性校验）先不用管
};
typedef struct xLIST_ITEM ListItem_t
```

ListItem_t 就是一个普通节点，如图：

![img](https://picx.zhimg.com/80/v2-4dfc171e240bf6e3a59121472c3b3f30_1440w.png?source=ccfced1a)

pvOwner和pxContainer需要解释一下： pvOwner指向链表项的对象，通常是任务控制块 TCB（每个任务都有一个TCB，具体是什么在介绍任务的时候说明，现在不用管他），也就是说pvOwner指向一个任务。 pxContainer指向链表项所在列表，就是List_t 。

他们有什么用呢？ 比如，我现在想把一个任务（节点）从当前链表（就绪链表）拿出来放到另一个链表中（挂起链表），通过pxContainer我们可以知道节点在哪个链表里，通过pvOwner我们可以知道这个节点是哪个任务。

关于把一个节点从一个链表拿出，放到另一个节点的具体操作，这里不详细说明，可以直接看最下面的源码，是简单的数据结构知识。

我这里主要说一下链表和节点的初始状态，涉及到两个函数*vListInitialise* 和 *vListInitialiseItem ，*下面给出源码。（我省略了一些调试用的代码）

### *vListInitialise：*链表根节点初始化

```c
/* 链表根节点初始化 */ 
void vListInitialise( List_t * const pxList )
{
    //  初始化列表遍历索引：让 pxIndex 指向列表的“哨兵节点”xListEnd
    // （pxIndex 用于后续遍历列表，初始时列表仅含哨兵节点，故指向它）
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );

    // 为哨兵节点设置“第一个完整性校验值”
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( &( pxList->xListEnd ) );

    // 设置哨兵节点的排序值：将 xItemValue 设为最大可能值（portMAX_DELAY）
    // （列表默认按 xItemValue 升序排序，哨兵节点值最大，确保其始终在列表末尾）
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* The list end next and previous pointers point to itself so we know
     * when the list is empty. */
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );

    // 若未启用“迷你列表项”（configUSE_MINI_LIST_ITEM=0），初始化哨兵节点的额外字段
    // （MiniListItem_t 是轻量版，无 pvOwner 和 pxContainer；ListItem_t 需初始化这两个字段）
    #if ( configUSE_MINI_LIST_ITEM == 0 )
    {
        pxList->xListEnd.pvOwner = NULL;
        pxList->xListEnd.pxContainer = NULL;
        listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( &( pxList->xListEnd ) );
    }
    #endif

    // 初始化列表项计数：当前列表仅含哨兵节点，有效列表项数量为 0
    pxList->uxNumberOfItems = ( UBaseType_t ) 0U;

    // 为列表本身设置“完整性校验值”（仅当启用校验时生效，检测列表结构是否被篡改）
    listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );

}
```

我首先说一下完整性校验的事，默认情况下完整性校验值为空，不过也可以自己设置，下面给出完整性校验相关源码。

```c
/* 以下宏可用于在列表结构（list structures）中植入已知值，随后在应用程序运行过程中检查这些已知值是否发生损坏。
这些宏或许能检测到列表数据结构在内存中被意外覆盖的情况，但无法检测因 FreeRTOS 配置错误或使用不当导致的数据错误。*/
#if ( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 0 )
    /* Define the macros to do nothing. */
    #define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE
    #define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE
    #define listFIRST_LIST_INTEGRITY_CHECK_VALUE
    #define listSECOND_LIST_INTEGRITY_CHECK_VALUE
    #define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
    #define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )
    #define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )
    #define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )
    #define listTEST_LIST_ITEM_INTEGRITY( pxItem )
    #define listTEST_LIST_INTEGRITY( pxList )
#else /* if ( configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES == 0 ) */
    /* Define macros that add new members into the list structures. */
    #define listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE     TickType_t xListItemIntegrityValue1;
    #define listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE    TickType_t xListItemIntegrityValue2;
    #define listFIRST_LIST_INTEGRITY_CHECK_VALUE          TickType_t xListIntegrityValue1;
    #define listSECOND_LIST_INTEGRITY_CHECK_VALUE         TickType_t xListIntegrityValue2;

/* Define macros that set the new structure members to known values. */
    #define listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )     ( pxItem )->xListItemIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
    #define listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem )    ( pxItem )->xListItemIntegrityValue2 = pdINTEGRITY_CHECK_VALUE
    #define listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList )              ( pxList )->xListIntegrityValue1 = pdINTEGRITY_CHECK_VALUE
    #define listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList )              ( pxList )->xListIntegrityValue2 = pdINTEGRITY_CHECK_VALUE

/* Define macros that will assert if one of the structure members does not
 * contain its expected value. */
    #define listTEST_LIST_ITEM_INTEGRITY( pxItem )                      configASSERT( ( ( pxItem )->xListItemIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxItem )->xListItemIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
    #define listTEST_LIST_INTEGRITY( pxList )                           configASSERT( ( ( pxList )->xListIntegrityValue1 == pdINTEGRITY_CHECK_VALUE ) && ( ( pxList )->xListIntegrityValue2 == pdINTEGRITY_CHECK_VALUE ) )
#endif /* configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES */
```

链表根节点初始化十分简单，就是设置一下指针指向和初值，可以看一下上面的源码，设置后大概就是下面这样：

![img](https://picx.zhimg.com/80/v2-6792d2f9aa4b6e80f3ece4cd13899334_1440w.png?source=ccfced1a)

### *vListInitialiseItem ：*链表节点初始化

```c
/* 链表节点初始化 */
void vListInitialiseItem( ListItem_t * const pxItem )
{
    /* 确保链表节点标记为“未加入任何链表”状态。 */
    // pxContainer指向节点所属的链表（List_t），初始化为NULL表示节点暂不属于任何链表
    pxItem->pxContainer = NULL;

    /* 若configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES配置为1，
     * 则向链表节点写入已知校验值（用于检测内存破坏）。 */
    // 写入第一个完整性校验值（预定义的魔术数，如0x5a5a5a5a）
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
    // 写入第二个完整性校验值（另一个预定义魔术数，如0xa5a5a5a5）
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
}
```

![img](https://picx.zhimg.com/80/v2-2eb8421f5d189bc31ba23da189741b02_1440w.png?source=ccfced1a)

为什么这里只初始化了pxContainer 呢？ 其他的项不同情况下有不同的作用，比如一个任务（Task），通常包含两个节点（xStateListItem 和 xEventListItem），一个定时器（timer），通常包含一个节点（xTimerListItem），他们的用法不同，节点的其它项在说到任务和定时器时再说明。

关于链表大致就介绍完毕了，在<list.c>和<list.h>中，还有很多函数，他们用来将节点插入链表，将节点从链表中移除，设置或者获得节点的内容，源码如下，很简单。

将节点插入到链表的尾部：

```c
void vListInsertEnd( List_t * const pxList,
                     ListItem_t * const pxNewListItem )
{
    ListItem_t * const pxIndex = pxList->pxIndex;

    traceENTER_vListInsertEnd( pxList, pxNewListItem );

    //  校验列表和新列表项的完整性（仅当启用数据完整性检查时生效）
    // （若列表或列表项的校验值被篡改，会触发断言，提示内存损坏）
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* Insert a new list item into pxList, but rather than sort the list,
     * makes the new list item the last item to be removed by a call to
     * listGET_OWNER_OF_NEXT_ENTRY(). */
    pxNewListItem->pxNext = pxIndex;
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;

    /* Only used during decision coverage testing. */
    // 测试覆盖率相关代码（实际运行时可能为空实现）
    mtCOVERAGE_TEST_DELAY();

    // 更新原有链表的指针，将新项接入链表：
    // - 当前索引前一项的下一个指针（pxNext）指向新项
    // - 当前索引的上一个指针（pxPrevious）指向新项
    pxIndex->pxPrevious->pxNext = pxNewListItem;
    pxIndex->pxPrevious = pxNewListItem;

    /* Remember which list the item is in. */
    pxNewListItem->pxContainer = pxList;

    ( pxList->uxNumberOfItems ) = ( UBaseType_t ) ( pxList->uxNumberOfItems + 1U );

    // 调用跟踪函数：记录“退出 vListInsertEnd 函数”的事件（调试用）
    traceRETURN_vListInsertEnd();
}
```

将节点按照升序排列插入到链表

```c
void vListInsert( List_t * const pxList,
                  ListItem_t * const pxNewListItem )
{
    // 定义迭代器指针（用于遍历列表找插入位置）和新项的排序值
    ListItem_t * pxIterator;
    const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;

    traceENTER_vListInsert( pxList, pxNewListItem );

    // 校验列表和新列表项的完整性（启用数据完整性检查时生效）
    // （若校验值被篡改，触发断言提示内存损坏，仅能检测物理覆盖，无法排查逻辑错误）
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    // 处理特殊情况：新项的排序值等于哨兵节点的最大值（portMAX_DELAY）
    // （哨兵节点 xListEnd 的 xItemValue 是最大值，若新项值与之相同，直接插入到哨兵节点前）
    if( xValueOfInsertion == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        // 遍历列表找插入位置：从哨兵节点开始，找到第一个“下一项值 > 新项值”的位置
        // （核心逻辑：按 xItemValue 升序插入，相同值的项插入到已有项之后，保证公平调度）
        for( pxIterator = ( ListItem_t * ) &( pxList->xListEnd ); pxIterator->pxNext->xItemValue <= xValueOfInsertion; pxIterator = pxIterator->pxNext )
        {
            /* There is nothing to do here, just iterating to the wanted
             * insertion position. */
            // 空循环体：仅通过循环条件移动迭代器到目标位置
        }
    }

    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewListItem;

    /* Remember which list the item is in.  This allows fast removal of the
     * item later. */
    pxNewListItem->pxContainer = pxList;

    ( pxList->uxNumberOfItems ) = ( UBaseType_t ) ( pxList->uxNumberOfItems + 1U );

    traceRETURN_vListInsert();
}
```

将节点从链表删除

```c
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
    /* The list item knows which list it is in.  Obtain the list from the list
     * item. */
    List_t * const pxList = pxItemToRemove->pxContainer;

    // 调用跟踪函数：记录“进入 uxListRemove 函数”的事件（调试用）
    traceENTER_uxListRemove( pxItemToRemove );

    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* Only used during decision coverage testing. */
    // 测试覆盖率相关代码（实际运行时可能为空实现，不影响功能）
    mtCOVERAGE_TEST_DELAY();

    /* Make sure the index is left pointing to a valid item. */
    // 维护遍历索引（pxIndex）的有效性：
    // 若当前列表的遍历索引正好指向待删除项，删除后需将索引指向待删除项的前一项
    // （避免后续遍历操作时，pxIndex 指向已被删除的无效内存）
    if( pxList->pxIndex == pxItemToRemove )
    {
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    pxItemToRemove->pxContainer = NULL;
    ( pxList->uxNumberOfItems ) = ( UBaseType_t ) ( pxList->uxNumberOfItems - 1U );

    traceRETURN_uxListRemove( pxList->uxNumberOfItems );

    return pxList->uxNumberOfItems;
}
```

```c
/* 初始化节点的拥有者 */
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )    ( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )

/* 获取节点拥有者 */
#define listGET_LIST_ITEM_OWNER( pxListItem )             ( ( pxListItem )->pvOwner )

/* 初始化节点排序辅助值 */
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )     ( ( pxListItem )->xItemValue = ( xValue ) )

/* 获取节点排序辅助值 */
#define listGET_LIST_ITEM_VALUE( pxListItem )             ( ( pxListItem )->xItemValue )

/* 获取链表根节点的节点计数器的值 */
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )        ( ( ( pxList )->xListEnd ).pxNext->xItemValue )

/* 获取链表的入口节点 */
#define listGET_HEAD_ENTRY( pxList )                      ( ( ( pxList )->xListEnd ).pxNext )

/* 获取节点的下一个节点 */
#define listGET_NEXT( pxListItem )                        ( ( pxListItem )->pxNext )

/* 获取链表的最后一个节点 */
#define listGET_END_MARKER( pxList )                      ( ( ListItem_t const * ) ( &( ( pxList )->xListEnd ) ) )

/* 判断链表是否为空 */
#define listLIST_IS_EMPTY( pxList )                       ( ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) ? pdTRUE : pdFALSE )

/* 获取链表的节点数 */
#define listCURRENT_LIST_LENGTH( pxList )                 ( ( pxList )->uxNumberOfItems )

/*
 * 获取链表中下一个节点所有者的访问函数。
 *
 * 链表成员 pxIndex 用于遍历链表。调用 listGET_OWNER_OF_NEXT_ENTRY 会将 pxIndex 递增到链表中的下一项，
 * 并返回该节点的 pxOwner 参数。因此，通过多次调用此函数，可以遍历链表中包含的所有项。
 *
 * 链表项的 pxOwner 参数是指向该链表项所有者的指针。在调度器中，这通常是一个任务控制块（TCB）。
 * pxOwner 参数有效地在链表项与其所有者之间建立了双向链接。
 *
 * @param pxTCB 用于存储下一个链表项所有者的地址。
 * @param pxList 要从中返回下一项所有者的链表。
 *
 * \page listGET_OWNER_OF_NEXT_ENTRY listGET_OWNER_OF_NEXT_ENTRY
 * \ingroup LinkedList
 */
#if ( configNUMBER_OF_CORES == 1 )
    // 单核（configNUMBER_OF_CORES == 1）场景
    #define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )                                       \
    do {                                                                                       \
        List_t * const pxConstList = ( pxList );                                               \
        /* 将索引递增到下一项并返回该项，确保不会返回链表末尾的标记（哨兵节点）。 */                 \
        ( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;                           \
        if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) ) \
        {                                                                                      \
            // 若索引指向哨兵节点，重置为列表头部（避免遍历到无效项）
            ( pxConstList )->pxIndex = ( pxConstList )->xListEnd.pxNext;                       \
        }                                                                                      \
        ( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;                                         \
    } while( 0 )
#else /* #if ( configNUMBER_OF_CORES == 1 ) */

/* 此函数在SMP（对称多处理）模式下不需要。FreeRTOS SMP调度器不使用
 * pxIndex，且它应始终指向xListEnd。不在此处定义此宏
 * 以防止更新pxIndex。
 */
#endif /* #if ( configNUMBER_OF_CORES == 1 ) */

/*
 * uxListRemove()的无返回值版本。通过内联方式为xTaskIncrementTick()提供轻微优化。
 *
 * 从链表中移除一个项。链表项包含指向其所在链表的指针，因此只需将链表项传入函数即可。
 *
 * @param uxListRemove 要移除的项。该项将从其pxContainer参数指向的链表中移除自己。
 *
 * @return 移除链表项后链表中剩余的项数。
 *
 * \page listREMOVE_ITEM listREMOVE_ITEM
 * \ingroup LinkedList
 */
#define listREMOVE_ITEM( pxItemToRemove ) \
    do {                                  \
        /* 链表项知道自己所在的链表。从链表项中获取链表。 */                                                                                 \
        List_t * const pxList = ( pxItemToRemove )->pxContainer;                                    \
                                                                                                    \
        ( pxItemToRemove )->pxNext->pxPrevious = ( pxItemToRemove )->pxPrevious;                    \
        ( pxItemToRemove )->pxPrevious->pxNext = ( pxItemToRemove )->pxNext;                        \
        /* 确保索引始终指向有效的项。 */                                 \
        if( pxList->pxIndex == ( pxItemToRemove ) )                                                 \
        {                                                                                           \
            pxList->pxIndex = ( pxItemToRemove )->pxPrevious;                                       \
        }                                                                                           \
                                                                                                    \
        ( pxItemToRemove )->pxContainer = NULL;                                                     \
        ( ( pxList )->uxNumberOfItems ) = ( UBaseType_t ) ( ( ( pxList )->uxNumberOfItems ) - 1U ); \
    } while( 0 )

/*
 * vListInsertEnd()的内联版本，为xTaskIncrementTick()提供轻微优化。
 *
 * 将链表项插入链表。该项的插入位置将使其成为多次调用
 * listGET_OWNER_OF_NEXT_ENTRY时最后返回的项。
 *
 * 链表成员pxIndex用于遍历链表。调用listGET_OWNER_OF_NEXT_ENTRY会将pxIndex递增到链表中的下一项。
 * 使用vListInsertEnd将项插入链表实际上是将项放在pxIndex指向的位置。这意味着
 * 在pxIndex再次指向被插入项之前，listGET_OWNER_OF_NEXT_ENTRY会返回链表中所有其他项。
 *
 * @param pxList 要插入项的链表。
 *
 * @param pxNewListItem 要插入链表的项。
 *
 * \page listINSERT_END listINSERT_END
 * \ingroup LinkedList
 */
/*为提升性能，FreeRTOS 提供了内联宏版本的插入 / 删除函数，核心逻辑与同名函数（vListInsertEnd/uxListRemove）一致，但通过宏实现内联，减少函数调用开销（主要用于高频调用场景，如 xTaskIncrementTick 时钟中断）。*/
#define listINSERT_END( pxList, pxNewListItem )           \
    do {                                                  \
        ListItem_t * const pxIndex = ( pxList )->pxIndex; \
                                                          \
        /* 仅当同时定义了configASSERT()时有效，这些测试可捕获内存中被覆盖的链表数据结构。
         * 它们无法捕获因FreeRTOS配置或使用不当导致的数据错误。 */ \
        listTEST_LIST_INTEGRITY( ( pxList ) );                                  \
        listTEST_LIST_ITEM_INTEGRITY( ( pxNewListItem ) );                      \
                                                                                \
        /* 将新链表项插入(pxList)，但不排序链表，而是使新链表项成为最后一个被
         * listGET_OWNER_OF_NEXT_ENTRY()调用返回的项。 */                                                        \
        ( pxNewListItem )->pxNext = pxIndex;                                                        \
        ( pxNewListItem )->pxPrevious = pxIndex->pxPrevious;                                        \
                                                                                                    \
        pxIndex->pxPrevious->pxNext = ( pxNewListItem );                                            \
        pxIndex->pxPrevious = ( pxNewListItem );                                                    \
                                                                                                    \
        /* 记录项所在的链表。 */                                                   \
        ( pxNewListItem )->pxContainer = ( pxList );                                                \
                                                                                                    \
        ( ( pxList )->uxNumberOfItems ) = ( UBaseType_t ) ( ( ( pxList )->uxNumberOfItems ) + 1U ); \
    } while( 0 )

/*
 * 获取链表中第一个节点所有者的访问函数。链表通常按节点值升序排序。
 *
 * 此函数返回链表第一个节点的 pxOwner 成员。链表项的 pxOwner 参数是指向该链表项所有者的指针，
 * 在调度器中，这通常是一个任务控制块（TCB）。pxOwner 参数有效地在链表项与其所有者之间建立了双向链接。
 *
 * @param pxList 要从中返回头部节点所有者的链表。
 *
 * \page listGET_OWNER_OF_HEAD_ENTRY listGET_OWNER_OF_HEAD_ENTRY
 * \ingroup LinkedList
 */
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )            ( ( &( ( pxList )->xListEnd ) )->pxNext->pvOwner )

/*
 * 检查某个链表项是否包含在指定链表中。链表项维护一个“容器（container）”指针，
 * 该指针指向其所属的链表。此宏仅需检查该容器指针与目标链表是否匹配即可。
 *
 * @param pxList 要判断链表项是否归属的目标链表。
 * @param pxListItem 要判断归属的链表项。
 * @return 若链表项在目标链表中，返回 pdTRUE；否则返回 pdFALSE。
 */
#define listIS_CONTAINED_WITHIN( pxList, pxListItem )    ( ( ( pxListItem )->pxContainer == ( pxList ) ) ? ( pdTRUE ) : ( pdFALSE ) )

/*
 * 返回某个链表项所属的链表（即该链表项被哪个链表引用）。
 *
 * @param pxListItem 要查询的链表项。
 * @return 指向引用该链表项的 List_t 类型对象的指针。
 */
#define listLIST_ITEM_CONTAINER( pxListItem )            ( ( pxListItem )->pxContainer )

/*
 * 提供一种简单的方式判断链表是否已初始化：vListInitialise() 函数会将 
 * pxList->xListEnd.xItemValue 设置为 portMAX_DELAY，通过此值可判断初始化状态。
 */
#define listLIST_IS_INITIALISED( pxList )                ( ( pxList )->xListEnd.xItemValue == portMAX_DELAY )
```

