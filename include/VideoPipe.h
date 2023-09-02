#ifndef VIDEOPIPE_H
#define VIDEOPIPE_H

#include <stdint.h>

typedef struct VideoPipe {
    int          fd;
    unsigned int maxPacketLength;
} VideoPipe;

VideoPipe* VideoPipeCreate(int fd, unsigned int maxPacketLength);
void       VideoPipeWriteFrame(VideoPipe* videoPipe, uint64_t uTimestamp, void* start, uint32_t length);
void       VideoPipeFree(VideoPipe* videoPipe);

#endif