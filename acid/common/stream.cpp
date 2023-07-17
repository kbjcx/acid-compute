#include "stream.h"

namespace acid {

size_t Stream::read_fix_size(void *buffer, size_t length) {
    size_t offset = 0;
    size_t left = length;
    while (left > 0) {
        size_t len = read((char*)buffer + offset, left);
        if (len <= 0) {
            return len;
        }
        offset += len;
        left -= len;
    }

    return length;
}

size_t Stream::read_fix_size(ByteArray::ptr byte_array, size_t length) {
    size_t left = length;
    while (left > 0) {
        size_t len = read(byte_array, left);
        if (len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

size_t Stream::write_fix_size(const void *buffer, size_t length) {
    size_t offset = 0;
    size_t left = length;

    while (left > 0) {
        size_t len = write((const char*)buffer + offset, left);
        if (len <= 0) {
            return len;
        }
        offset += len;
        left -= len;
    }
    return length;
}

size_t Stream::write_fix_size(ByteArray::ptr byte_array, size_t length) {
    size_t left = length;
    while (left > 0) {
        size_t len = write(byte_array, left);
        if (len <= 0) {
            return len;
        }
        left -= len;
    }
    return length;
}

} // namespace acid