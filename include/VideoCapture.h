#ifndef VIDEOCAPTURE_H
#define VIDEOCAPTURE_H

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define VIDEO_CAPTURE_BUFFER_COUNT 3

typedef struct FrameBuffer {
    void*         start;
    unsigned long length;
} FrameBuffer;

typedef struct VideoCapture {
    int                 fd;
    FrameBuffer*        frameBuffers;
    void*               leasedFrameBuffer;
    struct v4l2_buffer* leasedV4l2Buffer;
} VideoCapture;

VideoCapture* VideoCaptureCreate(char* deviceName, unsigned int resolutionWidth, unsigned int resolutionHeight, unsigned int timebaseNumerator, unsigned int timebaseDenominator);
unsigned int  VideoCaptureGetFrame(VideoCapture* videoCapture);
void          VideoCaptureReturnFrame(VideoCapture* videoCapture);
void          VideoCaptureFree(VideoCapture* videoCapture);

#endif