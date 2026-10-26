#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define STRESS_HEAP_SIZE (64 * 1024) // 64 KB pool
#define MAX_ACTIVE_CHUNKS 256
#define NUM_SIMULATION_CYCLES 5000

typedef enum {
    ALLOC_FIRST_FIT,
    ALLOC_BEST_FIT,
    ALLOC_WORST_FIT
} FitPolicy;

typedef struct ChunkHeader {
    size_t capacity;
    bool is_free;
    uint32_t alloc_seq;
    struct ChunkHeader *next;
    struct ChunkHeader *prev;
} ChunkHeader;

static uint8_t memory_arena[STRESS_HEAP_SIZE];
static ChunkHeader *free_root = NULL;

static size_t stat_alloc_ops = 0;
static size_t stat_free_ops = 0;
static size_t stat_failed_allocs = 0;
static size_t stat_coalesce_ops = 0;

static void arena_init(void) {
    memset(memory_arena, 0, STRESS_HEAP_SIZE);
    free_root = (ChunkHeader *)memory_arena;
    free_root->capacity = STRESS_HEAP_SIZE - sizeof(ChunkHeader);
    free_root->is_free = true;
    free_root->alloc_seq = 0;
    free_root->next = NULL;
    free_root->prev = NULL;

    stat_alloc_ops = 0;
    stat_free_ops = 0;
    stat_failed_allocs = 0;
    stat_coalesce_ops = 0;
}

static ChunkHeader *search_fit(size_t req_bytes, FitPolicy policy) {
    ChunkHeader *curr = free_root;
    ChunkHeader *selected = NULL;

    while (curr != NULL) {
        if (curr->is_free && curr->capacity >= req_bytes) {
            if (policy == ALLOC_FIRST_FIT) {
                return curr;
            } else if (policy == ALLOC_BEST_FIT) {
                if (!selected || curr->capacity < selected->capacity) {
                    selected = curr;
                }
            } else if (policy == ALLOC_WORST_FIT) {
                if (!selected || curr->capacity > selected->capacity) {
                    selected = curr;
                }
            }
        }
        curr = curr->next;
    }
    return selected;
}

static void *stress_alloc(size_t bytes, FitPolicy policy, uint32_t seq) {
    stat_alloc_ops++;
    size_t aligned_bytes = (bytes + 7) & ~7;
    ChunkHeader *target = search_fit(aligned_bytes, policy);

    if (!target) {
        stat_failed_allocs++;
        return NULL;
    }

    size_t overhead = sizeof(ChunkHeader);
    if (target->capacity >= aligned_bytes + overhead + 16) {
        ChunkHeader *split = (ChunkHeader *)((uint8_t *)target + overhead + aligned_bytes);
        split->capacity = target->capacity - aligned_bytes - overhead;
        split->is_free = true;
        split->alloc_seq = 0;
        split->next = target->next;
        split->prev = target;

        if (target->next) {
            target->next->prev = split;
        }
        target->next = split;
        target->capacity = aligned_bytes;
    }

    target->is_free = false;
    target->alloc_seq = seq;
    return (void *)((uint8_t *)target + overhead);
}

static void stress_free(void *ptr) {
    if (!ptr) return;
    stat_free_ops++;
    ChunkHeader *hdr = (ChunkHeader *)((uint8_t *)ptr - sizeof(ChunkHeader));
    hdr->is_free = true;
    hdr->alloc_seq = 0;

    // Coalesce Next
    if (hdr->next && hdr->next->is_free) {
        hdr->capacity += sizeof(ChunkHeader) + hdr->next->capacity;
        hdr->next = hdr->next->next;
        if (hdr->next) {
            hdr->next->prev = hdr;
        }
        stat_coalesce_ops++;
    }

    // Coalesce Prev
    if (hdr->prev && hdr->prev->is_free) {
        hdr->prev->capacity += sizeof(ChunkHeader) + hdr->capacity;
        hdr->prev->next = hdr->next;
        if (hdr->prev->next) {
            hdr->prev->next->prev = hdr->prev;
        }
        stat_coalesce_ops++;
    }
}

typedef struct {
    size_t total_free_bytes;
    size_t total_used_bytes;
    size_t largest_free_block;
    size_t count_free_blocks;
    size_t count_used_blocks;
    double ext_fragmentation_pct;
} LayoutTelemetry;

static LayoutTelemetry collect_telemetry(void) {
    LayoutTelemetry t = {0, 0, 0, 0, 0, 0.0};
    ChunkHeader *it = free_root;

    while (it != NULL) {
        if (it->is_free) {
            t.total_free_bytes += it->capacity;
            t.count_free_blocks++;
            if (it->capacity > t.largest_free_block) {
                t.largest_free_block = it->capacity;
            }
        } else {
            t.total_used_bytes += it->capacity;
            t.count_used_blocks++;
        }
        it = it->next;
    }

    if (t.total_free_bytes > 0) {
        t.ext_fragmentation_pct = (1.0 - ((double)t.largest_free_block / (double)t.total_free_bytes)) * 100.0;
    }
    return t;
}

static void execute_stress_profile(FitPolicy policy, const char *label) {
    arena_init();
    void *active_chunks[MAX_ACTIVE_CHUNKS];
    memset(active_chunks, 0, sizeof(active_chunks));

    srand(1337); // Deterministic seed for reproducible evaluation

    size_t max_ext_frag = 0;
    size_t samples = 0;
    double cumulative_frag = 0.0;

    for (int cycle = 0; cycle < NUM_SIMULATION_CYCLES; cycle++) {
        int slot = rand() % MAX_ACTIVE_CHUNKS;

        if (active_chunks[slot] != NULL) {
            stress_free(active_chunks[slot]);
            active_chunks[slot] = NULL;
        } else {
            // Skewed packet size distribution: 60% small, 30% medium, 10% jumbo
            size_t req_bytes;
            int r = rand() % 100;
            if (r < 60) {
                req_bytes = 64 + (rand() % 128);      // 64B - 192B
            } else if (r < 90) {
                req_bytes = 256 + (rand() % 512);     // 256B - 768B
            } else {
                req_bytes = 1024 + (rand() % 2048);   // 1KB - 3KB
            }

            active_chunks[slot] = stress_alloc(req_bytes, policy, cycle + 1);
        }

        if (cycle % 250 == 0) {
            LayoutTelemetry t = collect_telemetry();
            cumulative_frag += t.ext_fragmentation_pct;
            samples++;
            if (t.ext_fragmentation_pct > max_ext_frag) {
                max_ext_frag = (size_t)t.ext_fragmentation_pct;
            }
        }
    }

    LayoutTelemetry final_t = collect_telemetry();
    double avg_frag = samples > 0 ? (cumulative_frag / samples) : 0.0;

    printf("[POLICY: %-10s]\n", label);
    printf("  Cycles Completed    : %d operations\n", NUM_SIMULATION_CYCLES);
    printf("  Successful Allocs   : %zu\n", stat_alloc_ops - stat_failed_allocs);
    printf("  Failed Allocs (OOM) : %zu\n", stat_failed_allocs);
    printf("  Free Operations     : %zu\n", stat_free_ops);
    printf("  Coalesce Merges     : %zu\n", stat_coalesce_ops);
    printf("  Final Active Blocks : %zu (%zu bytes used)\n", final_t.count_used_blocks, final_t.total_used_bytes);
    printf("  Remaining Free Holes: %zu (%zu bytes free)\n", final_t.count_free_blocks, final_t.total_free_bytes);
    printf("  Average Ext Frag    : %.2f%%\n", avg_frag);
    printf("  Peak Ext Frag       : %zu.00%%\n", max_ext_frag);
    printf("--------------------------------------------------------------------\n");
}

int main(void) {
    printf("====================================================================\n");
    printf(" SENTINELNET PACKET MEMORY HIGH-LOAD STRESS BENCHMARK\n");
    printf(" Evaluator: 5,000 Random Packet Bursts on 64KB Managed Arena\n");
    printf("====================================================================\n");

    execute_stress_profile(ALLOC_FIRST_FIT, "First-Fit");
    execute_stress_profile(ALLOC_BEST_FIT,  "Best-Fit");
    execute_stress_profile(ALLOC_WORST_FIT, "Worst-Fit");

    return 0;
}
