#include "telemetry_processor.h"
#include "endian_utils.h" // Для ntohll, ntohf, ntohd и т.д.

#include <stdio.h>      // Для fprintf, printf
#include <string.h>     // Для memcpy, strlen, sprintf
#include <arpa/inet.h>  // Для ntohl
#include <time.h>       // Для localtime, strftime
#include <inttypes.h>   // Для PRIu64 и других макросов форматирования

ssize_t deserialize_telemetry_data(const unsigned char *buffer, size_t buffer_len, telemetry_data *data) {
    if (buffer == NULL || data == NULL) {
        fprintf(stderr, "deserialize_telemetry: NULL pointer passed.\n");
        return -1;
    }
    if (buffer_len == 0) {
        return 0; // Недостаточно данных даже для маркера
    }

    size_t offset = 0;

    // 1. Проверяем маркер
    if (buffer[offset] != 'T') {
        fprintf(stderr, "deserialize_telemetry: Invalid marker byte 0x%02X (expected 'T'). Skipping byte.\n", buffer[offset]);
        // Возвращаем 1, чтобы основной цикл сдвинул буфер на этот некорректный байт
        return 1; // Указываем, что обработали (пропустили) 1 байт
        // Или return -1; если хотим считать ошибкой потока
    }
    offset += sizeof(char);

    // Минимальный размер заголовка после маркера ('T')
    const size_t MIN_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint64_t);
    if (buffer_len < offset + MIN_HEADER_SIZE) {
        // fprintf(stderr, "deserialize_telemetry: Not enough data for header (%zu < %zu)\n", buffer_len, offset + header_size_no_marker);
        return 0; // Недостаточно данных для заголовка
    }

    // 2. ID
    uint32_t net_id;
    memcpy(&net_id, buffer + offset, sizeof(net_id));
    data->id = ntohl(net_id); // Используем стандартный ntohl
    offset += sizeof(net_id);

    // 3. Type
    uint8_t type_byte;
    memcpy(&type_byte, buffer + offset, sizeof(type_byte));
    // Проверка на допустимость типа (можно добавить)
    if (type_byte > DATA_TYPE_STATUS) { // Пример проверки
         fprintf(stderr, "deserialize_telemetry: Invalid data type %d for source %d\n", type_byte, data->id);
         return -1; // Ошибка: недопустимый тип
    }
    data->type = (telemetry_data_type)type_byte;
    offset += sizeof(type_byte);

    // 4. Timestamp
    uint64_t net_timestamp;
    memcpy(&net_timestamp, buffer + offset, sizeof(net_timestamp));
    data->timestamp_ms = (long long)ntohll(net_timestamp); // Используем наш ntohll
    offset += sizeof(net_timestamp);

    // 5. Value (определяем необходимый размер и проверяем наличие данных)
    size_t required_payload_size = 0;
    switch (data->type) {
        case DATA_TYPE_TEMPERATURE:
        case DATA_TYPE_PRESSURE:
        case DATA_TYPE_HUMIDITY:
            required_payload_size = sizeof(uint32_t); // Размер float как uint32_t
            break;
        case DATA_TYPE_GPS:
            required_payload_size = 2 * sizeof(uint64_t); // Размер двух double как uint64_t
            break;
        case DATA_TYPE_STATUS:
            required_payload_size = sizeof(data->value.status); // Размер массива char
            break;
        // default уже обработан проверкой выше
    }

    if (buffer_len < offset + required_payload_size) {
       // fprintf(stderr, "deserialize_telemetry: Not enough data for payload. Required: %zu, Available: %zu\n", required_payload_size, buffer_len - offset);
        return 0; // Недостаточно данных для значения
    }

    // Читаем значение
    switch (data->type) {
        case DATA_TYPE_TEMPERATURE: {
            uint32_t net_float;
            memcpy(&net_float, buffer + offset, sizeof(net_float));
            data->value.temperature = ntohf(net_float); // Наш ntohf
            offset += sizeof(net_float);
            break;
        }
         case DATA_TYPE_PRESSURE: {
            uint32_t net_float;
            memcpy(&net_float, buffer + offset, sizeof(net_float));
            data->value.pressure = ntohf(net_float); // Наш ntohf
            offset += sizeof(net_float);
            break;
        }
         case DATA_TYPE_HUMIDITY: {
            uint32_t net_float;
            memcpy(&net_float, buffer + offset, sizeof(net_float));
            data->value.humidity = ntohf(net_float); // Наш ntohf
            offset += sizeof(net_float);
            break;
        }
        case DATA_TYPE_GPS: {
            uint64_t net_lat, net_lon;
            memcpy(&net_lat, buffer + offset, sizeof(net_lat));
            offset += sizeof(net_lat);
            memcpy(&net_lon, buffer + offset, sizeof(net_lon));
            offset += sizeof(net_lon);
            data->value.gps.latitude = ntohd(net_lat); // Наш ntohd
            data->value.gps.longitude = ntohd(net_lon); // Наш ntohd
            break;
        }
        case DATA_TYPE_STATUS: {
            memcpy(data->value.status, buffer + offset, sizeof(data->value.status));
            // Убедимся, что строка нуль-терминирована (сервер должен это делать, но для надежности)
            data->value.status[sizeof(data->value.status) - 1] = '\0';
            offset += sizeof(data->value.status);
            break;
        }
         // default не нужен, тип проверен ранее
    }

    return (ssize_t)offset; // Возвращаем общее количество обработанных байт для этого сообщения
}

void print_telemetry_data(const telemetry_data *data) {
    if (!data) return;

    // Преобразование timestamp_ms в читаемый формат
    time_t sec = data->timestamp_ms / 1000;
    long ms = data->timestamp_ms % 1000;
    struct tm *tm_info;
    char time_buffer[30]; // Буфер для времени "YYYY-MM-DD HH:MM:SS.mmm"

    // Используем потокобезопасную версию localtime_r, если доступна, или localtime
    // struct tm result_tm; // для localtime_r
    // tm_info = localtime_r(&sec, &result_tm);
    tm_info = localtime(&sec); // Простая версия

    if (tm_info) {
        strftime(time_buffer, sizeof(time_buffer) - 4, "%Y-%m-%d %H:%M:%S", tm_info); // Оставляем место для ".ms\0"
        snprintf(time_buffer + strlen(time_buffer), 5, ".%03ld", ms); // Добавляем миллисекунды
    } else {
        snprintf(time_buffer, sizeof(time_buffer), "%lld ms", data->timestamp_ms); // Если время не парсится
    }


    printf("ID: %4d | Type: %d | Time: %s | Value: ",
           data->id, data->type, time_buffer);

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
            // Печатаем строку в кавычках, чтобы видеть пробелы, если они есть
            printf("Status = '%s'\n", data->value.status);
            break;
        // default не нужен, тип проверен при десериализации
    }
}