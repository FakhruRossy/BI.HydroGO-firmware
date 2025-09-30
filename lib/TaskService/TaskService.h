#ifndef TASKSERVICE_H
#define TASKSERVICE_H

#include <Arduino.h>

class TaskService {
public:
    virtual ~TaskService() = default;

    void begin();
    void update();  // call in main loop()

    void setInterval(unsigned long intervalMs);

protected:
    virtual void onStart() = 0;
    virtual void onUpdate() = 0;

private:
    unsigned long lastUpdate = 0;
    unsigned long interval = 1000;
};

#endif  // TASKSERVICE_H
