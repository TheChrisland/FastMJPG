#include "../include/VideoRecorder.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

VideoRecorder* VideoRecorderCreate(char* filename, unsigned int width, unsigned int height, unsigned int timeBaseNumerator, unsigned int timeBaseDenominator) {
    VideoRecorder* videoRecorder = malloc(sizeof(VideoRecorder));
    if (videoRecorder == NULL) {
        fprintf(stderr, "Error allocating memory for VideoRecorder.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoRecorder, 0, sizeof(VideoRecorder));
    avformat_alloc_output_context2(&videoRecorder->avFormatContext, NULL, "matroska", NULL);
    if (!(videoRecorder->avFormatContext)) {
        fprintf(stderr, "Error creating output context.\n");
        exit(EXIT_FAILURE);
    }
    videoRecorder->avCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!(videoRecorder->avCodec)) {
        fprintf(stderr, "Error finding codec.\n");
        exit(EXIT_FAILURE);
    }
    videoRecorder->avStream = avformat_new_stream(videoRecorder->avFormatContext, videoRecorder->avCodec);
    if (!(videoRecorder->avStream)) {
        fprintf(stderr, "Error creating av stream.\n");
        exit(EXIT_FAILURE);
    }
    videoRecorder->avStream->codecpar->codec_id   = AV_CODEC_ID_MJPEG;
    videoRecorder->avStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    videoRecorder->avStream->codecpar->format     = AV_PIX_FMT_YUVJ420P;
    videoRecorder->avStream->codecpar->width      = width;
    videoRecorder->avStream->codecpar->height     = height;
    videoRecorder->avStream->time_base            = (AVRational) { timeBaseNumerator, timeBaseDenominator };
    videoRecorder->avCodecContext                 = avcodec_alloc_context3(videoRecorder->avCodec);
    if (!(videoRecorder->avCodecContext)) {
        fprintf(stderr, "Error allocating codec context.\n");
        exit(EXIT_FAILURE);
    }
    videoRecorder->avCodecContext->time_base = videoRecorder->avStream->time_base;
    videoRecorder->avPacket                  = av_packet_alloc();
    if (!videoRecorder->avPacket) {
        fprintf(stderr, "Error allocating packet\n");
        exit(EXIT_FAILURE);
    }
    avcodec_parameters_to_context(videoRecorder->avCodecContext, videoRecorder->avStream->codecpar);
    avcodec_open2(videoRecorder->avCodecContext, videoRecorder->avCodec, NULL);
    if (!(videoRecorder->avFormatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&videoRecorder->avFormatContext->pb, filename, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Error opening output file.\n");
            exit(EXIT_FAILURE);
        }
    }
    if (avformat_write_header(videoRecorder->avFormatContext, NULL) < 0) {
        fprintf(stderr, "Error writing header.\n");
        exit(EXIT_FAILURE);
    }
    return videoRecorder;
}

void VideoRecorderRecordFrame(VideoRecorder* videoRecorder, uint64_t uTimestamp, void* start, unsigned int length) {
    if (videoRecorder->uTimestampZero == 0) {
        videoRecorder->uTimestampZero = uTimestamp;
    }
    videoRecorder->avPacket->data         = start;
    videoRecorder->avPacket->size         = length;
    videoRecorder->avPacket->stream_index = videoRecorder->avStream->index;
    videoRecorder->avPacket->pts          = av_rescale_q(uTimestamp - videoRecorder->uTimestampZero, (AVRational) { 1, 1000000 }, videoRecorder->avStream->time_base);
    videoRecorder->avPacket->dts          = videoRecorder->avPacket->pts;
    av_interleaved_write_frame(videoRecorder->avFormatContext, videoRecorder->avPacket);
    av_packet_unref(videoRecorder->avPacket);
}

void VideoRecorderFree(VideoRecorder* videoRecorder) {
    av_write_trailer(videoRecorder->avFormatContext);
    avcodec_close(videoRecorder->avCodecContext);
    avcodec_free_context(&videoRecorder->avCodecContext);
    av_packet_free(&videoRecorder->avPacket);
    avio_closep(&videoRecorder->avFormatContext->pb);
    avformat_free_context(videoRecorder->avFormatContext);
    free(videoRecorder);
}