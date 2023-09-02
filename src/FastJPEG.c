#include "../include/VideoCapture.h"
#include "../include/VideoDecoder.h"
#include "../include/VideoPipe.h"
#include "../include/VideoRecorder.h"
#include "../include/VideoRenderer.h"
#include "../include/VideoUDPReceiver.h"
#include "../include/VideoUDPSender.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_PARAMS 32
#define MAX_WINDOW_TITLE_LENGTH 256
#define MAX_VIDEO_DEVICE_PATH_LENGTH 512
#define PARAM_TYPE_CAPTURE 0
#define PARAM_TYPE_RECEIVE 1
#define PARAM_TYPE_RENDER 2
#define PARAM_TYPE_RECORD 3
#define PARAM_TYPE_SEND 4
#define PARAM_TYPE_PIPE 5

typedef struct CaptureParams {
    char*         deviceName;
    unsigned int  resolutionWidth;
    unsigned int  resolutionHeight;
    unsigned int  timebaseNumerator;
    unsigned int  timebaseDenominator;
    VideoCapture* videoCapture;
} CaptureParams;

typedef struct ReceiveParams {
    char*               localIPAddress;
    unsigned int        localPort;
    struct sockaddr_in* localAddress;
    unsigned int        maxPacketLength;
    unsigned int        maxJPEGLength;
    void*               jpegBuffer;
    unsigned int        jpegBufferLength;
    unsigned int        resolutionWidth;
    unsigned int        resolutionHeight;
    unsigned int        timebaseNumerator;
    unsigned int        timebaseDenominator;
    VideoUDPReceiver*   videoUDPReceiver;
} ReceiveParams;

typedef struct RenderParams {
    unsigned int   windowWidth;
    unsigned int   windowHeight;
    VideoRenderer* videoRenderer;
} RenderParams;

typedef struct RecordParams {
    char*          fileName;
    VideoRecorder* videoRecorder;
} RecordParams;

typedef struct SendParams {
    char*               localIPAddress;
    unsigned int        localPort;
    struct sockaddr_in* localAddress;
    char*               remoteIPAddress;
    unsigned int        remotePort;
    struct sockaddr_in* remoteAddress;
    unsigned int        maxPacketLength;
    unsigned int        maxJPEGLength;
    unsigned int        sendRounds;
    VideoUDPSender*     videoUDPSender;
} SendParams;

typedef struct PipeParams {
    int          pipeFileDescriptor;
    char*        rgbOrJPEG;
    bool         rgb;
    unsigned int maxPacketLength;
    VideoPipe*   videoPipe;
} PipeParams;

static void*         params[MAX_PARAMS];
static unsigned int  paramsTypes[MAX_PARAMS];
static unsigned int  paramsCount               = 0;
static unsigned int  sourceWidth               = 0;
static unsigned int  sourceHeight              = 0;
static unsigned int  sourceTimebaseNumerator   = 0;
static unsigned int  sourceTimebaseDenominator = 0;
static char*         renderWindowTitle         = NULL;
static uint64_t      uTimestamp                = 0;
static void*         jpegBuffer                = NULL;
static unsigned int  jpegBufferLength          = 0;
static VideoDecoder* videoDecoder              = NULL;
static bool          receivedSigint            = false;

static inline void printUsage() {
    printf("FastMJPG\n");
    printf("\n");
    printf("Help:\n");
    printf("fj help\n");
    printf("\n");
    printf("Devices:\n");
    printf("fj devices\n");
    printf("\n");
    printf("Usage:\n");
    printf("fj [input] [output 0] [output 1] ... [output n]\n");
    printf("\n");
    printf("Input:\n");
    printf("    capture\n");
    printf("        DEVICE_NAME           (string)  ie. /dev/video0\n");
    printf("        RESOLUTION_WIDTH      (uint)    ie. 1280\n");
    printf("        RESOLUTION_HEIGHT     (uint)    ie. 720\n");
    printf("        TIMEBASE_NUMERATOR    (uint)    ie. 1\n");
    printf("        TIMEBASE_DENOMINATOR  (uint)    ie. 30\n");
    printf("\n");
    printf("    receive\n");
    printf("        LOCAL_IP_ADDRESS      (string)  ie. 192.168.1.1\n");
    printf("        LOCAL_PORT            (uint)    ie. 8000\n");
    printf("        MAX_PACKET_LENGTH     (uint)    ie. 1400\n");
    printf("        MAX_JPEG_LENGTH       (uint)    ie. 1000000\n");
    printf("        RESOLUTION_WIDTH      (uint)    ie. 1280\n");
    printf("        RESOLUTION_HEIGHT     (uint)    ie. 720\n");
    printf("        TIMEBASE_NUMERATOR    (uint)    ie. 1\n");
    printf("        TIMEBASE_DENOMINATOR  (uint)    ie. 30\n");
    printf("\n");
    printf("Output:\n");
    printf("    render\n");
    printf("        WINDOW_WIDTH          (uint)    ie. 1280\n");
    printf("        WINDOW_HEIGHT         (uint)    ie. 720\n");
    printf("\n");
    printf("    record\n");
    printf("        FILE_NAME             (string)  ie. /home/user/video.mkv\n");
    printf("\n");
    printf("    send\n");
    printf("        LOCAL_IP_ADDRESS      (string)  ie. 192.168.1.1\n");
    printf("        LOCAL_PORT            (uint)    ie. 8000\n");
    printf("        REMOTE_IP_ADDRESS     (string)  ie. 192.168.1.1\n");
    printf("        REMOTE_PORT           (uint)    ie. 8000\n");
    printf("        MAX_PACKET_LENGTH     (uint)    ie. 1400\n");
    printf("        MAX_JPEG_LENGTH       (uint)    ie. 1000000\n");
    printf("        SEND_ROUNDS           (uint)    ie. 1\n");
    printf("\n");
    printf("    pipe\n");
    printf("        PIPE_FILE_DESCRIPTOR  (int)     ie. 3\n");
    printf("        RGB_OR_JPEG           (string)  ie. rgb or jpeg\n");
    printf("        MAX_PACKET_LENGTH     (uint)    ie. 4096\n");
}

static inline void printDevices() {
    struct v4l2_fmtdesc     format;
    struct v4l2_frmsizeenum size;
    struct v4l2_frmivalenum rate;
    bool                    supportsMJPG;
    bool                    firstFramerate;
    struct dirent*          entry;
    DIR*                    dir = opendir("/dev");
    if (!dir) {
        perror("Cannot open /dev");
        exit(EXIT_FAILURE);
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "video", 5) == 0) {
            char path[MAX_VIDEO_DEVICE_PATH_LENGTH];
            snprintf(path, MAX_VIDEO_DEVICE_PATH_LENGTH, "/dev/%s", entry->d_name);
            int fd = open(path, O_RDWR);
            if (fd == -1) {
                perror("Cannot open device");
                continue;
            }
            printf("%s:\n", path);
            supportsMJPG = false;
            memset(&format, 0, sizeof(format));
            format.index = 0;
            format.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            while (ioctl(fd, VIDIOC_ENUM_FMT, &format) == 0) {
                if (format.pixelformat == V4L2_PIX_FMT_MJPEG) {
                    supportsMJPG = true;
                    break;
                }
                format.index++;
            }
            if (!supportsMJPG) {
                printf("    Does not support MJPEG\n");
                close(fd);
                continue;
            }
            memset(&size, 0, sizeof(size));
            size.index        = 0;
            size.pixel_format = V4L2_PIX_FMT_MJPEG;
            while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size) == 0) {
                if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    printf("  %dx%d ->", size.discrete.width, size.discrete.height);
                    memset(&rate, 0, sizeof(rate));
                    rate.index        = 0;
                    rate.pixel_format = V4L2_PIX_FMT_MJPEG;
                    rate.width        = size.discrete.width;
                    rate.height       = size.discrete.height;
                    firstFramerate    = true;
                    while (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &rate) == 0) {
                        if (rate.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                            if (!firstFramerate) {
                                printf(",");
                            }
                            printf(" %dFPS [%u/%u]", (int)(rate.discrete.denominator / rate.discrete.numerator), rate.discrete.numerator, rate.discrete.denominator);
                            firstFramerate = false;
                        }
                        rate.index++;
                    }
                    printf("\n");
                }
                size.index++;
            }
            close(fd);
        }
    }
    closedir(dir);
}

static inline void parseParams(int argc, char** argv) {
    int argn = 1;
    for (;;) {
        if (argn >= argc) {
            break;
        } else if (paramsCount >= MAX_PARAMS) {
            fprintf(stderr, "Too many params.\n");
            exit(EXIT_FAILURE);
        }
        if (strcmp(argv[argn], "capture") == 0) {
            if (argc < argn + 6) {
                fprintf(stderr, "Not enough arguments.\n");
                exit(EXIT_FAILURE);
            }
            CaptureParams* captureParams = malloc(sizeof(CaptureParams));
            if (captureParams == NULL) {
                fprintf(stderr, "Unable to allocate memory for capture params.\n");
                exit(EXIT_FAILURE);
            }
            memset(captureParams, 0, sizeof(CaptureParams));
            captureParams->deviceName          = argv[argn + 1];
            captureParams->resolutionWidth     = atoi(argv[argn + 2]);
            sourceWidth                        = captureParams->resolutionWidth;
            captureParams->resolutionHeight    = atoi(argv[argn + 3]);
            sourceHeight                       = captureParams->resolutionHeight;
            captureParams->timebaseNumerator   = atoi(argv[argn + 4]);
            sourceTimebaseNumerator            = captureParams->timebaseNumerator;
            captureParams->timebaseDenominator = atoi(argv[argn + 5]);
            sourceTimebaseDenominator          = captureParams->timebaseDenominator;
            argn += 6;
            captureParams->videoCapture = VideoCaptureCreate(captureParams->deviceName, captureParams->resolutionWidth, captureParams->resolutionHeight, captureParams->timebaseNumerator, captureParams->timebaseDenominator);
            renderWindowTitle           = malloc(MAX_WINDOW_TITLE_LENGTH);
            if (renderWindowTitle == NULL) {
                fprintf(stderr, "Unable to allocate memory for render window title.\n");
                exit(EXIT_FAILURE);
            }
            memset(renderWindowTitle, 0, MAX_WINDOW_TITLE_LENGTH);
            snprintf(renderWindowTitle, MAX_WINDOW_TITLE_LENGTH, "%s %ux%u %u/%u", captureParams->deviceName, captureParams->resolutionWidth, captureParams->resolutionHeight, captureParams->timebaseNumerator, captureParams->timebaseDenominator);
            params[paramsCount]      = captureParams;
            paramsTypes[paramsCount] = PARAM_TYPE_CAPTURE;
            paramsCount++;
        } else if (strcmp(argv[argn], "receive") == 0) {
            if (argc < argn + 9) {
                fprintf(stderr, "Not enough arguments.\n");
                exit(EXIT_FAILURE);
            }
            ReceiveParams* receiveParams = malloc(sizeof(ReceiveParams));
            if (receiveParams == NULL) {
                fprintf(stderr, "Unable to allocate memory for receive params.\n");
                exit(EXIT_FAILURE);
            }
            memset(receiveParams, 0, sizeof(ReceiveParams));
            receiveParams->localIPAddress = argv[argn + 1];
            receiveParams->localPort      = atoi(argv[argn + 2]);
            receiveParams->localAddress   = malloc(sizeof(struct sockaddr_in));
            if (receiveParams->localAddress == NULL) {
                fprintf(stderr, "Unable to allocate memory for receive local address.\n");
                exit(EXIT_FAILURE);
            }
            memset(receiveParams->localAddress, 0, sizeof(struct sockaddr_in));
            receiveParams->localAddress->sin_family      = AF_INET;
            receiveParams->localAddress->sin_addr.s_addr = inet_addr(receiveParams->localIPAddress);
            receiveParams->localAddress->sin_port        = htons(receiveParams->localPort);
            receiveParams->maxPacketLength               = atoi(argv[argn + 3]);
            receiveParams->maxJPEGLength                 = atoi(argv[argn + 4]);
            receiveParams->resolutionWidth               = atoi(argv[argn + 5]);
            sourceWidth                                  = receiveParams->resolutionWidth;
            receiveParams->resolutionHeight              = atoi(argv[argn + 6]);
            sourceHeight                                 = receiveParams->resolutionHeight;
            receiveParams->timebaseNumerator             = atoi(argv[argn + 7]);
            sourceTimebaseNumerator                      = receiveParams->timebaseNumerator;
            receiveParams->timebaseDenominator           = atoi(argv[argn + 8]);
            sourceTimebaseDenominator                    = receiveParams->timebaseDenominator;
            receiveParams->jpegBuffer                    = malloc(receiveParams->maxJPEGLength);
            if (receiveParams->jpegBuffer == NULL) {
                fprintf(stderr, "Unable to allocate memory for receive jpeg buffer.\n");
                exit(EXIT_FAILURE);
            }
            memset(receiveParams->jpegBuffer, 0, receiveParams->maxJPEGLength);
            receiveParams->jpegBufferLength = 0;
            renderWindowTitle               = malloc(MAX_WINDOW_TITLE_LENGTH);
            if (renderWindowTitle == NULL) {
                fprintf(stderr, "Unable to allocate memory for render window title.\n");
                exit(EXIT_FAILURE);
            }
            memset(renderWindowTitle, 0, MAX_WINDOW_TITLE_LENGTH);
            snprintf(renderWindowTitle, MAX_WINDOW_TITLE_LENGTH, "%s:%u %ux%u %u/%u", receiveParams->localIPAddress, receiveParams->localPort, receiveParams->resolutionWidth, receiveParams->resolutionHeight, receiveParams->timebaseNumerator, receiveParams->timebaseDenominator);
            argn += 9;
            receiveParams->videoUDPReceiver = VideoUDPReceiverCreate(receiveParams->maxPacketLength, receiveParams->maxJPEGLength, receiveParams->localAddress);
            params[paramsCount]             = receiveParams;
            paramsTypes[paramsCount]        = PARAM_TYPE_RECEIVE;
            paramsCount++;
        } else if (strcmp(argv[argn], "render") == 0) {
            if (argc < argn + 3) {
                fprintf(stderr, "Not enough arguments.\n");
                exit(EXIT_FAILURE);
            }
            RenderParams* renderParams = malloc(sizeof(RenderParams));
            if (renderParams == NULL) {
                fprintf(stderr, "Unable to allocate memory for render params.\n");
                exit(EXIT_FAILURE);
            }
            memset(renderParams, 0, sizeof(RenderParams));
            renderParams->windowWidth  = atoi(argv[argn + 1]);
            renderParams->windowHeight = atoi(argv[argn + 2]);
            argn += 3;
            renderParams->videoRenderer = VideoRendererCreate(sourceWidth, sourceHeight, renderParams->windowWidth, renderParams->windowHeight, renderWindowTitle);
            params[paramsCount]         = renderParams;
            paramsTypes[paramsCount]    = PARAM_TYPE_RENDER;
            paramsCount++;
        } else if (strcmp(argv[argn], "record") == 0) {
            if (argc < argn + 2) {
                fprintf(stderr, "Not enough arguments.\n");
                exit(EXIT_FAILURE);
            }
            RecordParams* recordParams = malloc(sizeof(RecordParams));
            if (recordParams == NULL) {
                fprintf(stderr, "Unable to allocate memory for record params.\n");
                exit(EXIT_FAILURE);
            }
            memset(recordParams, 0, sizeof(RecordParams));
            recordParams->fileName = argv[argn + 1];
            argn += 2;
            recordParams->videoRecorder = VideoRecorderCreate(recordParams->fileName, sourceWidth, sourceHeight, sourceTimebaseNumerator, sourceTimebaseDenominator);
            params[paramsCount]         = recordParams;
            paramsTypes[paramsCount]    = PARAM_TYPE_RECORD;
            paramsCount++;
        } else if (strcmp(argv[argn], "send") == 0) {
            if (argc < argn + 8) {
                fprintf(stderr, "Not enough arguments.\n");
                exit(EXIT_FAILURE);
            }
            SendParams* sendParams = malloc(sizeof(SendParams));
            if (sendParams == NULL) {
                fprintf(stderr, "Unable to allocate memory for send params.\n");
                exit(EXIT_FAILURE);
            }
            memset(sendParams, 0, sizeof(SendParams));
            sendParams->localIPAddress = argv[argn + 1];
            sendParams->localPort      = atoi(argv[argn + 2]);
            sendParams->localAddress   = malloc(sizeof(struct sockaddr_in));
            if (sendParams->localAddress == NULL) {
                fprintf(stderr, "Unable to allocate memory for send local address.\n");
                exit(EXIT_FAILURE);
            }
            memset(sendParams->localAddress, 0, sizeof(struct sockaddr_in));
            sendParams->localAddress->sin_family      = AF_INET;
            sendParams->localAddress->sin_addr.s_addr = inet_addr(sendParams->localIPAddress);
            sendParams->localAddress->sin_port        = htons(sendParams->localPort);
            sendParams->remoteIPAddress               = argv[argn + 3];
            sendParams->remotePort                    = atoi(argv[argn + 4]);
            sendParams->remoteAddress                 = malloc(sizeof(struct sockaddr_in));
            if (sendParams->remoteAddress == NULL) {
                fprintf(stderr, "Unable to allocate memory for send remote address.\n");
                exit(EXIT_FAILURE);
            }
            memset(sendParams->remoteAddress, 0, sizeof(struct sockaddr_in));
            sendParams->remoteAddress->sin_family      = AF_INET;
            sendParams->remoteAddress->sin_addr.s_addr = inet_addr(sendParams->remoteIPAddress);
            sendParams->remoteAddress->sin_port        = htons(sendParams->remotePort);
            sendParams->maxPacketLength                = atoi(argv[argn + 5]);
            sendParams->maxJPEGLength                  = atoi(argv[argn + 6]);
            sendParams->sendRounds                     = atoi(argv[argn + 7]);
            argn += 8;
            sendParams->videoUDPSender = VideoUDPSenderCreate(sendParams->maxPacketLength, sendParams->maxJPEGLength, sendParams->localAddress, sendParams->remoteAddress);
            params[paramsCount]        = sendParams;
            paramsTypes[paramsCount]   = PARAM_TYPE_SEND;
            paramsCount++;
        } else if (strcmp(argv[argn], "pipe") == 0) {
            if (argc < argn + 3) {
                fprintf(stderr, "Not enough arguments.\n");
                exit(EXIT_FAILURE);
            }
            PipeParams* pipeParams = malloc(sizeof(PipeParams));
            if (pipeParams == NULL) {
                fprintf(stderr, "Unable to allocate memory for pipe params.\n");
                exit(EXIT_FAILURE);
            }
            memset(pipeParams, 0, sizeof(PipeParams));
            pipeParams->pipeFileDescriptor = atoi(argv[argn + 1]);
            pipeParams->rgbOrJPEG          = argv[argn + 2];
            pipeParams->rgb                = strcmp(pipeParams->rgbOrJPEG, "rgb") == 0;
            argn += 3;
            pipeParams->videoPipe    = VideoPipeCreate(pipeParams->pipeFileDescriptor, pipeParams->maxPacketLength);
            params[paramsCount]      = pipeParams;
            paramsTypes[paramsCount] = PARAM_TYPE_PIPE;
            paramsCount++;
        } else {
            fprintf(stderr, "Unexpected argument at %d: %s.\n", argn, argv[argn]);
            exit(EXIT_FAILURE);
        }
    }
}

static inline void validateParams() {
    if (paramsCount < 2) {
        fprintf(stderr, "Not enough params.\n");
        exit(EXIT_FAILURE);
    }
    if (paramsTypes[0] != PARAM_TYPE_CAPTURE && paramsTypes[0] != PARAM_TYPE_RECEIVE) {
        fprintf(stderr, "First param must be capture or receive.\n");
        exit(EXIT_FAILURE);
    }
    for (unsigned int paramIndex = 1; paramIndex < paramsCount; paramIndex++) {
        if (paramsTypes[paramIndex] == PARAM_TYPE_CAPTURE || paramsTypes[paramIndex] == PARAM_TYPE_RECEIVE) {
            fprintf(stderr, "Capture or receive must be the first param only.\n");
            exit(EXIT_FAILURE);
        }
    }
    unsigned int renderCount = 0;
    for (unsigned int paramIndex = 0; paramIndex < paramsCount; paramIndex++) {
        if (paramsTypes[paramIndex] == PARAM_TYPE_RENDER) {
            renderCount++;
        }
    }
    if (renderCount > 1) {
        fprintf(stderr, "There can be at most 1 render param.\n");
        exit(EXIT_FAILURE);
    }
}

static inline void createVideoDecoderIfRequired() {
    bool found = false;
    for (unsigned int paramIndex = 0; paramIndex < paramsCount; paramIndex++) {
        if (paramsTypes[paramIndex] == PARAM_TYPE_RENDER || (paramsTypes[paramIndex] == PARAM_TYPE_PIPE && ((PipeParams*)params[paramIndex])->rgb)) {
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }
    videoDecoder = VideoDecoderCreate(sourceWidth, sourceHeight);
}

static inline void mainLoop() {
    for (;;) {
        if (receivedSigint) {
            return;
        }
        for (unsigned int paramIndex = 0; paramIndex < paramsCount; paramIndex++) {
            bool frameDecoded = false;
            switch (paramsTypes[paramIndex]) {
                case PARAM_TYPE_CAPTURE: {
                    CaptureParams* captureParams = params[paramIndex];
                    jpegBufferLength             = VideoCaptureGetFrame(captureParams->videoCapture);
                    jpegBuffer                   = captureParams->videoCapture->leasedFrameBuffer;
                    uTimestamp                   = captureParams->videoCapture->leasedV4l2Buffer->timestamp.tv_sec * 1000000 + captureParams->videoCapture->leasedV4l2Buffer->timestamp.tv_usec;
                    break;
                }
                case PARAM_TYPE_RECEIVE: {
                    ReceiveParams* receiveParams = params[paramIndex];
                    if (!VideoUDPReceiverReceiveFrame(receiveParams->videoUDPReceiver)) {
                        return;
                    }
                    jpegBufferLength = receiveParams->videoUDPReceiver->jpegBufferLength;
                    jpegBuffer       = receiveParams->videoUDPReceiver->jpegBuffer;
                    uTimestamp       = receiveParams->videoUDPReceiver->uTimestamp;
                    break;
                }
                case PARAM_TYPE_RENDER: {
                    RenderParams* renderParams = params[paramIndex];
                    if (!frameDecoded) {
                        VideoDecoderDecodeFrame(videoDecoder, jpegBuffer, jpegBufferLength);
                        frameDecoded = true;
                    }
                    VideoRendererRender(renderParams->videoRenderer, videoDecoder->rgbBuffer);
                    break;
                }
                case PARAM_TYPE_RECORD: {
                    RecordParams* recordParams = params[paramIndex];
                    VideoRecorderRecordFrame(recordParams->videoRecorder, uTimestamp, jpegBuffer, jpegBufferLength);
                    break;
                }
                case PARAM_TYPE_SEND: {
                    SendParams* sendParams = params[paramIndex];
                    VideoUDPSenderSendFrame(sendParams->videoUDPSender, uTimestamp, jpegBuffer, jpegBufferLength, sendParams->sendRounds);
                    break;
                }
                case PARAM_TYPE_PIPE: {
                    PipeParams* pipeParams = params[paramIndex];
                    if (pipeParams->rgb && !frameDecoded) {
                        VideoDecoderDecodeFrame(videoDecoder, jpegBuffer, jpegBufferLength);
                        frameDecoded = true;
                    }
                    VideoPipeWriteFrame(pipeParams->videoPipe, uTimestamp, pipeParams->rgb ? videoDecoder->rgbBuffer : jpegBuffer, pipeParams->rgb ? videoDecoder->rgbBufferLength : jpegBufferLength);
                    break;
                }
            }
        }
        if (paramsTypes[0] == PARAM_TYPE_CAPTURE) {
            VideoCaptureReturnFrame(((CaptureParams*)params[0])->videoCapture);
        }
    }
}

static inline void freeAll() {
    for (unsigned int paramIndex = 0; paramIndex < paramsCount; paramIndex++) {
        switch (paramsTypes[paramIndex]) {
            case PARAM_TYPE_CAPTURE: {
                CaptureParams* captureParams = params[paramIndex];
                VideoCaptureFree(captureParams->videoCapture);
                free(captureParams);
                break;
            }
            case PARAM_TYPE_RECEIVE: {
                ReceiveParams* receiveParams = params[paramIndex];
                VideoUDPReceiverFree(receiveParams->videoUDPReceiver);
                free(receiveParams->localAddress);
                free(receiveParams->jpegBuffer);
                free(receiveParams);
                break;
            }
            case PARAM_TYPE_RENDER: {
                RenderParams* renderParams = params[paramIndex];
                VideoRendererFree(renderParams->videoRenderer);
                free(renderParams);
                break;
            }
            case PARAM_TYPE_RECORD: {
                RecordParams* recordParams = params[paramIndex];
                VideoRecorderFree(recordParams->videoRecorder);
                free(recordParams);
                break;
            }
            case PARAM_TYPE_SEND: {
                SendParams* sendParams = params[paramIndex];
                VideoUDPSenderFree(sendParams->videoUDPSender);
                free(sendParams->localAddress);
                free(sendParams->remoteAddress);
                free(sendParams);
                break;
            }
            case PARAM_TYPE_PIPE: {
                PipeParams* pipeParams = params[paramIndex];
                VideoPipeFree(pipeParams->videoPipe);
                free(pipeParams);
                break;
            }
        }
    }
    if (videoDecoder != NULL) {
        VideoDecoderFree(videoDecoder);
    }
    exit(EXIT_SUCCESS);
}

static inline void receiveSigint(int signal) {
    (void)signal;
    receivedSigint = true;
    if (paramsTypes[0] == PARAM_TYPE_RECEIVE) {
        ReceiveParams* receiveParams = params[0];
        close(receiveParams->videoUDPReceiver->fd);
        receiveParams->videoUDPReceiver->fd = -1;
    }
}

int main(int argc, char** argv) {
    if (argc == 1 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "h") == 0 || strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "?") == 0) {
        printUsage();
        return EXIT_SUCCESS;
    }
    if (strcmp(argv[1], "device") == 0 || strcmp(argv[1], "--device") == 0 || strcmp(argv[1], "devices") == 0 || strcmp(argv[1], "--devices") == 0 || strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "d") == 0) {
        printDevices();
        return EXIT_SUCCESS;
    }
    parseParams(argc, argv);
    validateParams();
    createVideoDecoderIfRequired();
    signal(SIGINT, receiveSigint);
    mainLoop();
    freeAll();
    return EXIT_SUCCESS;
}