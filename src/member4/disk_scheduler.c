#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

#define DISK_CYLINDERS 200

typedef struct {
    int total_seek_distance;
    double avg_seek_time_ms;
} DiskSeekMetrics;

static int compare_ints(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

DiskSeekMetrics simulate_fcfs(const int *reqs, int n, int initial_head) {
    int total_seek = 0;
    int curr = initial_head;
    for (int i = 0; i < n; i++) {
        total_seek += abs(curr - reqs[i]);
        curr = reqs[i];
    }
    DiskSeekMetrics m;
    m.total_seek_distance = total_seek;
    m.avg_seek_time_ms = ((double)total_seek / n) * 0.1;
    return m;
}

DiskSeekMetrics simulate_sstf(const int *reqs, int n, int initial_head) {
    int total_seek = 0;
    int curr = initial_head;
    bool visited[128] = {false};

    for (int step = 0; step < n; step++) {
        int best_idx = -1;
        int min_dist = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!visited[i]) {
                int dist = abs(curr - reqs[i]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_idx = i;
                }
            }
        }
        visited[best_idx] = true;
        total_seek += min_dist;
        curr = reqs[best_idx];
    }
    DiskSeekMetrics m;
    m.total_seek_distance = total_seek;
    m.avg_seek_time_ms = ((double)total_seek / n) * 0.1;
    return m;
}

DiskSeekMetrics simulate_scan(const int *reqs, int n, int initial_head, int disk_size) {
    int total_seek = 0;
    int sorted[128];
    for (int i = 0; i < n; i++) sorted[i] = reqs[i];
    qsort(sorted, n, sizeof(int), compare_ints);

    int idx = 0;
    while (idx < n && sorted[idx] < initial_head) idx++;

    int curr = initial_head;
    // Sweep toward high end
    for (int i = idx; i < n; i++) {
        total_seek += abs(curr - sorted[i]);
        curr = sorted[i];
    }
    // Reach high cylinder end boundary
    total_seek += abs(curr - (disk_size - 1));
    curr = disk_size - 1;

    // Sweep back low
    for (int i = idx - 1; i >= 0; i--) {
        total_seek += abs(curr - sorted[i]);
        curr = sorted[i];
    }

    DiskSeekMetrics m;
    m.total_seek_distance = total_seek;
    m.avg_seek_time_ms = ((double)total_seek / n) * 0.1;
    return m;
}

DiskSeekMetrics simulate_cscan(const int *reqs, int n, int initial_head, int disk_size) {
    int total_seek = 0;
    int sorted[128];
    for (int i = 0; i < n; i++) sorted[i] = reqs[i];
    qsort(sorted, n, sizeof(int), compare_ints);

    int idx = 0;
    while (idx < n && sorted[idx] < initial_head) idx++;

    int curr = initial_head;
    for (int i = idx; i < n; i++) {
        total_seek += abs(curr - sorted[i]);
        curr = sorted[i];
    }
    total_seek += abs(curr - (disk_size - 1));
    total_seek += (disk_size - 1); // Wrap to cylinder 0
    curr = 0;

    for (int i = 0; i < idx; i++) {
        total_seek += abs(curr - sorted[i]);
        curr = sorted[i];
    }

    DiskSeekMetrics m;
    m.total_seek_distance = total_seek;
    m.avg_seek_time_ms = ((double)total_seek / n) * 0.1;
    return m;
}

DiskSeekMetrics simulate_look(const int *reqs, int n, int initial_head) {
    int total_seek = 0;
    int sorted[128];
    for (int i = 0; i < n; i++) sorted[i] = reqs[i];
    qsort(sorted, n, sizeof(int), compare_ints);

    int idx = 0;
    while (idx < n && sorted[idx] < initial_head) idx++;

    int curr = initial_head;
    for (int i = idx; i < n; i++) {
        total_seek += abs(curr - sorted[i]);
        curr = sorted[i];
    }
    for (int i = idx - 1; i >= 0; i--) {
        total_seek += abs(curr - sorted[i]);
        curr = sorted[i];
    }

    DiskSeekMetrics m;
    m.total_seek_distance = total_seek;
    m.avg_seek_time_ms = ((double)total_seek / n) * 0.1;
    return m;
}

int main(void) {
    int disk_requests[] = {98, 183, 37, 122, 14, 124, 65, 67};
    int n = sizeof(disk_requests) / sizeof(disk_requests[0]);
    int initial_head = 53;

    printf("========================================================================\n");
    printf(" SENTINELNET SECONDARY STORAGE DISK SCHEDULER (MEMBER 4: OS UNIT 5)\n");
    printf(" Disk Size: %d Cylinders (0-%d) | Initial Head Position: Track %d\n",
           DISK_CYLINDERS, DISK_CYLINDERS - 1, initial_head);
    printf(" Workload  : 98, 183, 37, 122, 14, 124, 65, 67\n");
    printf("========================================================================\n");

    DiskSeekMetrics fcfs  = simulate_fcfs(disk_requests, n, initial_head);
    DiskSeekMetrics sstf  = simulate_sstf(disk_requests, n, initial_head);
    DiskSeekMetrics scan  = simulate_scan(disk_requests, n, initial_head, DISK_CYLINDERS);
    DiskSeekMetrics cscan = simulate_cscan(disk_requests, n, initial_head, DISK_CYLINDERS);
    DiskSeekMetrics look  = simulate_look(disk_requests, n, initial_head);

    printf("\n%-18s | %-20s | %-20s\n", "Policy", "Total Head Movement", "Avg Seek Time (Est)");
    printf("-------------------+----------------------+---------------------\n");
    printf("%-18s | %10d cylinders | %14.2f ms\n", "FCFS", fcfs.total_seek_distance, fcfs.avg_seek_time_ms);
    printf("%-18s | %10d cylinders | %14.2f ms\n", "SSTF", sstf.total_seek_distance, sstf.avg_seek_time_ms);
    printf("%-18s | %10d cylinders | %14.2f ms\n", "SCAN (Elevator)", scan.total_seek_distance, scan.avg_seek_time_ms);
    printf("%-18s | %10d cylinders | %14.2f ms\n", "C-SCAN (Circular)", cscan.total_seek_distance, cscan.avg_seek_time_ms);
    printf("%-18s | %10d cylinders | %14.2f ms\n", "LOOK", look.total_seek_distance, look.avg_seek_time_ms);
    printf("-------------------+----------------------+---------------------\n");
    printf(" Optimization Win  : LOOK reduces head seek by %d cylinders vs FCFS!\n",
           fcfs.total_seek_distance - look.total_seek_distance);
    printf("========================================================================\n");

    return 0;
}
