/** @file exercise4problem5.c
*
* @brief Holds the exercise4problem5 source
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

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "log.h"
#include "profiler.h"
#include "project_defs.h"
#include "client.h"

#define WARM_UP
#define CLIENT_CONNECT

#define WINDOWNAME "capture"
#define DEVICE_NUMBER (0)
#define MICROSECONDS_PER_SECOND (1000000)
#define MICROSECONDS_PER_MILLISECOND (1000)
#define NUM_FRAMES (1)
#define PERIOD (100)
#define NUM_RESOLUTIONS (1)
#define NUM_TRANSFORMS (1)
#define WARM_UP_FRAMES (40)
#define DIR_NAME_MAX (255)
#define MAX_INTENSITY (255)
#define FILE_NAME_MAX DIR_NAME_MAX
#define TIMESTAMP_MAX (40)
#define UNAME_MAX (256)
#define FILE_PERM (S_IRWXU | S_IRWXO | S_IRWXG)

#define TIMING_BUFFER (100)
#define DISPLAY_TIMESTAMP LOG_LOW("sec: %d, millisec: %f", diff.tv_sec, (float)(diff.tv_nsec)/1000000)
#define START_TIME start_timer(timer)
#define START_CAPTURE EQ_RET_E(capture->frame, cvQueryFrame(capture->capture), NULL, NULL);
#define DISPLAY_FRAME cvShowImage(WINDOWNAME, capture->frame); \
                      cvWaitKey(1);
#define GET_TIME stop_timer(timer); \
                 get_time(timer, &diff);

// Get a huge buffer in ram
char image_buf[921600];

// Structure for resolutions
typedef struct res {
  uint32_t hres;
  uint32_t vres;
} res_t;

// Array of resolutions
res_t resolutions[NUM_RESOLUTIONS] = {
  {.hres = 640,  .vres = 480}
};

// Capture structure to pass into pthread
typedef struct cap {
  CvCapture * capture;
  IplImage * frame;
  sem_t * start;
  sem_t * stop;
  res_t * res;
  char * dir_name;
  int32_t sockfd;
} cap_t;

// Flag for killing thread
uint32_t abort_test = 0;

// Structure to overlay blue green
typedef struct colors {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
} __attribute__((packed)) colors_t;

static inline uint8_t gamma_tf(uint8_t color)
{
  float conversion = (float)color / 255.0f;
  if (conversion >= 0 && conversion < 0.0013f)
  {
    return (uint8_t) ((float)(12.92f * conversion) * MAX_INTENSITY);
  }
  else
  {
    return (uint8_t) ((float)(1.055f * pow(conversion, .4545f) - 0.055f) * MAX_INTENSITY);
  }
}

static inline
uint32_t write_ppm(uint32_t fd, cap_t * cap, struct timespec * time, char * uname_str)
{
  FUNC_ENTRY;
  CHECK_NULL(cap);
  CHECK_NULL(time);
  CHECK_NULL(uname_str);

  colors_t * data = (colors_t *)cap->frame->imageData;
  const char * header = "P6\n640 480\n";
  const char * pixel_max = "255\n";
  char timestamp[TIMESTAMP_MAX] = {0};
  uint32_t count = 0;
  uint32_t res = 0;

  // Set first element in image_buf to zero
  image_buf[0] = 0;

  // Write the header
  EQ_RET_E(res, write(fd, (void *) header, strlen(header)), -1, FAILURE);
  EQ_RET_E(res, write(cap->sockfd, (void *) header, strlen(header)), -1, FAILURE);

  // Create and write the timestamp
  res = snprintf(timestamp,
                 TIMESTAMP_MAX,
                 "# Timestamp: %f\n",
                 (double) time->tv_sec + (double) time->tv_nsec);
  EQ_RET_E(res, write(fd, (void *) timestamp, res), -1, FAILURE);
  EQ_RET_E(res, write(cap->sockfd, (void *) timestamp, res), -1, FAILURE);

  // Write the uname string
  EQ_RET_E(res, write(fd, (void *) uname_str, strlen(uname_str)), -1, FAILURE);
  EQ_RET_E(res, write(cap->sockfd, (void *) uname_str, strlen(uname_str)), -1, FAILURE);

  // Write the max pixel intensity
  EQ_RET_E(res, write(fd, (void *) pixel_max, strlen(pixel_max)), -1, FAILURE);
  EQ_RET_E(res, write(cap->sockfd, (void *) pixel_max, strlen(pixel_max)), -1, FAILURE);

  // Write the image buffer with proper info
  for (uint32_t vres = 0; vres < 480; vres++)
  {
    for (uint32_t hres = 0; hres < 640; hres++)
    {
#ifdef GAMMA_FUNCTION
      // Do gamma function conversion which is specified by the NetPbm spec
      image_buf[count]     = gamma_tf(data->green);
      image_buf[count + 1] = gamma_tf(data->blue);
      image_buf[count + 2] = gamma_tf(data->red);
#else
      // Write the raw intensity
      image_buf[count]     = (data->red);
      image_buf[count + 1] = (data->green);
      image_buf[count + 2] = (data->blue);
#endif
      count += 3;
      data++;
    }
  }

  // Write the image
  EQ_RET_E(res, write(fd, (void *) image_buf, 640*480*3), -1, FAILURE);
  EQ_RET_E(res, write(cap->sockfd, (void *) image_buf, 640*480*3), -1, FAILURE);

  // Success
  return SUCCESS;
}

void * device_capture(void * param)
{
  FUNC_ENTRY;

  struct timespec time;
  struct utsname info;
  cap_t * capture = (cap_t *)param;
  uint32_t count = 0;
  uint32_t fd = 0;
  uint32_t res = 0;
  char file_name[FILE_NAME_MAX];
  char uname_str[UNAME_MAX] = {0};

  // Check for null pointer
  if (NULL == param)
  {
    LOG_ERROR("param parameter was NULL");
    return NULL;
  }

  // Create and write the uname info
  EQ_RET_E(res, uname(&info), -1, NULL);
  res = snprintf(uname_str,
                 UNAME_MAX,
                 "# uname: %s %s %s %s %s\n",
                 info.sysname,
                 info.nodename,
                 info.release,
                 info.version,
                 info.machine);

  // Loop capturing frames and displaying
  while(!abort_test)
  {
    // Wait for start signal
    sem_wait(capture->start);

    // Create the file name to save data
    snprintf(file_name, FILE_NAME_MAX, "%s/capture_%d.ppm", capture->dir_name, count);
    LOG_LOW("Using %s as file name for frame %d", file_name, count);

    // Open file to store contents
    EQ_RET_E(fd, open(file_name, O_CREAT | O_WRONLY, FILE_PERM), -1, NULL);

    // Capture the frame
    START_CAPTURE;

    // Get the time
    clock_gettime(CLOCK_REALTIME, &time);

    // Write data to file
    NOT_EQ_RET_E(res, write_ppm(fd, capture, &time, uname_str), SUCCESS, NULL);

    // Close file properly
    EQ_RET_E(res, close(fd), -1, NULL);

    // Display the frame
    DISPLAY_FRAME;

    // Post done
    sem_post(capture->stop);
    count++;
  }

  return NULL;
} // device_capture()

uint32_t create_dir(char * dir_name, uint8_t length)
{
  struct timespec dir_time;
  struct stat st = {0};
  uint32_t res = 0;
  const char * capture = "capture";

  // Check for null pointer
  CHECK_NULL(dir_name);

  // Get the system time to append to directory to be created
  clock_gettime(CLOCK_REALTIME, &dir_time);

  // Get the seconds to create a directory to store captured images
  snprintf(dir_name, length, "%s", capture);

  // Log the newly create directory name
  LOG_LOW("Trying to create directory name: %s", dir_name);

  // Stat to make sure it doesn't exist
  NOT_EQ_RET_E(res, stat(dir_name, &st), -1, FAILURE);

  // Try to create directory now
  NOT_EQ_RET_E(res, mkdir(dir_name, FILE_PERM), SUCCESS, FAILURE);

  return SUCCESS;
}

int capture()
{
  struct sched_param  sched;
  struct sched_param  test_sched;
  struct timespec diff;
  pthread_t cap_thread;
  pthread_attr_t sched_attr;
  sem_t start;
  sem_t stop;
  cap_t capture;
  int32_t test_policy = 0;
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  uint8_t timer = profiler_init();
  char dir_name[DIR_NAME_MAX] = "capture";

  // Initialize log (does nothing if not using syslog)
  log_init();

  // Print function entry
  FUNC_ENTRY;

#ifdef CLIENT_CONNECT
  // Initialize client socket
  int32_t sockfd = 0;
  NOT_EQ_RET_E(res, client_socket_init(&sockfd), SUCCESS, FAILURE);
#endif

  // Create a window
  cvNamedWindow(WINDOWNAME, CV_WINDOW_AUTOSIZE);

  // Create an array of function pointers
  void * (*transforms[NUM_TRANSFORMS])(void *) = {
    device_capture,
  };

#if 0
  // Create a new directoty
  NOT_EQ_EXIT_E(res, create_dir(dir_name, DIR_NAME_MAX), SUCCESS);
#endif

  // Semaphore for timing
  PT_NOT_EQ_EXIT(res, sem_init(&start, 0, 0), SUCCESS);
  PT_NOT_EQ_EXIT(res, sem_init(&stop, 0, 0), SUCCESS);

  // Create a capture object and set values
  EQ_EXIT_E(capture.capture, (CvCapture *)cvCreateCameraCapture(DEVICE_NUMBER), NULL);

  // Initialize the shedule attributes
  PT_NOT_EQ_EXIT(res, pthread_attr_init(&sched_attr), SUCCESS);

  // Set the attribues structure to PTHREAD_EXPLICIT_SCHED and SCHED_FIFO
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

  // Sed the priority to max - 1 for the test thread
  sched.sched_priority = rt_max_pri - 1;
  PT_NOT_EQ_EXIT(res,
                 pthread_attr_setschedparam(&sched_attr, &sched),
                 SUCCESS);

  // Loop over function pointers to transforms
  for (uint8_t j = 0; j < NUM_TRANSFORMS; j++)
  {
    void * (*cur_func)(void *) = transforms[j];

    // Loop over resolutions
    for (uint8_t i = 0; i < NUM_RESOLUTIONS; i++)
    {
      // Reset abort test
      abort_test = 0;

      // Set resolution
      res_t resolution = resolutions[i];
      LOG_HIGH("Setting resolution to %dx%d", resolution.hres, resolution.vres);
      cvSetCaptureProperty(capture.capture, CV_CAP_PROP_FRAME_WIDTH, resolution.hres);
      cvSetCaptureProperty(capture.capture, CV_CAP_PROP_FRAME_HEIGHT, resolution.vres);

      // Fill out general data structure for capture thread
      capture.dir_name = dir_name;
      capture.sockfd = sockfd;
      capture.start = &start;
      capture.stop = &stop;
      capture.res = &resolution;

      // Create pthread
      PT_NOT_EQ_EXIT(res,
                     pthread_create(&cap_thread, &sched_attr, cur_func, (void *)&capture),
                     SUCCESS);

      // Get the scheduler parameters to display
      PT_NOT_EQ_EXIT(res,
                     pthread_getschedparam(cap_thread, &test_policy, &test_sched),
                     SUCCESS);
      LOG_HIGH("cap_thread policy: %d, priority: %d",
               test_policy,
               test_sched.sched_priority);

#ifdef WARM_UP
      // Loop captures frames to allow camera to warm up
      LOG_MED("Running %d frames for warmup", WARM_UP_FRAMES);
      for (uint8_t frames = 0; frames < WARM_UP_FRAMES; frames++)
      {
        EQ_RET_E(capture.frame, cvQueryFrame(capture.capture), NULL, FAILURE);
      }
#endif

      // Loop captures frames to get stats
      for (uint32_t frames = 0; frames < NUM_FRAMES; frames++)
      {
        // Post semaphore for capture
        sem_post(capture.start);

        // Start the timer
        START_TIME;

        // Sleep for the period of a frame
        usleep(MICROSECONDS_PER_MILLISECOND * PERIOD - TIMING_BUFFER);

        // Wait for frame to be captured
        sem_wait(capture.stop);

        // Get the time
        GET_TIME;

        // Display the time
        DISPLAY_TIMESTAMP;
      }

      // Set the abort flag then allow the thread to exit
      sem_post(capture.start);
      abort_test = 1;
      sem_post(capture.stop);

      // Wait for test thread to join
      PT_NOT_EQ_EXIT(res,
                     pthread_join(cap_thread, NULL),
                     SUCCESS);
      LOG_HIGH("test thread joined");
    }
  }

  // Destroy capture and window
  cvReleaseCapture(&capture.capture);
  cvDestroyWindow(WINDOWNAME);

#ifdef CLIENT_CONNECT
  // Initialize client socket
  NOT_EQ_RET_E(res, client_socket_destroy(sockfd), SUCCESS, FAILURE);
#endif

  log_destroy();
  return 0;
} // capture()
