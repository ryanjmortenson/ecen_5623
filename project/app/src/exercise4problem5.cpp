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
#include <time.h>
#include <unistd.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "log.h"
#include "profiler.h"
#include "project_defs.h"

using namespace std;
using namespace cv;

#define WINDOWNAME "Exercise4Problem5"
#define DEVICE_NUMBER (0)
#define MICROSECONDS_PER_SECOND (1000000)
#define MICROSECONDS_PER_MILLISECOND (1000)
#define NUM_FRAMES (100)
#define PERIOD (100)
#define NUM_SECONDS (5)
#define NUM_RESOLUTIONS (5)
#define NUM_TRANSFORMS (3)
#define WARM_UP_FRAMES (40)

#define TIMING_BUFFER (50)
#define ADD_MILLISECONDS if (frames > 0) milliseconds += float(diff.tv_nsec) / 1000000
#define ADD_JITTER if (frames > 0) jitter += (float(diff.tv_nsec - TIMING_BUFFER) / 1000000) - PERIOD
#define DISPLAY_AVG_MILLISECONDS LOG_MED("Avg milliseconds: %f", milliseconds / (NUM_FRAMES - 1))
#define DISPLAY_AVG_JITTER LOG_MED("Avg jitter in milliseconds: %f", jitter / (NUM_FRAMES - 1))
#define DISPLAY_TIMESTAMP LOG_LOW("sec: %d, millisec: %f", diff.tv_sec, float(diff.tv_nsec)/1000000)
#define START_TIME start_timer(timer)
#define START_CAPTURE EQ_RET_E(capture->frame, cvQueryFrame(capture->capture), NULL, NULL);
#define DISPLAY_FRAME cvShowImage(WINDOWNAME, capture->frame); \
                      cvWaitKey(1);
#define GET_TIME stop_timer(timer); \
                 get_time(timer, &diff);

// Structure for resolutions
typedef struct res {
  uint32_t hres;
  uint32_t vres;
} res_t;

// Array of resolutions
res_t resolutions[NUM_RESOLUTIONS] = {
  {.hres = 640,  .vres = 480},
  {.hres = 640,  .vres = 400},
  {.hres = 352,  .vres = 288},
  {.hres = 320,  .vres = 240},
  {.hres = 160,  .vres = 120},
};

// Capture structure to pass into pthread
typedef struct cap {
  CvCapture * capture;
  IplImage * frame;
  sem_t * start;
  sem_t * stop;
} cap_t;

// Flag for killing thread
uint32_t abort_test = 0;

void * hough_int(void * param)
{
  FUNC_ENTRY;
  cap_t * capture = (cap_t *)param;

  Mat gray, canny_frame, cdst;
  vector<Vec4i> lines;

  // Loop capturing frames and displaying
  while(!abort_test)
  {
    // Wait for start signal
    sem_wait(capture->start);

    // Capture the frame
    START_CAPTURE;

    // Execute transform
    Mat mat_frame(capture->frame);
    Canny(mat_frame, canny_frame, 50, 200, 3);

    cvtColor(canny_frame, cdst, CV_GRAY2BGR);
    cvtColor(mat_frame, gray, CV_BGR2GRAY);

    HoughLinesP(canny_frame, lines, 1, CV_PI/180, 50, 50, 10);

    for( size_t i = 0; i < lines.size(); i++ )
    {
      Vec4i l = lines[i];
      line(mat_frame, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0,0,255), 3, CV_AA);
    }
    // Display the frame
    DISPLAY_FRAME;

    // Post done
    sem_post(capture->stop);
  }

  return NULL;
} // hough_int

void * hough_ellip(void * param)
{
  FUNC_ENTRY;
  cap_t * capture = (cap_t *)param;
  Mat gray, canny_frame, cdst;
  vector<Vec3f> circles;

  // Loop capturing frames and displaying
  while(!abort_test)
  {
    // Wait for start signal
    sem_wait(capture->start);

    // Capture frame
    START_CAPTURE;

    // Do Transform
    Mat mat_frame(capture->frame);
    cvtColor(mat_frame, gray, CV_BGR2GRAY);
    GaussianBlur(gray, gray, Size(9,9), 2, 2);

    HoughCircles(gray, circles, CV_HOUGH_GRADIENT, 1, gray.rows/8, 100, 50, 0, 0);

    for( size_t i = 0; i < circles.size(); i++ )
    {
      Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
      int radius = cvRound(circles[i][2]);
      // circle center
      circle( mat_frame, center, 3, Scalar(0,255,0), -1, 8, 0 );
      // circle outline
      circle( mat_frame, center, radius, Scalar(0,0,255), 3, 8, 0 );
    }
    // Display the frame
    DISPLAY_FRAME;

    // Post done
    sem_post(capture->stop);
  }

  return NULL;
} // hough_ellip

void * canny_int(void * param)
{
  FUNC_ENTRY;
  cap_t * capture = (cap_t *)param;
  Mat canny_frame, cdst, timg_gray, timg_grad;
  vector<Vec3f> circles;
  int lowThreshold=0;
  int kernel_size = 3;
  int ratio = 3;

  // Loop capturing frames and displaying
  while(!abort_test)
  {
    // Wait for start signal
    sem_wait(capture->start);

    // Capture Frame
    START_CAPTURE;

    Mat mat_frame(capture->frame);
    cvtColor(mat_frame, timg_gray, CV_RGB2GRAY);

    /// Reduce noise with a kernel 3x3
    blur( timg_gray, canny_frame, Size(3,3) );

    /// Canny detector
    Canny( canny_frame, canny_frame, lowThreshold, lowThreshold*ratio, kernel_size );

    /// Using Canny's output as a mask, we display our result
    timg_grad = Scalar::all(0);
    mat_frame.copyTo( timg_grad, canny_frame);

    // Display the frame
    DISPLAY_FRAME;

    // Post done
    sem_post(capture->stop);
  }

  return NULL;
} // hough_ellip


int ex4prob5()
{
  FUNC_ENTRY;
  log_init();

  // Create a window
  cvNamedWindow(WINDOWNAME, CV_WINDOW_AUTOSIZE);

  // Scheduling parameters
  struct sched_param  sched;
  struct sched_param  test_sched;
  int32_t test_policy = 0;
  int32_t res = 0;
  int32_t rt_max_pri = 0;
  float milliseconds = 0.0f;
  float jitter = 0.0f;
  uint8_t timer = profiler_init();
  cap_t capture;
  struct timespec diff;

  // Pthreads and pthread attributes
  pthread_t trans_thread;
  pthread_attr_t sched_attr;

  // Create an array of function pointers
  void * (*transforms[NUM_TRANSFORMS])(void *) = {
    hough_int,
    hough_ellip,
    canny_int
  };

  // Semaphore for timing
  sem_t start;
  sem_t stop;
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
    void * (*cur_function)(void *) = transforms[j];

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
      capture.start = &start;
      capture.stop = &stop;

      // Create pthread
      PT_NOT_EQ_EXIT(res,
                     pthread_create(&trans_thread, &sched_attr, cur_function, (void *)&capture),
                     SUCCESS);

      // Get the scheduler parameters to display
      PT_NOT_EQ_EXIT(res,
                     pthread_getschedparam(trans_thread, &test_policy, &test_sched),
                     SUCCESS);
      LOG_HIGH("trans_thread policy: %d, priority: %d",
               test_policy,
               test_sched.sched_priority);

    

      // Loop captures frames to allow camera to warm up 
      LOG_MED("Running %d frames for warmup", WARM_UP_FRAMES);
      for (uint8_t frames = 0; frames < WARM_UP_FRAMES; frames++)
      {
        sem_post(capture.start);
        sem_wait(capture.stop);
      }

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

        // Add up milliseconds and jitter to find and average
        ADD_MILLISECONDS;
        ADD_JITTER;

        // Display the time
        DISPLAY_TIMESTAMP;
      }

      DISPLAY_AVG_MILLISECONDS;
      DISPLAY_AVG_JITTER;

      // Reset counters
      milliseconds = 0;
      jitter = 0;

      // Set the abort flag then allow the thread to exit
      sem_post(capture.start);
      abort_test = 1;
      sem_post(capture.stop);
        

      // Wait for test thread to join
      PT_NOT_EQ_EXIT(res,
                     pthread_join(trans_thread, NULL),
                     SUCCESS);
      LOG_HIGH("test thread joined");
    }
  }

  // Destroy capture and window
  cvReleaseCapture(&capture.capture);
  cvDestroyWindow(WINDOWNAME);

  log_destroy();
  return 0;
} // ex4prob5()