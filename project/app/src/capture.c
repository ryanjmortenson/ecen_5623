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

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "log.h"
#include "profiler.h"
#include "project_defs.h"
#include "client.h"

#define WARM_UP
#define CLIENT_CONNECT

// Socket info
#define PORT (12345)
#define ADDRESS "127.0.0.1"

// Open CV info
#define WINDOWNAME "capture"
#define DEVICE_NUMBER (0)

// Timing info
#define MICROSECONDS_PER_SECOND (1000000)
#define MICROSECONDS_PER_MILLISECOND (1000)
#define NUM_FRAMES (1)
#define PERIOD (100)
#define WARM_UP_FRAMES (40)

// File info
#define DIR_NAME_MAX (255)
#define FILE_NAME_MAX DIR_NAME_MAX
#define FILE_PERM (S_IRWXU | S_IRWXO | S_IRWXG)
#define TIMESTAMP_MAX (40)
#define UNAME_MAX (256)
#define TIMING_BUFFER (100)

// Image info
#define MAX_INTENSITY_STR "255\n"
#define P6_HEADER "P6\n640 480\n"
#define MAX_INTENSITY (255)
#define MAX_INTENSITY_FLOAT (255.0f)
#define HRES (640)
#define VRES (480)
#define BYTES_PER_PIXEL (3)
#define IMAGE_NUM_BYTES (HRES * VRES * BYTES_PER_PIXEL)

// Helpful Macros
#define DISPLAY_TIMESTAMP LOG_LOW("sec: %d, millisec: %f", diff.tv_sec, (float)(diff.tv_nsec)/1000000)
#define START_TIME start_timer(timer)
#define START_CAPTURE(cap) EQ_RET_E(cap->frame, cvQueryFrame(cap->capture), NULL, NULL);
#define DISPLAY_FRAME(cap) cvShowImage(WINDOWNAME, cap->frame); \
                           cvWaitKey(1);
#define GET_TIME stop_timer(timer); \
                 get_time(timer, &diff);

// Image buffer for current frame
static char image_buf[IMAGE_NUM_BYTES];

// Structure for resolutions
typedef struct ppm {
  char timestamp[TIMESTAMP_MAX];
  char * uname_str;
  char * header;
  char * max_val;
  char * color_data;
} ppm_t;

// Capture structure to pass into pthread
typedef struct cap {
  CvCapture * capture;
  IplImage * frame;
  sem_t * start;
  sem_t * stop;
  char * dir_name;
#ifdef CLIENT_CONNECT
  int32_t sockfd;
#endif // CLIENT_CONNECT
} cap_t;

// Flag for killing thread
static uint32_t abort_test = 0;

// Structure to overlay blue green
typedef struct colors {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
} __attribute__((packed)) colors_t;

/*!
* @brief Applies gamma transfer function to color intensity
* @param color color intensity
* @return transformed result
*/
static inline
uint8_t gamma_tf(uint8_t color)
{
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

/*!
* @brief Addes the timestamp string to ppm struct
* @param ppm ppm structure to set data pointer
* @param time timespec structure holding image timestamp
* @return SUCCESS/FAILURE
*/
static inline
uint32_t add_timestamp(ppm_t * ppm, struct timespec * time)
{
  FUNC_ENTRY;
  CHECK_NULL(ppm);
  CHECK_NULL(time);

  // Create and write the timestamp
  snprintf(ppm->timestamp,
           TIMESTAMP_MAX,
           "# Timestamp: %f\n",
           (double) time->tv_sec + (double) time->tv_nsec);

  return SUCCESS;
} // add_timestamp()

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
* @brief Spins off a thread to do capture
* @param param void pointer to params
* @return NULL
*/
void * cap_func(void * param)
{
  FUNC_ENTRY;

  struct timespec time;
  struct utsname info;
  cap_t * cap = (cap_t *)param;
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

  // Create ppm struct since the only ppm
  ppm_t p6 = {
    .header = P6_HEADER,
    .max_val = MAX_INTENSITY_STR,
    .uname_str = uname_str
  };

  // Loop capturing frames and displaying
  while(!abort_test)
  {
    // Wait for start signal
    sem_wait(cap->start);

    // Create the file name to save data
    snprintf(file_name, FILE_NAME_MAX, "%s/capture_%d.ppm", cap->dir_name, count);
    LOG_LOW("Using %s as file name for frame %d", file_name, count);

    // Open file to store contents
    EQ_RET_E(fd, open(file_name, O_CREAT | O_WRONLY, FILE_PERM), -1, NULL);

    // Capture the frame
    START_CAPTURE(cap);

    // Get the time
    clock_gettime(CLOCK_REALTIME, &time);

    // Add the timestamp
    NOT_EQ_RET_E(res, add_timestamp(&p6, &time), SUCCESS, NULL);

    // Add the timestamp
    NOT_EQ_RET_E(res, create_image_buf(&p6, (colors_t *)cap->frame->imageData), SUCCESS, NULL);

    // Write data to file
    NOT_EQ_RET_E(res, write_ppm(fd, &p6), SUCCESS, NULL);

#ifdef CLIENT_CONNECT
    // Write data to socket
    NOT_EQ_RET_E(res, write_ppm(cap->sockfd, &p6), SUCCESS, NULL);
#endif // CLIENT_CONNECT

    // Close file properly
    EQ_RET_E(res, close(fd), -1, NULL);

    // Display the frame
    DISPLAY_FRAME(cap);

    // Post done
    sem_post(cap->stop);
    count++;
  }

  return NULL;
} // cap_func()

/*!
* @brief Creates a new directory for new captures
* @param dirname pointer to buffer holding first part of directory name
* @param length length of buffer holding directory name
* @return SUCCESS/FAILURE
*/
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
} // create_dir()

int capture()
{
  struct sched_param  sched;
  struct sched_param  cap_sched;
  struct timespec diff;
  pthread_t cap_thread;
  pthread_attr_t sched_attr;
  sem_t start;
  sem_t stop;
  cap_t cap;
  int32_t cap_policy = 0;
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
  NOT_EQ_RET_E(res, client_socket_init(&sockfd, ADDRESS, PORT), SUCCESS, FAILURE);
#endif // CLIENT_CONNECT

  // Create a window
  cvNamedWindow(WINDOWNAME, CV_WINDOW_AUTOSIZE);

#if 0
  // Create a new directory
  NOT_EQ_EXIT_E(res, create_dir(dir_name, DIR_NAME_MAX), SUCCESS);
#endif

  // Semaphore for timing
  PT_NOT_EQ_EXIT(res, sem_init(&start, 0, 0), SUCCESS);
  PT_NOT_EQ_EXIT(res, sem_init(&stop, 0, 0), SUCCESS);

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

  // Fill out general data structure for capture thread
  cap.dir_name = dir_name;
#ifdef CLIENT_CONNECT
  cap.sockfd = sockfd;
#endif // CLIENT_CONNECT
  cap.start = &start;
  cap.stop = &stop;

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

#ifdef WARM_UP
  // Loop captures frames to allow camera to warm up
  LOG_MED("Running %d frames for warm up", WARM_UP_FRAMES);
  for (uint8_t frames = 0; frames < WARM_UP_FRAMES; frames++)
  {
    EQ_RET_E(cap.frame, cvQueryFrame(cap.capture), NULL, FAILURE);
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
  LOG_HIGH("test thread joined");

  // Destroy capture and window
  cvReleaseCapture(&cap.capture);
  cvDestroyWindow(WINDOWNAME);

#ifdef CLIENT_CONNECT
  // Initialize client socket
  NOT_EQ_RET_E(res, client_socket_dest(sockfd), SUCCESS, FAILURE);
#endif // CLIENT_CONNECT

  log_destroy();
  return 0;
} // capture()
