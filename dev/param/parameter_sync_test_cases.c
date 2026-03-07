/**
 * One test: 30 structs (ParamRow) for HIL/SIL parameter sync.
 * Struct names: AVL, BMS, LC, PL, EFF, REG.
 * mem_address: random 32-bit-style addresses.
 */

#include "parameter_sync_test_cases.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define P32(x) ((void *)(uintptr_t)(x))

ParamRow test_data[TEST_NUM_STRUCTS] = {
    { "REG", 0, 0, "rega", P32(0x8a2f1000u), "float" },
    { "AVL", 0, 0, "avas", P32(0x3b4c2004u), "float" },
    { "PL", 0, 0, "plf", P32(0x1d6e3008u), "int" },
    { "BMS", 0, 0, "bmsf", P32(0x9f01800cu), "float" },
    { "LC", 0, 0, "lcg", P32(0x5c73a010u), "int" },
    { "EFF", 0, 0, "effr", P32(0x2e94b014u), "float" },
    { "AVL", 0, 0, "afd", P32(0x7a05c018u), "float" },
    { "AVL", 0, 0, "avd", P32(0x4b16d01cu), "int" },
    { "BMS", 0, 0, "bmsw", P32(0xcd27e020u), "float" },
    { "BMS", 0, 0, "bmso", P32(0x8e38f024u), "int" },
    { "LC", 0, 0, "lce", P32(0x50490128u), "float" },
    { "EFF", 0, 0, "effh", P32(0x215a102cu), "float" },
    { "EFF", 0, 0, "effl", P32(0xf36b2030u), "float" },
    { "PL", 0, 0, "ple", P32(0xb47c3034u), "int" },
    { "PL", 0, 0, "ple", P32(0x658d4038u), "float" },
    { "REG", 0, 0, "regc", P32(0x379e503cu), "float" },
    { "REG", 0, 0, "regb", P32(0xe8af6040u), "int" },
    { "AVL", 0, 0, "avn", P32(0x9ab07044u), "int" },
    { "AVL", 0, 0, "avq", P32(0x5cc18048u), "float" },
    { "BMS", 0, 0, "bmsp", P32(0x1dd2904cu), "float" },
    { "LC", 0, 0, "lcl", P32(0xdfe3a050u), "int" },
    { "EFF", 0, 0, "effl", P32(0xa0f4b054u), "float" },
    { "PL", 0, 0, "plz", P32(0x6205c058u), "int" },
    { "REG", 0, 0, "regz", P32(0x2316d05cu), "float" },
    { "AVL", 0, 0, "avx", P32(0xe427e060u), "float" },
    { "BMS", 0, 0, "bmsm", P32(0xa538f064u), "float" },
    { "LC", 0, 0, "lcr", P32(0x66490068u), "int" },
    { "EFF", 0, 0, "effr", P32(0x285a106cu), "float" },
    { "PL", 0, 0, "plr", P32(0xe96b2070u), "int" },
    { "REG", 0, 0, "regr", P32(0xab7c3074u), "float" },
};

TestView get_test(void) {
    TestView v = {
        .rows = test_data,
        .count = TEST_NUM_STRUCTS,
    };
    return v;
}

// static int compare_by_structname(const void *a, const void *b) {
//     return strcmp(((const ParamRow *)a)->struct_name, ((const ParamRow *)b)->struct_name);
// }

// static int compare_by_paramname(const void *a, const void *b) {
//     return strcmp(((const ParamRow *)a)->param_name, ((const ParamRow *)b)->param_name);
// }

// uint32_t *getmemoryaddress(int identifer, int paramnum, ParamRow* array, int *sum){
//     //  call and sort the entire 
//     uint32_t *ptr;
//     if(identifer==0){
//         ptr = array[0+paramnum].mem_address;
//     }
//     else{
//         ptr = array[sum[identifer - 1] + paramnum].mem_address;
//     }
//     return ptr;
// }

// int main() {
//     /* --- Load test data and sort by struct name --- */
//     TestView test = get_test();
//     ParamRow rows[TEST_NUM_STRUCTS];
//     memcpy(rows, test.rows, test.count * sizeof(ParamRow));// delete this dont need local copy
//     qsort(rows, test.count, sizeof(ParamRow), compare_by_structname); // sorting by struct name

//     size_t start[TEST_NUM_STRUCTS];
//     size_t counts[TEST_NUM_STRUCTS], num_groups = 0;

//     printf("Sorted by struct name:\n");
//     for (size_t i = 0; i < test.count; i++)
//         printf("  %s  id=%d  param#=%d  %s  %s\n",
//                rows[i].struct_name, rows[i].identifier, rows[i].param_number,
//                rows[i].param_name, rows[i].datatype);

//     /* step 1*/
//     /* --- Assign identifier by group order (AVL=0, BMS=1, ...) --- */
//     int count = 1, count_index = 0;
//     int identifer_number=0;
//     rows[0].identifier=identifer_number;
//     for (int i=1; i<TEST_NUM_STRUCTS;i++){
//         if(rows[i].struct_name != rows[i-1].struct_name){
//             // add the count number to the count array 
//             counts[count_index] = count;
//             count = 1;
//             count_index++;

//             identifer_number++;
//             rows[i].identifier=identifer_number;
//         }
//         else{
//             rows[i].identifier=identifer_number;
//             count++;
//         }
//     }
//     counts[count_index] = count;

//     int counts_length = count_index + 1;
//     /* step 2 */
//     int sum[counts_length];
//     int num = 0;
//     for(int i = 0; i < counts_length+1; i++){
//         num+=counts[i];
//         sum[i] = num;

//     }

//     for(int i = 0; i < counts_length; i++){
//         printf("%d ", counts[i]);
//     }
//     printf("\n");
//     for(int i = 0; i < counts_length; i++){
//         printf("%d ", sum[i]);
//     }
//     printf("\n");
//     printf("Count index: %d\n", counts_length);
//     /* --- Sort within each name by parameter name --- */
//     qsort(rows + 0,counts[0], sizeof(ParamRow), compare_by_paramname); // sorts first one 
//     for (size_t g = 1; g < counts_length; g++){
//         qsort(rows + sum[g - 1],counts[g], sizeof(ParamRow), compare_by_paramname);
//     }

//     int update=0;
//     rows[0].param_number=update;
//     for (int i=1; i<TEST_NUM_STRUCTS;i++){
//         if(rows[i].struct_name != rows[i-1].struct_name){
//             // add the count number to the count array 
//             update = 0;
//         }
//         else{
//             update++;
            
//         }
//         rows[i].param_number=update; 
//     }

//     printf("\nSorted by param number within each name (mem_address = 32-bit):\n");
//     printf("  %-6s  %2s  %6s  %-8s  %-12s  %s\n", "name", "id", "param#", "param", "mem_address", "datatype");
//     printf("  ------  --  ------  --------  ------------  -----\n");
//     for (size_t i = 0; i < test.count; i++)
//         printf("  %-6s  %2d  %6d  %-8s  %08lx       %s\n",
//                rows[i].struct_name, rows[i].identifier, rows[i].param_number,
//                rows[i].param_name, (unsigned long)(uintptr_t)rows[i].mem_address & 0xFFFFFFFFu, rows[i].datatype);
    
//     printf("%x\n", getmemoryaddress(3, 4, rows, sum));
//     return 0;
// }
