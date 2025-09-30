#include "TaskService.h"

void TaskService::begin() {
    lastUpdate = millis();
    onStart();
}

void TaskService::update() {
    unsigned long now = millis();
    if (now - lastUpdate >= interval) {
        lastUpdate = now;
        onUpdate();
    }
}

void TaskService::setInterval(unsigned long intervalMs) {
    interval = intervalMs;
}
