/**
 * One test: 30 structs (ParamRow) for HIL/SIL parameter sync.
 * Struct names: AVL, BMS, LC, PL, EFF, REG.
 * mem_address: random 32-bit-style addresses.
 */

#ifndef PARAM_SYNC
#define PARAM_SYNC

#include "parameter_test.h"
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

TestView get_test(const ParamRow *test_data, size_t test_count) {
    TestView v = {
        .rows = test_data,
        .count = test_count,
    };
    return v;
}

#endif