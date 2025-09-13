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

#ifndef INC_NEWLIB_FREERTOS_H
#define INC_NEWLIB_FREERTOS_H

/* 注意：Newlib支持是应广泛需求而加入的，但FreeRTOS维护者自身并未使用。
 * FreeRTOS不对Newlib的运行结果负责。用户必须熟悉Newlib，并且必须提供
 * 必要存根（stub）的系统级实现。需要注意的是（在撰写本文时），当前的Newlib设计
 * 实现了一个系统级的malloc()，该函数必须配备锁机制。
 *
 * 有关更多信息，请参见第三方链接：http://www.nadler.com/embedded/newlibAndFreeRTOS.html
 * */

#include <reent.h>

#define configUSE_C_RUNTIME_TLS_SUPPORT    1

#ifndef configTLS_BLOCK_TYPE
    #define configTLS_BLOCK_TYPE           struct _reent
#endif

#ifndef configINIT_TLS_BLOCK
    #define configINIT_TLS_BLOCK( xTLSBlock, pxTopOfStack )    _REENT_INIT_PTR( &( xTLSBlock ) )
#endif

#ifndef configSET_TLS_BLOCK
    #define configSET_TLS_BLOCK( xTLSBlock )    ( _impure_ptr = &( xTLSBlock ) )
#endif

#ifndef configDEINIT_TLS_BLOCK
    #define configDEINIT_TLS_BLOCK( xTLSBlock )    _reclaim_reent( &( xTLSBlock ) )
#endif

#endif /* INC_NEWLIB_FREERTOS_H */
