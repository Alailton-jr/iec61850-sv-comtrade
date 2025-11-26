#ifndef TIMER_H
#define TIMER_H

#include <time.h>
#include <cerrno>
#include <iostream>

/**
 * @brief High-precision timer for packet transmission timing
 * 
 * Uses CLOCK_MONOTONIC with absolute timers for accurate periodic packet transmission.
 * This approach minimizes jitter and timing drift compared to relative sleep methods.
 */
class Timer {
public:
    Timer() : next_period{0, 0} {}
    
    /**
     * @brief Increment the next period by the specified nanoseconds
     * @param period_ns Period in nanoseconds to add
     */
    void increment_period(long period_ns) {
        next_period.tv_nsec += period_ns;
        while (next_period.tv_nsec >= 1000000000L) {
            next_period.tv_sec += 1;
            next_period.tv_nsec -= 1000000000L;
        }
    }
    
    /**
     * @brief Start a new period from current time + offset
     * @param period_ns Initial period offset in nanoseconds
     */
    void start_period(long period_ns) {
        clock_gettime(CLOCK_MONOTONIC, &next_period);
        increment_period(period_ns);
    }
    
    /**
     * @brief Start period from a specific time
     * @param initial_time Absolute starting time
     */
    void start_period(const struct timespec& initial_time) {
        next_period.tv_sec = initial_time.tv_sec;
        next_period.tv_nsec = initial_time.tv_nsec;
    }
    
    /**
     * @brief Wait until the next period and increment for next call
     * @param period_ns Period duration in nanoseconds
     * 
     * Uses clock_nanosleep with TIMER_ABSTIME for precise absolute timing on Linux.
     * Falls back to relative nanosleep on macOS.
     */
    void wait_period(long period_ns) {
#ifdef __linux__
        int ret;
        do {
            ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, nullptr);
        } while (ret == EINTR);
        
        if (ret != 0 && ret != EINTR) {
            std::cerr << "Error in clock_nanosleep: " << ret << std::endl;
        }
#else
        // macOS: Use relative nanosleep as fallback
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        struct timespec sleep_time;
        sleep_time.tv_sec = next_period.tv_sec - now.tv_sec;
        sleep_time.tv_nsec = next_period.tv_nsec - now.tv_nsec;
        
        if (sleep_time.tv_nsec < 0) {
            sleep_time.tv_sec--;
            sleep_time.tv_nsec += 1000000000L;
        }
        
        // Only sleep if time is in the future
        if (sleep_time.tv_sec >= 0 && sleep_time.tv_nsec >= 0) {
            nanosleep(&sleep_time, nullptr);
        }
#endif
        increment_period(period_ns);
    }
    
    /**
     * @brief Get the next scheduled period time
     * @return Reference to the next period timespec
     */
    const struct timespec& get_next_period() const {
        return next_period;
    }

private:
    struct timespec next_period;
};

#endif // TIMER_H
