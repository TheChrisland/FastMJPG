#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct VideoRecorder {
    AVFormatContext* avFormatContext;
    AVCodec*         avCodec;
    AVStream*        avStream;
    AVCodecContext*  avCodecContext;
    AVPacket*        avPacket;
    uint64_t         uTimestampZero;
} VideoRecorder;

VideoRecorder* VideoRecorderCreate(char* filename, unsigned int width, unsigned int height, unsigned int timeBaseNumerator, unsigned int timeBaseDenominator);
void           VideoRecorderRecordFrame(VideoRecorder* videoRecorder, uint64_t uTimestamp, void* start, unsigned int length);
void           VideoRecorderFree(VideoRecorder* videoRecorder);

#endif