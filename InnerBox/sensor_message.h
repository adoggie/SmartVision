//
// Created by scott on 2019/3/16.
//

#ifndef INNERPROC_SENSOR_MESSAGE_H
#define INNERPROC_SENSOR_MESSAGE_H


#include "sensor.h"

//下发开门指令
struct SensorMessageOpenDoor : MessagePayload {
    SensorMessageOpenDoor() {
        a = (int) MessageType::McuValueSet;
        d = "door";
        e = "1";
    }
};


MessagePayload::Ptr parseSensorMessage(const char *text);

#endif //INNERPROC_SENSOR_MESSAGE_H
