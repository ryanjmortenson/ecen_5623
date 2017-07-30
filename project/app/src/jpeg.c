/** @file jpeg.c
*
* @brief Holds JPEG specific code
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
#include "jpeg.h"
#include "utilities.h"

// File storage info
#define FILE_NAME_MAX (255)
#define UNAME_MAX (255)
#define DIR_NAME "capture_jpeg"
#define IMAGE_EXT ".jpeg"

// Image buffer for current frame
static char image_buf[IMAGE_NUM_BYTES];

// Hold the uname str
static char uname_str[UNAME_MAX];
static uint16_t uname_len = 0;
static uint16_t comment_len = 0;

// Message queue
static image_q_inf_t image_q_inf;

// Resolution info
static resolution_t resolution;

// Add data macro
#define ADD_DATA(dest, src, count, tally) memcpy(&dest[tally], src, count); tally += count

static inline
uint32_t write_jpeg(char * file_name, cap_info_t cap, CvMat * image)
{
  FUNC_ENTRY;
  int32_t res = 0;
  int32_t fd = 0;
  uint32_t cur_loc = 0;
  char timestamp[TIMESTAMP_MAX];
  char image_start[] = {0xff, 0xd8};
  char com_start[] = {0xff, 0xfe};

  // Open file to store contents
  EQ_RET_E(fd, open(file_name, O_CREAT | O_RDWR, FILE_PERM), -1, FAILURE);

  // Add the timestamp to the image comment
  EQ_RET_E(res,
           get_timestamp(&cap.time, timestamp, TIMESTAMP_MAX),
           FAILURE,
           FAILURE);

  // Add all image data to a buffer including header
  ADD_DATA(image_buf, image_start, 2, cur_loc);
  ADD_DATA(image_buf, com_start, 2, cur_loc);
  ADD_DATA(image_buf, &comment_len, 2, cur_loc);
  ADD_DATA(image_buf, timestamp, TIMESTAMP_MAX, cur_loc);
  ADD_DATA(image_buf, uname_str, uname_len, cur_loc);
  ADD_DATA(image_buf, (image->data.ptr + 2), image->cols, cur_loc);

  // Write the file out
  EQ_RET_E(res, write(fd, image_buf, cur_loc), -1, FAILURE);

  // Close file properly
  EQ_RET_E(res, close(fd), -1, FAILURE);

  return SUCCESS;
}

void * handle_jpeg_t(void * param)
{
  FUNC_ENTRY;
  struct timespec diff;
  cap_info_t cap;
  CvMat * image;
  char file_name[FILE_NAME_MAX];
  static const int32_t comp[2] = {CV_IMWRITE_JPEG_QUALITY, 50};
  uint32_t res = 0;
  uint32_t count = 0;
  uint8_t timer = profiler_init();

  // Get the uname length for the comment
  uname_len = strlen(uname_str);

  // Calculate the total comment length for this image add 2 to include null
  // term
  comment_len = TIMESTAMP_MAX + uname_len + 2;
  comment_len = (comment_len << 8 | comment_len >> 8);

  while(1)
  {
    // Get the time after the loop is done
    GET_TIME;

    // Display the timestamp
    DISPLAY_TIMESTAMP;

    // Create the file name to save data
    snprintf(file_name, FILE_NAME_MAX, "%s/capture_%04d.jpeg", DIR_NAME, count);
    LOG_LOW("Using %s as file name for frame %d", file_name, count);
    EQ_RET_E(res,
             mq_receive(image_q_inf.image_q, (char *)&cap, image_q_inf.attr.mq_msgsize, NULL),
             -1,
             NULL);

    // Start the timer after the message has been capture to write to disk
    START_TIME;

    // Encode the frame into JPEG
    EQ_RET_E(image, cvEncodeImage(IMAGE_EXT, cap.frame, comp), NULL, NULL);

    // Add comment information
    EQ_RET_E(res, write_jpeg(file_name, cap, image), 1, NULL);

    // Increment counter
    count++;
  }
  mq_close(image_q_inf.image_q);
  return NULL;
} // handle_jpeg_t()

uint32_t jpeg_init(uint32_t hres, uint32_t vres)
{
  FUNC_ENTRY;
  struct sched_param sched;
  struct sched_param  jpeg_sched;
  pthread_attr_t sched_attr;
  pthread_t jpeg_thread;
  uint32_t res = 0;
  int32_t rt_max_pri = 0;
  int32_t jpeg_policy = 0;

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
  EQ_RET_E(res, get_uname(uname_str, UNAME_MAX), FAILURE, FAILURE);
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
                pthread_create(&jpeg_thread, &sched_attr, handle_jpeg_t, NULL),
                SUCCESS,
                FAILURE);

  // Get the scheduler parameters to display
  PT_NOT_EQ_RET(res,
                pthread_getschedparam(jpeg_thread, &jpeg_policy, &jpeg_sched),
                SUCCESS,
                FAILURE);
  LOG_HIGH("handle_jpeg_t policy: %d, priority: %d",
           jpeg_policy,
           jpeg_sched.sched_priority);
  return SUCCESS;
} // jpeg_init()
