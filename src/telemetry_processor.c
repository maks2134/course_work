#include "telemetry_processor.h"
#include "endian_utils.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <inttypes.h>

ssize_t deserialize_telemetry_data(const unsigned char *buffer, size_t buffer_len, telemetry_data *data) {
    if (buffer == NULL || data == NULL) {
        fprintf(stderr, "deserialize_telemetry: NULL pointer passed.\n");
        return -1;
    }
    if (buffer_len == 0) {
        return 0;
    }

    size_t offset = 0;

    if (buffer[offset] != 'T') {
        fprintf(stderr, "deserialize_telemetry: Invalid marker byte 0x%02X (expected 'T'). Skipping byte.\n", buffer[offset]);
        return 1;
    }
    offset += sizeof(char);

    const size_t MIN_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint64_t);
    if (buffer_len < offset + MIN_HEADER_SIZE) {
        return 0;
    }

    uint32_t net_id;
    memcpy(&net_id, buffer + offset, sizeof(net_id));
    data->id = ntohl(net_id);
    offset += sizeof(net_id);

    uint8_t type_byte;
    memcpy(&type_byte, buffer + offset, sizeof(type_byte));
    if (type_byte > DATA_TYPE_STATUS) {
        fprintf(stderr, "deserialize_telemetry: Invalid data type %d for source %d\n", type_byte, data->id);
        return -1;
    }
    data->type = (telemetry_data_type)type_byte;
    offset += sizeof(type_byte);

    uint64_t net_timestamp;
    memcpy(&net_timestamp, buffer + offset, sizeof(net_timestamp));
    data->timestamp_ms = (long long)ntohll(net_timestamp);
    offset += sizeof(net_timestamp);

    size_t required_payload_size = 0;
    switch (data->type) {
        case DATA_TYPE_TEMPERATURE:
        case DATA_TYPE_PRESSURE:
        case DATA_TYPE_HUMIDITY:
            required_payload_size = sizeof(uint32_t);
            break;
        case DATA_TYPE_GPS:
            required_payload_size = 2 * sizeof(uint64_t);
            break;
        case DATA_TYPE_STATUS:
            required_payload_size = sizeof(data->value.status);
            break;
    }

    if (buffer_len < offset + required_payload_size) {
        return 0;
    }

    switch (data->type) {
        case DATA_TYPE_TEMPERATURE: {
            uint32_t net_float;
            memcpy(&net_float, buffer + offset, sizeof(net_float));
            data->value.temperature = ntohf(net_float);
            offset += sizeof(net_float);
            break;
        }
        case DATA_TYPE_PRESSURE: {
            uint32_t net_float;
            memcpy(&net_float, buffer + offset, sizeof(net_float));
            data->value.pressure = ntohf(net_float);
            offset += sizeof(net_float);
            break;
        }
        case DATA_TYPE_HUMIDITY: {
            uint32_t net_float;
            memcpy(&net_float, buffer + offset, sizeof(net_float));
            data->value.humidity = ntohf(net_float);
            offset += sizeof(net_float);
            break;
        }
        case DATA_TYPE_GPS: {
            uint64_t net_lat, net_lon;
            memcpy(&net_lat, buffer + offset, sizeof(net_lat));
            offset += sizeof(net_lat);
            memcpy(&net_lon, buffer + offset, sizeof(net_lon));
            offset += sizeof(net_lon);
            data->value.gps.latitude = ntohd(net_lat);
            data->value.gps.longitude = ntohd(net_lon);
            break;
        }
        case DATA_TYPE_STATUS: {
            memcpy(data->value.status, buffer + offset, sizeof(data->value.status));
            data->value.status[sizeof(data->value.status) - 1] = '\0';
            offset += sizeof(data->value.status);
            break;
        }
    }

    return (ssize_t)offset;
}

void print_telemetry_data(const telemetry_data *data) {
    if (!data) return;

    time_t sec = data->timestamp_ms / 1000;
    long ms = data->timestamp_ms % 1000;
    struct tm *tm_info;
    char time_buffer[30];

    tm_info = localtime(&sec);

    if (tm_info) {
        strftime(time_buffer, sizeof(time_buffer) - 4, "%Y-%m-%d %H:%M:%S", tm_info);
        snprintf(time_buffer + strlen(time_buffer), 5, ".%03ld", ms);
    } else {
        snprintf(time_buffer, sizeof(time_buffer), "%lld ms", data->timestamp_ms);
    }

    printf("ID: %4d | Type: %d | Time: %s | Value: ", data->id, data->type, time_buffer);

    switch (data->type) {
        case DATA_TYPE_TEMPERATURE:
            printf("Temp = %.2f C\n", data->value.temperature);
            break;
        case DATA_TYPE_PRESSURE:
            printf("Press = %.2f hPa\n", data->value.pressure);
            break;
        case DATA_TYPE_HUMIDITY:
            printf("Hum = %.1f %%\n", data->value.humidity);
            break;
        case DATA_TYPE_GPS:
            printf("GPS = (%.6f, %.6f)\n", data->value.gps.latitude, data->value.gps.longitude);
            break;
        case DATA_TYPE_STATUS:
            printf("Status = '%s'\n", data->value.status);
            break;
    }
}