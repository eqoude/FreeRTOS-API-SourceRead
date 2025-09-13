/*
列表（List）模块说明
本文件是调度器所使用的列表（List）实现代码。尽管该实现高度适配调度器的需求，但也可供应用代码使用。

list_t 类型仅能存储指向 list_item_t 类型的指针。每个 ListItem_t 结构体都包含一个数值（xItemValue）。
大多数情况下，列表会按照 item 值的升序进行排序。

列表在创建时会默认包含一个列表项（ListItem_t），该列表项的值被设置为可存储的最大可能值，
因此它始终位于列表的末尾，充当 “标记项”（marker）的角色。列表的成员变量 pxHead 始终指向这个标记项 —— 即便该标记项实际处于列表的尾部。
这样设计的原因是，尾部的标记项会包含一个 “回绕指针”，指向列表真正的头部。

除了自身的数值（xItemValue）外，每个列表项还包含以下指针：

指向列表中下一个列表项的指针（pxNext）；
指向该列表项所属列表的指针（pxContainer）；
指向包含该列表项的 “宿主对象” 的指针（pvOwner）。

后两个指针的存在是为了提高列表操作的效率，实际上在 “包含列表项的宿主对象” 与 “列表项本身” 之间建立了双向关联。

列表结构（list structure）的成员变量可能会在中断服务函数中被修改，因此从理论上讲，
这些成员变量应被声明为 volatile 类型（注：volatile 用于告知编译器 “变量值可能被意外修改”，避免编译器对其进行过度优化，确保每次访问都从内存读取最新值）。
然而，这些成员变量的修改仅会以 “功能上原子化” 的方式进行 —— 要么在临界区（critical sections）内修改，
要么在调度器挂起（scheduler suspended）的状态下修改；并且访问这些成员变量时，要么通过指针引用传入函数，
要么通过 volatile 类型的变量进行索引。
因此，在目前所有已测试的使用场景中，可省略 volatile 限定符，这样能在不影响功能正确性的前提下，带来一定的性能提升。
我们已检查过 IAR、ARM（Keil MDK）和 GCC 编译器在开启 “最高优化等级” 选项时生成的汇编指令，确认其行为符合预期设计。
尽管如此，随着编译器技术的发展，尤其是在启用 “激进的跨模块优化”（aggressive cross module optimisation）时（目前该场景尚未经过充分验证），
为保证优化后的代码正确性，可能仍需添加 volatile 限定符。
若因未给列表结构成员添加 volatile 限定符，且启用了激进的跨模块优化，导致编译器判定相关代码 “不必要” 并将其移除，
最终会造成调度器完全失效，且故障现象会非常明显。
若遇到此类情况，只需在 FreeRTOSConfig.h 中将 configLIST_VOLATILE 定义为 volatile（如本注释块末尾的示例所示），
即可为列表结构中的相关位置添加 volatile 限定符。
若未定义 configLIST_VOLATILE，则下方的预处理指令会将 configLIST_VOLATILE 直接 “消去”（即不添加任何限定符）。
如需为列表结构成员添加 volatile 限定符，请在 FreeRTOSConfig.h 中添加以下代码（无需引号）：
"#define configLIST_VOLATILE volatile"
*/

#ifndef configLIST_VOLATILE
    #define configLIST_VOLATILE
#endif /* configSUPPORT_CROSS_MODULE_OPTIMISATION */

//configSUPPORT_CROSS_MODULE_OPTIMISATION

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/*
INDENT-OFF
    这是一个特殊注释，用于告诉代码格式化工具（如某些 IDE 的自动缩进功能）在此处关闭自动缩进，保持后续代码的原始格式不变（通常用于避免宏定义或特殊语法被格式化工具误处理）。
#ifdef __cplusplus
    这是一个条件编译指令，__cplusplus 是 C++ 编译器定义的宏。当这段代码被 C++ 编译器编译时，该条件为真，会执行后续代码；若被 C 编译器编译，则跳过。
extern "C" {
    这是 C++ 特有的语法，用于告知 C++ 编译器：大括号 { 内的代码需要按照 C 语言的规则进行编译和链接（尤其是函数名的命名规则）。
*/

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

/*
 * Definition of the only type of object that a list can contain.
 * 列表（list）所能包含的唯一对象类型的定义。
 */
struct xLIST;
struct xLIST_ITEM
{
    listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE           
    /**< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
    /*< 若 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设为 1，则此字段被设为一个已知值（用于完整性校验） */
    configLIST_VOLATILE TickType_t xItemValue;          
    /**< The value being listed.  In most cases this is used to sort the list in ascending order. */
    /*< 列表项中存储的值。在大多数情况下，该值用于将列表按升序排序。 */
    struct xLIST_ITEM * configLIST_VOLATILE pxNext;     
    /**< Pointer to the next ListItem_t in the list. */
    /*< 指向列表中下一个 ListItem_t 的指针。 */
    struct xLIST_ITEM * configLIST_VOLATILE pxPrevious; 
    /**< Pointer to the previous ListItem_t in the list. */
    /*< 指向列表中上一个 ListItem_t 的指针。 */
    void * pvOwner;                                    
    /**< Pointer to the object (normally a TCB) that contains the list item.  There is therefore a two way link between the object containing the list item and the list item itself. */
    /*<  指向包含此列表项的对象（通常是任务控制块 TCB）的指针。因此，包含列表项的对象与列表项本身之间存在双向链接。 */
    struct xLIST * configLIST_VOLATILE pxContainer;     
    /**< Pointer to the list in which this list item is placed (if any). */
    /*< 指向此列表项所在列表（如果有的话）的指针。 */
    listSECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE          
    /**< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
};
typedef struct xLIST_ITEM ListItem_t;

#if ( configUSE_MINI_LIST_ITEM == 1 )
    struct xMINI_LIST_ITEM
    {
        listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE /**< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
        /**< 若 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设为 1，则此字段被设为一个已知值（用于完整性校验）。*/
            
        configLIST_VOLATILE TickType_t xItemValue;
        struct xLIST_ITEM * configLIST_VOLATILE pxNext;
        struct xLIST_ITEM * configLIST_VOLATILE pxPrevious;
    };
    typedef struct xMINI_LIST_ITEM MiniListItem_t;
#else
    typedef struct xLIST_ITEM      MiniListItem_t;
#endif

/*
 * Definition of the type of queue used by the scheduler.
 */
typedef struct xLIST
{
    listFIRST_LIST_INTEGRITY_CHECK_VALUE      
    /**< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
    configLIST_VOLATILE UBaseType_t uxNumberOfItems;
    /**< 列表中包含的列表项数量 */
    ListItem_t * configLIST_VOLATILE pxIndex; 
    /**< Used to walk through the list.  Points to the last item returned by a call to listGET_OWNER_OF_NEXT_ENTRY (). */
    /**< 用于遍历列表的索引指针，指向通过调用 listGET_OWNER_OF_NEXT_ENTRY () 函数返回的最后一个列表项。 */
    MiniListItem_t xListEnd;                  
    /**< List item that contains the maximum possible item value meaning it is always at the end of the list and is therefore used as a marker. */
    /**< 包含最大可能值的列表项，这意味着它始终位于列表的末尾，因此被用作列表的标记项（哨兵节点）。即链表最后一个节点 */
    listSECOND_LIST_INTEGRITY_CHECK_VALUE     
    /**< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
} List_t;


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

/* This function is not required in SMP. FreeRTOS SMP scheduler doesn't use
 * pxIndex and it should always point to the xListEnd. Not defining this macro
 * here to prevent updating pxIndex.
 */
#endif /* #if ( configNUMBER_OF_CORES == 1 ) */

/*
 * Version of uxListRemove() that does not return a value.  Provided as a slight
 * optimisation for xTaskIncrementTick() by being inline.
 *
 * Remove an item from a list.  The list item has a pointer to the list that
 * it is in, so only the list item need be passed into the function.
 *
 * @param uxListRemove The item to be removed.  The item will remove itself from
 * the list pointed to by it's pxContainer parameter.
 *
 * @return The number of items that remain in the list after the list item has
 * been removed.
 *
 * \page listREMOVE_ITEM listREMOVE_ITEM
 * \ingroup LinkedList
 */
#define listREMOVE_ITEM( pxItemToRemove ) \
    do {                                  \
        /* The list item knows which list it is in.  Obtain the list from the list \
         * item. */                                                                                 \
        List_t * const pxList = ( pxItemToRemove )->pxContainer;                                    \
                                                                                                    \
        ( pxItemToRemove )->pxNext->pxPrevious = ( pxItemToRemove )->pxPrevious;                    \
        ( pxItemToRemove )->pxPrevious->pxNext = ( pxItemToRemove )->pxNext;                        \
        /* Make sure the index is left pointing to a valid item. */                                 \
        if( pxList->pxIndex == ( pxItemToRemove ) )                                                 \
        {                                                                                           \
            pxList->pxIndex = ( pxItemToRemove )->pxPrevious;                                       \
        }                                                                                           \
                                                                                                    \
        ( pxItemToRemove )->pxContainer = NULL;                                                     \
        ( ( pxList )->uxNumberOfItems ) = ( UBaseType_t ) ( ( ( pxList )->uxNumberOfItems ) - 1U ); \
    } while( 0 )

/*
 * Inline version of vListInsertEnd() to provide slight optimisation for
 * xTaskIncrementTick().
 *
 * Insert a list item into a list.  The item will be inserted in a position
 * such that it will be the last item within the list returned by multiple
 * calls to listGET_OWNER_OF_NEXT_ENTRY.
 *
 * The list member pxIndex is used to walk through a list.  Calling
 * listGET_OWNER_OF_NEXT_ENTRY increments pxIndex to the next item in the list.
 * Placing an item in a list using vListInsertEnd effectively places the item
 * in the list position pointed to by pxIndex.  This means that every other
 * item within the list will be returned by listGET_OWNER_OF_NEXT_ENTRY before
 * the pxIndex parameter again points to the item being inserted.
 *
 * @param pxList The list into which the item is to be inserted.
 *
 * @param pxNewListItem The list item to be inserted into the list.
 *
 * \page listINSERT_END listINSERT_END
 * \ingroup LinkedList
 */
/*为提升性能，FreeRTOS 提供了内联宏版本的插入 / 删除函数，核心逻辑与同名函数（vListInsertEnd/uxListRemove）一致，但通过宏实现内联，减少函数调用开销（主要用于高频调用场景，如 xTaskIncrementTick 时钟中断）。*/
#define listINSERT_END( pxList, pxNewListItem )           \
    do {                                                  \
        ListItem_t * const pxIndex = ( pxList )->pxIndex; \
                                                          \
        /* Only effective when configASSERT() is also defined, these tests may catch \
         * the list data structures being overwritten in memory.  They will not catch \
         * data errors caused by incorrect configuration or use of FreeRTOS. */ \
        listTEST_LIST_INTEGRITY( ( pxList ) );                                  \
        listTEST_LIST_ITEM_INTEGRITY( ( pxNewListItem ) );                      \
                                                                                \
        /* Insert a new list item into ( pxList ), but rather than sort the list, \
         * makes the new list item the last item to be removed by a call to \
         * listGET_OWNER_OF_NEXT_ENTRY(). */                                                        \
        ( pxNewListItem )->pxNext = pxIndex;                                                        \
        ( pxNewListItem )->pxPrevious = pxIndex->pxPrevious;                                        \
                                                                                                    \
        pxIndex->pxPrevious->pxNext = ( pxNewListItem );                                            \
        pxIndex->pxPrevious = ( pxNewListItem );                                                    \
                                                                                                    \
        /* Remember which list the item is in. */                                                   \
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

/*
 * Must be called before a list is used!  This initialises all the members
 * of the list structure and inserts the xListEnd item into the list as a
 * marker to the back of the list.
 *
 * @param pxList Pointer to the list being initialised.
 *
 * \page vListInitialise vListInitialise
 * \ingroup LinkedList
 */
void vListInitialise( List_t * const pxList ) PRIVILEGED_FUNCTION;

/*
 * Must be called before a list item is used.  This sets the list container to
 * null so the item does not think that it is already contained in a list.
 *
 * @param pxItem Pointer to the list item being initialised.
 *
 * \page vListInitialiseItem vListInitialiseItem
 * \ingroup LinkedList
 */
void vListInitialiseItem( ListItem_t * const pxItem ) PRIVILEGED_FUNCTION;

/*
 * Insert a list item into a list.  The item will be inserted into the list in
 * a position determined by its item value (ascending item value order).
 *
 * @param pxList The list into which the item is to be inserted.
 *
 * @param pxNewListItem The item that is to be placed in the list.
 *
 * \page vListInsert vListInsert
 * \ingroup LinkedList
 */
void vListInsert( List_t * const pxList,
                  ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * Insert a list item into a list.  The item will be inserted in a position
 * such that it will be the last item within the list returned by multiple
 * calls to listGET_OWNER_OF_NEXT_ENTRY.
 *
 * The list member pxIndex is used to walk through a list.  Calling
 * listGET_OWNER_OF_NEXT_ENTRY increments pxIndex to the next item in the list.
 * Placing an item in a list using vListInsertEnd effectively places the item
 * in the list position pointed to by pxIndex.  This means that every other
 * item within the list will be returned by listGET_OWNER_OF_NEXT_ENTRY before
 * the pxIndex parameter again points to the item being inserted.
 *
 * @param pxList The list into which the item is to be inserted.
 *
 * @param pxNewListItem The list item to be inserted into the list.
 *
 * \page vListInsertEnd vListInsertEnd
 * \ingroup LinkedList
 */
void vListInsertEnd( List_t * const pxList,
                     ListItem_t * const pxNewListItem ) PRIVILEGED_FUNCTION;

/*
 * Remove an item from a list.  The list item has a pointer to the list that
 * it is in, so only the list item need be passed into the function.
 *
 * @param uxListRemove The item to be removed.  The item will remove itself from
 * the list pointed to by it's pxContainer parameter.
 *
 * @return The number of items that remain in the list after the list item has
 * been removed.
 *
 * \page uxListRemove uxListRemove
 * \ingroup LinkedList
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove ) PRIVILEGED_FUNCTION;

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* ifndef LIST_H */