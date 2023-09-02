#include "../include/VideoDecoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>

VideoDecoder* VideoDecoderCreate(unsigned int width, unsigned int height) {
    VideoDecoder* videoDecoder = malloc(sizeof(VideoDecoder));
    if (videoDecoder == NULL) {
        fprintf(stderr, "Unable to allocate memory for VideoDecoder.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoDecoder, 0, sizeof(VideoDecoder));
    videoDecoder->tjHandle = tjInitDecompress();
    if (!videoDecoder->tjHandle) {
        fprintf(stderr, "Failed to initialize TurboJPEG decompressor!\n");
        exit(EXIT_FAILURE);
    }
    videoDecoder->width           = width;
    videoDecoder->height          = height;
    videoDecoder->rgbBufferLength = width * height * 3;
    videoDecoder->rgbBuffer       = tjAlloc(videoDecoder->rgbBufferLength);
    if (videoDecoder->rgbBuffer == NULL) {
        fprintf(stderr, "Unable to allocate memory for VideoDecoder rgbBuffer.\n");
        exit(EXIT_FAILURE);
    }
    return videoDecoder;
}

void VideoDecoderDecodeFrame(VideoDecoder* videoDecoder, void* jpeg, unsigned int jpegLength) {
    if (tjDecompress2(videoDecoder->tjHandle, jpeg, jpegLength, videoDecoder->rgbBuffer, videoDecoder->width, 0, videoDecoder->height, TJPF_RGB, 0) < 0) {
        fprintf(stderr, "JPEG decompression error: %s\n", tjGetErrorStr());
        exit(EXIT_FAILURE);
    }
}

void VideoDecoderFree(VideoDecoder* videoDecoder) {
    tjFree(videoDecoder->rgbBuffer);
    tjDestroy(videoDecoder->tjHandle);
    free(videoDecoder);
}