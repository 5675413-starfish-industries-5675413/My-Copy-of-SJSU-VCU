/*****************************************************************************
 * parse_values.h - Parse JSON configuration values into structs
 * Parses struct_members_output.json and assigns values to struct instances
 *****************************************************************************/

#ifndef _PARSE_VALUES_H
#define _PARSE_VALUES_H

#include "IO_Driver.h"
#include "powerLimit.h"
#include "motorController.h"
#include "torqueEncoder.h"

/**
 * Parse values from struct_members_output.json and assign them to struct instances
 * @param json_file_path Path to the struct_members_output.json file
 * @param pl PowerLimit struct pointer (can be NULL if not needed)
 * @param mcm MotorController struct pointer (can be NULL if not needed)
 * @param tps TorqueEncoder struct pointer (can be NULL if not needed)
 * @return 0 on success, -1 on error
 */
int parse_struct_values_from_json(const char* json_file_path, 
                                   PowerLimit* pl, 
                                   MotorController* mcm, 
                                   TorqueEncoder* tps);

/**
 * Parse PowerLimit struct values from JSON object
 * @param pl PowerLimit struct pointer
 * @param json_params JSON object containing PowerLimit parameters
 * @return 0 on success, -1 on error
 */
int parse_PowerLimit_from_json(PowerLimit* pl, void* json_params);

/**
 * Parse MotorController struct values from JSON object
 * @param mcm MotorController struct pointer
 * @param json_params JSON object containing MotorController parameters
 * @return 0 on success, -1 on error
 */
int parse_MotorController_from_json(MotorController* mcm, void* json_params);

/**
 * Parse TorqueEncoder struct values from JSON object
 * @param tps TorqueEncoder struct pointer
 * @param json_params JSON object containing TorqueEncoder parameters
 * @return 0 on success, -1 on error
 */
int parse_TorqueEncoder_from_json(TorqueEncoder* tps, void* json_params);

#endif // _PARSE_VALUES_H

