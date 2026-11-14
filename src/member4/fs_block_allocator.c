#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOTAL_BLOCKS 32

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    /* 32-Block Free-Space Bitmap (0 = Free, 1 = Allocated, -1 = Bad Sector) */
    int bitmap[TOTAL_BLOCKS] = {0};
    bitmap[0] = 1; /* Superblock */
    bitmap[1] = 1; /* Inode Table */
    bitmap[14] = -1; /* Bad Block marked & spared */

    /* 1. Contiguous Allocation (4 blocks starting at block 2) */
    int contig_blocks[4] = {2, 3, 4, 5};
    for (int i = 0; i < 4; i++) bitmap[contig_blocks[i]] = 1;

    /* 2. Linked Allocation (Non-contiguous blocks chained via next-pointers) */
    int linked_blocks[3] = {7, 12, 19};
    for (int i = 0; i < 3; i++) bitmap[linked_blocks[i]] = 1;

    /* 3. Indexed Allocation (Inode #42 points to index block 20 -> data blocks [21, 24, 28]) */
    int index_block = 20;
    int indexed_data[3] = {21, 24, 28};
    bitmap[index_block] = 1;
    for (int i = 0; i < 3; i++) bitmap[indexed_data[i]] = 1;

    int free_count = 0, used_count = 0, bad_count = 0;
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (bitmap[i] == 0) free_count++;
        else if (bitmap[i] == 1) used_count++;
        else bad_count++;
    }

    if (json_mode) {
        printf("{\n"
               "  \"total_blocks\": %d,\n"
               "  \"allocated_blocks\": %d,\n"
               "  \"free_blocks\": %d,\n"
               "  \"bad_blocks_spared\": %d,\n"
               "  \"allocation_methods\": {\n"
               "    \"contiguous\": [2, 3, 4, 5],\n"
               "    \"linked_chain\": \"7 -> 12 -> 19 -> NULL\",\n"
               "    \"indexed_inode_42\": {\"index_block\": %d, \"data_blocks\": [21, 24, 28]}\n"
               "  }\n"
               "}\n",
               TOTAL_BLOCKS, used_count, free_count, bad_count, index_block);
    } else {
        printf("=== OS Unit 5: File Block Allocation & Free-Space Bitmap (Drishti) ===\n");
        printf("Disk Bitmap (32 blocks) -> Allocated: %d | Free: %d | Bad Sectors Spared: %d (Block #14)\n",
               used_count, free_count, bad_count);
        printf("  Contiguous File : Blocks [2, 3, 4, 5]\n");
        printf("  Linked File     : 7 -> 12 -> 19 -> NULL\n");
        printf("  Indexed Inode   : Index Block #20 -> [21, 24, 28]\n");
    }
    return 0;
}
