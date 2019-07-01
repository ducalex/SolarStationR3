#include <stdint.h>
#include <esp_timer.h>
#include <sys/time.h>


// True RTC micros count since the ESP32 was first powered up
static inline uint64_t rtc_micros()
{
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return ((uint64_t)curTime.tv_sec * 1000000) + curTime.tv_usec;
}


static inline uint32_t rtc_millis()
{
    return rtc_micros() / 1000;
}
