/*
 * FreeRTOS Kernel V11.1.0
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */


#ifndef PORTMACRO_H
#define PORTMACRO_H

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR          char
#define portFLOAT         float
#define portDOUBLE        double
#define portLONG          long
#define portSHORT         short
#define portSTACK_TYPE    uint32_t
#define portBASE_TYPE     long

typedef portSTACK_TYPE   StackType_t;
typedef long             BaseType_t;
typedef unsigned long    UBaseType_t;

#if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS )
    typedef uint16_t     TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffff
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
    typedef uint32_t     TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffffffffUL

/* 32-bit tick type on a 32-bit architecture, so reads of the tick count do
 * not need to be guarded with a critical section. */
    #define portTICK_TYPE_IS_ATOMIC    1
#else
    #error configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width.
#endif
/*-----------------------------------------------------------*/

/* Architecture specifics. */
#define portSTACK_GROWTH      ( -1 )
#define portTICK_PERIOD_MS    ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT    8
#define portDONT_DISCARD      __attribute__( ( used ) )
/*-----------------------------------------------------------*/

/* Scheduler utilities. */
#define portYIELD()                                     \
    {                                                   \
        /* Set a PendSV to request a context switch. */ \
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; \
                                                        \
        /* Barriers are normally not required but do ensure the code is completely \
         * within the specified behaviour for the architecture. */ \
        __asm volatile ( "dsb" ::: "memory" );                     \
        __asm volatile ( "isb" );                                  \
    }

#define portNVIC_INT_CTRL_REG     ( *( ( volatile uint32_t * ) 0xe000ed04 ) )
#define portNVIC_PENDSVSET_BIT    ( 1UL << 28UL )
#define portEND_SWITCHING_ISR( xSwitchRequired ) \
    do                                           \
    {                                            \
        if( xSwitchRequired != pdFALSE )         \
        {                                        \
            traceISR_EXIT_TO_SCHEDULER();        \
            portYIELD();                         \
        }                                        \
        else                                     \
        {                                        \
            traceISR_EXIT();                     \
        }                                        \
    } while( 0 )
#define portYIELD_FROM_ISR( x )    portEND_SWITCHING_ISR( x )
/*-----------------------------------------------------------*/

/* Critical section management. */
// 翻译：/* 临界区管理相关接口定义 */
// 说明：注释点明这段代码的核心作用——提供临界区（保护代码不被中断/任务切换打断）的管理函数和宏

extern void vPortEnterCritical( void );
// 翻译：extern void vPortEnterCritical( void );
// 说明：声明外部函数 vPortEnterCritical（实际实现不在当前文件），功能是“进入临界区”

extern void vPortExitCritical( void );
// 翻译：extern void vPortExitCritical( void );
// 说明：声明外部函数 vPortExitCritical，功能是“退出临界区”

#define portSET_INTERRUPT_MASK_FROM_ISR()         ulPortRaiseBASEPRI()
// 翻译：#define portSET_INTERRUPT_MASK_FROM_ISR()         ulPortRaiseBASEPRI()
// 说明：定义宏 portSET_INTERRUPT_MASK_FROM_ISR，作用是“从中断服务程序（ISR）中设置中断屏蔽”，
//      实际调用函数 ulPortRaiseBASEPRI()（提升 BASEPRI 寄存器值，屏蔽低优先级中断），并返回屏蔽前的 BASEPRI 值

#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )    vPortSetBASEPRI( x )
// 翻译：#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )    vPortSetBASEPRI( x )
// 说明：定义宏 portCLEAR_INTERRUPT_MASK_FROM_ISR，作用是“从ISR中清除中断屏蔽”，
//      参数 x 是 portSET_INTERRUPT_MASK_FROM_ISR() 返回的“原屏蔽值”，
//      实际调用 vPortSetBASEPRI(x) 恢复到之前的中断屏蔽状态

#define portDISABLE_INTERRUPTS()                  vPortRaiseBASEPRI()
// 翻译：#define portDISABLE_INTERRUPTS()                  vPortRaiseBASEPRI()
// 说明：定义宏 portDISABLE_INTERRUPTS，作用是“禁用中断”，
//      实际调用 vPortRaiseBASEPRI() 屏蔽所有优先级低于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断

#define portENABLE_INTERRUPTS()                   vPortSetBASEPRI( 0 )
// 翻译：#define portENABLE_INTERRUPTS()                   vPortSetBASEPRI( 0 )
// 说明：定义宏 portENABLE_INTERRUPTS，作用是“启用中断”，
//      实际调用 vPortSetBASEPRI(0) 重置 BASEPRI 寄存器，恢复所有可屏蔽中断

#define portENTER_CRITICAL()                      vPortEnterCritical()
// 翻译：#define portENTER_CRITICAL()                      vPortEnterCritical()
// 说明：定义宏 portENTER_CRITICAL，作用是“进入临界区”，
//      实际调用外部函数 vPortEnterCritical()（禁用中断+递增临界区嵌套计数）

#define portEXIT_CRITICAL()                       vPortExitCritical()
// 翻译：#define portEXIT_CRITICAL()                       vPortExitCritical()
// 说明：定义宏 portEXIT_CRITICAL，作用是“退出临界区”，
//      实际调用外部函数 vPortExitCritical()（递减嵌套计数，计数为0时启用中断）

/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site.  These are
 * not necessary for to use this port.  They are defined so the common demo files
 * (which build with all the ports) will build. */
// 翻译：/* 正如 FreeRTOS.org 网站上所描述的任务函数宏。
//        这些宏对于使用本端口不是必需的。定义它们是为了让通用的演示文件
//        （需要与所有端口一起编译）能够编译通过。*/
// 解析：
// - 这些宏是为了兼容性而定义的，不是当前端口运行所必需的
// - 主要用途是让通用的演示代码能在不同的FreeRTOS端口上都能编译通过

#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
// 翻译：#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
// 解析：
// - 这是任务函数的原型声明宏
// - 展开后会生成形如 "void 函数名( void * 参数名 )" 的函数原型
// - 例如：portTASK_FUNCTION_PROTO( vTask1, pvParams ) 会展开为 void vTask1( void * pvParams )

#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )
// 翻译：#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )
// 解析：
// - 这是任务函数的定义宏
// - 展开后会生成形如 "void 函数名( void * 参数名 )" 的函数定义
// - 例如：portTASK_FUNCTION( vTask1, pvParams ) { ... } 会展开为 void vTask1( void * pvParams ) { ... }
/*-----------------------------------------------------------*/

/* Tickless idle/low power functionality. */
// 翻译：/* 无时钟（Tickless）空闲/低功耗功能相关定义 */
// 解析：注释点明这段代码的作用——实现FreeRTOS的“Tickless Idle”低功耗模式，即系统空闲时暂停SysTick定时器，减少功耗。

#ifndef portSUPPRESS_TICKS_AND_SLEEP
    // 翻译：#ifndef portSUPPRESS_TICKS_AND_SLEEP（条件编译：如果未定义portSUPPRESS_TICKS_AND_SLEEP）
    // 解析：
    // - 这是“防止重复定义”的保护逻辑：如果用户或其他文件未提前定义portSUPPRESS_TICKS_AND_SLEEP，才执行下面的代码；
    // - 若用户想禁用低功耗功能，可在编译选项中定义portSUPPRESS_TICKS_AND_SLEEP，跳过这段定义。

    extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime );
    // 翻译：extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime );
    // 解析：
    // - 声明外部函数vPortSuppressTicksAndSleep（函数体不在当前文件，通常在低功耗相关的实现文件中）；
    // - 参数xExpectedIdleTime：类型为TickType_t（FreeRTOS的时间类型，通常是32位无符号整数），表示“系统预计的空闲时长（单位：Tick）”；
    // - 函数作用：系统进入空闲状态时，暂停SysTick定时器，配置低功耗模式（如CPU休眠），并在空闲时长结束或被中断唤醒后，更新系统时基。

    #define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime )    vPortSuppressTicksAndSleep( xExpectedIdleTime )
    // 翻译：#define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime )    vPortSuppressTicksAndSleep( xExpectedIdleTime )
    // 解析：
    // - 定义宏portSUPPRESS_TICKS_AND_SLEEP，使其等价于调用函数vPortSuppressTicksAndSleep；
    // - 目的：通过宏封装底层函数，统一低功耗功能的调用接口，方便上层代码（如FreeRTOS内核的空闲任务）调用，同时屏蔽不同硬件平台的实现差异。
#endif
/*-----------------------------------------------------------*/

/* Architecture specific optimisations. */
#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION    1
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1

/* Generic helper function. */
    __attribute__( ( always_inline ) ) static inline uint8_t ucPortCountLeadingZeros( uint32_t ulBitmap )
    {
        uint8_t ucReturn;

        __asm volatile ( "clz %0, %1" : "=r" ( ucReturn ) : "r" ( ulBitmap ) : "memory" );

        return ucReturn;
    }

/* Check the configuration. */
    #if ( configMAX_PRIORITIES > 32 )
        #error configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.  It is very rare that a system requires more than 10 to 15 difference priorities as tasks that share a priority will time slice.
    #endif

/* Store/clear the ready priorities in a bit map. */
    #define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities )    ( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) )
    #define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities )     ( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) )

/*-----------------------------------------------------------*/

    #define portGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities )    uxTopPriority = ( 31UL - ( uint32_t ) ucPortCountLeadingZeros( ( uxReadyPriorities ) ) )

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/*-----------------------------------------------------------*/

#ifdef configASSERT
    void vPortValidateInterruptPriority( void );
    #define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()    vPortValidateInterruptPriority()
#endif

/* portNOP() is not required by this port. */
#define portNOP()

#define portINLINE              __inline

#ifndef portFORCE_INLINE
    #define portFORCE_INLINE    inline __attribute__( ( always_inline ) )
#endif

/*-----------------------------------------------------------*/

portFORCE_INLINE static BaseType_t xPortIsInsideInterrupt( void )
{
    uint32_t ulCurrentInterrupt;
    BaseType_t xReturn;

    /* Obtain the number of the currently executing interrupt. */
    __asm volatile ( "mrs %0, ipsr" : "=r" ( ulCurrentInterrupt )::"memory" );

    if( ulCurrentInterrupt == 0 )
    {
        xReturn = pdFALSE;
    }
    else
    {
        xReturn = pdTRUE;
    }

    return xReturn;
}

/*-----------------------------------------------------------*/

// 函数定义：强制内联的静态无返回值函数
portFORCE_INLINE static void vPortRaiseBASEPRI( void )
// 翻译：使用 portFORCE_INLINE 强制内联的静态函数，无返回值，函数名 vPortRaiseBASEPRI，无参数
// 解析：
// - portFORCE_INLINE：确保函数在调用处直接展开，减少函数调用开销，适合中断控制等对效率敏感的操作
// - static：函数仅在当前文件可见
// - 功能：将 BASEPRI 寄存器设置为配置的优先级阈值，屏蔽低优先级中断（无返回值版本）
{
    // 声明变量：存储要设置的新 BASEPRI 值
    uint32_t ulNewBASEPRI;
    // 翻译：声明 uint32_t 类型变量 ulNewBASEPRI，用于临时存储新的 BASEPRI 值
    // 解析：作为汇编指令和 C 代码之间的数据传递桥梁

    // 核心内联汇编：设置 BASEPRI 寄存器实现中断屏蔽
    __asm volatile
    (
        // 将配置的优先级阈值赋值给 ulNewBASEPRI 对应的寄存器
        "   mov %0, %1                                              \n" \
        // 翻译：mov 指令，将 %1 对应的立即数（configMAX_SYSCALL_INTERRUPT_PRIORITY）传送到 %0 对应的寄存器（最终存入 ulNewBASEPRI）
        // 解析：
        // - %0：对应输出操作数 ulNewBASEPRI
        // - %1：对应输入参数 configMAX_SYSCALL_INTERRUPT_PRIORITY（中断优先级阈值）
        // - 作用：准备要写入 BASEPRI 寄存器的值

        // 将新值写入 BASEPRI 寄存器，实现中断屏蔽
        "   msr basepri, %0                                         \n" \
        // 翻译：msr 指令，将 %0 对应的寄存器值（新的优先级阈值）写入 basepri 特殊寄存器
        // 解析：
        // - BASEPRI 寄存器设置后，所有优先级数值大于该阈值的中断（优先级更低）将被屏蔽
        // - 优先级数值小于或等于该阈值的中断（优先级更高）仍可正常响应

        // 指令同步屏障：确保配置生效
        "   isb                                                     \n" \
        // 翻译：isb（Instruction Synchronization Barrier）指令，刷新指令流水线
        // 解析：保证 BASEPRI 的修改在后续指令执行前完全生效，防止指令乱序导致的屏蔽延迟

        // 数据同步屏障：确保内存操作完成
        "   dsb                                                     \n" \
        // 翻译：dsb（Data Synchronization Barrier）指令，等待所有内存操作完成
        // 解析：确保 BASEPRI 的更新已被硬件确认，避免低优先级中断在屏蔽生效前触发

        // 内联汇编约束
        : "=r" ( ulNewBASEPRI )  // 输出操作数：%0 映射到 ulNewBASEPRI，使用通用寄存器存储
        : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY )  // 输入操作数：%1 是立即数，即配置的优先级阈值
        : "memory"  // 告知编译器汇编操作可能影响内存，禁止相关优化
    );
}

/*-----------------------------------------------------------*/

// 1. 函数定义：强制内联的静态函数，返回uint32_t类型，无参数
portFORCE_INLINE static uint32_t ulPortRaiseBASEPRI( void )
// 翻译：portFORCE_INLINE（强制内联）修饰的静态函数，返回值是uint32_t类型，函数名ulPortRaiseBASEPRI，无输入参数
// 解析：
// - portFORCE_INLINE：本质是编译器指令（如__forceinline），强制函数在调用处展开，不生成独立函数代码，减少中断控制的执行开销；
// - static：函数仅在当前.c文件内可见，避免外部文件同名冲突；
// - 核心功能：通过修改BASEPRI寄存器屏蔽低优先级中断，同时保存原始中断状态以便后续恢复。
{
    // 2. 变量声明：定义两个32位变量，用于存储原始和新的BASEPRI值
    uint32_t ulOriginalBASEPRI, ulNewBASEPRI;
    // 翻译：声明两个uint32_t类型的变量，ulOriginalBASEPRI（存储修改前的BASEPRI值）、ulNewBASEPRI（存储要设置的新BASEPRI值）
    // 解析：
    // - ulOriginalBASEPRI：保存操作前的BASEPRI状态，后续需返回给调用者（用于恢复中断）；
    // - ulNewBASEPRI：临时存储要写入BASEPRI的新值（即configMAX_SYSCALL_INTERRUPT_PRIORITY）。

    // 3. 核心内联汇编块：操作BASEPRI寄存器，实现中断屏蔽
    __asm volatile
    (
        // 3.1 读取当前BASEPRI值到ulOriginalBASEPRI
        "   mrs %0, basepri                                         \n" \
        // 翻译：mrs（从特殊寄存器读取）指令，将basepri寄存器的值读取到%0对应的寄存器（最终存入ulOriginalBASEPRI）
        // 解析：
        // - mrs：Move from Special Register，专门用于读取CPU的特殊寄存器（如BASEPRI、PSP等）；
        // - %0：内联汇编的占位符，对应输出操作数列表中的第一个变量（ulOriginalBASEPRI）；
        // - 作用：保存当前的中断屏蔽状态，避免后续修改后无法恢复。

        // 3.2 将配置的优先级阈值赋值给ulNewBASEPRI
        "   mov %1, %2                                              \n" \
        // 翻译：mov（数据移动）指令，将%2对应的立即数（configMAX_SYSCALL_INTERRUPT_PRIORITY）赋值给%1对应的寄存器（最终存入ulNewBASEPRI）
        // 解析：
        // - %1：对应输出操作数列表中的第二个变量（ulNewBASEPRI）；
        // - %2：对应输入操作数列表中的第一个值（configMAX_SYSCALL_INTERRUPT_PRIORITY，编译期常量）；
        // - 作用：将“允许响应的最高中断优先级”阈值，临时存入ulNewBASEPRI对应的寄存器，为修改BASEPRI做准备。

        // 3.3 将新值写入BASEPRI，实现中断屏蔽
        "   msr basepri, %1                                         \n" \
        // 翻译：msr（写入特殊寄存器）指令，将%1对应的寄存器值（ulNewBASEPRI）写入basepri寄存器
        // 解析：
        // - msr：Move to Special Register，专门用于写入CPU的特殊寄存器；
        // - 核心逻辑：Cortex-M内核规定，BASEPRI值设为N后，所有优先级数值> N的中断（即优先级更低）会被屏蔽，数值≤N的中断（优先级更高）正常响应；
        // - 作用：屏蔽低优先级中断，保护临界区代码不被打断。

        // 3.4 指令同步屏障：确保BASEPRI修改生效
        "   isb                                                     \n" \
        // 翻译：isb（指令同步屏障）指令，刷新CPU指令流水线，确保后续指令基于最新的BASEPRI状态执行
        // 解析：
        // - 由于CPU可能存在“指令乱序执行”优化，isb强制让所有修改BASEPRI的指令执行完成后，再执行后续指令；
        // - 作用：避免因指令乱序导致“中断屏蔽未生效就执行临界区代码”的问题。

        // 3.5 数据同步屏障：确保内存操作完成
        "   dsb                                                     \n" \
        // 翻译：dsb（数据同步屏障）指令，确保所有内存相关操作（包括BASEPRI的修改）在后续指令执行前完成
        // 解析：
        // - dsb强制CPU等待所有未完成的内存操作（如寄存器写入、内存读写）结束；
        // - 作用：防止低优先级中断在BASEPRI修改未写入硬件前“抢跑”触发，确保中断屏蔽的可靠性。

        // 3.6 内联汇编约束：定义操作数映射关系和资源依赖
        : "=r" ( ulOriginalBASEPRI ), "=r" ( ulNewBASEPRI )  // 输出操作数：%0→ulOriginalBASEPRI，%1→ulNewBASEPRI，"=r"表示用通用寄存器存储
        : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY )       // 输入操作数：%2→configMAX_SYSCALL_INTERRUPT_PRIORITY，"i"表示是立即数（编译期确定）
        : "memory"                                           // 破坏描述符：告知编译器汇编会影响内存，需禁止相关优化（避免临界区数据被优化丢失）
    );

    // 4. 返回原始BASEPRI值：供调用者恢复中断状态
    /* This return will not be reached but is necessary to prevent compiler warnings. */
    // 翻译：/* 理论上汇编执行后不会走到这里，但为了避免编译器警告，必须保留return语句 */
    return ulOriginalBASEPRI;
    // 翻译：返回ulOriginalBASEPRI（修改前的BASEPRI值）
    // 解析：
    // - 虽然内联汇编已完成核心操作，但C语言语法要求有返回值（否则编译器报错）；
    // - 返回的ulOriginalBASEPRI是关键：调用者（如portCLEAR_INTERRUPT_MASK_FROM_ISR）会用它恢复BASEPRI，解除中断屏蔽。
}
/*-----------------------------------------------------------*/

// 强制内联的静态函数：设置 BASEPRI 寄存器，实现按优先级屏蔽中断
portFORCE_INLINE static void vPortSetBASEPRI( uint32_t ulNewMaskValue )
{
    // 内联汇编块：修改 BASEPRI 寄存器，输入参数为 ulNewMaskValue，禁止编译器优化内存访问
    __asm volatile
    (
        "   msr basepri, %0 "  // 核心指令：将输入参数 %0（即 ulNewMaskValue）写入 BASEPRI 寄存器
        :                       // 输出操作数列表：无（该指令无输出）
        : "r" ( ulNewMaskValue )// 输入操作数列表："r" 表示将 ulNewMaskValue 放入某个通用寄存器（如 r0~r12），%0 指代该寄存器
        : "memory"              // 破坏描述符："memory" 告知编译器，此汇编指令会修改内存状态，需禁止编译器对内存访问的优化（避免数据一致性问题）
    );
}
/*-----------------------------------------------------------*/

#define portMEMORY_BARRIER()    __asm volatile ( "" ::: "memory" )

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* PORTMACRO_H */
