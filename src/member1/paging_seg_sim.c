#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 16
#define TLB_SIZE 4

typedef struct {
    int vpn;
    int pfn;
    int valid;
} TLBEntry;

typedef struct {
    const char *name;
    unsigned int base;
    unsigned int limit;
} SegmentDescriptor;

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    int page_table[NUM_PAGES] = {5, 9, 2, 14, 1, 7, 11, 3, 15, 4, 8, 12, 0, 6, 10, 13};
    TLBEntry tlb[TLB_SIZE] = {{0}};
    int tlb_ptr = 0, tlb_hits = 0, tlb_misses = 0;

    unsigned int vaddrs[] = {0x0010, 0x0040, 0x1020, 0x0080, 0x2010, 0x1050, 0x3004, 0x0090, 0x4012, 0x2088};
    int n_addrs = sizeof(vaddrs) / sizeof(vaddrs[0]);

    for (int i = 0; i < n_addrs; i++) {
        int vpn = vaddrs[i] / PAGE_SIZE;
        int hit = 0;
        for (int j = 0; j < TLB_SIZE; j++) {
            if (tlb[j].valid && tlb[j].vpn == vpn) {
                hit = 1;
                break;
            }
        }
        if (hit) {
            tlb_hits++;
        } else {
            tlb_misses++;
            tlb[tlb_ptr].vpn = vpn;
            tlb[tlb_ptr].pfn = page_table[vpn];
            tlb[tlb_ptr].valid = 1;
            tlb_ptr = (tlb_ptr + 1) % TLB_SIZE;
        }
    }

    SegmentDescriptor seg_table[3] = {
        {"Packet_Header",   0x1000, 64},
        {"Packet_Payload",  0x2000, 1500},
        {"Audit_Metadata",  0x4000, 256}
    };

    struct { int seg; unsigned int offset; } seg_tests[] = {
        {0, 40},
        {1, 1420},
        {0, 128},
        {2, 200}
    };

    int seg_valid = 0, seg_faults = 0;
    for (int i = 0; i < 4; i++) {
        if (seg_tests[i].offset < seg_table[seg_tests[i].seg].limit) seg_valid++;
        else seg_faults++;
    }

    double hit_ratio = (100.0 * tlb_hits) / n_addrs;

    if (json_mode) {
        printf("{\n"
               "  \"page_size_bytes\": %d,\n"
               "  \"tlb_entries\": %d,\n"
               "  \"tlb_hits\": %d,\n"
               "  \"tlb_misses\": %d,\n"
               "  \"tlb_hit_ratio_pct\": %.1f,\n"
               "  \"segments\": [\n"
               "    {\"id\": 0, \"name\": \"Packet_Header\", \"base\": \"0x1000\", \"limit\": 64},\n"
               "    {\"id\": 1, \"name\": \"Packet_Payload\", \"base\": \"0x2000\", \"limit\": 1500},\n"
               "    {\"id\": 2, \"name\": \"Audit_Metadata\", \"base\": \"0x4000\", \"limit\": 256}\n"
               "  ],\n"
               "  \"segmentation_checks\": {\"valid_translations\": %d, \"segfaults_blocked\": %d}\n"
               "}\n",
               PAGE_SIZE, TLB_SIZE, tlb_hits, tlb_misses, hit_ratio, seg_valid, seg_faults);
    } else {
        printf("=== OS Unit 4: Paging + TLB & Segmentation Bounds ===\n");
        printf("TLB Lookups: %d | Hits: %d | Misses: %d | Hit Ratio: %.1f%%\n",
               n_addrs, tlb_hits, tlb_misses, hit_ratio);
        printf("Segmentation Checks: %d Valid | %d Bounds Violations Blocked (SIGSEGV prevented)\n",
               seg_valid, seg_faults);
    }
    return 0;
}
