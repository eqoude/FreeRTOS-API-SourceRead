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

/*-----------------------------------------------------------
* 为 ARM CM3（ARM Cortex-M3）移植版本实现 portable.h 中定义的函数。
*----------------------------------------------------------*/

/* 调度器相关头文件包含。 */
#include "FreeRTOS.h"  // FreeRTOS 核心头文件（定义通用类型、宏、API 声明）
#include "task.h"      // 任务管理头文件（定义 TCB、任务调度相关函数）

/* 所有中断服务程序（ISR）的函数原型定义。 */
// 定义 portISR_t 为“无参数、无返回值的函数指针类型”，用于统一表示 ISR 函数
typedef void ( * portISR_t )( void );

/* 操作内核（Cortex-M3 核心）所需的常量定义。首先是寄存器地址... */
// 系统滴答定时器（SysTick）控制寄存器：用于启用/禁用 SysTick、配置时钟源、中断使能
#define portNVIC_SYSTICK_CTRL_REG             ( *( ( volatile uint32_t * ) 0xe000e010 ) )
// 系统滴答定时器（SysTick）重载寄存器：设置 SysTick 定时器的计数上限（决定 Tick 周期）
#define portNVIC_SYSTICK_LOAD_REG             ( *( ( volatile uint32_t * ) 0xe000e014 ) )
// 系统滴答定时器（SysTick）当前值寄存器：读取当前计数，或写入任意值清零
#define portNVIC_SYSTICK_CURRENT_VALUE_REG    ( *( ( volatile uint32_t * ) 0xe000e018 ) )
// 系统异常优先级寄存器 2（SHPR2）：配置 PendSV 等异常的优先级（Cortex-M3 异常优先级由 SHPR1~SHPR3 控制）
#define portNVIC_SHPR2_REG                    ( *( ( volatile uint32_t * ) 0xe000ed1c ) )
// 系统异常优先级寄存器 3（SHPR3）：配置 SysTick 等异常的优先级
#define portNVIC_SHPR3_REG                    ( *( ( volatile uint32_t * ) 0xe000ed20 ) )

/* ...然后是寄存器中的关键位定义。 */
// SysTick 控制寄存器：时钟源选择位（1=使用处理器内核时钟，0=使用外部参考时钟）
#define portNVIC_SYSTICK_CLK_BIT              ( 1UL << 2UL )
// SysTick 控制寄存器：中断使能位（1=启用 SysTick 计数到0时触发中断，0=禁用中断）
#define portNVIC_SYSTICK_INT_BIT              ( 1UL << 1UL )
// SysTick 控制寄存器：定时器使能位（1=启动 SysTick 计数，0=停止计数）
#define portNVIC_SYSTICK_ENABLE_BIT           ( 1UL << 0UL )
// SysTick 控制寄存器：计数标志位（只读，1=表示已计数到0，0=未计数到0；读取后自动清零）
#define portNVIC_SYSTICK_COUNT_FLAG_BIT       ( 1UL << 16UL )
// 系统控制寄存器（SCB->ICSR）中的 PendSV 清除位（写入1清除 PendSV 挂起状态）
#define portNVIC_PENDSVCLEAR_BIT              ( 1UL << 27UL )
// 系统控制寄存器（SCB->ICSR）中的 SysTick 挂起置位（写入1强制挂起 SysTick 中断）
#define portNVIC_PEND_SYSTICK_SET_BIT         ( 1UL << 26UL )
// 系统控制寄存器（SCB->ICSR）中的 SysTick 挂起清除位（写入1清除 SysTick 挂起状态）
#define portNVIC_PEND_SYSTICK_CLEAR_BIT       ( 1UL << 25UL )

// 定义“最低中断优先级”（Cortex-M3 支持 0~255 级优先级，255 为最低）
#define portMIN_INTERRUPT_PRIORITY            ( 255UL )

// 配置 PendSV 异常优先级：将最低优先级左移 16 位，对应 SHPR2 寄存器中 PendSV 优先级的位域（第 16~23 位）
#define portNVIC_PENDSV_PRI                   ( ( ( uint32_t ) portMIN_INTERRUPT_PRIORITY ) << 16UL )

// 配置 SysTick 异常优先级：将最低优先级左移 24 位，对应 SHPR3 寄存器中 SysTick 优先级的位域（第 24~31 位）
#define portNVIC_SYSTICK_PRI                  ( ( ( uint32_t ) portMIN_INTERRUPT_PRIORITY ) << 24UL )

/* 用于检查 FreeRTOS 中断处理程序（ISR）安装是否正确的常量。 */
// SCB->VTOR 寄存器：向量表偏移寄存器（存储中断向量表的起始地址，用于检查向量表配置）
#define portSCB_VTOR_REG                      ( *( ( portISR_t ** ) 0xE000ED08 ) )
// SVC 异常的向量表索引（中断向量表中第 11 个位置对应 SVC 异常处理函数）
#define portVECTOR_INDEX_SVC                  ( 11 )
// PendSV 异常的向量表索引（中断向量表中第 14 个位置对应 PendSV 异常处理函数）
#define portVECTOR_INDEX_PENDSV               ( 14 )

/* 用于检查中断优先级有效性的常量。 */
// 第一个用户中断编号（Cortex-M3 内核异常编号为 0~15，16 及以后为外设中断）
#define portFIRST_USER_INTERRUPT_NUMBER       ( 16 )
// NVIC 中断优先级寄存器组偏移地址（从 0xE000E3F0 开始，每个外设中断的优先级由该地址开始的寄存器控制）
#define portNVIC_IP_REGISTERS_OFFSET_16       ( 0xE000E3F0 )
// SCB->AIRCR 寄存器：应用程序中断及复位控制寄存器（用于配置优先级分组、系统复位等）
#define portAIRCR_REG                         ( *( ( volatile uint32_t * ) 0xE000ED0C ) )
// 8 位无符号数的最大值（用于优先级数值范围检查）
#define portMAX_8_BIT_VALUE                   ( ( uint8_t ) 0xff )
// 字节的最高位（用于判断优先级是否为“用户可配置”，部分架构中最高位为“使能位”）
#define portTOP_BIT_OF_BYTE                   ( ( uint8_t ) 0x80 )
// 优先级分组的最大位数（Cortex-M3 优先级分组支持 0~7 位抢占优先级，对应 AIRCR 寄存器的 PRIGROUP 位域）
#define portMAX_PRIGROUP_BITS                 ( ( uint8_t ) 7 )
// 优先级分组掩码（用于提取 AIRCR 寄存器中的 PRIGROUP 位域，对应第 8~10 位）
#define portPRIORITY_GROUP_MASK               ( 0x07UL << 8UL )
// 优先级分组移位值（PRIGROUP 位域在 AIRCR 寄存器中的起始移位量）
#define portPRIORITY_GROUP_SHIFT              ( 8UL )

/* 屏蔽 ICSR 寄存器中除 VECTACTIVE 位域外的所有位（VECTACTIVE 位域表示当前活跃的异常/中断编号） */
#define portVECTACTIVE_MASK                   ( 0xFFUL )

/* 初始化任务栈所需的常量。 */
// 初始 XPSR 寄存器值（XPSR 是程序状态寄存器，0x01000000 表示Thumb状态（Cortex-M3 仅支持 Thumb 指令集））
#define portINITIAL_XPSR                      ( 0x01000000UL )

/* SysTick 定时器是 24 位计数器（最大值为 2^24 - 1 = 0xFFFFFF） */
#define portMAX_24_BIT_NUMBER                 ( 0xffffffUL )

/* 补偿因子：用于估算“Tickless 空闲模式下 SysTick 计数器停止期间可能丢失的计数”
 *（Tickless 模式会暂停 SysTick 以节省功耗，恢复时需补偿暂停期间的计数，避免时间偏差） */
#define portMISSED_COUNTS_FACTOR              ( 94UL )

/* 为严格符合 Cortex-M 架构规范：任务起始地址的第 0 位必须为 0
 *（因为 Cortex-M 架构中，PC 寄存器第 0 位用于标识指令集（0=Thumb，1=ARM，而 M3 仅支持 Thumb，故需清 0）） */
#define portSTART_ADDRESS_MASK                ( ( StackType_t ) 0xfffffffeUL )

/* 允许用户覆盖默认的 SysTick 时钟频率。
 * 若用户定义了 configSYSTICK_CLOCK_HZ，则该值需等于 SysTick 控制寄存器中 CLK 位为 0 时的时钟频率；
 * 若未定义，则默认使用内核时钟频率（configCPU_CLOCK_HZ）。 */
#ifndef configSYSTICK_CLOCK_HZ
    #define configSYSTICK_CLOCK_HZ             ( configCPU_CLOCK_HZ )
    /* 确保 SysTick 时钟频率与内核时钟频率一致（设置 CLK 位为 1） */
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG    ( portNVIC_SYSTICK_CLK_BIT )
#else
    /* 选择 SysTick 时钟频率与内核时钟频率不一致的配置（设置 CLK 位为 0） */
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG    ( 0 )
#endif

/* 允许用户覆盖“初始 LR 寄存器预加载的值（默认是 prvTaskExitError() 函数地址）”
 *（部分调试器中，默认预加载 LR 可能影响栈回溯，用户可通过 configTASK_RETURN_ADDRESS 自定义） */
#ifdef configTASK_RETURN_ADDRESS
    #define portTASK_RETURN_ADDRESS    configTASK_RETURN_ADDRESS
#else
    #define portTASK_RETURN_ADDRESS    prvTaskExitError
#endif

/*
 * Setup the timer to generate the tick interrupts.  The implementation in this
 * file is weak to allow application writers to change the timer used to
 * generate the tick interrupt.
 */
void vPortSetupTimerInterrupt( void );

/*
 * Exception handlers.
 */
void xPortPendSVHandler( void ) __attribute__( ( naked ) );
void xPortSysTickHandler( void );
void vPortSVCHandler( void ) __attribute__( ( naked ) );

/*
 * Start first task is a separate function so it can be tested in isolation.
 */
static void prvPortStartFirstTask( void ) __attribute__( ( naked ) );

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void prvTaskExitError( void );

/*-----------------------------------------------------------*/

/* Each task maintains its own interrupt status in the critical nesting
 * variable. */
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

/*
 * The number of SysTick increments that make up one tick period.
 */
#if ( configUSE_TICKLESS_IDLE == 1 )
    static uint32_t ulTimerCountsForOneTick = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * The maximum number of tick periods that can be suppressed is limited by the
 * 24 bit resolution of the SysTick timer.
 */
#if ( configUSE_TICKLESS_IDLE == 1 )
    static uint32_t xMaximumPossibleSuppressedTicks = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * Compensate for the CPU cycles that pass while the SysTick is stopped (low
 * power functionality only.
 */
#if ( configUSE_TICKLESS_IDLE == 1 )
    static uint32_t ulStoppedTimerCompensation = 0;
#endif /* configUSE_TICKLESS_IDLE */

/*
 * Used by the portASSERT_IF_INTERRUPT_PRIORITY_INVALID() macro to ensure
 * FreeRTOS API functions are not called from interrupts that have been assigned
 * a priority above configMAX_SYSCALL_INTERRUPT_PRIORITY.
 */
#if ( configASSERT_DEFINED == 1 )
    static uint8_t ucMaxSysCallPriority = 0;
    static uint32_t ulMaxPRIGROUPValue = 0;
    static const volatile uint8_t * const pcInterruptPriorityRegisters = ( const volatile uint8_t * const ) portNVIC_IP_REGISTERS_OFFSET_16;
#endif /* configASSERT_DEFINED */

/*-----------------------------------------------------------*/

/*
 * 详见头文件中的函数描述（注：头文件中通常会说明该函数用于初始化任务栈，为任务首次运行准备栈帧）。
 */
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
/*-----------------------------------------------------------*/

// 静态函数：任务异常退出时的错误处理（仅在任务非法退出时被调用，用户无法直接调用）
static void prvTaskExitError( void )
{
    volatile uint32_t ulDummy = 0UL;  // volatile 变量：防止编译器优化掉死循环，同时消除“未使用变量”警告

    /* 任务实现函数绝对不能主动退出或尝试返回给调用者——因为任务没有可返回的上级调用者。
     * 若任务需要退出，应调用 vTaskDelete( NULL )（删除自身）。
     *
     * 若定义了 configASSERT()（断言功能），则强制触发断言，提示错误；
     * 之后在此处进入死循环，方便应用开发者定位问题。 */
    configASSERT( uxCriticalNesting == ~0UL );  // 断言：检查临界区嵌套计数是否为最大值（~0UL 即 32 位全1）
    portDISABLE_INTERRUPTS();  // 禁用所有中断：防止死循环被中断打断，确保程序停在此处

    // 死循环：一旦进入此函数，程序将永久停留在此处（提示任务非法退出）
    while( ulDummy == 0 )
    {
        /* 说明：
         * 1. 在调度器启动后调用此函数，是为了消除编译器“函数已定义但未调用”的警告；
         * 2. ulDummy 变量仅用于消除“函数调用后代码不可达”的警告——
         *    将 ulDummy 定义为 volatile，编译器会认为此变量可能被外部修改（如中断），
         *    从而不会判定“死循环后代码不可达”，避免输出相关警告。 */
    }
}
/*-----------------------------------------------------------*/

void vPortSVCHandler( void )
{
    __asm volatile (  // 声明为volatile：防止编译器优化汇编代码，确保指令按顺序执行
        // 1. 读取 pxCurrentTCB 的地址（pxCurrentTCBConst2 是存储 pxCurrentTCB 地址的常量）
        "   ldr r3, pxCurrentTCBConst2      \n" /* 恢复上下文：加载 pxCurrentTCBConst2 地址到 r3 */
        // 2. 从 r3 指向的地址中，读取 pxCurrentTCB 的值（即当前任务的 TCB 指针）
        "   ldr r1, [r3]                    \n" /* 通过 pxCurrentTCBConst2 获取 pxCurrentTCB 的地址，存入 r1 */
        // 3. 从 TCB 指针（r1）指向的地址中，读取栈顶指针（TCB 第一个成员是 pxTopOfStack）
        "   ldr r0, [r1]                    \n" /* pxCurrentTCB 的第一个成员是任务栈顶指针，加载到 r0 */
        // 4. 从栈顶指针（r0）开始，批量弹出 R4~R11 寄存器的值（这些寄存器需软件手动恢复）
        "   ldmia r0!, {r4-r11}             \n" /* 弹出异常入口时未自动保存的寄存器（R4~R11）及临界区嵌套计数；r0! 表示弹出后 r0 自动递增（栈从低地址向高地址恢复） */
        // 5. 将恢复后的栈顶指针（r0）写入 PSP 寄存器（进程栈指针，Cortex-M3 任务使用 PSP 栈）
        "   msr psp, r0                     \n" /* 恢复任务栈指针：将 r0 赋值给 PSP */
        // 6. 指令同步屏障（ISB）：确保之前的指令（如 PSP 赋值）执行完毕后，再执行后续指令（Cortex-M 架构要求，避免指令乱序）
        "   isb                             \n"
        // 7. 将 r0 清零，用于后续清除中断屏蔽
        "   mov r0, #0                      \n"
        // 8. 将 r0 写入 BASEPRI 寄存器：清除中断屏蔽（BASEPRI=0 表示不屏蔽任何中断）
        "   msr basepri, r0                 \n" /* 允许所有中断：恢复中断响应能力 */
        // 9. 修改 LR 寄存器（链接寄存器）的低 4 位为 0xD：设置异常返回模式
        "   orr r14, #0xd                   \n" /* 配置 LR 寄存器，指定异常返回时使用 PSP 栈、恢复 xPSR 状态（0xD 对应 Cortex-M 异常返回模式：Return to Thread mode, using PSP） */
        // 10. 从异常返回：CPU 自动从 PSP 栈加载 xPSR、PC、LR、R12、R3~R0 寄存器，跳转到任务入口函数
        "   bx r14                          \n" /* 异常返回：触发 CPU 恢复上下文，进入任务执行 */
        // 11. 空行：代码格式对齐
        "                                   \n"
        // 12. 4字节对齐：确保后续数据（pxCurrentTCBConst2）按 4 字节对齐（Cortex-M 架构要求，避免未对齐访问错误）
        "   .align 4                        \n"
        // 13. 定义常量 pxCurrentTCBConst2：存储全局变量 pxCurrentTCB 的地址（供第1行 ldr 指令读取）
        "pxCurrentTCBConst2: .word pxCurrentTCB             \n"
        );
}
/*-----------------------------------------------------------*/

// 静态函数：启动系统中的第一个任务（仅在调度器启动时被调用，用户无法直接访问）
static void prvPortStartFirstTask( void )
{
    // 内联汇编块：volatile 确保指令不被编译器优化，严格按顺序执行
    __asm volatile (
        " ldr r0, =0xE000ED08   \n" /* 读取 NVIC 向量表偏移寄存器（VTOR）的地址，用于定位系统栈 */
        " ldr r0, [r0]          \n" /* 从 VTOR 寄存器中读取向量表的起始地址 */
        " ldr r0, [r0]          \n" /* 从向量表起始地址中读取初始主栈指针（MSP）的值 */
        " msr msp, r0           \n" /* 将 MSP（主栈指针）设置为读取到的初始栈地址 */
        " cpsie i               \n" /* 全局使能 IRQ 中断（允许普通中断响应） */
        " cpsie f               \n" /* 全局使能 Fault 异常（允许故障异常响应，如硬件错误） */
        " dsb                   \n" /* 数据同步屏障：确保之前的内存操作（如栈指针配置）全部完成 */
        " isb                   \n" /* 指令同步屏障：刷新指令流水线，确保后续指令基于最新配置执行 */
        " svc 0                 \n" /* 触发 SVC（系统调用）异常，通过异常处理启动第一个任务 */
        " nop                   \n" /* 空操作：防止编译器优化掉后续代码，确保指令完整性 */
        " .ltorg                \n" /* 声明文字池：存储汇编中使用的立即数（如 0xE000ED08），避免地址未对齐错误 */
        );
}
/*-----------------------------------------------------------*/

/*
 * 详见头文件中的函数描述（注：头文件通常说明：此函数用于启动 FreeRTOS 调度器，启动成功则永久运行（不返回），失败返回 pdFALSE）。
 */
BaseType_t xPortStartScheduler( void )
{
    /* 应用程序安装 FreeRTOS 中断处理函数（Handler）的两种方式：
     * 1. 直接路由：为 SVCall（系统调用）和 PendSV（挂起服务调用）中断，分别绑定 vPortSVCHandler 和 xPortPendSVHandler；
     * 2. 间接路由：先为 SVCall/PendSV 安装自定义 Handler，再在自定义 Handler 中跳转至 FreeRTOS 提供的 vPortSVCHandler/xPortPendSVHandler。
     * 规则：若用间接路由，需在 FreeRTOSConfig.h 设 configCHECK_HANDLER_INSTALLATION = 0；优先推荐直接路由（configCHECK_HANDLER_INSTALLATION=1 时会校验路由正确性）。 */
    #if ( configCHECK_HANDLER_INSTALLATION == 1 )
    {
        // 从 SCB 寄存器的 VTOR（向量表偏移寄存器）中，获取中断向量表的起始地址（portSCB_VTOR_REG 是 VTOR 寄存器的硬件地址，通常为 0xE000ED08）
        const portISR_t * const pxVectorTable = portSCB_VTOR_REG;

        /* 校验核心：确保 SVCall/PendSV 中断的 Handler 是 FreeRTOS 规定的函数（不校验 SysTick，因用户可能用其他定时器驱动 RTOS 时基）。
         * 断言失败原因：要么 Handler 安装错误，要么 VTOR 未指向正确的向量表（向量表地址配置错误）。
         * 调试参考：https://www.FreeRTOS.org/FAQHelp.html */
        configASSERT( pxVectorTable[ portVECTOR_INDEX_SVC ] == vPortSVCHandler );  // 校验 SVCall Handler 是否为 vPortSVCHandler
        configASSERT( pxVectorTable[ portVECTOR_INDEX_PENDSV ] == xPortPendSVHandler );  // 校验 PendSV Handler 是否为 xPortPendSVHandler
    }
    #endif /* configCHECK_HANDLER_INSTALLATION */

    /* 中断优先级配置与校验（仅在启用 configASSERT 时执行，用于调试阶段确保优先级配置合法） */
    #if ( configASSERT_DEFINED == 1 )
    {
        volatile uint8_t ucOriginalPriority;          // 保存“第一个用户中断优先级寄存器”的原始值（避免后续操作覆盖用户配置）
        volatile uint32_t ulImplementedPrioBits = 0;  // 记录当前硬件实际支持的中断优先级位数（Cortex-M 芯片优先级位数可能为 2/4/8 位，非固定）
        // 计算“第一个用户中断优先级寄存器”的地址：portNVIC_IP_REGISTERS_OFFSET_16 是 NVIC 优先级寄存器基地址，portFIRST_USER_INTERRUPT_NUMBER 是第一个用户中断的编号（如 16，内核中断编号 0-15）
        volatile uint8_t * const pucFirstUserPriorityRegister = ( volatile uint8_t * const ) ( portNVIC_IP_REGISTERS_OFFSET_16 + portFIRST_USER_INTERRUPT_NUMBER );
        volatile uint8_t ucMaxPriorityValue;          // 硬件支持的“最大优先级值”（用于判断优先级位数）

        /* 背景：FreeRTOS 区分“ISR 安全 API”（以 FromISR 结尾）和普通 API，前者可在中断中调用，但要求中断优先级不超过 configMAX_SYSCALL_INTERRUPT_PRIORITY。
         * 步骤1：先保存第一个用户中断优先级寄存器的原始值，避免后续写入操作破坏原有配置。 */
        ucOriginalPriority = *pucFirstUserPriorityRegister;

        /* 步骤2：探测硬件实际支持的优先级位数（核心逻辑：通过“写最大值→读回”判断有效位）：
         * - 向优先级寄存器写入 8 位最大值（portMAX_8_BIT_VALUE=0xFF）；
         * - 读回寄存器值：未实现的优先级位会自动清 0，实现的位会保留 1，因此读回的值可反映有效位数（如读回 0xF0 表示高 4 位有效，支持 4 位优先级）。 */
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;

        /* 步骤3：校准“系统调用最大优先级”（configMAX_SYSCALL_INTERRUPT_PRIORITY）：
         * - 用硬件支持的最大优先级值（ucMaxPriorityValue）做掩码，确保配置的优先级不超过硬件能力（如硬件仅支持 4 位优先级，掩码后低 4 位会清 0）；
         * - 校准后的值存入 ucMaxSysCallPriority，后续临界区操作会用此值屏蔽中断。 */
        ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

        /* 断言1：校准后的系统调用最大优先级不能为 0：
         * - 原因：Cortex-M 的 BASEPRI 寄存器（用于屏蔽中断）值为 0 时，表示“不屏蔽任何中断”；
         * - 若 ucMaxSysCallPriority=0，临界区操作会失效（无法屏蔽中断），导致调度器错误。 */
        configASSERT( ucMaxSysCallPriority );

        /* 断言2：configMAX_SYSCALL_INTERRUPT_PRIORITY 中“硬件未实现的位”必须为 0：
         * - 例如硬件仅支持 4 位优先级（高 4 位有效），则配置的优先级低 4 位必须为 0，否则配置无效（硬件会忽略未实现的位）。 */
        configASSERT( ( configMAX_SYSCALL_INTERRUPT_PRIORITY & ( ~ucMaxPriorityValue ) ) == 0U );

        /* 步骤4：计算“最大优先级组（PRIGROUP）值”：
         * - PRIGROUP 是 Cortex-M 内核 AIRCR 寄存器的参数，用于划分“抢占优先级”和“子优先级”的位数（如 PRIGROUP=3 表示“抢占优先级 4 位，子优先级 0 位”）；
         * - 通过循环左移 ucMaxPriorityValue，统计最高位 1 的个数，确定硬件实现的优先级位数（ulImplementedPrioBits）。 */
        while( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )  // portTOP_BIT_OF_BYTE=0x80，判断最高位是否为 1
        {
            ulImplementedPrioBits++;                  // 每左移一次，有效位数加 1
            ucMaxPriorityValue <<= ( uint8_t ) 0x01;  // 左移 1 位，检查下一位
        }

        /* 特殊情况处理：硬件支持 8 位优先级（ulImplementedPrioBits=8）：
         * - Cortex-M 中，8 位优先级时 PRIGROUP 无法禁用子优先级，最低 1 位始终是子优先级（即 128 个抢占优先级，2 个子优先级）；
         * - 为避免混淆（如配置优先级 5 会屏蔽优先级 4 和 5，因二者抢占优先级相同），要求 configMAX_SYSCALL_INTERRUPT_PRIORITY 的最低位必须为 0。 */
        if( ulImplementedPrioBits == 8 )
        {
            configASSERT( ( configMAX_SYSCALL_INTERRUPT_PRIORITY & 0x1U ) == 0U );  // 校验最低位是否为 0
            ulMaxPRIGROUPValue = 0;  // 8 位优先级时，PRIGROUP 固定为 0
        }
        else
        {
            // 非 8 位优先级时：PRIGROUP 值 = 最大 PRIGROUP 位数（portMAX_PRIGROUP_BITS 通常为 7） - 硬件实现的优先级位数
            ulMaxPRIGROUPValue = portMAX_PRIGROUP_BITS - ulImplementedPrioBits;
        }

        /* 步骤5：校准 PRIGROUP 值的格式：
         * - PRIGROUP 在 AIRCR 寄存器中占 3 位，需左移 portPRIGROUP_SHIFT 位（通常为 8 位）到正确位置；
         * - 用 portPRIORITY_GROUP_MASK 掩码确保仅修改 PRIGROUP 相关位，不影响其他位。 */
        ulMaxPRIGROUPValue <<= portPRIGROUP_SHIFT;
        ulMaxPRIGROUPValue &= portPRIORITY_GROUP_MASK;

        /* 步骤6：恢复“第一个用户中断优先级寄存器”的原始值（还原用户配置，避免影响其他中断） */
        *pucFirstUserPriorityRegister = ucOriginalPriority;
    }
    #endif /* configASSERT_DEFINED */

    /* 配置核心中断的优先级（调度器依赖的关键中断）：
     * - PendSV：用于任务上下文切换，需设为最低优先级（确保任何中断都能打断它，避免切换延迟）；
     * - SysTick：用于产生 RTOS 时基（Tick 中断，如 1ms 一次），需设为低优先级（避免抢占关键任务）；
     * - SVCall：用于启动第一个任务，需设为最高优先级（确保启动过程不被其他中断打断）；
     * 寄存器说明：portNVIC_SHPR2_REG（SVCall 优先级寄存器，地址 0xE000ED1C）、portNVIC_SHPR3_REG（PendSV/SysTick 优先级寄存器，地址 0xE000ED20）；
     * 优先级值说明：portNVIC_PENDSV_PRI/portNVIC_SYSTICK_PRI 通常为 0xFF（最低优先级），portNVIC_SHPR2_REG 写 0 表示最高优先级。 */
    portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
    portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
    portNVIC_SHPR2_REG = 0;

    /* 启动 RTOS 时基定时器（产生 Tick 中断）：
     * - 此时中断已禁用（调度器启动前默认关中断），无需担心定时器启动时触发中断；
     * - 函数实现：vPortSetupTimerInterrupt 是 FreeRTOS 弱函数，默认配置 SysTick 定时器（如设置 LOAD 寄存器为“系统时钟/1000-1”，实现 1ms 中断，再使能 SysTick 中断和计数器）。 */
    vPortSetupTimerInterrupt();

    /* 初始化临界区嵌套计数：
     * - uxCriticalNesting 是全局变量，记录当前临界区嵌套层数（0 表示未进入临界区）；
     * - 第一个任务启动前，需确保临界区计数为 0，避免任务启动后临界区状态异常。 */
    uxCriticalNesting = 0;

    /* 启动第一个任务：
     * - 调用 prvPortStartFirstTask 函数，该函数会通过“读取 NVIC 向量表→初始化 MSP 栈指针→使能中断→触发 SVC 异常”的流程，最终进入 vPortSVCHandler 恢复第一个任务的上下文；
     * - 关键特性：若启动成功，此函数不会返回（CPU 会跳转到第一个任务的入口函数执行）；若返回，说明启动失败。 */
    prvPortStartFirstTask();

    /* 理论上不会执行到此处（因第一个任务启动后会持续运行）：
     * - 以下代码仅为避免编译器警告（如“prvTaskExitError 未被调用”“vTaskSwitchContext 未被引用”）；
     * - vTaskSwitchContext：任务切换核心函数，调用它可确保编译器不优化掉该函数符号；
     * - prvTaskExitError：任务非法退出时的错误处理函数，调用它可避免“静态函数未使用”的警告。 */
    vTaskSwitchContext();
    prvTaskExitError();

    /* 永远不会执行到此处（仅为函数返回类型匹配，避免编译器报错） */
    return 0;
}
/*-----------------------------------------------------------*/

// 退出临界区：恢复中断使能状态（需与 vPortEnterCritical 成对调用）
void vPortExitCritical( void )
{
    // 断言校验：确保当前处于临界区中（uxCriticalNesting > 0）
    // 若断言失败，说明“退出临界区的次数超过进入次数”（如未进入临界区就调用退出，或多调用了退出函数）
    configASSERT( uxCriticalNesting );

    // 临界区嵌套计数递减 1
    // 含义：每调用一次退出函数，嵌套层数减 1（对应之前一次进入临界区的操作）
    uxCriticalNesting--;

    // 判断：仅当嵌套计数减到 0 时（所有嵌套的临界区都已退出），才重新使能中断
    if( uxCriticalNesting == 0 )
    {
        // 使能所有可屏蔽中断（portENABLE_INTERRUPTS 是宏，本质是操作 CPU 状态寄存器）
        portENABLE_INTERRUPTS();
    }
}
/*-----------------------------------------------------------*/

// PendSV 异常处理函数：实现任务上下文切换（naked 函数，无栈帧处理）
void xPortPendSVHandler( void )
{
    /* 这是一个 naked 函数（无自动栈帧创建/销毁，直接执行汇编指令）。 */

    __asm volatile
    (
        // 步骤1：读取当前任务的栈指针（PSP，进程栈指针）到 r0
        "   mrs r0, psp                         \n"  // MRS（Move from Special Register）：将 PSP 寄存器的值读到 r0
                                                    // 注：Cortex-M 中，任务运行在用户模式，使用 PSP 作为栈指针；内核用 MSP
        
        // 步骤2：指令同步屏障（确保 PSP 读取完成，避免指令乱序）
        "   isb                                 \n"  // ISB（Instruction Synchronization Barrier）：刷新指令流水线，确保后续指令基于最新状态执行

        // 步骤3：获取当前任务控制块（TCB）的地址
        "   ldr r3, pxCurrentTCBConst           \n"  // 从文字池读取 pxCurrentTCB 的地址到 r3（pxCurrentTCB 是全局变量，存储当前任务 TCB 指针）
        "   ldr r2, [r3]                        \n"  // 从 r3 指向的地址（pxCurrentTCB）中，读取当前 TCB 的指针到 r2（r2 = *pxCurrentTCB）

        // 步骤4：保存当前任务的寄存器（r4-r11）到任务栈
        "   stmdb r0!, {r4-r11}                 \n"  // STMDB（Store Multiple Data, Decrement Before）：
                                                    // - 从 r4 到 r11 的寄存器值依次存入内存，栈指针 r0 先递减（适应满减栈）
                                                    // - 这些寄存器是编译器非自动保存的，必须手动保存以完整恢复上下文
        "   str r0, [r2]                        \n"  // 将更新后的栈指针（r0）存入当前 TCB 的第一个成员（TCB->pxTopOfStack），记录栈顶位置

        // 步骤5：调用 vTaskSwitchContext 切换任务（需先屏蔽低优先级中断）
        "   stmdb sp!, {r3, r14}                \n"  // 保存 r3（pxCurrentTCB 地址）和 r14（返回地址）到 MSP 栈（内核栈），避免被后续调用破坏
        "   mov r0, %0                          \n"  // 将输入参数（configMAX_SYSCALL_INTERRUPT_PRIORITY）移入 r0
        "   msr basepri, r0                     \n"  // 配置 BASEPRI 寄存器，屏蔽优先级低于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断，确保任务切换不被干扰
        "   bl vTaskSwitchContext               \n"  // 调用 C 函数 vTaskSwitchContext：更新 pxCurrentTCB 为下一个就绪任务的 TCB 指针
        "   mov r0, #0                          \n"  // 准备将 BASEPRI 设为 0（解除中断屏蔽）
        "   msr basepri, r0                     \n"  // 写入 BASEPRI，恢复中断响应
        "   ldmia sp!, {r3, r14}                \n"  // 从 MSP 栈恢复 r3 和 r14（恢复调用前的寄存器状态）

        // 步骤6：恢复新任务的上下文（从新任务的 TCB 中加载栈信息）
        "   ldr r1, [r3]                        \n"  // 从 r3（pxCurrentTCB 地址）读取新任务的 TCB 指针到 r1（r1 = *pxCurrentTCB，已被 vTaskSwitchContext 更新）
        "   ldr r0, [r1]                        \n"  // 从新 TCB 的第一个成员读取栈顶指针到 r0（r0 = 新任务的 pxTopOfStack）
        "   ldmia r0!, {r4-r11}                 \n"  // LDMIA（Load Multiple Data, Increment After）：
                                                    // - 从栈中依次恢复 r4 到 r11 的值，栈指针 r0 后递增
        "   msr psp, r0                         \n"  // 将更新后的栈指针（r0）写入 PSP 寄存器，新任务将使用此栈指针
        "   isb                                 \n"  // 指令同步屏障：确保 PSP 更新生效，避免后续指令使用旧栈指针
        "   bx r14                              \n"  // 分支跳转：r14 中是异常返回地址，执行后退出 PendSV 异常，进入新任务执行

        // 数据定义：文字池（存储 pxCurrentTCB 的地址，供 ldr 指令访问）
        "   .align 4                            \n"  // 按 4 字节对齐，确保地址正确
        "pxCurrentTCBConst: .word pxCurrentTCB  \n"  // 定义一个字（32 位），值为 pxCurrentTCB 全局变量的地址

        // 内联汇编参数：输入参数为 configMAX_SYSCALL_INTERRUPT_PRIORITY（用于屏蔽中断）
        ::"i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY )
    );
}
/*-----------------------------------------------------------*/

// SysTick 中断服务函数：处理 RTOS 时基更新与任务切换触发
void xPortSysTickHandler( void )
{
    /* SysTick 运行在最低中断优先级，因此当该中断执行时，所有中断都已解除屏蔽。
     * 因此无需保存和恢复中断屏蔽值，因为其状态是已知的（已解除屏蔽）。 */
    portDISABLE_INTERRUPTS();  // 禁用所有可屏蔽中断，确保中断处理过程不被打断（临界区保护）
    traceISR_ENTER();          // 调试跟踪宏：标记进入中断服务函数（可用于可视化工具，如 FreeRTOS+Trace）
    {
        /* 递增 RTOS 时基计数器（Tick），并处理任务延时到期。 */
        // xTaskIncrementTick()：核心函数，返回 pdTRUE 表示需要触发任务切换；pdFALSE 表示无需切换
        if( xTaskIncrementTick() != pdFALSE )
        {
            traceISR_EXIT_TO_SCHEDULER();  // 调试跟踪宏：标记中断退出后将触发调度器

            /* 需要执行上下文切换。上下文切换在 PendSV 中断中执行，因此挂起 PendSV 中断。 */
            // portNVIC_INT_CTRL_REG：内核中断控制寄存器（地址 0xE000ED04）
            // portNVIC_PENDSVSET_BIT：PendSV 中断挂起位（置 1 表示“请求执行 PendSV 中断”）
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
        else
        {
            traceISR_EXIT();  // 调试跟踪宏：标记中断正常退出（无需触发调度器）
        }
    }
    portENABLE_INTERRUPTS();   // 使能所有可屏蔽中断，退出临界区
}
/*-----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE == 1 )

    __attribute__( ( weak ) ) void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
    {
        uint32_t ulReloadValue, ulCompleteTickPeriods, ulCompletedSysTickDecrements, ulSysTickDecrementsLeft;
        TickType_t xModifiableIdleTime;

        /* Make sure the SysTick reload value does not overflow the counter. */
        if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
        {
            xExpectedIdleTime = xMaximumPossibleSuppressedTicks;
        }

        /* Enter a critical section but don't use the taskENTER_CRITICAL()
         * method as that will mask interrupts that should exit sleep mode. */
        __asm volatile ( "cpsid i" ::: "memory" );
        __asm volatile ( "dsb" );
        __asm volatile ( "isb" );

        /* If a context switch is pending or a task is waiting for the scheduler
         * to be unsuspended then abandon the low power entry. */
        if( eTaskConfirmSleepModeStatus() == eAbortSleep )
        {
            /* Re-enable interrupts - see comments above the cpsid instruction
             * above. */
            __asm volatile ( "cpsie i" ::: "memory" );
        }
        else
        {
            /* Stop the SysTick momentarily.  The time the SysTick is stopped for
             * is accounted for as best it can be, but using the tickless mode will
             * inevitably result in some tiny drift of the time maintained by the
             * kernel with respect to calendar time. */
            portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT );

            /* Use the SysTick current-value register to determine the number of
             * SysTick decrements remaining until the next tick interrupt.  If the
             * current-value register is zero, then there are actually
             * ulTimerCountsForOneTick decrements remaining, not zero, because the
             * SysTick requests the interrupt when decrementing from 1 to 0. */
            ulSysTickDecrementsLeft = portNVIC_SYSTICK_CURRENT_VALUE_REG;

            if( ulSysTickDecrementsLeft == 0 )
            {
                ulSysTickDecrementsLeft = ulTimerCountsForOneTick;
            }

            /* Calculate the reload value required to wait xExpectedIdleTime
             * tick periods.  -1 is used because this code normally executes part
             * way through the first tick period.  But if the SysTick IRQ is now
             * pending, then clear the IRQ, suppressing the first tick, and correct
             * the reload value to reflect that the second tick period is already
             * underway.  The expected idle time is always at least two ticks. */
            ulReloadValue = ulSysTickDecrementsLeft + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );

            if( ( portNVIC_INT_CTRL_REG & portNVIC_PEND_SYSTICK_SET_BIT ) != 0 )
            {
                portNVIC_INT_CTRL_REG = portNVIC_PEND_SYSTICK_CLEAR_BIT;
                ulReloadValue -= ulTimerCountsForOneTick;
            }

            if( ulReloadValue > ulStoppedTimerCompensation )
            {
                ulReloadValue -= ulStoppedTimerCompensation;
            }

            /* Set the new reload value. */
            portNVIC_SYSTICK_LOAD_REG = ulReloadValue;

            /* Clear the SysTick count flag and set the count value back to
             * zero. */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

            /* Restart SysTick. */
            portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;

            /* Sleep until something happens.  configPRE_SLEEP_PROCESSING() can
             * set its parameter to 0 to indicate that its implementation contains
             * its own wait for interrupt or wait for event instruction, and so wfi
             * should not be executed again.  However, the original expected idle
             * time variable must remain unmodified, so a copy is taken. */
            xModifiableIdleTime = xExpectedIdleTime;
            configPRE_SLEEP_PROCESSING( xModifiableIdleTime );

            if( xModifiableIdleTime > 0 )
            {
                __asm volatile ( "dsb" ::: "memory" );
                __asm volatile ( "wfi" );
                __asm volatile ( "isb" );
            }

            configPOST_SLEEP_PROCESSING( xExpectedIdleTime );

            /* Re-enable interrupts to allow the interrupt that brought the MCU
             * out of sleep mode to execute immediately.  See comments above
             * the cpsid instruction above. */
            __asm volatile ( "cpsie i" ::: "memory" );
            __asm volatile ( "dsb" );
            __asm volatile ( "isb" );

            /* Disable interrupts again because the clock is about to be stopped
             * and interrupts that execute while the clock is stopped will increase
             * any slippage between the time maintained by the RTOS and calendar
             * time. */
            __asm volatile ( "cpsid i" ::: "memory" );
            __asm volatile ( "dsb" );
            __asm volatile ( "isb" );

            /* Disable the SysTick clock without reading the
             * portNVIC_SYSTICK_CTRL_REG register to ensure the
             * portNVIC_SYSTICK_COUNT_FLAG_BIT is not cleared if it is set.  Again,
             * the time the SysTick is stopped for is accounted for as best it can
             * be, but using the tickless mode will inevitably result in some tiny
             * drift of the time maintained by the kernel with respect to calendar
             * time*/
            portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT );

            /* Determine whether the SysTick has already counted to zero. */
            if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
            {
                uint32_t ulCalculatedLoadValue;

                /* The tick interrupt ended the sleep (or is now pending), and
                 * a new tick period has started.  Reset portNVIC_SYSTICK_LOAD_REG
                 * with whatever remains of the new tick period. */
                ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL ) - ( ulReloadValue - portNVIC_SYSTICK_CURRENT_VALUE_REG );

                /* Don't allow a tiny value, or values that have somehow
                 * underflowed because the post sleep hook did something
                 * that took too long or because the SysTick current-value register
                 * is zero. */
                if( ( ulCalculatedLoadValue <= ulStoppedTimerCompensation ) || ( ulCalculatedLoadValue > ulTimerCountsForOneTick ) )
                {
                    ulCalculatedLoadValue = ( ulTimerCountsForOneTick - 1UL );
                }

                portNVIC_SYSTICK_LOAD_REG = ulCalculatedLoadValue;

                /* As the pending tick will be processed as soon as this
                 * function exits, the tick value maintained by the tick is stepped
                 * forward by one less than the time spent waiting. */
                ulCompleteTickPeriods = xExpectedIdleTime - 1UL;
            }
            else
            {
                /* Something other than the tick interrupt ended the sleep. */

                /* Use the SysTick current-value register to determine the
                 * number of SysTick decrements remaining until the expected idle
                 * time would have ended. */
                ulSysTickDecrementsLeft = portNVIC_SYSTICK_CURRENT_VALUE_REG;
                #if ( portNVIC_SYSTICK_CLK_BIT_CONFIG != portNVIC_SYSTICK_CLK_BIT )
                {
                    /* If the SysTick is not using the core clock, the current-
                     * value register might still be zero here.  In that case, the
                     * SysTick didn't load from the reload register, and there are
                     * ulReloadValue decrements remaining in the expected idle
                     * time, not zero. */
                    if( ulSysTickDecrementsLeft == 0 )
                    {
                        ulSysTickDecrementsLeft = ulReloadValue;
                    }
                }
                #endif /* portNVIC_SYSTICK_CLK_BIT_CONFIG */

                /* Work out how long the sleep lasted rounded to complete tick
                 * periods (not the ulReload value which accounted for part
                 * ticks). */
                ulCompletedSysTickDecrements = ( xExpectedIdleTime * ulTimerCountsForOneTick ) - ulSysTickDecrementsLeft;

                /* How many complete tick periods passed while the processor
                 * was waiting? */
                ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;

                /* The reload value is set to whatever fraction of a single tick
                 * period remains. */
                portNVIC_SYSTICK_LOAD_REG = ( ( ulCompleteTickPeriods + 1UL ) * ulTimerCountsForOneTick ) - ulCompletedSysTickDecrements;
            }

            /* Restart SysTick so it runs from portNVIC_SYSTICK_LOAD_REG again,
             * then set portNVIC_SYSTICK_LOAD_REG back to its standard value.  If
             * the SysTick is not using the core clock, temporarily configure it to
             * use the core clock.  This configuration forces the SysTick to load
             * from portNVIC_SYSTICK_LOAD_REG immediately instead of at the next
             * cycle of the other clock.  Then portNVIC_SYSTICK_LOAD_REG is ready
             * to receive the standard value immediately. */
            portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
            portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
            #if ( portNVIC_SYSTICK_CLK_BIT_CONFIG == portNVIC_SYSTICK_CLK_BIT )
            {
                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
            }
            #else
            {
                /* The temporary usage of the core clock has served its purpose,
                 * as described above.  Resume usage of the other clock. */
                portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT;

                if( ( portNVIC_SYSTICK_CTRL_REG & portNVIC_SYSTICK_COUNT_FLAG_BIT ) != 0 )
                {
                    /* The partial tick period already ended.  Be sure the SysTick
                     * counts it only once. */
                    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0;
                }

                portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
                portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
            }
            #endif /* portNVIC_SYSTICK_CLK_BIT_CONFIG */

            /* Step the tick to account for any tick periods that elapsed. */
            vTaskStepTick( ulCompleteTickPeriods );

            /* Exit with interrupts enabled. */
            __asm volatile ( "cpsie i" ::: "memory" );
        }
    }

#endif /* configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

/*
 * 配置 SysTick 定时器，使其按所需频率产生时基中断（RTOS 任务调度依赖此中断）。
 */
__attribute__( ( weak ) ) void vPortSetupTimerInterrupt( void )
{
    /* 计算配置时基中断所需的常量（仅在使能“无滴答休眠”功能时执行）。 */
    #if ( configUSE_TICKLESS_IDLE == 1 )  // configUSE_TICKLESS_IDLE：FreeRTOS 低功耗模式开关（1=使能，0=禁用）
    {
        // 1. 计算“产生一次 Tick 中断所需的 SysTick 计数次数”
        // configSYSTICK_CLOCK_HZ：SysTick 定时器的时钟频率（通常等于 CPU 内核频率，如 72MHz）
        // configTICK_RATE_HZ：RTOS 时基频率（如 1000Hz，即 1ms 一次 Tick）
        ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );

        // 2. 计算“低功耗模式下最大可关闭 Tick 的次数”
        // portMAX_24_BIT_NUMBER：SysTick 是 24 位定时器，最大计数值为 0xFFFFFF（即 16777215）
        // 含义：低功耗休眠时，最多可连续跳过 xMaximumPossibleSuppressedTicks 次 Tick，避免定时器溢出
        xMaximumPossibleSuppressedTicks = portMAX_24_BIT_NUMBER / ulTimerCountsForOneTick;

        // 3. 计算“定时器停止时的补偿计数”
        // portMISSED_COUNTS_FACTOR：补偿系数（通常为 2，用于修正低功耗唤醒时的计数偏差）
        // 含义：低功耗模式下定时器停止，唤醒后需补偿因停止导致的计数误差，确保 Tick 周期准确
        ulStoppedTimerCompensation = portMISSED_COUNTS_FACTOR / ( configCPU_CLOCK_HZ / configSYSTICK_CLOCK_HZ );
    }
    #endif /* configUSE_TICKLESS_IDLE */

    /* 停止 SysTick 定时器并清除当前计数值（避免配置前定时器误触发中断）。 */
    // portNVIC_SYSTICK_CTRL_REG：SysTick 控制寄存器地址（Cortex-M 内核固定地址：0xE000E010）
    // 写 0UL：禁用 SysTick 定时器、禁用 SysTick 中断、清除所有控制位
    portNVIC_SYSTICK_CTRL_REG = 0UL;
    // portNVIC_SYSTICK_CURRENT_VALUE_REG：SysTick 当前值寄存器地址（固定地址：0xE000E018）
    // 写 0UL：清除当前计数值（无论当前计数多少，直接归 0）
    portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;

    /* 配置 SysTick 定时器，使其按需求频率产生中断（核心配置步骤）。 */
    // 1. 配置 SysTick 装载寄存器（设置定时器“重装载值”，决定中断周期）
    // portNVIC_SYSTICK_LOAD_REG：SysTick 装载寄存器地址（固定地址：0xE000E014）
    // 公式含义：(时钟频率 / 时基频率) - 1 → 因 SysTick 是“递减计数”，从装载值减到 0 时触发中断
    // 示例：若 SysTick 时钟 72MHz，时基频率 1000Hz → 装载值 = (72e6 / 1000) - 1 = 71999 → 每 72000 个时钟周期（1ms）触发一次中断
    portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;

    // 2. 配置 SysTick 控制寄存器，启动定时器并使能中断
    // 各参数含义：
    // - portNVIC_SYSTICK_CLK_BIT_CONFIG：SysTick 时钟源选择（通常为 1，选择“内核时钟”；0 选择“外部参考时钟”）
    // - portNVIC_SYSTICK_INT_BIT：SysTick 中断使能位（1=使能，计数到 0 时触发 SysTick 中断）
    // - portNVIC_SYSTICK_ENABLE_BIT：SysTick 定时器使能位（1=启动定时器，0=停止）
    // 组合后：选择内核时钟、使能中断、启动定时器 → 定时器开始递减计数，到 0 触发中断，然后自动重装继续计数
    portNVIC_SYSTICK_CTRL_REG = ( portNVIC_SYSTICK_CLK_BIT_CONFIG | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT );
}
/*-----------------------------------------------------------*/

#if ( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
        uint32_t ulCurrentInterrupt;
        uint8_t ucCurrentPriority;

        /* Obtain the number of the currently executing interrupt. */
        __asm volatile ( "mrs %0, ipsr" : "=r" ( ulCurrentInterrupt )::"memory" );

        /* Is the interrupt number a user defined interrupt? */
        if( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )
        {
            /* Look up the interrupt's priority. */
            ucCurrentPriority = pcInterruptPriorityRegisters[ ulCurrentInterrupt ];

            /* The following assertion will fail if a service routine (ISR) for
             * an interrupt that has been assigned a priority above
             * configMAX_SYSCALL_INTERRUPT_PRIORITY calls an ISR safe FreeRTOS API
             * function.  ISR safe FreeRTOS API functions must *only* be called
             * from interrupts that have been assigned a priority at or below
             * configMAX_SYSCALL_INTERRUPT_PRIORITY.
             *
             * Numerically low interrupt priority numbers represent logically high
             * interrupt priorities, therefore the priority of the interrupt must
             * be set to a value equal to or numerically *higher* than
             * configMAX_SYSCALL_INTERRUPT_PRIORITY.
             *
             * Interrupts that  use the FreeRTOS API must not be left at their
             * default priority of  zero as that is the highest possible priority,
             * which is guaranteed to be above configMAX_SYSCALL_INTERRUPT_PRIORITY,
             * and  therefore also guaranteed to be invalid.
             *
             * FreeRTOS maintains separate thread and ISR API functions to ensure
             * interrupt entry is as fast and simple as possible.
             *
             * The following links provide detailed information:
             * https://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
             * https://www.FreeRTOS.org/FAQHelp.html */
            configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );
        }

        /* Priority grouping:  The interrupt controller (NVIC) allows the bits
         * that define each interrupt's priority to be split between bits that
         * define the interrupt's pre-emption priority bits and bits that define
         * the interrupt's sub-priority.  For simplicity all bits must be defined
         * to be pre-emption priority bits.  The following assertion will fail if
         * this is not the case (if some bits represent a sub-priority).
         *
         * If the application only uses CMSIS libraries for interrupt
         * configuration then the correct setting can be achieved on all Cortex-M
         * devices by calling NVIC_SetPriorityGrouping( 0 ); before starting the
         * scheduler.  Note however that some vendor specific peripheral libraries
         * assume a non-zero priority group setting, in which cases using a value
         * of zero will result in unpredictable behaviour. */
        configASSERT( ( portAIRCR_REG & portPRIORITY_GROUP_MASK ) <= ulMaxPRIGROUPValue );
    }

#endif /* configASSERT_DEFINED */
