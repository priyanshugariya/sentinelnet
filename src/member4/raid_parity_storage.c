#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int abs_diff(int a, int b) { return a > b ? a - b : b - a; }

static int disk_fcfs(const int req[], int n, int head) {
    int seek = 0, cur = head;
    for (int i = 0; i < n; i++) { seek += abs_diff(cur, req[i]); cur = req[i]; }
    return seek;
}

static int disk_sstf(const int req[], int n, int head) {
    int visited[16] = {0}, seek = 0, cur = head;
    for (int count = 0; count < n; count++) {
        int best = -1, best_d = 100000;
        for (int i = 0; i < n; i++) {
            if (!visited[i] && abs_diff(cur, req[i]) < best_d) {
                best_d = abs_diff(cur, req[i]);
                best = i;
            }
        }
        visited[best] = 1;
        seek += best_d;
        cur = req[best];
    }
    return seek;
}

static int disk_cscan(const int req[], int n, int head, int max_cyl) {
    int sorted[16];
    memcpy(sorted, req, n * sizeof(int));
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[i] > sorted[j]) { int t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }

    int seek = 0, cur = head;
    for (int i = 0; i < n; i++) {
        if (sorted[i] >= head) { seek += abs_diff(cur, sorted[i]); cur = sorted[i]; }
    }
    seek += abs_diff(cur, max_cyl - 1);
    seek += (max_cyl - 1);
    cur = 0;
    for (int i = 0; i < n; i++) {
        if (sorted[i] < head) { seek += abs_diff(cur, sorted[i]); cur = sorted[i]; }
    }
    return seek;
}

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    int requests[] = {98, 183, 37, 122, 14, 124, 65, 67};
    int n = 8, head = 53, max_cyl = 200;

    int fcfs_seek  = disk_fcfs(requests, n, head);
    int sstf_seek  = disk_sstf(requests, n, head);
    int cscan_seek = disk_cscan(requests, n, head, max_cyl);

    /* RAID-5 3-Disk XOR Striping & Fault Reconstruction */
    unsigned int disk0 = 0x53454E54; /* "SENT" */
    unsigned int disk1 = 0x494E454C; /* "INEL" */
    unsigned int parity_disk2 = disk0 ^ disk1;

    /* Simulate Disk 1 Total Failure (disk1 = 0x00000000) */
    unsigned int corrupted_disk1 = 0x00000000;
    (void)corrupted_disk1;
    unsigned int reconstructed_disk1 = disk0 ^ parity_disk2;
    int raid_intact = (reconstructed_disk1 == disk1);

    if (json_mode) {
        printf("{\n"
               "  \"disk_scheduling\": {\n"
               "    \"initial_head\": %d,\n"
               "    \"fcfs_seek_cylinders\": %d,\n"
               "    \"sstf_seek_cylinders\": %d,\n"
               "    \"cscan_seek_cylinders\": %d\n"
               "  },\n"
               "  \"raid5_parity_recovery\": {\n"
               "    \"disk0_hex\": \"0x%08X\",\n"
               "    \"disk1_original_hex\": \"0x%08X\",\n"
               "    \"parity_disk2_hex\": \"0x%08X\",\n"
               "    \"failed_drive\": \"Disk_1\",\n"
               "    \"reconstructed_disk1_hex\": \"0x%08X\",\n"
               "    \"parity_verified\": %s\n"
               "  }\n"
               "}\n",
               head, fcfs_seek, sstf_seek, cscan_seek,
               disk0, disk1, parity_disk2, reconstructed_disk1,
               raid_intact ? "true" : "false");
    } else {
        printf("=== OS Unit 5: Disk Head Scheduling & RAID-5 XOR Recovery (Drishti) ===\n");
        printf("Disk Head Seek (Start=%d) -> FCFS: %d cyl | SSTF: %d cyl | C-SCAN: %d cyl\n",
               head, fcfs_seek, sstf_seek, cscan_seek);
        printf("RAID-5 XOR Parity -> Disk0=0x%08X, Disk1=0x%08X, Parity=0x%08X\n",
               disk0, disk1, parity_disk2);
        printf("Disk 1 Failure Injected -> Reconstructed via (Disk0 ^ Parity) = 0x%08X [%s]\n",
               reconstructed_disk1, raid_intact ? "100% RECOVERED" : "ERROR");
    }
    return 0;
}
