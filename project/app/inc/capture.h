/** @file capture.h
*
* @brief Defines functions used for exercise3
*
*/

#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include <mqueue.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

// Max number for frames before unlinking old frames
#define MAX_FRAMES (2000)

typedef struct cap_info {
  IplImage * frame;
  struct timespec time;
} cap_info_t;

// Hold resolution information
typedef struct {
  uint32_t hres;
  uint32_t vres;
} resolution_t;

// Used for the image queue
typedef struct {
  mqd_t image_q;
  struct mq_attr attr;
} image_q_inf_t;

#define QUEUE_NAME "/frame_queue"

/*!
* @brief Starts capture, jpeg/ppm, and server service.  Then becomes the
* scheduler service
* @return SUCCESS/FAILURE
*/
int sched_service();

#endif /* __CAPTURE_H__ */
