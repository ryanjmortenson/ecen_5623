/** @file profiler.h
*
* @brief Holds declarations for profiler
*
*/

#ifndef __PROFILER_H__
#define __PROFILER_H__

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/*!
* @brief Initializes the profiler
*/
void profiler_init();

/*!
* @brief Starts the timer
* @return status SUCCESS/FAIL
*/
int8_t start_timer();

/*!
* @brief Stops the timer
* @return status SUCCESS/FAIL
*/
int8_t stop_timer();

/*!
* @brief Resets the timer
*/
void reset_timer();

/*!
* @brief Returns nanosecond time
* @param[in] diff Pointer to a timespec structure to fill out with second
*                 and nanosecond difference.
*/
void get_time(struct timespec * diff);


#endif // __PROFILER_H__
