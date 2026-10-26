#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define MAX_PAGES 128
#define MAX_FRAMES 16


typedef struct {
    int page_id;
    bool second_chance;
    int last_used;
} FrameSlot;

typedef struct {
    int page_faults;
    int page_hits;
    double hit_ratio_percent;
    double fault_ratio_percent;
} PagingMetrics;

static void init_frames(FrameSlot *frames, int num_frames) {
    for (int i = 0; i < num_frames; i++) {
        frames[i].page_id = -1;
        frames[i].second_chance = false;
        frames[i].last_used = -1;
    }
}

static int find_page_in_frames(const FrameSlot *frames, int num_frames, int page) {
    for (int i = 0; i < num_frames; i++) {
        if (frames[i].page_id == page) {
            return i;
        }
    }
    return -1;
}

PagingMetrics simulate_fifo(const int *refs, int ref_len, int num_frames) {
    FrameSlot frames[MAX_FRAMES];
    init_frames(frames, num_frames);
    PagingMetrics m = {0, 0, 0.0, 0.0};
    int pointer = 0;

    for (int i = 0; i < ref_len; i++) {
        int page = refs[i];
        if (find_page_in_frames(frames, num_frames, page) != -1) {
            m.page_hits++;
        } else {
            m.page_faults++;
            frames[pointer].page_id = page;
            pointer = (pointer + 1) % num_frames;
        }
    }
    m.hit_ratio_percent = ((double)m.page_hits / ref_len) * 100.0;
    m.fault_ratio_percent = ((double)m.page_faults / ref_len) * 100.0;
    return m;
}

PagingMetrics simulate_lru(const int *refs, int ref_len, int num_frames) {
    FrameSlot frames[MAX_FRAMES];
    init_frames(frames, num_frames);
    PagingMetrics m = {0, 0, 0.0, 0.0};

    for (int i = 0; i < ref_len; i++) {
        int page = refs[i];
        int idx = find_page_in_frames(frames, num_frames, page);
        if (idx != -1) {
            m.page_hits++;
            frames[idx].last_used = i;
        } else {
            m.page_faults++;
            int victim = -1;
            int oldest = INT_MAX;
            for (int f = 0; f < num_frames; f++) {
                if (frames[f].page_id == -1) {
                    victim = f;
                    break;
                }
                if (frames[f].last_used < oldest) {
                    oldest = frames[f].last_used;
                    victim = f;
                }
            }
            frames[victim].page_id = page;
            frames[victim].last_used = i;
        }
    }
    m.hit_ratio_percent = ((double)m.page_hits / ref_len) * 100.0;
    m.fault_ratio_percent = ((double)m.page_faults / ref_len) * 100.0;
    return m;
}

PagingMetrics simulate_optimal(const int *refs, int ref_len, int num_frames) {
    FrameSlot frames[MAX_FRAMES];
    init_frames(frames, num_frames);
    PagingMetrics m = {0, 0, 0.0, 0.0};

    for (int i = 0; i < ref_len; i++) {
        int page = refs[i];
        if (find_page_in_frames(frames, num_frames, page) != -1) {
            m.page_hits++;
        } else {
            m.page_faults++;
            int victim = -1;
            int farthest = i;

            for (int f = 0; f < num_frames; f++) {
                if (frames[f].page_id == -1) {
                    victim = f;
                    break;
                }
                int next_use = -1;
                for (int j = i + 1; j < ref_len; j++) {
                    if (refs[j] == frames[f].page_id) {
                        next_use = j;
                        break;
                    }
                }
                if (next_use == -1) {
                    victim = f;
                    break;
                }
                if (next_use > farthest) {
                    farthest = next_use;
                    victim = f;
                }
            }
            if (victim == -1) victim = 0;
            frames[victim].page_id = page;
        }
    }
    m.hit_ratio_percent = ((double)m.page_hits / ref_len) * 100.0;
    m.fault_ratio_percent = ((double)m.page_faults / ref_len) * 100.0;
    return m;
}

PagingMetrics simulate_clock(const int *refs, int ref_len, int num_frames) {
    FrameSlot frames[MAX_FRAMES];
    init_frames(frames, num_frames);
    PagingMetrics m = {0, 0, 0.0, 0.0};
    int hand = 0;

    for (int i = 0; i < ref_len; i++) {
        int page = refs[i];
        int idx = find_page_in_frames(frames, num_frames, page);
        if (idx != -1) {
            m.page_hits++;
            frames[idx].second_chance = true;
        } else {
            m.page_faults++;
            while (true) {
                if (frames[hand].page_id == -1) {
                    frames[hand].page_id = page;
                    frames[hand].second_chance = false;
                    hand = (hand + 1) % num_frames;
                    break;
                }
                if (frames[hand].second_chance) {
                    frames[hand].second_chance = false;
                    hand = (hand + 1) % num_frames;
                } else {
                    frames[hand].page_id = page;
                    frames[hand].second_chance = false;
                    hand = (hand + 1) % num_frames;
                    break;
                }
            }
        }
    }
    m.hit_ratio_percent = ((double)m.page_hits / ref_len) * 100.0;
    m.fault_ratio_percent = ((double)m.page_faults / ref_len) * 100.0;
    return m;
}

void print_paging_matrix(const int *refs, int len, int frames) {
    PagingMetrics fifo = simulate_fifo(refs, len, frames);
    PagingMetrics lru  = simulate_lru(refs, len, frames);
    PagingMetrics opt  = simulate_optimal(refs, len, frames);
    PagingMetrics clk  = simulate_clock(refs, len, frames);

    printf("\n>>> WORKLOAD PERFORMANCE COMPARISON (Frames = %d) <<<\n", frames);
    printf("%-16s | %-12s | %-12s | %-14s | %-14s\n",
           "Policy", "Page Faults", "Page Hits", "Hit Ratio (%)", "Fault Ratio (%)");
    printf("-----------------+--------------+--------------+----------------+----------------\n");
    printf("%-16s | %-12d | %-12d | %12.2f%% | %12.2f%%\n", "FIFO", fifo.page_faults, fifo.page_hits, fifo.hit_ratio_percent, fifo.fault_ratio_percent);
    printf("%-16s | %-12d | %-12d | %12.2f%% | %12.2f%%\n", "LRU", lru.page_faults, lru.page_hits, lru.hit_ratio_percent, lru.fault_ratio_percent);
    printf("%-16s | %-12d | %-12d | %12.2f%% | %12.2f%%\n", "Clock (2nd Ch)", clk.page_faults, clk.page_hits, clk.hit_ratio_percent, clk.fault_ratio_percent);
    printf("%-16s | %-12d | %-12d | %12.2f%% | %12.2f%%\n", "Optimal (Belady)", opt.page_faults, opt.page_hits, opt.hit_ratio_percent, opt.fault_ratio_percent);
    printf("-----------------+--------------+--------------+----------------+----------------\n");
}

void demonstrate_beladys_anomaly(void) {
    //Belady's Anomaly reference string
    int anomaly_seq[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    int len = sizeof(anomaly_seq) / sizeof(anomaly_seq[0]);

    printf("\nBELADY'S ANOMALY DEMONSTRATION (FIFO ALLOCATION)\n");

    PagingMetrics f3 = simulate_fifo(anomaly_seq, len, 3);
    PagingMetrics f4 = simulate_fifo(anomaly_seq, len, 4);

    printf(" Frames = 3  Page Faults: %d\n", f3.page_faults);
    printf(" Frames = 4  Page Faults: %d\n", f4.page_faults);
}

int main(void) {
    int packet_header_cache_refs[] = {
        7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1
    };
    int len = sizeof(packet_header_cache_refs) / sizeof(packet_header_cache_refs[0]);

    printf("\n SENTINELNET VIRTUAL MEMORY PAGE REPLACEMENT ENGINE\n");

    print_paging_matrix(packet_header_cache_refs, len, 3);
    print_paging_matrix(packet_header_cache_refs, len, 4);
    demonstrate_beladys_anomaly();

    return 0;
}
