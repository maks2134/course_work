#include "endian_utils.h"
#include <string.h> // для memcpy
#include <stdint.h>

// Определяем стандартные макросы порядка байтов, если они не определены
// (Это больше для совместимости, на современных системах они должны быть)
#ifndef __ORDER_LITTLE_ENDIAN__
#define __ORDER_LITTLE_ENDIAN__ 1234
#endif
#ifndef __ORDER_BIG_ENDIAN__
#define __ORDER_BIG_ENDIAN__    4321
#endif

// Определяем текущий порядок байтов, если он не задан компилятором
#ifndef __BYTE_ORDER__
    #warning "__BYTE_ORDER__ not pre-defined, detecting manually."
    // Простой способ определения порядка байт во время компиляции (или выполнения)
    // Лучше полагаться на определение компилятора, если оно есть.
    union {
        uint16_t i;
        char c[2];
    } _endian_check = { 0x0100 }; // 0x01 у младшего адреса -> Big Endian

    #define __BYTE_ORDER__ ((_endian_check.c[0] == 1) ? __ORDER_BIG_ENDIAN__ : __ORDER_LITTLE_ENDIAN__)

#endif // __BYTE_ORDER__


// Встроенные функции компилятора для смены порядка байт (gcc/clang)
// Обычно они самые эффективные
#ifndef __bswap_32
    #define __bswap_32(x) __builtin_bswap32(x)
#endif
#ifndef __bswap_64
    #define __bswap_64(x) __builtin_bswap64(x)
#endif


// --- Реализация ---

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

    // Система Little Endian (например, x86, ARM на macOS/iOS)
    // htonX: нужно перевернуть байты (Host -> Network=Big)
    // ntohX: нужно перевернуть байты (Network=Big -> Host)

    uint64_t htonll(uint64_t value) {
        return __bswap_64(value);
    }

    uint64_t ntohll(uint64_t value) {
        return __bswap_64(value);
    }

    uint32_t htonf(float value) {
        uint32_t temp;
        memcpy(&temp, &value, sizeof(uint32_t));
        return __bswap_32(temp);
    }

    float ntohf(uint32_t value) {
        float temp;
        uint32_t swapped = __bswap_32(value);
        memcpy(&temp, &swapped, sizeof(float));
        return temp;
    }

    uint64_t htond(double value) {
        uint64_t temp;
        memcpy(&temp, &value, sizeof(uint64_t));
        return __bswap_64(temp);
    }

    double ntohd(uint64_t value) {
        double temp;
        uint64_t swapped = __bswap_64(value);
        memcpy(&temp, &swapped, sizeof(double));
        return temp;
    }

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

    // Система Big Endian (например, старые PowerPC Mac, SPARC)
    // Порядок байт совпадает с сетевым, преобразование не нужно.

    uint64_t htonll(uint64_t value) {
        return value;
    }

    uint64_t ntohll(uint64_t value) {
        return value;
    }

    uint32_t htonf(float value) {
        uint32_t temp;
        memcpy(&temp, &value, sizeof(uint32_t));
        return temp;
    }

    float ntohf(uint32_t value) {
        float temp;
        memcpy(&temp, &value, sizeof(float));
        return temp;
    }

    uint64_t htond(double value) {
        uint64_t temp;
        memcpy(&temp, &value, sizeof(uint64_t));
        return temp;
    }

    double ntohd(uint64_t value) {
        double temp;
        memcpy(&temp, &value, sizeof(double));
        return temp;
    }

#else
    #error "Unsupported byte order (__BYTE_ORDER__ defined but not Little or Big Endian)"
#endif // __BYTE_ORDER__ check
