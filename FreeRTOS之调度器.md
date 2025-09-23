# FreeRTOS之调度器

一般情况下我们的程序是这样的：

```c
int main(vodi){
    //硬件初始化
    //任务初始化
    //定时器初始化
    //信号量初始化

    vTaskStartScheduler();//开启调度 

    while(1);//正常情况下不会运行到这里
}
```

那选择我们就从*vTaskStartScheduler()* 开始看看系统怎么运行的。

## **GCC 内联汇编的标准格式**

在讲源码前有必要提一下本文涉及到的**GCC 内联汇编的标准格式**，后面记得回来看看，理解下

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

## *vTaskStartScheduler*

（省略了MPU、多核、调试、TLS的相关内容，本文不关注，完整版可以去下载源码）

```c
// 函数：启动FreeRTOS调度器（调度器启动后，任务才会开始被调度执行）
void vTaskStartScheduler( void )
{
    BaseType_t xReturn;  // 用于存储函数返回值（判断空闲任务、定时器任务是否创建成功）

    // 1. 创建空闲任务（每个核心都需要一个空闲任务，用于核心无其他就绪任务时执行）
    xReturn = prvCreateIdleTasks();

    // 若启用软件定时器功能
    #if ( configUSE_TIMERS == 1 )
    {
        // 若空闲任务创建成功，再创建定时器服务任务（处理软件定时器的超时回调）
        if( xReturn == pdPASS )
        {
            xReturn = xTimerCreateTimerTask();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();  // 覆盖率测试标记（空闲任务创建失败时执行）
        }
    }
    #endif /* configUSE_TIMERS */

    // 若空闲任务（+定时器任务，若启用）创建成功，继续启动流程
    if( xReturn == pdPASS )
    {
        /* 仅当用户定义了FREERTOS_TASKS_C_ADDITIONS_INIT()宏时，才调用此函数
         * 用于用户自定义的初始化操作（如扩展任务功能的额外配置） */
        #ifdef FREERTOS_TASKS_C_ADDITIONS_INIT
        {
            freertos_tasks_c_additions_init();
        }
        #endif

        /* 此处禁用中断：确保在调用xPortStartScheduler()之前/过程中，不会触发时钟节拍中断
         * （已创建任务的栈中存储了“中断使能”的状态字，第一个任务运行时会自动重新使能中断） */
        portDISABLE_INTERRUPTS();

        // 2. 初始化调度器核心变量
        xNextTaskUnblockTime = portMAX_DELAY;  // 初始化“下一个任务解阻塞时间”为最大值（表示暂无任务延迟）
        xSchedulerRunning = pdTRUE;            // 标记“调度器已启动”
        xTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;  // 初始化Tick计数器（默认0，即系统启动时刻）

        /* 配置时钟节拍中断是硬件相关操作，因此实现在移植层（portable）中 */

        /* xPortStartScheduler()的返回值通常无需处理（多数移植中此函数不会返回）
         * 此处强转为void避免编译器警告 */
        // 3. 调用移植层函数，启动调度循环（核心步骤）
        ( void ) xPortStartScheduler();

        /* 多数情况下，xPortStartScheduler()不会返回：
         * - 若返回pdTRUE：堆内存不足，无法创建空闲任务或定时器任务（理论上此处已提前检查，不会触发）
         * - 若返回pdFALSE：应用层调用了xTaskEndScheduler()（多数移植不实现此函数，因无“返回点”） */
    }else{}
}
```

四件事：

1.创建空闲任务（怎么创建的看[FreeRTOS之Task-03空闲任务（API+源码）](./FreeRTOS之Task-03空闲任务（API+源码）.md)）

*prvCreateIdleTasks()*

2.创建定时器守护任务（怎么创建的看[FreeRTOS之Timer-02定时器守护任务（源码）](./FreeRTOS之Timer-02定时器守护任务（源码）.md)）

*prvCreateIdleTasks()*

3.执行用户定义函数（需定义宏 *FREERTOS_TASKS_C_ADDITIONS_INIT* ）

*freertos_tasks_c_additions_init();*

看来可以把一些初始化放在这里。

4.初始化调度器核心变量

```c
        xNextTaskUnblockTime = portMAX_DELAY;  // 初始化“下一个任务解阻塞时间”为最大值（表示暂无任务延迟）
        xSchedulerRunning = pdTRUE;            // 标记“调度器已启动”
        xTickCount = ( TickType_t ) configINITIAL_TICK_COUNT;  // 初始化Tick计数器（默认0，即系统启动时刻）
```

最后调用移植层函数（*xPortStartScheduler*），启动调度循环（我使用的是ARM CM3（ARM Cortex-M3）移植版本）

```c
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
```

默认 *configCHECK_HANDLER_INSTALLATION ==1* 所以首先校验 *SVCall*（系统调用）和 *PendSV*（挂起服务调用）中断是否正确。

```c
/* 用于检查 FreeRTOS 中断处理程序（ISR）安装是否正确的常量。 */
// SCB->VTOR 寄存器：向量表偏移寄存器（存储中断向量表的起始地址，用于检查向量表配置）
#define portSCB_VTOR_REG                      ( *( ( portISR_t ** ) 0xE000ED08 ) )
// SVC 异常的向量表索引（中断向量表中第 11 个位置对应 SVC 异常处理函数）
#define portVECTOR_INDEX_SVC                  ( 11 )
// PendSV 异常的向量表索引（中断向量表中第 14 个位置对应 PendSV 异常处理函数）
#define portVECTOR_INDEX_PENDSV               ( 14 )

// 从 SCB 寄存器的 VTOR（向量表偏移寄存器）中，获取中断向量表的起始地址（portSCB_VTOR_REG 是 VTOR 寄存器的硬件地址，通常为 0xE000ED08）
const portISR_t * const pxVectorTable = portSCB_VTOR_REG;

/* 校验核心：确保 SVCall/PendSV 中断的 Handler 是 FreeRTOS 规定的函数（不校验 SysTick，因用户可能用其他定时器驱动 RTOS 时基）。
* 断言失败原因：要么 Handler 安装错误，要么 VTOR 未指向正确的向量表（向量表地址配置错误）。 */
configASSERT( pxVectorTable[ portVECTOR_INDEX_SVC ] == vPortSVCHandler );  // 校验 SVCall Handler 是否为 vPortSVCHandler
configASSERT( pxVectorTable[ portVECTOR_INDEX_PENDSV ] == xPortPendSVHandler );  // 校验 PendSV Handler 是否为 xPortPendSVHandler
```

校验后，设置*PendSV*、*SysTick*、*SVCall*的优先级。

```c
// 系统异常优先级寄存器 2（SHPR2）：配置 PendSV 等异常的优先级（Cortex-M3 异常优先级由 SHPR1~SHPR3 控制）
#define portNVIC_SHPR2_REG                    ( *( ( volatile uint32_t * ) 0xe000ed1c ) )
// 系统异常优先级寄存器 3（SHPR3）：配置 SysTick 等异常的优先级
#define portNVIC_SHPR3_REG                    ( *( ( volatile uint32_t * ) 0xe000ed20 ) )
// 定义“最低中断优先级”（Cortex-M3 支持 0~255 级优先级，255 为最低）
#define portMIN_INTERRUPT_PRIORITY            ( 255UL )
// 配置 PendSV 异常优先级：将最低优先级左移 16 位，对应 SHPR2 寄存器中 PendSV 优先级的位域（第 16~23 位）
#define portNVIC_PENDSV_PRI                   ( ( ( uint32_t ) portMIN_INTERRUPT_PRIORITY ) << 16UL )
// 配置 SysTick 异常优先级：将最低优先级左移 24 位，对应 SHPR3 寄存器中 SysTick 优先级的位域（第 24~31 位）
#define portNVIC_SYSTICK_PRI                  ( ( ( uint32_t ) portMIN_INTERRUPT_PRIORITY ) << 24UL )

/* 配置核心中断的优先级（调度器依赖的关键中断）：
 * - PendSV：用于任务上下文切换，需设为最低优先级（确保任何中断都能打断它，避免切换延迟）；
 * - SysTick：用于产生 RTOS 时基（Tick 中断，如 1ms 一次），需设为低优先级（避免抢占关键任务）；
 * - SVCall：用于启动第一个任务，需设为最高优先级（确保启动过程不被其他中断打断）；
 * 寄存器说明：portNVIC_SHPR2_REG（SVCall 优先级寄存器，地址 0xE000ED1C）、portNVIC_SHPR3_REG（PendSV/SysTick 优先级寄存器，地址 0xE000ED20）；
 * 优先级值说明：portNVIC_PENDSV_PRI/portNVIC_SYSTICK_PRI 通常为 0xFF（最低优先级），portNVIC_SHPR2_REG 写 0 表示最高优先级。 */
portNVIC_SHPR3_REG |= portNVIC_PENDSV_PRI;
portNVIC_SHPR3_REG |= portNVIC_SYSTICK_PRI;
portNVIC_SHPR2_REG = 0;
```

配置启动 *SysTick* 定时器

```c
/* 启动 RTOS 时基定时器（产生 Tick 中断）：
 * - 此时中断已禁用（调度器启动前默认关中断），无需担心定时器启动时触发中断；
 * - 函数实现：vPortSetupTimerInterrupt 是 FreeRTOS 弱函数，默认配置 SysTick 定时器（如设置 LOAD 寄存器为“系统时钟/1000-1”，实现 1ms 中断，再使能 SysTick 中断和计数器）。 */
vPortSetupTimerInterrupt();

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
```

然后开启第一个任务

```c
    /* 初始化临界区嵌套计数：
     * - uxCriticalNesting 是全局变量，记录当前临界区嵌套层数（0 表示未进入临界区）；
     * - 第一个任务启动前，需确保临界区计数为 0，避免任务启动后临界区状态异常。 */
    uxCriticalNesting = 0;

    /* 启动第一个任务：
     * - 调用 prvPortStartFirstTask 函数，该函数会通过“读取 NVIC 向量表→初始化 MSP 栈指针→使能中断→触发 SVC 异常”的流程，最终进入 vPortSVCHandler 恢复第一个任务的上下文；
     * - 关键特性：若启动成功，此函数不会返回（CPU 会跳转到第一个任务的入口函数执行）；若返回，说明启动失败。 */
    prvPortStartFirstTask();

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
```

这里触发了SVC异常，下面看看

```c
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
```

这里先岔开话题，说一下这里提到的 r0-r14

可以看一下 [[001\] [ARM-Cortex-M3/4] 内部寄存器_xpsr寄存器-CSDN博客](https://blog.csdn.net/kouxi1/article/details/122914131)

Cortex-M3 架构在**中断触发时**，会自动将 xPSR、PC、LR、R12、R3~R0 这 8 个寄存器压入当前任务栈（称为 “硬件自动压栈”）；而 R11~R4 需由软件手动压栈（称为 “软件手动压栈”）。

我们这里看下FreeRTOS中的任务栈初始化函数 *pxPortInitialiseStack*

```c
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
```

![img](https://pic1.zhimg.com/80/v2-931e20e732b7cb8d97ba4f297d42e5ca_1440w.png?source=ccfced1a)

现在返回SVC异常：

```c
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
```

首先读取 *pxCurrentTCB* 地址：

```c
ldr r3, pxCurrentTCBConst2
pxCurrentTCBConst2: .word pxCurrentTCB 
```

找到栈顶指针

```c
ldr r1, [r3]  读取 pxCurrentTCB 的值
ldr r0, [r1]  读取栈顶指针（TCB 第一个成员是 pxTopOfStack）
```

从栈顶指针（r0）开始，批量弹出 R4~R11 寄存器的值（Cortex-M3 架构在中断触发时，会自动将 xPSR、PC、LR、R12、R3~R0 这 8 个寄存器压入当前任务栈（称为 “硬件自动压栈”）；而 R11~R4 需由软件手动压栈（称为 “软件手动压栈”）。）

```c
ldmia r0!, {r4-r11} 
```

> 从任务栈的初始化 pxPortInitialiseStack 可以看到栈顶下面就是R11~R4 这8个寄存器

将恢复后的栈顶指针（r0）写入 PSP 寄存器

```c
msr psp, r0
```

> 为什么将恢复后的栈顶指针（r0）写入 PSP 寄存器？ Cortex-M3 有两个栈指针：  MSP（主栈指针）：用于操作系统内核和异常处理（如中断服务程序）；  PSP（进程栈指针）：专门用于用户任务（应用程序代码） 在 FreeRTOS 等实时操作系统中，每个任务都有自己独立的栈空间，其栈顶地址保存在任务控制块（TCB）中。当任务被调度执行时：需要从 TCB 中恢复该任务的栈顶指针，并将其写入 PSP，告知 CPU："当前任务使用这个栈空间" 之前ldmia r0!, {r4-r11}已经恢复了任务的部分寄存器（R4-R11），后续执行 bx r14 异常返回时，CPU 会自动从 PSP 指向的栈中恢复剩余寄存器（xPSR、PC、LR、R12、R0-R3）

随后清除中断屏蔽（BASEPRI=0 表示不屏蔽任何中断）

```c
mov r0, #0 
msr basepri, r0  /* 将 r0 写入 BASEPRI 寄存器：清除中断屏蔽（BASEPRI=0 表示不屏蔽任何中断） */
```

修改 LR 寄存器（链接寄存器）的低 4 位为 0xD：设置异常返回模式

```c
orr r14, #0xd
/* 配置 LR 寄存器，指定异常返回时使用 PSP 栈、恢复 xPSR 状态（0xD 对应 Cortex-M 异常返回模式：Return to Thread mode, using PSP） */
```

> LR 寄存器的默认为*prvTaskExitError* *#define   portTASK_RETURN_ADDRESS   prvTaskExitError* *prvTaskExitError* 定义如下

```c
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
```

最后从异常返回：CPU 自动从 PSP 栈加载 xPSR、PC、LR、R12、R3~R0 寄存器，跳转到任务入口函数

```c
bx r14       /* 异常返回：触发 CPU 恢复上下文，进入任务执行 */
```

这样就进入了第一个任务。

之前开启调度的时候提到了三个中断

> 配置核心中断的优先级（调度器依赖的关键中断）： - PendSV：用于任务上下文切换，需设为最低优先级（确保任何中断都能打断它，避免切换延迟）； - SysTick：用于产生 RTOS 时基（Tick 中断，如 1ms 一次），需设为低优先级（避免抢占关键任务）； - SVCall：用于启动第一个任务，需设为最高优先级（确保启动过程不被其他中断打断）；

PendSV还没有说明，下面给出源码

```c
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
```

读取当前任务的栈指针（PSP，进程栈指针）到 r0，获取当前任务控制块（TCB）的地址

```c
mrs r0, psp
isb
ldr r3, pxCurrentTCBConst
ldr r2, [r3]

pxCurrentTCBConst: .word pxCurrentTCB
```

保存当前任务的寄存器（r4-r11）到任务栈

```c
stmdb r0!, {r4-r11}
str r0, [r2]            // 将更新后的栈指针（r0）存入当前 TCB 的第一个成员（TCB->pxTopOfStack），记录栈顶位置
```

调用 vTaskSwitchContext 切换任务

```c
stmdb sp!, {r3, r14}        // 保存 r3（pxCurrentTCB 地址）和 r14（返回地址）到 MSP 栈（内核栈），避免被后续调用破坏
mov r0, %0                  // 将输入参数（configMAX_SYSCALL_INTERRUPT_PRIORITY）移入 r0
msr basepri, r0             // 配置 BASEPRI 寄存器，屏蔽优先级低于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断，确保任务切换不被干扰
bl vTaskSwitchContext       // 调用 C 函数 vTaskSwitchContext：更新 pxCurrentTCB 为下一个就绪任务的 TCB 指针
```

*vTaskSwitchContext*  函数定义如下

```c
#ifndef portTASK_SWITCH_HOOK
    #define portTASK_SWITCH_HOOK( pxTCB )    ( void ) ( pxTCB )
#endif

void vTaskSwitchContext( void )
{

    if( uxSchedulerSuspended != ( UBaseType_t ) 0U )
    {
        /* 调度器当前被挂起 - 不允许上下文切换 */
        xYieldPendings[ 0 ] = pdTRUE; // 标记挂起的切换请求（单内核核心ID为0）
    }
    else
    {
        xYieldPendings[ 0 ] = pdFALSE;

        #if ( configGENERATE_RUN_TIME_STATS == 1 ) // 若启用运行时间统计，默认为不使用
        #endif /* configGENERATE_RUN_TIME_STATS */

        /* 在当前运行任务被切换出前，保存其errno值 */
        #if ( configUSE_POSIX_ERRNO == 1 )
        {
            pxCurrentTCB->iTaskErrno = FreeRTOS_errno;
        }
        #endif

        taskSELECT_HIGHEST_PRIORITY_TASK(); // 选择最高优先级的就绪任务

        /* 用于在切换任务后注入端口特定行为的宏，
        * 例如设置堆栈结束监视点或重新配置MPU */
        portTASK_SWITCH_HOOK( pxCurrentTCB );

        /* 新任务切换进入后，更新全局errno */
        #if ( configUSE_POSIX_ERRNO == 1 )
        {
        	FreeRTOS_errno = pxCurrentTCB->iTaskErrno;
        }
        #endif
    }
}
```

如果调度器被挂起，标记挂起的切换请求，如果调度器没有挂起，要进行任务切换，重点在于函数 *taskSELECT_HIGHEST_PRIORITY_TASK() // 选择最高优先级的就绪任务* 

```c
#define taskSELECT_HIGHEST_PRIORITY_TASK()                                                 \
do {                                                                                       \
    UBaseType_t uxTopPriority = uxTopReadyPriority;                                        \
    /* 查找包含就绪任务的最高优先级队列。 */                                                     \
    while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) != pdFALSE )       \
    {                                                                                      \
        configASSERT( uxTopPriority );                                                     \
        --uxTopPriority;                                                                   \
    }                                                                                      \
    /* listGET_OWNER_OF_NEXT_ENTRY 遍历链表，因此相同优先级的任务                               \
    * 可以平等共享处理器时间。 */                                                              \
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) );  \
    uxTopReadyPriority = uxTopPriority;                                                    \
} while( 0 ) /* taskSELECT_HIGHEST_PRIORITY_TASK */ 
```

关于就绪任务，FreeRTOS里对于每个优先级都有一个就绪链表，*uxTopReadyPriority* 是就绪任务最高优先级，从这个就绪任务最高优先级的链表里找 *pxReadyTasksLists[ uxTopPriority ]* ，如果当前链表没有任务，那就去低一级的优先级链表里找，直到找到任务。找到任务后将任务放到*pxCurrentTCB*（记录当前一个运行的任务，更新*pxCurrentTCB* ）。

现在返回到PendSV中断。

调用 vTaskSwitchContext 切换任务

```c
stmdb sp!, {r3, r14}        // 保存 r3（pxCurrentTCB 地址）和 r14（返回地址）到 MSP 栈（内核栈），避免被后续调用破坏
mov r0, %0                  // 将输入参数（configMAX_SYSCALL_INTERRUPT_PRIORITY）移入 r0
msr basepri, r0             // 配置 BASEPRI 寄存器，屏蔽优先级低于 configMAX_SYSCALL_INTERRUPT_PRIORITY 的中断，确保任务切换不被干扰
bl vTaskSwitchContext       // 调用 C 函数 vTaskSwitchContext：更新 pxCurrentTCB 为下一个就绪任务的 TCB 指针
mov r0, #0                  // 准备将 BASEPRI 设为 0（解除中断屏蔽）
msr basepri, r0             // 写入 BASEPRI，恢复中断响应
ldmia sp!, {r3, r14}        // 从 MSP 栈恢复 r3 和 r14（恢复调用前的寄存器状态）
```

恢复新任务的上下文（从新任务的 TCB 中加载栈信息），这里的逻辑和SVC差不多

```c
ldr r1, [r3]                        // 从 r3（pxCurrentTCB 地址）读取新任务的 TCB 指针到 r1（r1 = *pxCurrentTCB，已被 vTaskSwitchContext 更新）
ldr r0, [r1]                        // 从新 TCB 的第一个成员读取栈顶指针到 r0（r0 = 新任务的 pxTopOfStack）
ldmia r0!, {r4-r11}                 // LDMIA（Load Multiple Data, Increment After）：
                                                    // - 从栈中依次恢复 r4 到 r11 的值，栈指针 r0 后递增
msr psp, r0                         // 将更新后的栈指针（r0）写入 PSP 寄存器，新任务将使用此栈指针
isb                                 // 指令同步屏障：确保 PSP 更新生效，避免后续指令使用旧栈指针
bx r14                              // 分支跳转：r14 中是异常返回地址，执行后退出 PendSV 异常，进入新任务执行
```

接下来看看SysTickHandler

```c
void xPortSysTickHandler( void )
{
    /* The SysTick runs at the lowest interrupt priority, so when this interrupt
     * executes all interrupts must be unmasked.  There is therefore no need to
     * save and then restore the interrupt mask value as its value is already
     * known. */
    portDISABLE_INTERRUPTS();  // 禁用所有可屏蔽中断，确保中断处理过程不被打断（临界区保护）
    {
        /* Increment the RTOS tick. */
        if( xTaskIncrementTick() != pdFALSE )
        {
            /* 需要执行上下文切换。上下文切换在 PendSV 中断中执行，因此挂起 PendSV 中断。 */
            // portNVIC_INT_CTRL_REG：内核中断控制寄存器（地址 0xE000ED04）
            // portNVIC_PENDSVSET_BIT：PendSV 中断挂起位（置 1 表示“请求执行 PendSV 中断”）
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }else{}
    }
    portENABLE_INTERRUPTS();   // 使能所有可屏蔽中断，退出临界区
}
```

关于 *xTaskIncrementTick* 在[FreeRTOS之资源管理（API+源码）](./FreeRTOS之资源管理（API+源码）.md)中说过，主要是递增全局滴答计数、检查此时延迟态的任务（延迟链表的任务）是否有任务进入就绪态（把任务放到就绪链表中）、判断是否需要上下文切换。

也就是说启动系统中的第一个任务，调用SVCHandler，然后随着SysTickHandler不断递增系统滴答计数，需要的话会触发PendSVHandler实现任务上下文切换。（SysTickHandler中主要处理延迟的任务，实际还有任务比如在等待一个消息，有消息时也会让任务进入就绪态，如果此任务优先级大并且是抢断式的话，也会触发上下文切换）。