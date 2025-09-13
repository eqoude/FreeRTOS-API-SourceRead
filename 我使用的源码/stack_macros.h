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

#ifndef STACK_MACROS_H
#define STACK_MACROS_H

/*
 * 如果即将切换出去的任务的栈当前已溢出，或者看起来在过去可能发生过溢出，
 * 则调用栈溢出钩子函数。
 *
 * 将 configCHECK_FOR_STACK_OVERFLOW 设为 1 时，此宏仅检查当前栈状态——将当前栈顶值
 * 与栈限制进行比较。将 configCHECK_FOR_STACK_OVERFLOW 设为大于 1 的值时，还会检查
 * 栈的最后几个字节，以确保任务创建时设置的初始值未被覆盖。请注意，第二种检查
 * 并不能保证总能检测到栈溢出。
 */

/*-----------------------------------------------------------*/

/*
 * portSTACK_LIMIT_PADDING 是栈中被视为已使用的额外字数。
 */
#ifndef portSTACK_LIMIT_PADDING
    #define portSTACK_LIMIT_PADDING    0  // 栈限制填充值，默认0
#endif

#if ( ( configCHECK_FOR_STACK_OVERFLOW == 1 ) && ( portSTACK_GROWTH < 0 ) )

/* Only the current stack state is to be checked. */
    #define taskCHECK_FOR_STACK_OVERFLOW()                                                      \
    do {                                                                                        \
        /* Is the currently saved stack pointer within the stack limit? */                      \
        if( pxCurrentTCB->pxTopOfStack <= pxCurrentTCB->pxStack + portSTACK_LIMIT_PADDING )     \
        {                                                                                       \
            char * pcOverflowTaskName = pxCurrentTCB->pcTaskName;                               \
            vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName ); \
        }                                                                                       \
    } while( 0 )

#endif /* configCHECK_FOR_STACK_OVERFLOW == 1 */
/*-----------------------------------------------------------*/

#if ( ( configCHECK_FOR_STACK_OVERFLOW == 1 ) && ( portSTACK_GROWTH > 0 ) )

/* Only the current stack state is to be checked. */
    #define taskCHECK_FOR_STACK_OVERFLOW()                                                       \
    do {                                                                                         \
                                                                                                 \
        /* Is the currently saved stack pointer within the stack limit? */                       \
        if( pxCurrentTCB->pxTopOfStack >= pxCurrentTCB->pxEndOfStack - portSTACK_LIMIT_PADDING ) \
        {                                                                                        \
            char * pcOverflowTaskName = pxCurrentTCB->pcTaskName;                                \
            vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName );  \
        }                                                                                        \
    } while( 0 )

#endif /* configCHECK_FOR_STACK_OVERFLOW == 1 */
/*-----------------------------------------------------------*/

#if ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) && ( portSTACK_GROWTH < 0 ) )

    #define taskCHECK_FOR_STACK_OVERFLOW()                                                      \
    do {                                                                                        \
        const uint32_t * const pulStack = ( uint32_t * ) pxCurrentTCB->pxStack;                 \
        const uint32_t ulCheckValue = ( uint32_t ) 0xa5a5a5a5U;                                 \
                                                                                                \
        if( ( pulStack[ 0 ] != ulCheckValue ) ||                                                \
            ( pulStack[ 1 ] != ulCheckValue ) ||                                                \
            ( pulStack[ 2 ] != ulCheckValue ) ||                                                \
            ( pulStack[ 3 ] != ulCheckValue ) )                                                 \
        {                                                                                       \
            char * pcOverflowTaskName = pxCurrentTCB->pcTaskName;                               \
            vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName ); \
        }                                                                                       \
    } while( 0 )

#endif /* #if( configCHECK_FOR_STACK_OVERFLOW > 1 ) */
/*-----------------------------------------------------------*/

#if ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) && ( portSTACK_GROWTH > 0 ) )

    #define taskCHECK_FOR_STACK_OVERFLOW()                                                                                                \
    do {                                                                                                                                  \
        int8_t * pcEndOfStack = ( int8_t * ) pxCurrentTCB->pxEndOfStack;                                                                  \
        static const uint8_t ucExpectedStackBytes[] = { tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE,   \
                                                        tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE,   \
                                                        tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE,   \
                                                        tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE,   \
                                                        tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE, tskSTACK_FILL_BYTE }; \
                                                                                                                                          \
                                                                                                                                          \
        pcEndOfStack -= sizeof( ucExpectedStackBytes );                                                                                   \
                                                                                                                                          \
        /* Has the extremity of the task stack ever been written over? */                                                                 \
        if( memcmp( ( void * ) pcEndOfStack, ( void * ) ucExpectedStackBytes, sizeof( ucExpectedStackBytes ) ) != 0 )                     \
        {                                                                                                                                 \
            char * pcOverflowTaskName = pxCurrentTCB->pcTaskName;                                                                         \
            vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName );                                           \
        }                                                                                                                                 \
    } while( 0 )

#endif /* #if( configCHECK_FOR_STACK_OVERFLOW > 1 ) */
/*-----------------------------------------------------------*/

/* Remove stack overflow macro if not being used. */
#ifndef taskCHECK_FOR_STACK_OVERFLOW
    #define taskCHECK_FOR_STACK_OVERFLOW()
#endif



#endif /* STACK_MACROS_H */
