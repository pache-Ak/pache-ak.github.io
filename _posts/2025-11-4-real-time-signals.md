---
title: "异步编程指南其三:实时信号"
description: "介绍Linux/Unix实时信号的特性和携带额外数据的能力"
date: 2025-11-04
categories:
  - 编程指南
  - Linux系统编程
tags:
  - 信号
  - Unix编程
  - 进程通信
  - POSIX
toc: true  # 启用目录自动生成
math: false  # 若无数学公式可设为false
mermaid: true  # 若使用mermaid图表可启用
footer: "异步编程指南系列 · 第三篇"  # 可选页脚说明
---

我们已经介绍了标准信号的特性和局限性，特别是其不支持排队，可能导致信号丢失。本章将探讨实时信号，它通过支持排队解决了可靠性的问题，并提供了携带额外数据的能力。

## 实时信号简介

`POSIX.1-2001`定义的实时信号支持的实时信号范围由宏`SIGRTMIN`和`SIGRTMAX`定义。

实时信号具有以下特点：

- 实时信号的多个实例可以排队。
- 如果使用`sigqueue`发送信号，则可以随信号一起发送一个附带值(可以是整数或指针)。
- 实时信号的传递顺序是有保证的。同一类型的多个实时信号按发送顺序传递。如果不同的实时信号发送到一个进程，它们会从编号最小的信号开始传递。(即，编号小的信号优先级最高。)
- 对于未处理的实时信号，默认操作是终止接收该信号的进程。

## 核心数据结构

随信号一同发送的附带数据项(可以是整数或指针值)，其类型如下：

```C
union sigval {
    int sival_int;
    void *sival_ptr;
};
```

数据和各类信息通过信号处理函数的第二个参数传递,类型为`siginfo_t`。

以下三个字段在`siginfo_t`中固定设置:

- `si_signo` : `int`类型,信号的编号,等价于信号处理函数的第一个参数
- `si_errno` : `int`类型,与此信号相关联的错误号值，如`<errno.h>`中所述
    > 在 Linux 上通常不使用。
- `si_code`  : `int`类型,用于指示发送此信号的原因

其余字段在实现上可能是一个联合体(`union`)，因此程序应仅读取与当前信号和`si_code`值相对应的有效字段，避免访问未定义的数据。

- `si_pid` : `pid_t`类型,发送信号的进程id
- `si_uid` : `uid_t`类型,发送信号进程的实际用户id
- `si_value` : `sigval`类型, 信号附带的数据

下面列举了对于所有信号都可用的`si_code`的取值和对应的可用字段,更多信息请参见信号高级特性章节.

| `si_code` | 原因 | 其他字段 |
| --- | --- | --- |
| `SI_USER` | 由`kill()`、`pthread_kill()`、`raise()`、`abort()`或`alarm()`发送的信号。| `si_pid` `si_uid` |
| `SI_QUEUE` | 由 `sigqueue()`发送的信号。 | `si_pid` `si_uid` `si_value` |
| `SI_TIMER` | 信号是由 设置 `timer_settimer()` 的计时器到期产生的。| `si_pid` `si_uid` `si_value` |
| `SI_MESGQ` | 信号是通过消息到达空消息队列生成的。| `si_pid` `si_uid` `si_value` |
| `SI_ASYNCIO` | 信号是由异步I/O请求完成而产生的。 | `si_pid` `si_uid` `si_value` |

在了解了核心数据结构后，我们来看如何发送和接受携带数据的实时信号。

## 发送信号

使用`sigqueue()`系统调用发送实时信号，并可通过`union sigval`参数附加数据。

以下是一个简单的发送实时信号的示例:

```C
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pid_t target_pid = atoi(argv[1]);

    union sigval value;
    value.sival_int = 42;

    if (sigqueue(target_pid, SIGRTMIN, value) == -1) {
        perror("sigqueue");
        exit(EXIT_FAILURE);
    }
}
```

## 处理实时信号及数据

[上一章](/posts/reliable-signals)我们已经介绍了如何安装信号处理函数,本章我们将在其基础上获取数据以及进程和用户id。

要接收并处理附加数据，必须使用`sa_sigaction`处理程序，并设置`SA_SIGINFO`标志，`sa_handler`无法获取`siginfo_t`信息。

以下是一个简单的示例,判断如果是父进程发送信号,则将`value`设置为信号携带的值:

```C
#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t value = 0;

void handler(int sig, siginfo_t *info, void *ucontext) {
    if (info->si_code == SI_QUEUE && info->si_pid == getppid()) {
        value = info->si_value.sival_int;
    }
}
```

## 信号递送顺序与优先级

当进程同时有多个信号处于待处理状态时，信号的递送顺序遵循严格的优先级规则：

1. **实时信号按编号递增顺序**：对于不同的实时信号，编号小的信号优先于编号大的信号。即`SIGRTMIN`会最先被递送，`SIGRTMAX`最后。

2. **同一实时信号的多个实例按发送顺序**：对于同一实时信号的多个实例，它们会按照发送的顺序被递送（即先发送的先递送）。

3. **标准信号与实时信号的优先级未规定**：POSIX标准并未明确规定标准信号和实时信号之间的优先级。
    > 在Linux系统中，如果有标准信号和实时信号同时待处理，标准信号会先于实时信号被递送。

### 示例程序验证

我们设计了验证程序[SigPri.c](/code/signal/SigPri.c)来演示上述规则：

**实验设计**：

1. 屏蔽所有信号，注册要发送的信号处理程序
    > 注册时设置信号处理程序屏蔽所有信号,原因在下文解释
2. 发送标准信号和实时信号（实时信号发送不止一次）
3. 解除屏蔽，观察处理顺序

**预期结果**：
标准信号1 → 标准信号2 → 低编号实时信号(实例1) → 低编号实时信号(实例2) → 高编号实时信号(实例1) → ...

## 编程注意事项

由于可用的实时信号范围会根据glibc线程实现而变化(并且这种变化可能在运行时根据可用的内核和glibc发生)，实际上，不同的UNIX系统上实时信号的范围也各不相同，因此程序绝不应该使用硬编码的数字来引用实时信号，而应该始终使用`SIGRTMIN+n`的表示法，并包含适当的(运行时)检查，确保`SIGRTMIN+n`不超过`SIGRTMAX`。

实时信号并不确保信号一定能成功发送给目标进程而不丢失,信号的队列并非无限大的,当队列满时,`sigqueue`发送信号会失败,并设置`errno`为`EAGAIN`。发送方有义务确认信号是否成功发送。以下是一个简单的指数退避重试示例，用于处理队列满的情况。请注意，**此示例仅用于演示目的，在生产环境中可能需要更完善的错误处理策略**。

```C
unsigned int time = 1;
while (sigqueue(target_pid, SIGRTMIN, value) == -1 && errno == EAGAIN) {
    sleep(time);
    time *= 2;
}
```

当通过`sival_ptr`传递指针时，发送方和接收方必须共享相同的内存空间(如通过共享内存或父子进程继承)，否则指针将指向无效地址。

### 特别注意：**优先级规则只影响递送顺序，不改变信号重入行为**

即使高优先级信号正在处理，低优先级信号也会立即递送，可能导致[信号重入](/posts/async-signal-safe)。

上述的示例程序在注册信号处理程序时屏蔽了所有信号来防止**信号重入**.读者可以尝试将屏蔽集设置为空,预计会得到顺序完全相反的输出结果.

在使用到递送优先级特性的程序中要慎重考虑如何设置屏蔽集,防范高优先级信号被低优先级信号重入引起优先级反转.

## 本章小结

本章系统介绍了实时信号的核心机制：

- **实时信号的三大特性**：支持排队、可携带数据、有保证的传递顺序
- **核心数据结构**：`union sigval`用于携带数据，`siginfo_t`用于获取信号详细信息
- **发送机制**：使用`sigqueue()`发送信号及伴随数据
- **处理机制**：通过`SA_SIGINFO`标志和`sa_sigaction`处理函数获取信号数据
- **重要限制**：信号编号的可移植性、队列大小的限制、指针数据的内存共享要求
- **递送优先级**:当进程同时有多个信号处于待处理状态时，信号的递送顺序遵循优先级规则

在实际编程时要注意以下事项:

- 确保实时信号在系统支持的范围内
- `sigqueue`发送信号会失败,发送方有义务确认信号是否成功发送
