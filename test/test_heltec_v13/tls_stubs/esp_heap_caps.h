#pragma once
#include <cstddef>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
extern size_t testFree;
extern size_t testMinimum;
inline size_t heap_caps_get_free_size(int caps)
{
    return caps == MALLOC_CAP_SPIRAM ? 2000000 : testFree;
}
inline size_t heap_caps_get_largest_free_block(int)
{
    return testFree;
}
inline size_t heap_caps_get_minimum_free_size(int)
{
    return testMinimum;
}
