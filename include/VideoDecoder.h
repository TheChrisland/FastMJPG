#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include <stdio.h>
#include <stdlib.h>
#include <turbojpeg.h>

typedef struct VideoDecoder {
    tjhandle     tjHandle;
    unsigned int width;
    unsigned int height;
    void*        rgbBuffer;
    unsigned int rgbBufferLength;

} VideoDecoder;

VideoDecoder* VideoDecoderCreate(unsigned int width, unsigned int height);
void          VideoDecoderDecodeFrame(VideoDecoder* videoDecoder, void* start, unsigned int length);
void          VideoDecoderFree(VideoDecoder* videoDecoder);

#endif