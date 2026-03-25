"""
Counts the number of parameters in a struct as well as the amount of structs. 
Returns paramCount, structCount
"""

import json

def getParamAndStructCount():
    try:
        with open(STRUCT_MEMBERS, 'r') as jF:
            paramCount = []
            structCount = 0
            data = json.load(jF)

            for item in data:
                if isinstance(item, dict) and 'parameters' in item:
                    paramCount.append(len(item['parameters'])) #Counts number of parameters and then places that count into paramCount.
            structCount = len(paramCount)

    except FileNotFoundError:
        print('Error: struct_members_output.json not found!')
    
    return paramCount, structCount