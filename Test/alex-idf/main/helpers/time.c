#include <stdint.h>
#include <unistd.h>
#include <esp_timer.h>
#include <sys/time.h>


// True RTC micros count since the ESP32 was first powered up
uint64_t rtc_micros()
{
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return ((uint64_t)curTime.tv_sec * 1000000) + curTime.tv_usec;
}


uint32_t rtc_millis()
{
    return rtc_micros() / 1000;
}


uint64_t micros()
{
    return esp_timer_get_time();
}


uint32_t millis()
{
    return micros() / 1000;
}


void delay(uint32_t ms)
{
    usleep(ms * 1000); // Will choose automatically between a loop delay or vTaskDelay
}
