#ifndef TIMERCPP_H
#define TIMERCPP_H

#include <iostream>
#include <thread>
#include <chrono>

class Timer {
    public:
        bool stop = false;
        void setTimeout(auto function, int delay);
        void setInterval(auto function, int interval);
        void stopLruTimer();
        void startLruTimer();
};

void Timer::setTimeout(auto function, int delay) {
    this->stop = false;
    std::thread t([=]() {
        if(this->stop) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if(this->stop) return;
        function();
    });
    t.detach();
}

void Timer::setInterval(auto function, int interval) {
    this->stop = false;
    std::thread t([=]() {
        while(true) {
            if(this->stop) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            while (this->stop) {}
            function();
        }
    });
    t.detach();
}
#endif
