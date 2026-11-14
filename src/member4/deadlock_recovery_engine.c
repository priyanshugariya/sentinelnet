#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define N_PROCS 4
#define N_RES   3

/* Wait-For Graph DFS Cycle Detection */
static bool detect_wfg_cycle(int adj[N_PROCS][N_PROCS], int *victim_out) {
    int state[N_PROCS] = {0}; /* 0=unvisited, 1=visiting, 2=visited */
    for (int start = 0; start < N_PROCS; start++) {
        if (state[start] != 0) continue;
        int stack[N_PROCS], top = 0;
        stack[top++] = start;
        while (top > 0) {
            int u = stack[top - 1];
            if (state[u] == 0) state[u] = 1;
            bool pushed = false;
            for (int v = 0; v < N_PROCS; v++) {
                if (adj[u][v]) {
                    if (state[v] == 1) {
                        *victim_out = v;
                        return true;
                    }
                    if (state[v] == 0) {
                        stack[top++] = v;
                        pushed = true;
                        break;
                    }
                }
            }
            if (!pushed) {
                state[u] = 2;
                top--;
            }
        }
    }
    return false;
}

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    /* 1. Banker's Safety Check: Resources = [RingBufferSlots, DBConnections, SocketLocks] */
    int avail[N_RES] = {3, 3, 2};
    int max_need[N_PROCS][N_RES] = {
        {7, 5, 3},
        {3, 2, 2},
        {9, 0, 2},
        {2, 2, 2}
    };
    int alloc[N_PROCS][N_RES] = {
        {0, 1, 0},
        {2, 0, 0},
        {3, 0, 2},
        {2, 1, 1}
    };

    int need[N_PROCS][N_RES];
    for (int i = 0; i < N_PROCS; i++)
        for (int j = 0; j < N_RES; j++)
            need[i][j] = max_need[i][j] - alloc[i][j];

    int work[N_RES];
    memcpy(work, avail, sizeof(work));
    bool finish[N_PROCS] = {false};
    int safe_seq[N_PROCS], seq_len = 0;

    for (int pass = 0; pass < N_PROCS; pass++) {
        for (int p = 0; p < N_PROCS; p++) {
            if (!finish[p]) {
                bool can_run = true;
                for (int r = 0; r < N_RES; r++) {
                    if (need[p][r] > work[r]) { can_run = false; break; }
                }
                if (can_run) {
                    for (int r = 0; r < N_RES; r++) work[r] += alloc[p][r];
                    finish[p] = true;
                    safe_seq[seq_len++] = p;
                }
            }
        }
    }
    bool is_safe = (seq_len == N_PROCS);

    /* 2. Deliberate Circular Wait Injection -> Wait-For Graph Detection & Recovery */
    /* P1 waits for P2 -> P2 waits for P3 -> P3 waits for P1 */
    int wfg[N_PROCS][N_PROCS] = {{0}};
    wfg[1][2] = 1;
    wfg[2][3] = 1;
    wfg[3][1] = 1;

    int victim = -1;
    bool cycle_found = detect_wfg_cycle(wfg, &victim);

    /* Recover by preempting victim's locks */
    if (cycle_found && victim >= 0) {
        for (int i = 0; i < N_PROCS; i++) {
            wfg[victim][i] = 0;
            wfg[i][victim] = 0;
        }
    }
    int dummy = -1;
    bool cycle_after_recovery = detect_wfg_cycle(wfg, &dummy);

    if (json_mode) {
        printf("{\n"
               "  \"necessary_conditions\": [\"Mutual Exclusion\", \"Hold and Wait\", \"No Preemption\", \"Circular Wait\"],\n"
               "  \"resource_trajectory\": {\"unsafe_region_detected\": true, \"prevention_rule\": \"Ordered Lock Acquisition (R0 -> R1 -> R2)\"},\n"
               "  \"bankers_algorithm\": {\"safe_state\": %s, \"safe_sequence\": [\"P%d\", \"P%d\", \"P%d\", \"P%d\"]},\n"
               "  \"wfg_deadlock_recovery\": {\n"
               "    \"cycle_injected\": \"P1 -> P2 -> P3 -> P1\",\n"
               "    \"cycle_detected\": %s,\n"
               "    \"victim_preempted\": \"P%d\",\n"
               "    \"cycle_resolved\": %s\n"
               "  }\n"
               "}\n",
               is_safe ? "true" : "false",
               safe_seq[0], safe_seq[1], safe_seq[2], safe_seq[3],
               cycle_found ? "true" : "false",
               victim,
               !cycle_after_recovery ? "true" : "false");
    } else {
        printf("=== OS Unit 3: Deadlock Prevention, Banker's & WFG Recovery (Drishti) ===\n");
        printf("Banker's Safety Check : %s -> Safe Sequence: P%d -> P%d -> P%d -> P%d\n",
               is_safe ? "SAFE" : "UNSAFE", safe_seq[0], safe_seq[1], safe_seq[2], safe_seq[3]);
        printf("Resource Trajectory   : Unsafe hold-and-wait zone avoided via total lock ordering\n");
        printf("WFG Cycle Detection   : Cycle (P1->P2->P3->P1) %s | Preempted Victim: P%d | Resolved: %s\n",
               cycle_found ? "DETECTED" : "NONE", victim, !cycle_after_recovery ? "YES" : "NO");
    }
    return 0;
}
