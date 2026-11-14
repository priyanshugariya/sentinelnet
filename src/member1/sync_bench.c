#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>

#define ITERATIONS 200000

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static atomic_bool p_flag[2];
static atomic_int p_turn;
static long peterson_counter = 0;

static void *peterson_worker(void *arg) {
    int id = *(int *)arg;
    int other = 1 - id;
    for (int i = 0; i < ITERATIONS; i++) {
        atomic_store_explicit(&p_flag[id], true, memory_order_seq_cst);
        atomic_store_explicit(&p_turn, other, memory_order_seq_cst);
        while (atomic_load_explicit(&p_flag[other], memory_order_seq_cst) &&
               atomic_load_explicit(&p_turn, memory_order_seq_cst) == other) {
        }
        peterson_counter++;
        atomic_store_explicit(&p_flag[id], false, memory_order_seq_cst);
    }
    return NULL;
}

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t slots_sem;
static long mutex_counter = 0;

static void *mutex_sem_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        sem_wait(&slots_sem);
        pthread_mutex_lock(&mtx);
        mutex_counter++;
        pthread_mutex_unlock(&mtx);
        sem_post(&slots_sem);
    }
    return NULL;
}

static atomic_long lockfree_counter = 0;

static void *atomic_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        atomic_fetch_add_explicit(&lockfree_counter, 1, memory_order_relaxed);
    }
    return NULL;
}

static pthread_rwlock_t telemetry_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static int shared_threat_score = 42;
static int rw_reads = 0, rw_writes = 0;

static void *rw_reader(void *arg) {
    (void)arg;
    for (int i = 0; i < 50; i++) {
        pthread_rwlock_rdlock(&telemetry_rwlock);
        volatile int snap = shared_threat_score;
        (void)snap;
        rw_reads++;
        pthread_rwlock_unlock(&telemetry_rwlock);
    }
    return NULL;
}

static void *rw_writer(void *arg) {
    (void)arg;
    for (int i = 0; i < 10; i++) {
        pthread_rwlock_wrlock(&telemetry_rwlock);
        shared_threat_score += 5;
        rw_writes++;
        pthread_rwlock_unlock(&telemetry_rwlock);
    }
    return NULL;
}

static pthread_mutex_t forks[5];
static int meals_eaten[5] = {0};

static void *philosopher(void *arg) {
    int id = *(int *)arg;
    int left = id;
    int right = (id + 1) % 5;
    int first = left < right ? left : right;
    int second = left < right ? right : left;

    for (int m = 0; m < 20; m++) {
        pthread_mutex_lock(&forks[first]);
        pthread_mutex_lock(&forks[second]);
        meals_eaten[id]++;
        pthread_mutex_unlock(&forks[second]);
        pthread_mutex_unlock(&forks[first]);
    }
    return NULL;
}

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);
    pthread_t t1, t2;
    int id0 = 0, id1 = 1;

    atomic_init(&p_flag[0], false);
    atomic_init(&p_flag[1], false);
    double t0 = now_ms();
    pthread_create(&t1, NULL, peterson_worker, &id0);
    pthread_create(&t2, NULL, peterson_worker, &id1);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    double peterson_ms = now_ms() - t0;

    sem_init(&slots_sem, 0, 1);
    t0 = now_ms();
    pthread_create(&t1, NULL, mutex_sem_worker, NULL);
    pthread_create(&t2, NULL, mutex_sem_worker, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    double mutex_ms = now_ms() - t0;
    sem_destroy(&slots_sem);

    t0 = now_ms();
    pthread_create(&t1, NULL, atomic_worker, NULL);
    pthread_create(&t2, NULL, atomic_worker, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    double atomic_ms = now_ms() - t0;

    pthread_t r1, r2, w1;
    pthread_create(&r1, NULL, rw_reader, NULL);
    pthread_create(&r2, NULL, rw_reader, NULL);
    pthread_create(&w1, NULL, rw_writer, NULL);
    pthread_join(r1, NULL);
    pthread_join(r2, NULL);
    pthread_join(w1, NULL);

    pthread_t phils[5];
    int pids[5] = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++) pthread_mutex_init(&forks[i], NULL);
    for (int i = 0; i < 5; i++) pthread_create(&phils[i], NULL, philosopher, &pids[i]);
    for (int i = 0; i < 5; i++) pthread_join(phils[i], NULL);
    int total_meals = 0;
    for (int i = 0; i < 5; i++) total_meals += meals_eaten[i];

    if (json_mode) {
        printf("{\n"
               "  \"iterations_per_thread\": %d,\n"
               "  \"benchmarks\": [\n"
               "    {\"mechanism\": \"Peterson's Solution (Seq-Cst)\", \"time_ms\": %.2f, \"final_count\": %ld, \"expected\": %d},\n"
               "    {\"mechanism\": \"POSIX Mutex + Semaphore\", \"time_ms\": %.2f, \"final_count\": %ld, \"expected\": %d},\n"
               "    {\"mechanism\": \"C11 Lock-Free Atomics\", \"time_ms\": %.2f, \"final_count\": %ld, \"expected\": %d}\n"
               "  ],\n"
               "  \"classical_problems\": {\n"
               "    \"readers_writers\": {\"reads\": %d, \"writes\": %d, \"final_score\": %d},\n"
               "    \"dining_philosophers\": {\"philosophers\": 5, \"total_meals\": %d, \"deadlocks\": 0}\n"
               "  }\n"
               "}\n",
               ITERATIONS,
               peterson_ms, peterson_counter, ITERATIONS * 2,
               mutex_ms, mutex_counter, ITERATIONS * 2,
               atomic_ms, atomic_load(&lockfree_counter), ITERATIONS * 2,
               rw_reads, rw_writes, shared_threat_score, total_meals);
    } else {
        printf("=== OS Unit 2: Synchronization Benchmarks (2x %d ops) ===\n", ITERATIONS);
        printf("1. Peterson's Solution   : %7.2f ms (Count: %ld / %d)\n", peterson_ms, peterson_counter, ITERATIONS * 2);
        printf("2. Mutex + Semaphore     : %7.2f ms (Count: %ld / %d)\n", mutex_ms, mutex_counter, ITERATIONS * 2);
        printf("3. C11 Lock-Free Atomics : %7.2f ms (Count: %ld / %d)\n", atomic_ms, atomic_load(&lockfree_counter), ITERATIONS * 2);
        printf("Readers-Writers: %d reads, %d writes | Dining Philosophers: %d deadlock-free meals\n",
               rw_reads, rw_writes, total_meals);
    }
    return 0;
}
