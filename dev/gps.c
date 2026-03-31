/*****************************************************************************
 * gps.c - GPS Position Data from MoTeC DAQ
 ******************************************************************************
 * Parses GPS Latitude and Longitude from MoTeC CAN transmit message
 * CAN ID 0x650:
 *   Bytes 0-3: GPS Latitude  (signed 32-bit, base resolution 0.0000001 deg)
 *   Bytes 4-7: GPS Longitude (signed 32-bit, base resolution 0.0000001 deg)
 ****************************************************************************/

#include <stdlib.h>
#include "IO_Driver.h"
#include "IO_CAN.h"
#include "IO_RTC.h"
#include "gps.h"

#define GPS_CAN_ID_POSITION  0x650
#define GPS_RESOLUTION       0.0000001f  // MoTeC base resolution in degrees
#define GPS_TIMEOUT_US       2000000     // 2 seconds - mark invalid if no update

struct _GPS
{
    float latitude;       // Degrees (e.g., 37.4129557)
    float longitude;      // Degrees (e.g., -121.1081600)
    bool  valid;          // TRUE if we've received at least one message recently
    ubyte4 lastUpdateTime; // Timestamp of last CAN message received
};

GPS* GPS_new(void)
{
    GPS* me = (GPS*)malloc(sizeof(struct _GPS));
    if (me == NULL)
    {
        return NULL;
    }

    me->latitude = 0.0f;
    me->longitude = 0.0f;
    me->valid = FALSE;
    me->lastUpdateTime = 0;

    return me;
}

void GPS_parseCanMessage(GPS* me, IO_CAN_DATA_FRAME* msg)
{
    if (me == NULL || msg == NULL) return;

    if (msg->id == GPS_CAN_ID_POSITION)
    {
        // Bytes 0-3: Latitude as signed 32-bit integer
        sbyte4 rawLat = (sbyte4)(
            ((ubyte4)msg->data[0])       |
            ((ubyte4)msg->data[1] << 8)  |
            ((ubyte4)msg->data[2] << 16) |
            ((ubyte4)msg->data[3] << 24)
        );

        // Bytes 4-7: Longitude as signed 32-bit integer
        sbyte4 rawLon = (sbyte4)(
            ((ubyte4)msg->data[4])       |
            ((ubyte4)msg->data[5] << 8)  |
            ((ubyte4)msg->data[6] << 16) |
            ((ubyte4)msg->data[7] << 24)
        );

        me->latitude  = (float)rawLat * GPS_RESOLUTION;
        me->longitude = (float)rawLon * GPS_RESOLUTION;
        me->valid = TRUE;
        IO_RTC_StartTime(&me->lastUpdateTime);
    }
}

float GPS_getLatitude(GPS* me)
{
    return me->latitude;
}

float GPS_getLongitude(GPS* me)
{
    return me->longitude;
}

bool GPS_isValid(GPS* me)
{
    // Check if we've received a GPS message recently
    if (me->valid && IO_RTC_GetTimeUS(me->lastUpdateTime) > GPS_TIMEOUT_US)
    {
        me->valid = FALSE;
    }
    return me->valid;
}
