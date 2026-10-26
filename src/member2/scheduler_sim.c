#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_TASKS 32
#define GANTT_MAX_TICKS 512

typedef enum {
    ALGO_FCFS,
    ALGO_SJF_NP,
    ALGO_SRTF_P,
    ALGO_ROUND_ROBIN,
    ALGO_PRIORITY_NP,
    ALGO_PRIORITY_P
} SchedAlgorithm;

typedef struct {
    int pid;
    char task_name[24];
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;
    int completion_time;
    int waiting_time;
    int turnaround_time;
    int response_time;
    bool has_started;
} TaskDescriptor;

typedef struct {
    int pid;
    int start_tick;
    int duration;
} GanttSlice;

typedef struct {
    GanttSlice slices[GANTT_MAX_TICKS];
    int count;
    int total_execution_span;
} GanttLog;

typedef struct {
    double avg_waiting_time;
    double avg_turnaround_time;
    double avg_response_time;
    double cpu_utilization_pct;
    double throughput_tasks_per_sec;
    int total_context_switches;
} SchedulingMetrics;

static void clone_task_set(const TaskDescriptor *src, TaskDescriptor *dst, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
        dst[i].remaining_time = src[i].burst_time;
        dst[i].completion_time = 0;
        dst[i].waiting_time = 0;
        dst[i].turnaround_time = 0;
        dst[i].response_time = -1;
        dst[i].has_started = false;
    }
}

static void log_execution_slice(GanttLog *log, int pid) {
    if (log->count > 0 && log->slices[log->count - 1].pid == pid) {
        log->slices[log->count - 1].duration++;
        log->total_execution_span++;
        return;
    }
    if (log->count < GANTT_MAX_TICKS) {
        log->slices[log->count].pid = pid;
        log->slices[log->count].start_tick = log->total_execution_span;
        log->slices[log->count].duration = 1;
        log->count++;
        log->total_execution_span++;
    }
}

static void print_ascii_gantt(const GanttLog *log, const char *label) {
    printf("\n>>> Execution Timeline (Gantt Chart): %s <<<\n", label);
    printf("+");
    for (int i = 0; i < log->count; i++) {
        printf("-------+");
    }
    printf("\n|");
    for (int i = 0; i < log->count; i++) {
        if (log->slices[i].pid == -1) {
            printf(" IDLE  |");
        } else {
            printf("  P%-2d  |", log->slices[i].pid);
        }
    }
    printf("\n+");
    for (int i = 0; i < log->count; i++) {
        printf("-------+");
    }
    printf("\n0");
    int accum = 0;
    for (int i = 0; i < log->count; i++) {
        accum += log->slices[i].duration;
        printf("%8d", accum);
    }
    printf("\n");
}

static SchedulingMetrics evaluate_and_print_metrics(TaskDescriptor *tasks, int n, const GanttLog *log, const char *label) {
    double sum_wt = 0.0, sum_tat = 0.0, sum_rt = 0.0;
    int busy_time = 0;

    printf("\n------------------------------------------------------------------------------------\n");
    printf(" Evaluation Breakdown: %s\n", label);
    printf("------------------------------------------------------------------------------------\n");
    printf("%-5s | %-16s | %-4s | %-5s | %-4s | %-5s | %-5s | %-5s | %-5s\n",
           "PID", "Task Payload", "Arr", "Burst", "Prio", "Comp", "Wait", "TAT", "Resp");
    printf("------+------------------+------+-------+------+-------+-------+-------+-------\n");

    for (int i = 0; i < n; i++) {
        sum_wt += tasks[i].waiting_time;
        sum_tat += tasks[i].turnaround_time;
        sum_rt += tasks[i].response_time;
        busy_time += tasks[i].burst_time;
        printf("P%-4d | %-16s | %-4d | %-5d | %-4d | %-5d | %-5d | %-5d | %-5d\n",
               tasks[i].pid, tasks[i].task_name, tasks[i].arrival_time,
               tasks[i].burst_time, tasks[i].priority, tasks[i].completion_time,
               tasks[i].waiting_time, tasks[i].turnaround_time, tasks[i].response_time);
    }
    printf("------------------------------------------------------------------------------------\n");

    int csw = 0;
    for (int i = 1; i < log->count; i++) {
        if (log->slices[i].pid != log->slices[i - 1].pid && log->slices[i].pid != -1) {
            csw++;
        }
    }

    SchedulingMetrics m;
    m.avg_waiting_time = sum_wt / n;
    m.avg_turnaround_time = sum_tat / n;
    m.avg_response_time = sum_rt / n;
    m.total_context_switches = csw;
    m.cpu_utilization_pct = log->total_execution_span > 0 ? ((double)busy_time / log->total_execution_span) * 100.0 : 0.0;
    m.throughput_tasks_per_sec = log->total_execution_span > 0 ? ((double)n / log->total_execution_span) * 1000.0 : 0.0;

    printf(" Average Waiting Time (AWT)       : %.2f ms\n", m.avg_waiting_time);
    printf(" Average Turnaround Time (ATAT)   : %.2f ms\n", m.avg_turnaround_time);
    printf(" Average Response Time (ART)      : %.2f ms\n", m.avg_response_time);
    printf(" Context Switches Occurred        : %d\n", m.total_context_switches);
    printf(" CPU Utilization                  : %.2f%%\n", m.cpu_utilization_pct);
    printf(" System Throughput                : %.2f packets/sec (simulated)\n", m.throughput_tasks_per_sec);
    printf("------------------------------------------------------------------------------------\n");
    return m;
}

SchedulingMetrics simulate_fcfs(const TaskDescriptor *workload, int count) {
    TaskDescriptor tasks[MAX_TASKS];
    clone_task_set(workload, tasks, count);
    GanttLog log = { .count = 0, .total_execution_span = 0 };

    int tick = 0;
    for (int i = 0; i < count; i++) {
        while (tick < tasks[i].arrival_time) {
            log_execution_slice(&log, -1);
            tick++;
        }
        tasks[i].response_time = tick - tasks[i].arrival_time;
        for (int b = 0; b < tasks[i].burst_time; b++) {
            log_execution_slice(&log, tasks[i].pid);
            tick++;
        }
        tasks[i].completion_time = tick;
        tasks[i].turnaround_time = tasks[i].completion_time - tasks[i].arrival_time;
        tasks[i].waiting_time = tasks[i].turnaround_time - tasks[i].burst_time;
    }

    print_ascii_gantt(&log, "First-Come First-Served (FCFS)");
    return evaluate_and_print_metrics(tasks, count, &log, "FCFS Policy");
}

SchedulingMetrics simulate_sjf_non_preemptive(const TaskDescriptor *workload, int count) {
    TaskDescriptor tasks[MAX_TASKS];
    clone_task_set(workload, tasks, count);
    GanttLog log = { .count = 0, .total_execution_span = 0 };

    bool done[MAX_TASKS] = {false};
    int completed = 0;
    int tick = 0;

    while (completed < count) {
        int candidate = -1;
        int min_burst = INT_MAX;

        for (int i = 0; i < count; i++) {
            if (!done[i] && tasks[i].arrival_time <= tick) {
                if (tasks[i].burst_time < min_burst) {
                    min_burst = tasks[i].burst_time;
                    candidate = i;
                }
            }
        }

        if (candidate == -1) {
            log_execution_slice(&log, -1);
            tick++;
            continue;
        }

        tasks[candidate].response_time = tick - tasks[candidate].arrival_time;
        for (int b = 0; b < tasks[candidate].burst_time; b++) {
            log_execution_slice(&log, tasks[candidate].pid);
            tick++;
        }
        tasks[candidate].completion_time = tick;
        tasks[candidate].turnaround_time = tasks[candidate].completion_time - tasks[candidate].arrival_time;
        tasks[candidate].waiting_time = tasks[candidate].turnaround_time - tasks[candidate].burst_time;
        done[candidate] = true;
        completed++;
    }

    print_ascii_gantt(&log, "Shortest Job First (SJF - Non-Preemptive)");
    return evaluate_and_print_metrics(tasks, count, &log, "SJF Non-Preemptive");
}

SchedulingMetrics simulate_srtf_preemptive(const TaskDescriptor *workload, int count) {
    TaskDescriptor tasks[MAX_TASKS];
    clone_task_set(workload, tasks, count);
    GanttLog log = { .count = 0, .total_execution_span = 0 };

    int completed = 0;
    int tick = 0;

    while (completed < count) {
        int candidate = -1;
        int min_rem = INT_MAX;

        for (int i = 0; i < count; i++) {
            if (tasks[i].arrival_time <= tick && tasks[i].remaining_time > 0) {
                if (tasks[i].remaining_time < min_rem) {
                    min_rem = tasks[i].remaining_time;
                    candidate = i;
                }
            }
        }

        if (candidate == -1) {
            log_execution_slice(&log, -1);
            tick++;
            continue;
        }

        if (!tasks[candidate].has_started) {
            tasks[candidate].response_time = tick - tasks[candidate].arrival_time;
            tasks[candidate].has_started = true;
        }

        log_execution_slice(&log, tasks[candidate].pid);
        tasks[candidate].remaining_time--;
        tick++;

        if (tasks[candidate].remaining_time == 0) {
            tasks[candidate].completion_time = tick;
            tasks[candidate].turnaround_time = tasks[candidate].completion_time - tasks[candidate].arrival_time;
            tasks[candidate].waiting_time = tasks[candidate].turnaround_time - tasks[candidate].burst_time;
            completed++;
        }
    }

    print_ascii_gantt(&log, "Shortest Remaining Time First (SRTF - Preemptive)");
    return evaluate_and_print_metrics(tasks, count, &log, "SRTF Preemptive");
}

SchedulingMetrics simulate_round_robin(const TaskDescriptor *workload, int count, int quantum) {
    TaskDescriptor tasks[MAX_TASKS];
    clone_task_set(workload, tasks, count);
    GanttLog log = { .count = 0, .total_execution_span = 0 };

    int queue[MAX_TASKS * 32];
    int q_head = 0, q_tail = 0;
    bool in_queue[MAX_TASKS] = {false};
    int completed = 0;
    int tick = 0;

    for (int i = 0; i < count; i++) {
        if (tasks[i].arrival_time == 0) {
            queue[q_tail++] = i;
            in_queue[i] = true;
        }
    }

    while (completed < count) {
        if (q_head == q_tail) {
            int next_arr = INT_MAX;
            for (int i = 0; i < count; i++) {
                if (tasks[i].remaining_time > 0 && tasks[i].arrival_time > tick) {
                    if (tasks[i].arrival_time < next_arr) next_arr = tasks[i].arrival_time;
                }
            }
            while (tick < next_arr) {
                log_execution_slice(&log, -1);
                tick++;
            }
            for (int i = 0; i < count; i++) {
                if (tasks[i].arrival_time <= tick && tasks[i].remaining_time > 0 && !in_queue[i]) {
                    queue[q_tail++] = i;
                    in_queue[i] = true;
                }
            }
            continue;
        }

        int curr = queue[q_head++];
        in_queue[curr] = false;

        if (!tasks[curr].has_started) {
            tasks[curr].response_time = tick - tasks[curr].arrival_time;
            tasks[curr].has_started = true;
        }

        int run_slice = (tasks[curr].remaining_time > quantum) ? quantum : tasks[curr].remaining_time;
        for (int s = 0; s < run_slice; s++) {
            log_execution_slice(&log, tasks[curr].pid);
            tick++;
            for (int i = 0; i < count; i++) {
                if (tasks[i].arrival_time == tick && tasks[i].remaining_time > 0 && !in_queue[i] && i != curr) {
                    queue[q_tail++] = i;
                    in_queue[i] = true;
                }
            }
        }
        tasks[curr].remaining_time -= run_slice;

        if (tasks[curr].remaining_time == 0) {
            tasks[curr].completion_time = tick;
            tasks[curr].turnaround_time = tasks[curr].completion_time - tasks[curr].arrival_time;
            tasks[curr].waiting_time = tasks[curr].turnaround_time - tasks[curr].burst_time;
            completed++;
        } else {
            queue[q_tail++] = curr;
            in_queue[curr] = true;
        }
    }

    char desc[64];
    snprintf(desc, sizeof(desc), "Round Robin (Quantum = %d ms)", quantum);
    print_ascii_gantt(&log, desc);
    return evaluate_and_print_metrics(tasks, count, &log, desc);
}

SchedulingMetrics simulate_priority_preemptive(const TaskDescriptor *workload, int count) {
    TaskDescriptor tasks[MAX_TASKS];
    clone_task_set(workload, tasks, count);
    GanttLog log = { .count = 0, .total_execution_span = 0 };

    int completed = 0;
    int tick = 0;

    while (completed < count) {
        int candidate = -1;
        int highest_prio = INT_MAX;

        for (int i = 0; i < count; i++) {
            if (tasks[i].arrival_time <= tick && tasks[i].remaining_time > 0) {
                if (tasks[i].priority < highest_prio) {
                    highest_prio = tasks[i].priority;
                    candidate = i;
                }
            }
        }

        if (candidate == -1) {
            log_execution_slice(&log, -1);
            tick++;
            continue;
        }

        if (!tasks[candidate].has_started) {
            tasks[candidate].response_time = tick - tasks[candidate].arrival_time;
            tasks[candidate].has_started = true;
        }

        log_execution_slice(&log, tasks[candidate].pid);
        tasks[candidate].remaining_time--;
        tick++;

        if (tasks[candidate].remaining_time == 0) {
            tasks[candidate].completion_time = tick;
            tasks[candidate].turnaround_time = tasks[candidate].completion_time - tasks[candidate].arrival_time;
            tasks[candidate].waiting_time = tasks[candidate].turnaround_time - tasks[candidate].burst_time;
            completed++;
        }
    }

    print_ascii_gantt(&log, "Preemptive Priority Dispatching");
    return evaluate_and_print_metrics(tasks, count, &log, "Priority Preemptive");
}

void print_comparative_summary_table(const SchedulingMetrics *m_fcfs,
                                     const SchedulingMetrics *m_sjf,
                                     const SchedulingMetrics *m_srtf,
                                     const SchedulingMetrics *m_rr,
                                     const SchedulingMetrics *m_prio) {
    printf("\n====================================================================================\n");
    printf(" SUMMARY COMPARISON MATRIX: SENTINELNET CPU DISPATCH POLICIES\n");
    printf("====================================================================================\n");
    printf("%-24s | %-10s | %-10s | %-10s | %-8s | %-8s\n",
           "Algorithm", "Avg WT(ms)", "Avg TAT(ms)", "Avg RT(ms)", "CSW Count", "CPU Util");
    printf("-------------------------+------------+------------+------------+----------+---------\n");
    printf("%-24s | %-10.2f | %-10.2f | %-10.2f | %-8d | %6.2f%%\n",
           "FCFS", m_fcfs->avg_waiting_time, m_fcfs->avg_turnaround_time, m_fcfs->avg_response_time,
           m_fcfs->total_context_switches, m_fcfs->cpu_utilization_pct);
    printf("%-24s | %-10.2f | %-10.2f | %-10.2f | %-8d | %6.2f%%\n",
           "SJF (Non-Preemptive)", m_sjf->avg_waiting_time, m_sjf->avg_turnaround_time, m_sjf->avg_response_time,
           m_sjf->total_context_switches, m_sjf->cpu_utilization_pct);
    printf("%-24s | %-10.2f | %-10.2f | %-10.2f | %-8d | %6.2f%%\n",
           "SRTF (Preemptive)", m_srtf->avg_waiting_time, m_srtf->avg_turnaround_time, m_srtf->avg_response_time,
           m_srtf->total_context_switches, m_srtf->cpu_utilization_pct);
    printf("%-24s | %-10.2f | %-10.2f | %-10.2f | %-8d | %6.2f%%\n",
           "Round Robin (Q=2ms)", m_rr->avg_waiting_time, m_rr->avg_turnaround_time, m_rr->avg_response_time,
           m_rr->total_context_switches, m_rr->cpu_utilization_pct);
    printf("%-24s | %-10.2f | %-10.2f | %-10.2f | %-8d | %6.2f%%\n",
           "Priority Preemptive", m_prio->avg_waiting_time, m_prio->avg_turnaround_time, m_prio->avg_response_time,
           m_prio->total_context_switches, m_prio->cpu_utilization_pct);
    printf("====================================================================================\n\n");
}

int main(void) {
    TaskDescriptor workload[] = {
        {1, "DNS_INGESTION",   0, 4, 4, 3, 0, 0, 0, -1, false},
        {2, "TLS_HANDSHAKE",   1, 8, 8, 1, 0, 0, 0, -1, false},
        {3, "ICMP_INSPECT",    2, 2, 2, 4, 0, 0, 0, -1, false},
        {4, "SQL_PAYLOAD_DPI", 3, 6, 6, 2, 0, 0, 0, -1, false},
        {5, "HEARTBEAT_POLL",  4, 1, 1, 5, 0, 0, 0, -1, false}
    };
    int count = sizeof(workload) / sizeof(workload[0]);

    printf("====================================================================================\n");
    printf(" SENTINELNET WORKER PROCESS SCHEDULER (MEMBER 2: OS UNIT 2)\n");
    printf(" Workload: %d Active Ingestion Tasks with Variable Burst and Arrival Characteristics\n", count);
    printf("====================================================================================\n");

    SchedulingMetrics m_fcfs = simulate_fcfs(workload, count);
    SchedulingMetrics m_sjf  = simulate_sjf_non_preemptive(workload, count);
    SchedulingMetrics m_srtf = simulate_srtf_preemptive(workload, count);
    SchedulingMetrics m_rr   = simulate_round_robin(workload, count, 2);
    SchedulingMetrics m_prio = simulate_priority_preemptive(workload, count);

    print_comparative_summary_table(&m_fcfs, &m_sjf, &m_srtf, &m_rr, &m_prio);
    return 0;
}
