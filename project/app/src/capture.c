/** @file capture.c
*
* @brief Holds the capture source
*
*/

#define JPEG_COMPRESSION

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <time.h>

#include "capture.h"
#include "jpeg.h"
#include "log.h"
#include "profiler.h"
#include "ppm.h"
#include "project_defs.h"
#include "utilities.h"

#define WARM_UP

// Capture ping pong buffer size
#define CAP_BUF_SIZE (4)

// Socket info
#define PORT (12345)
#define ADDRESS "127.0.0.1"

// Open CV info
#define WINDOWNAME "capture"
#define DEVICE_NUMBER (0)

// Timing info
#define MICROSECONDS_PER_SECOND (1000000)
#define MICROSECONDS_PER_MILLISECOND (1000)
#define NUM_FRAMES (100)
#define PERIOD (100)
#define WARM_UP_FRAMES (40)

// File info
#define DIR_NAME_MAX (255)
#define FILE_NAME_MAX DIR_NAME_MAX
#define UNAME_MAX (256)
#define TIMING_BUFFER (100)


#define START_CAPTURE(cap, frame) EQ_RET_E(frame,                      \
                                           cvQueryFrame(cap->capture), \
                                           NULL,                       \
                                           NULL)

#define DISPLAY_FRAME(cap, frame) cvShowImage(WINDOWNAME, frame); \
                                  cvWaitKey(1)

// flag for killing thread
static uint32_t abort_test = 0;
static cap_info_t cap_info[CAP_BUF_SIZE];

// Capture structure to pass into pthread
typedef struct cap {
  CvCapture * capture;
  sem_t * start;
  sem_t * stop;
  mqd_t image_queue;
} cap_t;

/*!
* @brief Spins off a thread to do capture
* @param param void pointer to params
* @return NULL
*/
void * cap_func(void * param)
{
  FUNC_ENTRY;

  struct timespec time;
  cap_t * cap = (cap_t *)param;
  cap_info_t * cur_cap_info;
  uint32_t count = 0;
  uint32_t res = 0;

  // Check for null pointer
  if (NULL == param)
  {
    LOG_ERROR("param parameter was NULL");
    return NULL;
  }

  // Loop capturing frames and displaying
  while(!abort_test)
  {
    // Get the time
    clock_gettime(CLOCK_REALTIME, &time);

    // Wait for start signal
    sem_wait(cap->start);

    // Get the current cap info
    cur_cap_info = &cap_info[count % CAP_BUF_SIZE];

    // Capture the frame and add the timestamp to the struct
    START_CAPTURE(cap, cur_cap_info->frame);
    cur_cap_info->time = time;

    // Try to send the cap info via messaqe queue
    NOT_EQ_RET_E(res,
                 mq_send(cap->image_queue, (char *)cur_cap_info, sizeof(*cur_cap_info), 0),
                 SUCCESS,
                 NULL);

    // Display the frame
    DISPLAY_FRAME(cap, cur_cap_info->frame);

    // Post done
    sem_post(cap->stop);
    count++;
  }

  return NULL;
} // cap_func()

int capture()
{
  struct sched_param sched;
  struct sched_param cap_sched;
  struct timespec diff;
  pthread_t cap_thread;
  pthread_attr_t sched_attr;
  sem_t start;
  sem_t stop;
  cap_t cap;
  IplImage * frame;
  int32_t cap_policy = 0;
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  uint8_t timer = profiler_init();

  // Initialize log (does nothing if not using syslog)
  log_init();

  // Print function entry
  FUNC_ENTRY;

  // Unlink the queue name in case it is still hanging around, it is okay
  // if this fails it is just precaution for stale queues
  mq_unlink(QUEUE_NAME);

  // Create a window
  cvNamedWindow(WINDOWNAME, CV_WINDOW_AUTOSIZE);

  // Try to create the queue
  EQ_EXIT_E(cap.image_queue,
            mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, S_IRWXU, NULL),
            -1);

  // Semaphore for timing
  PT_NOT_EQ_EXIT(res, sem_init(&start, 0, 0), SUCCESS);
  PT_NOT_EQ_EXIT(res, sem_init(&stop, 0, 0), SUCCESS);
  cap.start = &start;
  cap.stop = &stop;

  // Create a capture object and set values
  EQ_EXIT_E(cap.capture, (CvCapture *)cvCreateCameraCapture(DEVICE_NUMBER), NULL);

  // Initialize the schedule attributes
  PT_NOT_EQ_EXIT(res, pthread_attr_init(&sched_attr), SUCCESS);

  // Set the attributes structure to PTHREAD_EXPLICIT_SCHED and SCHED_FIFO
  PT_NOT_EQ_EXIT(res,
                 pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED),
                 SUCCESS);
  PT_NOT_EQ_EXIT(res,
                 pthread_attr_setschedpolicy(&sched_attr, SCHED_FIFO),
                 SUCCESS);

  // Get the max priority
  EQ_EXIT_E(rt_max_pri, sched_get_priority_max(SCHED_FIFO), -1);

  // Set scheduler to SCHED_FIFO for the main thread
  sched.sched_priority = rt_max_pri;
  NOT_EQ_EXIT_E(res,
                sched_setscheduler(getpid(), SCHED_FIFO, &sched),
                SUCCESS);

  // Set the priority to max - 1 for the test thread
  sched.sched_priority = rt_max_pri - 1;
  PT_NOT_EQ_EXIT(res,
                 pthread_attr_setschedparam(&sched_attr, &sched),
                 SUCCESS);

  // Set resolution
  LOG_HIGH("Setting resolution to %dx%d", HRES, VRES);
  cvSetCaptureProperty(cap.capture, CV_CAP_PROP_FRAME_WIDTH, HRES);
  cvSetCaptureProperty(cap.capture, CV_CAP_PROP_FRAME_HEIGHT, VRES);

  // Create pthread
  PT_NOT_EQ_EXIT(res,
                 pthread_create(&cap_thread, &sched_attr, cap_func, (void *)&cap),
                 SUCCESS);

  // Get the scheduler parameters to display
  PT_NOT_EQ_EXIT(res,
                 pthread_getschedparam(cap_thread, &cap_policy, &cap_sched),
                 SUCCESS);
  LOG_HIGH("cap_thread policy: %d, priority: %d",
           cap_policy,
           cap_sched.sched_priority);

#ifdef JPEG_COMPRESSION
  // Try setting up the JPEG thread
  NOT_EQ_EXIT_E(res, jpeg_init(), SUCCESS);
#else
  // Try setting up the ppm thread
  NOT_EQ_EXIT_E(res, ppm_init(), SUCCESS);
#endif

#ifdef WARM_UP
  // Loop captures frames to allow camera to warm up
  LOG_MED("Running %d frames for warm up", WARM_UP_FRAMES);
  for (uint8_t frames = 0; frames < WARM_UP_FRAMES; frames++)
  {
    EQ_RET_E(frame, cvQueryFrame(cap.capture), NULL, FAILURE);
  }
#endif // WARM_UP

  // Loop captures frames to get stats
  for (uint32_t frames = 0; frames < NUM_FRAMES; frames++)
  {
    // Post semaphore for capture
    sem_post(cap.start);

    // Start the timer
    START_TIME;

    // Sleep for the period of a frame
    usleep(MICROSECONDS_PER_MILLISECOND * PERIOD - TIMING_BUFFER);

    // Wait for frame to be captured
    sem_wait(cap.stop);

    // Get the time
    GET_TIME;

    // Display the time
    DISPLAY_TIMESTAMP;
  }

  // Set the abort flag then allow the thread to exit
  sem_post(cap.start);
  abort_test = 1;
  sem_post(cap.stop);

  // Wait for test thread to join
  PT_NOT_EQ_EXIT(res,
                 pthread_join(cap_thread, NULL),
                 SUCCESS);
  LOG_HIGH("capture thread joined");

  // Destroy capture and window
  cvReleaseCapture(&cap.capture);
  cvDestroyWindow(WINDOWNAME);

  // Close the message queue fd
  mq_close(cap.image_queue);

  // Unlink it so it is destroyed
  mq_unlink(QUEUE_NAME);

  // Destroy log
  log_destroy();
  return 0;
} // capture()
