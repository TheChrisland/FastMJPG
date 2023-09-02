#ifndef VIDEOUDPSENDER_H
#define VIDEOUDPSENDER_H

#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct VideoUDPSender {
    unsigned int        maxPacketLength;
    unsigned int        maxJPEGLength;
    unsigned int        maxPacketBodyLength;
    unsigned int        maxPacketsPerJPEG;
    struct sockaddr_in* localAddress;
    struct sockaddr_in* remoteAddress;
    int                 fd;
    void*               packet;
} VideoUDPSender;

VideoUDPSender* VideoUDPSenderCreate(unsigned int maxPacketLength, unsigned int maxJPEGLength, struct sockaddr_in* localAddress, struct sockaddr_in* remoteAddress);
void            VideoUDPSenderSendFrame(VideoUDPSender* videoUDPSender, uint64_t uTimestamp, void* jpeg, unsigned int jpegLength, unsigned int sendRounds);
void            VideoUDPSenderFree(VideoUDPSender* videoUDPSender);

#endif