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
#define FILE_NAME_FMT "%s/capture_%04d.jpeg"
#define NUM_IMAGE_BUFS (4)

// Flag for setting abort status
extern uint8_t abort_test;

// Location to store JPEG data and pass to TCP client service.  Using multiple buffering
// for safety of data without using MUTEX.
static char image_buf[NUM_IMAGE_BUFS][IMAGE_NUM_BYTES];

// Struct of information for the thread
typedef struct {
  // Image buffer for current frame
  char * cur_buf;

  // Hold the uname str
  char uname_str[UNAME_MAX];
  uint16_t uname_len;
  uint16_t comment_len;

  // Filename
  char file_name[FILE_NAME_MAX];

  // Current image pointer
  CvMat * image;

  // Capture info object
  cap_info_t cap;
} jpeg_cap_t;

// Add data macro
#define ADD_DATA(dest, src, count, tally) memcpy(&dest[tally], src, count); tally += count

static inline
uint32_t write_jpeg(jpeg_cap_t * cap)
{
  FUNC_ENTRY;
  int32_t res = 0;
  int32_t fd = 0;
  uint32_t cur_loc = 0;
  char timestamp[TIMESTAMP_MAX];
  char image_start[] = {0xff, 0xd8};
  char com_start[] = {0xff, 0xfe};

  // Open file to store contents
  EQ_RET_E(fd, open(cap->file_name, O_CREAT | O_RDWR, FILE_PERM), -1, FAILURE);

  // Add the timestamp to the image comment
  EQ_RET_E(res,
           get_timestamp(&cap->cap.time, timestamp, TIMESTAMP_MAX),
           FAILURE,
           FAILURE);

  // Add all image data to a buffer including header
  ADD_DATA(cap->cur_buf, image_start, 2, cur_loc);
  ADD_DATA(cap->cur_buf, com_start, 2, cur_loc);
  ADD_DATA(cap->cur_buf, &(cap->comment_len), 2, cur_loc);
  ADD_DATA(cap->cur_buf, timestamp, TIMESTAMP_MAX, cur_loc);
  ADD_DATA(cap->cur_buf, cap->uname_str, cap->uname_len, cur_loc);
  ADD_DATA(cap->cur_buf, (cap->image->data.ptr + 2), cap->image->cols, cur_loc);

  // Write the file out
  EQ_RET_E(res, write(fd, cap->cur_buf, cur_loc), -1, FAILURE);

  // Close file properly
  EQ_RET_E(res, close(fd), -1, FAILURE);

  return SUCCESS;
}

void * handle_jpeg_t(void * param)
{
  FUNC_ENTRY;
  struct timespec diff;
  image_q_inf_t image_q_inf;
  jpeg_cap_t cap;
  const int32_t comp[2] = {CV_IMWRITE_JPEG_QUALITY, 50};
  int32_t res = 0;
  uint32_t count = 0;
  uint8_t timer = profiler_init();
  char unlink_name[FILE_NAME_MAX];

  // Get the uname string and display
  EQ_RET_EA(res, get_uname(cap.uname_str, UNAME_MAX), FAILURE, NULL, abort_test);
  LOG_LOW("Using uname string: %s", cap.uname_str);

  // Get the uname length for the comment
  cap.uname_len = strlen(cap.uname_str);

  // Calculate the total comment length for this image add 2 to include null
  // term
  cap.comment_len = TIMESTAMP_MAX + cap.uname_len + 2;
  cap.comment_len = (cap.comment_len << 8 | cap.comment_len >> 8);

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

  while(!abort_test)
  {
    // Get the current buffer from the multi buffer for processing image
    cap.cur_buf = image_buf[count % NUM_IMAGE_BUFS];

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

    // Start the timer for encoding/writing the image
    START_TIME;

    // Encode the frame into JPEG
    EQ_RET_EA(cap.image,
              cvEncodeImage(IMAGE_EXT, cap.cap.frame, comp),
              NULL,
              NULL,
              abort_test);

    // Add comment information
    EQ_RET_EA(res, write_jpeg(&cap), 1, NULL, abort_test);

    // Unlink old file if the number for frames is greater than the max frame setting
    res = count - MAX_FRAMES;
    if (res > -1)
    {
      LOG_FATAL("%d", res);
      snprintf(unlink_name, FILE_NAME_MAX, FILE_NAME_FMT, DIR_NAME, res);
      LOG_LOW("Unlinking %s", unlink_name);
      EQ_RET_EA(res, unlink(unlink_name), -1, NULL, abort_test);
    }

    // Increment counter
    count++;
  }
  LOG_HIGH("handle_jpeg_t thread exiting");
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
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  int32_t jpeg_policy = 0;

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
