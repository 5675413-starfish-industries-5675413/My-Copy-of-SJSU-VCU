
#include <stdlib.h> //malloc

#include "IO_Driver.h" 
#include "IO_CAN.h"
#include "IO_RTC.h"

#include "mathFunctions.h"
#include "sensors.h"
#include "canManager.h"
#include "avlTree.h"
#include "motorController.h"
#include "bms.h"
#include "safety.h"
#include "wheelSpeeds.h"
#include "serial.h"
#include "sensorCalculations.h"
#include "LaunchControl.h"
#include "powerLimit.h"
#include "drs.h"
#include "PID.h"
#include "regen.h"
#include "efficiency.h"
#include "brakePressureSensor.h"
#include "shunt.h"


struct _CanManager {
    //AVLNode* incomingTree;
    //AVLNode* outgoingTree;


    SerialManager* sm;

    ubyte1 canMessageLimit;
    
    //These are our four FIFO queues.  All messages should come/go through one of these queues.
    //Functions shall have a CanChannel enum (see header) parameter.  Direction (send/receive is not
    //specified by this parameter.  The CAN0/CAN1 is selected based on the parameter passed in, and 
    //Read/Write is selected based on the function that is being called (get/send)
    ubyte1 can0_busSpeed;
    ubyte1 can0_readHandle;
    ubyte1 can0_read_messageLimit;
    ubyte1 can0_writeHandle;
    ubyte1 can0_write_messageLimit;

    ubyte1 can1_busSpeed;
    ubyte1 can1_readHandle;
    ubyte1 can1_read_messageLimit;
    ubyte1 can1_writeHandle;
    ubyte1 can1_write_messageLimit;
    
    IO_ErrorType ioErr_can0_Init;
    IO_ErrorType ioErr_can1_Init;

    IO_ErrorType ioErr_can0_fifoInit_R;
    IO_ErrorType ioErr_can0_fifoInit_W;
    IO_ErrorType ioErr_can1_fifoInit_R;
    IO_ErrorType ioErr_can1_fifoInit_W;

    IO_ErrorType ioErr_can0_read;
    IO_ErrorType ioErr_can0_write;
    IO_ErrorType ioErr_can1_read;
    IO_ErrorType ioErr_can1_write;

    ubyte4 sendDelayus;


    //WARNING: These values are not initialized - be careful to only access
    //pointers that have been previously assigned
    //AVLNode* canMessageHistory[0x7FF];
    AVLNode* canMessageHistory[0x7FF];
};

//Keep track of CAN message IDs, their data, and when they were last sent.
/*
struct _CanMessageNode
{
    IO_CAN_DATA_FRAME canMessage;
    ubyte4 timeBetweenMessages_Min;
    ubyte4 timeBetweenMessages_Max;
    ubyte1 lastMessage_data[8];
    ubyte4 lastMessage_timeStamp;
    canHistoryNode* left;
    canHistoryNode* right;
};
*/

CanManager* CanManager_new(ubyte2 can0_busSpeed, ubyte1 can0_read_messageLimit, ubyte1 can0_write_messageLimit
                         , ubyte2 can1_busSpeed, ubyte1 can1_read_messageLimit, ubyte1 can1_write_messageLimit
                         , ubyte4 defaultSendDelayus, SerialManager* serialMan) //ubyte4 defaultMinSendDelay, ubyte4 defaultMaxSendDelay)
{
    CanManager* me = (CanManager*)malloc(sizeof(struct _CanManager));

    me->sm = serialMan;
    SerialManager_send(me->sm, "CanManager's reference to SerialManager was created.\n");
    
    //create can history data structure (AVL tree?)
    //me->incomingTree = NULL;
    //me->outgoingTree = NULL;
    for (ubyte4 id = 0; id <= 0x7FF; id++)
    {
        me->canMessageHistory[id] = 0;
    }

    me->sendDelayus = defaultSendDelayus;

    //Activate the CAN channels --------------------------------------------------
    me->ioErr_can0_Init = IO_CAN_Init(IO_CAN_CHANNEL_0, can0_busSpeed, 0, 0, 0);
    me->ioErr_can1_Init = IO_CAN_Init(IO_CAN_CHANNEL_1, can1_busSpeed, 0, 0, 0);

    //Configure the FIFO queues
    //This specifies: The handle names for the queues
    //, which channel the queue belongs to
    //, the # of messages (or maximum count?)
    //, the direction of the queue (in/out)
    //, the frame size
    //, and other stuff?
    IO_CAN_ConfigFIFO(&me->can0_readHandle, IO_CAN_CHANNEL_0, can0_read_messageLimit, IO_CAN_MSG_READ, IO_CAN_STD_FRAME, 0, 0);
    IO_CAN_ConfigFIFO(&me->can0_writeHandle, IO_CAN_CHANNEL_0, can0_write_messageLimit, IO_CAN_MSG_WRITE, IO_CAN_STD_FRAME, 0, 0);
    IO_CAN_ConfigFIFO(&me->can1_readHandle, IO_CAN_CHANNEL_1, can1_read_messageLimit, IO_CAN_MSG_READ, IO_CAN_STD_FRAME, 0, 0);
    IO_CAN_ConfigFIFO(&me->can1_writeHandle, IO_CAN_CHANNEL_1, can1_write_messageLimit, IO_CAN_MSG_WRITE, IO_CAN_STD_FRAME, 0, 0);

    //Assume read/write at error state until used
    me->ioErr_can0_read = IO_E_CAN_BUS_OFF;
    me->ioErr_can0_write = IO_E_CAN_BUS_OFF;
    me->ioErr_can1_read = IO_E_CAN_BUS_OFF;
    me->ioErr_can1_write = IO_E_CAN_BUS_OFF;

    //-------------------------------------------------------------------
    //Define default messages
    //-------------------------------------------------------------------
    //AVLNode* insertedMessage;
    //insertedMessage = AVL_insert(me->canMessageHistory, 0x0C0, 0, 50000, 125000, TRUE); //MCM command message
    
    ubyte2 messageID;
    //Outgoing ----------------------------
    messageID = 0xC0;  //MCM Command Message
    me->canMessageHistory[messageID]->timeBetweenMessages_Min = 25000;
    me->canMessageHistory[messageID]->timeBetweenMessages_Max = 125000;
    me->canMessageHistory[messageID]->required = TRUE;
    for (ubyte1 i = 0; i <= 7; i++) { me->canMessageHistory[messageID]->data[i] = 0; }
    IO_RTC_StartTime(&me->canMessageHistory[messageID]->lastMessage_timeStamp);

    for (messageID = 0x500; messageID <= 0x515; messageID++)
    {
        me->canMessageHistory[messageID]->timeBetweenMessages_Min = 50000;
        me->canMessageHistory[messageID]->timeBetweenMessages_Max = 250000;
        me->canMessageHistory[messageID]->required = TRUE;
        for (ubyte1 i = 0; i <= 7; i++) { me->canMessageHistory[messageID]->data[i] = 0; }
        IO_RTC_StartTime(&me->canMessageHistory[messageID]->lastMessage_timeStamp);
    }

    //Incoming ----------------------------
    messageID = 0xAA;  //MCM ______
    me->canMessageHistory[messageID]->timeBetweenMessages_Min = 0;
    me->canMessageHistory[messageID]->timeBetweenMessages_Max = 500000;
    me->canMessageHistory[messageID]->required = TRUE;
    for (ubyte1 i = 0; i <= 7; i++) { me->canMessageHistory[messageID]->data[i] = 0; }
    IO_RTC_StartTime(&me->canMessageHistory[messageID]->lastMessage_timeStamp);

    messageID = 0xAB;  //MCM ________
    me->canMessageHistory[messageID]->timeBetweenMessages_Min = 0;
    me->canMessageHistory[messageID]->timeBetweenMessages_Max = 500000;
    me->canMessageHistory[messageID]->required = TRUE;
    for (ubyte1 i = 0; i <= 7; i++) { me->canMessageHistory[messageID]->data[i] = 0; }
    IO_RTC_StartTime(&me->canMessageHistory[messageID]->lastMessage_timeStamp);

    messageID = 0x623;  //BMS faults
    me->canMessageHistory[messageID]->timeBetweenMessages_Min = 0;
    me->canMessageHistory[messageID]->timeBetweenMessages_Max = 5000000;
    me->canMessageHistory[messageID]->required = TRUE;
    for (ubyte1 i = 0; i <= 7; i++) { me->canMessageHistory[messageID]->data[i] = 0; }
    IO_RTC_StartTime(&me->canMessageHistory[messageID]->lastMessage_timeStamp);

    messageID = 0x629;  //BMS details
    me->canMessageHistory[messageID]->timeBetweenMessages_Min = 0;
    me->canMessageHistory[messageID]->timeBetweenMessages_Max = 1000000;
    me->canMessageHistory[messageID]->required = TRUE;
    for (ubyte1 i = 0; i <= 7; i++) { me->canMessageHistory[messageID]->data[i] = 0; }
    IO_RTC_StartTime(&me->canMessageHistory[messageID]->lastMessage_timeStamp);

    return me;
}


/*****************************************************************************
* This function takes an array of messages, determines which messages to send
* based on whether or not data has changed since the last time it was sent,
* or if a certain amount of time has passed since the last time it was sent.
*
* Messages that need to be sent are copied to another array and passed to the
* FIFO queue.
*
* Note: http://stackoverflow.com/questions/5573310/difference-between-passing-array-and-array-pointer-into-function-in-c
* http://stackoverflow.com/questions/2360794/how-to-pass-an-array-of-struct-using-pointer-in-c-c
****************************************************************************/
IO_ErrorType CanManager_send(CanManager* me, CanChannel channel, IO_CAN_DATA_FRAME canMessages[], ubyte1 canMessageCount)
{
    //SerialManager_send(me->sm, "Do you even send?\n");
    bool sendSerialDebug = FALSE;
    ubyte2 serialMessageID = 0xC0;
    bool sendMessage = FALSE;
    ubyte1 messagesToSendCount = 0;
    IO_CAN_DATA_FRAME messagesToSend[canMessageCount];//[channel == CAN0_HIPRI ? me->can0_write_messageLimit : me->can1_write_messageLimit];

    //----------------------------------------------------------------------------
    // Check if message exists in outgoing message history tree
    //----------------------------------------------------------------------------
    AVLNode* lastMessage;  //replace with me->canMessageHistory[ID]
    ubyte1 messagePosition; //used twice
    for (messagePosition = 0; messagePosition < canMessageCount; messagePosition++)
    {
        bool firstTimeMessage = FALSE;
        bool dataChanged = FALSE;
        bool minTimeExceeded = FALSE;
        bool maxTimeExceeded = FALSE;

        ubyte2 outboundMessageID = canMessages[messagePosition].id;
        lastMessage = me->canMessageHistory[outboundMessageID];
        sendMessage = FALSE;

        //----------------------------------------------------------------------------
        // Check if this message exists in the array
        //----------------------------------------------------------------------------
        firstTimeMessage = (me->canMessageHistory[outboundMessageID] == 0);  //############################## ALWAYS FALSE? ##############################
        if (firstTimeMessage)
        {
            me->canMessageHistory[outboundMessageID]->timeBetweenMessages_Min = 25000;
            me->canMessageHistory[outboundMessageID]->timeBetweenMessages_Max = 125000;
            me->canMessageHistory[outboundMessageID]->required = TRUE;
            for (ubyte1 i = 0; i <= 7; i++) { me->canMessageHistory[outboundMessageID]->data[i] = 0; }
            //IO_RTC_StartTime(&me->canMessageHistory[outboundMessageID]->lastMessage_timeStamp);
            me->canMessageHistory[outboundMessageID]->lastMessage_timeStamp = 0;
        }

        //----------------------------------------------------------------------------
        // Check if data has changed since last time message was sent
        //----------------------------------------------------------------------------
        //Check each data byte in the data array
        for (ubyte1 dataPosition = 0; dataPosition < 8; dataPosition++)
        {
            ubyte1 oldData = lastMessage->data[dataPosition];
            ubyte1 newData = canMessages[messagePosition].data[dataPosition];
            //if any data byte is changed, then probably want to send the message
            if (oldData == newData)
            {
                //data has not changed.  No action required (DO NOT SET)
                //dataChanged = FALSE;
            }
            else
            {
                dataChanged = TRUE; //ONLY MODIFY IF CHANGED
            }
        }//end checking each byte in message

        //----------------------------------------------------------------------------
        // Check if time has exceeded
        //----------------------------------------------------------------------------
        minTimeExceeded = ((IO_RTC_GetTimeUS(lastMessage->lastMessage_timeStamp) >= lastMessage->timeBetweenMessages_Min));
        maxTimeExceeded = ((IO_RTC_GetTimeUS(lastMessage->lastMessage_timeStamp) >= 50000));//lastMessage->timeBetweenMessages_Max));
        
        //----------------------------------------------------------------------------
        // If any criteria were exceeded, send the message out
        //----------------------------------------------------------------------------
        if (  (firstTimeMessage)
           || (dataChanged && minTimeExceeded)
           || (!dataChanged && maxTimeExceeded)
           )
        {
            sendMessage = TRUE;
        }

        //----------------------------------------------------------------------------
        // If we determined that this message should be sent
        //----------------------------------------------------------------------------
        if (sendMessage == TRUE)
        {
            //copy the message that needs to be sent into the outgoing messages array
            //see http://stackoverflow.com/questions/1693853/copying-arrays-of-structs-in-c
            //http://www.socialledge.com/sjsu/index.php?title=ES101_-_Lesson_9_:_Structures
            messagesToSend[messagesToSendCount++] = canMessages[messagePosition];
        }
        else
        {
            if (sendSerialDebug && (serialMessageID == outboundMessageID)) {
                SerialManager_send(me->sm, "This message did not need to be sent.\n");
            }
        }
    } //end of loop for each message in outgoing messages

    IO_UART_Task();
    //----------------------------------------------------------------------------
    // If there are messages to send
    //----------------------------------------------------------------------------
    IO_ErrorType sendResult = IO_E_OK;
    if (messagesToSendCount > 0)
    {
        //Send the messages to send to the appropriate FIFO queue
        sendResult = IO_CAN_WriteFIFO((channel == CAN0_HIPRI) ? me->can0_writeHandle : me->can1_writeHandle, messagesToSend, messagesToSendCount);
        *((channel == CAN0_HIPRI) ? &me->ioErr_can0_write : &me->ioErr_can1_write) = sendResult;

        //Update the outgoing message tree with message sent timestamps
        if ((channel == CAN0_HIPRI ? me->ioErr_can0_write : me->ioErr_can1_write) == IO_E_OK)
        {
            //Loop through the messages that we sent...
            ///////////AVLNode* messageToUpdate;
            for (messagePosition = 0; messagePosition < messagesToSendCount; messagePosition++)
            {
                //...find the message ID in the outgoing message tree again (big inefficiency here)...
                //////////messageToUpdate = AVL_find(me->outgoingTree, messagesToSend[messagePosition].id);

                //and update the message sent timestamp
                /////////////IO_RTC_GetTimeUS(messageToUpdate->lastMessage_timeStamp); //Update the timestamp for when the message was last sent
                //IO_RTC_GetTimeUS(me->canMessageHistory[messagesToSend[messagePosition].id]->lastMessage_timeStamp);
                IO_RTC_StartTime(&me->canMessageHistory[messagesToSend[messagePosition].id]->lastMessage_timeStamp);
            }
        }
    }
    return sendResult;
}

/*
//Helper functions
ubyte4 CanManager_timeSinceLastTransmit(IO_CAN_DATA_FRAME* canMessage)  //Overflows/resets at 74 min
bool CanManager_enoughTimeSinceLastTransmit(IO_CAN_DATA_FRAME* canMessage) // timesincelast > timeBetweenMessages_Min
bool CanManager_dataChangedSinceLastTransmit(IO_CAN_DATA_FRAME* canMessage) //bitwise comparison for all data bytes
*/


/*****************************************************************************
* read
****************************************************************************/
void CanManager_read(CanManager* me, CanChannel channel, MotorController* mcm, InstrumentCluster* ic, BatteryManagementSystem* bms, SafetyChecker* sc, WheelSpeeds* wss, Shunt* shunt)
{
    IO_CAN_DATA_FRAME canMessages[(channel == CAN0_HIPRI ? me->can0_read_messageLimit : me->can1_read_messageLimit)];
    ubyte1 canMessageCount;
      //FIFO queue only holds 128 messages max

    //Read messages from hipri channel 
    *(channel == CAN0_HIPRI ? &me->ioErr_can0_read : &me->ioErr_can1_read) =
    IO_CAN_ReadFIFO((channel == CAN0_HIPRI ? me->can0_readHandle : me->can1_writeHandle)
                    , canMessages
                    , (channel == CAN0_HIPRI ? me->can0_read_messageLimit : me->can1_read_messageLimit)
                    , &canMessageCount);

    //Determine message type based on ID
    for (int currMessage = 0; currMessage < canMessageCount; currMessage++)
    {
        switch (canMessages[currMessage].id)
        {
        //-------------------------------------------------------------------------
        //Motor controller
        //-------------------------------------------------------------------------
        case 0xA0:
        case 0xA1:
        case 0xA2:
            MCM_parseCanMessage(mcm, &canMessages[currMessage]); //check if this and following messages needs break statement
        case 0xA3:
        case 0xA4:
        case 0xA5:
        case 0xA6:
            MCM_parseCanMessage(mcm, &canMessages[currMessage]);
        case 0xA7:
            MCM_parseCanMessage(mcm, &canMessages[currMessage]);
        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xAB:
        case 0xAC:
        case 0xAD:
        case 0xAE:
        case 0xAF:
            MCM_parseCanMessage(mcm, &canMessages[currMessage]);
            break;

		case 0x578:
			WheelSpeeds_parseCanMessage(wss, &canMessages[currMessage]);
			break;
        //-------------------------------------------------------------------------
        //BMS
        //-------------------------------------------------------------------------
        case 0x600:
        case 0x602: //Faults
            BMS_parseCanMessage(bms, &canMessages[currMessage]);
            break;
        case 0x604:
        case 0x608:
        case 0x610:
        case 0x611:
        case 0x612:
        case 0x613:
        case 0x620:
        case 0x621:
        case 0x622: //Cell Voltage Summary
            BMS_parseCanMessage(bms, &canMessages[currMessage]);
            break;
        case 0x623: //Cell Temperature Summary
            BMS_parseCanMessage(bms, &canMessages[currMessage]);
            break;
        case 0x624:
        //1st Module
        case 0x630:
        case 0x631:
        case 0x632:
        //2nd Module
        case 0x633:
        case 0x634:
        case 0x635:
        //3rd Module
        case 0x636:
        case 0x637:
        case 0x638:
        //4th Module
        case 0x639:
        case 0x63A:
        case 0x63B:
        //5th Module
        case 0x63C:
        case 0x63D:
        case 0x63E:
        //6th Module
        case 0x63F:
        case 0x640:
        case 0x641:
        //7th Module
        case 0x642:
        case 0x643:
        case 0x644:
        //8th Module
        case 0x645:
        case 0x646:
        case 0x647:

        case 0x629:
            BMS_parseCanMessage(bms, &canMessages[currMessage]);
            break;

        case 0x702:
            IC_parseCanMessage(ic, mcm, &canMessages[currMessage]);
            break;
        case 0x703:
            IC_parseCanMessage(ic, mcm, &canMessages[currMessage]);
            break;
        case 0x704:
            IC_parseCanMessage(ic, mcm, &canMessages[currMessage]);
            break;
        case 0x705:
            WheelSpeeds_parseCanMessage(wss, &canMessages[currMessage]);
            break;
            
        //-------------------------------------------------------------------------
        // Shunt
        //-------------------------------------------------------------------------
        case 0x3F1:
            Shunt_parseCanMessage(shunt, &canMessages[currMessage]);
            break;
        case 0x3F4:
            Shunt_parseCanMessage(shunt, &canMessages[currMessage]);
            break;
            
        //-------------------------------------------------------------------------
        //VCU Debug Control
        //-------------------------------------------------------------------------
        case 0x5FF:
            SafetyChecker_parseCanMessage(sc, &canMessages[currMessage]);
            MCM_parseCanMessage(mcm, &canMessages[currMessage]);
            break;
            //default:
        }
    }

    //Echo message on lopri channel
    //IO_CAN_WriteFIFO(me->can1_writeHandle, canMessages, messagesReceived);
    // CanManager_send(me, CAN1_LOPRI, canMessages, canMessageCount);
    //IO_CAN_WriteMsg(canFifoHandle_LoPri_Write, canMessages);
}

ubyte1 CanManager_getReadStatus(CanManager* me, CanChannel channel)
{
    return (channel == CAN0_HIPRI) ? me->ioErr_can0_read : me->ioErr_can1_read;
}




/*****************************************************************************
* device-specific functions
****************************************************************************/
/*****************************************************************************
* Standalone Sensor messages
******************************************************************************
* Load sensor values into CAN messages
* Each can message's .data[] holds 1 byte - sensor data must be broken up into separate bytes
* The message addresses are at:
* https://docs.google.com/spreadsheets/d/1sYXx191RtMq5Vp5PbPsq3BziWvESF9arZhEjYUMFO3Y/edit
****************************************************************************/
void canOutput_sendSensorMessages(CanManager* me)
{

}


//----------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------
void canOutput_sendDebugMessage(CanManager* me, TorqueEncoder* tps, BrakePressureSensor* bps, MotorController* mcm, InstrumentCluster* ic, BatteryManagementSystem* bms, WheelSpeeds* wss, SafetyChecker* sc, LaunchControl* lc, PowerLimit *pl, DRS *drs, Regen *regen, Efficiency *eff)
{
    IO_CAN_DATA_FRAME canMessages[me->can0_write_messageLimit];
    ubyte1 errorCount;
    float4 tempPedalPercent;   //Pedal percent float (a decimal between 0 and 1
    ubyte1 tps0Percent;  //Pedal percent int   (a number from 0 to 100)
    ubyte1 tps1Percent;
    ubyte2 canMessageCount = 0;
    // ubyte2 canMessage1Count = 0;
    ubyte2 canMessageID = 0x500;
    ubyte1 byteNum;
    // ubyte1 byteNum1;

    TorqueEncoder_getIndividualSensorPercent(tps, 0, &tempPedalPercent); //borrow the pedal percent variable
    tps0Percent = 0xFF * tempPedalPercent;
    TorqueEncoder_getIndividualSensorPercent(tps, 1, &tempPedalPercent);
    tps1Percent = 0xFF * (tempPedalPercent);
    //tps1Percent = 0xFF * (1 - tempPedalPercent);  //OLD: flipped over pedal percent (this value for display in CAN only)

    TorqueEncoder_getPedalTravel(tps, &errorCount, &tempPedalPercent); //getThrottlePercent(TRUE, &errorCount);
    ubyte1 throttlePercent = 0xFF * tempPedalPercent;

    BrakePressureSensor_getPedalTravel(bps, &errorCount, &tempPedalPercent); //getThrottlePercent(TRUE, &errorCount);
    ubyte1 brakePercent = 0xFF * tempPedalPercent;

    //500: TPS 0
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1; //500
    canMessages[canMessageCount - 1].data[byteNum++] = throttlePercent;
    canMessages[canMessageCount - 1].data[byteNum++] = tps0Percent;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_TPS0.sensorValue; // tps->tps0_value;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_TPS0.sensorValue >> 8; //tps->tps0_value >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMin;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMin >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMax;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMax >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    //501: TPS 1
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = throttlePercent;
    canMessages[canMessageCount - 1].data[byteNum++] = tps1Percent;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_value;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_value >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMin;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMin >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMax;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMax >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    //502: BPS0
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = brakePercent; //This should be bps0Percent, but for now bps0Percent = brakePercent
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps0_voltage;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps0_voltage >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps0_calibMin;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps0_calibMin >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps0_calibMax;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps0_calibMax >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    //503: WSS mm/s unfiltered output 
    // canMessageCount++;
    // byteNum = 0;
    // canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    // canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, FL, FALSE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, FL, FALSE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, FR, FALSE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, FR, FALSE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, RL, FALSE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, RL, FALSE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, RR, FALSE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedMPS(wss, RR, FALSE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].length = byteNum;

    // 503 FREE MESSAGE
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].length = byteNum;

    //504: DAQ Wheel Speeds
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getDAQWheelSpeedRPM(wss, FL));
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)((ubyte2)WheelSpeeds_getDAQWheelSpeedRPM(wss, FL) >> 8);
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getDAQWheelSpeedRPM(wss, FR));
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)((ubyte2)WheelSpeeds_getDAQWheelSpeedRPM(wss, FR) >> 8); 
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getDAQWheelSpeedRPM(wss, RR));
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)((ubyte2)WheelSpeeds_getDAQWheelSpeedRPM(wss, RR) >> 8); 
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getDAQWheelSpeedRPM(wss, RL));
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)((ubyte2)WheelSpeeds_getDAQWheelSpeedRPM(wss, RL) >> 8); 
    canMessages[canMessageCount - 1].length = byteNum;

    /*
    //TEMP: WSS3
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;  //505
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RL.sensorValue;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RL.sensorValue >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RL.sensorValue >> 16;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RL.sensorValue >> 24;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RR.sensorValue;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RR.sensorValue >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RR.sensorValue >> 16;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_RR.sensorValue >> 24;
    canMessages[canMessageCount - 1].length = byteNum;
    */
    /*
    //TEMP: FRONT WSS PIN DEBUG
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;  //505
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FL.ioErr_signalInit;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FL.ioErr_signalInit >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FL.ioErr_signalGet;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FL.ioErr_signalGet >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FR.ioErr_signalInit;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FR.ioErr_signalInit >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FR.ioErr_signalGet;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_WSS_FR.ioErr_signalGet >> 8;
    canMessages[canMessageCount - 1].length = byteNum;
    */

    //505: WSS RPM filtered output (DONT NEED TO LOGGED ON DAQ)
    // canMessageCount++;
    // byteNum = 0;
    // canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    // canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, FR, TRUE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, FR, TRUE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, RL, TRUE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, RL, TRUE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, RR, TRUE) + 0.5);
    // canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, RR, TRUE) + 0.5)) >> 8;
    // canMessages[canMessageCount - 1].length = byteNum;

    //505 FREE MESSAGE
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].length = byteNum;

    //506: Safety Checker
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getFaults(sc);
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getFaults(sc) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getFaults(sc) >> 16;
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getFaults(sc) >> 24;
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getWarnings(sc);
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getWarnings(sc) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getNotices(sc);
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getNotices(sc) >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    //12v battery
    float4 LVBatterySOC = 0;
    if (Sensor_LVBattery.sensorValue < 12730)
        LVBatterySOC = .0 + .1 * getPercent(Sensor_LVBattery.sensorValue, 9200, 12730, FALSE);
    else if (Sensor_LVBattery.sensorValue < 12866)
        LVBatterySOC = .1 + .1 * getPercent(Sensor_LVBattery.sensorValue, 12730, 12866, FALSE);
    else if (Sensor_LVBattery.sensorValue < 12996)
        LVBatterySOC = .2 + .1 * getPercent(Sensor_LVBattery.sensorValue, 12866, 12996, FALSE);
    else if (Sensor_LVBattery.sensorValue < 13104)
        LVBatterySOC = .3 + .1 * getPercent(Sensor_LVBattery.sensorValue, 12996, 13104, FALSE);
    else if (Sensor_LVBattery.sensorValue < 13116)
        LVBatterySOC = .4 + .1 * getPercent(Sensor_LVBattery.sensorValue, 13104, 13116, FALSE);
    else if (Sensor_LVBattery.sensorValue < 13130)
        LVBatterySOC = .5 + .1 * getPercent(Sensor_LVBattery.sensorValue, 13116, 13130, FALSE);
    else if (Sensor_LVBattery.sensorValue < 13160)
        LVBatterySOC = .6 + .1 * getPercent(Sensor_LVBattery.sensorValue, 13130, 13160, FALSE);
    else if (Sensor_LVBattery.sensorValue < 13270)
        LVBatterySOC = .7 + .1 * getPercent(Sensor_LVBattery.sensorValue, 13160, 13270, FALSE);
    else if (Sensor_LVBattery.sensorValue < 13300)
        LVBatterySOC = .8 + .1 * getPercent(Sensor_LVBattery.sensorValue, 13270, 13300, FALSE);
    else //if (Sensor_LVBattery.sensorValue < 14340)
        LVBatterySOC = .9 + .1 * getPercent(Sensor_LVBattery.sensorValue, 13300, 14340, FALSE);
    Sensor_LVBattery.sensorValue = Sensor_LVBattery.sensorValue + 0.46;
    //507: RegenTQ+LV battery 
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)Sensor_LVBattery.sensorValue;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_LVBattery.sensorValue >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] =  (sbyte2)(Regen_get_torqueCommand(regen));
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(Regen_get_torqueCommand(regen))) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = 
    canMessages[canMessageCount - 1].data[byteNum++] = 
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].length = byteNum;

    //508: Regen settings
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] =  (ubyte2)(BrakePressureSensor_getBPS0_Pressure(bps));
    canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(BrakePressureSensor_getBPS0_Pressure(bps))) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] =  (ubyte2)(BrakePressureSensor_getBPS1_Pressure(bps));
    canMessages[canMessageCount - 1].data[byteNum++] = ((ubyte2)(BrakePressureSensor_getBPS1_Pressure(bps))) >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    //509: MCM RTD Status
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_HVILTerminationSense.sensorValue;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_HVILTerminationSense.sensorValue >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = MCM_getHvilOverrideStatus(mcm);
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)MCM_getCommandedTorque(mcm); //+
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(MCM_getCommandedTorque(mcm) >> 8);
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(MCM_getGroundSpeedKPH(mcm) * 100); //+
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte2)(MCM_getGroundSpeedKPH(mcm) * 100) >> 8; 
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].length = byteNum;

    // 50A: Efficiency Status A
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(Efficiency_getLapCounter(eff));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(Efficiency_getCornerEnergy_kWh(eff) * 1000); // Convert to Wh
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(Efficiency_getCornerEnergy_kWh(eff) * 1000)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(Efficiency_getLapEnergy_kWh(eff) * 1000); // Convert to Wh
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(Efficiency_getLapEnergy_kWh(eff) * 1000)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(Efficiency_getFinishedLap(eff) ? 1 : 0);
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(Efficiency_getLapDistance_km(eff) * 100); // Convert to 0.01km units
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(Efficiency_getLapDistance_km(eff) * 100)) >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    //50B: Launch Control Status A
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(LaunchControl_getTorqueCommand(lc));
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(LaunchControl_getTorqueCommand(lc))) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(LaunchControl_getSlipRatioScaled(lc));
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(LaunchControl_getSlipRatioScaled(lc))) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(LaunchControl_getState(lc));
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(LaunchControl_getPhase(lc));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(LaunchControl_getSpeedCommand(lc));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(LaunchControl_getSpeedCommand(lc) >> 8);
    canMessages[canMessageCount - 1].length = byteNum;

    //50C: SAS 
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].data[byteNum++] = steering_degrees(); //MOVE LATER
    canMessages[canMessageCount - 1].data[byteNum++] = steering_degrees() >> 8; 
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].length = byteNum;

    //50D: BPS1 (TEMPORARY ADDRESS)
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = brakePercent; //This should be bps0Percent, but for now bps0Percent = brakePercent
    canMessages[canMessageCount - 1].data[byteNum++] = 0;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps1_voltage;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps1_voltage >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps1_calibMin;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps1_calibMin >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps1_calibMax;
    canMessages[canMessageCount - 1].data[byteNum++] = bps->bps1_calibMax >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    
    //50E: Efficiency Status B
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLongitude(eff));
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLongitude(eff)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLongitude(eff)) >> 16;
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLongitude(eff)) >> 24;
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLatitude(eff));
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLatitude(eff)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLatitude(eff)) >> 16;
    canMessages[canMessageCount - 1].data[byteNum++] = (Efficiency_getLapLatitude(eff)) >> 24;
    canMessages[canMessageCount - 1].length = byteNum;

    //50F: MCM Power Debug
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].data[byteNum++] = MCM_getPower(mcm);
    canMessages[canMessageCount - 1].data[byteNum++] = (MCM_getPower(mcm) >> 8);
    canMessages[canMessageCount - 1].data[byteNum++] = (MCM_getPower(mcm) >> 16);
    canMessages[canMessageCount - 1].data[byteNum++] = (MCM_getPower(mcm) >> 24);
    canMessages[canMessageCount - 1].data[byteNum++] = SafetyChecker_getWarnings(sc);
    canMessages[canMessageCount - 1].data[byteNum++] = (SafetyChecker_getWarnings(sc) >> 8);
    canMessages[canMessageCount - 1].data[byteNum++] = (SafetyChecker_getWarnings(sc) >> 16);
    canMessages[canMessageCount - 1].data[byteNum++] = (SafetyChecker_getWarnings(sc) >> 24);
    canMessages[canMessageCount - 1].length = byteNum;

    //510: Motor controller command message
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = 0xC0;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)MCM_commands_getTorque(mcm); // When speed mode is enabled, torque command is added to speed mode torque output
    canMessages[canMessageCount - 1].data[byteNum++] = MCM_commands_getTorque(mcm) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)MCM_commands_getSpeed(mcm);
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(MCM_commands_getSpeed(mcm) >> 8);
    canMessages[canMessageCount - 1].data[byteNum++] = 0;  //Motor direction (0 = Forward, 1 = Reverse). Opposite of what is written in CM200 documentation
    canMessages[canMessageCount - 1].data[byteNum++] = (MCM_getSpeedModeEnabled(mcm) << 2) | (0 << 1) | ((MCM_commands_getInverter(mcm) == ENABLED) << 0); //unused/unused/unused/unused/unused/Speed Mode Enable/Discharge/Inverter Enable
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)MCM_commands_getTorqueLimit(mcm);
    canMessages[canMessageCount - 1].data[byteNum++] = MCM_commands_getTorqueLimit(mcm) >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

   // 511 : Power Limit Status A
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(POWERLIMIT_getInitialisationThreshold(pl));
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(POWERLIMIT_getMode(pl));
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(POWERLIMIT_getStatus(pl));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getTotalError(pl->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getTotalError(pl->pid)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getProportional(pl->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getProportional(pl->pid)) >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    // 512 : Power Limit Status B
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getIntegral(pl->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getIntegral(pl->pid)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(POWERLIMIT_getTorqueCommand(pl));
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(POWERLIMIT_getTorqueCommand(pl))) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(POWERLIMIT_getTargetPower(pl));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getOutput(pl->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getOutput(pl->pid)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (ubyte1)(POWERLIMIT_getClampingMethod(pl));
    canMessages[canMessageCount - 1].length = byteNum;

    // 513 : Launch Control Status B
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getProportional(lc->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getProportional(lc->pid)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getIntegral(lc->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(PID_getIntegral(lc->pid))) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getTotalError(lc->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getTotalError(lc->pid)) >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = (sbyte2)(PID_getOutput(lc->pid));
    canMessages[canMessageCount - 1].data[byteNum++] = ((sbyte2)(PID_getOutput(lc->pid))) >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    CanManager_send(me, CAN0_HIPRI, canMessages, canMessageCount); 


    //----------------------------------------------------------------------------
    //Additional sensors
    //----------------------------------------------------------------------------

    //Place the can messsages into the FIFO queue ---------------------------------------------------
    //IO_CAN_WriteFIFO
      //Important: Only transmit one message (the MCU message)

    //IO_CAN_WriteFIFO(canFifoHandle_LoPri_Write, canMessages, canMessageCount);  

}

void canOutput_sendDebugMessage1(CanManager* me, MotorController* mcm, TorqueEncoder* tps, Shunt* shunt)
{
    IO_CAN_DATA_FRAME canMessages[me->can1_write_messageLimit];
    ubyte1 errorCount;
    float4 tempPedalPercent;   //Pedal percent float (a decimal between 0 and 1
    ubyte1 tps0Percent;  //Pedal percent int   (a number from 0 to 100)
    ubyte1 tps1Percent;
    ubyte2 canMessageCount = 0;
    // ubyte2 canMessage1Count = 0;
    ubyte2 canMessageID = 0x500;
    ubyte1 byteNum;
    // ubyte1 byteNum1;

    TorqueEncoder_getIndividualSensorPercent(tps, 0, &tempPedalPercent); //borrow the pedal percent variable
    tps0Percent = 0xFF * tempPedalPercent;
    TorqueEncoder_getIndividualSensorPercent(tps, 1, &tempPedalPercent);
    tps1Percent = 0xFF * (tempPedalPercent);
    //tps1Percent = 0xFF * (1 - tempPedalPercent);  //OLD: flipped over pedal percent (this value for display in CAN only)

    TorqueEncoder_getPedalTravel(tps, &errorCount, &tempPedalPercent); //getThrottlePercent(TRUE, &errorCount);
    ubyte1 throttlePercent = 0xFF * tempPedalPercent;

    //500: TPS 0
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1; //500
    canMessages[canMessageCount - 1].data[byteNum++] = throttlePercent;
    canMessages[canMessageCount - 1].data[byteNum++] = tps0Percent;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_TPS0.sensorValue; // tps->tps0_value;
    canMessages[canMessageCount - 1].data[byteNum++] = Sensor_TPS0.sensorValue >> 8; //tps->tps0_value >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMin;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMin >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMax;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps0_calibMax >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    //TPS 1
    canMessageCount++;
    byteNum = 0;
    canMessages[canMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[canMessageCount - 1].id = canMessageID + canMessageCount - 1;
    canMessages[canMessageCount - 1].data[byteNum++] = throttlePercent;
    canMessages[canMessageCount - 1].data[byteNum++] = tps1Percent;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_value;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_value >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMin;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMin >> 8;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMax;
    canMessages[canMessageCount - 1].data[byteNum++] = tps->tps1_calibMax >> 8;
    canMessages[canMessageCount - 1].length = byteNum;

    // Shunt messages
    ubyte2 shuntMessageCount = 0;
    canMessageID = 0x514;
    
    shuntMessageCount++;
    byteNum = 0;
    canMessages[shuntMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[shuntMessageCount - 1].id = canMessageID + shuntMessageCount - 1;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->current_mA >> 24) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->current_mA >> 16) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->current_mA >> 8) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->current_mA >> 0) & 0xFF;
    canMessages[shuntMessageCount - 1].length = byteNum;

    shuntMessageCount++;
    byteNum = 0;
    canMessages[shuntMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[shuntMessageCount - 1].id = canMessageID + shuntMessageCount - 1;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->temperature_dC >> 24) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->temperature_dC >> 16) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->temperature_dC >> 8) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->temperature_dC >> 0) & 0xFF;
    canMessages[shuntMessageCount - 1].length = byteNum;

    shuntMessageCount++;
    byteNum = 0;
    canMessages[shuntMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[shuntMessageCount - 1].id = canMessageID + shuntMessageCount - 1;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->vBus_mV >> 24) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->vBus_mV >> 16) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->vBus_mV >> 8) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->vBus_mV >> 0) & 0xFF;
    canMessages[shuntMessageCount - 1].length = byteNum;

    shuntMessageCount++;
    byteNum = 0;
    canMessages[shuntMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[shuntMessageCount - 1].id = canMessageID + shuntMessageCount - 1;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 56) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 48) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 40) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 32) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 24) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 16) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 8) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->coulombCount >> 0) & 0xFF;
    canMessages[shuntMessageCount - 1].length = byteNum;

    shuntMessageCount++;
    byteNum = 0;
    canMessages[shuntMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[shuntMessageCount - 1].id = canMessageID + shuntMessageCount - 1;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->power_dW >> 24) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->power_dW >> 16) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->power_dW >> 8) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->power_dW >> 0) & 0xFF;
    canMessages[shuntMessageCount - 1].length = byteNum;

    shuntMessageCount++;
    byteNum = 0;
    canMessages[shuntMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[shuntMessageCount - 1].id = canMessageID + shuntMessageCount - 1;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 56) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 48) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 40) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 32) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 24) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 16) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 8) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->energy_Wh >> 0) & 0xFF;
    canMessages[shuntMessageCount - 1].length = byteNum;

    shuntMessageCount++;
    byteNum = 0;
    canMessages[shuntMessageCount - 1].id_format = IO_CAN_STD_FRAME;
    canMessages[shuntMessageCount - 1].id = canMessageID + shuntMessageCount - 1;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->errors >> 8) & 0xFF;
    canMessages[shuntMessageCount - 1].data[byteNum++] = (shunt->errors >> 0) & 0xFF;
    canMessages[shuntMessageCount - 1].length = byteNum;

    canMessageCount += shuntMessageCount;

    CanManager_send(me, CAN1_LOPRI, canMessages, canMessageCount); 

}
