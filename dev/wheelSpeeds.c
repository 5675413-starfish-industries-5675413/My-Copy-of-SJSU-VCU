#include <stdlib.h> //Needed for malloc
#include <math.h>
#include "IO_RTC.h"
#include "IO_DIO.h"
#include "IO_CAN.h"


#include "wheelSpeeds.h"
#include "mathFunctions.h"

#include "sensors.h"
//extern Sensor Sensor_BPS0;
//extern Sensor Sensor_BenchTPS1;

/*****************************************************************************
* Wheel Speed object
******************************************************************************
* This object converts raw wheel speed sensor readings to usable formats
* for i.e. traction control
****************************************************************************/

struct _WheelSpeeds
{
    float4 tireCircumferenceMeters_F; //calculated
    float4 tireCircumferenceMeters_R; //calculated
    float4 pulsesPerRotation_F;
    float4 pulsesPerRotation_R;
    ubyte4 frequency_FL;
    ubyte4 frequency_FR;
    ubyte4 frequency_RL;
    ubyte4 frequency_RR;
    ubyte4 filteredFrequency_FL;
    ubyte4 filteredFrequency_FR;
    ubyte4 filteredFrequency_RL;
    ubyte4 filteredFrequency_RR;
    float4 smoothingFactor;
};

/*****************************************************************************
* Torque Encoder (TPS) functions
* RULE EV2.3.5:
* If an implausibility occurs between the values of these two sensors the power to the motor(s) must be immediately shut down completely.
* It is not necessary to completely deactivate the tractive system, the motor controller(s) shutting down the power to the motor(s) is sufficient.
****************************************************************************/
WheelSpeeds *WheelSpeeds_new(float4 tireDiameterInches_F, float4 tireDiameterInches_R, ubyte1 pulsesPerRotation_F, ubyte1 pulsesPerRotation_R)
{
    WheelSpeeds *me = (WheelSpeeds *)malloc(sizeof(struct _WheelSpeeds));

    //1 inch = .0254 m
    me->tireCircumferenceMeters_F = 3.14159 * (.0254 * tireDiameterInches_F);
    me->tireCircumferenceMeters_R = 3.14159 * (.0254 * tireDiameterInches_R);
    me->pulsesPerRotation_F = pulsesPerRotation_F;
    me->pulsesPerRotation_R = pulsesPerRotation_R;
    me->frequency_FL = 0;
    me->frequency_FR = 0;
    me->frequency_RL = 0;
    me->frequency_RR = 0;
    me->filteredFrequency_FL = 0;
    me->filteredFrequency_FR = 0;
    me->filteredFrequency_RL = 0;
    me->filteredFrequency_RR = 0;
    me->smoothingFactor = 0.2;

    //Turn on WSS power pins
    IO_DO_Set(IO_DO_07, TRUE); // WSS x4

    return me;
}

void WheelSpeeds_update(WheelSpeeds *me, bool interpolate)
{
    if (interpolate) {
        me->frequency_FL = Sensor_WSS_FL.heldSensorValue;
        me->frequency_FR = Sensor_WSS_FR.heldSensorValue;
        me->frequency_RL = Sensor_WSS_RL.heldSensorValue;
        me->frequency_RR = Sensor_WSS_RR.heldSensorValue;	
    }
    else {
        me->frequency_FL = Sensor_WSS_FL.sensorValue;
        me->frequency_FR = Sensor_WSS_FR.sensorValue;
        me->frequency_RL = Sensor_WSS_RL.sensorValue;
        me->frequency_RR = Sensor_WSS_RR.sensorValue;
    }

	me->filteredFrequency_FL = me->smoothingFactor * me->frequency_FL + (1 - me->smoothingFactor) * me->filteredFrequency_FL;
	me->filteredFrequency_FR = me->smoothingFactor * me->frequency_FR + (1 - me->smoothingFactor) * me->filteredFrequency_FR;
	me->filteredFrequency_RL = me->smoothingFactor * me->frequency_RL + (1 - me->smoothingFactor) * me->filteredFrequency_RL;
	me->filteredFrequency_RR = me->smoothingFactor * me->frequency_RR + (1 - me->smoothingFactor) * me->filteredFrequency_RR;
}


float4 WheelSpeeds_getWheelSpeedMPS(WheelSpeeds *me, Wheel corner, bool filter)
{
    float4 speed;
    ubyte4 frequency;
	//speed (m/s) = m * (pulses/sec)/pulses
    switch (corner) {
        case FL:
            frequency = filter ? me->filteredFrequency_FL : me->frequency_FL;
            speed = me->tireCircumferenceMeters_F * frequency / me->pulsesPerRotation_F;
            break;
        case FR:
            frequency = filter ? me->filteredFrequency_FR : me->frequency_FR;
            speed = me->tireCircumferenceMeters_F * frequency / me->pulsesPerRotation_F;
            break;
        case RL:
            frequency = filter ? me->filteredFrequency_RL : me->frequency_RL;
            speed = me->tireCircumferenceMeters_R * frequency / me->pulsesPerRotation_R;
            break;
        case RR:
            frequency = filter ? me->filteredFrequency_RR : me->frequency_RR;
            speed = me->tireCircumferenceMeters_R * frequency / me->pulsesPerRotation_R;
            break;
        default:
            speed = 0;
    }
    return speed;
}

float4 WheelSpeeds_getWheelSpeedRPM(WheelSpeeds *me, Wheel corner, bool filter)
{
    ubyte4 frequency;
	switch (corner)
	{
		case FL:
			frequency = filter ? me->filteredFrequency_FL : me->frequency_FL;
			break;
		case FR:
			frequency = filter ? me->filteredFrequency_FR : me->frequency_FR;
			break;
		case RL:
			frequency = filter ? me->filteredFrequency_RL : me->frequency_RL;
			break;
		case RR:
			frequency = filter ? me->filteredFrequency_RR : me->frequency_RR;
			break;
		default:
			frequency = 0;
	}
    //Multiply sensorValue by 60 seconds to get RPM (1 Hz per bump)
    return frequency*60.0f/NUM_BUMPS;
}

float4 WheelSpeeds_getSlowestFrontMPS(WheelSpeeds *me, bool filter)
{
	float4 speed_FL = WheelSpeeds_getWheelSpeedMPS(me, FL, filter);
	float4 speed_FR = WheelSpeeds_getWheelSpeedMPS(me, FR, filter);
    return (speed_FL < speed_FR) ? speed_FL : speed_FR;
}

float4 WheelSpeeds_getFastestRearMPS(WheelSpeeds *me, bool filter)
{
	float4 speed_RL = WheelSpeeds_getWheelSpeedMPS(me, RL, filter);
	float4 speed_RR = WheelSpeeds_getWheelSpeedMPS(me, RR, filter);
    return (speed_RL > speed_RR) ? speed_RL : speed_RR;
}

float4 WheelSpeeds_getFastestRearRPM(WheelSpeeds *me, bool filter) {
	float4 speed_RL = WheelSpeeds_getWheelSpeedRPM(me, RL, filter);
	float4 speed_RR = WheelSpeeds_getWheelSpeedRPM(me, RR, filter);
	return (speed_RL > speed_RR) ? speed_RL : speed_RR;
}


float4 WheelSpeeds_getGroundSpeedMPS(WheelSpeeds *me, ubyte1 tire_config, bool filter)
{
	float4 speed_FL = WheelSpeeds_getWheelSpeedMPS(me, FL, filter);
	float4 speed_FR = WheelSpeeds_getWheelSpeedMPS(me, FR, filter);
    switch (tire_config)
    {
        //Use both
        case 0:
            return (speed_FL + speed_FR) / 2.0f;

        //Use only FL
        case 1:
            return speed_FL;

        //Use only FR
        case 2:
            return speed_FR;
    }

    return 0;
}

float4 WheelSpeeds_getGroundSpeedKPH(WheelSpeeds *me, ubyte1 tire_config, bool filter)
{
    return (WheelSpeeds_getGroundSpeedMPS(me, tire_config, filter) * 3.6); //m/s to kph
}

float4 WheelSpeeds_getGroundSpeedRPM(WheelSpeeds *me, ubyte1 tire_config, bool filter) {
	float4 speed_FL = WheelSpeeds_getWheelSpeedRPM(me, FL, filter);
	float4 speed_FR = WheelSpeeds_getWheelSpeedRPM(me, FR, filter);
    switch (tire_config)
    {
        //Use both
        case 0:
            return (speed_FL + speed_FR) / 2.0f;

        //Use only FL
        case 1:
            return speed_FL;

        //Use only FR
        case 2:
            return speed_FR;
    }

    return 0;
}

bool WheelSpeeds_isWheelSpeedsNonZero(WheelSpeeds *me, bool filter) {
	float speed_FL = WheelSpeeds_getWheelSpeedMPS(me, FL, filter);
	float speed_FR = WheelSpeeds_getWheelSpeedMPS(me, FR, filter);
	float speed_RL = WheelSpeeds_getWheelSpeedMPS(me, RL, filter);
	float speed_RR = WheelSpeeds_getWheelSpeedMPS(me, RR, filter);
    return (speed_FL != 0 && speed_FR != 0 && speed_RL != 0 && speed_RR != 0);
}

void WheelSpeeds_parseCanMessage(WheelSpeeds *me, IO_CAN_DATA_FRAME *wssCanMessage) {
    switch (wssCanMessage->id) {
        case 0x705:
            Sensor_WSS_FL.heldSensorValue = ((ubyte2)wssCanMessage->data[0] << 8 | wssCanMessage->data[1]);
            Sensor_WSS_FR.heldSensorValue = ((ubyte2)wssCanMessage->data[2] << 8 | wssCanMessage->data[3]);
            Sensor_WSS_RL.heldSensorValue = ((ubyte2)wssCanMessage->data[4] << 8 | wssCanMessage->data[5]);
            Sensor_WSS_RR.heldSensorValue = ((ubyte2)wssCanMessage->data[6] << 8 | wssCanMessage->data[7]);
    }
}
