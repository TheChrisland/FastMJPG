#include "../include/VideoCapture.h"
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
#include <time.h>
#include <math.h>

static uint64_t getEpochTimeShift() {
    struct timeval epochTime;
    struct timespec monotonicTime;
    gettimeofday(&epochTime, NULL);
    clock_gettime(CLOCK_MONOTONIC, &monotonicTime);
    uint64_t uMonotonicTime = monotonicTime.tv_sec * 1000000 + (uint64_t)round(monotonicTime.tv_nsec / 1000.0);
    uint64_t uEpochTime = epochTime.tv_sec * 1000000 + epochTime.tv_usec;
    return uEpochTime - uMonotonicTime;
}

static int xioctl(int fd, long unsigned int requestCode, void* arg) {
    for (;;) {
        int result = ioctl(fd, requestCode, arg);
        if (result == -1 && errno == EINTR) {
            continue;
        }
        return result;
    }
}

VideoCapture* VideoCaptureCreate(char* deviceName, unsigned int resolutionWidth, unsigned int resolutionHeight, unsigned int timebaseNumerator, unsigned int timebaseDenominator) {
    VideoCapture* videoCapture = malloc(sizeof(VideoCapture));
    if (videoCapture == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoCapture.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoCapture, 0, sizeof(VideoCapture));
    videoCapture->epochTimeShift = getEpochTimeShift();
    videoCapture->leasedFrameBuffer = malloc(sizeof(FrameBuffer));
    if (videoCapture->leasedFrameBuffer == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoCapture leased frame buffer.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoCapture->leasedFrameBuffer, 0, sizeof(FrameBuffer));
    videoCapture->leasedV4l2Buffer = malloc(sizeof(struct v4l2_buffer));
    if (videoCapture->leasedV4l2Buffer == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoCapture leased V4L2 buffer.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoCapture->leasedV4l2Buffer, 0, sizeof(struct v4l2_buffer));
    struct stat fileStats;
    memset(&fileStats, 0, sizeof(fileStats));
    if (stat(deviceName, &fileStats) == -1) {
        fprintf(stderr, "Error: Device name did not exist.\n");
        exit(EXIT_FAILURE);
    }
    if (!S_ISCHR(fileStats.st_mode)) {
        fprintf(stderr, "Error: Device was not a special character file desriptor.\n");
        exit(EXIT_FAILURE);
    }
    videoCapture->fd = open(deviceName, O_RDWR, 0);
    if (videoCapture->fd == -1) {
        fprintf(stderr, "Error: Couln't open device.\n");
        exit(EXIT_FAILURE);
    }
    struct v4l2_capability v4l2DeviceCapabilities;
    memset(&v4l2DeviceCapabilities, 0, sizeof(v4l2DeviceCapabilities));
    if (xioctl(videoCapture->fd, VIDIOC_QUERYCAP, &v4l2DeviceCapabilities) == -1) {
        fprintf(stderr, "Error: Unexpected error querying device capabilities VIDIOC_QUERYCAP.\n");
        exit(EXIT_FAILURE);
    }
    if (!(v4l2DeviceCapabilities.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Error: Not a video capture device.\n");
        exit(EXIT_FAILURE);
    }
    if (!(v4l2DeviceCapabilities.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Error: Device does not support streaming.\n");
        exit(EXIT_FAILURE);
    }
    struct v4l2_format v4l2DeviceFormat;
    memset(&v4l2DeviceFormat, 0, sizeof(v4l2DeviceFormat));
    v4l2DeviceFormat.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2DeviceFormat.fmt.pix.width       = resolutionWidth;
    v4l2DeviceFormat.fmt.pix.height      = resolutionHeight;
    v4l2DeviceFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    if (xioctl(videoCapture->fd, VIDIOC_TRY_FMT, &v4l2DeviceFormat) == -1) {
        fprintf(stderr, "Error: Unexpected error trying device format VIDIOC_TRY_FMT.\n");
        exit(EXIT_FAILURE);
    }
    if (v4l2DeviceFormat.fmt.pix.width != resolutionWidth || v4l2DeviceFormat.fmt.pix.height != resolutionHeight || v4l2DeviceFormat.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        fprintf(stderr, "Error: Device did not accept requested format.\n");
        exit(EXIT_FAILURE);
    }
    if (xioctl(videoCapture->fd, VIDIOC_S_FMT, &v4l2DeviceFormat) == -1) {
        fprintf(stderr, "Error: Unexpected error setting device format VIDIOC_S_FMT.\n");
        exit(EXIT_FAILURE);
    }
    struct v4l2_streamparm v4l2StreamParameters;
    memset(&v4l2StreamParameters, 0, sizeof(v4l2StreamParameters));
    v4l2StreamParameters.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2StreamParameters.parm.capture.timeperframe.numerator   = timebaseNumerator;
    v4l2StreamParameters.parm.capture.timeperframe.denominator = timebaseDenominator;
    if (xioctl(videoCapture->fd, VIDIOC_S_PARM, &v4l2StreamParameters) < 0) {
        fprintf(stderr, "Error: Unexpected error setting device stream paramaters VIDIOC_S_PARM.\n");
        exit(EXIT_FAILURE);
    }
    struct v4l2_requestbuffers v4l2RequestBuffers;
    memset(&v4l2RequestBuffers, 0, sizeof(v4l2RequestBuffers));
    v4l2RequestBuffers.count  = VIDEO_CAPTURE_BUFFER_COUNT;
    v4l2RequestBuffers.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2RequestBuffers.memory = V4L2_MEMORY_MMAP;
    if (xioctl(videoCapture->fd, VIDIOC_REQBUFS, &v4l2RequestBuffers) == -1) {
        fprintf(stderr, "Error: Unexpected error requesting buffers VIDIOC_REQBUFS.\n");
        exit(EXIT_FAILURE);
    }
    if (v4l2RequestBuffers.count != VIDEO_CAPTURE_BUFFER_COUNT) {
        fprintf(stderr, "Error: Device did not accept requested number of buffers.\n");
        exit(EXIT_FAILURE);
    }
    videoCapture->frameBuffers = malloc(sizeof(FrameBuffer) * VIDEO_CAPTURE_BUFFER_COUNT);
    if (!videoCapture->frameBuffers) {
        fprintf(stderr, "Error: Unable to allocate memory for frame buffers.\n");
        exit(EXIT_FAILURE);
    }
    for (unsigned int frameBufferIndex = 0; frameBufferIndex < VIDEO_CAPTURE_BUFFER_COUNT; frameBufferIndex++) {
        struct v4l2_buffer v4l2Buffer;
        memset(&v4l2Buffer, 0, sizeof(v4l2Buffer));
        v4l2Buffer.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2Buffer.memory = V4L2_MEMORY_MMAP;
        v4l2Buffer.index  = frameBufferIndex;
        if (xioctl(videoCapture->fd, VIDIOC_QUERYBUF, &v4l2Buffer) == -1) {
            fprintf(stderr, "Error: Unexpected error querying frame buffer VIDIOC_QUERYBUF.\n");
            exit(EXIT_FAILURE);
        }
        videoCapture->frameBuffers[frameBufferIndex].length = v4l2Buffer.length;
        videoCapture->frameBuffers[frameBufferIndex].start  = mmap(NULL, v4l2Buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, videoCapture->fd, v4l2Buffer.m.offset);
        if (videoCapture->frameBuffers[frameBufferIndex].start == MAP_FAILED) {
            fprintf(stderr, "Error: Unexpected error mapping frame buffer memory MAP_FAILED.\n");
            exit(EXIT_FAILURE);
        }
    }
    for (unsigned frameBufferIndex = 0; frameBufferIndex < VIDEO_CAPTURE_BUFFER_COUNT; frameBufferIndex++) {
        struct v4l2_buffer v4l2Buffer;
        memset(&v4l2Buffer, 0, sizeof(v4l2Buffer));
        v4l2Buffer.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2Buffer.memory = V4L2_MEMORY_MMAP;
        v4l2Buffer.index  = frameBufferIndex;
        if (xioctl(videoCapture->fd, VIDIOC_QBUF, &v4l2Buffer) == -1) {
            fprintf(stderr, "Error: Unexpected error queueing frame buffer VIDIOC_QBUF.\n");
            exit(EXIT_FAILURE);
        }
    }
    enum v4l2_buf_type v4l2BufferType;
    v4l2BufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(videoCapture->fd, VIDIOC_STREAMON, &v4l2BufferType) == -1) {
        fprintf(stderr, "Error: Unexpected error starting stream VIDIOC_STREAMON.\n");
        exit(EXIT_FAILURE);
    }
    return videoCapture;
}

void VideoCaptureGetFrame(VideoCapture* videoCapture) {
    memset(videoCapture->leasedV4l2Buffer, 0, sizeof(struct v4l2_buffer));
    videoCapture->leasedV4l2Buffer->type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    videoCapture->leasedV4l2Buffer->memory = V4L2_MEMORY_MMAP;
    if (xioctl(videoCapture->fd, VIDIOC_DQBUF, videoCapture->leasedV4l2Buffer) == -1) {
        fprintf(stderr, "Error: Unexpected error dequeueing frame buffer VIDIOC_DQBUF.\n");
        exit(EXIT_FAILURE);
    }
    videoCapture->leasedFrameBuffer = &videoCapture->frameBuffers[videoCapture->leasedV4l2Buffer->index];
    videoCapture->leasedFrameBuffer->bytesUsed = videoCapture->leasedV4l2Buffer->bytesused;
    videoCapture->leasedFrameBuffer->uTimestamp = videoCapture->leasedV4l2Buffer->timestamp.tv_sec * 1000000 + videoCapture->leasedV4l2Buffer->timestamp.tv_usec + videoCapture->epochTimeShift;
}

void VideoCaptureReturnFrame(VideoCapture* videoCapture) {
    if (xioctl(videoCapture->fd, VIDIOC_QBUF, videoCapture->leasedV4l2Buffer) == -1) {
        fprintf(stderr, "Error: Unexpected error queueing frame buffer VIDIOC_QBUF.\n");
        exit(EXIT_FAILURE);
    }
}

void VideoCaptureFree(VideoCapture* videoCapture) {
    enum v4l2_buf_type v4l2BufferType;
    v4l2BufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(videoCapture->fd, VIDIOC_STREAMOFF, &v4l2BufferType) == -1) {
        fprintf(stderr, "Error: Unexpected error stopping stream VIDIOC_STREAMOFF.\n");
        exit(EXIT_FAILURE);
    }
    for (unsigned int frameBufferIndex = 0; frameBufferIndex < VIDEO_CAPTURE_BUFFER_COUNT; frameBufferIndex++) {
        if (munmap(videoCapture->frameBuffers[frameBufferIndex].start, videoCapture->frameBuffers[frameBufferIndex].length) == -1) {
            fprintf(stderr, "Error: Unexpected error unmapping frame buffer memory munmap.\n");
            exit(EXIT_FAILURE);
        }
    }
    free(videoCapture->frameBuffers);
    if (close(videoCapture->fd) == -1) {
        fprintf(stderr, "Error: Unexpected error closing device file descriptor.\n");
        exit(EXIT_FAILURE);
    }
    videoCapture->fd = -1;
    free(videoCapture);
}
