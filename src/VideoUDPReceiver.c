#include "../include/VideoUDPReceiver.h"
#include "../include/VideoUDPShared.h"
#include <endian.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

VideoUDPReceiver* VideoUDPReceiverCreate(unsigned int maxPacketLength, unsigned int maxJPEGLength, struct sockaddr_in* localAddress) {
    VideoUDPReceiver* videoUDPReceiver = malloc(sizeof(VideoUDPReceiver));
    if (videoUDPReceiver == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoUDPReceiver.\n");
        exit(EXIT_FAILURE);
    }
    videoUDPReceiver->maxPacketLength     = maxPacketLength;
    videoUDPReceiver->maxJPEGLength       = maxJPEGLength;
    videoUDPReceiver->maxPacketBodyLength = maxPacketLength - HEADER_LENGTH;
    videoUDPReceiver->maxPacketsPerJPEG   = (maxJPEGLength / videoUDPReceiver->maxPacketBodyLength) + 1;
    videoUDPReceiver->localAddress        = localAddress;
    videoUDPReceiver->fd                  = -1;
    videoUDPReceiver->flags               = malloc(videoUDPReceiver->maxPacketsPerJPEG * sizeof(bool));
    if (videoUDPReceiver->flags == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoUDPReceiver flags.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPReceiver->flags, 0, videoUDPReceiver->maxPacketsPerJPEG * sizeof(bool));
    videoUDPReceiver->packet = malloc(maxPacketLength);
    if (videoUDPReceiver->packet == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoUDPReceiver packet buffer.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPReceiver->packet, 0, maxPacketLength);
    videoUDPReceiver->jpegBuffer = malloc(maxJPEGLength);
    if (videoUDPReceiver->jpegBuffer == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoUDPReceiver jpeg buffer.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoUDPReceiver->jpegBuffer, 0, maxJPEGLength);
    videoUDPReceiver->fd = VideoUDPSharedCreateSocket(videoUDPReceiver->localAddress);
    return videoUDPReceiver;
}

bool VideoUDPReceiverReceiveFrame(VideoUDPReceiver* videoUDPReceiver) {
    uint64_t trackedUTimestamp            = 0;
    bool     trackedUTimestampInitialized = false;
    uint32_t packetsFlagged               = 0;
    for (;;) {
        ssize_t bytesReceived = recvfrom(videoUDPReceiver->fd, videoUDPReceiver->packet, videoUDPReceiver->maxPacketLength, 0, NULL, NULL);
        if (bytesReceived < 0 && errno == EINTR) {
            continue;
        }
        if (bytesReceived < 0 && errno == EBADF) {
            return false;
        }
        if (bytesReceived < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            fprintf(stderr, "Socket was misconfigured non-blocking.\n");
            exit(EXIT_FAILURE);
        }
        if (bytesReceived < 0) {
            perror("Socket error.");
            exit(EXIT_FAILURE);
        }
        if (bytesReceived == 0) {
            fprintf(stderr, "Received 0 length packet.\n");
            exit(EXIT_FAILURE);
        }
        if (bytesReceived < HEADER_LENGTH) {
            fprintf(stderr, "Socket partial receive.\n");
            exit(EXIT_FAILURE);
        }
        uint64_t beUTimestamp;
        uint32_t bePacketIndex;
        uint32_t bePacketCount;
        uint32_t bePacketBodyLength;
        memcpy(&beUTimestamp, videoUDPReceiver->packet + HEADER_UTIMESTAMP_OFFSET, HEADER_UTIMESTAMP_SIZE);
        memcpy(&bePacketIndex, videoUDPReceiver->packet + HEADER_PACKET_INDEX_OFFSET, HEADER_PACKET_INDEX_SIZE);
        memcpy(&bePacketCount, videoUDPReceiver->packet + HEADER_PACKET_COUNT_OFFSET, HEADER_PACKET_COUNT_SIZE);
        memcpy(&bePacketBodyLength, videoUDPReceiver->packet + HEADER_BODY_LENGTH_OFFSET, HEADER_BODY_LENGTH_SIZE);
        uint64_t uTimestamp       = be64toh(beUTimestamp);
        uint32_t packetIndex      = ntohl(bePacketIndex);
        uint32_t packetCount      = ntohl(bePacketCount);
        uint32_t packetBodyLength = ntohl(bePacketBodyLength);
        if (HEADER_LENGTH + packetBodyLength != bytesReceived) {
            fprintf(stderr, "Packet length mismatch\n");
            exit(EXIT_FAILURE);
        }
        memcpy(videoUDPReceiver->jpegBuffer + (packetIndex * videoUDPReceiver->maxPacketBodyLength), videoUDPReceiver->packet + PACKET_BODY_START_OFFSET, packetBodyLength);
        if (!trackedUTimestampInitialized || trackedUTimestamp != uTimestamp) {
            trackedUTimestamp            = uTimestamp;
            trackedUTimestampInitialized = true;
            packetsFlagged               = 0;
            memset(videoUDPReceiver->flags, 0, videoUDPReceiver->maxPacketsPerJPEG * sizeof(bool));
        }
        if (videoUDPReceiver->flags[packetIndex]) {
            continue;
        }
        if (packetIndex == packetCount - 1) {
            videoUDPReceiver->jpegBufferLength = (packetCount - 1) * videoUDPReceiver->maxPacketBodyLength + packetBodyLength;
            videoUDPReceiver->uTimestamp       = trackedUTimestamp;
        }
        videoUDPReceiver->flags[packetIndex] = true;
        packetsFlagged++;
        if (packetsFlagged == packetCount) {
            return true;
        }
    }
}

void VideoUDPReceiverFree(VideoUDPReceiver* videoUDPReceiver) {
    if (videoUDPReceiver != NULL) {
        if (videoUDPReceiver->fd >= 0) {
            close(videoUDPReceiver->fd);
            videoUDPReceiver->fd = -1;
        }
        free(videoUDPReceiver->flags);
        free(videoUDPReceiver->packet);
        free(videoUDPReceiver->jpegBuffer);
        free(videoUDPReceiver);
    }
}