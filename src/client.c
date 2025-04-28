#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       // для close, read, write (хотя используем recv)
#include <sys/socket.h>   // для socket, connect, recv
#include <netinet/in.h>   // для sockaddr_in
#include <arpa/inet.h>    // для inet_pton, htons, ntohs
#include <errno.h>        // для errno

// Включаем наши заголовочные файлы
#include "client_utils.h"
#include "telemetry_processor.h"
// telemetry.h и endian_utils.h включаются через telemetry_processor.h и client_utils.h

#define SERVER_PORT 8080       // Порт сервера по умолчанию
#define SERVER_IP "127.0.0.1"  // IP сервера по умолчанию

int main(int argc, char *argv[]) {
    int sockfd = -1;
    struct sockaddr_in server_addr;
    const char *server_ip = SERVER_IP;
    int server_port = SERVER_PORT;
    receive_buffer_t recv_buf; // Буфер для приема данных

    // Парсинг аргументов командной строки (если есть)
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

    // Инициализация буфера приема
    if (init_receive_buffer(&recv_buf, RECV_BUFFER_SIZE) != 0) {
        // Сообщение об ошибке выводится внутри init_receive_buffer
        exit(EXIT_FAILURE);
    }

    // Создание сокета
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        free_receive_buffer(&recv_buf); // Очищаем буфер перед выходом
        error_exit("socket creation failed");
    }

    // Настройка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP address: %s\n", server_ip);
        close(sockfd);
        free_receive_buffer(&recv_buf);
        exit(EXIT_FAILURE);
    }

    // Подключение к серверу
    printf("Connecting to %s:%d...\n", server_ip, server_port);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        free_receive_buffer(&recv_buf);
        error_exit("connect failed");
    }
    printf("Connected successfully. Waiting for data...\n");

    // Основной цикл приема и обработки данных
    while (1) {
        // Убедимся, что в буфере есть место для чтения хотя бы одного байта
        // (recv может прочитать меньше, чем запрошено)
        // RECV_BUFFER_SIZE - просто разумный размер для чтения за раз
        if (ensure_buffer_capacity(&recv_buf, RECV_BUFFER_SIZE) != 0) {
            fprintf(stderr, "Failed to ensure buffer capacity. Exiting.\n");
            break; // Ошибка выделения памяти
        }

        // Читаем данные из сокета в свободное место буфера
        ssize_t bytes_received = recv(sockfd, recv_buf.buffer + recv_buf.length, recv_buf.capacity - recv_buf.length, 0);

        if (bytes_received < 0) {
            if (errno == EINTR) {
                continue; // Прервано сигналом, просто повторяем чтение
            }
            perror("recv failed");
            break; // Невосстановимая ошибка чтения
        } else if (bytes_received == 0) {
            printf("Server closed the connection.\n");
            break; // Сервер закрыл соединение штатно
        } else {
            // printf("Received %zd bytes\n", bytes_received);
            recv_buf.length += bytes_received; // Увеличиваем количество данных в буфере

            // Обрабатываем все полные сообщения, которые могли накопиться в буфере
            size_t processed_in_cycle = 0; // Сколько байт обработали в текущей итерации внутреннего цикла
            while (recv_buf.length > processed_in_cycle) { // Пока есть необработанные данные
                telemetry_data current_data; // Объявление переменной для текущего сообщения
                const unsigned char *data_start = recv_buf.buffer + processed_in_cycle;
                const size_t data_len = recv_buf.length - processed_in_cycle;

                // Передаем АДРЕС переменной current_data в функцию десериализации
                ssize_t processed_now = deserialize_telemetry_data(data_start, data_len, &current_data);

                if (processed_now > 0) {
                    // Успешно разобрали сообщение
                    // Передаем АДРЕС переменной current_data в функцию печати
                    print_telemetry_data(&current_data);
                    processed_in_cycle += processed_now; // Увеличиваем счетчик обработанных байт
                } else if (processed_now == 0) {
                    // Данных в буфере недостаточно для полного сообщения, выходим из внутреннего цикла
                    // и ждем следующего recv()
                    // printf("Waiting for more data...\n");
                    break;
                } else { // processed_now < 0 (ошибка или пропуск байта)
                    // Ошибка разбора (-1) или пропуск байта (1 при ошибке маркера)
                    fprintf(stderr, "Error or skip during deserialization. Advancing buffer by 1 byte.\n");
                    // Сдвигаемся на 1 байт, чтобы попытаться найти следующее сообщение
                    processed_in_cycle += 1; // Всегда сдвигаем на 1 при ошибке/пропуске
                    // break; // Можно разорвать цикл при невосстановимой ошибке
                }
            }

            // Если мы обработали какие-то данные, сдвигаем остаток буфера в начало
            if (processed_in_cycle > 0) {
                if (processed_in_cycle < recv_buf.length) {
                    // Есть остаток данных, который не образовал полного сообщения
                    memmove(recv_buf.buffer, recv_buf.buffer + processed_in_cycle, recv_buf.length - processed_in_cycle);
                }
                recv_buf.length -= processed_in_cycle; // Уменьшаем размер данных в буфере
            }
        }
    }

    // Завершение работы
    printf("Closing connection.\n");
    if (sockfd >= 0) {
        close(sockfd);
    }
    free_receive_buffer(&recv_buf); // Освобождаем память буфера

    return 0;
}