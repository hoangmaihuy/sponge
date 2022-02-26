//
// Created by Mai Hoàng on 21/02/2022.
//

#ifndef SPONGE_TIMER_HH
#define SPONGE_TIMER_HH

#include <cstdint>

class Timer {
  private:
    int64_t timer;
    bool running;

  public:
    Timer();
    bool is_timeout() const;
    bool is_running() const;
    void passed(uint64_t duration);
    void reset(uint64_t timeout);
    void start();
    void stop();
    int64_t time_left() const;
};

#endif  // SPONGE_TIMER_HH
