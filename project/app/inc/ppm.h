/** @file ppm.h
*
* @brief Exposes PPM specific functionality
*
*/

#ifndef _PPM_H
#define _PPM_H

// Max timestamp length
#define TIMESTAMP_MAX (40)

// Image Resolution information
#define HRES (640)
#define VRES (480)
#define BYTES_PER_PIXEL (3)
#define IMAGE_NUM_BYTES (HRES * VRES * BYTES_PER_PIXEL)

// PPM structure
typedef struct ppm {
  char timestamp[TIMESTAMP_MAX];
  char * uname_str;
  char * header;
  char * max_val;
  char * color_data;
} ppm_t;

// Structure to overlay blue, green, and red
typedef struct colors {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
} __attribute__((packed)) colors_t;

uint32_t ppm_init();
#endif /* _PPM_H */
