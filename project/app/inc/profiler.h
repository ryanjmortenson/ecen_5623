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
uint8_t profiler_init();

/*!
* @brief Starts the timer
* @return status SUCCESS/FAIL
*/
int8_t start_timer(uint8_t timer);

/*!
* @brief Stops the timer
* @return status SUCCESS/FAIL
*/
int8_t stop_timer(uint8_t timer);

/*!
* @brief Resets the timer
*/
void reset_timer(uint8_t timer);

/*!
* @brief Returns nanosecond time
* @param[in] diff Pointer to a timespec structure to fill out with second
*                 and nanosecond difference.
*/
void get_time(uint8_t timer, struct timespec * diff);


#endif // __PROFILER_H__
