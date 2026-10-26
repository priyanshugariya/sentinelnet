#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define HEAP_CAPACITY 4096
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

typedef enum {
    POLICY_FIRST_FIT,
    POLICY_BEST_FIT,
    POLICY_WORST_FIT
} AllocPolicy;

typedef struct BlockHeader {
    size_t size;
    bool is_free;
    uint32_t tag_id;
    struct BlockHeader* next;
    struct BlockHeader* prev;
} BlockHeader;

static uint8_t raw_memory_pool[HEAP_CAPACITY];
static BlockHeader* free_list_head = NULL;
static size_t total_alloc_requests = 0;
static size_t total_free_requests = 0;
static size_t total_coalesce_events = 0;

void mem_engine_init(void) {
    memset(raw_memory_pool, 0, HEAP_CAPACITY);
    free_list_head = (BlockHeader*)raw_memory_pool;
    free_list_head->size = HEAP_CAPACITY - sizeof(BlockHeader);
    free_list_head->is_free = true;
    free_list_head->tag_id = 0;
    free_list_head->next = NULL;
    free_list_head->prev = NULL;
    total_alloc_requests = 0;
    total_free_requests = 0;
    total_coalesce_events = 0;
}

static BlockHeader* find_first_fit(size_t aligned_size) {
    BlockHeader* curr = free_list_head;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= aligned_size) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static BlockHeader* find_best_fit(size_t aligned_size) {
    BlockHeader* curr = free_list_head;
    BlockHeader* best = NULL;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= aligned_size) {
            if (best == NULL || curr->size < best->size) {
                best = curr;
            }
        }
        curr = curr->next;
    }
    return best;
}

static BlockHeader* find_worst_fit(size_t aligned_size) {
    BlockHeader* curr = free_list_head;
    BlockHeader* worst = NULL;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= aligned_size) {
            if (worst == NULL || curr->size > worst->size) {
                worst = curr;
            }
        }
        curr = curr->next;
    }
    return worst;
}

static void split_block(BlockHeader* target, size_t requested_size, uint32_t tag) {
    size_t min_split_threshold = sizeof(BlockHeader) + ALIGNMENT;
    if (target->size >= requested_size + min_split_threshold) {
        BlockHeader* remainder = (BlockHeader*)((uint8_t*)target + sizeof(BlockHeader) + requested_size);
        remainder->size = target->size - requested_size - sizeof(BlockHeader);
        remainder->is_free = true;
        remainder->tag_id = 0;
        remainder->next = target->next;
        remainder->prev = target;

        if (target->next != NULL) {
            target->next->prev = remainder;
        }
        target->next = remainder;
        target->size = requested_size;
    }
    target->is_free = false;
    target->tag_id = tag;
}

void* mem_engine_alloc(size_t size, AllocPolicy policy, uint32_t tag) {
    total_alloc_requests++;
    size_t aligned_size = ALIGN(size);
    BlockHeader* victim = NULL;

    switch (policy) {
        case POLICY_FIRST_FIT: victim = find_first_fit(aligned_size); break;
        case POLICY_BEST_FIT:  victim = find_best_fit(aligned_size); break;
        case POLICY_WORST_FIT: victim = find_worst_fit(aligned_size); break;
        default:               victim = find_first_fit(aligned_size); break;
    }

    if (!victim) {
        return NULL;
    }

    split_block(victim, aligned_size, tag);
    return (void*)((uint8_t*)victim + sizeof(BlockHeader));
}

static void coalesce_adjacent(BlockHeader* blk) {
    if (blk->next != NULL && blk->next->is_free) {
        blk->size += sizeof(BlockHeader) + blk->next->size;
        blk->next = blk->next->next;
        if (blk->next != NULL) {
            blk->next->prev = blk;
        }
        total_coalesce_events++;
    }

    if (blk->prev != NULL && blk->prev->is_free) {
        blk->prev->size += sizeof(BlockHeader) + blk->size;
        blk->prev->next = blk->next;
        if (blk->next != NULL) {
            blk->next->prev = blk->prev;
        }
        total_coalesce_events++;
    }
}

void mem_engine_free(void* ptr) {
    if (!ptr) return;
    total_free_requests++;
    BlockHeader* header = (BlockHeader*)((uint8_t*)ptr - sizeof(BlockHeader));
    header->is_free = true;
    header->tag_id = 0;
    coalesce_adjacent(header);
}

typedef struct {
    size_t total_free_bytes;
    size_t total_allocated_bytes;
    size_t largest_free_block;
    size_t free_block_count;
    size_t allocated_block_count;
    double external_frag_percent;
} MemoryMetrics;

MemoryMetrics mem_engine_compute_metrics(void) {
    MemoryMetrics m = {0, 0, 0, 0, 0, 0.0};
    BlockHeader* curr = free_list_head;

    while (curr != NULL) {
        if (curr->is_free) {
            m.total_free_bytes += curr->size;
            m.free_block_count++;
            if (curr->size > m.largest_free_block) {
                m.largest_free_block = curr->size;
            }
        } else {
            m.total_allocated_bytes += curr->size;
            m.allocated_block_count++;
        }
        curr = curr->next;
    }

    if (m.total_free_bytes > 0) {
        m.external_frag_percent = (1.0 - ((double)m.largest_free_block / (double)m.total_free_bytes)) * 100.0;
    } else {
        m.external_frag_percent = 0.0;
    }
    return m;
}

void mem_engine_dump_visual(const char* label) {
    MemoryMetrics m = mem_engine_compute_metrics();
    printf("\n========================================================================\n");
    printf(" MEMORY LAYOUT STATE: %s\n", label);
    printf("========================================================================\n");
    printf(" Allocated: %-6zu B | Free: %-6zu B | Largest Free: %-6zu B\n", 
           m.total_allocated_bytes, m.total_free_bytes, m.largest_free_block);
    printf(" Free Blocks: %-4zu | Alloc Blocks: %-4zu | Ext Fragmentation: %.2f%%\n", 
           m.free_block_count, m.allocated_block_count, m.external_frag_percent);
    printf(" Coalesce Merges: %zu | Total Requests: [Alloc: %zu | Free: %zu]\n", 
           total_coalesce_events, total_alloc_requests, total_free_requests);
    printf("------------------------------------------------------------------------\n");
    printf(" Physical Block Segments:\n");

    BlockHeader* curr = free_list_head;
    int index = 0;
    while (curr != NULL) {
        printf("  [%02d] Offset: %-4td | Size: %-5zu B | Status: %-7s | Tag: %u\n",
               index++,
               (uint8_t*)curr - raw_memory_pool,
               curr->size,
               curr->is_free ? "FREE" : "IN-USE",
               curr->tag_id);
        curr = curr->next;
    }
    printf("========================================================================\n");
}

void run_comparative_policy_benchmark(void) {
    const char* policy_names[] = {"FIRST-FIT", "BEST-FIT", "WORST-FIT"};
    AllocPolicy policies[] = {POLICY_FIRST_FIT, POLICY_BEST_FIT, POLICY_WORST_FIT};

    printf("\n>>> RUNNING COMPARATIVE MULTI-ALGORITHM HEURISTIC SIMULATION <<<\n");
    printf("%-12s | %-10s | %-10s | %-12s | %-14s\n",
           "Algorithm", "Free (B)", "Largest(B)", "Free Blocks", "Ext Frag (%)");
    printf("--------------------------------------------------------------------\n");

    for (int p = 0; p < 3; p++) {
        mem_engine_init();
        void* pointers[16];
        size_t test_sizes[16] = {
            128, 256, 64, 512, 128, 384, 80, 200,
            64, 128, 300, 150, 96, 400, 50, 180
        };

        for (int i = 0; i < 16; i++) {
            pointers[i] = mem_engine_alloc(test_sizes[i], policies[p], 100 + i);
        }

        for (int i = 1; i < 16; i += 2) {
            mem_engine_free(pointers[i]);
            pointers[i] = NULL;
        }

        pointers[1] = mem_engine_alloc(100, policies[p], 501);
        pointers[3] = mem_engine_alloc(220, policies[p], 502);
        pointers[5] = mem_engine_alloc(60,  policies[p], 503);

        MemoryMetrics m = mem_engine_compute_metrics();
        printf("%-12s | %-10zu | %-10zu | %-12zu | %6.2f%%\n",
               policy_names[p],
               m.total_free_bytes,
               m.largest_free_block,
               m.free_block_count,
               m.external_frag_percent);
    }
    printf("--------------------------------------------------------------------\n");
}

int main(void) {
    printf("========================================================================\n");
    printf(" SENTINELNET PACKET MEMORY ENGINE (MEMBER 1: OS UNIT 4)\n");
    printf(" Architecture: 64-bit Word Aligned, Bi-directional Coalescing Doubly Linked Pool\n");
    printf(" Total Heap Budget: %d Bytes\n", HEAP_CAPACITY);
    printf("========================================================================\n");

    mem_engine_init();
    mem_engine_dump_visual("Initial State (Raw Linear Pool)");

    printf("\n--> Allocating incoming network packet buffers...\n");
    void* p1 = mem_engine_alloc(300, POLICY_FIRST_FIT, 101);
    void* p2 = mem_engine_alloc(700, POLICY_FIRST_FIT, 102);
    void* p3 = mem_engine_alloc(450, POLICY_FIRST_FIT, 103);
    void* p4 = mem_engine_alloc(900, POLICY_FIRST_FIT, 104);
    mem_engine_dump_visual("Active Workload (Packets Allocated)");

    printf("\n--> Freeing non-adjacent packet blocks #2 and #4 to trigger fragmentation...\n");
    mem_engine_free(p2);
    mem_engine_free(p4);
    mem_engine_dump_visual("Fragmented Memory Holes Present");

    printf("\n--> Freeing adjacent block #3 to trigger bidirectional coalescing...\n");
    mem_engine_free(p3);
    mem_engine_dump_visual("Post Coalesce Merging (Holes Reclaimed)");

    mem_engine_free(p1);
    mem_engine_dump_visual("All Buffers Returned to Free Pool");

    run_comparative_policy_benchmark();
    return 0;
}
