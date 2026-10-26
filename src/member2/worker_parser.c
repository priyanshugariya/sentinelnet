#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define FIFO_IN_PATH "/tmp/sentinel_pipe"
#define MAX_PAYLOAD_LEN 1500
#define PROTO_TCP 6
#define PROTO_UDP 17
#define PROTO_ICMP 1

typedef struct {
    uint32_t magic_header;
    uint32_t packet_seq;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
    uint8_t flags;
    uint16_t payload_len;
    uint32_t checksum;
    char payload[MAX_PAYLOAD_LEN];
} __attribute__((packed)) WirePacketFrame;

typedef struct {
    uint64_t total_frames_parsed;
    uint64_t total_bytes_processed;
    uint64_t tcp_packets;
    uint64_t udp_packets;
    uint64_t icmp_packets;
    uint64_t corrupted_checksums;
    uint64_t malformed_headers;
} WorkerTelemetry;

static volatile bool keep_running = true;
static WorkerTelemetry telemetry = {0};

static void handle_sigint(int sig) {
    (void)sig;
    keep_running = false;
}

static uint32_t compute_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

static void ip_to_string(uint32_t ip, char *buffer, size_t len) {
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    const char *res = inet_ntop(AF_INET, &addr, buffer, len);
    if (!res) {
        snprintf(buffer, len, "0.0.0.0");
    }
}

static const char *resolve_protocol_label(uint8_t proto) {
    switch (proto) {
        case PROTO_TCP:  return "TCP";
        case PROTO_UDP:  return "UDP";
        case PROTO_ICMP: return "ICMP";
        default:         return "RAW/UNKNOWN";
    }
}

static bool validate_and_process_frame(const WirePacketFrame *frame) {
    if (frame->magic_header != 0x534E4554) { // "SNET" signature
        telemetry.malformed_headers++;
        return false;
    }

    uint32_t expected_crc = compute_crc32((const uint8_t *)frame->payload, frame->payload_len);
    if (frame->checksum != 0 && frame->checksum != expected_crc) {
        telemetry.corrupted_checksums++;
        return false;
    }

    telemetry.total_frames_parsed++;
    telemetry.total_bytes_processed += sizeof(WirePacketFrame) + frame->payload_len;

    switch (frame->protocol) {
        case PROTO_TCP:  telemetry.tcp_packets++; break;
        case PROTO_UDP:  telemetry.udp_packets++; break;
        case PROTO_ICMP: telemetry.icmp_packets++; break;
        default: break;
    }
    return true;
}

static void dump_worker_report(void) {
    printf("\n====================================================================\n");
    printf(" SENTINELNET WORKER PARSER ENGINE METRICS (MEMBER 2: OS UNIT 2/IPC)\n");
    printf("====================================================================\n");
    printf(" Total Frames Parsed    : %lu\n", telemetry.total_frames_parsed);
    printf(" Ingested Data Volume   : %.2f KB\n", (double)telemetry.total_bytes_processed / 1024.0);
    printf(" Transport Breakdown    : TCP=%lu | UDP=%lu | ICMP=%lu\n",
           telemetry.tcp_packets, telemetry.udp_packets, telemetry.icmp_packets);
    printf(" Corrupted Checksums    : %lu (Dropped)\n", telemetry.corrupted_checksums);
    printf(" Malformed Signatures   : %lu (Dropped)\n", telemetry.malformed_headers);
    printf(" Operational State      : HEALTHY\n");
    printf("====================================================================\n");
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    printf("====================================================================\n");
    printf(" SENTINELNET HIGH-THROUGHPUT IPC FRAME PARSER (MEMBER 2)\n");
    printf(" Listening on POSIX FIFO Pipe: %s\n", FIFO_IN_PATH);
    printf("====================================================================\n");

    // Standalone benchmark test when run without pipe
    if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
        printf("--> Running parsing verification across 2,000 synthetic frames...\n");
        for (uint32_t i = 1; i <= 2000; i++) {
            WirePacketFrame test_frame;
            memset(&test_frame, 0, sizeof(test_frame));
            test_frame.magic_header = 0x534E4554;
            test_frame.packet_seq = i;
            test_frame.src_ip = 0x0A000001 + (i % 254);
            test_frame.dst_ip = 0xC0A80101;
            test_frame.src_port = 1024 + (i % 50000);
            test_frame.dst_port = 80;
            test_frame.protocol = (i % 3 == 0) ? PROTO_TCP : ((i % 3 == 1) ? PROTO_UDP : PROTO_ICMP);
            test_frame.payload_len = 64;
            snprintf(test_frame.payload, sizeof(test_frame.payload), "SYNTHETIC_TELEMETRY_PAYLOAD_BURST_%08u", i);
            test_frame.checksum = compute_crc32((const uint8_t *)test_frame.payload, test_frame.payload_len);

            validate_and_process_frame(&test_frame);
        }
        dump_worker_report();
        return 0;
    }

    mkfifo(FIFO_IN_PATH, 0666);
    int fd = open(FIFO_IN_PATH, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("[-] Failed to initialize FIFO endpoint");
        return 1;
    }

    WirePacketFrame frame;
    char src_ip_str[INET_ADDRSTRLEN];
    char dst_ip_str[INET_ADDRSTRLEN];

    while (keep_running) {
        ssize_t bytes_read = read(fd, &frame, sizeof(WirePacketFrame));
        if (bytes_read == sizeof(WirePacketFrame)) {
            if (validate_and_process_frame(&frame)) {
                ip_to_string(frame.src_ip, src_ip_str, sizeof(src_ip_str));
                ip_to_string(frame.dst_ip, dst_ip_str, sizeof(dst_ip_str));
                if (telemetry.total_frames_parsed % 200 == 0) {
                    printf("[PARSER] Seq #%-6u | %-4s | %s:%-5u -> %s:%-5u | Len: %-4u B\n",
                           frame.packet_seq, resolve_protocol_label(frame.protocol),
                           src_ip_str, frame.src_port, dst_ip_str, frame.dst_port, frame.payload_len);
                }
            }
        } else {
            usleep(10000); // 10ms idle sleep when pipe empty
        }
    }

    close(fd);
    dump_worker_report();
    return 0;
}
