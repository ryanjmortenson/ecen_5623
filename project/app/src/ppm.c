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
#define FILE_NAME_FMT "%s/capture_%04d.ppm"

// Image info
#define MAX_INTENSITY_STR "255\n"
#define MAX_INTENSITY_STR_LEN (4)
#define P6_HEADER "P6\n640 480\n"
#define P6_HEADER_LEN (11)
#define MAX_INTENSITY (255)
#define MAX_INTENSITY_FLOAT (255.0f)

// Image buffer for current frame used to hold converted PPM file
static char image_buf[IMAGE_NUM_BYTES];

// Abort flag
extern uint8_t abort_test;

// PPM capture info
typedef struct {
  // Hold the uname str
  char uname_str[UNAME_MAX];

  // Length of uname string
  uint8_t uname_len;

  // Resolution info
  resolution_t resolution;

  // Timestamp to place in image
  char timestamp[TIMESTAMP_MAX];

  // Captupre info object
  cap_info_t cap;

  // Filename
  char file_name[FILE_NAME_MAX];
} ppm_cap_t;

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
uint32_t write_ppm(uint32_t fd, ppm_cap_t * ppm)
{
  FUNC_ENTRY;
  CHECK_NULL(ppm);

  int32_t res = 0;

  LOG_LOW("Writing ppm image to fd: %d", fd);
  // Write the header
  EQ_RET_E(res,
           write(fd, (void *) P6_HEADER, P6_HEADER_LEN),
           -1,
           FAILURE);

  // Write the timestamp comment
  EQ_RET_E(res,
           write(fd, (void *) ppm->timestamp, strlen(ppm->timestamp)),
           -1,
           FAILURE);

  // Write the uname string comment
  EQ_RET_E(res,
           write(fd, (void *) ppm->uname_str, ppm->uname_len),
           -1,
           FAILURE);

  // Write the max val
  EQ_RET_E(res,
           write(fd, (void *) MAX_INTENSITY_STR, MAX_INTENSITY_STR_LEN),
           -1,
           FAILURE);

  // Write the color data
  EQ_RET_E(res, write(fd, (void *)image_buf, IMAGE_NUM_BYTES), -1, FAILURE);

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
uint32_t create_image_buf(ppm_cap_t * ppm, colors_t * data)
{
  FUNC_ENTRY;
  CHECK_NULL(data);
  CHECK_NULL(ppm);

  uint32_t count = 0;

  // Write the image buffer with proper info
  for (uint32_t vres = 0; vres < ppm->resolution.vres; vres++)
  {
    for (uint32_t hres = 0; hres < ppm->resolution.hres; hres++)
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
  return SUCCESS;
} // create_image_buf()

void * ppm_service(void * param)
{
  FUNC_ENTRY;
  struct timespec diff;
  ppm_cap_t cap;
  int32_t res = 0;
  uint32_t fd = 0;
  uint32_t count = 0;
  uint8_t timer = profiler_init();
  char unlink_name[FILE_NAME_MAX];
  image_q_inf_t image_q_inf;

  // Set the resolution
  cap.resolution.hres = HRES;
  cap.resolution.vres = VRES;

  // Try to create the queue
  EQ_RET_EA(image_q_inf.image_q,
            mq_open(QUEUE_NAME, O_RDONLY | O_CREAT, S_IRWXU, NULL),
            -1,
            NULL,
            abort_test);

  // Get the message queue attributes
  NOT_EQ_RET_EA(res,
                mq_getattr(image_q_inf.image_q, &image_q_inf.attr),
                SUCCESS,
                NULL,
                abort_test)

  // Get the uname string and display
  EQ_RET_EA(res, get_uname(cap.uname_str, UNAME_MAX), FAILURE, NULL, abort_test);
  LOG_LOW("Using uname string: %s", cap.uname_str);
  cap.uname_len = strlen(cap.uname_str);

  while(!abort_test)
  {
    // Get the time after the loop is done
    GET_TIME;

    // Display the timestamp
    DISPLAY_TIMESTAMP;

    // Create the file name to save data
    snprintf(cap.file_name, FILE_NAME_MAX, FILE_NAME_FMT, DIR_NAME, count);
    LOG_LOW("Using %s file name", cap.file_name);
    EQ_RET_EA(res,
              mq_receive(image_q_inf.image_q, (char *)&cap.cap, image_q_inf.attr.mq_msgsize, NULL),
              -1,
              NULL,
              abort_test);

    // Start the timer after the message has been capture to write to disk
    START_TIME;

    // Translate the data from the capture buffer into properly formatted ppm
    // data
    NOT_EQ_RET_EA(res,
                  create_image_buf(&cap, (colors_t *)cap.cap.frame->imageData),
                  SUCCESS,
                  NULL,
                  abort_test);

    // Open file to store contents
    EQ_RET_EA(fd,
              open(cap.file_name, O_CREAT | O_WRONLY, FILE_PERM),
              -1,
              NULL,
              abort_test);

    // Add the timestamp to the image comment
    EQ_RET_EA(res,
             get_timestamp(&cap.cap.time, cap.timestamp, TIMESTAMP_MAX),
             FAILURE,
             NULL,
             abort_test);

    // Write data to file
    NOT_EQ_RET_EA(res, write_ppm(fd, &cap), SUCCESS, NULL, abort_test);

    // Close file properly
    EQ_RET_EA(res, close(fd), -1, NULL, abort_test);

    // Unlink old file if the number for frames is greater than the max frame setting
    res = count - MAX_FRAMES;
    if (res > -1)
    {
      snprintf(unlink_name, FILE_NAME_MAX, FILE_NAME_FMT, DIR_NAME, res);
      LOG_LOW("Unlinking %s", unlink_name);
      EQ_RET_EA(res, unlink(unlink_name), -1, NULL, abort_test);
    }

    // Increment counter
    count++;
  }
  LOG_HIGH("ppm_service thread exiting");
  mq_close(image_q_inf.image_q);
  return NULL;
} // ppm_service()

uint32_t ppm_init()
{
  FUNC_ENTRY;
  struct sched_param sched;
  struct sched_param  ppm_sched;
  pthread_attr_t sched_attr;
  pthread_t ppm_thread;
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  int32_t ppm_policy = 0;

  // Try to create directory for storing images
  EQ_RET_E(res, create_dir(DIR_NAME), FAILURE, FAILURE);

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
                pthread_create(&ppm_thread, &sched_attr, ppm_service, NULL),
                SUCCESS,
                FAILURE);

  // Get the scheduler parameters to display
  PT_NOT_EQ_RET(res,
                pthread_getschedparam(ppm_thread, &ppm_policy, &ppm_sched),
                SUCCESS,
                FAILURE);
  LOG_HIGH("ppm_service policy: %d, priority: %d",
           ppm_policy,
           ppm_sched.sched_priority);
  return SUCCESS;
} // ppm_init()
