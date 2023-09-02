#include "../include/VideoUDPSender.h"
#include "../include/VideoUDPShared.h"
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

VideoUDPSender* VideoUDPSenderCreate(unsigned int maxPacketLength, unsigned int maxJPEGLength, struct sockaddr_in* localAddress, struct sockaddr_in* remoteAddress) {
    VideoUDPSender* videoUDPSender = malloc(sizeof(VideoUDPSender));
    if (videoUDPSender == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoUDPSender.\n");
        exit(EXIT_FAILURE);
    }
    videoUDPSender->maxPacketLength     = maxPacketLength;
    videoUDPSender->maxJPEGLength       = maxJPEGLength;
    videoUDPSender->maxPacketBodyLength = maxPacketLength - HEADER_LENGTH;
    videoUDPSender->maxPacketsPerJPEG   = (maxJPEGLength / videoUDPSender->maxPacketBodyLength) + 1;
    videoUDPSender->localAddress        = localAddress;
    videoUDPSender->remoteAddress       = remoteAddress;
    videoUDPSender->fd                  = -1;
    videoUDPSender->packet              = malloc(maxPacketLength);
    if (videoUDPSender->packet == NULL) {
        fprintf(stderr, "Unable to allocate memory for VideoUDPSender packet.\n");
        exit(EXIT_FAILURE);
    }
    videoUDPSender->fd = VideoUDPSharedCreateSocket(videoUDPSender->localAddress);
    return videoUDPSender;
}

void VideoUDPSenderSendFrame(VideoUDPSender* videoUDPSender, uint64_t uTimestamp, void* jpeg, unsigned int jpegLength, unsigned int sendRounds) {
    if (jpegLength == 0) {
        fprintf(stderr, "Payload length was zero.\n");
        exit(EXIT_FAILURE);
    }
    if (jpegLength > videoUDPSender->maxJPEGLength) {
        fprintf(stderr, "Payload length was greater than max jpeg length.\n");
        exit(EXIT_FAILURE);
    }
    uint64_t beUTimestamp  = htobe64(uTimestamp);
    uint32_t packetCount   = (jpegLength + videoUDPSender->maxPacketBodyLength - 1) / videoUDPSender->maxPacketBodyLength;
    uint32_t bePacketCount = htonl(packetCount);
    for (unsigned int sendRoundIndex = 0; sendRoundIndex < sendRounds; sendRoundIndex++) {
        for (uint32_t packetIndex = 0; packetIndex < packetCount; packetIndex++) {
            uint32_t bePacketIndex      = htonl(packetIndex);
            uint32_t packetBodyLength   = (packetIndex == packetCount - 1) ? jpegLength - packetIndex * videoUDPSender->maxPacketBodyLength : videoUDPSender->maxPacketBodyLength;
            uint32_t bePacketBodyLength = htonl(packetBodyLength);
            memcpy(videoUDPSender->packet + HEADER_UTIMESTAMP_OFFSET, &beUTimestamp, HEADER_UTIMESTAMP_SIZE);
            memcpy(videoUDPSender->packet + HEADER_PACKET_INDEX_OFFSET, &bePacketIndex, HEADER_PACKET_INDEX_SIZE);
            memcpy(videoUDPSender->packet + HEADER_PACKET_COUNT_OFFSET, &bePacketCount, HEADER_PACKET_COUNT_SIZE);
            memcpy(videoUDPSender->packet + HEADER_BODY_LENGTH_OFFSET, &bePacketBodyLength, HEADER_BODY_LENGTH_SIZE);
            memcpy(videoUDPSender->packet + PACKET_BODY_START_OFFSET, jpeg + packetIndex * videoUDPSender->maxPacketBodyLength, packetBodyLength);
            ssize_t bytesSent = sendto(videoUDPSender->fd, videoUDPSender->packet, HEADER_LENGTH + packetBodyLength, 0, (struct sockaddr*)videoUDPSender->remoteAddress, sizeof(struct sockaddr_in));
            if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                fprintf(stderr, "Socket was misconfigured non-blocking.\n");
                exit(EXIT_FAILURE);
            }
            if (bytesSent < 0 && errno == EINTR) {
                packetIndex--;
                continue;
            }
            if (bytesSent < 0) {
                perror("Socket error.");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void VideoUDPSenderFree(VideoUDPSender* videoUDPSender) {
    if (videoUDPSender != NULL) {
        if (videoUDPSender->fd >= 0) {
            close(videoUDPSender->fd);
            videoUDPSender->fd = -1;
        }
        free(videoUDPSender->packet);
        free(videoUDPSender);
    }
}