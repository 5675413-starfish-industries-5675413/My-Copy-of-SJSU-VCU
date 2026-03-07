#include "parameter_sync_test_cases.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_by_structname(const void *a, const void *b) {
    return strcmp(((const ParamRow *)a)->struct_name, ((const ParamRow *)b)->struct_name);
}

static int compare_by_paramname(const void *a, const void *b) {
    return strcmp(((const ParamRow *)a)->param_name, ((const ParamRow *)b)->param_name);
}

uint32_t *getmemoryaddress(int identifer, int paramnum, ParamRow* array, int *sum){
    //  call and sort the entire 
    uint32_t *ptr;
    if(identifer==0){
        ptr = array[0+paramnum].mem_address;
    }
    else{
        printf("%d %d\n", sum[identifer - 1], paramnum);
        ptr = array[sum[identifer - 1] + paramnum-1].mem_address;
    }
    return ptr;
}

int main(void) {
    /* --- Load test data and sort by struct name --- */
    TestView test = get_test();
    ParamRow rows[TEST_NUM_STRUCTS];
    memcpy(rows, test.rows, test.count * sizeof(ParamRow));
    qsort(rows, test.count, sizeof(ParamRow), compare_by_structname); // sorting by struct name

    size_t start[TEST_NUM_STRUCTS];
    size_t counts[TEST_NUM_STRUCTS], num_groups = 0;

    printf("Sorted by struct name:\n");
    for (size_t i = 0; i < test.count; i++)
        printf("  %s  id=%d  param#=%d  %s  %s\n",
               rows[i].struct_name, rows[i].identifier, rows[i].param_number,
               rows[i].param_name, rows[i].datatype);

    /* step 1*/
    /* --- Assign identifier by group order (AVL=0, BMS=1, ...) --- */
    int count = 1, count_index = 0;
    int identifer_number=0;
    rows[0].identifier=identifer_number;
    for (int i=1; i<TEST_NUM_STRUCTS;i++){
        if(rows[i].struct_name != rows[i-1].struct_name){
            // add the count number to the count array 
            counts[count_index] = count;
            count = 1;
            count_index++;

            identifer_number++;
            rows[i].identifier=identifer_number;
        }
        else{
            rows[i].identifier=identifer_number;
            count++;
        }
    }
    counts[count_index] = count;

    int counts_length = count_index + 1;
    /* step 2 */
    int sum[counts_length];
    int num = 0;
    for(int i = 0; i < counts_length+1; i++){
        num+=counts[i];
        sum[i] = num;

    }

    // for(int i = 0; i < counts_length; i++){
    //     printf("%d ", counts[i]);
    // }
    // printf("\n");
    // for(int i = 0; i < counts_length; i++){
    //     printf("%d ", sum[i]);
    // }
    // printf("\n");
    // printf("Count index: %d\n", counts_length);
    /* --- Sort within each name by parameter name --- */
    qsort(rows + 0,counts[0], sizeof(ParamRow), compare_by_paramname); // sorts first one 
    for (size_t g = 1; g < counts_length; g++){
        qsort(rows + sum[g - 1],counts[g], sizeof(ParamRow), compare_by_paramname);
    }

    int update=0;
    rows[0].param_number=update;
    for (int i=1; i<TEST_NUM_STRUCTS;i++){
        if(rows[i].struct_name != rows[i-1].struct_name){
            // add the count number to the count array 
            update = 0;
        }
        else{
            update++;
            
        }
        rows[i].param_number=update; 
    }

    printf("\nSorted by param number within each name (mem_address = 32-bit):\n");
    printf("  %-6s  %2s  %6s  %-8s  %-12s  %s\n", "name", "id", "param#", "param", "mem_address", "datatype");
    printf("  ------  --  ------  --------  ------------  -----\n");
    for (size_t i = 0; i < test.count; i++)
        printf("  %-6s  %2d  %6d  %-8s  %08lx       %s\n",
               rows[i].struct_name, rows[i].identifier, rows[i].param_number,
               rows[i].param_name, (unsigned long)(uintptr_t)rows[i].mem_address & 0xFFFFFFFFu, rows[i].datatype);
    
    printf("%x\n", getmemoryaddress(3, 4, rows, sum));
    return 0;
}


/*

1. clean code
2. abstract for parameter_sync_test.c
3. starting from 0 vs. starting from 1
- add all necessary params to TESTview 

** At the end, it should just return a memory address


HIL.init(){
.....
    alphetize table
.....
}

memoryaddress(*HIL, identifier, paramenumber)
*/