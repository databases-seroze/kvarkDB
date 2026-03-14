#ifndef __KVARKDB_COMPRESS_H__
#define __KVARKDB_COMPRESS_H__

#include <lz4.h>
#include <snappy-c.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

typedef enum{
    SNAPPY_COMPRESSION,
    LZ4_COMPRESSION,
    ZSTD_COMPRESSION,
} compression_type;

/*
 * compress_data
 * compresses data using the specified compression algorithm.
 * the returned buffer is prefixed with an 8-byte little-endian header
 * storing the original data size, followed by the compressed payload.
 * @param data             input bytes to compress
 * @param data_size        size of input
 * @param compressed_size  output: total size of returned buffer (header + payload)
 * @param type             compression algorithm to use
 * @return heap-allocated buffer (caller must free), or NULL on failure
 */
uint8_t *compress_data(uint8_t *data, size_t data_size, size_t *compressed_size, compression_type type);

/*
 * decompress_data
 * decompresses data produced by compress_data.
 * reads the embedded 8-byte header to determine original size — no need to pass it in.
 * @param data               compressed buffer (header + payload)
 * @param data_size          total size of compressed buffer
 * @param decompressed_size  output: size of the returned decompressed buffer
 * @param type               compression algorithm to use
 * @return heap-allocated buffer (caller must free), or NULL on failure
 */
uint8_t *decompress_data(uint8_t *data, size_t data_size, size_t *decompressed_size, compression_type type);

#endif
