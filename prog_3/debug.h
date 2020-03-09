#ifndef DEBUG_HELPER_H
#define DEBUG_HELPER_H

#include <stdio.h>

//#define DEBUG // define to enable DEBUG_PRINT

#ifdef DEBUG
    #define DEBUG_PRINT(str, args...) printf(str, ##args)
#else
    #define DEBUG_PRINT(str, args...)
#endif

#endif
