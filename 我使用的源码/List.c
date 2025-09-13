/* 
 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 可以防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。
 这种重新  定义操作只应在从应用程序文件中包含 task.h 时进行。
*/
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "list.h"

/* 
 对于支持 MPU（内存保护单元）的移植版本（MPU ports），
 需要为上文的头文件定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 宏，但本文件中不应定义该宏。
 这样做是为了生成正确的 “特权模式（privileged）与非特权模式（unprivileged）” 链接关系及内存放置（placement）配置。 
*/
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE
//#undef 的作用
//显式取消宏定义，是为了 “局部限定宏的生效范围”：仅在包含上文头文件时用该宏屏蔽 MPU 包装，
//后续内核代码编译时，宏已失效，不会干扰其他逻辑（如内核内部对 API 函数的直接调用）。

/* 链表根节点初始化 */ 
void vListInitialise( List_t * const pxList )
{
    //  调用跟踪函数：记录“进入 vListInitialise 函数”的事件（用于调试/跟踪）
    // （trace 系列函数由 FreeRTOS 跟踪机制提供，默认可能为空实现，需开启跟踪功能）
    traceENTER_vListInitialise( pxList );

    //  初始化列表遍历索引：让 pxIndex 指向列表的“哨兵节点”xListEnd
    // （pxIndex 用于后续遍历列表，初始时列表仅含哨兵节点，故指向它）
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );

    // 为哨兵节点设置“第一个完整性校验值”
    // （仅当 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES=1 时生效，用于检测内存篡改）
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( &( pxList->xListEnd ) );

    // 设置哨兵节点的排序值：将 xItemValue 设为最大可能值（portMAX_DELAY）
    // （列表默认按 xItemValue 升序排序，哨兵节点值最大，确保其始终在列表末尾）
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* The list end next and previous pointers point to itself so we know
     * when the list is empty. */
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );

    /* Initialize the remaining fields of xListEnd when it is a proper ListItem_t */
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

    /* Write known values into the list if
     * configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
    // 为列表本身设置“完整性校验值”（仅当启用校验时生效，检测列表结构是否被篡改）
    listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );

    // 调用跟踪函数：记录“退出 vListInitialise 函数”的事件（用于调试/跟踪）
    traceRETURN_vListInitialise();
}
/*-----------------------------------------------------------*/

/* 链表节点初始化 */
void vListInitialiseItem( ListItem_t * const pxItem )
{
    traceENTER_vListInitialiseItem( pxItem );

    /* 确保链表节点标记为“未加入任何链表”状态。 */
    // pxContainer指向节点所属的链表（List_t），初始化为NULL表示节点暂不属于任何链表
    pxItem->pxContainer = NULL;

    /* 若configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES配置为1，
     * 则向链表节点写入已知校验值（用于检测内存破坏）。 */
    // 写入第一个完整性校验值（预定义的魔术数，如0x5a5a5a5a）
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
    // 写入第二个完整性校验值（另一个预定义魔术数，如0xa5a5a5a5）
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );

    traceRETURN_vListInitialiseItem();
}
/*-----------------------------------------------------------*/

/* 将节点插入到链表的尾部 */
void vListInsertEnd( List_t * const pxList,
                     ListItem_t * const pxNewListItem )
{
    ListItem_t * const pxIndex = pxList->pxIndex;

    traceENTER_vListInsertEnd( pxList, pxNewListItem );

    /* Only effective when configASSERT() is also defined, these tests may catch
     * the list data structures being overwritten in memory.  They will not catch
     * data errors caused by incorrect configuration or use of FreeRTOS. */
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
/*-----------------------------------------------------------*/

/* 将节点按照升序排列插入到链表 */
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
        /* *** NOTE ***********************************************************
        *  If you find your application is crashing here then likely causes are
        *  listed below.  In addition see https://www.FreeRTOS.org/FAQHelp.html for
        *  more tips, and ensure configASSERT() is defined!
        *  https://www.FreeRTOS.org/a00110.html#configASSERT
        *
        *   1) Stack overflow -
        *      see https://www.FreeRTOS.org/Stacks-and-stack-overflow-checking.html
        *   2) Incorrect interrupt priority assignment, especially on Cortex-M
        *      parts where numerically high priority values denote low actual
        *      interrupt priorities, which can seem counter intuitive.  See
        *      https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html and the definition
        *      of configMAX_SYSCALL_INTERRUPT_PRIORITY on
        *      https://www.FreeRTOS.org/a00110.html
        *   3) Calling an API function from within a critical section or when
        *      the scheduler is suspended, or calling an API function that does
        *      not end in "FromISR" from an interrupt.
        *   4) Using a queue or semaphore before it has been initialised or
        *      before the scheduler has been started (are interrupts firing
        *      before vTaskStartScheduler() has been called?).
        *   5) If the FreeRTOS port supports interrupt nesting then ensure that
        *      the priority of the tick interrupt is at or below
        *      configMAX_SYSCALL_INTERRUPT_PRIORITY.
        **********************************************************************/

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
/*-----------------------------------------------------------*/

/* 将节点从链表删除 */
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
/*-----------------------------------------------------------*/
