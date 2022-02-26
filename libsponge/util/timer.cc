//
// Created by Mai Hoàng on 21/02/2022.
//

#include <cassert>
#include "timer.hh"

Timer::Timer() : timer(0), running(false) { }

bool Timer::is_timeout() const { return running && timer <= 0; }

bool Timer::is_running() const { return running; }

void Timer::start() {
    assert(timer > 0 && !running);
    running = true;
}

void Timer::stop() {
    assert(running);
    running = false;
}

void Timer::reset(uint64_t timeout) {
    running = true;
    timer = timeout;
}

void Timer::passed(uint64_t duration) {
    assert(running);
    timer -= duration;
}

int64_t Timer::time_left() const { return timer; }



