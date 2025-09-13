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


#ifndef INC_TASK_H
#define INC_TASK_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h must appear in source files before include task.h"
#endif

#include "list.h"

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

/*-----------------------------------------------------------
* 宏与定义
*----------------------------------------------------------*/

/*
 * 若 tskKERNEL_VERSION_NUMBER 以 + 结尾，表示该版本是编号发布版本之后的开发中版本。
 *
 * tskKERNEL_VERSION_MAJOR（主版本号）、tskKERNEL_VERSION_MINOR（次版本号）、tskKERNEL_VERSION_BUILD（构建号）
 * 这三个值将反映上一个已发布版本的版本号。
 */
#define tskKERNEL_VERSION_NUMBER       "V11.1.0"  // FreeRTOS 内核版本号（字符串形式，此处为已发布的 V11.1.0 版本）
#define tskKERNEL_VERSION_MAJOR        11         // 主版本号：表示重大功能迭代或架构变更，此处为 11
#define tskKERNEL_VERSION_MINOR        1         // 次版本号：表示新增功能、优化或兼容更新，此处为 1
#define tskKERNEL_VERSION_BUILD        0         // 构建号：通常用于修复漏洞、Bug 修复或微小调整，此处为 0

/* MPU（内存保护单元）区域参数，通过 MemoryRegion_t 结构体的 ulParameters 成员传递。 */
#define tskMPU_REGION_READ_ONLY        ( 1U << 0U )  // MPU 区域权限：只读
#define tskMPU_REGION_READ_WRITE       ( 1U << 1U )  // MPU 区域权限：可读可写
#define tskMPU_REGION_EXECUTE_NEVER    ( 1U << 2U )  // MPU 区域权限：禁止执行（代码无法在此区域运行）
#define tskMPU_REGION_NORMAL_MEMORY    ( 1U << 3U )  // MPU 区域类型：普通内存（如 RAM、Flash，支持缓存）
#define tskMPU_REGION_DEVICE_MEMORY    ( 1U << 4U )  // MPU 区域类型：设备内存（如外设寄存器，不支持缓存）

/* 存储在MPU（内存保护单元）设置中的区域权限，用于授权访问请求。 */
#define tskMPU_READ_PERMISSION         ( 1U << 0U )  // MPU读取权限（第0位置1）
#define tskMPU_WRITE_PERMISSION        ( 1U << 1U )  // MPU写入权限（第1位置1）

/* 任务直接通知功能早期版本中，每个任务仅支持1个通知。
 * 现在每个任务支持一个通知数组，数组长度由configTASK_NOTIFICATION_ARRAY_ENTRIES配置项定义。
 * 为保证向后兼容，所有使用早期任务直接通知功能的代码，默认使用数组的第一个索引。 */
#define tskDEFAULT_INDEX_TO_NOTIFY     ( 0 )  // 任务通知数组的默认索引（第0个元素）

/**
 * task.h
 *
 * 用于引用任务的类型。例如，调用 xTaskCreate 函数时，会通过指针参数返回一个 TaskHandle_t 类型的变量，
 * 该变量随后可作为 vTaskDelete 函数的参数，用于删除对应的任务。
 *
 * \defgroup TaskHandle_t TaskHandle_t
 * \ingroup Tasks
 */
struct tskTaskControlBlock; /* 采用旧的命名规则，以避免破坏支持内核感知的调试器（kernel aware debuggers）。 */
typedef struct tskTaskControlBlock         * TaskHandle_t;       // 任务句柄类型（可修改的任务引用）
typedef const struct tskTaskControlBlock   * ConstTaskHandle_t; // 常量任务句柄类型（不可修改的任务引用）

/*
 * 定义应用层任务钩子函数（Task Hook Function）必须遵循的函数原型。
 */
typedef BaseType_t (* TaskHookFunction_t)( void * arg );

/* eTaskGetState 函数返回的任务状态。 */
typedef enum
{
    eRunning = 0, /* 任务正在查询自身状态，因此必定为运行态。 */
    eReady,       /* 被查询的任务处于就绪态或挂起就绪列表中。 */
    eBlocked,     /* 被查询的任务处于阻塞态。 */
    eSuspended,   /* 被查询的任务处于挂起态，或处于带无限超时的阻塞态。 */
    eDeleted,     /* 被查询的任务已被删除，但其任务控制块（TCB）尚未被释放。 */
    eInvalid      /* 用作“无效状态”的值。 */
} eTaskState;

/* 调用 vTaskNotify() 时可执行的操作。 */
typedef enum
{
    eNoAction = 0,            /* 通知任务，但不更新其通知值。 */
    eSetBits,                 /* 对任务的通知值执行按位设置操作。 */
    eIncrement,               /* 递增任务的通知值。 */
    eSetValueWithOverwrite,   /* 将任务的通知值设置为特定值，即使任务尚未读取先前的值（覆盖模式）。 */
    eSetValueWithoutOverwrite /* 仅在任务已读取先前值的情况下，才将任务的通知值设置为特定值（不覆盖模式）。 */
} eNotifyAction;
/*
 * 仅内部使用。
 */
// 定义超时结构体
typedef struct xTIME_OUT
{
    BaseType_t xOverflowCount;  // 溢出计数器
    TickType_t xTimeOnEntering; // 进入时的时间戳
} TimeOut_t;

/*
 * 定义使用MPU（内存保护单元）时分配给任务的内存范围。
 */
// 定义内存区域结构体
typedef struct xMEMORY_REGION
{
    void * pvBaseAddress;       // 基地址指针
    uint32_t ulLengthInBytes;   // 字节长度
    uint32_t ulParameters;      // 参数（通常用于MPU配置）
} MemoryRegion_t;

/*
 * 创建受MPU保护的任务所需的参数。
 */
// 定义任务参数结构体
typedef struct xTASK_PARAMETERS
{
    TaskFunction_t pvTaskCode;  // 任务函数指针
    const char * pcName;        // 任务名称
    configSTACK_DEPTH_TYPE usStackDepth; // 栈深度
    void * pvParameters;        // 传递给任务的参数
    UBaseType_t uxPriority;     // 任务优先级
    StackType_t * puxStackBuffer; // 栈缓冲区指针
    MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ]; // 内存区域数组
    #if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
        StaticTask_t * const pxTaskBuffer; // 静态任务缓冲区（仅在特定配置下有效）
    #endif
} TaskParameters_t;

/* 与uxTaskGetSystemState()函数一起使用，用于返回系统中每个任务的状态。 */
// 定义任务状态结构体
typedef struct xTASK_STATUS
{
    TaskHandle_t xHandle;                         /* 与结构体中其余信息相关的任务句柄。 */
    const char * pcTaskName;                      /* 指向任务名称的指针。如果在结构体填充后任务被删除，此值将无效！ */
    UBaseType_t xTaskNumber;                      /* 任务的唯一编号。 */
    eTaskState eCurrentState;                     /* 结构体填充时任务所处的状态。 */
    UBaseType_t uxCurrentPriority;                /* 结构体填充时任务运行的优先级（可能是继承的）。 */
    UBaseType_t uxBasePriority;                   /* 当任务的当前优先级已被继承以避免获取互斥锁时的无界优先级反转时，任务将返回的优先级。仅当在FreeRTOSConfig.h中configUSE_MUTEXES定义为1时有效。 */
    configRUN_TIME_COUNTER_TYPE ulRunTimeCounter; /* 到目前为止分配给任务的总运行时间，由运行时统计时钟定义。参见https://www.FreeRTOS.org/rtos-run-time-stats.html。仅当在FreeRTOSConfig.h中configGENERATE_RUN_TIME_STATS定义为1时有效。 */
    StackType_t * pxStackBase;                    /* 指向任务栈区域的最低地址。 */
    #if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
        StackType_t * pxTopOfStack;               /* 指向任务栈区域的顶部地址。 */
        StackType_t * pxEndOfStack;               /* 指向任务栈区域的结束地址。 */
    #endif
    configSTACK_DEPTH_TYPE usStackHighWaterMark;  /* 自任务创建以来任务剩余的最小栈空间量。此值越接近零，任务就越接近溢出其栈。 */
    #if ( ( configUSE_CORE_AFFINITY == 1 ) && ( configNUMBER_OF_CORES > 1 ) )
        UBaseType_t uxCoreAffinityMask;           /* 任务的核心亲和性掩码 */
    #endif
} TaskStatus_t;

/* eTaskConfirmSleepModeStatus()的可能返回值。 */
// 定义睡眠模式状态枚举
typedef enum
{
    eAbortSleep = 0, /* 自调用portSUPPRESS_TICKS_AND_SLEEP()以来，有任务已就绪或挂起了上下文切换 - 中止进入睡眠模式。 */
    eStandardSleep   /* 进入睡眠模式，持续时间不会超过预期的空闲时间。 */
    #if ( INCLUDE_vTaskSuspend == 1 )
        ,
        eNoTasksWaitingTimeout /* 没有任务在等待超时，因此可以安全地进入只能通过外部中断退出的睡眠模式。 */
    #endif /* INCLUDE_vTaskSuspend */
} eSleepModeStatus;

/**
 * 定义空闲任务（idle task）所使用的优先级。此宏的值不得修改。
 *
 * \ingroup TaskUtils  // 属于“任务工具”（TaskUtils）功能组
 */
#define tskIDLE_PRIORITY    ( ( UBaseType_t ) 0U )
// 解释：
// 1. 这是FreeRTOS（一款实时操作系统）中的核心宏定义，用于指定系统“空闲任务”的优先级；
// 2. “idle task”是系统自动创建的低优先级任务，仅在无其他高优先级任务需要执行时运行（如释放内存、低功耗处理等）；
// 3. (UBaseType_t) 是FreeRTOS定义的无符号基础类型，用于确保优先级数值的类型一致性；
// 4. 0U 表示优先级数值为0（FreeRTOS中默认优先级数值越小，任务优先级越低，因此空闲任务是系统最低优先级任务）；
// 5. 注释明确“不得修改”，因为修改会破坏系统对空闲任务的调度逻辑，可能导致系统异常。

/**
 * 定义任务与所有可用内核（core）的亲和性（affinity）。
 *
 * \ingroup TaskUtils  // 属于“任务工具”（TaskUtils）功能组
 */
#define tskNO_AFFINITY      ( ( UBaseType_t ) -1 )

/**
 * task. h  // 该宏定义所在的头文件
 *
 * 用于强制触发任务上下文切换（context switch）的宏。
 *
 * \defgroup taskYIELD taskYIELD  // 属于“调度器控制”（SchedulerControl）功能组，组名为taskYIELD
 * \ingroup SchedulerControl
 */
#define taskYIELD()                          portYIELD()

// 解释：
// 1. “上下文切换”指系统暂停当前运行的任务、保存其运行状态（如寄存器值、程序计数器等），并加载另一个就绪任务的状态使其开始运行的过程；
// 2. taskYIELD() 是FreeRTOS提供的上层通用宏，用于主动请求调度器进行上下文切换（例如当前任务已完成核心工作，希望让渡CPU给其他任务）；
// 3. 该宏最终调用 portYIELD()，这是与硬件平台相关的底层宏（“port”代表“端口”，即针对特定CPU/硬件的适配层）；
// 4. portYIELD() 的实现因硬件而异，例如在ARM Cortex-M系列中可能通过触发 PendSV（可悬起的系统调用）异常来实现上下文切换，确保切换过程的安全性和统一性；
// 5. 使用场景：任务在执行过程中无需继续占用CPU（如等待事件但未阻塞），主动调用此宏可提高系统响应速度和CPU利用率。

/**
 * task.h  // 该宏定义所在的头文件
 *
 * 用于标记临界代码区域开始的宏。在临界区域内，抢占ive（抢占式）上下文文切换不会发生。
 *
 * 注意：根据具体的移植实现，这可能会修改栈（stack），因此必须谨慎使用！
 *
 * \defgroup taskENTER_CRITICAL taskENTER_CRITICAL  // 属于“调度器控制”功能组，组名为taskENTER_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskENTER_CRITICAL()                 portENTER_CRITICAL()
// 解释：
// 1. 临界区域（critical region）是一段不允许被中断或任务抢占的代码，用于保护共享资源的操作（如访问全局变量）；
// 2. 调用此宏后，FreeRTOS会禁用任务抢占机制，确保当前任务能完整执行临界区代码而不被其他任务打断；
// 3. 实际功能由portENTER_CRITICAL()实现，这是与硬件平台相关的底层函数（不同CPU的中断控制方式不同）；
// 4. 注意事项中提到可能修改栈，是因为某些平台实现中需要保存/修改栈状态来实现临界区保护。

// 根据配置的CPU核心数量（configNUMBER_OF_CORES）定义不同的中断服务程序（ISR）中的临界区进入宏
#if ( configNUMBER_OF_CORES == 1 )
    #define taskENTER_CRITICAL_FROM_ISR()    portSET_INTERRUPT_MASK_FROM_ISR()
#else
    #define taskENTER_CRITICAL_FROM_ISR()    portENTER_CRITICAL_FROM_ISR()
#endif
// 解释：
// 1. 这是专门用于中断服务程序（ISR）中的临界区进入宏（FROM_ISR表示从ISR中调用）；
// 2. 在单核心（configNUMBER_OF_CORES == 1）系统中，使用portSET_INTERRUPT_MASK_FROM_ISR()实现，通常通过屏蔽中断来保护临界区；
// 3. 在多核系统中，使用更复杂的portENTER_CRITICAL_FROM_ISR()，需要考虑跨核心的同步问题；
// 4. 区分普通任务和ISR中的临界区宏，是因为ISR的执行环境和普通任务不同（如栈空间、中断优先级）。

/**
 * task.h  // 该宏定义所在的头文件
 *
 * 用于标记临界代码区域结束的宏。在临界区域内，抢占式上下文切换不会发生。
 *
 * 注意：根据具体的移植实现，这可能会修改栈，因此必须谨慎使用！
 *
 * \defgroup taskEXIT_CRITICAL taskEXIT_CRITICAL  // 属于“调度器控制”功能组，组名为taskEXIT_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskEXIT_CRITICAL()                    portEXIT_CRITICAL()
// 解释：
// 1. 与taskENTER_CRITICAL()配对使用，用于退出临界区域，恢复系统的抢占ive调度功能；
// 2. 实际功能由portEXIT_CRITICAL()实现，会恢复进入临界区前的系统状态（如重新允许中断）；
// 3. 必须与taskENTER_CRITICAL()成对出现，否则会导致系统长期无法进行任务切换，引发严重问题。

// 根据配置的CPU核心数量定义不同的中断服务程序中的临界区退出宏
#if ( configNUMBER_OF_CORES == 1 )
    #define taskEXIT_CRITICAL_FROM_ISR( x )    portCLEAR_INTERRUPT_MASK_FROM_ISR( x )
#else
    #define taskEXIT_CRITICAL_FROM_ISR( x )    portEXIT_CRITICAL_FROM_ISR( x )
#endif
// 解释：
// 1. 与taskENTER_CRITICAL_FROM_ISR()配对使用，用于退出ISR中的临界区域；
// 2. 单核心系统中使用portCLEAR_INTERRUPT_MASK_FROM_ISR(x)，参数x通常是进入临界区时保存的中断状态，用于恢复；
// 3. 多核系统中使用portEXIT_CRITICAL_FROM_ISR(x)，处理更复杂的跨核心状态恢复；
// 4. 必须在ISR中与对应的进入宏成对使用，确保中断状态正确恢复，避免系统异常。

/**
 * task.h  // 宏定义所在的头文件
 *
 * 用于禁用所有可屏蔽中断的宏。
 *
 * \defgroup taskDISABLE_INTERRUPTS taskDISABLE_INTERRUPTS  // 属于“调度器控制”功能组
 * \ingroup SchedulerControl
 */
#define taskDISABLE_INTERRUPTS()    portDISABLE_INTERRUPTS()
// 解释：
// 1. 此宏用于全局禁用处理器的所有可屏蔽中断（maskable interrupts），即除不可屏蔽中断（NMI）外的所有中断都将被禁止；
// 2. 实际功能由硬件平台相关的portDISABLE_INTERRUPTS()实现，不同CPU的中断禁用方式不同（如修改中断屏蔽寄存器）；
// 3. 主要用于需要完全禁止中断响应的场景（如极短的临界操作），但应谨慎使用，避免影响系统对实时事件的响应。

/**
 * task.h  // 宏定义所在的头文件
 *
 * 用于启用微控制器中断的宏。
 *
 * \defgroup taskENABLE_INTERRUPTS taskENABLE_INTERRUPTS  // 属于“调度器控制”功能组
 * \ingroup SchedulerControl
 */
#define taskENABLE_INTERRUPTS()     portENABLE_INTERRUPTS()
// 解释：
// 1. 与taskDISABLE_INTERRUPTS()配对使用，用于恢复处理器的中断响应功能；
// 2. 由底层的portENABLE_INTERRUPTS()实现，会恢复进入中断禁用前的中断状态；
// 3. 必须与禁用宏成对使用，否则可能导致系统无法响应外部事件。

/* xTaskGetSchedulerState()函数的返回值定义。
 * 当configASSERT()被定义时，taskSCHEDULER_SUSPENDED被设为0以生成更优的代码，
 * 因为该常量会用于assert()语句中。 */
#define taskSCHEDULER_SUSPENDED      ( ( BaseType_t ) 0 )  // 调度器已挂起
#define taskSCHEDULER_NOT_STARTED    ( ( BaseType_t ) 1 )  // 调度器未启动
#define taskSCHEDULER_RUNNING        ( ( BaseType_t ) 2 )  // 调度器正在运行
// 解释：
// 1. 这些宏定义了调度器（scheduler）的三种状态，供xTaskGetSchedulerState()函数返回使用；
// 2. taskSCHEDULER_SUSPENDED（0）：调度器暂停工作，任务切换被禁止；
// 3. taskSCHEDULER_NOT_STARTED（1）：调度器尚未启动（如vTaskStartScheduler()未被调用）；
// 4. taskSCHEDULER_RUNNING（2）：调度器正常运行，任务按优先级进行切换；
// 5. 将挂起状态设为0是为了在断言（assert）中简化判断逻辑，提升代码执行效率。

/* 检查核心ID是否有效的宏。 */
#define taskVALID_CORE_ID( xCoreID )    ( ( ( ( ( BaseType_t ) 0 <= ( xCoreID ) ) && ( ( xCoreID ) < ( BaseType_t ) configNUMBER_OF_CORES ) ) ) ? ( pdTRUE ) : ( pdFALSE ) )
// 解释：
// 1. 用于验证给定的核心ID（xCoreID）是否在有效范围内（适用于多核处理器）；
// 2. 核心ID的有效范围是：大于等于0，且小于配置的核心总数（configNUMBER_OF_CORES）；
// 3. 若ID有效返回pdTRUE（真），否则返回pdFALSE（假）；
// 4. 主要用于多核环境下的任务核心亲和性设置、核心特定操作等场景，避免因无效核心ID导致的错误。

/*-----------------------------------------------------------
* 任务创建相关API
*----------------------------------------------------------*/

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskCreate(
 *                            TaskFunction_t pxTaskCode,
 *                            const char * const pcName,
 *                            const configSTACK_DEPTH_TYPE uxStackDepth,
 *                            void *pvParameters,
 *                            UBaseType_t uxPriority,
 *                            TaskHandle_t *pxCreatedTask
 *                        );
 * @endcode
 *
 * 创建一个新任务并将其添加到就绪任务列表中。
 *
 * 在FreeRTOS内部实现中，任务使用两块内存：
 * 第一块用于存储任务的数据结构，第二块作为任务的栈。
 * 如果使用xTaskCreate()创建任务，这两块内存会在函数内部自动动态分配
 *（参见https://www.FreeRTOS.org/a00111.html）。
 * 如果使用xTaskCreateStatic()创建任务，则需要应用程序开发者提供所需内存，
 * 因此xTaskCreateStatic()可以在不使用任何动态内存分配的情况下创建任务。
 *
 * 参见xTaskCreateStatic()了解不使用动态内存分配的版本。
 *
 * xTaskCreate()仅能用于创建可以无限制访问整个微控制器内存映射的任务。
 * 包含MPU（内存保护单元）支持的系统可以使用xTaskCreateRestricted()创建
 * 受MPU约束的任务。
 *
 * @param pxTaskCode 任务入口函数的指针。任务必须实现为永不返回（即无限循环）。
 *
 * @param pcName 任务的描述性名称，主要用于调试。最大长度由configMAX_TASK_NAME_LEN定义，默认是16。
 *
 * @param uxStackDepth 任务栈的大小，以栈能容纳的变量数量表示，而非字节数。
 * 例如，如果栈是16位宽，uxStackDepth定义为100，则会为栈分配200字节的存储空间。
 *
 * @param pvParameters 指向将作为所创建任务参数的指针。
 *
 * @param uxPriority 任务运行的优先级。包含MPU支持的系统可以通过设置优先级参数的
 * portPRIVILEGE_BIT位来创建特权（系统）模式的任务。例如，要创建优先级为2的特权任务，
 * uxPriority参数应设置为(2 | portPRIVILEGE_BIT)。
 *
 * @param pxCreatedTask 用于传回一个句柄，通过该句柄可以引用所创建的任务。
 *
 * @return 如果任务成功创建并添加到就绪列表，则返回pdPASS，否则返回projdefs.h中定义的错误代码。
 *
 * 使用示例：
 * @code{c}
 * // 要创建的任务
 * void vTaskCode( void * pvParameters )
 * {
 *   for( ;; )
 *   {
 *       // 任务代码放在这里
 *   }
 * }
 *
 * // 创建任务的函数
 * void vOtherFunction( void )
 * {
 * static uint8_t ucParameterToPass;
 * TaskHandle_t xHandle = NULL;
 *
 *   // 创建任务，存储句柄。注意，传递的参数ucParameterToPass
 *   // 必须在任务的生命周期内存在，因此在这种情况下被声明为static。
 *   // 如果它只是一个自动栈变量，那么当新任务尝试访问它时，它可能已经不存在，或者至少已被损坏。
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, &ucParameterToPass, tskIDLE_PRIORITY, &xHandle );
 *   configASSERT( xHandle );
 *
 *   // 使用句柄删除任务
 *   if( xHandle != NULL )
 *   {
 *      vTaskDelete( xHandle );
 *   }
 * }
 * @endcode
 * \defgroup xTaskCreate xTaskCreate
 * \ingroup Tasks
 */
// 如果支持动态内存分配，则声明xTaskCreate函数
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
    BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                            const char * const pcName,
                            const configSTACK_DEPTH_TYPE uxStackDepth,
                            void * const pvParameters,
                            UBaseType_t uxPriority,
                            TaskHandle_t * const pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

// 如果支持动态内存分配、是多核系统且支持核心亲和性，则声明xTaskCreateAffinitySet函数
#if ( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) )
    BaseType_t xTaskCreateAffinitySet( TaskFunction_t pxTaskCode,
                                       const char * const pcName,
                                       const configSTACK_DEPTH_TYPE uxStackDepth,
                                       void * const pvParameters,
                                       UBaseType_t uxPriority,
                                       UBaseType_t uxCoreAffinityMask,
                                       TaskHandle_t * const pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task.h
 * @code{c}
 * TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,
 *                               const char * const pcName,
 *                               const configSTACK_DEPTH_TYPE uxStackDepth,
 *                               void *pvParameters,
 *                               UBaseType_t uxPriority,
 *                               StackType_t *puxStackBuffer,
 *                               StaticTask_t *pxTaskBuffer );
 * @endcode
 *
 * 创建一个新任务并将其添加到就绪任务列表中。
 *
 * 在FreeRTOS内部实现中，任务使用两块内存：
 * 第一块用于存储任务的数据结构（TCB），第二块作为任务的栈。
 * 若使用xTaskCreate()创建任务，这两块内存会在函数内部自动动态分配
 *（参见https://www.FreeRTOS.org/a00111.html）；
 * 若使用xTaskCreateStatic()创建任务，则需由应用程序开发者自行提供所需内存，
 * 因此xTaskCreateStatic()可在不使用任何动态内存分配的情况下创建任务。
 *
 * @param pxTaskCode 任务入口函数的指针。任务必须实现为永不返回的形式（即无限循环）。
 *
 * @param pcName 任务的描述性名称，主要用于调试。字符串的最大长度由FreeRTOSConfig.h中的
 * configMAX_TASK_NAME_LEN定义。
 *
 * @param uxStackDepth 任务栈的大小，以栈能容纳的“变量数量”表示（非字节数）。
 * 例如，若栈宽度为32位（即每个栈元素占4字节），且uxStackDepth定义为100，
 * 则栈存储需占用400字节（100 × 32位）。
 *
 * @param pvParameters 指向将作为所创建任务参数的指针。
 *
 * @param uxPriority 任务运行的优先级。
 *
 * @param puxStackBuffer 必须指向一个至少包含uxStackDepth个元素的StackType_t数组，
 * 该数组将作为任务的栈，从而避免栈的动态分配。
 *
 * @param pxTaskBuffer 必须指向一个StaticTask_t类型的变量，
 * 该变量将用于存储任务的数据结构（TCB），从而避免该部分内存的动态分配。
 *
 * @return 若puxStackBuffer和pxTaskBuffer均不为NULL，则任务创建成功，返回所创建任务的句柄；
 * 若puxStackBuffer或pxTaskBuffer任一为NULL，则任务创建失败，返回NULL。
 *
 * 使用示例：
 * @code{c}
 *
 *  // 待创建任务将使用的栈缓冲区大小
 *  // 注意：这是栈能容纳的“变量数量”，而非字节数。例如，若每个栈元素为32位（4字节），
 *  // 此处设为100，则需分配400字节（100 × 32位）的栈空间。
 #define STACK_SIZE 200
 *
 *  // 用于存储待创建任务TCB的结构体
 *  StaticTask_t xTaskBuffer;
 *
 *  // 待创建任务将使用的栈缓冲区。注意这是一个StackType_t类型的数组，
 *  // StackType_t的大小由RTOS移植层（port）决定。
 *  StackType_t xStack[ STACK_SIZE ];
 *
 *  // 待创建任务的实现函数
 *  void vTaskCode( void * pvParameters )
 *  {
 *      // 预期参数值为1，因为在调用xTaskCreateStatic()时，pvParameters参数传入的是1
 *      configASSERT( ( uint32_t ) pvParameters == 1U );
 *
 *      for( ;; )
 *      {
 *          // 任务代码写在此处
 *      }
 *  }
 *
 *  // 创建任务的函数
 *  void vOtherFunction( void )
 *  {
 *      TaskHandle_t xHandle = NULL;
 *
 *      // 不使用任何动态内存分配创建任务
 *      xHandle = xTaskCreateStatic(
 *                    vTaskCode,       // 实现任务的函数
 *                    "NAME",          // 任务的文本名称
 *                    STACK_SIZE,      // 栈大小（按变量数量，非字节）
 *                    ( void * ) 1,    // 传入任务的参数
 *                    tskIDLE_PRIORITY,// 任务创建时的优先级
 *                    xStack,          // 用作任务栈的数组
 *                    &xTaskBuffer );  // 用于存储任务数据结构的变量
 *
 *      // puxStackBuffer和pxTaskBuffer均非NULL，因此任务已创建成功，
 *      // xHandle即为任务句柄。可通过该句柄挂起任务。
 *      vTaskSuspend( xHandle );
 *  }
 * @endcode
 * \defgroup xTaskCreateStatic xTaskCreateStatic
 * \ingroup Tasks
 */
// 若支持静态内存分配，则声明xTaskCreateStatic函数（PRIVILEGED_FUNCTION表示仅特权模式可调用）
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,
                                    const char * const pcName,
                                    const configSTACK_DEPTH_TYPE uxStackDepth,
                                    void * const pvParameters,
                                    UBaseType_t uxPriority,
                                    StackType_t * const puxStackBuffer,
                                    StaticTask_t * const pxTaskBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

// 若支持静态内存分配、为多核系统且支持核心亲和性，则声明xTaskCreateStaticAffinitySet函数
#if ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) )
    TaskHandle_t xTaskCreateStaticAffinitySet( TaskFunction_t pxTaskCode,
                                               const char * const pcName,
                                               const configSTACK_DEPTH_TYPE uxStackDepth,
                                               void * const pvParameters,
                                               UBaseType_t uxPriority,
                                               StackType_t * const puxStackBuffer,
                                               StaticTask_t * const pxTaskBuffer,
                                               UBaseType_t uxCoreAffinityMask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskCreateRestricted( TaskParameters_t *pxTaskDefinition, TaskHandle_t *pxCreatedTask );
 * @endcode
 *
 * 仅当 configSUPPORT_DYNAMIC_ALLOCATION 设为 1 时可用。
 *
 * xTaskCreateRestricted() 仅应在包含 MPU（内存保护单元）实现的系统中使用。
 *
 * 创建一个新任务并将其添加到就绪任务列表中。函数参数定义了分配给该任务的内存区域及相关访问权限。
 *
 * 参见 xTaskCreateRestrictedStatic() 了解不使用任何动态内存分配的版本。
 *
 * @param pxTaskDefinition 指向一个结构体的指针，该结构体包含：
 * - 常规 xTaskCreate() 的所有参数（参见 xTaskCreate() API 文档）
 * - 可选的栈缓冲区
 * - 内存区域定义（用于配置 MPU 权限）
 *
 * @param pxCreatedTask 用于传回一个句柄，通过该句柄可引用所创建的任务。
 *
 * @return 若任务成功创建并添加到就绪列表，返回 pdPASS；否则返回 projdefs.h 中定义的错误代码。
 *
 * 使用示例：
 * @code{c}
 * // 定义一个 TaskParameters_t 结构体，描述待创建的任务
 * static const TaskParameters_t xCheckTaskParameters =
 * {
 *  vATask,     // pvTaskCode - 实现任务的函数
 *  "ATask",    // pcName - 任务的文本名称，用于调试
 *  100,        // uxStackDepth - 栈大小（按“变量数量”定义，非字节数）
 *  NULL,       // pvParameters - 作为参数传入任务函数
 *  ( 1U | portPRIVILEGE_BIT ),// uxPriority - 任务优先级；若任务需运行在特权模式，需置位 portPRIVILEGE_BIT
 *  cStackBuffer,// puxStackBuffer - 用作任务栈的缓冲区
 *
 *  // xRegions - 为任务分配最多 3 个独立内存区域，并配置相应访问权限
 *  // 不同处理器有不同的内存对齐要求，详见 FreeRTOS 文档
 *  {
 *      // 基地址                  长度    访问权限参数
 *      { cReadWriteArray,              32,     portMPU_REGION_READ_WRITE },    // 可读写区域
 *      { cReadOnlyArray,               32,     portMPU_REGION_READ_ONLY },     // 只读区域
 *      { cPrivilegedOnlyAccessArray,   128,    portMPU_REGION_PRIVILEGED_READ_WRITE } // 仅特权模式可读写区域
 *  }
 * };
 *
 * int main( void )
 * {
 * TaskHandle_t xHandle;
 *
 *  // 根据上述定义的结构体创建任务。此处请求返回任务句柄（第二个参数非 NULL），
 *  // 仅作演示用途，实际未使用该句柄。
 *  xTaskCreateRestricted( &xRegTest1Parameters, &xHandle );
 *
 *  // 启动调度器
 *  vTaskStartScheduler();
 *
 *  // 仅当创建空闲任务和/或定时器任务的内存不足时，才会执行到此处
 *  for( ;; );
 * }
 * @endcode
 * \defgroup xTaskCreateRestricted xTaskCreateRestricted
 * \ingroup Tasks
 */
// 若启用 MPU 封装功能（portUSING_MPU_WRAPPERS == 1），则声明 xTaskCreateRestricted 函数
// PRIVILEGED_FUNCTION 表示该函数仅能在特权模式下调用
#if ( portUSING_MPU_WRAPPERS == 1 )
    BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition,
                                      TaskHandle_t * pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

// 若同时满足：启用 MPU 封装、多核系统、支持核心亲和性，则声明扩展函数 xTaskCreateRestrictedAffinitySet
#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) )
    BaseType_t xTaskCreateRestrictedAffinitySet( const TaskParameters_t * const pxTaskDefinition,
                                                 UBaseType_t uxCoreAffinityMask,  // 新增参数：任务核心亲和性掩码
                                                 TaskHandle_t * pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * BaseType_t xTaskCreateRestrictedStatic( TaskParameters_t *pxTaskDefinition, TaskHandle_t *pxCreatedTask );
 * @endcode
 *
 * Only available when configSUPPORT_STATIC_ALLOCATION is set to 1.
 *
 * xTaskCreateRestrictedStatic() should only be used in systems that include an
 * MPU implementation.
 *
 * Internally, within the FreeRTOS implementation, tasks use two blocks of
 * memory.  The first block is used to hold the task's data structures.  The
 * second block is used by the task as its stack.  If a task is created using
 * xTaskCreateRestricted() then the stack is provided by the application writer,
 * and the memory used to hold the task's data structure is automatically
 * dynamically allocated inside the xTaskCreateRestricted() function.  If a task
 * is created using xTaskCreateRestrictedStatic() then the application writer
 * must provide the memory used to hold the task's data structures too.
 * xTaskCreateRestrictedStatic() therefore allows a memory protected task to be
 * created without using any dynamic memory allocation.
 *
 * @param pxTaskDefinition Pointer to a structure that contains a member
 * for each of the normal xTaskCreate() parameters (see the xTaskCreate() API
 * documentation) plus an optional stack buffer and the memory region
 * definitions.  If configSUPPORT_STATIC_ALLOCATION is set to 1 the structure
 * contains an additional member, which is used to point to a variable of type
 * StaticTask_t - which is then used to hold the task's data structure.
 *
 * @param pxCreatedTask Used to pass back a handle by which the created task
 * can be referenced.
 *
 * @return pdPASS if the task was successfully created and added to a ready
 * list, otherwise an error code defined in the file projdefs.h
 *
 * Example usage:
 * @code{c}
 * // Create an TaskParameters_t structure that defines the task to be created.
 * // The StaticTask_t variable is only included in the structure when
 * // configSUPPORT_STATIC_ALLOCATION is set to 1.  The PRIVILEGED_DATA macro can
 * // be used to force the variable into the RTOS kernel's privileged data area.
 * static PRIVILEGED_DATA StaticTask_t xTaskBuffer;
 * static const TaskParameters_t xCheckTaskParameters =
 * {
 *  vATask,     // pvTaskCode - the function that implements the task.
 *  "ATask",    // pcName - just a text name for the task to assist debugging.
 *  100,        // uxStackDepth - the stack size DEFINED IN WORDS.
 *  NULL,       // pvParameters - passed into the task function as the function parameters.
 *  ( 1U | portPRIVILEGE_BIT ),// uxPriority - task priority, set the portPRIVILEGE_BIT if the task should run in a privileged state.
 *  cStackBuffer,// puxStackBuffer - the buffer to be used as the task stack.
 *
 *  // xRegions - Allocate up to three separate memory regions for access by
 *  // the task, with appropriate access permissions.  Different processors have
 *  // different memory alignment requirements - refer to the FreeRTOS documentation
 *  // for full information.
 *  {
 *      // Base address                 Length  Parameters
 *      { cReadWriteArray,              32,     portMPU_REGION_READ_WRITE },
 *      { cReadOnlyArray,               32,     portMPU_REGION_READ_ONLY },
 *      { cPrivilegedOnlyAccessArray,   128,    portMPU_REGION_PRIVILEGED_READ_WRITE }
 *  }
 *
 *  &xTaskBuffer; // Holds the task's data structure.
 * };
 *
 * int main( void )
 * {
 * TaskHandle_t xHandle;
 *
 *  // Create a task from the const structure defined above.  The task handle
 *  // is requested (the second parameter is not NULL) but in this case just for
 *  // demonstration purposes as its not actually used.
 *  xTaskCreateRestrictedStatic( &xRegTest1Parameters, &xHandle );
 *
 *  // Start the scheduler.
 *  vTaskStartScheduler();
 *
 *  // Will only get here if there was insufficient memory to create the idle
 *  // and/or timer task.
 *  for( ;; );
 * }
 * @endcode
 * \defgroup xTaskCreateRestrictedStatic xTaskCreateRestrictedStatic
 * \ingroup Tasks
 */
#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
    BaseType_t xTaskCreateRestrictedStatic( const TaskParameters_t * const pxTaskDefinition,
                                            TaskHandle_t * pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) )
    BaseType_t xTaskCreateRestrictedStaticAffinitySet( const TaskParameters_t * const pxTaskDefinition,
                                                       UBaseType_t uxCoreAffinityMask,
                                                       TaskHandle_t * pxCreatedTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * void vTaskAllocateMPURegions( TaskHandle_t xTask, const MemoryRegion_t * const pxRegions );
 * @endcode
 *
 * Memory regions are assigned to a restricted task when the task is created by
 * a call to xTaskCreateRestricted().  These regions can be redefined using
 * vTaskAllocateMPURegions().
 *
 * @param xTaskToModify The handle of the task being updated.
 *
 * @param[in] pxRegions A pointer to a MemoryRegion_t structure that contains the
 * new memory region definitions.
 *
 * Example usage:
 * @code{c}
 * // Define an array of MemoryRegion_t structures that configures an MPU region
 * // allowing read/write access for 1024 bytes starting at the beginning of the
 * // ucOneKByte array.  The other two of the maximum 3 definable regions are
 * // unused so set to zero.
 * static const MemoryRegion_t xAltRegions[ portNUM_CONFIGURABLE_REGIONS ] =
 * {
 *  // Base address     Length      Parameters
 *  { ucOneKByte,       1024,       portMPU_REGION_READ_WRITE },
 *  { 0,                0,          0 },
 *  { 0,                0,          0 }
 * };
 *
 * void vATask( void *pvParameters )
 * {
 *  // This task was created such that it has access to certain regions of
 *  // memory as defined by the MPU configuration.  At some point it is
 *  // desired that these MPU regions are replaced with that defined in the
 *  // xAltRegions const struct above.  Use a call to vTaskAllocateMPURegions()
 *  // for this purpose.  NULL is used as the task handle to indicate that this
 *  // function should modify the MPU regions of the calling task.
 *  vTaskAllocateMPURegions( NULL, xAltRegions );
 *
 *  // Now the task can continue its function, but from this point on can only
 *  // access its stack and the ucOneKByte array (unless any other statically
 *  // defined or shared regions have been declared elsewhere).
 * }
 * @endcode
 * \defgroup vTaskAllocateMPURegions vTaskAllocateMPURegions
 * \ingroup Tasks
 */
#if ( portUSING_MPU_WRAPPERS == 1 )
    void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify,
                                  const MemoryRegion_t * const pxRegions ) PRIVILEGED_FUNCTION;
#endif

/**
 * task.h
 * @code{c}
 * void vTaskDelete( TaskHandle_t xTaskToDelete );
 * @endcode
 *
 * 需将 INCLUDE_vTaskDelete 定义为 1，此函数才会生效。更多信息请参见配置章节。
 *
 * 从 RTOS 实时内核的管理中移除一个任务。被删除的任务将从所有“就绪列表”“阻塞列表”“挂起列表”和“事件列表”中移除。
 *
 * 注意：空闲任务（idle task）负责释放已删除任务的内核分配内存。因此，若应用中调用了 vTaskDelete()，
 * 必须确保空闲任务不会被剥夺处理器运行时间（否则已删除任务的内存无法释放，会导致内存泄漏）。
 * 任务代码自行分配的内存（如用户通过 malloc 分配的内存）不会被自动释放，需在任务删除前手动释放。
 *
 * 参见演示应用文件 death.c，获取使用 vTaskDelete() 的示例代码。
 *
 * @param xTaskToDelete 待删除任务的句柄。传入 NULL 时，将删除当前调用该函数的任务（即“自删除”）。
 *
 * 使用示例：
 * @code{c}
 * void vOtherFunction( void )
 * {
 * TaskHandle_t xHandle;
 *
 *   // 创建任务，并存储任务句柄
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );
 *
 *   // 使用句柄删除该任务
 *   vTaskDelete( xHandle );
 * }
 * @endcode
 * \defgroup vTaskDelete vTaskDelete
 * \ingroup Tasks
 */
// 声明 vTaskDelete 函数，PRIVILEGED_FUNCTION 表示该函数仅能在特权模式下调用
void vTaskDelete( TaskHandle_t xTaskToDelete ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
* 任务控制相关API
*----------------------------------------------------------*/

/**
 * task.h
 * @code{c}
 * void vTaskDelay( const TickType_t xTicksToDelay );
 * @endcode
 *
 * 将任务阻塞指定的时钟节拍数（tick）。任务实际阻塞的时间取决于系统时钟节拍率（tick rate）。
 * 可通过常量 portTICK_PERIOD_MS（每个节拍的毫秒数）将节拍数转换为实际时间，精度为一个节拍周期。
 *
 * 需将 INCLUDE_vTaskDelay 定义为 1，此函数才会生效。更多信息请参见配置章节。
 *
 * vTaskDelay() 指定的是“相对阻塞时间”——即从调用该函数的时刻起，延迟指定节拍后解除阻塞。
 * 例如，指定阻塞100个节拍，任务会在调用 vTaskDelay() 后的第100个节拍时解除阻塞。
 * 因此，vTaskDelay() 不适合用于控制周期性任务的执行频率：因为代码执行路径、其他任务或中断活动，
 * 都会影响 vTaskDelay() 的调用间隔，进而导致任务下次执行时间不确定。
 * 若需实现固定频率的周期性任务，建议使用 xTaskDelayUntil()——该函数通过指定“绝对唤醒时间”（而非相对时间），
 * 确保任务按固定周期执行。
 *
 * @param xTicksToDelay 调用任务需要阻塞的“时钟节拍数”。
 *
 * 使用示例：
 *
 * void vTaskFunction( void * pvParameters )
 * {
 * // 计算500毫秒对应的节拍数（portTICK_PERIOD_MS为每个节拍的毫秒数）
 * const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
 *
 *   for( ;; )
 *   {
 *       // 每500毫秒翻转一次LED，两次翻转间阻塞任务
 *       vToggleLED();
 *       vTaskDelay( xDelay );
 *   }
 * }
 *
 * \defgroup vTaskDelay vTaskDelay
 * \ingroup TaskCtrl
 */
// 声明 vTaskDelay 函数，PRIVILEGED_FUNCTION 表示仅能在特权模式下调用
void vTaskDelay( const TickType_t xTicksToDelay ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskDelayUntil( TickType_t *pxPreviousWakeTime, const TickType_t xTimeIncrement );
 * @endcode
 *
 * 需将 INCLUDE_xTaskDelayUntil 定义为 1，此函数才会生效。更多信息请参见配置章节。
 *
 * 将任务阻塞到指定的“绝对时间”。该函数适用于周期性任务，确保任务按固定频率执行。
 *
 * 此函数与 vTaskDelay() 的核心区别：
 * vTaskDelay() 是“相对延迟”——从调用时刻起阻塞指定节拍；而由于任务执行路径、中断/抢占的影响，
 * 两次调用 vTaskDelay() 的间隔可能不固定，导致周期性任务频率漂移。
 * xTaskDelayUntil() 是“绝对延迟”——直接指定任务需要解除阻塞的“绝对时间”，可避免频率漂移。
 *
 * 宏 pdMS_TO_TICKS() 可将毫秒级时间转换为节拍数（精度为一个节拍周期），方便用户按实际时间配置。
 *
 * @param pxPreviousWakeTime 指向变量的指针，该变量存储任务“上一次解除阻塞的时间”。
 * 首次使用前，必须将该变量初始化为当前系统时间（参见下方示例）；后续调用时，函数会自动更新该变量。
 *
 * @param xTimeIncrement 任务的“周期节拍数”。任务会在 “*pxPreviousWakeTime + xTimeIncrement” 时刻解除阻塞。
 * 若每次调用都传入相同的 xTimeIncrement，任务将按固定周期执行。
 *
 * @return 用于判断任务是否实际被阻塞的返回值：
 * - pdTRUE：任务成功被阻塞，并在指定时间解除阻塞；
 * - pdFALSE：若“下一次预期唤醒时间”已在过去（如任务执行耗时超过周期），则任务不会被阻塞，直接返回 pdFALSE。
 *
 * 使用示例：
 * @code{c}
 * // 每10个节拍执行一次任务（固定周期）
 * void vTaskFunction( void * pvParameters )
 * {
 * TickType_t xLastWakeTime;  // 存储上一次唤醒时间
 * const TickType_t xFrequency = 10;  // 任务周期（10个节拍）
 * BaseType_t xWasDelayed;    // 记录任务是否被实际阻塞
 *
 *     // 首次使用前，初始化 xLastWakeTime 为当前系统时间
 *     xLastWakeTime = xTaskGetTickCount ();
 *     for( ;; )
 *     {
 *         // 阻塞到下一个周期时间点（xLastWakeTime + xFrequency）
 *         xWasDelayed = xTaskDelayUntil( &xLastWakeTime, xFrequency );
 *
 *         // 此处执行周期性任务逻辑。xWasDelayed 可用于判断任务是否超时：
 *         // 若 xWasDelayed 为 pdFALSE，说明上一轮执行耗时超过周期，导致唤醒时间已过期
 *     }
 * }
 * @endcode
 * \defgroup xTaskDelayUntil xTaskDelayUntil
 * \ingroup TaskCtrl
 */
// 声明 xTaskDelayUntil 函数，支持返回阻塞状态
BaseType_t xTaskDelayUntil( TickType_t * const pxPreviousWakeTime,
                            const TickType_t xTimeIncrement ) PRIVILEGED_FUNCTION;

/*
 * vTaskDelayUntil() 是 xTaskDelayUntil() 的旧版本，无返回值（仅用于兼容旧代码）。
 * 通过宏定义将旧版本映射到新版本，忽略返回值（(void) 强制转换避免“未使用返回值”警告）
 */
#define vTaskDelayUntil( pxPreviousWakeTime, xTimeIncrement )                   \
    do {                                                                        \
        ( void ) xTaskDelayUntil( ( pxPreviousWakeTime ), ( xTimeIncrement ) ); \
    } while( 0 )

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskAbortDelay( TaskHandle_t xTask );
 * @endcode
 *
 * 需在 FreeRTOSConfig.h 中将 INCLUDE_xTaskAbortDelay 定义为 1，此函数才会生效。
 *
 * 任务在等待事件时会进入“阻塞态”，所等待的事件分为两类：
 * 1. 时间类事件：如调用 vTaskDelay() 等待指定时长；
 * 2. 对象类事件：如调用 xQueueReceive()（等待队列数据）或 ulTaskNotifyTake()（等待任务通知）。
 * 若对处于“阻塞态”的任务调用 xTaskAbortDelay()（传入该任务的句柄），则该任务会立即退出阻塞态，
 * 并从之前导致其进入阻塞态的函数（如 vTaskDelay()、xQueueReceive()）中返回。
 *
 * 该函数**没有中断安全版本（FromISR 版本）**，原因是：中断服务程序（ISR）需要知道任务阻塞在哪个对象上（如队列、信号量），
 * 才能判断需执行哪些配套操作（例如，若任务阻塞在队列上，ISR 需确认队列是否处于锁定状态），而 ISR 通常无法高效获取该信息。
 *
 * @param xTask 需从阻塞态中唤醒的任务句柄。
 *
 * @return 返回值说明：
 * - pdFAIL：若指定任务（xTask 指向的任务）未处于阻塞态，则返回 pdFAIL；
 * - pdPASS：若指定任务成功从阻塞态退出，则返回 pdPASS。
 *
 * \defgroup xTaskAbortDelay xTaskAbortDelay
 * \ingroup TaskCtrl
 */
// 仅当启用 xTaskAbortDelay 功能时，声明该函数（PRIVILEGED_FUNCTION 表示仅特权模式可调用）
#if ( INCLUDE_xTaskAbortDelay == 1 )
    BaseType_t xTaskAbortDelay( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask );
 * @endcode
 *
 * INCLUDE_uxTaskPriorityGet must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Obtain the priority of any task.
 *
 * @param xTask Handle of the task to be queried.  Passing a NULL
 * handle results in the priority of the calling task being returned.
 *
 * @return The priority of xTask.
 *
 * Example usage:
 * @code{c}
 * void vAFunction( void )
 * {
 * TaskHandle_t xHandle;
 *
 *   // Create a task, storing the handle.
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );
 *
 *   // ...
 *
 *   // Use the handle to obtain the priority of the created task.
 *   // It was created with tskIDLE_PRIORITY, but may have changed
 *   // it itself.
 *   if( uxTaskPriorityGet( xHandle ) != tskIDLE_PRIORITY )
 *   {
 *       // The task has changed it's priority.
 *   }
 *
 *   // ...
 *
 *   // Is our priority higher than the created task?
 *   if( uxTaskPriorityGet( xHandle ) < uxTaskPriorityGet( NULL ) )
 *   {
 *       // Our priority (obtained using NULL handle) is higher.
 *   }
 * }
 * @endcode
 * \defgroup uxTaskPriorityGet uxTaskPriorityGet
 * \ingroup TaskCtrl
 */
UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask );
 * @endcode
 *
 * A version of uxTaskPriorityGet() that can be used from an ISR.
 */
UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * UBaseType_t uxTaskBasePriorityGet( const TaskHandle_t xTask );
 * @endcode
 *
 * INCLUDE_uxTaskPriorityGet and configUSE_MUTEXES must be defined as 1 for this
 * function to be available. See the configuration section for more information.
 *
 * Obtain the base priority of any task.
 *
 * @param xTask Handle of the task to be queried.  Passing a NULL
 * handle results in the base priority of the calling task being returned.
 *
 * @return The base priority of xTask.
 *
 * \defgroup uxTaskPriorityGet uxTaskBasePriorityGet
 * \ingroup TaskCtrl
 */
UBaseType_t uxTaskBasePriorityGet( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * UBaseType_t uxTaskBasePriorityGetFromISR( const TaskHandle_t xTask );
 * @endcode
 *
 * A version of uxTaskBasePriorityGet() that can be used from an ISR.
 */
UBaseType_t uxTaskBasePriorityGetFromISR( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * eTaskState eTaskGetState( TaskHandle_t xTask );
 * @endcode
 *
 * INCLUDE_eTaskGetState must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Obtain the state of any task.  States are encoded by the eTaskState
 * enumerated type.
 *
 * @param xTask Handle of the task to be queried.
 *
 * @return The state of xTask at the time the function was called.  Note the
 * state of the task might change between the function being called, and the
 * functions return value being tested by the calling task.
 */
#if ( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_xTaskAbortDelay == 1 ) )
    eTaskState eTaskGetState( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState );
 * @endcode
 *
 * configUSE_TRACE_FACILITY must be defined as 1 for this function to be
 * available.  See the configuration section for more information.
 *
 * Populates a TaskStatus_t structure with information about a task.
 *
 * @param xTask Handle of the task being queried.  If xTask is NULL then
 * information will be returned about the calling task.
 *
 * @param pxTaskStatus A pointer to the TaskStatus_t structure that will be
 * filled with information about the task referenced by the handle passed using
 * the xTask parameter.
 *
 * @param xGetFreeStackSpace The TaskStatus_t structure contains a member to report
 * the stack high water mark of the task being queried.  Calculating the stack
 * high water mark takes a relatively long time, and can make the system
 * temporarily unresponsive - so the xGetFreeStackSpace parameter is provided to
 * allow the high water mark checking to be skipped.  The high watermark value
 * will only be written to the TaskStatus_t structure if xGetFreeStackSpace is
 * not set to pdFALSE;
 *
 * @param eState The TaskStatus_t structure contains a member to report the
 * state of the task being queried.  Obtaining the task state is not as fast as
 * a simple assignment - so the eState parameter is provided to allow the state
 * information to be omitted from the TaskStatus_t structure.  To obtain state
 * information then set eState to eInvalid - otherwise the value passed in
 * eState will be reported as the task state in the TaskStatus_t structure.
 *
 * Example usage:
 * @code{c}
 * void vAFunction( void )
 * {
 * TaskHandle_t xHandle;
 * TaskStatus_t xTaskDetails;
 *
 *  // Obtain the handle of a task from its name.
 *  xHandle = xTaskGetHandle( "Task_Name" );
 *
 *  // Check the handle is not NULL.
 *  configASSERT( xHandle );
 *
 *  // Use the handle to obtain further information about the task.
 *  vTaskGetInfo( xHandle,
 *                &xTaskDetails,
 *                pdTRUE, // Include the high water mark in xTaskDetails.
 *                eInvalid ); // Include the task state in xTaskDetails.
 * }
 * @endcode
 * \defgroup vTaskGetInfo vTaskGetInfo
 * \ingroup TaskCtrl
 */
#if ( configUSE_TRACE_FACILITY == 1 )
    void vTaskGetInfo( TaskHandle_t xTask,
                       TaskStatus_t * pxTaskStatus,
                       BaseType_t xGetFreeStackSpace,
                       eTaskState eState ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority );
 * @endcode
 *
 * INCLUDE_vTaskPrioritySet must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Set the priority of any task.
 *
 * A context switch will occur before the function returns if the priority
 * being set is higher than the currently executing task.
 *
 * @param xTask Handle to the task for which the priority is being set.
 * Passing a NULL handle results in the priority of the calling task being set.
 *
 * @param uxNewPriority The priority to which the task will be set.
 *
 * Example usage:
 * @code{c}
 * void vAFunction( void )
 * {
 * TaskHandle_t xHandle;
 *
 *   // Create a task, storing the handle.
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );
 *
 *   // ...
 *
 *   // Use the handle to raise the priority of the created task.
 *   vTaskPrioritySet( xHandle, tskIDLE_PRIORITY + 1 );
 *
 *   // ...
 *
 *   // Use a NULL handle to raise our priority to the same value.
 *   vTaskPrioritySet( NULL, tskIDLE_PRIORITY + 1 );
 * }
 * @endcode
 * \defgroup vTaskPrioritySet vTaskPrioritySet
 * \ingroup TaskCtrl
 */
void vTaskPrioritySet( TaskHandle_t xTask,
                       UBaseType_t uxNewPriority ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * void vTaskSuspend( TaskHandle_t xTaskToSuspend );
 * @endcode
 *
 * INCLUDE_vTaskSuspend must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Suspend any task.  When suspended a task will never get any microcontroller
 * processing time, no matter what its priority.
 *
 * Calls to vTaskSuspend are not accumulative -
 * i.e. calling vTaskSuspend () twice on the same task still only requires one
 * call to vTaskResume () to ready the suspended task.
 *
 * @param xTaskToSuspend Handle to the task being suspended.  Passing a NULL
 * handle will cause the calling task to be suspended.
 *
 * Example usage:
 * @code{c}
 * void vAFunction( void )
 * {
 * TaskHandle_t xHandle;
 *
 *   // Create a task, storing the handle.
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );
 *
 *   // ...
 *
 *   // Use the handle to suspend the created task.
 *   vTaskSuspend( xHandle );
 *
 *   // ...
 *
 *   // The created task will not run during this period, unless
 *   // another task calls vTaskResume( xHandle ).
 *
 *   //...
 *
 *
 *   // Suspend ourselves.
 *   vTaskSuspend( NULL );
 *
 *   // We cannot get here unless another task calls vTaskResume
 *   // with our handle as the parameter.
 * }
 * @endcode
 * \defgroup vTaskSuspend vTaskSuspend
 * \ingroup TaskCtrl
 */
void vTaskSuspend( TaskHandle_t xTaskToSuspend ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * void vTaskResume( TaskHandle_t xTaskToResume );
 * @endcode
 *
 * INCLUDE_vTaskSuspend must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Resumes a suspended task.
 *
 * A task that has been suspended by one or more calls to vTaskSuspend ()
 * will be made available for running again by a single call to
 * vTaskResume ().
 *
 * @param xTaskToResume Handle to the task being readied.
 *
 * Example usage:
 * @code{c}
 * void vAFunction( void )
 * {
 * TaskHandle_t xHandle;
 *
 *   // Create a task, storing the handle.
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandle );
 *
 *   // ...
 *
 *   // Use the handle to suspend the created task.
 *   vTaskSuspend( xHandle );
 *
 *   // ...
 *
 *   // The created task will not run during this period, unless
 *   // another task calls vTaskResume( xHandle ).
 *
 *   //...
 *
 *
 *   // Resume the suspended task ourselves.
 *   vTaskResume( xHandle );
 *
 *   // The created task will once again get microcontroller processing
 *   // time in accordance with its priority within the system.
 * }
 * @endcode
 * \defgroup vTaskResume vTaskResume
 * \ingroup TaskCtrl
 */
void vTaskResume( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * void xTaskResumeFromISR( TaskHandle_t xTaskToResume );
 * @endcode
 *
 * INCLUDE_xTaskResumeFromISR must be defined as 1 for this function to be
 * available.  See the configuration section for more information.
 *
 * An implementation of vTaskResume() that can be called from within an ISR.
 *
 * A task that has been suspended by one or more calls to vTaskSuspend ()
 * will be made available for running again by a single call to
 * xTaskResumeFromISR ().
 *
 * xTaskResumeFromISR() should not be used to synchronise a task with an
 * interrupt if there is a chance that the interrupt could arrive prior to the
 * task being suspended - as this can lead to interrupts being missed. Use of a
 * semaphore as a synchronisation mechanism would avoid this eventuality.
 *
 * @param xTaskToResume Handle to the task being readied.
 *
 * @return pdTRUE if resuming the task should result in a context switch,
 * otherwise pdFALSE. This is used by the ISR to determine if a context switch
 * may be required following the ISR.
 *
 * \defgroup vTaskResumeFromISR vTaskResumeFromISR
 * \ingroup TaskCtrl
 */
BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume ) PRIVILEGED_FUNCTION;

#if ( configUSE_CORE_AFFINITY == 1 )

/**
 * @brief Sets the core affinity mask for a task.
 *
 * It sets the cores on which a task can run. configUSE_CORE_AFFINITY must
 * be defined as 1 for this function to be available.
 *
 * @param xTask The handle of the task to set the core affinity mask for.
 * Passing NULL will set the core affinity mask for the calling task.
 *
 * @param uxCoreAffinityMask A bitwise value that indicates the cores on
 * which the task can run. Cores are numbered from 0 to configNUMBER_OF_CORES - 1.
 * For example, to ensure that a task can run on core 0 and core 1, set
 * uxCoreAffinityMask to 0x03.
 *
 * Example usage:
 *
 * // The function that creates task.
 * void vAFunction( void )
 * {
 * TaskHandle_t xHandle;
 * UBaseType_t uxCoreAffinityMask;
 *
 *      // Create a task, storing the handle.
 *      xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &( xHandle ) );
 *
 *      // Define the core affinity mask such that this task can only run
 *      // on core 0 and core 2.
 *      uxCoreAffinityMask = ( ( 1 << 0 ) | ( 1 << 2 ) );
 *
 *      //Set the core affinity mask for the task.
 *      vTaskCoreAffinitySet( xHandle, uxCoreAffinityMask );
 * }
 */
    void vTaskCoreAffinitySet( const TaskHandle_t xTask,
                               UBaseType_t uxCoreAffinityMask );
#endif

#if ( ( configNUMBER_OF_CORES > 1 ) && ( configUSE_CORE_AFFINITY == 1 ) )

/**
 * @brief Gets the core affinity mask for a task.
 *
 * configUSE_CORE_AFFINITY must be defined as 1 for this function to be
 * available.
 *
 * @param xTask The handle of the task to get the core affinity mask for.
 * Passing NULL will get the core affinity mask for the calling task.
 *
 * @return The core affinity mask which is a bitwise value that indicates
 * the cores on which a task can run. Cores are numbered from 0 to
 * configNUMBER_OF_CORES - 1. For example, if a task can run on core 0 and core 1,
 * the core affinity mask is 0x03.
 *
 * Example usage:
 *
 * // Task handle of the networking task - it is populated elsewhere.
 * TaskHandle_t xNetworkingTaskHandle;
 *
 * void vAFunction( void )
 * {
 * TaskHandle_t xHandle;
 * UBaseType_t uxNetworkingCoreAffinityMask;
 *
 *     // Create a task, storing the handle.
 *     xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, &( xHandle ) );
 *
 *     //Get the core affinity mask for the networking task.
 *     uxNetworkingCoreAffinityMask = vTaskCoreAffinityGet( xNetworkingTaskHandle );
 *
 *     // Here is a hypothetical scenario, just for the example. Assume that we
 *     // have 2 cores - Core 0 and core 1. We want to pin the application task to
 *     // the core different than the networking task to ensure that the
 *     // application task does not interfere with networking.
 *     if( ( uxNetworkingCoreAffinityMask & ( 1 << 0 ) ) != 0 )
 *     {
 *         // The networking task can run on core 0, pin our task to core 1.
 *         vTaskCoreAffinitySet( xHandle, ( 1 << 1 ) );
 *     }
 *     else
 *     {
 *         // Otherwise, pin our task to core 0.
 *         vTaskCoreAffinitySet( xHandle, ( 1 << 0 ) );
 *     }
 * }
 */
    UBaseType_t vTaskCoreAffinityGet( ConstTaskHandle_t xTask );
#endif

#if ( configUSE_TASK_PREEMPTION_DISABLE == 1 )

/**
 * @brief Disables preemption for a task.
 *
 * @param xTask The handle of the task to disable preemption. Passing NULL
 * disables preemption for the calling task.
 *
 * Example usage:
 *
 * void vTaskCode( void *pvParameters )
 * {
 *     // Silence warnings about unused parameters.
 *     ( void ) pvParameters;
 *
 *     for( ;; )
 *     {
 *         // ... Perform some function here.
 *
 *         // Disable preemption for this task.
 *         vTaskPreemptionDisable( NULL );
 *
 *         // The task will not be preempted when it is executing in this portion ...
 *
 *         // ... until the preemption is enabled again.
 *         vTaskPreemptionEnable( NULL );
 *
 *         // The task can be preempted when it is executing in this portion.
 *     }
 * }
 */
    void vTaskPreemptionDisable( const TaskHandle_t xTask );
#endif

#if ( configUSE_TASK_PREEMPTION_DISABLE == 1 )

/**
 * @brief Enables preemption for a task.
 *
 * @param xTask The handle of the task to enable preemption. Passing NULL
 * enables preemption for the calling task.
 *
 * Example usage:
 *
 * void vTaskCode( void *pvParameters )
 * {
 *     // Silence warnings about unused parameters.
 *     ( void ) pvParameters;
 *
 *     for( ;; )
 *     {
 *         // ... Perform some function here.
 *
 *         // Disable preemption for this task.
 *         vTaskPreemptionDisable( NULL );
 *
 *         // The task will not be preempted when it is executing in this portion ...
 *
 *         // ... until the preemption is enabled again.
 *         vTaskPreemptionEnable( NULL );
 *
 *         // The task can be preempted when it is executing in this portion.
 *     }
 * }
 */
    void vTaskPreemptionEnable( const TaskHandle_t xTask );
#endif

/*-----------------------------------------------------------
* SCHEDULER CONTROL
*----------------------------------------------------------*/

/**
 * task. h
 * @code{c}
 * void vTaskStartScheduler( void );
 * @endcode
 *
 * Starts the real time kernel tick processing.  After calling the kernel
 * has control over which tasks are executed and when.
 *
 * See the demo application file main.c for an example of creating
 * tasks and starting the kernel.
 *
 * Example usage:
 * @code{c}
 * void vAFunction( void )
 * {
 *   // Create at least one task before starting the kernel.
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );
 *
 *   // Start the real time kernel with preemption.
 *   vTaskStartScheduler ();
 *
 *   // Will not get here unless a task calls vTaskEndScheduler ()
 * }
 * @endcode
 *
 * \defgroup vTaskStartScheduler vTaskStartScheduler
 * \ingroup SchedulerControl
 */
void vTaskStartScheduler( void ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * void vTaskEndScheduler( void );
 * @endcode
 *
 * NOTE:  At the time of writing only the x86 real mode port, which runs on a PC
 * in place of DOS, implements this function.
 *
 * Stops the real time kernel tick.  All created tasks will be automatically
 * deleted and multitasking (either preemptive or cooperative) will
 * stop.  Execution then resumes from the point where vTaskStartScheduler ()
 * was called, as if vTaskStartScheduler () had just returned.
 *
 * See the demo application file main. c in the demo/PC directory for an
 * example that uses vTaskEndScheduler ().
 *
 * vTaskEndScheduler () requires an exit function to be defined within the
 * portable layer (see vPortEndScheduler () in port. c for the PC port).  This
 * performs hardware specific operations such as stopping the kernel tick.
 *
 * vTaskEndScheduler () will cause all of the resources allocated by the
 * kernel to be freed - but will not free resources allocated by application
 * tasks.
 *
 * Example usage:
 * @code{c}
 * void vTaskCode( void * pvParameters )
 * {
 *   for( ;; )
 *   {
 *       // Task code goes here.
 *
 *       // At some point we want to end the real time kernel processing
 *       // so call ...
 *       vTaskEndScheduler ();
 *   }
 * }
 *
 * void vAFunction( void )
 * {
 *   // Create at least one task before starting the kernel.
 *   xTaskCreate( vTaskCode, "NAME", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL );
 *
 *   // Start the real time kernel with preemption.
 *   vTaskStartScheduler ();
 *
 *   // Will only get here when the vTaskCode () task has called
 *   // vTaskEndScheduler ().  When we get here we are back to single task
 *   // execution.
 * }
 * @endcode
 *
 * \defgroup vTaskEndScheduler vTaskEndScheduler
 * \ingroup SchedulerControl
 */
void vTaskEndScheduler( void ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * void vTaskSuspendAll( void );
 * @endcode
 *
 * Suspends the scheduler without disabling interrupts.  Context switches will
 * not occur while the scheduler is suspended.
 *
 * After calling vTaskSuspendAll () the calling task will continue to execute
 * without risk of being swapped out until a call to xTaskResumeAll () has been
 * made.
 *
 * API functions that have the potential to cause a context switch (for example,
 * xTaskDelayUntil(), xQueueSend(), etc.) must not be called while the scheduler
 * is suspended.
 *
 * Example usage:
 * @code{c}
 * void vTask1( void * pvParameters )
 * {
 *   for( ;; )
 *   {
 *       // Task code goes here.
 *
 *       // ...
 *
 *       // At some point the task wants to perform a long operation during
 *       // which it does not want to get swapped out.  It cannot use
 *       // taskENTER_CRITICAL ()/taskEXIT_CRITICAL () as the length of the
 *       // operation may cause interrupts to be missed - including the
 *       // ticks.
 *
 *       // Prevent the real time kernel swapping out the task.
 *       vTaskSuspendAll ();
 *
 *       // Perform the operation here.  There is no need to use critical
 *       // sections as we have all the microcontroller processing time.
 *       // During this time interrupts will still operate and the kernel
 *       // tick count will be maintained.
 *
 *       // ...
 *
 *       // The operation is complete.  Restart the kernel.
 *       xTaskResumeAll ();
 *   }
 * }
 * @endcode
 * \defgroup vTaskSuspendAll vTaskSuspendAll
 * \ingroup SchedulerControl
 */
void vTaskSuspendAll( void ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * BaseType_t xTaskResumeAll( void );
 * @endcode
 *
 * Resumes scheduler activity after it was suspended by a call to
 * vTaskSuspendAll().
 *
 * xTaskResumeAll() only resumes the scheduler.  It does not unsuspend tasks
 * that were previously suspended by a call to vTaskSuspend().
 *
 * @return If resuming the scheduler caused a context switch then pdTRUE is
 *         returned, otherwise pdFALSE is returned.
 *
 * Example usage:
 * @code{c}
 * void vTask1( void * pvParameters )
 * {
 *   for( ;; )
 *   {
 *       // Task code goes here.
 *
 *       // ...
 *
 *       // At some point the task wants to perform a long operation during
 *       // which it does not want to get swapped out.  It cannot use
 *       // taskENTER_CRITICAL ()/taskEXIT_CRITICAL () as the length of the
 *       // operation may cause interrupts to be missed - including the
 *       // ticks.
 *
 *       // Prevent the real time kernel swapping out the task.
 *       vTaskSuspendAll ();
 *
 *       // Perform the operation here.  There is no need to use critical
 *       // sections as we have all the microcontroller processing time.
 *       // During this time interrupts will still operate and the real
 *       // time kernel tick count will be maintained.
 *
 *       // ...
 *
 *       // The operation is complete.  Restart the kernel.  We want to force
 *       // a context switch - but there is no point if resuming the scheduler
 *       // caused a context switch already.
 *       if( !xTaskResumeAll () )
 *       {
 *            taskYIELD ();
 *       }
 *   }
 * }
 * @endcode
 * \defgroup xTaskResumeAll xTaskResumeAll
 * \ingroup SchedulerControl
 */
BaseType_t xTaskResumeAll( void ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------
* TASK UTILITIES
*----------------------------------------------------------*/

/**
 * task. h
 * @code{c}
 * TickType_t xTaskGetTickCount( void );
 * @endcode
 *
 * @return The count of ticks since vTaskStartScheduler was called.
 *
 * \defgroup xTaskGetTickCount xTaskGetTickCount
 * \ingroup TaskUtils
 */
TickType_t xTaskGetTickCount( void ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * TickType_t xTaskGetTickCountFromISR( void );
 * @endcode
 *
 * @return The count of ticks since vTaskStartScheduler was called.
 *
 * This is a version of xTaskGetTickCount() that is safe to be called from an
 * ISR - provided that TickType_t is the natural word size of the
 * microcontroller being used or interrupt nesting is either not supported or
 * not being used.
 *
 * \defgroup xTaskGetTickCountFromISR xTaskGetTickCountFromISR
 * \ingroup TaskUtils
 */
TickType_t xTaskGetTickCountFromISR( void ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * uint16_t uxTaskGetNumberOfTasks( void );
 * @endcode
 *
 * @return The number of tasks that the real time kernel is currently managing.
 * This includes all ready, blocked and suspended tasks.  A task that
 * has been deleted but not yet freed by the idle task will also be
 * included in the count.
 *
 * \defgroup uxTaskGetNumberOfTasks uxTaskGetNumberOfTasks
 * \ingroup TaskUtils
 */
UBaseType_t uxTaskGetNumberOfTasks( void ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * char *pcTaskGetName( TaskHandle_t xTaskToQuery );
 * @endcode
 *
 * @return The text (human readable) name of the task referenced by the handle
 * xTaskToQuery.  A task can query its own name by either passing in its own
 * handle, or by setting xTaskToQuery to NULL.
 *
 * \defgroup pcTaskGetName pcTaskGetName
 * \ingroup TaskUtils
 */
char * pcTaskGetName( TaskHandle_t xTaskToQuery ) PRIVILEGED_FUNCTION;

/**
 * task. h
 * @code{c}
 * TaskHandle_t xTaskGetHandle( const char *pcNameToQuery );
 * @endcode
 *
 * NOTE:  This function takes a relatively long time to complete and should be
 * used sparingly.
 *
 * @return The handle of the task that has the human readable name pcNameToQuery.
 * NULL is returned if no matching name is found.  INCLUDE_xTaskGetHandle
 * must be set to 1 in FreeRTOSConfig.h for pcTaskGetHandle() to be available.
 *
 * \defgroup pcTaskGetHandle pcTaskGetHandle
 * \ingroup TaskUtils
 */
#if ( INCLUDE_xTaskGetHandle == 1 )
    TaskHandle_t xTaskGetHandle( const char * pcNameToQuery ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * BaseType_t xTaskGetStaticBuffers( TaskHandle_t xTask,
 *                                   StackType_t ** ppuxStackBuffer,
 *                                   StaticTask_t ** ppxTaskBuffer );
 * @endcode
 *
 * Retrieve pointers to a statically created task's data structure
 * buffer and stack buffer. These are the same buffers that are supplied
 * at the time of creation.
 *
 * @param xTask The task for which to retrieve the buffers.
 *
 * @param ppuxStackBuffer Used to return a pointer to the task's stack buffer.
 *
 * @param ppxTaskBuffer Used to return a pointer to the task's data structure
 * buffer.
 *
 * @return pdTRUE if buffers were retrieved, pdFALSE otherwise.
 *
 * \defgroup xTaskGetStaticBuffers xTaskGetStaticBuffers
 * \ingroup TaskUtils
 */
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    BaseType_t xTaskGetStaticBuffers( TaskHandle_t xTask,
                                      StackType_t ** ppuxStackBuffer,
                                      StaticTask_t ** ppxTaskBuffer ) PRIVILEGED_FUNCTION;
#endif /* configSUPPORT_STATIC_ALLOCATION */

/**
 * task.h
 * @code{c}
 * UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask );
 * @endcode
 *
 * INCLUDE_uxTaskGetStackHighWaterMark must be set to 1 in FreeRTOSConfig.h for
 * this function to be available.
 *
 * Returns the high water mark of the stack associated with xTask.  That is,
 * the minimum free stack space there has been (in words, so on a 32 bit machine
 * a value of 1 means 4 bytes) since the task started.  The smaller the returned
 * number the closer the task has come to overflowing its stack.
 *
 * uxTaskGetStackHighWaterMark() and uxTaskGetStackHighWaterMark2() are the
 * same except for their return type.  Using configSTACK_DEPTH_TYPE allows the
 * user to determine the return type.  It gets around the problem of the value
 * overflowing on 8-bit types without breaking backward compatibility for
 * applications that expect an 8-bit return type.
 *
 * @param xTask Handle of the task associated with the stack to be checked.
 * Set xTask to NULL to check the stack of the calling task.
 *
 * @return The smallest amount of free stack space there has been (in words, so
 * actual spaces on the stack rather than bytes) since the task referenced by
 * xTask was created.
 */
#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )
    UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task.h
 * @code{c}
 * configSTACK_DEPTH_TYPE uxTaskGetStackHighWaterMark2( TaskHandle_t xTask );
 * @endcode
 *
 * INCLUDE_uxTaskGetStackHighWaterMark2 must be set to 1 in FreeRTOSConfig.h for
 * this function to be available.
 *
 * Returns the high water mark of the stack associated with xTask.  That is,
 * the minimum free stack space there has been (in words, so on a 32 bit machine
 * a value of 1 means 4 bytes) since the task started.  The smaller the returned
 * number the closer the task has come to overflowing its stack.
 *
 * uxTaskGetStackHighWaterMark() and uxTaskGetStackHighWaterMark2() are the
 * same except for their return type.  Using configSTACK_DEPTH_TYPE allows the
 * user to determine the return type.  It gets around the problem of the value
 * overflowing on 8-bit types without breaking backward compatibility for
 * applications that expect an 8-bit return type.
 *
 * @param xTask Handle of the task associated with the stack to be checked.
 * Set xTask to NULL to check the stack of the calling task.
 *
 * @return The smallest amount of free stack space there has been (in words, so
 * actual spaces on the stack rather than bytes) since the task referenced by
 * xTask was created.
 */
#if ( INCLUDE_uxTaskGetStackHighWaterMark2 == 1 )
    configSTACK_DEPTH_TYPE uxTaskGetStackHighWaterMark2( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif

/* When using trace macros it is sometimes necessary to include task.h before
 * FreeRTOS.h.  When this is done TaskHookFunction_t will not yet have been defined,
 * so the following two prototypes will cause a compilation error.  This can be
 * fixed by simply guarding against the inclusion of these two prototypes unless
 * they are explicitly required by the configUSE_APPLICATION_TASK_TAG configuration
 * constant. */
#ifdef configUSE_APPLICATION_TASK_TAG
    #if configUSE_APPLICATION_TASK_TAG == 1

/**
 * task.h
 * @code{c}
 * void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction );
 * @endcode
 *
 * Sets pxHookFunction to be the task hook function used by the task xTask.
 * Passing xTask as NULL has the effect of setting the calling tasks hook
 * function.
 */
        void vTaskSetApplicationTaskTag( TaskHandle_t xTask,
                                         TaskHookFunction_t pxHookFunction ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * @code{c}
 * void xTaskGetApplicationTaskTag( TaskHandle_t xTask );
 * @endcode
 *
 * Returns the pxHookFunction value assigned to the task xTask.  Do not
 * call from an interrupt service routine - call
 * xTaskGetApplicationTaskTagFromISR() instead.
 */
        TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * @code{c}
 * void xTaskGetApplicationTaskTagFromISR( TaskHandle_t xTask );
 * @endcode
 *
 * Returns the pxHookFunction value assigned to the task xTask.  Can
 * be called from an interrupt service routine.
 */
        TaskHookFunction_t xTaskGetApplicationTaskTagFromISR( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
    #endif /* configUSE_APPLICATION_TASK_TAG ==1 */
#endif /* ifdef configUSE_APPLICATION_TASK_TAG */

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )

/* Each task contains an array of pointers that is dimensioned by the
 * configNUM_THREAD_LOCAL_STORAGE_POINTERS setting in FreeRTOSConfig.h.  The
 * kernel does not use the pointers itself, so the application writer can use
 * the pointers for any purpose they wish.  The following two functions are
 * used to set and query a pointer respectively. */
    void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet,
                                            BaseType_t xIndex,
                                            void * pvValue ) PRIVILEGED_FUNCTION;
    void * pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery,
                                               BaseType_t xIndex ) PRIVILEGED_FUNCTION;

#endif

#if ( configCHECK_FOR_STACK_OVERFLOW > 0 )

/**
 * task.h
 * @code{c}
 * void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName);
 * @endcode
 *
 * The application stack overflow hook is called when a stack overflow is detected for a task.
 *
 * Details on stack overflow detection can be found here: https://www.FreeRTOS.org/Stacks-and-stack-overflow-checking.html
 *
 * @param xTask the task that just exceeded its stack boundaries.
 * @param pcTaskName A character string containing the name of the offending task.
 */
    /* MISRA Ref 8.6.1 [External linkage] */
    /* More details at: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-86 */
    /* coverity[misra_c_2012_rule_8_6_violation] */
    void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                        char * pcTaskName );

#endif

#if ( configUSE_IDLE_HOOK == 1 )

/**
 * task.h
 * @code{c}
 * void vApplicationIdleHook( void );
 * @endcode
 *
 * The application idle hook is called by the idle task.
 * This allows the application designer to add background functionality without
 * the overhead of a separate task.
 * NOTE: vApplicationIdleHook() MUST NOT, UNDER ANY CIRCUMSTANCES, CALL A FUNCTION THAT MIGHT BLOCK.
 */
    /* MISRA Ref 8.6.1 [External linkage] */
    /* More details at: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-86 */
    /* coverity[misra_c_2012_rule_8_6_violation] */
    void vApplicationIdleHook( void );

#endif


#if  ( configUSE_TICK_HOOK != 0 )

/**
 *  task.h
 * @code{c}
 * void vApplicationTickHook( void );
 * @endcode
 *
 * This hook function is called in the system tick handler after any OS work is completed.
 */
    /* MISRA Ref 8.6.1 [External linkage] */
    /* More details at: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-86 */
    /* coverity[misra_c_2012_rule_8_6_violation] */
    void vApplicationTickHook( void );

#endif

#if ( configSUPPORT_STATIC_ALLOCATION == 1 )

/**
 * task.h
 * @code{c}
 * void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer, StackType_t ** ppxIdleTaskStackBuffer, configSTACK_DEPTH_TYPE * puxIdleTaskStackSize )
 * @endcode
 *
 * This function is used to provide a statically allocated block of memory to FreeRTOS to hold the Idle Task TCB.  This function is required when
 * configSUPPORT_STATIC_ALLOCATION is set.  For more information see this URI: https://www.FreeRTOS.org/a00110.html#configSUPPORT_STATIC_ALLOCATION
 *
 * @param ppxIdleTaskTCBBuffer A handle to a statically allocated TCB buffer
 * @param ppxIdleTaskStackBuffer A handle to a statically allocated Stack buffer for the idle task
 * @param puxIdleTaskStackSize A pointer to the number of elements that will fit in the allocated stack buffer
 */
    void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                        StackType_t ** ppxIdleTaskStackBuffer,
                                        configSTACK_DEPTH_TYPE * puxIdleTaskStackSize );

/**
 * task.h
 * @code{c}
 * void vApplicationGetPassiveIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer, StackType_t ** ppxIdleTaskStackBuffer, configSTACK_DEPTH_TYPE * puxIdleTaskStackSize, BaseType_t xCoreID )
 * @endcode
 *
 * This function is used to provide a statically allocated block of memory to FreeRTOS to hold the Idle Tasks TCB.  This function is required when
 * configSUPPORT_STATIC_ALLOCATION is set.  For more information see this URI: https://www.FreeRTOS.org/a00110.html#configSUPPORT_STATIC_ALLOCATION
 *
 * In the FreeRTOS SMP, there are a total of configNUMBER_OF_CORES idle tasks:
 *  1. 1 Active idle task which does all the housekeeping.
 *  2. ( configNUMBER_OF_CORES - 1 ) Passive idle tasks which do nothing.
 * These idle tasks are created to ensure that each core has an idle task to run when
 * no other task is available to run.
 *
 * The function vApplicationGetPassiveIdleTaskMemory is called with passive idle
 * task index 0, 1 ... ( configNUMBER_OF_CORES - 2 ) to get memory for passive idle
 * tasks.
 *
 * @param ppxIdleTaskTCBBuffer A handle to a statically allocated TCB buffer
 * @param ppxIdleTaskStackBuffer A handle to a statically allocated Stack buffer for the idle task
 * @param puxIdleTaskStackSize A pointer to the number of elements that will fit in the allocated stack buffer
 * @param xPassiveIdleTaskIndex The passive idle task index of the idle task buffer
 */
    #if ( configNUMBER_OF_CORES > 1 )
        void vApplicationGetPassiveIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                                   StackType_t ** ppxIdleTaskStackBuffer,
                                                   configSTACK_DEPTH_TYPE * puxIdleTaskStackSize,
                                                   BaseType_t xPassiveIdleTaskIndex );
    #endif /* #if ( configNUMBER_OF_CORES > 1 ) */
#endif /* if ( configSUPPORT_STATIC_ALLOCATION == 1 ) */

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter );
 * @endcode
 *
 * Calls the hook function associated with xTask.  Passing xTask as NULL has
 * the effect of calling the Running tasks (the calling task) hook function.
 *
 * pvParameter is passed to the hook function for the task to interpret as it
 * wants.  The return value is the value returned by the task hook function
 * registered by the user.
 */
#if ( configUSE_APPLICATION_TASK_TAG == 1 )
    BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask,
                                             void * pvParameter ) PRIVILEGED_FUNCTION;
#endif

/**
 * xTaskGetIdleTaskHandle() is only available if
 * INCLUDE_xTaskGetIdleTaskHandle is set to 1 in FreeRTOSConfig.h.
 *
 * In single-core FreeRTOS, this function simply returns the handle of the idle
 * task. It is not valid to call xTaskGetIdleTaskHandle() before the scheduler
 * has been started.
 *
 * In the FreeRTOS SMP, there are a total of configNUMBER_OF_CORES idle tasks:
 *  1. 1 Active idle task which does all the housekeeping.
 *  2. ( configNUMBER_OF_CORES - 1 ) Passive idle tasks which do nothing.
 * These idle tasks are created to ensure that each core has an idle task to run when
 * no other task is available to run. Call xTaskGetIdleTaskHandle() or
 * xTaskGetIdleTaskHandleForCore() with xCoreID set to 0  to get the Active
 * idle task handle. Call xTaskGetIdleTaskHandleForCore() with xCoreID set to
 * 1,2 ... ( configNUMBER_OF_CORES - 1 ) to get the Passive idle task handles.
 */
#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )
    #if ( configNUMBER_OF_CORES == 1 )
        TaskHandle_t xTaskGetIdleTaskHandle( void ) PRIVILEGED_FUNCTION;
    #endif /* #if ( configNUMBER_OF_CORES == 1 ) */

    TaskHandle_t xTaskGetIdleTaskHandleForCore( BaseType_t xCoreID ) PRIVILEGED_FUNCTION;
#endif /* #if ( INCLUDE_xTaskGetIdleTaskHandle == 1 ) */

/**
 * configUSE_TRACE_FACILITY must be defined as 1 in FreeRTOSConfig.h for
 * uxTaskGetSystemState() to be available.
 *
 * uxTaskGetSystemState() populates an TaskStatus_t structure for each task in
 * the system.  TaskStatus_t structures contain, among other things, members
 * for the task handle, task name, task priority, task state, and total amount
 * of run time consumed by the task.  See the TaskStatus_t structure
 * definition in this file for the full member list.
 *
 * NOTE:  This function is intended for debugging use only as its use results in
 * the scheduler remaining suspended for an extended period.
 *
 * @param pxTaskStatusArray A pointer to an array of TaskStatus_t structures.
 * The array must contain at least one TaskStatus_t structure for each task
 * that is under the control of the RTOS.  The number of tasks under the control
 * of the RTOS can be determined using the uxTaskGetNumberOfTasks() API function.
 *
 * @param uxArraySize The size of the array pointed to by the pxTaskStatusArray
 * parameter.  The size is specified as the number of indexes in the array, or
 * the number of TaskStatus_t structures contained in the array, not by the
 * number of bytes in the array.
 *
 * @param pulTotalRunTime If configGENERATE_RUN_TIME_STATS is set to 1 in
 * FreeRTOSConfig.h then *pulTotalRunTime is set by uxTaskGetSystemState() to the
 * total run time (as defined by the run time stats clock, see
 * https://www.FreeRTOS.org/rtos-run-time-stats.html) since the target booted.
 * pulTotalRunTime can be set to NULL to omit the total run time information.
 *
 * @return The number of TaskStatus_t structures that were populated by
 * uxTaskGetSystemState().  This should equal the number returned by the
 * uxTaskGetNumberOfTasks() API function, but will be zero if the value passed
 * in the uxArraySize parameter was too small.
 *
 * Example usage:
 * @code{c}
 *  // This example demonstrates how a human readable table of run time stats
 *  // information is generated from raw data provided by uxTaskGetSystemState().
 *  // The human readable table is written to pcWriteBuffer
 *  void vTaskGetRunTimeStats( char *pcWriteBuffer )
 *  {
 *  TaskStatus_t *pxTaskStatusArray;
 *  volatile UBaseType_t uxArraySize, x;
 *  configRUN_TIME_COUNTER_TYPE ulTotalRunTime, ulStatsAsPercentage;
 *
 *      // Make sure the write buffer does not contain a string.
 * pcWriteBuffer = 0x00;
 *
 *      // Take a snapshot of the number of tasks in case it changes while this
 *      // function is executing.
 *      uxArraySize = uxTaskGetNumberOfTasks();
 *
 *      // Allocate a TaskStatus_t structure for each task.  An array could be
 *      // allocated statically at compile time.
 *      pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );
 *
 *      if( pxTaskStatusArray != NULL )
 *      {
 *          // Generate raw status information about each task.
 *          uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );
 *
 *          // For percentage calculations.
 *          ulTotalRunTime /= 100U;
 *
 *          // Avoid divide by zero errors.
 *          if( ulTotalRunTime > 0 )
 *          {
 *              // For each populated position in the pxTaskStatusArray array,
 *              // format the raw data as human readable ASCII data
 *              for( x = 0; x < uxArraySize; x++ )
 *              {
 *                  // What percentage of the total run time has the task used?
 *                  // This will always be rounded down to the nearest integer.
 *                  // ulTotalRunTimeDiv100 has already been divided by 100.
 *                  ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalRunTime;
 *
 *                  if( ulStatsAsPercentage > 0U )
 *                  {
 *                      sprintf( pcWriteBuffer, "%s\t\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
 *                  }
 *                  else
 *                  {
 *                      // If the percentage is zero here then the task has
 *                      // consumed less than 1% of the total run time.
 *                      sprintf( pcWriteBuffer, "%s\t\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].pcTaskName, pxTaskStatusArray[ x ].ulRunTimeCounter );
 *                  }
 *
 *                  pcWriteBuffer += strlen( ( char * ) pcWriteBuffer );
 *              }
 *          }
 *
 *          // The array is no longer needed, free the memory it consumes.
 *          vPortFree( pxTaskStatusArray );
 *      }
 *  }
 *  @endcode
 */
#if ( configUSE_TRACE_FACILITY == 1 )
    UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray,
                                      const UBaseType_t uxArraySize,
                                      configRUN_TIME_COUNTER_TYPE * const pulTotalRunTime ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * void vTaskListTasks( char *pcWriteBuffer, size_t uxBufferLength );
 * @endcode
 *
 * configUSE_TRACE_FACILITY and configUSE_STATS_FORMATTING_FUNCTIONS must
 * both be defined as 1 for this function to be available.  See the
 * configuration section of the FreeRTOS.org website for more information.
 *
 * NOTE 1: This function will disable interrupts for its duration.  It is
 * not intended for normal application runtime use but as a debug aid.
 *
 * Lists all the current tasks, along with their current state and stack
 * usage high water mark.
 *
 * Tasks are reported as blocked ('B'), ready ('R'), deleted ('D') or
 * suspended ('S').
 *
 * PLEASE NOTE:
 *
 * This function is provided for convenience only, and is used by many of the
 * demo applications.  Do not consider it to be part of the scheduler.
 *
 * vTaskListTasks() calls uxTaskGetSystemState(), then formats part of the
 * uxTaskGetSystemState() output into a human readable table that displays task:
 * names, states, priority, stack usage and task number.
 * Stack usage specified as the number of unused StackType_t words stack can hold
 * on top of stack - not the number of bytes.
 *
 * vTaskListTasks() has a dependency on the snprintf() C library function that might
 * bloat the code size, use a lot of stack, and provide different results on
 * different platforms.  An alternative, tiny, third party, and limited
 * functionality implementation of snprintf() is provided in many of the
 * FreeRTOS/Demo sub-directories in a file called printf-stdarg.c (note
 * printf-stdarg.c does not provide a full snprintf() implementation!).
 *
 * It is recommended that production systems call uxTaskGetSystemState()
 * directly to get access to raw stats data, rather than indirectly through a
 * call to vTaskListTasks().
 *
 * @param pcWriteBuffer A buffer into which the above mentioned details
 * will be written, in ASCII form.  This buffer is assumed to be large
 * enough to contain the generated report.  Approximately 40 bytes per
 * task should be sufficient.
 *
 * @param uxBufferLength Length of the pcWriteBuffer.
 *
 * \defgroup vTaskListTasks vTaskListTasks
 * \ingroup TaskUtils
 */
#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )
    void vTaskListTasks( char * pcWriteBuffer,
                         size_t uxBufferLength ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * void vTaskList( char *pcWriteBuffer );
 * @endcode
 *
 * configUSE_TRACE_FACILITY and configUSE_STATS_FORMATTING_FUNCTIONS must
 * both be defined as 1 for this function to be available.  See the
 * configuration section of the FreeRTOS.org website for more information.
 *
 * WARN: This function assumes that the pcWriteBuffer is of length
 * configSTATS_BUFFER_MAX_LENGTH. This function is there only for
 * backward compatibility. New applications are recommended to
 * use vTaskListTasks and supply the length of the pcWriteBuffer explicitly.
 *
 * NOTE 1: This function will disable interrupts for its duration.  It is
 * not intended for normal application runtime use but as a debug aid.
 *
 * Lists all the current tasks, along with their current state and stack
 * usage high water mark.
 *
 * Tasks are reported as blocked ('B'), ready ('R'), deleted ('D') or
 * suspended ('S').
 *
 * PLEASE NOTE:
 *
 * This function is provided for convenience only, and is used by many of the
 * demo applications.  Do not consider it to be part of the scheduler.
 *
 * vTaskList() calls uxTaskGetSystemState(), then formats part of the
 * uxTaskGetSystemState() output into a human readable table that displays task:
 * names, states, priority, stack usage and task number.
 * Stack usage specified as the number of unused StackType_t words stack can hold
 * on top of stack - not the number of bytes.
 *
 * vTaskList() has a dependency on the snprintf() C library function that might
 * bloat the code size, use a lot of stack, and provide different results on
 * different platforms.  An alternative, tiny, third party, and limited
 * functionality implementation of snprintf() is provided in many of the
 * FreeRTOS/Demo sub-directories in a file called printf-stdarg.c (note
 * printf-stdarg.c does not provide a full snprintf() implementation!).
 *
 * It is recommended that production systems call uxTaskGetSystemState()
 * directly to get access to raw stats data, rather than indirectly through a
 * call to vTaskList().
 *
 * @param pcWriteBuffer A buffer into which the above mentioned details
 * will be written, in ASCII form.  This buffer is assumed to be large
 * enough to contain the generated report.  Approximately 40 bytes per
 * task should be sufficient.
 *
 * \defgroup vTaskList vTaskList
 * \ingroup TaskUtils
 */
#define vTaskList( pcWriteBuffer )    vTaskListTasks( ( pcWriteBuffer ), configSTATS_BUFFER_MAX_LENGTH )

/**
 * task. h
 * @code{c}
 * void vTaskGetRunTimeStatistics( char *pcWriteBuffer, size_t uxBufferLength );
 * @endcode
 *
 * configGENERATE_RUN_TIME_STATS and configUSE_STATS_FORMATTING_FUNCTIONS
 * must both be defined as 1 for this function to be available.  The application
 * must also then provide definitions for
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and portGET_RUN_TIME_COUNTER_VALUE()
 * to configure a peripheral timer/counter and return the timers current count
 * value respectively.  The counter should be at least 10 times the frequency of
 * the tick count.
 *
 * NOTE 1: This function will disable interrupts for its duration.  It is
 * not intended for normal application runtime use but as a debug aid.
 *
 * Setting configGENERATE_RUN_TIME_STATS to 1 will result in a total
 * accumulated execution time being stored for each task.  The resolution
 * of the accumulated time value depends on the frequency of the timer
 * configured by the portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() macro.
 * Calling vTaskGetRunTimeStatistics() writes the total execution time of each
 * task into a buffer, both as an absolute count value and as a percentage
 * of the total system execution time.
 *
 * NOTE 2:
 *
 * This function is provided for convenience only, and is used by many of the
 * demo applications.  Do not consider it to be part of the scheduler.
 *
 * vTaskGetRunTimeStatistics() calls uxTaskGetSystemState(), then formats part of
 * the uxTaskGetSystemState() output into a human readable table that displays the
 * amount of time each task has spent in the Running state in both absolute and
 * percentage terms.
 *
 * vTaskGetRunTimeStatistics() has a dependency on the snprintf() C library function
 * that might bloat the code size, use a lot of stack, and provide different
 * results on different platforms.  An alternative, tiny, third party, and
 * limited functionality implementation of snprintf() is provided in many of the
 * FreeRTOS/Demo sub-directories in a file called printf-stdarg.c (note
 * printf-stdarg.c does not provide a full snprintf() implementation!).
 *
 * It is recommended that production systems call uxTaskGetSystemState() directly
 * to get access to raw stats data, rather than indirectly through a call to
 * vTaskGetRunTimeStatistics().
 *
 * @param pcWriteBuffer A buffer into which the execution times will be
 * written, in ASCII form.  This buffer is assumed to be large enough to
 * contain the generated report.  Approximately 40 bytes per task should
 * be sufficient.
 *
 * @param uxBufferLength Length of the pcWriteBuffer.
 *
 * \defgroup vTaskGetRunTimeStatistics vTaskGetRunTimeStatistics
 * \ingroup TaskUtils
 */
#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) && ( configUSE_TRACE_FACILITY == 1 ) )
    void vTaskGetRunTimeStatistics( char * pcWriteBuffer,
                                    size_t uxBufferLength ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * void vTaskGetRunTimeStats( char *pcWriteBuffer );
 * @endcode
 *
 * configGENERATE_RUN_TIME_STATS and configUSE_STATS_FORMATTING_FUNCTIONS
 * must both be defined as 1 for this function to be available.  The application
 * must also then provide definitions for
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and portGET_RUN_TIME_COUNTER_VALUE()
 * to configure a peripheral timer/counter and return the timers current count
 * value respectively.  The counter should be at least 10 times the frequency of
 * the tick count.
 *
 * WARN: This function assumes that the pcWriteBuffer is of length
 * configSTATS_BUFFER_MAX_LENGTH. This function is there only for
 * backward compatiblity. New applications are recommended to use
 * vTaskGetRunTimeStatistics and supply the length of the pcWriteBuffer
 * explicitly.
 *
 * NOTE 1: This function will disable interrupts for its duration.  It is
 * not intended for normal application runtime use but as a debug aid.
 *
 * Setting configGENERATE_RUN_TIME_STATS to 1 will result in a total
 * accumulated execution time being stored for each task.  The resolution
 * of the accumulated time value depends on the frequency of the timer
 * configured by the portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() macro.
 * Calling vTaskGetRunTimeStats() writes the total execution time of each
 * task into a buffer, both as an absolute count value and as a percentage
 * of the total system execution time.
 *
 * NOTE 2:
 *
 * This function is provided for convenience only, and is used by many of the
 * demo applications.  Do not consider it to be part of the scheduler.
 *
 * vTaskGetRunTimeStats() calls uxTaskGetSystemState(), then formats part of the
 * uxTaskGetSystemState() output into a human readable table that displays the
 * amount of time each task has spent in the Running state in both absolute and
 * percentage terms.
 *
 * vTaskGetRunTimeStats() has a dependency on the snprintf() C library function
 * that might bloat the code size, use a lot of stack, and provide different
 * results on different platforms.  An alternative, tiny, third party, and
 * limited functionality implementation of snprintf() is provided in many of the
 * FreeRTOS/Demo sub-directories in a file called printf-stdarg.c (note
 * printf-stdarg.c does not provide a full snprintf() implementation!).
 *
 * It is recommended that production systems call uxTaskGetSystemState() directly
 * to get access to raw stats data, rather than indirectly through a call to
 * vTaskGetRunTimeStats().
 *
 * @param pcWriteBuffer A buffer into which the execution times will be
 * written, in ASCII form.  This buffer is assumed to be large enough to
 * contain the generated report.  Approximately 40 bytes per task should
 * be sufficient.
 *
 * \defgroup vTaskGetRunTimeStats vTaskGetRunTimeStats
 * \ingroup TaskUtils
 */
#define vTaskGetRunTimeStats( pcWriteBuffer )    vTaskGetRunTimeStatistics( ( pcWriteBuffer ), configSTATS_BUFFER_MAX_LENGTH )

/**
 * task. h
 * @code{c}
 * configRUN_TIME_COUNTER_TYPE ulTaskGetRunTimeCounter( const TaskHandle_t xTask );
 * configRUN_TIME_COUNTER_TYPE ulTaskGetRunTimePercent( const TaskHandle_t xTask );
 * @endcode
 *
 * configGENERATE_RUN_TIME_STATS must be defined as 1 for these functions to be
 * available.  The application must also then provide definitions for
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and
 * portGET_RUN_TIME_COUNTER_VALUE() to configure a peripheral timer/counter and
 * return the timers current count value respectively.  The counter should be
 * at least 10 times the frequency of the tick count.
 *
 * Setting configGENERATE_RUN_TIME_STATS to 1 will result in a total
 * accumulated execution time being stored for each task.  The resolution
 * of the accumulated time value depends on the frequency of the timer
 * configured by the portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() macro.
 * While uxTaskGetSystemState() and vTaskGetRunTimeStats() writes the total
 * execution time of each task into a buffer, ulTaskGetRunTimeCounter()
 * returns the total execution time of just one task and
 * ulTaskGetRunTimePercent() returns the percentage of the CPU time used by
 * just one task.
 *
 * @return The total run time of the given task or the percentage of the total
 * run time consumed by the given task.  This is the amount of time the task
 * has actually been executing.  The unit of time is dependent on the frequency
 * configured using the portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and
 * portGET_RUN_TIME_COUNTER_VALUE() macros.
 *
 * \defgroup ulTaskGetRunTimeCounter ulTaskGetRunTimeCounter
 * \ingroup TaskUtils
 */
#if ( configGENERATE_RUN_TIME_STATS == 1 )
    configRUN_TIME_COUNTER_TYPE ulTaskGetRunTimeCounter( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
    configRUN_TIME_COUNTER_TYPE ulTaskGetRunTimePercent( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif

/**
 * task. h
 * @code{c}
 * configRUN_TIME_COUNTER_TYPE ulTaskGetIdleRunTimeCounter( void );
 * configRUN_TIME_COUNTER_TYPE ulTaskGetIdleRunTimePercent( void );
 * @endcode
 *
 * configGENERATE_RUN_TIME_STATS must be defined as 1 for these functions to be
 * available.  The application must also then provide definitions for
 * portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and
 * portGET_RUN_TIME_COUNTER_VALUE() to configure a peripheral timer/counter and
 * return the timers current count value respectively.  The counter should be
 * at least 10 times the frequency of the tick count.
 *
 * Setting configGENERATE_RUN_TIME_STATS to 1 will result in a total
 * accumulated execution time being stored for each task.  The resolution
 * of the accumulated time value depends on the frequency of the timer
 * configured by the portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() macro.
 * While uxTaskGetSystemState() and vTaskGetRunTimeStats() writes the total
 * execution time of each task into a buffer, ulTaskGetIdleRunTimeCounter()
 * returns the total execution time of just the idle task and
 * ulTaskGetIdleRunTimePercent() returns the percentage of the CPU time used by
 * just the idle task.
 *
 * Note the amount of idle time is only a good measure of the slack time in a
 * system if there are no other tasks executing at the idle priority, tickless
 * idle is not used, and configIDLE_SHOULD_YIELD is set to 0.
 *
 * @return The total run time of the idle task or the percentage of the total
 * run time consumed by the idle task.  This is the amount of time the
 * idle task has actually been executing.  The unit of time is dependent on the
 * frequency configured using the portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() and
 * portGET_RUN_TIME_COUNTER_VALUE() macros.
 *
 * \defgroup ulTaskGetIdleRunTimeCounter ulTaskGetIdleRunTimeCounter
 * \ingroup TaskUtils
 */
#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( INCLUDE_xTaskGetIdleTaskHandle == 1 ) )
    configRUN_TIME_COUNTER_TYPE ulTaskGetIdleRunTimeCounter( void ) PRIVILEGED_FUNCTION;
    configRUN_TIME_COUNTER_TYPE ulTaskGetIdleRunTimePercent( void ) PRIVILEGED_FUNCTION;
#endif

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskNotifyIndexed( TaskHandle_t xTaskToNotify, UBaseType_t uxIndexToNotify, uint32_t ulValue, eNotifyAction eAction );
 * BaseType_t xTaskNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction );
 * @endcode
 *
 * 详细说明请参考：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 只有当 configUSE_TASK_NOTIFICATIONS 未定义或定义为 1 时，这些函数才会生效。
 *
 * 向指定任务直接发送任务通知，可附带可选的数据值和更新动作。
 *
 * 每个任务都拥有一个私有的“通知值数组”（或称“通知”），数组中的每个元素都是 32 位无符号整数（uint32_t）。
 * 常量 configTASK_NOTIFICATION_ARRAY_ENTRIES 用于设置该数组的索引数量；为保证向后兼容，若未定义此常量，其默认值为 1。
 * 在 FreeRTOS V10.4.0 版本之前，每个任务仅支持一个通知值。
 *
 * 事件可通过“中间对象”发送给任务，这类对象的示例包括队列、信号量、互斥锁和事件组。
 * 而任务通知是一种无需此类中间对象、直接向任务发送事件的通信方式。
 *
 * 向任务发送通知时，可选择执行特定动作（例如更新、覆盖或递增该任务的某个通知值）。
 * 因此，任务通知既可用于向任务传递数据，也可作为轻量级、高性能的二进制信号量或计数信号量使用。
 *
 * 任务可调用 xTaskNotifyWaitIndexed() 或 ulTaskNotifyTakeIndexed()（或它们的非索引版本），
 * [可选地] 阻塞等待通知触发。任务处于阻塞状态时，不会占用任何 CPU 时间。
 *
 * 发送给任务的通知会保持“待处理状态”，直到任务调用 xTaskNotifyWaitIndexed() 或 ulTaskNotifyTakeIndexed()
 *（或它们的非索引版本）将其清除。
 * 若任务在“等待通知”期间（已处于阻塞状态）收到通知，则该任务会自动从阻塞状态移除（解除阻塞），且对应的通知会被清除。
 *
 * **注意** 数组中的每个通知均独立运作——任务一次只能阻塞等待数组中的一个通知，
 * 发送到其他数组索引的通知无法唤醒该任务。
 *
 * 向后兼容说明：
 * 在 FreeRTOS V10.4.0 版本之前，每个任务仅拥有一个“通知值”，所有任务通知 API 均仅对该值进行操作。
 * 由于将单个通知值替换为通知值数组，因此需要一套新的 API 来操作数组中指定索引的通知。
 * xTaskNotify() 是原始 API 函数，为保证向后兼容，它始终操作数组中索引为 0 的通知值。
 * 调用 xTaskNotify() 等效于调用 xTaskNotifyIndexed() 且将 uxIndexToNotify 参数设为 0。
 *
 * @param xTaskToNotify 接收通知的目标任务句柄。
 *                      任务句柄可通过创建任务的 xTaskCreate() API 函数返回；
 *                      当前运行任务的句柄可通过调用 xTaskGetCurrentTaskHandle() 函数获取。
 *
 * @param uxIndexToNotify 目标任务通知值数组中，接收通知的索引。
 *                        uxIndexToNotify 必须小于 configTASK_NOTIFICATION_ARRAY_ENTRIES。
 *                        xTaskNotify() 函数无此参数，它始终向索引 0 发送通知。
 *
 * @param ulValue 可随通知一同发送的数据。数据的用途由 eAction 参数决定。
 *
 * @param eAction 指定通知对任务通知值的更新方式（若需要更新）。
 *                eAction 的有效值如下：
 *
 * eSetBits -
 * 目标通知值与 ulValue 执行按位或操作（即 旧值 |= ulValue）。
 * 在此动作下，xTaskNotifyIndexed() 始终返回 pdPASS。
 *
 * eIncrement -
 * 目标通知值自增 1。ulValue 参数无效，在此动作下 xTaskNotifyIndexed() 始终返回 pdPASS。
 *
 * eSetValueWithOverwrite -
 * 无论接收通知的任务是否已处理同一数组索引的上一个通知（即该索引处已有待处理通知），
 * 都将目标通知值设为 ulValue（覆盖旧值）。在此动作下 xTaskNotifyIndexed() 始终返回 pdPASS。
 *
 * eSetValueWithoutOverwrite -
 * 若接收通知的任务在同一数组索引处**无待处理通知**，则将目标通知值设为 ulValue，且 xTaskNotifyIndexed() 返回 pdPASS；
 * 若接收通知的任务在同一数组索引处**已有待处理通知**，则不执行任何操作，且 xTaskNotifyIndexed() 返回 pdFAIL。
 *
 * eNoAction -
 * 任务在指定数组索引处收到通知，但该索引对应的通知值不发生更新。ulValue 参数无效，在此动作下 xTaskNotifyIndexed() 始终返回 pdPASS。
 *
 * pulPreviousNotificationValue -
 * 可选输出参数，用于传出“通知函数修改前的任务通知值”（即更新前的旧值）。
 *
 * @return 返回值由 eAction 参数决定，具体说明请参考 eAction 参数的描述。
 *
 * \defgroup xTaskNotifyIndexed xTaskNotifyIndexed
 * \ingroup TaskNotifications
 */
// 任务通知底层核心函数（PRIVILEGED_FUNCTION 表示仅内核/特权代码可调用）
BaseType_t xTaskGenericNotify( TaskHandle_t xTaskToNotify,
                               UBaseType_t uxIndexToNotify,
                               uint32_t ulValue,
                               eNotifyAction eAction,
                               uint32_t * pulPreviousNotificationValue ) PRIVILEGED_FUNCTION;

// xTaskNotify：非索引版本宏定义，固定操作索引 0（tskDEFAULT_INDEX_TO_NOTIFY），不返回旧通知值（传NULL）
#define xTaskNotify( xTaskToNotify, ulValue, eAction ) \
    xTaskGenericNotify( ( xTaskToNotify ), ( tskDEFAULT_INDEX_TO_NOTIFY ), ( ulValue ), ( eAction ), NULL )

// xTaskNotifyIndexed：索引版本宏定义，可指定通知索引，不返回旧通知值（传NULL）
#define xTaskNotifyIndexed( xTaskToNotify, uxIndexToNotify, ulValue, eAction ) \
    xTaskGenericNotify( ( xTaskToNotify ), ( uxIndexToNotify ), ( ulValue ), ( eAction ), NULL )

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskNotifyAndQueryIndexed( TaskHandle_t xTaskToNotify, UBaseType_t uxIndexToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotifyValue );
 * BaseType_t xTaskNotifyAndQuery( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotifyValue );
 * @endcode
 *
 * 详细说明请参考：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * xTaskNotifyAndQueryIndexed() 的功能与 xTaskNotifyIndexed() 一致，
 * 额外增加了“通过 pulPreviousNotifyValue 参数返回任务旧通知值”的功能（此处的旧值指函数调用时的值，而非函数返回时的值）。
 *
 * xTaskNotifyAndQuery() 的功能与 xTaskNotify() 一致，
 * 额外增加了“通过 pulPreviousNotifyValue 参数返回任务旧通知值”的功能（此处的旧值指函数调用时的值，而非函数返回时的值）。
 *
 * \defgroup xTaskNotifyAndQueryIndexed xTaskNotifyAndQueryIndexed
 * \ingroup TaskNotifications
 */

// xTaskNotifyAndQuery：非索引版本宏定义，固定操作索引 0，返回旧通知值（传pulPreviousNotificationValue参数）
#define xTaskNotifyAndQuery( xTaskToNotify, ulValue, eAction, pulPreviousNotificationValue ) \
    xTaskGenericNotify( ( xTaskToNotify ), ( tskDEFAULT_INDEX_TO_NOTIFY ), ( ulValue ), ( eAction ), ( pulPreviousNotificationValue ) )

// xTaskNotifyAndQueryIndexed：索引版本宏定义，可指定通知索引，返回旧通知值（传pulPreviousNotificationValue参数）
#define xTaskNotifyAndQueryIndexed( xTaskToNotify, uxIndexToNotify, ulValue, eAction, pulPreviousNotificationValue ) \
    xTaskGenericNotify( ( xTaskToNotify ), ( uxIndexToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotificationValue ) )

/**
 * task. h
 * @code{c}
 * BaseType_t xTaskNotifyIndexedFromISR( TaskHandle_t xTaskToNotify, UBaseType_t uxIndexToNotify, uint32_t ulValue, eNotifyAction eAction, BaseType_t *pxHigherPriorityTaskWoken );
 * BaseType_t xTaskNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, BaseType_t *pxHigherPriorityTaskWoken );
 * @endcode
 *
 * See https://www.FreeRTOS.org/RTOS-task-notifications.html for details.
 *
 * configUSE_TASK_NOTIFICATIONS must be undefined or defined as 1 for these
 * functions to be available.
 *
 * A version of xTaskNotifyIndexed() that can be used from an interrupt service
 * routine (ISR).
 *
 * Each task has a private array of "notification values" (or 'notifications'),
 * each of which is a 32-bit unsigned integer (uint32_t).  The constant
 * configTASK_NOTIFICATION_ARRAY_ENTRIES sets the number of indexes in the
 * array, and (for backward compatibility) defaults to 1 if left undefined.
 * Prior to FreeRTOS V10.4.0 there was only one notification value per task.
 *
 * Events can be sent to a task using an intermediary object.  Examples of such
 * objects are queues, semaphores, mutexes and event groups.  Task notifications
 * are a method of sending an event directly to a task without the need for such
 * an intermediary object.
 *
 * A notification sent to a task can optionally perform an action, such as
 * update, overwrite or increment one of the task's notification values.  In
 * that way task notifications can be used to send data to a task, or be used as
 * light weight and fast binary or counting semaphores.
 *
 * A task can use xTaskNotifyWaitIndexed() to [optionally] block to wait for a
 * notification to be pending, or ulTaskNotifyTakeIndexed() to [optionally] block
 * to wait for a notification value to have a non-zero value.  The task does
 * not consume any CPU time while it is in the Blocked state.
 *
 * A notification sent to a task will remain pending until it is cleared by the
 * task calling xTaskNotifyWaitIndexed() or ulTaskNotifyTakeIndexed() (or their
 * un-indexed equivalents).  If the task was already in the Blocked state to
 * wait for a notification when the notification arrives then the task will
 * automatically be removed from the Blocked state (unblocked) and the
 * notification cleared.
 *
 * **NOTE** Each notification within the array operates independently - a task
 * can only block on one notification within the array at a time and will not be
 * unblocked by a notification sent to any other array index.
 *
 * Backward compatibility information:
 * Prior to FreeRTOS V10.4.0 each task had a single "notification value", and
 * all task notification API functions operated on that value. Replacing the
 * single notification value with an array of notification values necessitated a
 * new set of API functions that could address specific notifications within the
 * array.  xTaskNotifyFromISR() is the original API function, and remains
 * backward compatible by always operating on the notification value at index 0
 * within the array. Calling xTaskNotifyFromISR() is equivalent to calling
 * xTaskNotifyIndexedFromISR() with the uxIndexToNotify parameter set to 0.
 *
 * @param uxIndexToNotify The index within the target task's array of
 * notification values to which the notification is to be sent.  uxIndexToNotify
 * must be less than configTASK_NOTIFICATION_ARRAY_ENTRIES.  xTaskNotifyFromISR()
 * does not have this parameter and always sends notifications to index 0.
 *
 * @param xTaskToNotify The handle of the task being notified.  The handle to a
 * task can be returned from the xTaskCreate() API function used to create the
 * task, and the handle of the currently running task can be obtained by calling
 * xTaskGetCurrentTaskHandle().
 *
 * @param ulValue Data that can be sent with the notification.  How the data is
 * used depends on the value of the eAction parameter.
 *
 * @param eAction Specifies how the notification updates the task's notification
 * value, if at all.  Valid values for eAction are as follows:
 *
 * eSetBits -
 * The task's notification value is bitwise ORed with ulValue.  xTaskNotify()
 * always returns pdPASS in this case.
 *
 * eIncrement -
 * The task's notification value is incremented.  ulValue is not used and
 * xTaskNotify() always returns pdPASS in this case.
 *
 * eSetValueWithOverwrite -
 * The task's notification value is set to the value of ulValue, even if the
 * task being notified had not yet processed the previous notification (the
 * task already had a notification pending).  xTaskNotify() always returns
 * pdPASS in this case.
 *
 * eSetValueWithoutOverwrite -
 * If the task being notified did not already have a notification pending then
 * the task's notification value is set to ulValue and xTaskNotify() will
 * return pdPASS.  If the task being notified already had a notification
 * pending then no action is performed and pdFAIL is returned.
 *
 * eNoAction -
 * The task receives a notification without its notification value being
 * updated.  ulValue is not used and xTaskNotify() always returns pdPASS in
 * this case.
 *
 * @param pxHigherPriorityTaskWoken  xTaskNotifyFromISR() will set
 * *pxHigherPriorityTaskWoken to pdTRUE if sending the notification caused the
 * task to which the notification was sent to leave the Blocked state, and the
 * unblocked task has a priority higher than the currently running task.  If
 * xTaskNotifyFromISR() sets this value to pdTRUE then a context switch should
 * be requested before the interrupt is exited.  How a context switch is
 * requested from an ISR is dependent on the port - see the documentation page
 * for the port in use.
 *
 * @return Dependent on the value of eAction.  See the description of the
 * eAction parameter.
 *
 * \defgroup xTaskNotifyIndexedFromISR xTaskNotifyIndexedFromISR
 * \ingroup TaskNotifications
 */
BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify,
                                      UBaseType_t uxIndexToNotify,
                                      uint32_t ulValue,
                                      eNotifyAction eAction,
                                      uint32_t * pulPreviousNotificationValue,
                                      BaseType_t * pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#define xTaskNotifyFromISR( xTaskToNotify, ulValue, eAction, pxHigherPriorityTaskWoken ) \
    xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( tskDEFAULT_INDEX_TO_NOTIFY ), ( ulValue ), ( eAction ), NULL, ( pxHigherPriorityTaskWoken ) )
#define xTaskNotifyIndexedFromISR( xTaskToNotify, uxIndexToNotify, ulValue, eAction, pxHigherPriorityTaskWoken ) \
    xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( uxIndexToNotify ), ( ulValue ), ( eAction ), NULL, ( pxHigherPriorityTaskWoken ) )

/**
 * task. h
 * @code{c}
 * BaseType_t xTaskNotifyAndQueryIndexedFromISR( TaskHandle_t xTaskToNotify, UBaseType_t uxIndexToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken );
 * BaseType_t xTaskNotifyAndQueryFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken );
 * @endcode
 *
 * See https://www.FreeRTOS.org/RTOS-task-notifications.html for details.
 *
 * xTaskNotifyAndQueryIndexedFromISR() performs the same operation as
 * xTaskNotifyIndexedFromISR() with the addition that it also returns the
 * subject task's prior notification value (the notification value at the time
 * the function is called rather than at the time the function returns) in the
 * additional pulPreviousNotifyValue parameter.
 *
 * xTaskNotifyAndQueryFromISR() performs the same operation as
 * xTaskNotifyFromISR() with the addition that it also returns the subject
 * task's prior notification value (the notification value at the time the
 * function is called rather than at the time the function returns) in the
 * additional pulPreviousNotifyValue parameter.
 *
 * \defgroup xTaskNotifyAndQueryIndexedFromISR xTaskNotifyAndQueryIndexedFromISR
 * \ingroup TaskNotifications
 */
#define xTaskNotifyAndQueryIndexedFromISR( xTaskToNotify, uxIndexToNotify, ulValue, eAction, pulPreviousNotificationValue, pxHigherPriorityTaskWoken ) \
    xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( uxIndexToNotify ), ( ulValue ), ( eAction ), ( pulPreviousNotificationValue ), ( pxHigherPriorityTaskWoken ) )
#define xTaskNotifyAndQueryFromISR( xTaskToNotify, ulValue, eAction, pulPreviousNotificationValue, pxHigherPriorityTaskWoken ) \
    xTaskGenericNotifyFromISR( ( xTaskToNotify ), ( tskDEFAULT_INDEX_TO_NOTIFY ), ( ulValue ), ( eAction ), ( pulPreviousNotificationValue ), ( pxHigherPriorityTaskWoken ) )

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskNotifyWaitIndexed( UBaseType_t uxIndexToWaitOn, uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait );
 *
 * BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait );
 * @endcode
 *
 * 等待任务通知数组中指定索引处的“直接任务通知”变为待处理状态。
 *
 * 详细说明请参考：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 只有当 configUSE_TASK_NOTIFICATIONS 未定义或定义为 1 时，本函数才会生效。
 *
 * 每个任务都拥有一个私有的“通知值数组”（或称“通知”），数组中的每个元素都是 32 位无符号整数（uint32_t）。
 * 常量 configTASK_NOTIFICATION_ARRAY_ENTRIES 用于设置该数组的索引数量；为保证向后兼容，若未定义此常量，其默认值为 1。
 * 在 FreeRTOS V10.4.0 版本之前，每个任务仅支持一个通知值。
 *
 * 事件可通过“中间对象”发送给任务，这类对象的示例包括队列、信号量、互斥锁和事件组。
 * 而任务通知是一种无需此类中间对象、直接向任务发送事件的通信方式。
 *
 * 向任务发送通知时，可选择执行特定动作（例如更新、覆盖或递增该任务的某个通知值）。
 * 因此，任务通知既可用于向任务传递数据，也可作为轻量级、高性能的二进制信号量或计数信号量使用。
 *
 * 发送给任务的通知会保持“待处理状态”，直到任务调用 xTaskNotifyWaitIndexed() 或 ulTaskNotifyTakeIndexed()
 *（或它们的非索引版本）将其清除。
 * 若任务在“等待通知”期间（已处于阻塞状态）收到通知，则该任务会自动从阻塞状态移除（解除阻塞），且对应的通知会被清除。
 *
 * 任务可调用 xTaskNotifyWaitIndexed() [可选地] 阻塞等待通知变为待处理状态，
 * 或调用 ulTaskNotifyTakeIndexed() [可选地] 阻塞等待通知值变为非零值。
 * 任务处于阻塞状态时，不会占用任何 CPU 时间。
 *
 * **注意** 数组中的每个通知均独立运作——任务一次只能阻塞等待数组中的一个通知，
 * 发送到其他数组索引的通知无法唤醒该任务。
 *
 * 向后兼容说明：
 * 在 FreeRTOS V10.4.0 版本之前，每个任务仅拥有一个“通知值”，所有任务通知 API 均仅对该值进行操作。
 * 由于将单个通知值替换为通知值数组，因此需要一套新的 API 来操作数组中指定索引的通知。
 * xTaskNotifyWait() 是原始 API 函数，为保证向后兼容，它始终操作数组中索引为 0 的通知值。
 * 调用 xTaskNotifyWait() 等效于调用 xTaskNotifyWaitIndexed() 且将 uxIndexToWaitOn 参数设为 0。
 *
 * @param uxIndexToWaitOn 调用任务的通知值数组中，当前任务要等待通知的索引。
 *                        uxIndexToWaitOn 必须小于 configTASK_NOTIFICATION_ARRAY_ENTRIES。
 *                        xTaskNotifyWait() 函数无此参数，它始终等待索引 0 处的通知。
 *
 * @param ulBitsToClearOnEntry 在任务检查是否有通知待处理（并在无通知时可选阻塞）之前，
 *                             会先清除调用任务通知值中“ulBitsToClearOnEntry 所置位的那些位”。
 *                             若将 ulBitsToClearOnEntry 设为 ULONG_MAX（需包含 limits.h）或 0xffffffffU（无需包含 limits.h），
 *                             会将任务的通知值重置为 0；若设为 0，则任务的通知值保持不变。
 *
 * @param ulBitsToClearOnExit 若在调用任务退出 xTaskNotifyWait() 函数前，已有通知待处理或收到新通知，
 *                            则会通过 pulNotificationValue 参数传出任务的通知值（参考 xTaskNotify() API 函数）。
 *                            之后，会清除任务通知值中“ulBitsToClearOnExit 所置位的那些位”
 *                            （注意：*pulNotificationValue 的赋值先于任何位的清除操作）。
 *                            若将 ulBitsToClearOnExit 设为 ULONG_MAX（需包含 limits.h）或 0xffffffffUL（无需包含 limits.h），
 *                            会在函数退出前将任务的通知值重置为 0；若设为 0，则函数退出时任务的通知值保持不变
 *                            （此时 pulNotificationValue 传出的值与任务的通知值一致）。
 *
 * @param pulNotificationValue 用于从函数中传出任务的通知值。
 *                             注意：ulBitsToClearOnExit 非零时会触发位清除操作，但该操作不会影响传出的通知值。
 *
 * @param xTicksToWait 若调用 xTaskNotifyWait() 时无通知待处理，任务应在阻塞状态下等待通知的最长时间（以内核节拍为单位）。
 *                     任务处于阻塞状态时，不会占用任何处理时间。
 *                     可使用宏 pdMS_TO_TICKS( 毫秒值 ) 将毫秒级时间转换为节拍数。
 *
 * @return 若收到通知（包括调用 xTaskNotifyWait() 时已存在的待处理通知），则返回 pdPASS；否则返回 pdFAIL。
 *
 * \defgroup xTaskNotifyWaitIndexed xTaskNotifyWaitIndexed
 * \ingroup TaskNotifications
 */

// 任务通知等待底层核心函数（PRIVILEGED_FUNCTION 表示仅内核/特权代码可调用）
BaseType_t xTaskGenericNotifyWait( UBaseType_t uxIndexToWaitOn,
                                   uint32_t ulBitsToClearOnEntry,
                                   uint32_t ulBitsToClearOnExit,
                                   uint32_t * pulNotificationValue,
                                   TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

// xTaskNotifyWait：非索引版本宏定义，固定等待索引 0（tskDEFAULT_INDEX_TO_NOTIFY）
#define xTaskNotifyWait( ulBitsToClearOnEntry, ulBitsToClearOnExit, pulNotificationValue, xTicksToWait ) \
    xTaskGenericNotifyWait( tskDEFAULT_INDEX_TO_NOTIFY, ( ulBitsToClearOnEntry ), ( ulBitsToClearOnExit ), ( pulNotificationValue ), ( xTicksToWait ) )

// xTaskNotifyWaitIndexed：索引版本宏定义，可指定等待的通知索引
#define xTaskNotifyWaitIndexed( uxIndexToWaitOn, ulBitsToClearOnEntry, ulBitsToClearOnExit, pulNotificationValue, xTicksToWait ) \
    xTaskGenericNotifyWait( ( uxIndexToWaitOn ), ( ulBitsToClearOnEntry ), ( ulBitsToClearOnExit ), ( pulNotificationValue ), ( xTicksToWait ) )

/**
 * task. h
 * @code{c}
 * BaseType_t xTaskNotifyGiveIndexed( TaskHandle_t xTaskToNotify, UBaseType_t uxIndexToNotify );
 * BaseType_t xTaskNotifyGive( TaskHandle_t xTaskToNotify );
 * @endcode
 *
 * Sends a direct to task notification to a particular index in the target
 * task's notification array in a manner similar to giving a counting semaphore.
 *
 * See https://www.FreeRTOS.org/RTOS-task-notifications.html for more details.
 *
 * configUSE_TASK_NOTIFICATIONS must be undefined or defined as 1 for these
 * macros to be available.
 *
 * Each task has a private array of "notification values" (or 'notifications'),
 * each of which is a 32-bit unsigned integer (uint32_t).  The constant
 * configTASK_NOTIFICATION_ARRAY_ENTRIES sets the number of indexes in the
 * array, and (for backward compatibility) defaults to 1 if left undefined.
 * Prior to FreeRTOS V10.4.0 there was only one notification value per task.
 *
 * Events can be sent to a task using an intermediary object.  Examples of such
 * objects are queues, semaphores, mutexes and event groups.  Task notifications
 * are a method of sending an event directly to a task without the need for such
 * an intermediary object.
 *
 * A notification sent to a task can optionally perform an action, such as
 * update, overwrite or increment one of the task's notification values.  In
 * that way task notifications can be used to send data to a task, or be used as
 * light weight and fast binary or counting semaphores.
 *
 * xTaskNotifyGiveIndexed() is a helper macro intended for use when task
 * notifications are used as light weight and faster binary or counting
 * semaphore equivalents.  Actual FreeRTOS semaphores are given using the
 * xSemaphoreGive() API function, the equivalent action that instead uses a task
 * notification is xTaskNotifyGiveIndexed().
 *
 * When task notifications are being used as a binary or counting semaphore
 * equivalent then the task being notified should wait for the notification
 * using the ulTaskNotifyTakeIndexed() API function rather than the
 * xTaskNotifyWaitIndexed() API function.
 *
 * **NOTE** Each notification within the array operates independently - a task
 * can only block on one notification within the array at a time and will not be
 * unblocked by a notification sent to any other array index.
 *
 * Backward compatibility information:
 * Prior to FreeRTOS V10.4.0 each task had a single "notification value", and
 * all task notification API functions operated on that value. Replacing the
 * single notification value with an array of notification values necessitated a
 * new set of API functions that could address specific notifications within the
 * array.  xTaskNotifyGive() is the original API function, and remains backward
 * compatible by always operating on the notification value at index 0 in the
 * array. Calling xTaskNotifyGive() is equivalent to calling
 * xTaskNotifyGiveIndexed() with the uxIndexToNotify parameter set to 0.
 *
 * @param xTaskToNotify The handle of the task being notified.  The handle to a
 * task can be returned from the xTaskCreate() API function used to create the
 * task, and the handle of the currently running task can be obtained by calling
 * xTaskGetCurrentTaskHandle().
 *
 * @param uxIndexToNotify The index within the target task's array of
 * notification values to which the notification is to be sent.  uxIndexToNotify
 * must be less than configTASK_NOTIFICATION_ARRAY_ENTRIES.  xTaskNotifyGive()
 * does not have this parameter and always sends notifications to index 0.
 *
 * @return xTaskNotifyGive() is a macro that calls xTaskNotify() with the
 * eAction parameter set to eIncrement - so pdPASS is always returned.
 *
 * \defgroup xTaskNotifyGiveIndexed xTaskNotifyGiveIndexed
 * \ingroup TaskNotifications
 */
#define xTaskNotifyGive( xTaskToNotify ) \
    xTaskGenericNotify( ( xTaskToNotify ), ( tskDEFAULT_INDEX_TO_NOTIFY ), ( 0 ), eIncrement, NULL )
#define xTaskNotifyGiveIndexed( xTaskToNotify, uxIndexToNotify ) \
    xTaskGenericNotify( ( xTaskToNotify ), ( uxIndexToNotify ), ( 0 ), eIncrement, NULL )

/**
 * task. h
 * @code{c}
 * void vTaskNotifyGiveIndexedFromISR( TaskHandle_t xTaskHandle, UBaseType_t uxIndexToNotify, BaseType_t *pxHigherPriorityTaskWoken );
 * void vTaskNotifyGiveFromISR( TaskHandle_t xTaskHandle, BaseType_t *pxHigherPriorityTaskWoken );
 * @endcode
 *
 * A version of xTaskNotifyGiveIndexed() that can be called from an interrupt
 * service routine (ISR).
 *
 * See https://www.FreeRTOS.org/RTOS-task-notifications.html for more details.
 *
 * configUSE_TASK_NOTIFICATIONS must be undefined or defined as 1 for this macro
 * to be available.
 *
 * Each task has a private array of "notification values" (or 'notifications'),
 * each of which is a 32-bit unsigned integer (uint32_t).  The constant
 * configTASK_NOTIFICATION_ARRAY_ENTRIES sets the number of indexes in the
 * array, and (for backward compatibility) defaults to 1 if left undefined.
 * Prior to FreeRTOS V10.4.0 there was only one notification value per task.
 *
 * Events can be sent to a task using an intermediary object.  Examples of such
 * objects are queues, semaphores, mutexes and event groups.  Task notifications
 * are a method of sending an event directly to a task without the need for such
 * an intermediary object.
 *
 * A notification sent to a task can optionally perform an action, such as
 * update, overwrite or increment one of the task's notification values.  In
 * that way task notifications can be used to send data to a task, or be used as
 * light weight and fast binary or counting semaphores.
 *
 * vTaskNotifyGiveIndexedFromISR() is intended for use when task notifications
 * are used as light weight and faster binary or counting semaphore equivalents.
 * Actual FreeRTOS semaphores are given from an ISR using the
 * xSemaphoreGiveFromISR() API function, the equivalent action that instead uses
 * a task notification is vTaskNotifyGiveIndexedFromISR().
 *
 * When task notifications are being used as a binary or counting semaphore
 * equivalent then the task being notified should wait for the notification
 * using the ulTaskNotifyTakeIndexed() API function rather than the
 * xTaskNotifyWaitIndexed() API function.
 *
 * **NOTE** Each notification within the array operates independently - a task
 * can only block on one notification within the array at a time and will not be
 * unblocked by a notification sent to any other array index.
 *
 * Backward compatibility information:
 * Prior to FreeRTOS V10.4.0 each task had a single "notification value", and
 * all task notification API functions operated on that value. Replacing the
 * single notification value with an array of notification values necessitated a
 * new set of API functions that could address specific notifications within the
 * array.  xTaskNotifyFromISR() is the original API function, and remains
 * backward compatible by always operating on the notification value at index 0
 * within the array. Calling xTaskNotifyGiveFromISR() is equivalent to calling
 * xTaskNotifyGiveIndexedFromISR() with the uxIndexToNotify parameter set to 0.
 *
 * @param xTaskToNotify The handle of the task being notified.  The handle to a
 * task can be returned from the xTaskCreate() API function used to create the
 * task, and the handle of the currently running task can be obtained by calling
 * xTaskGetCurrentTaskHandle().
 *
 * @param uxIndexToNotify The index within the target task's array of
 * notification values to which the notification is to be sent.  uxIndexToNotify
 * must be less than configTASK_NOTIFICATION_ARRAY_ENTRIES.
 * xTaskNotifyGiveFromISR() does not have this parameter and always sends
 * notifications to index 0.
 *
 * @param pxHigherPriorityTaskWoken  vTaskNotifyGiveFromISR() will set
 * *pxHigherPriorityTaskWoken to pdTRUE if sending the notification caused the
 * task to which the notification was sent to leave the Blocked state, and the
 * unblocked task has a priority higher than the currently running task.  If
 * vTaskNotifyGiveFromISR() sets this value to pdTRUE then a context switch
 * should be requested before the interrupt is exited.  How a context switch is
 * requested from an ISR is dependent on the port - see the documentation page
 * for the port in use.
 *
 * \defgroup vTaskNotifyGiveIndexedFromISR vTaskNotifyGiveIndexedFromISR
 * \ingroup TaskNotifications
 */
void vTaskGenericNotifyGiveFromISR( TaskHandle_t xTaskToNotify,
                                    UBaseType_t uxIndexToNotify,
                                    BaseType_t * pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;
#define vTaskNotifyGiveFromISR( xTaskToNotify, pxHigherPriorityTaskWoken ) \
    vTaskGenericNotifyGiveFromISR( ( xTaskToNotify ), ( tskDEFAULT_INDEX_TO_NOTIFY ), ( pxHigherPriorityTaskWoken ) )
#define vTaskNotifyGiveIndexedFromISR( xTaskToNotify, uxIndexToNotify, pxHigherPriorityTaskWoken ) \
    vTaskGenericNotifyGiveFromISR( ( xTaskToNotify ), ( uxIndexToNotify ), ( pxHigherPriorityTaskWoken ) )

/**
 * task.h
 * @code{c}
 * uint32_t ulTaskNotifyTakeIndexed( UBaseType_t uxIndexToWaitOn, BaseType_t xClearCountOnExit, TickType_t xTicksToWait );
 *
 * uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait );
 * @endcode
 *
 * 以类似获取计数信号量的方式，等待调用任务通知数组中特定索引处的直接任务通知。
 *
 * 详细说明请参考：https://www.FreeRTOS.org/RTOS-task-notifications.html
 *
 * 只有当 configUSE_TASK_NOTIFICATIONS 未定义或定义为 1 时，本函数才会生效。
 *
 * 每个任务都拥有一个私有的“通知值数组”（或称“通知”），数组中的每个元素都是 32 位无符号整数（uint32_t）。
 * 常量 configTASK_NOTIFICATION_ARRAY_ENTRIES 用于设置该数组的索引数量；为保证向后兼容，若未定义此常量，其默认值为 1。
 * 在 FreeRTOS V10.4.0 版本之前，每个任务仅支持一个通知值。
 *
 * 事件可通过“中间对象”发送给任务，这类对象的示例包括队列、信号量、互斥锁和事件组。
 * 而任务通知是一种无需此类中间对象、直接向任务发送事件的通信方式。
 *
 * 向任务发送通知时，可选择执行特定动作（例如更新、覆盖或递增该任务的某个通知值）。
 * 因此，任务通知既可用于向任务传递数据，也可作为轻量级、高性能的二进制信号量或计数信号量使用。
 *
 * ulTaskNotifyTakeIndexed() 适用于将任务通知用作更快、更轻量的二进制信号量或计数信号量替代方案的场景。
 * FreeRTOS 实际的信号量通过 xSemaphoreTake() API 函数获取，而使用任务通知实现等效操作的函数是 ulTaskNotifyTakeIndexed()。
 *
 * 当任务将其通知值用作二进制信号量或计数信号量时，其他任务应使用 xTaskNotifyGiveIndexed() 宏，
 * 或 eAction 参数设为 eIncrement 的 xTaskNotifyIndex() 函数向其发送通知。
 *
 * ulTaskNotifyTakeIndexed() 退出时，既可以将 uxIndexToWaitOn 参数指定的数组索引处的任务通知值清零
 *（此时通知值的作用类似于二进制信号量），也可以将该通知值递减
 *（此时通知值的作用类似于计数信号量）。
 *
 * 任务可调用 ulTaskNotifyTakeIndexed() [可选地] 阻塞等待通知。
 * 任务处于阻塞状态时，不会占用任何 CPU 时间。
 *
 * xTaskNotifyWaitIndexed() 在有通知待处理时返回，而 ulTaskNotifyTakeIndexed() 在任务的通知值非零时返回。
 *
 * **注意** 数组中的每个通知均独立运作——任务一次只能阻塞等待数组中的一个通知，
 * 发送到其他数组索引的通知无法唤醒该任务。
 *
 * 向后兼容说明：
 * 在 FreeRTOS V10.4.0 版本之前，每个任务仅拥有一个“通知值”，所有任务通知 API 均仅对该值进行操作。
 * 由于将单个通知值替换为通知值数组，因此需要一套新的 API 来操作数组中指定索引的通知。
 * ulTaskNotifyTake() 是原始 API 函数，为保证向后兼容，它始终操作数组中索引为 0 的通知值。
 * 调用 ulTaskNotifyTake() 等效于调用 ulTaskNotifyTakeIndexed() 且将 uxIndexToWaitOn 参数设为 0。
 *
 * @param uxIndexToWaitOn 调用任务的通知值数组中，当前任务要等待通知值非零的索引。
 *                        uxIndexToWaitOn 必须小于 configTASK_NOTIFICATION_ARRAY_ENTRIES。
 *                        xTaskNotifyTake() 函数无此参数，它始终等待索引 0 处的通知。
 *
 * @param xClearCountOnExit 若 xClearCountOnExit 为 pdFALSE，则函数退出时将任务的通知值递减，
 *                          此时通知值的作用类似于计数信号量；
 *                          若 xClearCountOnExit 不为 pdFALSE，则函数退出时将任务的通知值清零，
 *                          此时通知值的作用类似于二进制信号量。
 *
 * @param xTicksToWait 若调用 ulTaskNotifyTake() 时通知值不大于零，任务应在阻塞状态下等待通知值大于零的最长时间（以内核节拍为单位）。
 *                     任务处于阻塞状态时，不会占用任何处理时间。
 *                     可使用宏 pdMS_TO_TICKS( 毫秒值 ) 将毫秒级时间转换为节拍数。
 *
 * @return 任务的通知计数在被清零或递减之前的值（具体请参考 xClearCountOnExit 参数的说明）。
 *
 * \defgroup ulTaskNotifyTakeIndexed ulTaskNotifyTakeIndexed
 * \ingroup TaskNotifications
 */

// 任务通知获取底层核心函数（PRIVILEGED_FUNCTION 表示仅内核/特权代码可调用）
uint32_t ulTaskGenericNotifyTake( UBaseType_t uxIndexToWaitOn,
                                  BaseType_t xClearCountOnExit,
                                  TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

// ulTaskNotifyTake：非索引版本宏定义，固定操作索引 0（tskDEFAULT_INDEX_TO_NOTIFY）
#define ulTaskNotifyTake( xClearCountOnExit, xTicksToWait ) \
    ulTaskGenericNotifyTake( ( tskDEFAULT_INDEX_TO_NOTIFY ), ( xClearCountOnExit ), ( xTicksToWait ) )

// ulTaskNotifyTakeIndexed：索引版本宏定义，可指定操作的通知索引
#define ulTaskNotifyTakeIndexed( uxIndexToWaitOn, xClearCountOnExit, xTicksToWait ) \
    ulTaskGenericNotifyTake( ( uxIndexToWaitOn ), ( xClearCountOnExit ), ( xTicksToWait ) )

/**
 * task. h
 * @code{c}
 * BaseType_t xTaskNotifyStateClearIndexed( TaskHandle_t xTask, UBaseType_t uxIndexToCLear );
 *
 * BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask );
 * @endcode
 *
 * See https://www.FreeRTOS.org/RTOS-task-notifications.html for details.
 *
 * configUSE_TASK_NOTIFICATIONS must be undefined or defined as 1 for these
 * functions to be available.
 *
 * Each task has a private array of "notification values" (or 'notifications'),
 * each of which is a 32-bit unsigned integer (uint32_t).  The constant
 * configTASK_NOTIFICATION_ARRAY_ENTRIES sets the number of indexes in the
 * array, and (for backward compatibility) defaults to 1 if left undefined.
 * Prior to FreeRTOS V10.4.0 there was only one notification value per task.
 *
 * If a notification is sent to an index within the array of notifications then
 * the notification at that index is said to be 'pending' until it is read or
 * explicitly cleared by the receiving task.  xTaskNotifyStateClearIndexed()
 * is the function that clears a pending notification without reading the
 * notification value.  The notification value at the same array index is not
 * altered.  Set xTask to NULL to clear the notification state of the calling
 * task.
 *
 * Backward compatibility information:
 * Prior to FreeRTOS V10.4.0 each task had a single "notification value", and
 * all task notification API functions operated on that value. Replacing the
 * single notification value with an array of notification values necessitated a
 * new set of API functions that could address specific notifications within the
 * array.  xTaskNotifyStateClear() is the original API function, and remains
 * backward compatible by always operating on the notification value at index 0
 * within the array. Calling xTaskNotifyStateClear() is equivalent to calling
 * xTaskNotifyStateClearIndexed() with the uxIndexToNotify parameter set to 0.
 *
 * @param xTask The handle of the RTOS task that will have a notification state
 * cleared.  Set xTask to NULL to clear a notification state in the calling
 * task.  To obtain a task's handle create the task using xTaskCreate() and
 * make use of the pxCreatedTask parameter, or create the task using
 * xTaskCreateStatic() and store the returned value, or use the task's name in
 * a call to xTaskGetHandle().
 *
 * @param uxIndexToClear The index within the target task's array of
 * notification values to act upon.  For example, setting uxIndexToClear to 1
 * will clear the state of the notification at index 1 within the array.
 * uxIndexToClear must be less than configTASK_NOTIFICATION_ARRAY_ENTRIES.
 * ulTaskNotifyStateClear() does not have this parameter and always acts on the
 * notification at index 0.
 *
 * @return pdTRUE if the task's notification state was set to
 * eNotWaitingNotification, otherwise pdFALSE.
 *
 * \defgroup xTaskNotifyStateClearIndexed xTaskNotifyStateClearIndexed
 * \ingroup TaskNotifications
 */
BaseType_t xTaskGenericNotifyStateClear( TaskHandle_t xTask,
                                         UBaseType_t uxIndexToClear ) PRIVILEGED_FUNCTION;
#define xTaskNotifyStateClear( xTask ) \
    xTaskGenericNotifyStateClear( ( xTask ), ( tskDEFAULT_INDEX_TO_NOTIFY ) )
#define xTaskNotifyStateClearIndexed( xTask, uxIndexToClear ) \
    xTaskGenericNotifyStateClear( ( xTask ), ( uxIndexToClear ) )

/**
 * task. h
 * @code{c}
 * uint32_t ulTaskNotifyValueClearIndexed( TaskHandle_t xTask, UBaseType_t uxIndexToClear, uint32_t ulBitsToClear );
 *
 * uint32_t ulTaskNotifyValueClear( TaskHandle_t xTask, uint32_t ulBitsToClear );
 * @endcode
 *
 * See https://www.FreeRTOS.org/RTOS-task-notifications.html for details.
 *
 * configUSE_TASK_NOTIFICATIONS must be undefined or defined as 1 for these
 * functions to be available.
 *
 * Each task has a private array of "notification values" (or 'notifications'),
 * each of which is a 32-bit unsigned integer (uint32_t).  The constant
 * configTASK_NOTIFICATION_ARRAY_ENTRIES sets the number of indexes in the
 * array, and (for backward compatibility) defaults to 1 if left undefined.
 * Prior to FreeRTOS V10.4.0 there was only one notification value per task.
 *
 * ulTaskNotifyValueClearIndexed() clears the bits specified by the
 * ulBitsToClear bit mask in the notification value at array index uxIndexToClear
 * of the task referenced by xTask.
 *
 * Backward compatibility information:
 * Prior to FreeRTOS V10.4.0 each task had a single "notification value", and
 * all task notification API functions operated on that value. Replacing the
 * single notification value with an array of notification values necessitated a
 * new set of API functions that could address specific notifications within the
 * array.  ulTaskNotifyValueClear() is the original API function, and remains
 * backward compatible by always operating on the notification value at index 0
 * within the array. Calling ulTaskNotifyValueClear() is equivalent to calling
 * ulTaskNotifyValueClearIndexed() with the uxIndexToClear parameter set to 0.
 *
 * @param xTask The handle of the RTOS task that will have bits in one of its
 * notification values cleared. Set xTask to NULL to clear bits in a
 * notification value of the calling task.  To obtain a task's handle create the
 * task using xTaskCreate() and make use of the pxCreatedTask parameter, or
 * create the task using xTaskCreateStatic() and store the returned value, or
 * use the task's name in a call to xTaskGetHandle().
 *
 * @param uxIndexToClear The index within the target task's array of
 * notification values in which to clear the bits.  uxIndexToClear
 * must be less than configTASK_NOTIFICATION_ARRAY_ENTRIES.
 * ulTaskNotifyValueClear() does not have this parameter and always clears bits
 * in the notification value at index 0.
 *
 * @param ulBitsToClear Bit mask of the bits to clear in the notification value of
 * xTask. Set a bit to 1 to clear the corresponding bits in the task's notification
 * value. Set ulBitsToClear to 0xffffffff (UINT_MAX on 32-bit architectures) to clear
 * the notification value to 0.  Set ulBitsToClear to 0 to query the task's
 * notification value without clearing any bits.
 *
 *
 * @return The value of the target task's notification value before the bits
 * specified by ulBitsToClear were cleared.
 * \defgroup ulTaskNotifyValueClear ulTaskNotifyValueClear
 * \ingroup TaskNotifications
 */
uint32_t ulTaskGenericNotifyValueClear( TaskHandle_t xTask,
                                        UBaseType_t uxIndexToClear,
                                        uint32_t ulBitsToClear ) PRIVILEGED_FUNCTION;
#define ulTaskNotifyValueClear( xTask, ulBitsToClear ) \
    ulTaskGenericNotifyValueClear( ( xTask ), ( tskDEFAULT_INDEX_TO_NOTIFY ), ( ulBitsToClear ) )
#define ulTaskNotifyValueClearIndexed( xTask, uxIndexToClear, ulBitsToClear ) \
    ulTaskGenericNotifyValueClear( ( xTask ), ( uxIndexToClear ), ( ulBitsToClear ) )

/**
 * task.h
 * @code{c}
 * void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut );
 * @endcode
 *
 * Capture the current time for future use with xTaskCheckForTimeOut().
 *
 * @param pxTimeOut Pointer to a timeout object into which the current time
 * is to be captured.  The captured time includes the tick count and the number
 * of times the tick count has overflowed since the system first booted.
 * \defgroup vTaskSetTimeOutState vTaskSetTimeOutState
 * \ingroup TaskCtrl
 */
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait );
 * @endcode
 *
 * Determines if pxTicksToWait ticks has passed since a time was captured
 * using a call to vTaskSetTimeOutState().  The captured time includes the tick
 * count and the number of times the tick count has overflowed.
 *
 * @param pxTimeOut The time status as captured previously using
 * vTaskSetTimeOutState. If the timeout has not yet occurred, it is updated
 * to reflect the current time status.
 * @param pxTicksToWait The number of ticks to check for timeout i.e. if
 * pxTicksToWait ticks have passed since pxTimeOut was last updated (either by
 * vTaskSetTimeOutState() or xTaskCheckForTimeOut()), the timeout has occurred.
 * If the timeout has not occurred, pxTicksToWait is updated to reflect the
 * number of remaining ticks.
 *
 * @return If timeout has occurred, pdTRUE is returned. Otherwise pdFALSE is
 * returned and pxTicksToWait is updated to reflect the number of remaining
 * ticks.
 *
 * @see https://www.FreeRTOS.org/xTaskCheckForTimeOut.html
 *
 * Example Usage:
 * @code{c}
 *  // Driver library function used to receive uxWantedBytes from an Rx buffer
 *  // that is filled by a UART interrupt. If there are not enough bytes in the
 *  // Rx buffer then the task enters the Blocked state until it is notified that
 *  // more data has been placed into the buffer. If there is still not enough
 *  // data then the task re-enters the Blocked state, and xTaskCheckForTimeOut()
 *  // is used to re-calculate the Block time to ensure the total amount of time
 *  // spent in the Blocked state does not exceed MAX_TIME_TO_WAIT. This
 *  // continues until either the buffer contains at least uxWantedBytes bytes,
 *  // or the total amount of time spent in the Blocked state reaches
 *  // MAX_TIME_TO_WAIT - at which point the task reads however many bytes are
 *  // available up to a maximum of uxWantedBytes.
 *
 *  size_t xUART_Receive( uint8_t *pucBuffer, size_t uxWantedBytes )
 *  {
 *  size_t uxReceived = 0;
 *  TickType_t xTicksToWait = MAX_TIME_TO_WAIT;
 *  TimeOut_t xTimeOut;
 *
 *      // Initialize xTimeOut.  This records the time at which this function
 *      // was entered.
 *      vTaskSetTimeOutState( &xTimeOut );
 *
 *      // Loop until the buffer contains the wanted number of bytes, or a
 *      // timeout occurs.
 *      while( UART_bytes_in_rx_buffer( pxUARTInstance ) < uxWantedBytes )
 *      {
 *          // The buffer didn't contain enough data so this task is going to
 *          // enter the Blocked state. Adjusting xTicksToWait to account for
 *          // any time that has been spent in the Blocked state within this
 *          // function so far to ensure the total amount of time spent in the
 *          // Blocked state does not exceed MAX_TIME_TO_WAIT.
 *          if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) != pdFALSE )
 *          {
 *              //Timed out before the wanted number of bytes were available,
 *              // exit the loop.
 *              break;
 *          }
 *
 *          // Wait for a maximum of xTicksToWait ticks to be notified that the
 *          // receive interrupt has placed more data into the buffer.
 *          ulTaskNotifyTake( pdTRUE, xTicksToWait );
 *      }
 *
 *      // Attempt to read uxWantedBytes from the receive buffer into pucBuffer.
 *      // The actual number of bytes read (which might be less than
 *      // uxWantedBytes) is returned.
 *      uxReceived = UART_read_from_receive_buffer( pxUARTInstance,
 *                                                  pucBuffer,
 *                                                  uxWantedBytes );
 *
 *      return uxReceived;
 *  }
 * @endcode
 * \defgroup xTaskCheckForTimeOut xTaskCheckForTimeOut
 * \ingroup TaskCtrl
 */
BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut,
                                 TickType_t * const pxTicksToWait ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * @code{c}
 * BaseType_t xTaskCatchUpTicks( TickType_t xTicksToCatchUp );
 * @endcode
 *
 * This function corrects the tick count value after the application code has held
 * interrupts disabled for an extended period resulting in tick interrupts having
 * been missed.
 *
 * This function is similar to vTaskStepTick(), however, unlike
 * vTaskStepTick(), xTaskCatchUpTicks() may move the tick count forward past a
 * time at which a task should be removed from the blocked state.  That means
 * tasks may have to be removed from the blocked state as the tick count is
 * moved.
 *
 * @param xTicksToCatchUp The number of tick interrupts that have been missed due to
 * interrupts being disabled.  Its value is not computed automatically, so must be
 * computed by the application writer.
 *
 * @return pdTRUE if moving the tick count forward resulted in a task leaving the
 * blocked state and a context switch being performed.  Otherwise pdFALSE.
 *
 * \defgroup xTaskCatchUpTicks xTaskCatchUpTicks
 * \ingroup TaskCtrl
 */
BaseType_t xTaskCatchUpTicks( TickType_t xTicksToCatchUp ) PRIVILEGED_FUNCTION;

/**
 * task.h
 * @code{c}
 * void vTaskResetState( void );
 * @endcode
 *
 * This function resets the internal state of the task. It must be called by the
 * application before restarting the scheduler.
 *
 * \defgroup vTaskResetState vTaskResetState
 * \ingroup SchedulerControl
 */
void vTaskResetState( void ) PRIVILEGED_FUNCTION;


/*-----------------------------------------------------------
* SCHEDULER INTERNALS AVAILABLE FOR PORTING PURPOSES
*----------------------------------------------------------*/

#if ( configNUMBER_OF_CORES == 1 )
    #define taskYIELD_WITHIN_API()    portYIELD_WITHIN_API()
#else /* #if ( configNUMBER_OF_CORES == 1 ) */
    #define taskYIELD_WITHIN_API()    vTaskYieldWithinAPI()
#endif /* #if ( configNUMBER_OF_CORES == 1 ) */

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS ONLY
 * INTENDED FOR USE WHEN IMPLEMENTING A PORT OF THE SCHEDULER AND IS
 * AN INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * Called from the real time kernel tick (either preemptive or cooperative),
 * this increments the tick count and checks if any tasks that are blocked
 * for a finite period required removing from a blocked list and placing on
 * a ready list.  If a non-zero value is returned then a context switch is
 * required because either:
 *   + A task was removed from a blocked list because its timeout had expired,
 *     or
 *   + Time slicing is in use and there is a task of equal priority to the
 *     currently running task.
 */
BaseType_t xTaskIncrementTick( void ) PRIVILEGED_FUNCTION;

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS AN
 * INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED.
 *
 * Removes the calling task from the ready list and places it both
 * on the list of tasks waiting for a particular event, and the
 * list of delayed tasks.  The task will be removed from both lists
 * and replaced on the ready list should either the event occur (and
 * there be no higher priority tasks waiting on the same event) or
 * the delay period expires.
 *
 * The 'unordered' version replaces the event list item value with the
 * xItemValue value, and inserts the list item at the end of the list.
 *
 * The 'ordered' version uses the existing event list item value (which is the
 * owning task's priority) to insert the list item into the event list in task
 * priority order.
 *
 * @param pxEventList The list containing tasks that are blocked waiting
 * for the event to occur.
 *
 * @param xItemValue The item value to use for the event list item when the
 * event list is not ordered by task priority.
 *
 * @param xTicksToWait The maximum amount of time that the task should wait
 * for the event to occur.  This is specified in kernel ticks, the constant
 * portTICK_PERIOD_MS can be used to convert kernel ticks into a real time
 * period.
 */
void vTaskPlaceOnEventList( List_t * const pxEventList,
                            const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;
void vTaskPlaceOnUnorderedEventList( List_t * pxEventList,
                                     const TickType_t xItemValue,
                                     const TickType_t xTicksToWait ) PRIVILEGED_FUNCTION;

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS AN
 * INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED.
 *
 * This function performs nearly the same function as vTaskPlaceOnEventList().
 * The difference being that this function does not permit tasks to block
 * indefinitely, whereas vTaskPlaceOnEventList() does.
 *
 */
void vTaskPlaceOnEventListRestricted( List_t * const pxEventList,
                                      TickType_t xTicksToWait,
                                      const BaseType_t xWaitIndefinitely ) PRIVILEGED_FUNCTION;

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS AN
 * INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED.
 *
 * Removes a task from both the specified event list and the list of blocked
 * tasks, and places it on a ready queue.
 *
 * xTaskRemoveFromEventList()/vTaskRemoveFromUnorderedEventList() will be called
 * if either an event occurs to unblock a task, or the block timeout period
 * expires.
 *
 * xTaskRemoveFromEventList() is used when the event list is in task priority
 * order.  It removes the list item from the head of the event list as that will
 * have the highest priority owning task of all the tasks on the event list.
 * vTaskRemoveFromUnorderedEventList() is used when the event list is not
 * ordered and the event list items hold something other than the owning tasks
 * priority.  In this case the event list item value is updated to the value
 * passed in the xItemValue parameter.
 *
 * @return pdTRUE if the task being removed has a higher priority than the task
 * making the call, otherwise pdFALSE.
 */
BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList ) PRIVILEGED_FUNCTION;
void vTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem,
                                        const TickType_t xItemValue ) PRIVILEGED_FUNCTION;

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS ONLY
 * INTENDED FOR USE WHEN IMPLEMENTING A PORT OF THE SCHEDULER AND IS
 * AN INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * Sets the pointer to the current TCB to the TCB of the highest priority task
 * that is ready to run.
 */
#if ( configNUMBER_OF_CORES == 1 )
    portDONT_DISCARD void vTaskSwitchContext( void ) PRIVILEGED_FUNCTION;
#else
    portDONT_DISCARD void vTaskSwitchContext( BaseType_t xCoreID ) PRIVILEGED_FUNCTION;
#endif

/*
 * THESE FUNCTIONS MUST NOT BE USED FROM APPLICATION CODE.  THEY ARE USED BY
 * THE EVENT BITS MODULE.
 */
TickType_t uxTaskResetEventItemValue( void ) PRIVILEGED_FUNCTION;

/*
 * Return the handle of the calling task.
 */
TaskHandle_t xTaskGetCurrentTaskHandle( void ) PRIVILEGED_FUNCTION;

/*
 * Return the handle of the task running on specified core.
 */
TaskHandle_t xTaskGetCurrentTaskHandleForCore( BaseType_t xCoreID ) PRIVILEGED_FUNCTION;

/*
 * Shortcut used by the queue implementation to prevent unnecessary call to
 * taskYIELD();
 */
void vTaskMissedYield( void ) PRIVILEGED_FUNCTION;

/*
 * Returns the scheduler state as taskSCHEDULER_RUNNING,
 * taskSCHEDULER_NOT_STARTED or taskSCHEDULER_SUSPENDED.
 */
BaseType_t xTaskGetSchedulerState( void ) PRIVILEGED_FUNCTION;

/*
 * Raises the priority of the mutex holder to that of the calling task should
 * the mutex holder have a priority less than the calling task.
 */
BaseType_t xTaskPriorityInherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;

/*
 * Set the priority of a task back to its proper priority in the case that it
 * inherited a higher priority while it was holding a semaphore.
 */
BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder ) PRIVILEGED_FUNCTION;

/*
 * If a higher priority task attempting to obtain a mutex caused a lower
 * priority task to inherit the higher priority task's priority - but the higher
 * priority task then timed out without obtaining the mutex, then the lower
 * priority task will disinherit the priority again - but only down as far as
 * the highest priority task that is still waiting for the mutex (if there were
 * more than one task waiting for the mutex).
 */
void vTaskPriorityDisinheritAfterTimeout( TaskHandle_t const pxMutexHolder,
                                          UBaseType_t uxHighestPriorityWaitingTask ) PRIVILEGED_FUNCTION;

/*
 * Get the uxTaskNumber assigned to the task referenced by the xTask parameter.
 */
#if ( configUSE_TRACE_FACILITY == 1 )
    UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif

/*
 * Set the uxTaskNumber of the task referenced by the xTask parameter to
 * uxHandle.
 */
#if ( configUSE_TRACE_FACILITY == 1 )
    void vTaskSetTaskNumber( TaskHandle_t xTask,
                             const UBaseType_t uxHandle ) PRIVILEGED_FUNCTION;
#endif

/*
 * Only available when configUSE_TICKLESS_IDLE is set to 1.
 * If tickless mode is being used, or a low power mode is implemented, then
 * the tick interrupt will not execute during idle periods.  When this is the
 * case, the tick count value maintained by the scheduler needs to be kept up
 * to date with the actual execution time by being skipped forward by a time
 * equal to the idle period.
 */
#if ( configUSE_TICKLESS_IDLE != 0 )
    void vTaskStepTick( TickType_t xTicksToJump ) PRIVILEGED_FUNCTION;
#endif

/*
 * Only available when configUSE_TICKLESS_IDLE is set to 1.
 * Provided for use within portSUPPRESS_TICKS_AND_SLEEP() to allow the port
 * specific sleep function to determine if it is ok to proceed with the sleep,
 * and if it is ok to proceed, if it is ok to sleep indefinitely.
 *
 * This function is necessary because portSUPPRESS_TICKS_AND_SLEEP() is only
 * called with the scheduler suspended, not from within a critical section.  It
 * is therefore possible for an interrupt to request a context switch between
 * portSUPPRESS_TICKS_AND_SLEEP() and the low power mode actually being
 * entered.  eTaskConfirmSleepModeStatus() should be called from a short
 * critical section between the timer being stopped and the sleep mode being
 * entered to ensure it is ok to proceed into the sleep mode.
 */
#if ( configUSE_TICKLESS_IDLE != 0 )
    eSleepModeStatus eTaskConfirmSleepModeStatus( void ) PRIVILEGED_FUNCTION;
#endif

/*
 * For internal use only.  Increment the mutex held count when a mutex is
 * taken and return the handle of the task that has taken the mutex.
 */
TaskHandle_t pvTaskIncrementMutexHeldCount( void ) PRIVILEGED_FUNCTION;

/*
 * For internal use only.  Same as vTaskSetTimeOutState(), but without a critical
 * section.
 */
void vTaskInternalSetTimeOutState( TimeOut_t * const pxTimeOut ) PRIVILEGED_FUNCTION;

/*
 * For internal use only. Same as portYIELD_WITHIN_API() in single core FreeRTOS.
 * For SMP this is not defined by the port.
 */
#if ( configNUMBER_OF_CORES > 1 )
    void vTaskYieldWithinAPI( void );
#endif

/*
 * This function is only intended for use when implementing a port of the scheduler
 * and is only available when portCRITICAL_NESTING_IN_TCB is set to 1 or configNUMBER_OF_CORES
 * is greater than 1. This function can be used in the implementation of portENTER_CRITICAL
 * if port wants to maintain critical nesting count in TCB in single core FreeRTOS.
 * It should be used in the implementation of portENTER_CRITICAL if port is running a
 * multiple core FreeRTOS.
 */
#if ( ( portCRITICAL_NESTING_IN_TCB == 1 ) || ( configNUMBER_OF_CORES > 1 ) )
    void vTaskEnterCritical( void );
#endif

/*
 * This function is only intended for use when implementing a port of the scheduler
 * and is only available when portCRITICAL_NESTING_IN_TCB is set to 1 or configNUMBER_OF_CORES
 * is greater than 1. This function can be used in the implementation of portEXIT_CRITICAL
 * if port wants to maintain critical nesting count in TCB in single core FreeRTOS.
 * It should be used in the implementation of portEXIT_CRITICAL if port is running a
 * multiple core FreeRTOS.
 */
#if ( ( portCRITICAL_NESTING_IN_TCB == 1 ) || ( configNUMBER_OF_CORES > 1 ) )
    void vTaskExitCritical( void );
#endif

/*
 * This function is only intended for use when implementing a port of the scheduler
 * and is only available when configNUMBER_OF_CORES is greater than 1. This function
 * should be used in the implementation of portENTER_CRITICAL_FROM_ISR if port is
 * running a multiple core FreeRTOS.
 */
#if ( configNUMBER_OF_CORES > 1 )
    UBaseType_t vTaskEnterCriticalFromISR( void );
#endif

/*
 * This function is only intended for use when implementing a port of the scheduler
 * and is only available when configNUMBER_OF_CORES is greater than 1. This function
 * should be used in the implementation of portEXIT_CRITICAL_FROM_ISR if port is
 * running a multiple core FreeRTOS.
 */
#if ( configNUMBER_OF_CORES > 1 )
    void vTaskExitCriticalFromISR( UBaseType_t uxSavedInterruptStatus );
#endif

#if ( portUSING_MPU_WRAPPERS == 1 )

/*
 * For internal use only.  Get MPU settings associated with a task.
 */
    xMPU_SETTINGS * xTaskGetMPUSettings( TaskHandle_t xTask ) PRIVILEGED_FUNCTION;

#endif /* portUSING_MPU_WRAPPERS */


#if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) && ( configENABLE_ACCESS_CONTROL_LIST == 1 ) )

/*
 * For internal use only.  Grant/Revoke a task's access to a kernel object.
 */
    void vGrantAccessToKernelObject( TaskHandle_t xExternalTaskHandle,
                                     int32_t lExternalKernelObjectHandle ) PRIVILEGED_FUNCTION;
    void vRevokeAccessToKernelObject( TaskHandle_t xExternalTaskHandle,
                                      int32_t lExternalKernelObjectHandle ) PRIVILEGED_FUNCTION;

/*
 * For internal use only.  Grant/Revoke a task's access to a kernel object.
 */
    void vPortGrantAccessToKernelObject( TaskHandle_t xInternalTaskHandle,
                                         int32_t lInternalIndexOfKernelObject ) PRIVILEGED_FUNCTION;
    void vPortRevokeAccessToKernelObject( TaskHandle_t xInternalTaskHandle,
                                          int32_t lInternalIndexOfKernelObject ) PRIVILEGED_FUNCTION;

#endif /* #if ( ( portUSING_MPU_WRAPPERS == 1 ) && ( configUSE_MPU_WRAPPERS_V1 == 0 ) && ( configENABLE_ACCESS_CONTROL_LIST == 1 ) ) */

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */
#endif /* INC_TASK_H */
