#include "bsp_hooks.hpp"

#include "qpcpp.hpp"  // QP-C++ framework

void BSPHooks::onStartup() {
    QP::ESP32_tickHookInit();
#ifdef QS_ON
    xTaskCreatePinnedToCore(QSpy_Task, /* Function to implement the task */
                            "QSPY",    /* Name of the task */
                            10000,     /* Stack size in words */
                            NULL,      /* Task input parameter */
                            configMAX_PRIORITIES - 1, /* Priority of the task */
                            NULL,                     /* Task handle. */
                            QP_CPU_NUM); /* Core where the task should run */
#endif
}