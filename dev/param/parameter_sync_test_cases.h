/**
 * Test data for HIL/SIL parameter sync algorithm.
 * One test: 30 structs (ParamRow) sent in at once.
 * Struct names: AVL, BMS, LC, PL, EFF, REG.
 * mem_address is a pointer (NULL until computed later).
 */

#ifndef PARAMETER_SYNC_TEST_CASES_H
#define PARAMETER_SYNC_TEST_CASES_H

#include <stddef.h>

#define TEST_NUM_STRUCTS 30

/* Single row in the 2D array (6 fields). */
typedef struct {
    const char *struct_name;
    int identifier;
    int param_number;
    const char *param_name;
    void *mem_address;
    const char *datatype;
} ParamRow;

/* One test: 30 structs. */
typedef struct {
    const ParamRow *rows;
    size_t count;  /* always TEST_NUM_STRUCTS */
} TestView;

/**
 * Returns the single test: 30 structs to send in at once.
 */
TestView get_test(void);

#endif /* PARAMETER_SYNC_TEST_CASES_H */
