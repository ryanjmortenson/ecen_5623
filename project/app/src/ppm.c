/** @file ppm.c
*
* @brief Holds NetPBM PPM specific code
*
*/

#include <errno.h>
#include <math.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "capture.h"
#include "log.h"
#include "project_defs.h"
#include "profiler.h"
#include "ppm.h"
#include "utilities.h"

// File storage info
#define FILE_NAME_MAX (255)
#define UNAME_MAX (255)
#define DIR_NAME "capture_ppm"

// Image info
#define MAX_INTENSITY_STR "255\n"
#define P6_HEADER "P6\n640 480\n"
#define MAX_INTENSITY (255)
#define MAX_INTENSITY_FLOAT (255.0f)

// Image buffer for current frame
static char image_buf[IMAGE_NUM_BYTES];

// Hold the uname str
static char uname_str[UNAME_MAX];

// Message queue
static image_q_inf_t image_q_inf;

// Resolution info
static resolution_t resolution;

/*!
* @brief Applies gamma transfer function to color intensity
* @param color color intensity
* @return transformed result
*/
static inline
uint8_t gamma_tf(uint8_t color)
{
  FUNC_ENTRY;
  float conversion = (float)color / MAX_INTENSITY_FLOAT;
  if (conversion >= 0 && conversion < 0.0013f)
  {
    return (uint8_t) ((float)(12.92f * conversion) * MAX_INTENSITY);
  }
  else
  {
    return (uint8_t) ((float)(1.055f * pow(conversion, .4545f) - 0.055f) * MAX_INTENSITY);
  }
} // gamma_tf()

/*!
* @brief Writes ppm to file and socket
* @param fd file descriptor to write ppm to
* @param ppm struct holding ppm data to write to fd
* @return SUCCESS/FAILURE
*/
static inline
uint32_t write_ppm(uint32_t fd, ppm_t * ppm)
{
  FUNC_ENTRY;
  CHECK_NULL(ppm);

  uint32_t res = 0;

  LOG_LOW("Writing ppm image to fd: %d", fd);
  // Write the header
  EQ_RET_E(res,
           write(fd, (void *) ppm->header, strlen(ppm->header)),
           -1,
           FAILURE);

  // Write the timestamp comment
  EQ_RET_E(res,
           write(fd, (void *) ppm->timestamp, strlen(ppm->timestamp)),
           -1,
           FAILURE);

  // Write the uname string comment
  EQ_RET_E(res,
           write(fd, (void *) ppm->uname_str, strlen(ppm->uname_str)),
           -1,
           FAILURE);

  // Write the max val
  EQ_RET_E(res,
           write(fd, (void *) ppm->max_val, strlen(ppm->max_val)),
           -1,
           FAILURE);

  // Write the color data
  EQ_RET_E(res, write(fd, (void *) ppm->color_data, IMAGE_NUM_BYTES), -1, FAILURE);

  // Success
  return SUCCESS;
} // write_ppm()

/*!
* @brief Puts image buffer rgb in correct format
* @param ppm ppm structure to set data pointer
* @param data imageData casted as a colors_t data structure
* @return SUCCESS/FAILURE
*/
static inline
uint32_t create_image_buf(ppm_t * ppm, colors_t * data)
{
  FUNC_ENTRY;
  CHECK_NULL(data);
  CHECK_NULL(ppm);

  uint32_t count = 0;

  // Write the image buffer with proper info
  for (uint32_t vres = 0; vres < VRES; vres++)
  {
    for (uint32_t hres = 0; hres < HRES; hres++)
    {
#ifdef GAMMA_FUNCTION
      // Do gamma function conversion which is specified by the NetPbm spec
      image_buf[count]     = gamma_tf(data->green);
      image_buf[count + 1] = gamma_tf(data->blue);
      image_buf[count + 2] = gamma_tf(data->red);
#else // GAMMA_FUNCTION
      // Write the raw intensity
      image_buf[count]     = (data->red);
      image_buf[count + 1] = (data->green);
      image_buf[count + 2] = (data->blue);
#endif // GAMMA_FUNCTION
      count += 3;
      data++;
    }
  }

  // Set the color data pointer to image buf
  ppm->color_data = image_buf;
  return SUCCESS;
} // create_image_buf()

void * handle_ppm_t(void * param)
{
  FUNC_ENTRY;
  struct timespec diff;
  cap_info_t cap;
  char file_name[FILE_NAME_MAX];
  uint32_t res = 0;
  uint32_t fd = 0;
  uint32_t count = 0;
  uint8_t timer = profiler_init();

  // Create ppm p6 struct since the only ppm type used
  ppm_t p6 = {
    .header = P6_HEADER,
    .max_val = MAX_INTENSITY_STR,
    .uname_str = uname_str
  };

  while(1)
  {
    // Get the time after the loop is done
    GET_TIME;

    // Display the timestamp
    DISPLAY_TIMESTAMP;

    // Create the file name to save data
    snprintf(file_name, FILE_NAME_MAX, "%s/capture_%04d.ppm", DIR_NAME, count);
    LOG_LOW("Using %s as file name for frame %d", file_name, count);
    EQ_RET_E(res,
             mq_receive(image_q_inf.image_q, (char *)&cap, image_q_inf.attr.mq_msgsize, NULL),
             -1,
             NULL);

    // Start the timer after the message has been capture to write to disk
    START_TIME;

    // Translate the data from the capture buffer into properly formatted ppm
    // data
    NOT_EQ_RET_E(res,
                create_image_buf(&p6, (colors_t *)cap.frame->imageData),
                SUCCESS,
                NULL);

    // Open file to store contents
    EQ_RET_E(fd, open(file_name, O_CREAT | O_WRONLY, FILE_PERM), -1, NULL);

    // Add the timestamp to the image comment
    EQ_RET_E(res,
             get_timestamp(&cap.time, p6.timestamp, TIMESTAMP_MAX),
             FAILURE,
             NULL);

    // Write data to file
    NOT_EQ_RET_E(res, write_ppm(fd, &p6), SUCCESS, NULL);

    // Close file properly
    EQ_RET_E(res, close(fd), -1, NULL);

    // Increment counter
    count++;
  }
  mq_close(image_q_inf.image_q);
  return NULL;
} // handle_ppm_t()

uint32_t ppm_init(uint32_t hres, uint32_t vres)
{
  FUNC_ENTRY;
  struct sched_param sched;
  struct sched_param  ppm_sched;
  pthread_attr_t sched_attr;
  pthread_t ppm_thread;
  uint32_t res = 0;
  int32_t rt_max_pri = 0;
  int32_t ppm_policy = 0;

  // Set the resolution
  resolution.hres = hres;
  resolution.vres = vres;

  // Try to create directory for storing images
  EQ_RET_E(res, create_dir(DIR_NAME), FAILURE, FAILURE);

  // Try to create the queue
  EQ_RET_E(image_q_inf.image_q,
           mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, S_IRWXU, NULL),
           -1,
           FAILURE);

  // Get the message queue attributes
  NOT_EQ_RET_E(res,
               mq_getattr(image_q_inf.image_q, &image_q_inf.attr),
               SUCCESS,
               FAILURE)

  // Get the uname string and display
  EQ_RET_E(res, get_uname(uname_str, 255), FAILURE, FAILURE);
  LOG_LOW("Using uname string: %s", uname_str);

  // Initialize the schedule attributes
  PT_NOT_EQ_EXIT(res, pthread_attr_init(&sched_attr), SUCCESS);

  // Set the attributes structure to PTHREAD_EXPLICIT_SCHED and SCHED_FIFO
  PT_NOT_EQ_RET(res,
                pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED),
                SUCCESS,
                FAILURE);
  PT_NOT_EQ_RET(res,
                pthread_attr_setschedpolicy(&sched_attr, SCHED_FIFO),
                SUCCESS,
                FAILURE);

  // Get the max priority
  EQ_RET_E(rt_max_pri, sched_get_priority_max(SCHED_FIFO), -1, FAILURE);

  // Set the priority to max - 1 for the test thread
  sched.sched_priority = rt_max_pri - 1;
  PT_NOT_EQ_RET(res,
                pthread_attr_setschedparam(&sched_attr, &sched),
                SUCCESS,
                FAILURE);
  // Create pthread
  PT_NOT_EQ_RET(res,
                pthread_create(&ppm_thread, &sched_attr, handle_ppm_t, NULL),
                SUCCESS,
                FAILURE);

  // Get the scheduler parameters to display
  PT_NOT_EQ_RET(res,
                pthread_getschedparam(ppm_thread, &ppm_policy, &ppm_sched),
                SUCCESS,
                FAILURE);
  LOG_HIGH("handle_ppm_t policy: %d, priority: %d",
           ppm_policy,
           ppm_sched.sched_priority);
  return SUCCESS;
} // ppm_init()
