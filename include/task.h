
#ifndef __TASKH__
#define __TASKH__

#include "defs.h"

#define C_CLOCK         32


typedef byte_t (*task_t)(byte_t);

/**
 * Task structure.
 */
typedef struct {
   task_t fun;                ///< Task address.
   byte_t par;                ///< Parameter value.
   byte_t tmr;                ///< Time to wait before calling.
} tid_t;

/**
 * Maximum pending tasks.
 */
#define NTASKS             8

/**
 * Task list.
 */
extern volatile tid_t _tasks[NTASKS];

extern volatile _uint32_t ticks;

/*
 * Timing helper macros.
 */
#if defined(__CPIK__)
#define timer_t unsigned long long int
#else
#define timer_t unsigned long int
#endif
#define start_timer(X) (X+ticks.d)
#define timeout(X) (ticks.d>X?1:0)

extern bool_t wdt_tick;
extern void tick();
extern void task_init();
extern void task_main();
extern bool_t task_add(task_t f, byte_t tempo, byte_t par);
extern bool_t i_task_add(task_t f, byte_t tempo, byte_t par);
extern bool_t task_cancel(task_t f);
extern bool_t i_task_cancel(task_t f);
extern void task_cancel_all();
extern void i_task_cancel_all();

#endif
