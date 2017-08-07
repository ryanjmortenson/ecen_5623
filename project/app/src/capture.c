/** @file capture.c
*
* @brief Holds the capture source
*
*/

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
#include "log.h"
#include "profiler.h"
#include "project_defs.h"
#include "utilities.h"

// Use either JPEG or PPM to save files
#ifdef JPEG_COMPRESSION
#include "jpeg.h"
#include "server.h"
#else
#include "ppm.h"
#endif

// Warm up camera by capturing frames
#define WARM_UP
#define WARM_UP_FRAMES (10)

// Capture ping pong buffer size
#define CAP_BUF_SIZE (4)

// Open CV info
#define WINDOWNAME "capture"
#define DEVICE_NUMBER (0)

// Timing info
#define MICROSECONDS_PER_SECOND (1000000)
#define MICROSECONDS_PER_MILLISECOND (1000)

// Frame capture info
#define NUM_FRAMES (20)
#define PERIOD (100)
#define TIMING_BUFFER (125)

// Flag for stopping application.  All services extern this variable.
uint32_t abort_test = 0;

// Ping Pong buffer for cap info
static cap_info_t cap_info[CAP_BUF_SIZE];

// Capture structure
static struct cap {
  CvCapture * capture;
  sem_t start;
  sem_t stop;
  mqd_t image_queue;
} cap;

/*!
* @brief Captures frames and passes them through a message queue for the
*        jpeg/ppm service to convert and save to disk
* @param param void pointer to params
* @return NULL
*/
void * cap_service(void * param)
{
  FUNC_ENTRY;

  struct timespec time;
  struct timespec diff;
  cap_info_t * cur_cap_info;
  uint32_t count = 0;
  uint32_t res = 0;
  uint8_t timer = profiler_init();

  // Loop capturing frames and displaying
  while(!abort_test)
  {
    GET_TIME;
    DISPLAY_TIMESTAMP;
    // Wait for start signal
    sem_wait(&cap.start);
    START_TIME;

    // Get the time
    clock_gettime(CLOCK_REALTIME, &time);

    // Get the current cap info
    cur_cap_info = &cap_info[count % CAP_BUF_SIZE];

    EQ_RET_E(cur_cap_info->frame, cvQueryFrame(cap.capture), NULL, NULL);

    cur_cap_info->time = time;

    // Try to send the cap info via messaqe queue
    NOT_EQ_RET_EA(res,
                 mq_send(cap.image_queue, (char *)cur_cap_info, sizeof(*cur_cap_info), 0),
                 SUCCESS,
                 NULL,
                 abort_test);

    cvShowImage(WINDOWNAME, cur_cap_info->frame);
    cvWaitKey(1);

    // Post done
    sem_post(&cap.stop);
    count++;
  }
  LOG_HIGH("cap_service thread exiting");
  return NULL;
} // cap_service()

int sched_service()
{
  struct sched_param sched;
  struct sched_param cap_sched;
  struct timespec diff;
  pthread_t cap_thread;
  pthread_attr_t sched_attr;
  int32_t cap_policy = 0;
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  uint8_t timer = profiler_init();

#ifdef WARM_UP
  // Frame used for capture during warm up phase
  IplImage * frame;
#endif // WARM_UP

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
  PT_NOT_EQ_EXIT(res, sem_init(&cap.start, 0, 0), SUCCESS);
  PT_NOT_EQ_EXIT(res, sem_init(&cap.stop, 0, 0), SUCCESS);

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
                 pthread_create(&cap_thread, &sched_attr, cap_service, NULL),
                 SUCCESS);

  // Get the scheduler parameters to display
  PT_NOT_EQ_EXIT(res,
                 pthread_getschedparam(cap_thread, &cap_policy, &cap_sched),
                 SUCCESS);
  LOG_HIGH("cap_thread policy: %d, priority: %d",
           cap_policy,
           cap_sched.sched_priority);

#ifdef JPEG_COMPRESSION
  // Try setting up the JPEG service
  NOT_EQ_EXIT_E(res, jpeg_init(), SUCCESS);
  // Try setting up the server service
  NOT_EQ_EXIT_E(res, server_init(), SUCCESS);
#else
  // Try setting up the ppm service
  NOT_EQ_EXIT_E(res, ppm_init(), SUCCESS);
#endif

#ifdef WARM_UP
  // Loop captures frames to allow camera to warm up
  LOG_MED("Running %d frames for warm up", WARM_UP_FRAMES);
  for (uint8_t frames = 0; frames < WARM_UP_FRAMES; frames++)
  {
    EQ_RET_E(frame, cvQueryFrame(cap.capture), NULL, FAILURE);
    cvShowImage(WINDOWNAME, frame);
    cvWaitKey(1);
    usleep(MICROSECONDS_PER_SECOND);
  }
#endif // WARM_UP

  // Loop captures frames to get stats
  for (uint32_t frames = 0; frames < NUM_FRAMES; frames++)
  {
    // Start the timer
    START_TIME;

    // Post semaphore for capture
    sem_post(&cap.start);

    // Get the time
    GET_TIME;

    // Display the time
    DISPLAY_TIMESTAMP;

    // Sleep for the period of a frame
    usleep(MICROSECONDS_PER_MILLISECOND * PERIOD - TIMING_BUFFER);

    // Start the timer
    START_TIME;

    if (!abort_test)
    {
      // Wait for frame to be captured
      sem_wait(&cap.stop);
    }
    else
    {
      break;
    }
    // Get the time
    GET_TIME;

    // Display the time
    DISPLAY_TIMESTAMP;
  }

  // Set the abort flag then allow the thread to exit
  sem_post(&cap.start);
  abort_test = 1;
  sem_post(&cap.stop);

  // Wait for test thread to join
  PT_NOT_EQ_EXIT(res,
                 pthread_join(cap_thread, NULL),
                 SUCCESS);
  LOG_MED("cap_service thread joined");

  // Destroy capture and window
  cvReleaseCapture(&cap.capture);
  cvDestroyWindow(WINDOWNAME);

  // Close the message queue fd
  mq_close(cap.image_queue);

  // Unlink it so it is destroyed
  mq_unlink(QUEUE_NAME);

  // Destroy log
  log_destroy();

  // Log capture thread exiting
  LOG_HIGH("sched_service exiting");
  return 0;
} // capture()
