#ifndef __KVARKDB_COMPRESS__
#define __KVARKDB_COMPRESS__

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
 * compresses data using the specified compression algorithm
 * @param data the data to compress
 * @param data_size the size of the data
 * @param compressed_size is pointer to the final compressed data
 * @param type the compression algorithm to use
 * @return the compressed data
 */
uint8_t *compress_data(uint8_t* data, size_t data_size, size_t *compressed_size, compression_type compression_type);

/*
 * decompress_data
 * decompresses data using the specified compression algorithm
 * @param data the data to decompress
 * @param data_size the size of the data
 * @param type the compression algorithm to use
 * @return the decompressed data
 */
uint8_t *decompress_data(uint8_t *data, size_t data_size, size_t *compressed_size, compression_type type);

#endif
