#ifndef SENTINEL_PACKET_H
#define SENTINEL_PACKET_H

#include <stdint.h>
#include <time.h>

#define RING_BUFFER_CAPACITY 64
#define PIPE_PATH "/tmp/sentinel_pipe"

typedef struct {
    uint32_t packet_id;
    char src_ip[16];
    char dst_ip[16];
    uint16_t src_port;
    uint16_t dst_port;
    char protocol[8];
    uint32_t packet_size;
    uint8_t is_malformed;
    char timestamp[32];
} PacketRecord;

typedef struct {
    PacketRecord buffer[RING_BUFFER_CAPACITY];
    int in;
    int out;
    int count;
} RingBuffer;

#endif
