/** @file jpeg.h
*
* @brief Exposes JPEG specific functionality
*
*/

#ifndef _JPEG_H
#define _JPEG_H

// Max timestamp length
#define TIMESTAMP_MAX (32)
#define DIR_NAME "capture_jpeg"

// Image Resolution information
#define HRES (640)
#define VRES (480)
#define BYTES_PER_PIXEL (3)
#define IMAGE_NUM_BYTES (HRES * VRES * BYTES_PER_PIXEL)

/*!
* @brief Start the jpeg_service thread
* @return SUCCESS/FAILURE
*/
uint32_t jpeg_init();
#endif /* _JPEG_H */
