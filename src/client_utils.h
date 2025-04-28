#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include <stddef.h>

#define RECV_BUFFER_SIZE 1024

typedef struct {
    unsigned char *buffer;
    size_t capacity;
    size_t length;
} receive_buffer_t;

void error_exit(const char *msg);
int init_receive_buffer(receive_buffer_t *rb, size_t initial_capacity);
void free_receive_buffer(receive_buffer_t *rb);
int ensure_buffer_capacity(receive_buffer_t *rb, size_t required_space);

#endif