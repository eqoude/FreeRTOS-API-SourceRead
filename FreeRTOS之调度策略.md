# FreeRTOS之调度策略

先来看看FreeRTOS支持几种调度策略，在《Mastering-the-FreeRTOS-Real-Time-Kernel.v1.1.0》有一幅图

![img](https://picx.zhimg.com/80/v2-1a1231fc98e3d9a1bc750d5f659aa924_1440w.png?source=ccfced1a)

*FreeRTOS* 是通过 *configUSE_PREEMPTION* 和 *configUSE_TIME_SLICING* 这两个宏设置调度策略的，共支持三种。

## 带时间片的抢占式调度

> 带时间片的抢占式调度:高优先级任务可抢占低优先级任务，且相同优先级任务按时间片轮流执行（时间片由时钟中断周期决定）。

![img](https://picx.zhimg.com/80/v2-e568a41d47b8d692237f85adec8c7ec9_1440w.png?source=ccfced1a)

这里我们说一种关于空闲任务的情况

![img](https://pic1.zhimg.com/80/v2-9b6fe4a9950d76543232f55aea60834b_1440w.png?source=ccfced1a)

此时任务1和空闲任务轮流执行，此时任务1需要等到空闲任务时间片结束后才能运行，一般空闲任务要处理的事情不重要，此时任务1不能得到有效的执行，当然，修改任务1优先级可以处理，这里介绍一个宏 *configIDLE_SHOULD_YIELD* ，如果设置为1，那么如果有其他处于就绪状态的空闲优先级任务，空闲任务将让出（主动放弃其分配的时间片剩余部分）。 

![img](https://pica.zhimg.com/80/v2-13bbd75601d7b6430511098b62eac6f1_1440w.png?source=ccfced1a)

## 不带时间片的抢占式调度

> 不带时间片的抢占式调度:高优先级任务可抢占低优先级任务，但相同优先级任务中，只有当前任务主动阻塞（如调用vTaskDelay()）时，其他任务才能执行。

![img](https://pica.zhimg.com/80/v2-96bf957d60aa66f2d47b619c336c5fb7_1440w.png?source=ccfced1a)

如果不使用时间片划分，那么调度器仅在以下两种情况下选择一个新任务进入运行状态： 1.一个更高优先级的任务进入就绪状态。 2.处于运行状态的任务进入阻塞或挂起状态。

## 协作式调度

> 协作式调度:任务无法被抢占，需主动调用taskYIELD()或进入阻塞状态才会切换到其他任务，无优先级抢占机制。

![img](https://pic1.zhimg.com/80/v2-c47dfb462ffb1d0655dda95a5ed7f49b_1440w.png?source=ccfced1a)