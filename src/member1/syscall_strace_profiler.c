#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/resource.h>

static volatile sig_atomic_t reload_flag = 0;

typedef struct {
    int max_pps_threshold;
    int alert_payload_bytes;
    char capture_iface[32];
} SentinelConfig;

static void handle_sighup(int sig) {
    (void)sig;
    reload_flag = 1;
}

static void load_config(const char *path, SentinelConfig *cfg) {
    cfg->max_pps_threshold = 5000;
    cfg->alert_payload_bytes = 1400;
    strcpy(cfg->capture_iface, "eth0");

    FILE *fp = fopen(path, "r");
    if (!fp) {
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "# Edit this file in vi and send SIGHUP to hot-reload\n");
            fprintf(fp, "MAX_PPS=5000\nALERT_BYTES=1400\nINTERFACE=eth0\n");
            fclose(fp);
        }
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (strncmp(line, "MAX_PPS=", 8) == 0) cfg->max_pps_threshold = atoi(line + 8);
        else if (strncmp(line, "ALERT_BYTES=", 12) == 0) cfg->alert_payload_bytes = atoi(line + 12);
        else if (strncmp(line, "INTERFACE=", 10) == 0) {
            sscanf(line + 10, "%31s", cfg->capture_iface);
        }
    }
    fclose(fp);
}

static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);
    const char *cfg_path = "config/sentinel.conf";

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, NULL);

    SentinelConfig cfg;
    load_config(cfg_path, &cfg);

    raise(SIGHUP);
    if (reload_flag) {
        load_config(cfg_path, &cfg);
        reload_flag = 0;
    }

    double t0 = now_us();
    int fd = open("data/.syscall_probe.tmp", O_CREAT | O_RDWR | O_TRUNC, 0644);
    char buf[512];
    memset(buf, 'S', sizeof(buf));
    for (int i = 0; i < 200; i++) {
        if (write(fd, buf, sizeof(buf)) < 0) break;
    }
    lseek(fd, 0, SEEK_SET);
    for (int i = 0; i < 200; i++) {
        if (read(fd, buf, sizeof(buf)) <= 0) break;
    }
    close(fd);
    unlink("data/.syscall_probe.tmp");
    double file_io_us = now_us() - t0;

    struct utsname sysinfo;
    uname(&sysinfo);
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);

    if (json_mode) {
        printf("{\n"
               "  \"pid\": %d,\n"
               "  \"kernel\": \"%s %s\",\n"
               "  \"config\": {\"max_pps\": %d, \"alert_bytes\": %d, \"interface\": \"%s\", \"sighup_reloaded\": true},\n"
               "  \"syscall_latency_us\": %.2f,\n"
               "  \"voluntary_ctx_switches\": %ld,\n"
               "  \"involuntary_ctx_switches\": %ld,\n"
               "  \"syscall_catalog\": [\n"
               "    {\"category\": \"Process Control\", \"calls\": \"fork, execve, waitpid, exit, sigaction\"},\n"
               "    {\"category\": \"File Management\", \"calls\": \"open, read, write, lseek, close, unlink\"},\n"
               "    {\"category\": \"Device Management\", \"calls\": \"ioctl, mmap, munmap, fsync\"},\n"
               "    {\"category\": \"Info Maintenance\", \"calls\": \"getpid, uname, clock_gettime, getrusage\"},\n"
               "    {\"category\": \"Communication\", \"calls\": \"pipe, mkfifo, shm_open, msgsnd, msgrcv\"}\n"
               "  ]\n"
               "}\n",
               getpid(), sysinfo.sysname, sysinfo.release,
               cfg.max_pps_threshold, cfg.alert_payload_bytes, cfg.capture_iface,
               file_io_us, ru.ru_nvcsw, ru.ru_nivcsw);
    } else {
        printf("=== OS Unit 1: System Call Profiler & vi SIGHUP Config ===\n");
        printf("PID: %d | Kernel: %s %s\n", getpid(), sysinfo.sysname, sysinfo.release);
        printf("Loaded Config (%s): MAX_PPS=%d, ALERT_BYTES=%d, IFACE=%s\n",
               cfg_path, cfg.max_pps_threshold, cfg.alert_payload_bytes, cfg.capture_iface);
        printf("400x read/write/lseek Syscall Burst Latency: %.2f us\n", file_io_us);
        printf("Context Switches -> Voluntary: %ld | Involuntary: %ld\n", ru.ru_nvcsw, ru.ru_nivcsw);
    }
    return 0;
}
