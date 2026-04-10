#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"

static const char *TAG = "timer_manager";

class TimerManager {
public:
    typedef void (*CallbackFunction)();

    TimerManager() : timer_started(false), callback(nullptr), timeout(pdMS_TO_TICKS(1800000)) {
        timer = xTimerCreate("timer", timeout, pdTRUE, (void*)this, timer_callback);
    }

    void start_timer() {
        if (!timer_started) {
            ESP_LOGI(TAG, "Starting timer");
            xTimerChangePeriod(timer, timeout, 0);
            xTimerStart(timer, 0);
            timer_started = true;
        } else {
            ESP_LOGW(TAG, "Timer already started");
        }
    }

    void stop_timer() {
        if (timer_started) {
            ESP_LOGI(TAG, "Stopping timer");
            xTimerStop(timer, 0);
            timer_started = false;
        } else {
            ESP_LOGW(TAG, "Timer not started");
        }
    }

    void set_callback(CallbackFunction cb) {
        callback = cb;
    }

    void set_timeout(TickType_t to) {
        timeout = to;
    }

private:
    TimerHandle_t timer;
    bool timer_started;
    CallbackFunction callback;
    TickType_t timeout;

    static void timer_callback(TimerHandle_t xTimer) {
        TimerManager *tm = (TimerManager*)pvTimerGetTimerID(xTimer);
        if (tm->callback) {
            tm->callback();
        }
        xTimerStop(xTimer, 0);
    }
};
