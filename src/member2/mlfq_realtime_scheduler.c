#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    const char *task_name;
    int arrival;
    int burst;
    int remaining;
    int priority;   /* 1 = Highest (Threat Alert), 3 = Lowest (Bulk Log) */
    int queue_lvl;  /* MLFQ level: 0 (Q=2), 1 (Q=4), 2 (FCFS) */
    int completion;
    int waiting;
    int turnaround;
    int assigned_core;
} PacketTask;

static void run_priority_preemptive(PacketTask tasks[], int n, double *avg_wait, double *avg_tat) {
    int completed = 0, t = 0;
    while (completed < n) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (tasks[i].arrival <= t && tasks[i].remaining > 0) {
                if (best == -1 || tasks[i].priority < tasks[best].priority) {
                    best = i;
                }
            }
        }
        if (best == -1) { t++; continue; }
        tasks[best].remaining--;
        t++;
        if (tasks[best].remaining == 0) {
            completed++;
            tasks[best].completion = t;
            tasks[best].turnaround = t - tasks[best].arrival;
            tasks[best].waiting = tasks[best].turnaround - tasks[best].burst;
        }
    }
    double sw = 0, st = 0;
    for (int i = 0; i < n; i++) { sw += tasks[i].waiting; st += tasks[i].turnaround; }
    *avg_wait = sw / n;
    *avg_tat = st / n;
}

static void run_mlfq(PacketTask tasks[], int n, double *avg_wait, double *avg_tat) {
    int quanta[3] = {2, 4, 100};
    int completed = 0, t = 0;
    while (completed < n) {
        int chosen = -1;
        for (int q = 0; q < 3 && chosen == -1; q++) {
            for (int i = 0; i < n; i++) {
                if (tasks[i].arrival <= t && tasks[i].remaining > 0 && tasks[i].queue_lvl == q) {
                    chosen = i;
                    break;
                }
            }
        }
        if (chosen == -1) { t++; continue; }
        int slice = quanta[tasks[chosen].queue_lvl];
        int exec = tasks[chosen].remaining < slice ? tasks[chosen].remaining : slice;
        tasks[chosen].remaining -= exec;
        t += exec;

        /* Aging: promote waiting tasks in Q2 back to Q1 if waiting > 10 ticks */
        for (int i = 0; i < n; i++) {
            if (i != chosen && tasks[i].remaining > 0 && tasks[i].arrival < t && tasks[i].queue_lvl == 2) {
                if ((t - tasks[i].arrival) > 10) tasks[i].queue_lvl = 1;
            }
        }

        if (tasks[chosen].remaining == 0) {
            completed++;
            tasks[chosen].completion = t;
            tasks[chosen].turnaround = t - tasks[chosen].arrival;
            tasks[chosen].waiting = tasks[chosen].turnaround - tasks[chosen].burst;
        } else if (tasks[chosen].queue_lvl < 2) {
            tasks[chosen].queue_lvl++;
        }
    }
    double sw = 0, st = 0;
    for (int i = 0; i < n; i++) { sw += tasks[i].waiting; st += tasks[i].turnaround; }
    *avg_wait = sw / n;
    *avg_tat = st / n;
}

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    PacketTask base[5] = {
        {1, "Bulk_PCAP_Archive",  0, 8, 8, 3, 0, 0, 0, 0, 0},
        {2, "SYN_Flood_Inspect",  1, 3, 3, 1, 0, 0, 0, 0, 0},
        {3, "SQLi_Regex_Scan",    2, 4, 4, 1, 0, 0, 0, 0, 0},
        {4, "DNS_Lookup_Audit",   3, 2, 2, 2, 0, 0, 0, 0, 0},
        {5, "Syslog_Flush",       4, 5, 5, 3, 0, 0, 0, 0, 0}
    };

    PacketTask p_tasks[5], m_tasks[5];
    memcpy(p_tasks, base, sizeof(base));
    memcpy(m_tasks, base, sizeof(base));

    double prio_wait = 0, prio_tat = 0, mlfq_wait = 0, mlfq_tat = 0;
    run_priority_preemptive(p_tasks, 5, &prio_wait, &prio_tat);
    run_mlfq(m_tasks, 5, &mlfq_wait, &mlfq_tat);

    /* Multi-core load balancing across 2 CPU cores (least-loaded core assignment) */
    int core_load[2] = {0, 0};
    for (int i = 0; i < 5; i++) {
        int target_core = (core_load[0] <= core_load[1]) ? 0 : 1;
        m_tasks[i].assigned_core = target_core;
        core_load[target_core] += m_tasks[i].burst;
    }

    if (json_mode) {
        printf("{\n"
               "  \"priority_preemptive\": {\"avg_waiting_ms\": %.2f, \"avg_turnaround_ms\": %.2f},\n"
               "  \"mlfq_with_aging\": {\"avg_waiting_ms\": %.2f, \"avg_turnaround_ms\": %.2f},\n"
               "  \"multicore_balancing\": {\"core_0_load\": %d, \"core_1_load\": %d},\n"
               "  \"tasks\": [\n",
               prio_wait, prio_tat, mlfq_wait, mlfq_tat, core_load[0], core_load[1]);
        for (int i = 0; i < 5; i++) {
            printf("    {\"id\": %d, \"name\": \"%s\", \"burst\": %d, \"mlfq_wait\": %d, \"mlfq_tat\": %d, \"core\": %d}%s\n",
                   m_tasks[i].id, m_tasks[i].task_name, m_tasks[i].burst,
                   m_tasks[i].waiting, m_tasks[i].turnaround, m_tasks[i].assigned_core,
                   i < 4 ? "," : "");
        }
        printf("  ]\n}\n");
    } else {
        printf("=== OS Unit 2: Priority, MLFQ & Multi-Core Scheduler (Anjali) ===\n");
        printf("Preemptive Priority -> Avg Wait: %.2f ms | Avg Turnaround: %.2f ms\n", prio_wait, prio_tat);
        printf("MLFQ (3-Level+Aging)-> Avg Wait: %.2f ms | Avg Turnaround: %.2f ms\n", mlfq_wait, mlfq_tat);
        printf("Multi-Core Balance  -> Core 0 Load: %d ticks | Core 1 Load: %d ticks\n", core_load[0], core_load[1]);
    }
    return 0;
}
