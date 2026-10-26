#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define NUM_PROCESSES 5
#define NUM_RESOURCES 3

// System Resources: R0=IPC Pipes, R1=DB Locks, R2=Memory Buffers
typedef struct {
    int available[NUM_RESOURCES];
    int max[NUM_PROCESSES][NUM_RESOURCES];
    int allocation[NUM_PROCESSES][NUM_RESOURCES];
    int need[NUM_PROCESSES][NUM_RESOURCES];
} BankerSystem;

static void calculate_need_matrix(BankerSystem *s) {
    for (int i = 0; i < NUM_PROCESSES; i++) {
        for (int j = 0; j < NUM_RESOURCES; j++) {
            s->need[i][j] = s->max[i][j] - s->allocation[i][j];
        }
    }
}

static void print_system_state(const BankerSystem *s, const char *label) {
    printf("\n========================================================================\n");
    printf(" BANKER'S STATE MATRIX: %s\n", label);
    printf("========================================================================\n");
    printf(" Process | Allocation (R0 R1 R2) |    Max (R0 R1 R2)    |    Need (R0 R1 R2)\n");
    printf("---------+-----------------------+----------------------+--------------------\n");
    for (int i = 0; i < NUM_PROCESSES; i++) {
        printf("   P%d    |       %d   %d   %d       |      %d   %d   %d      |     %d   %d   %d\n",
               i,
               s->allocation[i][0], s->allocation[i][1], s->allocation[i][2],
               s->max[i][0], s->max[i][1], s->max[i][2],
               s->need[i][0], s->need[i][1], s->need[i][2]);
    }
    printf("------------------------------------------------------------------------\n");
    printf(" Available Resources: R0=%d, R1=%d, R2=%d\n",
           s->available[0], s->available[1], s->available[2]);
    printf("========================================================================\n");
}

bool evaluate_safety_state(const BankerSystem *s, int *safe_sequence) {
    int work[NUM_RESOURCES];
    bool finish[NUM_PROCESSES];

    for (int j = 0; j < NUM_RESOURCES; j++) {
        work[j] = s->available[j];
    }
    for (int i = 0; i < NUM_PROCESSES; i++) {
        finish[i] = false;
    }

    int count = 0;
    while (count < NUM_PROCESSES) {
        bool found_candidate = false;
        for (int p = 0; p < NUM_PROCESSES; p++) {
            if (!finish[p]) {
                bool can_satisfy = true;
                for (int r = 0; r < NUM_RESOURCES; r++) {
                    if (s->need[p][r] > work[r]) {
                        can_satisfy = false;
                        break;
                    }
                }
                if (can_satisfy) {
                    for (int r = 0; r < NUM_RESOURCES; r++) {
                        work[r] += s->allocation[p][r];
                    }
                    safe_sequence[count++] = p;
                    finish[p] = true;
                    found_candidate = true;
                }
            }
        }
        if (!found_candidate) {
            return false; // Deadlock potential detected
        }
    }
    return true;
}

bool process_resource_request(BankerSystem *s, int pid, const int req[NUM_RESOURCES]) {
    printf("\n--> Process P%d requesting resources: [R0=%d, R1=%d, R2=%d]\n",
           pid, req[0], req[1], req[2]);

    for (int r = 0; r < NUM_RESOURCES; r++) {
        if (req[r] > s->need[pid][r]) {
            printf("[-] Error: Process exceeded its declared maximum claim.\n");
            return false;
        }
        if (req[r] > s->available[r]) {
            printf("[-] Process P%d must wait: Insufficient available resources.\n", pid);
            return false;
        }
    }

    // Speculative allocation
    for (int r = 0; r < NUM_RESOURCES; r++) {
        s->available[r] -= req[r];
        s->allocation[pid][r] += req[r];
        s->need[pid][r] -= req[r];
    }

    int safe_sequence[NUM_PROCESSES];
    if (evaluate_safety_state(s, safe_sequence)) {
        printf("[+] Request APPROVED: System remains in a SAFE state.\n");
        printf("    Safe Sequence: ");
        for (int i = 0; i < NUM_PROCESSES; i++) {
            printf("P%d%s", safe_sequence[i], (i == NUM_PROCESSES - 1) ? "\n" : " -> ");
        }
        return true;
    } else {
        printf("[-] Request DENIED: Speculative allocation leads to an UNSAFE state.\n");
        // Rollback
        for (int r = 0; r < NUM_RESOURCES; r++) {
            s->available[r] += req[r];
            s->allocation[pid][r] -= req[r];
            s->need[pid][r] += req[r];
        }
        return false;
    }
}

void detect_wait_for_graph_cycle(const int wfg[NUM_PROCESSES][NUM_PROCESSES]) {
    printf("\n========================================================================\n");
    printf(" WAIT-FOR GRAPH (WFG) DEADLOCK DETECTION ANALYSIS\n");
    printf("========================================================================\n");

    int in_degree[NUM_PROCESSES] = {0};
    for (int i = 0; i < NUM_PROCESSES; i++) {
        for (int j = 0; j < NUM_PROCESSES; j++) {
            if (wfg[i][j]) {
                in_degree[j]++;
            }
        }
    }

    int queue[NUM_PROCESSES];
    int front = 0, rear = 0;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (in_degree[i] == 0) {
            queue[rear++] = i;
        }
    }

    int visited_count = 0;
    while (front < rear) {
        int u = queue[front++];
        visited_count++;
        for (int v = 0; v < NUM_PROCESSES; v++) {
            if (wfg[u][v]) {
                in_degree[v]--;
                if (in_degree[v] == 0) {
                    queue[rear++] = v;
                }
            }
        }
    }

    if (visited_count == NUM_PROCESSES) {
        printf(" WFG Assessment: NO DEADLOCK (Acyclic Graph / All Processes Resolve)\n");
    } else {
        printf(" WFG Assessment: DEADLOCK DETECTED! Circular wait identified in graph.\n");
        printf(" Resolution    : Preempt locks or abort lowest-priority transaction.\n");
    }
    printf("========================================================================\n");
}

int main(void) {
    BankerSystem sys = {
        .available = {3, 3, 2},
        .max = {
            {7, 5, 3}, // P0
            {3, 2, 2}, // P1
            {9, 0, 2}, // P2
            {2, 2, 2}, // P3
            {4, 3, 3}  // P4
        },
        .allocation = {
            {0, 1, 0}, // P0
            {2, 0, 0}, // P1
            {3, 0, 2}, // P2
            {2, 1, 1}, // P3
            {0, 0, 2}  // P4
        }
    };

    calculate_need_matrix(&sys);
    print_system_state(&sys, "Initial Ingestion State");

    int safe_seq[NUM_PROCESSES];
    if (evaluate_safety_state(&sys, safe_seq)) {
        printf("Initial State Assessment: SAFE STATE\nSafe Path: ");
        for (int i = 0; i < NUM_PROCESSES; i++) {
            printf("P%d%s", safe_seq[i], (i == NUM_PROCESSES - 1) ? "\n" : " -> ");
        }
    }

    // Safe Request Test: P1 asks for [1, 0, 2]
    int safe_req[NUM_RESOURCES] = {1, 0, 2};
    process_resource_request(&sys, 1, safe_req);

    // Unsafe Request Test: P4 asks for [3, 3, 0]
    int unsafe_req[NUM_RESOURCES] = {3, 3, 0};
    process_resource_request(&sys, 4, unsafe_req);

    // WFG Deadlock Simulation
    int wfg_matrix[NUM_PROCESSES][NUM_PROCESSES] = {
        {0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0},
        {1, 0, 0, 0, 0}, // Circular wait P0 -> P1 -> P2 -> P3 -> P0
        {0, 0, 0, 0, 0}
    };
    detect_wait_for_graph_cycle(wfg_matrix);

    return 0;
}
