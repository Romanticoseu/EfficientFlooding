#ifndef __RANGINGPROTOCOLDEBUG_H__
#define __RANGINGPROTOCOLDEBUG_H__

#include "debug.h"

//#define DEBUG_INIT
//#define DEBUG_TASK
//#define DEBUG_ALGO
#define DEBUG_ALGO_DATA
//#define DEBUG_STRUCT
//#define DEBUG_TASK_DATA
//#define DEBUG_STRUCT_DATA

#ifdef DEBUG_INIT
#define DEBUG_PRINT_INIT(fmt, ...) DEBUG_PRINT("INIT: "fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INIT(fmt, ...)
#endif

#ifdef DEBUG_TASK
#define DEBUG_PRINT_TASK(fmt, ...) DEBUG_PRINT("TASK: "fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_TASK(fmt, ...)
#endif

#ifdef DEBUG_ALGO
#define DEBUG_PRINT_ALGO(fmt, ...) DEBUG_PRINT("ALGO: "fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_ALGO(fmt, ...)
#endif

#ifdef DEBUG_ALGO_DATA
#define DEBUG_PRINT_ALGO_DATA(fmt, ...) DEBUG_PRINT("ALGO_DATA: "fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_ALGO_DATA(fmt, ...)
#endif

#ifdef DEBUG_TASK_DATA
#define DEBUG_PRINT_TASK_DATA(fmt, ...) DEBUG_PRINT("TASK_DATA: "fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_TASK_DATA(fmt, ...)
#endif

#ifdef DEBUG_STRUCT
#define DEBUG_PRINT_STRUCT(fmt, ...) DEBUG_PRINT("STRUCT: "fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_STRUCT(fmt, ...)
#endif

#ifdef DEBUG_STRUCT_DATA
#define DEBUG_PRINT_STRUCT_DATA(fmt, ...) DEBUG_PRINT("STRUCT_DATA: "fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT_STRUCT_DATA(fmt, ...)
#endif

#endif // __RANGINGPROTOCOLDEBUG_H__