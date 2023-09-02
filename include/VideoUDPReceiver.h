#ifndef VIDEOUDPRECEIVER_H
#define VIDEOUDPRECEIVER_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct VideoUDPReceiver {
    unsigned int        maxPacketLength;
    unsigned int        maxJPEGLength;
    unsigned int        maxPacketBodyLength;
    unsigned int        maxPacketsPerJPEG;
    struct sockaddr_in* localAddress;
    int                 fd;
    bool*               flags;
    void*               packet;
    void*               jpegBuffer;
    unsigned int        jpegBufferLength;
    uint64_t            uTimestamp;
} VideoUDPReceiver;

VideoUDPReceiver* VideoUDPReceiverCreate(unsigned int maxPacketLength, unsigned int maxJPEGLength, struct sockaddr_in* localAddress);
bool              VideoUDPReceiverReceiveFrame(VideoUDPReceiver* videoUDPReceiver);
void              VideoUDPReceiverFree(VideoUDPReceiver* videoUDPReceiver);

#endif