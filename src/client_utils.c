#include "client_utils.h"
#include <stdio.h>  
#include <stdlib.h> 
#include <string.h> 

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int init_receive_buffer(receive_buffer_t *rb, size_t initial_capacity) {
    if (!rb) return -1;
    rb->buffer = malloc(initial_capacity);
    if (!rb->buffer) {
        perror("malloc failed for receive buffer");
        return -1;
    }
    rb->capacity = initial_capacity;
    rb->length = 0;
    printf("Initialized receive buffer with capacity %zu bytes\n", initial_capacity);
    return 0;
}

void free_receive_buffer(receive_buffer_t *rb) {
    if (rb && rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
        rb->capacity = 0;
        rb->length = 0;
        printf("Freed receive buffer\n");
    }
}

int ensure_buffer_capacity(receive_buffer_t *rb, size_t required_space) {
    if (!rb) return -1;
    if (rb->capacity - rb->length < required_space) {
        size_t new_capacity = rb->capacity;
        while (new_capacity - rb->length < required_space) {
             new_capacity = (new_capacity == 0) ? (rb->length + required_space) : new_capacity * 2;
             if (new_capacity < rb->length + required_space) {
                new_capacity = rb->length + required_space;
             }
        }
        unsigned char *new_buffer = realloc(rb->buffer, new_capacity);
        if (!new_buffer) {
            perror("realloc failed for receive buffer");
            return -1;
        }
        rb->buffer = new_buffer;
        rb->capacity = new_capacity;
        printf("Resized receive buffer to %zu bytes\n", new_capacity);
    }
    return 0;
}