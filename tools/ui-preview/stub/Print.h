#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) = 0;

    size_t write(const uint8_t* buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            n += write(*buffer++);
        }
        return n;
    }

    size_t print(const char* text) {
        size_t n = 0;
        while (*text) {
            n += write(static_cast<uint8_t>(*text++));
        }
        return n;
    }

    size_t print(char c) { return write(static_cast<uint8_t>(c)); }

    size_t print(int value) {
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "%d", value);
        return print(buffer);
    }
};
