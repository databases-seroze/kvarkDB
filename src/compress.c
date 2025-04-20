#include "compress.h"

// compress data
uint8_t *compress_data(uint8_t *data, size_t data_size, size_t *compressed_size, compression_type type) {
    uint8_t* compressed_data = NULL;

    // Input validation
    if (!data || data_size == 0) {
        return NULL;
    }

    switch (type) {
        case SNAPPY_COMPRESSION: {
            *compressed_size = snappy_max_compressed_length(data_size);
            compressed_data = malloc(*compressed_size);
            if (!compressed_data) return NULL;

            snappy_compress((const char *)data, data_size, (char *)compressed_data,
                           compressed_size);
            break;
        }

        case LZ4_COMPRESSION: {
            *compressed_size = LZ4_compressBound(data_size);
            compressed_data = malloc(*compressed_size);
            if (!compressed_data) return NULL;

            *compressed_size = LZ4_compress_default((const char *)data,
                                                  (char *)compressed_data,
                                                  data_size,
                                                  *compressed_size);
            if (*compressed_size == 0) {
                free(compressed_data);
                return NULL;
            }
            break;
        }

        case ZSTD_COMPRESSION: {
            *compressed_size = ZSTD_compressBound(data_size);
            compressed_data = malloc(*compressed_size);
            if (!compressed_data) return NULL;

            *compressed_size = ZSTD_compress(compressed_data, *compressed_size,
                                           data, data_size,
                                           ZSTD_defaultCLevel());
            if (ZSTD_isError(*compressed_size)) {
                free(compressed_data);
                return NULL;
            }
            break;
        }

        default:
            return NULL;
    }

    // Realloc to actual compressed size to save memory
    uint8_t* final_data = realloc(compressed_data, *compressed_size);
    return final_data ? final_data : compressed_data; // Return original if realloc fails
}

// decompress data
uint8_t *decompress_data(uint8_t *data, size_t decompressed_data_size, size_t *decompressed_size, compression_type type) {
    uint8_t* decompressed_data = NULL;
    size_t estimated_size = 0;

    // Input validation
    if (!data || decompressed_data_size == 0) {
        return NULL;
    }


    switch (type) {
        case SNAPPY_COMPRESSION: {
            if (snappy_uncompressed_length((const char *)data, decompressed_data_size, &estimated_size) != SNAPPY_OK) {
                return NULL;
            }
            decompressed_data = malloc(estimated_size);
            if (!decompressed_data) return NULL;

            // Fix: Add the fourth argument (pointer to uncompressed length)
            if (snappy_uncompress((const char *)data, decompressed_data_size,
                                (char *)decompressed_data, &estimated_size) != SNAPPY_OK) {
                free(decompressed_data);
                return NULL;
            }
            *decompressed_size = estimated_size;
            break;
        }

        case LZ4_COMPRESSION: {
            if (*decompressed_size == 0) return NULL;

            decompressed_data = malloc(*decompressed_size);
            if (!decompressed_data) return NULL;

            int result = LZ4_decompress_safe((const char *)data,
                                           (char *)decompressed_data,
                                           decompressed_data_size,
                                           *decompressed_size);
            if (result < 0) {
                free(decompressed_data);
                return NULL;
            }
            *decompressed_size = result;
            break;
        }

        case ZSTD_COMPRESSION: {
            estimated_size = ZSTD_getFrameContentSize(data, decompressed_data_size);
            if (estimated_size == ZSTD_CONTENTSIZE_ERROR ||
                estimated_size == ZSTD_CONTENTSIZE_UNKNOWN) {
                return NULL;
            }

            decompressed_data = malloc(estimated_size);
            if (!decompressed_data) return NULL;

            *decompressed_size = ZSTD_decompress(decompressed_data, estimated_size,
                                                data, decompressed_data_size);
            if (ZSTD_isError(*decompressed_size)) {
                free(decompressed_data);
                return NULL;
            }
            break;
        }

        default:
            return NULL;
    }

    return decompressed_data;
}
