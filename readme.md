构建了一个 基于 epoll + timerfd 的高效定时器调度框架。它注册了多个定时器任务，通过 epoll 等待 timerfd 超时事件，然后触发并处理相应的回调函数。
