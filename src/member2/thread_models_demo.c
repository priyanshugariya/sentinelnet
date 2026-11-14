#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/syscall.h>

/* Thread-Local Storage (TLS) for lock-free per-worker packet statistics */
static __thread int tls_worker_id = 0;
static __thread long tls_packets_processed = 0;

static atomic_int cooperative_stop = 0;
static long collected_counts[3] = {0};
static pid_t kernel_tids[3] = {0};
static volatile sig_atomic_t usr1_caught_by_handler = 0;

static void *signal_handler_thread(void *arg) {
    sigset_t *set = (sigset_t *)arg;
    int sig = 0;
    if (sigwait(set, &sig) == 0 && sig == SIGUSR1) {
        usr1_caught_by_handler = 1;
    }
    return NULL;
}

static void *packet_worker(void *arg) {
    int id = *(int *)arg;
    tls_worker_id = id;
    kernel_tids[id] = (pid_t)syscall(SYS_gettid);

    while (!atomic_load(&cooperative_stop)) {
        tls_packets_processed += 150;
        if (tls_packets_processed >= 600) break;
        usleep(1000);
    }
    collected_counts[tls_worker_id] = tls_packets_processed;
    return NULL;
}

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    /* Mask SIGUSR1 in main and all worker threads; route exclusively to dedicated signal thread */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_t sig_thr;
    pthread_create(&sig_thr, NULL, signal_handler_thread, &set);

    pthread_t workers[3];
    int ids[3] = {0, 1, 2};
    for (int i = 0; i < 3; i++) {
        pthread_create(&workers[i], NULL, packet_worker, &ids[i]);
    }

    /* Send SIGUSR1 to process -> caught cleanly by dedicated signal thread */
    kill(getpid(), SIGUSR1);
    pthread_join(sig_thr, NULL);

    for (int i = 0; i < 3; i++) {
        pthread_join(workers[i], NULL);
    }
    atomic_store(&cooperative_stop, 1);

    if (json_mode) {
        printf("{\n"
               "  \"threading_model\": \"1:1 Kernel-Level Threads (NPTL / Linux)\",\n"
               "  \"process_pid\": %d,\n"
               "  \"workers\": [\n"
               "    {\"worker\": 0, \"kernel_tid\": %d, \"tls_packets\": %ld},\n"
               "    {\"worker\": 1, \"kernel_tid\": %d, \"tls_packets\": %ld},\n"
               "    {\"worker\": 2, \"kernel_tid\": %d, \"tls_packets\": %ld}\n"
               "  ],\n"
               "  \"dedicated_sigusr1_caught\": %s,\n"
               "  \"shutdown_mechanism\": \"Cooperative Atomic Stop Flag (Safe Cleanup)\"\n"
               "}\n",
               getpid(),
               kernel_tids[0], collected_counts[0],
               kernel_tids[1], collected_counts[1],
               kernel_tids[2], collected_counts[2],
               usr1_caught_by_handler ? "true" : "false");
    } else {
        printf("=== OS Unit 2: Multithreading Models & Issues (Anjali) ===\n");
        printf("Model: 1:1 Kernel Threads | Process PID: %d\n", getpid());
        for (int i = 0; i < 3; i++) {
            printf("  Worker %d -> Kernel TID: %d | TLS Packets Parsed: %ld\n",
                   i, kernel_tids[i], collected_counts[i]);
        }
        printf("Per-Thread Signal Routing (SIGUSR1 via sigwait): %s\n",
               usr1_caught_by_handler ? "CAUGHT SAFELY" : "MISSED");
    }
    return 0;
}
