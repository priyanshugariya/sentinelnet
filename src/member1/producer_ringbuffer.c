#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include "sentinel_packet.h"

RingBuffer ring;
pthread_mutex_t lock;
sem_t sem_empty;
sem_t sem_full;
atomic_uint total_produced = 0;

const char *sample_ips[] = {
    "192.168.1.105", "10.0.0.15", "172.16.0.4", "45.33.32.156", "192.168.1.200"
};

void init_ring_buffer(void) {
    ring.in = 0;
    ring.out = 0;
    ring.count = 0;
    pthread_mutex_init(&lock, NULL);
    sem_init(&sem_empty, 0, RING_BUFFER_CAPACITY);
    sem_init(&sem_full, 0, 0);
}

void* producer_thread(void* arg) {
    (void)arg;
    uint32_t seq = 1;
    while (seq <= 500) {
        sem_wait(&sem_empty);
        pthread_mutex_lock(&lock);

        PacketRecord pkt;
        pkt.packet_id = seq++;
        strncpy(pkt.src_ip, sample_ips[rand() % 5], sizeof(pkt.src_ip));
        strncpy(pkt.dst_ip, "192.168.1.1", sizeof(pkt.dst_ip));
        pkt.src_port = 1024 + (rand() % 50000);
        pkt.dst_port = 80;
        strcpy(pkt.protocol, (rand() % 2 == 0) ? "TCP" : "UDP");
        pkt.packet_size = 64 + (rand() % 1400);

        if (strcmp(pkt.src_ip, "45.33.32.156") == 0 && (rand() % 3 == 0)) {
            pkt.is_malformed = 1;
        } else {
            pkt.is_malformed = 0;
        }

        time_t now = time(NULL);
        strftime(pkt.timestamp, sizeof(pkt.timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

        ring.buffer[ring.in] = pkt;
        ring.in = (ring.in + 1) % RING_BUFFER_CAPACITY;
        ring.count++;
        atomic_fetch_add(&total_produced, 1);

        pthread_mutex_unlock(&lock);
        sem_post(&sem_full);
        usleep(30000);
    }
    return NULL;
}

int main(void) {
    srand(time(NULL));
    init_ring_buffer();
    printf("[Member 1: OS Unit 2] Producer Ring Buffer initialized (Capacity: %d)\n", RING_BUFFER_CAPACITY);

    pthread_t th;
    pthread_create(&th, NULL, producer_thread, NULL);

    for (int i = 0; i < 5; i++) {
        pthread_mutex_lock(&lock);
        printf("[Ring Buffer State] Fill: %d/%d slots | Total Produced: %u\n",
               ring.count, RING_BUFFER_CAPACITY, atomic_load(&total_produced));
        pthread_mutex_unlock(&lock);
        sleep(1);
    }

    pthread_cancel(th);
    pthread_join(th, NULL);
    sem_destroy(&sem_empty);
    sem_destroy(&sem_full);
    pthread_mutex_destroy(&lock);
    return 0;
}
