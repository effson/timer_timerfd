#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <set>
#include <chrono>
#include <iostream>

using namespace std;

struct TimerNodeBase {
    time_t expire;
    uint64_t id;
}

struct TimerNode : public TimerNodeBase {
    using Callback = std::function<void(const TimerNode &node)>;
    Callback func;
    TimerNode(time_t expire, uint64_t id, Callback func) : func(std::move(func)) {
        this->expire = expire;
        this->id = id;
    }
};

bool operator < (const TimerNode &lhs, const TimerNode &rhs) {
    if (lhs.expire < rhs.expire) {
        return true;
    } else if (lhs.expire > rhs.expire) {
        return false;
    } else {
        return lhs.id < rhs.id;
    }
}

class Timer {
public:
    static inline time_t GetTick() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    // 1.std::bind
    // 2. class operator()
    // 3. []{} lambda表达式
    TimerNodeBase AddTimer(int msec, TimerNode::Callback func) {
        time_t expire = GetTick() + msec;
        if (timeouts.empty() || expire <= timeouts.crbegin()->expire) {
            auto pairs = timeouts.emplace(GenID(), expire, std::move(func));
            return static_cast<TimerNodeBase>(*pairs.first);
        }
        auto ele = timeouts.emplace_hint(timeouts.crbegin().base(), GenID(), expire, std::move(func));
        return static_cast<TimerNodeBase>(*ele);
    }

    void DelTimer(const TimerNodeBase &node) {
        auto iter = timeouts.find(node);
        if (it != timeouts.end()) {
            timeouts.erase(iter);
        }
    }

    void HandleTimer(time_t now) {   
        auto iter = timeouts.begin();
        while (iter != timeouts.end() && iter->expire <= now) {
            iter->func(*iter);
            iter = timeouts.erase(iter);
        }
    }

public:
    virtual void UpdateTimerfd(const int fd) {
        struct timespec abstime;
        auto iter = timeouts.begin();
        if (iter != timeouts.end()) {
            abstime.tv_sec = iter->expire / 1000;
            abstime.tv_nsec = (iter->expire % 1000) * 1000000;
        } else {
            abstime.tv_sec = 0;
            abstime.tv_nsec = 0;
        }

        struct itimerspec its = {
            .it_value = abstime,
            .it_interval = {}
        }
        timerfd_settime(fd, TFD_TINER_ABSTIME, &its, nullptr);
    }

private:
    uint64_t GenID() {
        return gid ++;
    }
    static uint64_t gid;
    set<TimerNode, std::less<>> timeouts;
}

uint64_t Timer::gid = 0;

int main() {

    int epfd = epoll_create1(1);

    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct epoll_event ev = {.events = EPOLLIN | EPOLLET};
    epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &ev);

    unique_ptr<Timer> timer = make_unique<Timer>();
    int i = 0;
    timer->AddTimer(1000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << "node id:" << node.id << "revoke times" << ++i << endl;
    });

    timer->AddTimer(1000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << "node id:" << node.id << "revoke times" << ++i << endl;
    });

    timer->AddTimer(3000, [&](const TimerNode &node) {
        cout << Timer::GetTick() << "node id:" << node.id << "revoke times" << ++i << endl;
    });

    auto node = timer->AddTimer(2100, [&](const TimerNode &node) {
        cout << Timer::GetTick() << "node id:" << node.id << "revoke times" << ++i << endl;
    });

    timer->DelTimer(node);
    cout << "now time:" << Timer::GetTick() << endl;
    struct epoll_event evs[64] = {0};
    while (true) {
        timer->UpdateTimerfd(timerfd);
        int n = epoll_wait(epfd, evs, 64, -1);
        time_t now = Timer::GetTick();
        for (int i = 0; i < n; ++i) {           
                
        }
        timer->HandleTimer(now);
    }
    epoll_ctl(epfd, EPOLL_CTL_DEL, timerfd, &ev);
    close(timerfd);
    close(epfd);
    return 0;
}
