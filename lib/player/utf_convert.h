#ifndef UTF_CONVERT_H
#define UTF_CONVERT_H

#include <stddef.h>
#include <stdint.h>

static size_t utf8_write(
    char *out,
    size_t out_size,
    size_t pos,
    uint32_t cp
) {
    if (cp <= 0x7F) {
        if (pos + 1 >= out_size) return pos;
        out[pos++] = (char)cp;
    }
    else if (cp <= 0x7FF) {
        if (pos + 2 >= out_size) return pos;
        out[pos++] = (char)(0xC0 | (cp >> 6));
        out[pos++] = (char)(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF) {
        if (pos + 3 >= out_size) return pos;
        out[pos++] = (char)(0xE0 | (cp >> 12));
        out[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[pos++] = (char)(0x80 | (cp & 0x3F));
    }
    else {
        if (pos + 4 >= out_size) return pos;
        out[pos++] = (char)(0xF0 | (cp >> 18));
        out[pos++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[pos++] = (char)(0x80 | (cp & 0x3F));
    }

    return pos;
}

static size_t utf16_to_utf8(
    const uint8_t *src,
    size_t src_len,
    char *out,
    size_t out_size,
    int big_endian
) {
    size_t in = 0;
    size_t written = 0;

    if (out_size == 0)
        return 0;

    while (in + 1 < src_len) {
        uint16_t unit;

        if (big_endian)
            unit = ((uint16_t)src[in] << 8) | src[in + 1];
        else
            unit = ((uint16_t)src[in + 1] << 8) | src[in];

        in += 2;

        if (unit == 0)
            break;

        uint32_t cp = unit;

        /* UTF-16 surrogate pair */
        if (unit >= 0xD800 && unit <= 0xDBFF &&
            in + 1 < src_len) {

            uint16_t low;

            if (big_endian)
                low = ((uint16_t)src[in] << 8) | src[in + 1];
            else
                low = ((uint16_t)src[in + 1] << 8) | src[in];

            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000
                   + (((uint32_t)unit - 0xD800) << 10)
                   + ((uint32_t)low - 0xDC00);

                in += 2;
            }
            else {
                cp = 0xFFFD;
            }
        }
        else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            cp = 0xFFFD;
        }

        size_t next = utf8_write(
            out,
            out_size,
            written,
            cp
        );

        if (next == written)
            break;

        written = next;
    }

    out[written] = '\0';
    return written;
}

#endif
