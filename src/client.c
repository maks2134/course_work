#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "client_utils.h"
#include "telemetry_processor.h"

#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"

int main(int argc, char *argv[]) {
    int sockfd = -1;
    struct sockaddr_in server_addr;
    const char *server_ip = SERVER_IP;
    int server_port = SERVER_PORT;
    receive_buffer_t recv_buf;

    if (argc > 1) {
        server_ip = argv[1];
    }
    if (argc > 2) {
        server_port = atoi(argv[2]);
        if (server_port <= 0 || server_port > 65535) {
            fprintf(stderr, "Invalid port number: %s. Using default %d.\n", argv[2], SERVER_PORT);
            server_port = SERVER_PORT;
        }
    }

    if (init_receive_buffer(&recv_buf, RECV_BUFFER_SIZE) != 0) {
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        free_receive_buffer(&recv_buf);
        error_exit("socket creation failed");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address: %s\n", server_ip);
        close(sockfd);
        free_receive_buffer(&recv_buf);
        exit(EXIT_FAILURE);
    }

    printf("Connecting to %s:%d...\n", server_ip, server_port);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        free_receive_buffer(&recv_buf);
        error_exit("connect failed");
    }
    printf("Connected successfully. Waiting for data...\n");

    while (1) {
        if (ensure_buffer_capacity(&recv_buf, RECV_BUFFER_SIZE) != 0) {
            fprintf(stderr, "Failed to ensure buffer capacity. Exiting.\n");
            break;
        }

        ssize_t bytes_received = recv(sockfd, recv_buf.buffer + recv_buf.length, recv_buf.capacity - recv_buf.length, 0);
        if (bytes_received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv failed");
            break;
        } else if (bytes_received == 0) {
            printf("Server closed the connection.\n");
            break;
        } else {
            recv_buf.length += bytes_received;

            size_t processed_in_cycle = 0;
            while (recv_buf.length > processed_in_cycle) {
                telemetry_data current_data;
                const unsigned char *data_start = recv_buf.buffer + processed_in_cycle;
                const size_t data_len = recv_buf.length - processed_in_cycle;

                ssize_t processed_now = deserialize_telemetry_data(data_start, data_len, &current_data);
                if (processed_now > 0) {
                    print_telemetry_data(&current_data);
                    processed_in_cycle += processed_now;
                } else if (processed_now == 0) {
                    break;
                } else {
                    fprintf(stderr, "Error or skip during deserialization. Advancing buffer by 1 byte.\n");
                    processed_in_cycle += 1;
                }
            }

            if (processed_in_cycle > 0) {
                if (processed_in_cycle < recv_buf.length) {
                    memmove(recv_buf.buffer, recv_buf.buffer + processed_in_cycle, recv_buf.length - processed_in_cycle);
                }
                recv_buf.length -= processed_in_cycle;
            }
        }
    }

    printf("Closing connection.\n");
    if (sockfd >= 0) {
        close(sockfd);
    }
    free_receive_buffer(&recv_buf);

    return 0;
}