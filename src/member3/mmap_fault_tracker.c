#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#define MMAP_LOG_FILE "data/sentinel_mmap_audit.bin"
#define MMAP_POOL_PAGES 512
#define MMAP_PAGE_SIZE 4096
#define MMAP_TOTAL_BYTES (MMAP_POOL_PAGES * MMAP_PAGE_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t record_seq;
    uint64_t timestamp_ns;
    char payload_digest[64];
    uint8_t flags;
} __attribute__((packed)) MmapLogHeader;

typedef struct {
    long minor_faults;
    long major_faults;
    long user_time_us;
    long sys_time_us;
} KernelFaultSnapshot;

static KernelFaultSnapshot sample_kernel_usage(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    KernelFaultSnapshot s;
    s.minor_faults = ru.ru_minflt;
    s.major_faults = ru.ru_majflt;
    s.user_time_us = (ru.ru_utime.tv_sec * 1000000L) + ru.ru_utime.tv_usec;
    s.sys_time_us  = (ru.ru_stime.tv_sec * 1000000L) + ru.ru_stime.tv_usec;
    return s;
}

static void benchmark_random_access_paging(char *mapped_region) {
    printf("--> Executing non-sequential random page access probe (Locality of Reference Test)...\n");
    KernelFaultSnapshot pre = sample_kernel_usage();

    // Accessing 1,000 randomized pages across the virtual arena
    srand(42);
    for (int i = 0; i < 1000; i++) {
        int page_idx = rand() % MMAP_POOL_PAGES;
        size_t offset = (size_t)page_idx * MMAP_PAGE_SIZE;
        volatile char test_byte = mapped_region[offset];
        (void)test_byte;
        mapped_region[offset] = (char)(i % 127);
    }

    KernelFaultSnapshot post = sample_kernel_usage();
    printf("\nAdditional Minor Faults in Random Probe: %ld\n", post.minor_faults - pre.minor_faults);
    printf("\nAdditional Major Faults in Random Probe: %ld\n", post.major_faults - pre.major_faults);
}

static void test_memory_protection_boundaries(int fd) {
    printf("\nTesting POSIX mprotect boundary constraints\n");
    char *ro_region = mmap(NULL, MMAP_PAGE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (ro_region != MAP_FAILED) {
        printf("\nPROT_READ page mapped successfully at %p\n", (void *)ro_region);
        if (mprotect(ro_region, MMAP_PAGE_SIZE, PROT_READ | PROT_WRITE) == 0) {
            printf("\nmprotect dynamic privilege promotion: GRANTED (PROT_READ -> PROT_WRITE)\n");
        }
        munmap(ro_region, MMAP_PAGE_SIZE);
    }
}

int main(void) {;
    printf(" SENTINELNET  Mapping & demand paging engine\n");
    printf(" Target Arena: %s (%d Pages = %d KB)\n", MMAP_LOG_FILE, MMAP_POOL_PAGES, MMAP_TOTAL_BYTES / 1024);

    int fd = open(MMAP_LOG_FILE, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("File creation failed");
        return 1;
    }

    if (ftruncate(fd, MMAP_TOTAL_BYTES) != 0) {
        perror("File truncate failed");
        close(fd);
        return 1;
    }

    KernelFaultSnapshot snap_before = sample_kernel_usage();

    char *mapped_region = mmap(NULL, MMAP_TOTAL_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_region == MAP_FAILED) {
        perror("MMAP allocation failed");
        close(fd);
        return 1;
    }

    printf("\nRegion successfully mapped at virtual base: %p\n", (void *)mapped_region);
    printf("\nWriting structured audit log records to trigger demand paging...\n");

    for (int p = 0; p < MMAP_POOL_PAGES; p++) {
        size_t offset = (size_t)p * MMAP_PAGE_SIZE;
        MmapLogHeader *hdr = (MmapLogHeader *)(mapped_region + offset);
        hdr->magic = 0x534E4554;
        hdr->record_seq = p + 1;
        hdr->timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;
        hdr->flags = 0x01;
        snprintf(hdr->payload_digest,sizeof(hdr->payload_digest),"PAGE_%d",p);
    }

    if (msync(mapped_region, MMAP_TOTAL_BYTES, MS_SYNC) != 0) {
        perror("msync synchronization failed");
    }

    KernelFaultSnapshot snap_after = sample_kernel_usage();
    long delta_minor = snap_after.minor_faults - snap_before.minor_faults;
    long delta_major = snap_after.major_faults - snap_before.major_faults;
    long delta_sys_us = snap_after.sys_time_us - snap_before.sys_time_us;

    printf("\n--- Paging Statistics ---\n");
    printf(" Minor Page Faults (Serviced from RAM) : %ld\n", delta_minor);
    printf(" Major Page Faults (Read from Disk)    : %ld\n", delta_major);
    printf(" Kernel Syscall Execution Time        : %ld us\n", delta_sys_us);
    printf(" Paging Performance Efficiency         : %.2f KB/minor fault\n",
           delta_minor > 0 ? (double)(MMAP_TOTAL_BYTES / 1024) / delta_minor : 0.0);

    benchmark_random_access_paging(mapped_region);
    test_memory_protection_boundaries(fd);

    munmap(mapped_region, MMAP_TOTAL_BYTES);
    close(fd);
    return 0;
}
