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

#ifndef PROJDEFS_H
#define PROJDEFS_H

/*
 * 定义任务函数必须遵循的函数原型。在本文件中定义，以确保在包含portable.h之前该类型是已知的。
 */
typedef void (* TaskFunction_t)( void * arg );

/* 将毫秒时间转换为节拍时间。如果此处的定义不适合你的应用，可以在FreeRTOSConfig.h中
 * 定义同名宏来覆盖此宏。 */
#ifndef pdMS_TO_TICKS
    #define pdMS_TO_TICKS( xTimeInMs )    ( ( TickType_t ) ( ( ( uint64_t ) ( xTimeInMs ) * ( uint64_t ) configTICK_RATE_HZ ) / ( uint64_t ) 1000U ) )
#endif

/* 将节拍时间转换为毫秒时间。如果此处的定义不适合你的应用，可以在FreeRTOSConfig.h中
 * 定义同名宏来覆盖此宏。 */
#ifndef pdTICKS_TO_MS
    #define pdTICKS_TO_MS( xTimeInTicks )    ( ( TickType_t ) ( ( ( uint64_t ) ( xTimeInTicks ) * ( uint64_t ) 1000U ) / ( uint64_t ) configTICK_RATE_HZ ) )
#endif

#define pdFALSE                                  ( ( BaseType_t ) 0 )  // 假（有符号类型）
#define pdTRUE                                   ( ( BaseType_t ) 1 )  // 真（有符号类型）
#define pdFALSE_SIGNED                           ( ( BaseType_t ) 0 )  // 假（显式有符号类型）
#define pdTRUE_SIGNED                            ( ( BaseType_t ) 1 )  // 真（显式有符号类型）
#define pdFALSE_UNSIGNED                         ( ( UBaseType_t ) 0 )  // 假（无符号类型）
#define pdTRUE_UNSIGNED                          ( ( UBaseType_t ) 1 )  // 真（无符号类型）

#define pdPASS                                   ( pdTRUE )  // 成功
#define pdFAIL                                   ( pdFALSE )  // 失败
#define errQUEUE_EMPTY                           ( ( BaseType_t ) 0 )  // 队列空
#define errQUEUE_FULL                            ( ( BaseType_t ) 0 )  // 队列满

/* FreeRTOS 错误定义。 */
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY    ( -1 )  // 无法分配所需内存
#define errQUEUE_BLOCKED                         ( -4 )  // 队列操作被阻塞
#define errQUEUE_YIELD                           ( -5 )  // 队列操作触发任务切换

/* 用于基本数据损坏检查的宏。 */
#ifndef configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES
    #define configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES    0
#endif

#if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS )
    #define pdINTEGRITY_CHECK_VALUE    0x5a5a  // 16位节拍类型的完整性检查值
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
    #define pdINTEGRITY_CHECK_VALUE    0x5a5a5a5aUL  // 32位节拍类型的完整性检查值
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_64_BITS )
    #define pdINTEGRITY_CHECK_VALUE    0x5a5a5a5a5a5a5a5aULL  // 64位节拍类型的完整性检查值
#else
    #error configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width.  // 错误：configTICK_TYPE_WIDTH_IN_BITS设置为不支持的节拍类型宽度
#endif

/* 以下 errno 值由 FreeRTOS+ 组件使用，而非 FreeRTOS 本身。 */
#define pdFREERTOS_ERRNO_NONE             0   /* 无错误 */
#define pdFREERTOS_ERRNO_ENOENT           2   /* 没有 such 文件或目录 */
#define pdFREERTOS_ERRNO_EINTR            4   /* 系统调用被中断 */
#define pdFREERTOS_ERRNO_EIO              5   /* I/O 错误 */
#define pdFREERTOS_ERRNO_ENXIO            6   /* 没有 such 设备或地址 */
#define pdFREERTOS_ERRNO_EBADF            9   /* 错误的文件编号 */
#define pdFREERTOS_ERRNO_EAGAIN           11  /* 没有更多进程 */
#define pdFREERTOS_ERRNO_EWOULDBLOCK      11  /* 操作将阻塞 */
#define pdFREERTOS_ERRNO_ENOMEM           12  /* 内存不足 */
#define pdFREERTOS_ERRNO_EACCES           13  /* 权限被拒绝 */
#define pdFREERTOS_ERRNO_EFAULT           14  /* 错误的地址 */
#define pdFREERTOS_ERRNO_EBUSY            16  /* 挂载设备忙 */
#define pdFREERTOS_ERRNO_EEXIST           17  /* 文件已存在 */
#define pdFREERTOS_ERRNO_EXDEV            18  /* 跨设备链接 */
#define pdFREERTOS_ERRNO_ENODEV           19  /* 没有 such 设备 */
#define pdFREERTOS_ERRNO_ENOTDIR          20  /* 不是目录 */
#define pdFREERTOS_ERRNO_EISDIR           21  /* 是目录 */
#define pdFREERTOS_ERRNO_EINVAL           22  /* 无效的参数 */
#define pdFREERTOS_ERRNO_ENOSPC           28  /* 设备上没有剩余空间 */
#define pdFREERTOS_ERRNO_ESPIPE           29  /* 非法的查找 */
#define pdFREERTOS_ERRNO_EROFS            30  /* 只读文件系统 */
#define pdFREERTOS_ERRNO_EUNATCH          42  /* 协议驱动未附加 */
#define pdFREERTOS_ERRNO_EBADE            50  /* 无效的交换 */
#define pdFREERTOS_ERRNO_EFTYPE           79  /* 不适当的文件类型或格式 */
#define pdFREERTOS_ERRNO_ENMFILE          89  /* 没有更多文件 */
#define pdFREERTOS_ERRNO_ENOTEMPTY        90  /* 目录不为空 */
#define pdFREERTOS_ERRNO_ENAMETOOLONG     91  /* 文件或路径名太长 */
#define pdFREERTOS_ERRNO_EOPNOTSUPP       95  /* 传输端点不支持该操作 */
#define pdFREERTOS_ERRNO_EAFNOSUPPORT     97  /* 协议不支持地址族 */
#define pdFREERTOS_ERRNO_ENOBUFS          105 /* 没有可用的缓冲区空间 */
#define pdFREERTOS_ERRNO_ENOPROTOOPT      109 /* 协议不可用 */
#define pdFREERTOS_ERRNO_EADDRINUSE       112 /* 地址已在使用中 */
#define pdFREERTOS_ERRNO_ETIMEDOUT        116 /* 连接超时 */
#define pdFREERTOS_ERRNO_EINPROGRESS      119 /* 连接已在进行中 */
#define pdFREERTOS_ERRNO_EALREADY         120 /* 套接字已连接 */
#define pdFREERTOS_ERRNO_EADDRNOTAVAIL    125 /* 地址不可用 */
#define pdFREERTOS_ERRNO_EISCONN          127 /* 套接字已连接 */
#define pdFREERTOS_ERRNO_ENOTCONN         128 /* 套接字未连接 */
#define pdFREERTOS_ERRNO_ENOMEDIUM        135 /* 未插入介质 */
#define pdFREERTOS_ERRNO_EILSEQ           138 /* 遇到无效的 UTF-16 序列。 */
#define pdFREERTOS_ERRNO_ECANCELED        140 /* 操作已取消。 */

/* 以下字节序值由 FreeRTOS+ 组件使用，而非 FreeRTOS 本身。 */
#define pdFREERTOS_LITTLE_ENDIAN          0  /* FreeRTOS 小端模式 */
#define pdFREERTOS_BIG_ENDIAN             1  /* FreeRTOS 大端模式 */

/* 为通用命名重新定义字节序值。 */
#define pdLITTLE_ENDIAN                   pdFREERTOS_LITTLE_ENDIAN  /* 小端模式 */
#define pdBIG_ENDIAN                      pdFREERTOS_BIG_ENDIAN     /* 大端模式 */


#endif /* PROJDEFS_H */
