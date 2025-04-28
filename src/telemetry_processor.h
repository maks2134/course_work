#ifndef TELEMETRY_PROCESSOR_H
#define TELEMETRY_PROCESSOR_H

#include <stddef.h>
#include "telemetry.h"

ssize_t deserialize_telemetry_data(const unsigned char *buffer, size_t buffer_len, telemetry_data *data);
void print_telemetry_data(const telemetry_data *data);

#endif