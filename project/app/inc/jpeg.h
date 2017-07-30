/** @file ppm.h
*
* @brief Exposes PPM specific functionality
*
*/

#ifndef _PPM_H
#define _PPM_H

// Max timestamp length
#define TIMESTAMP_MAX (32)

// Image Resolution information
#define HRES (640)
#define VRES (480)
#define BYTES_PER_PIXEL (3)
#define IMAGE_NUM_BYTES (HRES * VRES * BYTES_PER_PIXEL)

uint32_t jpeg_init();
#endif /* _PPM_H */
