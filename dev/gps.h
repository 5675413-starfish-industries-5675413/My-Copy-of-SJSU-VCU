#ifndef _GPS_H
#define _GPS_H

/*****************************************************************************
 * gps.h - GPS Position Data from MoTeC DAQ
 * Receives GPS Latitude and Longitude over CAN from MoTeC C185
 * CAN ID 0x650: GPS Position (Lat + Long as signed 32-bit, resolution 1e-7 deg)
 *****************************************************************************/

#include "IO_Driver.h"
#include "IO_CAN.h"

typedef struct _GPS GPS;

// Constructor
GPS* GPS_new(void);

// CAN parsing - called from canManager when 0x650 is received
void GPS_parseCanMessage(GPS* me, IO_CAN_DATA_FRAME* msg);

// Getters
float GPS_getLatitude(GPS* me);
float GPS_getLongitude(GPS* me);
bool  GPS_isValid(GPS* me);

#endif // _GPS_H
