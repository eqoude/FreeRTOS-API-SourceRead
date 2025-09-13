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

/*******************************************************************************
 * This file provides an example FreeRTOSConfig.h header file, inclusive of an
 * abbreviated explanation of each configuration item.  Online and reference
 * documentation provides more information.
 * https://www.freertos.org/a00110.html
 *
 * Constant values enclosed in square brackets ('[' and ']') must be completed
 * before this file will build.
 *
 * Use the FreeRTOSConfig.h supplied with the RTOS port in use rather than this
 * generic file, if one is available.
 ******************************************************************************/

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/******************************************************************************/
/* 与硬件描述相关的定义。********************************************************/
/******************************************************************************/

/* 在大多数情况下，configCPU_CLOCK_HZ 必须设置为驱动驱动内核周期性滴答中断的外设时钟频率。
 * 默认值设为 20MHz，与 QEMU 演示配置匹配。
 * 你的应用程序肯定需要不同的值，请正确设置。
 * 该值通常（但不总是）等于系统主时钟频率。*/
#define configCPU_CLOCK_HZ    ( ( unsigned long ) 20000000 )

/* configSYSTICK_CLOCK_HZ 是仅针对 ARM Cortex-M 移植版本的可选参数。
 *
 * 默认情况下，ARM Cortex-M 移植版本通过 Cortex-M 的 SysTick 定时器生成 RTOS 滴答中断。
 * 大多数 Cortex-M MCU 的 SysTick 定时器与 MCU 本身运行在相同频率下——这种情况下，
 * 不需要 configSYSTICK_CLOCK_HZ，应保持未定义。
 * 如果 SysTick 定时器的时钟频率与 MCU 内核频率不同，则按常规设置 configCPU_CLOCK_HZ 为 MCU 时钟频率，
 * 并将 configSYSTICK_CLOCK_HZ 设置为 SysTick 时钟频率。未定义时不生效。
 * 默认值为未定义（已注释）。如果需要此值，请取消注释并设置为合适的值。*/

/*
 #define configSYSTICK_CLOCK_HZ                  [平台特定值]
 */

/******************************************************************************/
/* Scheduling behaviour related definitions. **********************************/
/******************************************************************************/

/* configTICK_RATE_HZ 用于设置节拍中断的频率（单位为Hz），通常根据 configCPU_CLOCK_HZ 的值计算得出。 */
#define configTICK_RATE_HZ                         100

/* 将 configUSE_PREEMPTION 设为 1 以使用抢占式调度；设为 0 以使用协作式调度。
 * 详见：https://www.freertos.org/single-core-amp-smp-rtos-scheduling.html */
#define configUSE_PREEMPTION                       1

/* 将 configUSE_TIME_SLICING 设为 1 时，调度器会在每个滴答中断时，
 * 在同等优先级的就绪态任务之间进行切换；设为 0 时，调度器不会仅因滴答中断
 * 而在就绪态任务之间切换。
 * 详见：https://freertos.org/single-core-amp-smp-rtos-scheduling.html */
#define configUSE_TIME_SLICING                     0

/* 将 configUSE_PORT_OPTIMISED_TASK_SELECTION 设为 1 时，
 * 会使用针对目标硬件指令集优化的算法（通常是利用“前导零计数”汇编指令）
 * 选择下一个要运行的任务；设为 0 时，会使用适用于所有 FreeRTOS 移植版本的
 * 通用 C 语言算法。并非所有 FreeRTOS 移植版本都支持此选项。
 * 若未定义，默认值为 0。 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    0

/* 将 configUSE_TICKLESS_IDLE 设为 1 以使用低功耗无滴答模式；设为 0 时，
 * 滴答中断会始终运行。并非所有 FreeRTOS 移植版本都支持无滴答模式。
 * 详见：https://www.freertos.org/low-power-tickless-rtos.html
 * 若未定义，默认值为 0。 */
#define configUSE_TICKLESS_IDLE                    0

/* configMAX_PRIORITIES 设置可用的任务优先级数量。
 * 任务可被分配的优先级范围是 0 到 (configMAX_PRIORITIES - 1)。
 * 0 是最低优先级。*/
#define configMAX_PRIORITIES                       5

/* configMINIMAL_STACK_SIZE 定义空闲任务（Idle task）使用的栈大小
 * （以字为单位，而非字节！）。内核不会将此常量用于其他用途。
 * 演示应用程序使用此常量，使演示程序在不同硬件架构间具有一定的可移植性。*/
#define configMINIMAL_STACK_SIZE                   128

/* configMAX_TASK_NAME_LEN 设置任务的人类可读名称的最大长度（以字符为单位）。
 * 包含 NULL 终止符。*/
#define configMAX_TASK_NAME_LEN                    16

/* 时间以“节拍（ticks）”为单位进行计量——节拍数是自RTOS内核启动启动以来节拍中断执行的次数。
 * 节拍计数存储在TickType_t类型的变量中。
 *
 * configTICK_TYPE_WIDTH_IN_BITS控制TickType_t的类型（以及因此决定的位宽）：
 *
 * 将configTICK_TYPE_WIDTH_IN_BITS定义为TICK_TYPE_WIDTH_16_BITS，会使
 * TickType_t被定义（通过typedef）为无符号16位类型。
 *
 * 将configTICK_TYPE_WIDTH_IN_BITS定义为TICK_TYPE_WIDTH_32_BITS，会使
 * TickType_t被定义（通过typedef）为无符号32位类型。
 *
 * 将configTICK_TYPE_WIDTH_IN_BITS定义为TICK_TYPE_WIDTH_64_BITS，会使
 * TickType_t被定义（通过typedef）为无符号64位类型。 */
#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_64_BITS

/* Set configIDLE_SHOULD_YIELD to 1 to have the Idle task yield to an
 * application task if there is an Idle priority (priority 0) application task that
 * can run.  Set to 0 to have the Idle task use all of its timeslice.  Default to 1
 * if left undefined. */
#define configIDLE_SHOULD_YIELD                    1

/* Each task has an array of task notifications.
 * configTASK_NOTIFICATION_ARRAY_ENTRIES sets the number of indexes in the array.
 * See https://www.freertos.org/RTOS-task-notifications.html  Defaults to 1 if
 * left undefined. */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES      1

/* configQUEUE_REGISTRY_SIZE sets the maximum number of queues and semaphores
 * that can be referenced from the queue registry.  Only required when using a
 * kernel aware debugger.  Defaults to 0 if left undefined. */
#define configQUEUE_REGISTRY_SIZE                  0

/* Set configENABLE_BACKWARD_COMPATIBILITY to 1 to map function names and
 * datatypes from old version of FreeRTOS to their latest equivalent.  Defaults to
 * 1 if left undefined. */
#define configENABLE_BACKWARD_COMPATIBILITY        0

/* Each task has its own array of pointers that can be used as thread local
 * storage.  configNUM_THREAD_LOCAL_STORAGE_POINTERS set the number of indexes in
 * the array.  See https://www.freertos.org/thread-local-storage-pointers.html
 * Defaults to 0 if left undefined. */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS    0

/* When configUSE_MINI_LIST_ITEM is set to 0, MiniListItem_t and ListItem_t are
 * both the same. When configUSE_MINI_LIST_ITEM is set to 1, MiniListItem_t contains
 * 3 fewer fields than ListItem_t which saves some RAM at the cost of violating
 * strict aliasing rules which some compilers depend on for optimization. Defaults
 * to 1 if left undefined. */
#define configUSE_MINI_LIST_ITEM                   1

/* Sets the type used by the parameter to xTaskCreate() that specifies the stack
 * size of the task being created.  The same type is used to return information
 * about stack usage in various other API calls.  Defaults to size_t if left
 * undefined. */
#define configSTACK_DEPTH_TYPE                     size_t

/* configMESSAGE_BUFFER_LENGTH_TYPE sets the type used to store the length of
 * each message written to a FreeRTOS message buffer (the length is also written to
 * the message buffer.  Defaults to size_t if left undefined - but that may waste
 * space if messages never go above a length that could be held in a uint8_t. */
#define configMESSAGE_BUFFER_LENGTH_TYPE           size_t

/* If configHEAP_CLEAR_MEMORY_ON_FREE is set to 1, then blocks of memory allocated
 * using pvPortMalloc() will be cleared (i.e. set to zero) when freed using
 * vPortFree(). Defaults to 0 if left undefined. */
#define configHEAP_CLEAR_MEMORY_ON_FREE            1

/* vTaskList and vTaskGetRunTimeStats APIs take a buffer as a parameter and assume
 * that the length of the buffer is configSTATS_BUFFER_MAX_LENGTH. Defaults to
 * 0xFFFF if left undefined.
 * New applications are recommended to use vTaskListTasks and
 * vTaskGetRunTimeStatistics APIs instead and supply the length of the buffer
 * explicitly to avoid memory corruption. */
#define configSTATS_BUFFER_MAX_LENGTH              0xFFFF

/* Set configUSE_NEWLIB_REENTRANT to 1 to have a newlib reent structure
 * allocated for each task.  Set to 0 to not support newlib reent structures.
 * Default to 0 if left undefined.
 *
 * Note Newlib support has been included by popular demand, but is not used or
 * tested by the FreeRTOS maintainers themselves. FreeRTOS is not responsible for
 * resulting newlib operation. User must be familiar with newlib and must provide
 * system-wide implementations of the necessary stubs. Note that (at the time of
 * writing) the current newlib design implements a system-wide malloc() that must
 * be provided with locks. */
#define configUSE_NEWLIB_REENTRANT                 0

/******************************************************************************/
/* Software timer related definitions. ****************************************/
/******************************************************************************/

/* Set configUSE_TIMERS to 1 to include software timer functionality in the
 * build.  Set to 0 to exclude software timer functionality from the build.  The
 * FreeRTOS/source/timers.c source file must be included in the build if
 * configUSE_TIMERS is set to 1.  Default to 0 if left undefined.  See
 * https://www.freertos.org/RTOS-software-timer.html. */
#define configUSE_TIMERS                1

/* configTIMER_TASK_PRIORITY sets the priority used by the timer task.  Only
 * used if configUSE_TIMERS is set to 1.  The timer task is a standard FreeRTOS
 * task, so its priority is set like any other task.  See
 * https://www.freertos.org/RTOS-software-timer-service-daemon-task.html  Only used
 * if configUSE_TIMERS is set to 1. */
#define configTIMER_TASK_PRIORITY       ( configMAX_PRIORITIES - 1 )

/* configTIMER_TASK_STACK_DEPTH sets the size of the stack allocated to the
 * timer task (in words, not in bytes!).  The timer task is a standard FreeRTOS
 * task.  See https://www.freertos.org/RTOS-software-timer-service-daemon-task.html
 * Only used if configUSE_TIMERS is set to 1. */
#define configTIMER_TASK_STACK_DEPTH    configMINIMAL_STACK_SIZE

/* configTIMER_QUEUE_LENGTH sets the length of the queue (the number of discrete
 * items the queue can hold) used to send commands to the timer task.  See
 * https://www.freertos.org/RTOS-software-timer-service-daemon-task.html  Only used
 * if configUSE_TIMERS is set to 1. */
#define configTIMER_QUEUE_LENGTH        10

/******************************************************************************/
/* Event Group related definitions. *******************************************/
/******************************************************************************/

/* Set configUSE_EVENT_GROUPS to 1 to include event group functionality in the
 * build. Set to 0 to exclude event group functionality from the build. The
 * FreeRTOS/source/event_groups.c source file must be included in the build if
 * configUSE_EVENT_GROUPS is set to 1. Defaults to 1 if left undefined. */

#define configUSE_EVENT_GROUPS    1

/******************************************************************************/
/* Stream Buffer related definitions. *****************************************/
/******************************************************************************/

/* Set configUSE_STREAM_BUFFERS to 1 to include stream buffer functionality in
 * the build. Set to 0 to exclude event group functionality from the build. The
 * FreeRTOS/source/stream_buffer.c source file must be included in the build if
 * configUSE_STREAM_BUFFERS is set to 1. Defaults to 1 if left undefined. */

#define configUSE_STREAM_BUFFERS    1

/******************************************************************************/
/* 内存分配相关定义 ***********************************************************/
/******************************************************************************/

/* 将 configSUPPORT_STATIC_ALLOCATION 设置为 1 时，将包含使用静态分配内存
 * 创建 FreeRTOS 对象（任务、队列等）的 API 函数。设置为 0 时，将从编译中
 * 排除创建静态分配对象的功能。如果未定义，默认值为 0。
 * 详见：https://www.freertos.org/Static_Vs_Dynamic_Memory_Allocation.html */
#define configSUPPORT_STATIC_ALLOCATION              1

/* 将 configSUPPORT_DYNAMIC_ALLOCATION 设置为 1 时，将包含使用动态分配内存
 * 创建 FreeRTOS 对象（任务、队列等）的 API 函数。设置为 0 时，将从编译中
 * 排除创建动态分配对象的功能。如果未定义，默认值为 1。
 * 详见：https://www.freertos.org/Static_Vs_Dynamic_Memory_Allocation.html */
#define configSUPPORT_DYNAMIC_ALLOCATION             1

/* 当包含 heap_1.c、heap_2.c 或 heap_4.c 时，设置 FreeRTOS 堆的总大小（以字节为单位）。
 * 此值默认为 4096 字节，但必须根据每个应用程序进行调整。注意堆将出现在 .bss 段中。
 * 详见：https://www.freertos.org/a00111.html */
#define configTOTAL_HEAP_SIZE                        4096

/* 将 configAPPLICATION_ALLOCATED_HEAP 设置为 1 时，由应用程序分配用作 FreeRTOS 堆的数组。
 * 设置为 0 时，由链接器分配用作 FreeRTOS 堆的数组。如果未定义，默认值为 0。 */
#define configAPPLICATION_ALLOCATED_HEAP             0

/* 将 configSTACK_ALLOCATION_FROM_SEPARATE_HEAP 设置为 1 时，任务堆栈将从
 * FreeRTOS 堆以外的地方分配。这在需要确保堆栈位于快速内存中时非常有用。
 * 设置为 0 时，任务堆栈将来自标准 FreeRTOS 堆。如果设置为 1，应用程序编写者
 * 必须提供 pvPortMallocStack() 和 vPortFreeStack() 的实现。如果未定义，默认值为 0。 */
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP    0

/* 将 configENABLE_HEAP_PROTECTOR 设置为 1 时，将在 heap_4.c 和 heap_5.c 中启用
 * 边界检查和内部堆块指针混淆，以帮助捕获指针损坏。如果未定义，默认值为 0。 */
#define configENABLE_HEAP_PROTECTOR                  0

/******************************************************************************/
/* Interrupt nesting behaviour configuration. *********************************/
/******************************************************************************/

/* configKERNEL_INTERRUPT_PRIORITY sets the priority of the tick and context
 * switch performing interrupts.  Not supported by all FreeRTOS ports.  See
 * https://www.freertos.org/RTOS-Cortex-M3-M4.html for information specific to
 * ARM Cortex-M devices. */
#define configKERNEL_INTERRUPT_PRIORITY          0

/* configMAX_SYSCALL_INTERRUPT_PRIORITY sets the interrupt priority above which
 * FreeRTOS API calls must not be made.  Interrupts above this priority are never
 * disabled, so never delayed by RTOS activity.  The default value is set to the
 * highest interrupt priority (0).  Not supported by all FreeRTOS ports.
 * See https://www.freertos.org/RTOS-Cortex-M3-M4.html for information specific to
 * ARM Cortex-M devices. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     0

/* Another name for configMAX_SYSCALL_INTERRUPT_PRIORITY - the name used depends
 * on the FreeRTOS port. */
#define configMAX_API_CALL_INTERRUPT_PRIORITY    0

/******************************************************************************/
/* Hook and callback function related definitions. ****************************/
/******************************************************************************/

/* Set the following configUSE_* constants to 1 to include the named hook
 * functionality in the build.  Set to 0 to exclude the hook functionality from the
 * build.  The application writer is responsible for providing the hook function
 * for any set to 1.  See https://www.freertos.org/a00016.html. */
#define configUSE_IDLE_HOOK                   0
#define configUSE_TICK_HOOK                   0
#define configUSE_MALLOC_FAILED_HOOK          0
#define configUSE_DAEMON_TASK_STARTUP_HOOK    0

/* Set configUSE_SB_COMPLETED_CALLBACK to 1 to have send and receive completed
 * callbacks for each instance of a stream buffer or message buffer. When the
 * option is set to 1, APIs xStreamBufferCreateWithCallback() and
 * xStreamBufferCreateStaticWithCallback() (and likewise APIs for message
 * buffer) can be used to create a stream buffer or message buffer instance
 * with application provided callbacks. Defaults to 0 if left undefined. */
#define configUSE_SB_COMPLETED_CALLBACK       0

/* Set configCHECK_FOR_STACK_OVERFLOW to 1 or 2 for FreeRTOS to check for a
 * stack overflow at the time of a context switch.  Set to 0 to not look for a
 * stack overflow.  If configCHECK_FOR_STACK_OVERFLOW is 1 then the check only
 * looks for the stack pointer being out of bounds when a task's context is saved
 * to its stack - this is fast but somewhat ineffective.  If
 * configCHECK_FOR_STACK_OVERFLOW is 2 then the check looks for a pattern written
 * to the end of a task's stack having been overwritten.  This is slower, but will
 * catch most (but not all) stack overflows.  The application writer must provide
 * the stack overflow callback when configCHECK_FOR_STACK_OVERFLOW is set to 1.
 * See https://www.freertos.org/Stacks-and-stack-overflow-checking.html  Defaults
 * to 0 if left undefined. */
#define configCHECK_FOR_STACK_OVERFLOW        2

/******************************************************************************/
/* Run time and task stats gathering related definitions. *********************/
/******************************************************************************/

/* Set configGENERATE_RUN_TIME_STATS to 1 to have FreeRTOS collect data on the
 * processing time used by each task.  Set to 0 to not collect the data.  The
 * application writer needs to provide a clock source if set to 1.  Defaults to 0
 * if left undefined.  See https://www.freertos.org/rtos-run-time-stats.html. */
#define configGENERATE_RUN_TIME_STATS           0

/* Set configUSE_TRACE_FACILITY to include additional task structure members
 * are used by trace and visualisation functions and tools.  Set to 0 to exclude
 * the additional information from the structures. Defaults to 0 if left
 * undefined. */
#define configUSE_TRACE_FACILITY                0

/* Set to 1 to include the vTaskList() and vTaskGetRunTimeStats() functions in
 * the build.  Set to 0 to exclude these functions from the build.  These two
 * functions introduce a dependency on string formatting functions that would
 * otherwise not exist - hence they are kept separate.  Defaults to 0 if left
 * undefined. */
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/******************************************************************************/
/* Co-routine related definitions. ********************************************/
/******************************************************************************/

/* Set configUSE_CO_ROUTINES to 1 to include co-routine functionality in the
 * build, or 0 to omit co-routine functionality from the build. To include
 * co-routines, croutine.c must be included in the project. Defaults to 0 if left
 * undefined. */
#define configUSE_CO_ROUTINES              0

/* configMAX_CO_ROUTINE_PRIORITIES defines the number of priorities available
 * to the application co-routines. Any number of co-routines can share the same
 * priority. Defaults to 0 if left undefined. */
#define configMAX_CO_ROUTINE_PRIORITIES    1

/******************************************************************************/
/* Debugging assistance. ******************************************************/
/******************************************************************************/

/* configASSERT() has the same semantics as the standard C assert().  It can
 * either be defined to take an action when the assertion fails, or not defined
 * at all (i.e. comment out or delete the definitions) to completely remove
 * assertions.  configASSERT() can be defined to anything you want, for example
 * you can call a function if an assert fails that passes the filename and line
 * number of the failing assert (for example, "vAssertCalled( __FILE__, __LINE__ )"
 * or it can simple disable interrupts and sit in a loop to halt all execution
 * on the failing line for viewing in a debugger. */
#define configASSERT( x )         \
    if( ( x ) == 0 )              \
    {                             \
        taskDISABLE_INTERRUPTS(); \
        for( ; ; )                \
        ;                         \
    }

/******************************************************************************/
/* FreeRTOS MPU specific definitions. *****************************************/
/******************************************************************************/

/* If configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS is set to 1 then
 * the application writer can provide functions that execute in privileged mode.
 * See: https://www.freertos.org/a00110.html#configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS
 * Defaults to 0 if left undefined.  Only used by the FreeRTOS Cortex-M MPU ports,
 * not the standard ARMv7-M Cortex-M port. */
#define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS    0

/* Set configTOTAL_MPU_REGIONS to the number of MPU regions implemented on your
 * target hardware.  Normally 8 or 16.  Only used by the FreeRTOS Cortex-M MPU
 * ports, not the standard ARMv7-M Cortex-M port.  Defaults to 8 if left
 * undefined. */
#define configTOTAL_MPU_REGIONS                                   8

/* configTEX_S_C_B_FLASH allows application writers to override the default
 * values for the for TEX, Shareable (S), Cacheable (C) and Bufferable (B) bits for
 * the MPU region covering Flash.  Defaults to 0x07UL (which means TEX=000, S=1,
 * C=1, B=1) if left undefined.  Only used by the FreeRTOS Cortex-M MPU ports, not
 * the standard ARMv7-M Cortex-M port. */
#define configTEX_S_C_B_FLASH                                     0x07UL

/* configTEX_S_C_B_SRAM allows application writers to override the default
 * values for the for TEX, Shareable (S), Cacheable (C) and Bufferable (B) bits for
 * the MPU region covering RAM. Defaults to 0x07UL (which means TEX=000, S=1, C=1,
 * B=1) if left undefined.  Only used by the FreeRTOS Cortex-M MPU ports, not
 * the standard ARMv7-M Cortex-M port. */
#define configTEX_S_C_B_SRAM                                      0x07UL

/* Set configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY to 0 to prevent any privilege
 * escalations originating from outside of the kernel code itself.  Set to 1 to
 * allow application tasks to raise privilege.  Defaults to 1 if left undefined.
 * Only used by the FreeRTOS Cortex-M MPU ports, not the standard ARMv7-M Cortex-M
 * port. */
#define configENFORCE_SYSTEM_CALLS_FROM_KERNEL_ONLY               1

/* Set configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS to 1 to allow unprivileged
 * tasks enter critical sections (effectively mask interrupts). Set to 0 to
 * prevent unprivileged tasks entering critical sections.  Defaults to 1 if left
 * undefined.  Only used by the FreeRTOS Cortex-M MPU ports, not the standard
 * ARMv7-M Cortex-M port. */
#define configALLOW_UNPRIVILEGED_CRITICAL_SECTIONS                0

/* FreeRTOS Kernel version 10.6.0 introduced a new v2 MPU wrapper, namely
 * mpu_wrappers_v2.c. Set configUSE_MPU_WRAPPERS_V1 to 0 to use the new v2 MPU
 * wrapper. Set configUSE_MPU_WRAPPERS_V1 to 1 to use the old v1 MPU wrapper
 * (mpu_wrappers.c). Defaults to 0 if left undefined. */
#define configUSE_MPU_WRAPPERS_V1                                 0

/* When using the v2 MPU wrapper, set configPROTECTED_KERNEL_OBJECT_POOL_SIZE to
 * the total number of kernel objects, which includes tasks, queues, semaphores,
 * mutexes, event groups, timers, stream buffers and message buffers, in your
 * application. The application will not be able to have more than
 * configPROTECTED_KERNEL_OBJECT_POOL_SIZE kernel objects at any point of
 * time. */
#define configPROTECTED_KERNEL_OBJECT_POOL_SIZE                   10

/* When using the v2 MPU wrapper, set configSYSTEM_CALL_STACK_SIZE to the size
 * of the system call stack in words. Each task has a statically allocated
 * memory buffer of this size which is used as the stack to execute system
 * calls. For example, if configSYSTEM_CALL_STACK_SIZE is defined as 128 and
 * there are 10 tasks in the application, the total amount of memory used for
 * system call stacks is 128 * 10 = 1280 words. */
#define configSYSTEM_CALL_STACK_SIZE                              128

/* When using the v2 MPU wrapper, set configENABLE_ACCESS_CONTROL_LIST to 1 to
 * enable Access Control List (ACL) feature. When ACL is enabled, an
 * unprivileged task by default does not have access to any kernel object other
 * than itself. The application writer needs to explicitly grant the
 * unprivileged task access to the kernel objects it needs using the APIs
 * provided for the same. Defaults to 0 if left undefined. */
#define configENABLE_ACCESS_CONTROL_LIST                          1

/******************************************************************************/
/* SMP( Symmetric MultiProcessing ) Specific Configuration definitions. *******/
/******************************************************************************/

/* Set configNUMBER_OF_CORES to the number of available processor cores. Defaults
 * to 1 if left undefined. */

/*
 #define configNUMBER_OF_CORES                     [Num of available cores]
 */

/* When using SMP (i.e. configNUMBER_OF_CORES is greater than one), set
 * configRUN_MULTIPLE_PRIORITIES to 0 to allow multiple tasks to run
 * simultaneously only if they do not have equal priority, thereby maintaining
 * the paradigm of a lower priority task never running if a higher priority task
 * is able to run. If configRUN_MULTIPLE_PRIORITIES is set to 1, multiple tasks
 * with different priorities may run simultaneously - so a higher and lower
 * priority task may run on different cores at the same time. */
#define configRUN_MULTIPLE_PRIORITIES             0

/* When using SMP (i.e. configNUMBER_OF_CORES is greater than one), set
 * configUSE_CORE_AFFINITY to 1 to enable core affinity feature. When core
 * affinity feature is enabled, the vTaskCoreAffinitySet and vTaskCoreAffinityGet
 * APIs can be used to set and retrieve which cores a task can run on. If
 * configUSE_CORE_AFFINITY is set to 0 then the FreeRTOS scheduler is free to
 * run any task on any available core. */
#define configUSE_CORE_AFFINITY                   0

/* When using SMP with core affinity feature enabled, set
 * configTASK_DEFAULT_CORE_AFFINITY to change the default core affinity mask for
 * tasks created without an affinity mask specified. Setting the define to 1 would
 * make such tasks run on core 0 and setting it to (1 << portGET_CORE_ID()) would
 * make such tasks run on the current core. This config value is useful, if
 * swapping tasks between cores is not supported (e.g. Tricore) or if legacy code
 * should be controlled. Defaults to tskNO_AFFINITY if left undefined. */
#define configTASK_DEFAULT_CORE_AFFINITY          tskNO_AFFINITY

/* When using SMP (i.e. configNUMBER_OF_CORES is greater than one), if
 * configUSE_TASK_PREEMPTION_DISABLE is set to 1, individual tasks can be set to
 * either pre-emptive or co-operative mode using the vTaskPreemptionDisable and
 * vTaskPreemptionEnable APIs. */
#define configUSE_TASK_PREEMPTION_DISABLE         0

/* When using SMP (i.e. configNUMBER_OF_CORES is greater than one), set
 * configUSE_PASSIVE_IDLE_HOOK to 1 to allow the application writer to use
 * the passive idle task hook to add background functionality without the overhead
 * of a separate task. Defaults to 0 if left undefined. */
#define configUSE_PASSIVE_IDLE_HOOK               0

/* When using SMP (i.e. configNUMBER_OF_CORES is greater than one),
 * configTIMER_SERVICE_TASK_CORE_AFFINITY allows the application writer to set
 * the core affinity of the RTOS Daemon/Timer Service task. Defaults to
 * tskNO_AFFINITY if left undefined. */
#define configTIMER_SERVICE_TASK_CORE_AFFINITY    tskNO_AFFINITY


/******************************************************************************/
/* ARMv8-M secure side port related definitions. ******************************/
/******************************************************************************/

/* secureconfigMAX_SECURE_CONTEXTS define the maximum number of tasks that can
 *  call into the secure side of an ARMv8-M chip.  Not used by any other ports. */
#define secureconfigMAX_SECURE_CONTEXTS        5

/* Defines the kernel provided implementation of
 * vApplicationGetIdleTaskMemory() and vApplicationGetTimerTaskMemory()
 * to provide the memory that is used by the Idle task and Timer task respectively.
 * The application can provide it's own implementation of
 * vApplicationGetIdleTaskMemory() and vApplicationGetTimerTaskMemory() by
 * setting configKERNEL_PROVIDED_STATIC_MEMORY to 0 or leaving it undefined. */
#define configKERNEL_PROVIDED_STATIC_MEMORY    1

/******************************************************************************/
/* ARMv8-M port Specific Configuration definitions. ***************************/
/******************************************************************************/

/* Set configENABLE_TRUSTZONE to 1 when running FreeRTOS on the non-secure side
 * to enable the TrustZone support in FreeRTOS ARMv8-M ports which allows the
 * non-secure FreeRTOS tasks to call the (non-secure callable) functions
 * exported from secure side. */
#define configENABLE_TRUSTZONE            1

/* If the application writer does not want to use TrustZone, but the hardware does
 * not support disabling TrustZone then the entire application (including the FreeRTOS
 * scheduler) can run on the secure side without ever branching to the non-secure side.
 * To do that, in addition to setting configENABLE_TRUSTZONE to 0, also set
 * configRUN_FREERTOS_SECURE_ONLY to 1. */
#define configRUN_FREERTOS_SECURE_ONLY    1

/* Set configENABLE_MPU to 1 to enable the Memory Protection Unit (MPU), or 0
 * to leave the Memory Protection Unit disabled. */
#define configENABLE_MPU                  1

/* Set configENABLE_FPU to 1 to enable the Floating Point Unit (FPU), or 0
 * to leave the Floating Point Unit disabled. */
#define configENABLE_FPU                  1

/* Set configENABLE_MVE to 1 to enable the M-Profile Vector Extension (MVE) support,
 * or 0 to leave the MVE support disabled. This option is only applicable to Cortex-M55
 * and Cortex-M85 ports as M-Profile Vector Extension (MVE) is available only on
 * these architectures. configENABLE_MVE must be left undefined, or defined to 0
 * for the Cortex-M23,Cortex-M33 and Cortex-M35P ports. */
#define configENABLE_MVE                  1

/******************************************************************************/
/* ARMv7-M and ARMv8-M port Specific Configuration definitions. ***************/
/******************************************************************************/

/* Set configCHECK_HANDLER_INSTALLATION to 1 to enable additional asserts to verify
 * that the application has correctly installed FreeRTOS interrupt handlers.
 *
 * An application can install FreeRTOS interrupt handlers in one of the following ways:
 *   1. Direct Routing  -  Install the functions vPortSVCHandler and xPortPendSVHandler
 *                         for SVC call and PendSV interrupts respectively.
 *   2. Indirect Routing - Install separate handlers for SVC call and PendSV
 *                         interrupts and route program control from those handlers
 *                         to vPortSVCHandler and xPortPendSVHandler functions.
 * The applications that use Indirect Routing must set configCHECK_HANDLER_INSTALLATION to 0.
 *
 * Defaults to 1 if left undefined. */
#define configCHECK_HANDLER_INSTALLATION    1

/******************************************************************************/
/* Definitions that include or exclude functionality. *************************/
/******************************************************************************/

/* Set the following configUSE_* constants to 1 to include the named feature in
 * the build, or 0 to exclude the named feature from the build. */
#define configUSE_TASK_NOTIFICATIONS           1
#define configUSE_MUTEXES                      1
#define configUSE_RECURSIVE_MUTEXES            1
#define configUSE_COUNTING_SEMAPHORES          1
#define configUSE_QUEUE_SETS                   0
#define configUSE_APPLICATION_TASK_TAG         0

/* Set the following INCLUDE_* constants to 1 to incldue the named API function,
 * or 0 to exclude the named API function.  Most linkers will remove unused
 * functions even when the constant is 1. */
#define INCLUDE_vTaskPrioritySet               1
#define INCLUDE_uxTaskPriorityGet              1
#define INCLUDE_vTaskDelete                    1
#define INCLUDE_vTaskSuspend                   1
#define INCLUDE_xResumeFromISR                 1
#define INCLUDE_vTaskDelayUntil                1
#define INCLUDE_vTaskDelay                     1
#define INCLUDE_xTaskGetSchedulerState         1
#define INCLUDE_xTaskGetCurrentTaskHandle      1
#define INCLUDE_uxTaskGetStackHighWaterMark    0
#define INCLUDE_xTaskGetIdleTaskHandle         0
#define INCLUDE_eTaskGetState                  0
#define INCLUDE_xEventGroupSetBitFromISR       1
#define INCLUDE_xTimerPendFunctionCall         0
#define INCLUDE_xTaskAbortDelay                0
#define INCLUDE_xTaskGetHandle                 0
#define INCLUDE_xTaskResumeFromISR             1

#endif /* FREERTOS_CONFIG_H */
