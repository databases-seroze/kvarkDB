#include "compress.h"

#define HEADER_SIZE 8

static void write_u64_le(uint8_t *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) buf[i] = (val >> (8 * i)) & 0xFF;
}

static uint64_t read_u64_le(const uint8_t *buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) val |= ((uint64_t)buf[i]) << (8 * i);
    return val;
}

uint8_t *compress_data(uint8_t *data, size_t data_size, size_t *compressed_size, compression_type type) {
    if (!data || data_size == 0) return NULL;

    size_t max_size = 0;
    switch (type) {
        case SNAPPY_COMPRESSION: max_size = snappy_max_compressed_length(data_size); break;
        case LZ4_COMPRESSION:    max_size = LZ4_compressBound(data_size);            break;
        case ZSTD_COMPRESSION:   max_size = ZSTD_compressBound(data_size);           break;
        default: return NULL;
    }

    uint8_t *buf = malloc(HEADER_SIZE + max_size);
    if (!buf) return NULL;

    write_u64_le(buf, (uint64_t)data_size);

    size_t actual_size = 0;
    switch (type) {
        case SNAPPY_COMPRESSION: {
            actual_size = max_size;
            snappy_compress((const char *)data, data_size, (char *)(buf + HEADER_SIZE), &actual_size);
            break;
        }
        case LZ4_COMPRESSION: {
            actual_size = LZ4_compress_default((const char *)data, (char *)(buf + HEADER_SIZE),
                                               data_size, (int)max_size);
            if (actual_size == 0) { free(buf); return NULL; }
            break;
        }
        case ZSTD_COMPRESSION: {
            actual_size = ZSTD_compress(buf + HEADER_SIZE, max_size, data, data_size, ZSTD_defaultCLevel());
            if (ZSTD_isError(actual_size)) { free(buf); return NULL; }
            break;
        }
        default: free(buf); return NULL;
    }

    *compressed_size = HEADER_SIZE + actual_size;
    uint8_t *final = realloc(buf, *compressed_size);
    return final ? final : buf;
}

uint8_t *decompress_data(uint8_t *data, size_t data_size, size_t *decompressed_size, compression_type type) {
    if (!data || data_size <= HEADER_SIZE) return NULL;

    uint64_t original_size = read_u64_le(data);
    if (original_size == 0 || original_size > UINT32_MAX) return NULL;

    const uint8_t *payload = data + HEADER_SIZE;
    size_t payload_size = data_size - HEADER_SIZE;

    uint8_t *out = malloc(original_size);
    if (!out) return NULL;

    switch (type) {
        case SNAPPY_COMPRESSION: {
            size_t out_size = original_size;
            if (snappy_uncompress((const char *)payload, payload_size,
                                  (char *)out, &out_size) != SNAPPY_OK) {
                free(out); return NULL;
            }
            *decompressed_size = out_size;
            break;
        }
        case LZ4_COMPRESSION: {
            int result = LZ4_decompress_safe((const char *)payload, (char *)out,
                                             (int)payload_size, (int)original_size);
            if (result < 0) { free(out); return NULL; }
            *decompressed_size = result;
            break;
        }
        case ZSTD_COMPRESSION: {
            size_t result = ZSTD_decompress(out, original_size, payload, payload_size);
            if (ZSTD_isError(result)) { free(out); return NULL; }
            *decompressed_size = result;
            break;
        }
        default: free(out); return NULL;
    }

    return out;
}
