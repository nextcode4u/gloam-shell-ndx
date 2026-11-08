#include "text.h"
uint32_t text_next(const char **cursor)
{
    const unsigned char *p = (const unsigned char *)*cursor;
    if (!*p)
        return 0;
    unsigned first = *p;
    (*cursor)++;
    if (first < 0x80)
        return first;
    int bytes = first >= 0xC2 && first <= 0xDF   ? 2
                : first >= 0xE0 && first <= 0xEF ? 3
                : first >= 0xF0 && first <= 0xF4 ? 4
                                                 : 0;
    if (!bytes)
        return '?';
    uint32_t value = first & (0x7F >> bytes);
    for (int i = 1; i < bytes; i++) {
        if ((p[i] & 0xC0) != 0x80)
            return '?';
        if (i == 1 && ((first == 0xE0 && p[i] < 0xA0) || (first == 0xED && p[i] >= 0xA0) ||
                       (first == 0xF0 && p[i] < 0x90) || (first == 0xF4 && p[i] >= 0x90)))
            return '?';
        value = (value << 6) | (p[i] & 63);
    }
    *cursor += bytes - 1;
    return value;
}
size_t text_prefix_bytes(const char *text, size_t characters)
{
    if (!text)
        return 0;
    const char *cursor = text;
    while (*cursor && characters--)
        text_next(&cursor);
    return (size_t)(cursor - text);
}
size_t text_length(const char *text)
{
    if (!text)
        return 0;
    size_t length = 0;
    while (*text) {
        text_next(&text);
        length++;
    }
    return length;
}

void text_from_utf16le(char *output, size_t capacity, const unsigned char *data, size_t units)
{
    if (!capacity)
        return;
    size_t written = 0;
    for (size_t i = 0; i < units; i++) {
        uint32_t value = data[i * 2] | (uint32_t)data[i * 2 + 1] << 8;
        if (!value)
            break;
        if (value >= 0xD800 && value <= 0xDBFF) {
            uint32_t low = i + 1 < units ? data[i * 2 + 2] | (uint32_t)data[i * 2 + 3] << 8 : 0;
            if (low >= 0xDC00 && low <= 0xDFFF) {
                value = 0x10000 + ((value - 0xD800) << 10) + low - 0xDC00;
                i++;
            } else
                value = '?';
        } else if (value >= 0xDC00 && value <= 0xDFFF)
            value = '?';
        if (value < 32)
            value = ' ';
        unsigned char encoded[4];
        size_t bytes;
        if (value < 0x80) {
            encoded[0] = value;
            bytes = 1;
        } else if (value < 0x800) {
            encoded[0] = 0xC0 | (value >> 6);
            encoded[1] = 0x80 | (value & 63);
            bytes = 2;
        } else if (value < 0x10000) {
            encoded[0] = 0xE0 | (value >> 12);
            encoded[1] = 0x80 | ((value >> 6) & 63);
            encoded[2] = 0x80 | (value & 63);
            bytes = 3;
        } else {
            encoded[0] = 0xF0 | (value >> 18);
            encoded[1] = 0x80 | ((value >> 12) & 63);
            encoded[2] = 0x80 | ((value >> 6) & 63);
            encoded[3] = 0x80 | (value & 63);
            bytes = 4;
        }
        if (bytes >= capacity - written)
            break;
        for (size_t byte = 0; byte < bytes; byte++)
            output[written++] = encoded[byte];
    }
    output[written] = 0;
}
