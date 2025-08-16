// Simplified motorController.h - Regen and Launch Control removed
#ifndef _MOTORCONTROLLER_H
#define _MOTORCONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

// Type definitions
typedef uint8_t ubyte1;
typedef uint16_t ubyte2; 
typedef uint32_t ubyte4;
typedef int8_t sbyte1;
typedef int16_t sbyte2;
typedef int32_t sbyte4;
typedef float float4;

// Regen mode
typedef enum { REGENMODE_OFF = 0, REGENMODE_FORMULAE, REGENMODE_HYBRID, REGENMODE_TESLA, REGENMODE_FIXED } RegenMode;

// Mock structures (simplified versions)
typedef struct {
    float4 travelPercent;
    bool calibrated;
    float4 outputPercent;
} TorqueEncoder;

typedef struct {
    float4 percent;
    bool calibrated;
} BrakePressureSensor;

typedef struct {
    float4 volume;
    ubyte4 duration;
    bool playing;
} ReadyToDriveSound;

typedef struct {
    bool connected;
    char buffer[256];
} SerialManager;

typedef struct {
    float4 sensorValue;
    bool calibrated;
    char name[32];
} Sensor;

typedef struct {
    ubyte2 id;
    ubyte1 data[8];
    ubyte1 length;
} IO_CAN_DATA_FRAME;

// Enums
typedef enum { ENABLED, DISABLED, UNKNOWN } Status;
typedef enum { CLOCKWISE, COUNTERCLOCKWISE, FORWARD, REVERSE, _0, _1 } Direction;

// Forward declaration
typedef struct _MotorController MotorController;

// Constructor
MotorController* MotorController_new(SerialManager* sm, ubyte2 canMessageBaseID, Direction initialDirection, sbyte2 torqueMaxInDNm, sbyte1 minRegenSpeedKPH, sbyte1 regenRampdownStartSpeed);

//----------------------------------------------------------------------------
// Command Functions
//----------------------------------------------------------------------------
void MCM_commands_setTorqueDNm(MotorController* me, sbyte2 torque);
void MCM_commands_setDirection(MotorController* me, Direction rotation);
void MCM_commands_setInverter(MotorController* me, Status inverterState);
void MCM_commands_setDischarge(MotorController* me, Status dischargeState);
void MCM_commands_setTorqueLimit(MotorController* me, sbyte2 torqueLimit);

sbyte2 MCM_commands_getTorque(MotorController* me);
Direction MCM_commands_getDirection(MotorController* me);
Status MCM_commands_getInverter(MotorController* me);
Status MCM_commands_getDischarge(MotorController* me);
sbyte2 MCM_commands_getTorqueLimit(MotorController* me);

ubyte2 MCM_commands_getUpdateCount(MotorController* me);
void MCM_commands_resetUpdateCountAndTime(MotorController* me);
ubyte4 MCM_commands_getTimeSinceLastCommandSent(MotorController* me);

//----------------------------------------------------------------------------
// Update Functions (CAN Inputs)
//----------------------------------------------------------------------------
void MCM_updateLockoutStatus(MotorController* me, Status newState);
void MCM_updateInverterStatus(MotorController* me, Status newState);

//----------------------------------------------------------------------------
// Status Functions
//----------------------------------------------------------------------------
Status MCM_getLockoutStatus(MotorController* me);
Status MCM_getInverterStatus(MotorController* me);

sbyte4 MCM_getPower(MotorController* me);
ubyte2 MCM_getCommandedTorque(MotorController* me);
sbyte2 MCM_getAppsTorque(MotorController* me);
sbyte2 MCM_getBpsTorque(MotorController* me);

bool MCM_getHvilOverrideStatus(MotorController* me);
Status MCM_getInverterOverrideStatus(MotorController* me);

void MCM_setRTDSFlag(MotorController* me, bool start);
bool MCM_getRTDSFlag(MotorController* me);

sbyte2 MCM_getTemp(MotorController* me);
sbyte2 MCM_getMotorTemp(MotorController* me);

sbyte4 MCM_getGroundSpeedKPH(MotorController *me, sbyte4 motorRPM, sbyte4 FD_Ratio, sbyte4 Revolutions, sbyte4 tireCirc, sbyte4 KPH_Unit_Conversion);

//----------------------------------------------------------------------------
// Mutator Functions
//----------------------------------------------------------------------------
void MCM_setMaxTorqueDNm(MotorController* me, ubyte2 torque);
ubyte2 MCM_getMaxTorqueDNm(MotorController* me);
ubyte2 MCM_getTorqueMax(MotorController* me);

//----------------------------------------------------------------------------
// Inter-object functions
//----------------------------------------------------------------------------
void MCM_setRegenMode(MotorController *me, RegenMode regenMode);
void MCM_calculateCommands(MotorController *mcm, TorqueEncoder *tps, BrakePressureSensor *bps);
void MCM_relayControl(MotorController* mcm, Sensor* HVILTermSense);
void MCM_inverterControl(MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps, ReadyToDriveSound* rtds);
void MCM_parseCanMessage(MotorController* mcm, IO_CAN_DATA_FRAME* mcmCanMessage);

ubyte1 MCM_getStartupStage(MotorController* me);
void MCM_setStartupStage(MotorController* me, ubyte1 stage);

//----------------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------------
float4 getPercent(float4 input, float4 min, float4 max, bool increasing);

#endif // _MOTORCONTROLLER_H