下面是本专栏使用到的文档、源码

FreeRTOS官网：[FreeRTOS™ - FreeRTOS™](https://link.zhihu.com/?target=https%3A//www.freertos.org/)

API说明和使用：

1. 官网查看：进入官网，点击文档->API引用
2. 官网下载PDF文档：进入官网，点击文档->书籍和手册，下载一个版本（不过是英文版）
3. 【安富莱】FreeRTOS教程 [硬汉嵌入式论坛 - Powered by Discuz!](https://link.zhihu.com/?target=https%3A//forum.anfulai.cn/forum.php) 嵌入式文档教程->FreeRTOS教程

源码下载：

1.  官网下载：FreeRTOS下载，进入官网，点击右上方下载，下载一个版本
2.  前往GitHub下载：[https://github.com/FreeRTOS/FreeRTOS-Kernel](https://link.zhihu.com/?target=https%3A//github.com/FreeRTOS/FreeRTOS-Kernel)



本文使用内核版本为 FreeRTOS Kernel V11.1.0

移植版本为 ARM CM3（ARM Cortex-M3）



知乎看源码不方便，所以还是放到了GitHub上面，和知乎内容是一样的。

原知乎专栏  [FreeRTOS-从API使用到源码阅读 - 知乎](https://zhuanlan.zhihu.com/column/c_1949139698313310238)





> 为什么先看编码风格和内存管理这些？
> 因为去读上面的文档，首先也是介绍这两个。
> 内存管理的话，去找一个移植的教程他可能就让你把<heap4.c>或是<heap5.c>放到你的工程里，那到底有什么区别呢，需要细说一下。像是<heap4.c>还好直接就用了，<heap5.c>是需要你多做一些事的。

[FreeRTOS之数据类型和编码风格](./FreeRTOS之数据类型和编码风格.md)

[FreeRTOS内存管理01（源码）]((./FreeRTOS内存管理01（源码）.md))（直接从源码开始，因为我们使用时很少用到相关的接口）

[FreeRTOS内存管理02（源码）](./FreeRTOS内存管理02（源码）.md) （直接从源码开始，因为我们使用时很少用到相关的接口）

> 为什么要先说链表呢？
> 去读上面的文档，他们也不会介绍这个，但是要去理解FreeTROS，链表其实挺重要的，在FreeRTOS中任务的就绪、挂起........都是一个链表，有就绪链表、挂起链表.......让一个人任务变成就绪态就是把相应的任务放到这个链表中。那这里主要就介绍一下链表的结构。

FreeRTOS之链表（源码） - 知乎 （只有源码，因为我们使用时很少用到相关的接口）

> 下面就要简单一点了，是经常要用的，可以直接看API看看怎么用，这里的源码主要说了一下任务句柄是什么，一个任务长什么样子，它在内存中怎么放的。

FreeRTOS之Task-01任务创建（API+源码） - 知乎 （API+源码）

FreeRTOS之Task-02任务延时（API+源码） - 知乎 （API+源码）

FreeRTOS之Task-03空闲任务（API+源码） - 知乎 （API+源码）