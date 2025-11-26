#ifndef TIMER_H
#define TIMER_H

#ifdef _WIN32
    #include <windows.h>
    #include <cstdint>
#else
    #include <time.h>
    #include <cerrno>
#endif
#include <iostream>

/**
 * @brief High-precision timer for packet transmission timing
 * 
 * Uses CLOCK_MONOTONIC with absolute timers for accurate periodic packet transmission.
 * This approach minimizes jitter and timing drift compared to relative sleep methods.
 * 
 * Windows: Uses QueryPerformanceCounter for high-precision timing
 * Linux/macOS: Uses clock_gettime with CLOCK_MONOTONIC
 */
class Timer {
public:
#ifdef _WIN32
    Timer() : next_period_us(0), freq_us(0.0) {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        freq_us = static_cast<double>(freq.QuadPart) / 1000000.0;  // Convert to microseconds
    }
#else
    Timer() : next_period{0, 0} {}
#endif
    
    /**
     * @brief Increment next period by specified nanoseconds
     * @param period_ns Period to add in nanoseconds
     */
    void increment_period(long long period_ns) {
#ifdef _WIN32
        // Convert nanoseconds to microseconds for Windows
        next_period_us += period_ns / 1000.0;
#else
        next_period.tv_nsec += period_ns;
        while (next_period.tv_nsec >= 1000000000L) {
            next_period.tv_sec += 1;
            next_period.tv_nsec -= 1000000000L;
        }
#endif
    }
    
    /**
     * @brief Start a new period from current time + offset
     * @param period_ns Initial period offset in nanoseconds
     */
    void start_period(long long period_ns) {
#ifdef _WIN32
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        next_period_us = now.QuadPart / freq_us;
        increment_period(period_ns);
#else
        clock_gettime(CLOCK_MONOTONIC, &next_period);
        increment_period(period_ns);
#endif
    }
    
#ifndef _WIN32
    /**
     * @brief Start period from a specific time (Unix only)
     * @param initial_time Absolute starting time
     */
    void start_period(const struct timespec& initial_time) {
        next_period.tv_sec = initial_time.tv_sec;
        next_period.tv_nsec = initial_time.tv_nsec;
    }
#endif
    
    /**
     * @brief Wait until the next period and increment for next call
     * @param period_ns Period duration in nanoseconds
     * 
     * Uses clock_nanosleep with TIMER_ABSTIME for precise absolute timing on Linux.
     * Falls back to relative nanosleep on macOS.
     * Windows uses high-precision QueryPerformanceCounter.
     */
    void wait_period(long long period_ns) {
#ifdef _WIN32
        // Windows: Busy-wait for high precision, fallback to Sleep for coarse delays
        LARGE_INTEGER now;
        double target_us = next_period_us;
        
        while (true) {
            QueryPerformanceCounter(&now);
            double current_us = now.QuadPart / freq_us;
            double diff_us = target_us - current_us;
            
            if (diff_us <= 0) break;
            
            // If more than 1ms away, use Sleep to avoid busy-waiting
            if (diff_us > 1000) {
                Sleep(static_cast<DWORD>((diff_us - 500) / 1000));  // Sleep most of the time
            }
            // Otherwise busy-wait for precision
        }
        
#elif defined(__linux__)
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
    
#ifndef _WIN32
    /**
     * @brief Get the next scheduled period time (Unix only)
     * @return Reference to the next period timespec
     */
    const struct timespec& get_next_period() const {
        return next_period;
    }
#endif

private:
#ifdef _WIN32
    double next_period_us;  // Next period in microseconds
    double freq_us;         // Performance counter frequency in microseconds
#else
    struct timespec next_period;
#endif
};

#endif // TIMER_H
