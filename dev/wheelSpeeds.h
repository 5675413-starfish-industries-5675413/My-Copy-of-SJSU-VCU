#ifndef _WHEELSPEEDS_H
#define _WHEELSPEEDS_H

#include "IO_Driver.h"
#include "sensors.h"
#include "sensorCalculations.h"
#include "IO_CAN.h"


#define METERS_IN_KILOMETER 1000.0f
#define MINUTES_IN_HOUR 60.0f
typedef enum { FL,FR,RL,RR } Wheel;

//After update(), access to tps Sensor objects should no longer be necessary.
//In other words, only updateFromSensors itself should use the tps Sensor objects
//Also, all values in the TorqueEncoder object are from 
typedef struct _WheelSpeeds WheelSpeeds;

WheelSpeeds* WheelSpeeds_new(float4 tireDiameterInches_F, float4 tireDiameterInches_R, ubyte1 pulsesPerRotation_F, ubyte1 pulsesPerRotation_R);
void WheelSpeeds_update(WheelSpeeds* me, bool interpolate);
float4 WheelSpeeds_getWheelSpeedMPS(WheelSpeeds *me, Wheel corner, bool filter);
float4 WheelSpeeds_getWheelSpeedRPM(WheelSpeeds *me, Wheel corner, bool filter);
float4 WheelSpeeds_getSlowestFrontMPS(WheelSpeeds *me, bool filter);
float4 WheelSpeeds_getFastestRearMPS(WheelSpeeds *me, bool filter);
float4 WheelSpeeds_getFastestRearRPM(WheelSpeeds *me, bool filter);
float4 WheelSpeeds_getGroundSpeedMPS(WheelSpeeds *me, ubyte1 tire_config, bool filter);
float4 WheelSpeeds_getGroundSpeedKPH(WheelSpeeds *me, ubyte1 tire_config, bool filter);
float4 WheelSpeeds_getGroundSpeedRPM(WheelSpeeds *me, ubyte1 tire_config, bool filter);
bool WheelSpeeds_isWheelSpeedsNonZero(WheelSpeeds *me, bool filter);
void WheelSpeeds_parseCanMessage(WheelSpeeds *me, IO_CAN_DATA_FRAME *wssCanMessage);
bool WheelSpeeds_isDAQWheelSpeedsNonZero(WheelSpeeds *me);
float4 WheelSpeeds_getDAQWheelSpeedRPM(WheelSpeeds *me, Wheel wheel);

#endif //  _WHEELSPEEDS_H
