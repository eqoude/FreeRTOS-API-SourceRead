# FreeRTOS之数据类型和编码风格

本文介绍基本来自于FreeeRTOS官方文档

### 数据类型

每个FreeRTOS移植都有一个独特的portmacro.h头文件，该文件包含（除其他内容外）两种特定于移植的数据类型定义：TickType_t和BaseType_t。

在 ARM CM3（ARM Cortex-M3）移植版本中定义如下

```c
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
```

*TickType_t* 受宏 *configTICK_TYPE_WIDTH_IN_BITS*  影响，想要设置该宏，查看下方的编码风格可知该宏在*FreeRTOSConfig.h* 中设置

### 编码风格

1. 变量名 变量以其类型作为前缀： “c”表示char “s”表示int16_t (short) “l”表示int32_t (long) “x”表示BaseType_t以及任何其他非标准类型（结构体、任务句柄、队列句柄等） 如果一个变量是无符号的，它也会加上前缀“u”。如果一个变量是指针，它也会加上前缀“p”。例如，类型为uint8_t的变量将加上前缀“uc”，而类型为指向字符的指针（char *）的变量将加上前缀“pc”。
2. 函数名 函数会以前缀形式标注其返回值类型和定义所在的文件，例如： vTaskPrioritySet()返回值为空，定义在tasks.c文件中 xQueueReceive()返回一个BaseType_t类型的变量，在queue.c中定义 pvTimerGetTimerID()函数返回一个指向void类型的指针，定义在timers.c文件中 文件作用域（私有）函数以“prv”为前缀。
3. 宏名称 大多数宏以大写字母书写，并以前缀小写字母表示宏的定义位置。

| 类别（示例）                         | 头文件                    |
| ------------------------------------ | ------------------------- |
| port（例如，portMAX_DELAY）          | portable.h or portmacro.h |
| task（例如，taskENTER_CRITICAL ()）  | task.h                    |
| pd（例如，pdTRUE）                   | projdefs.h                |
| config（例如，configUSE_PREEMPTION） | FreeRTOSConfig.h          |
| err（例如，errQUEUE_FULL）           | projdefs.h                |