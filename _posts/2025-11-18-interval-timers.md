---
title: "异步编程指南系列其四：间隔定时器"
description: "介绍间隔定时器的使用和局限性"
date: 2025-11-18
categories:
  - 编程指南
  - Linux系统编程
tags:
  - 信号
  - Unix编程
  - 定时器
  - POSIX
toc: true  # 启用目录自动生成
math: false  # 若无数学公式可设为false
mermaid: true  # 若使用mermaid图表可启用
footer: "异步编程指南系列 · 第四篇"  # 可选页脚说明
---

定时器在编程中有广泛的应用，例如实现超时机制、定期执行任务、性能分析等。在Unix/Linux系统中，有多种定时器接口可供选择，从简单的单次定时器到复杂的间隔定时器。

本章将介绍 `alarm()​`和**间隔定时器**(`itimer`)​，并在下一章介绍**POSIX定时器**。

## alarm()

`alarm()` 安排在若干秒后向调用进程发送一个 `SIGALRM` 信号。

```c
#include <unistd.h>

unsigned int alarm(unsigned int seconds);
```

如果 `seconds` 为 0，则取消任何未决的闹钟。

> 在任何情况下，之前设置的 `alarm()` 都会被取消。
{: .prompt-warning }

`alarm()`函数虽然简单易用，但存在明显局限性：

- 只能设置单个定时器，无法同时管理多个定时事件
- 仅支持秒级精度，无法满足高精度定时需求
- 只能触发单次，不支持周期性定时
- 与`alarm()`自身和`setitimer()`共用同一个定时器，相互干扰

## 间隔定时器(interval timers)

间隔定时器，即那些最初在未来某个时间点到期，并且（可选地）在那之后按固定间隔到期的定时器。当定时器到期时，会向调用进程生成一个信号，并且定时器会重置为指定的间隔（如果间隔不为零）。

间隔定时器(`setitimer()`)相比`alarm()`提供了更强大的功能：

- 支持三种不同类型的定时器（`REAL` `VIRTUAL` `PROF`）
- 提供微秒级精度
- 支持周期性触发

> 需要注意：**POSIX.1-2008**已将`getitimer()`和`setitimer()`标记为**过时函数**。虽然目前主流系统仍支持这些接口，但在新项目中应避免使用。
{: .prompt-warning }

### 间隔定时器API

```c
#include <sys/time.h>

int getitimer(int which, struct itimerval *curr_value);
int setitimer(int which, const struct itimerval *restrict new_value,
              struct itimerval *_Nullable restrict old_value);
```

函数 `getitimer ()` 会将由 `which` 指定的定时器的当前值存入 `curr_value` 所指向的缓冲区中。

函数 `setitimer ()` 通过将定时器设置为 `new_value` 所指定的值，来启用或禁用由 `which` 指定的定时器。如果 `old_value` 不为 `NULL`，它所指向的缓冲区将用于返回定时器的先前值（即与 `getitimer ()` 返回的信息相同）。

如果 `new_value.it_value` 中的任何一个字段为非零值，那么该定时器将被启用，并在指定时间初始到期。如果 `new_value.it_value` 中的两个字段均为零，那么该定时器将被禁用。

`new_value.it_interval` 字段指定了定时器的新间隔；如果其两个子字段均为零，则该定时器为单触发模式。

> 标准对以下调用的含义未作规定：
>
> `setitimer(which, NULL, &old_value);`
>
> 许多系统（Solaris、各种 BSD 系统，或许还有其他系统）将其视为等同于：
>
> `getitimer(which, &old_value);`
>
> 而在 Linux 中，这被当作是新值字段均为零的调用，也就是说，定时器会被禁用。不要使用 Linux 的这种不当特性：它不具备可移植性，而且也毫无必要。

### 定时器类型

提供了三种类型的定时器（通过 `which` 参数指定），每种定时器都依据不同的时钟进行计数，并在定时器到期时生成不同的信号：

- `ITIMER_REAL`     此定时器以实际（即墙上时钟）时间倒计时。每次到期时，会生成一个 `SIGALRM` 信号。
- `ITIMER_VIRTUAL`  时器根据进程所消耗的用户态 CPU 时间进行倒计时。（该测量包括进程中所有线程消耗的 CPU 时间。）每次到期时，会生成一个 `SIGVTALRM` 信号。
- `ITIMER_PROF`     此定时器根据进程消耗的总（即用户态和系统态）CPU 时间进行倒计时。（该测量包括进程中所有线程消耗的 CPU 时间。）每次到期时，会生成一个 `SIGPROF` 信号。
与 `ITIMER_VIRTUAL` 配合使用时，此定时器可用于分析进程消耗的用户态和系统态 CPU 时间。

> 一个进程每种类型的定时器各只有一个。重复调用`setitimer`不会创建新的定时器，而是会**覆盖已有的同类型定时器**，这可能导致意外的定时器重置。
{: .prompt-warning }

### 定时器数据结构

```c
struct itimerval {
    struct timeval it_interval; /* Interval for periodic timer */
    struct timeval it_value;    /* Time until next expiration */
};

struct timeval {
    time_t      tv_sec;         /* seconds */
    suseconds_t tv_usec;        /* microseconds */
};
```

`it_value` 子结构中存放的是指定定时器距离下次到期剩余的时间。

如果 `it_value` 的两个字段均为零，则表示当前该定时器未启用（处于 inactive 状态）。

`it_interval` 子结构中存放的是定时器的间隔时间

如果 `it_interval` 的两个字段均为零，则这是一个一次性定时器（即只到期一次）。

## 使用示例

```c
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

static volatile size_t count = 0;
void handler_alarm(int sig, siginfo_t *info, void *ucontext) {
    if (info->si_code > 0)
        count += 1;
}
const size_t get_count() {
    return count;
}

volatile sig_atomic_t loop = 1;
void handler_usr1(int sig) {
    loop = 0;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = &handler_alarm;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    act.sa_flags = 0;
    act.sa_handler = &handler_usr1;
    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    alarm(3U);
    pause();

    struct itimerval cutitimer;

    printf("Timer expired %luq times.\n", get_count());
    if (getitimer(ITIMER_REAL, &cutitimer) == -1) {
        perror("getitimer");
        exit(EXIT_FAILURE);
    }
    printf("Time interval: %ld.%06lds, remaining time: %ld.%06lds.\n",
           cutitimer.it_interval.tv_sec, cutitimer.it_interval.tv_usec,
           cutitimer.it_value.tv_sec, cutitimer.it_value.tv_usec);

    struct itimerval timer = {
        .it_interval={ .tv_sec=5, .tv_usec=0, },
        .it_value={ .tv_sec=1, .tv_usec=0, },
    };
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer");
        exit(EXIT_FAILURE);
    }

    while (loop) {
        pause();
        printf("Timer expired %luq times.\n", get_count());
        if (getitimer(ITIMER_REAL, &cutitimer) == -1) {
            perror("getitimer");
            exit(EXIT_FAILURE);
        }
        printf("Time interval: %ld.%06lds, remaining time: %ld.%06lds.\n",
               cutitimer.it_interval.tv_sec, cutitimer.it_interval.tv_usec,
               cutitimer.it_value.tv_sec, cutitimer.it_value.tv_usec);
    }

    return 0;
}

```

这个示例程序实现了一个可手动停止的定时器，能通过信号机制实现定时计数，先由`alarm()`在 3 秒后触发首次计数，之后`setitimer()`切换为 1 秒后再每 5 秒循环计数，每次计数会显示当前次数和定时器状态；同时支持通过特定信号手动停止程序，`SIGALRM`信号处理函数设计避免外部干扰，两个信号处理函数都保证了单线程下的异步信号安全（详见[异步信号安全](/posts/async-signal-safe)）。

在上述示例中，用相同的方式处理 `alarm()` 和 `setitimer()` 意在表明它们使用了同一个定时器。实际编程应注意避免错误的覆盖了正在使用的定时器。

在这个程序中，只使用对应的信号处理程序`handler_alarm()`和只读访问`get_count()`来保障`count`的安全读写，实际编程中应通过独立文件或类等技术封装`count`的定义，只导出`handler_alarm()`和`get_count()`接口来避免`count`被误用。

其中，在`handler_alarm()`信号处理程序中判断了信号来源，防止了其他程序（如`kill`命令）干扰到程序的正常计数。

> 根据 **POSIX** 标准，若信号的 `si_code` 字段为 `SI_USER`、`SI_QUEUE`，或任何小于等于 `0` 的值，则该信号由进程主动发送，反之，若 `si_code` 的值大于 `0`，则表明该信号由系统内核生成。`alarm()` 和 `setitimer()` 函数使系统向进程生成 `SIGALRM` 信号。因此，可通过判断 `si_code > 0` 来识别 `SIGALRM` 信号是否由本进程的系统调用产生的，避免其他程序（例如 `kill` 命令）发送信号干扰。

`handler_alarm()`信号处理函数使用了全局变量`count`，是不可重入函数，但是`count`只有`handler_alarm()`这一个函数写入，且通过默认屏蔽自身信号保证了`handler_alarm()`相对于信号原子,因此这个函数在单线程下是信号安全的。

同理，信号处理函数`handler_usr1()`，使用原子类型`sig_atomic_t`和简单原子赋值操作，保证了信号安全。

如果定时器间隔设置过短，`SIGALRM` 信号的发送频率可能超过处理能力。由于标准信号不支持排队机制，多余的信号会被丢弃，导致计数不准确。实践中应确保间隔时间足够长，以完成信号处理和相关操作。

> 对于需要高精度高频触发的场景，下一章介绍的**POSIX定时器**将解决这一问题。
{: .prompt-tip }

## 注意事项

`alarm()` 与 `setitimer()` 共享同一个底层定时器，因此对其中任一函数的调用都会影响另一个的使用。此外，`sleep()` 的实现可能依赖于 `SIGALRM` 信号，因此混合使用 `alarm()` 与 `sleep()`（或 `usleep()`）容易导致不可预期的行为。

需注意的是，**POSIX.1** 并未明确定义 `setitimer()` 与 `alarm()`、`sleep()` 和 `usleep()` 之间的交互语义。进一步地，**POSIX.1-2008** 已将 `getitimer()` 和 `setitimer()` 标记为过时（obsolete），推荐使用更现代且功能更强的 **POSIX 定时器**API 替代。

## 向POSIX定时器过渡

| 类型 | `alarm()` | **间隔定时器** | **POSIX定时器** |
| --- | --- | --- | --- |
| 精度 | 秒 | 微秒 | 纳秒 |
| 数量 | 1个 | 每类型1个 | 多个 |
| 周期性 | 不支持 | 支持 | 支持 |
| 标准状态 | 标准 | 过时 | 推荐 |

如表格所示，传统定时器接口有各种局限性，现代应用更推荐使用**POSIX定时器API**，它们提供：

- 创建多个独立定时器的能力
- 纳秒级精度支持
- 更灵活的信号处理和线程通知机制
- 更好的可移植性和标准符合性

在下一章中，我们将详细介绍**POSIX定时器**的强大功能和使用方法。
