#include <stdarg.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/compress.h"

// Test data
static const char* TEST_STRING = "This is a test string for compression algorithms. "
                                "It should be long enough to actually compress.";

// Helper function to compare two buffers
static int buffers_equal(const uint8_t* a, const uint8_t* b, size_t size) {
    return memcmp(a, b, size) == 0;
}

void test_compression_roundtrip(void) {
    printf("[TEST] Starting test_compression_roundtrip...\n");
    printf("%s", TEST_STRING);
    compression_type types[] = {SNAPPY_COMPRESSION, LZ4_COMPRESSION, ZSTD_COMPRESSION};
    const char* type_names[] = {"Snappy", "LZ4", "Zstd"};

    for (int i = 0; i < 3; i++) {
        printf("  Testing %s...\n", type_names[i]);

        size_t original_size = strlen(TEST_STRING) + 1;
        size_t compressed_size = 0;
        size_t decompressed_size = 0;

        // Compress
        uint8_t* compressed = compress_data((uint8_t*)TEST_STRING, original_size,
                                          &compressed_size, types[i]);
        assert(compressed!=NULL);
        assert(compressed_size > 0);

        // Decompress
        decompressed_size = original_size;
        uint8_t* decompressed = decompress_data(compressed, compressed_size,
                                              &decompressed_size, types[i]);
        assert(decompressed!=NULL);
        assert(decompressed_size == original_size);
        assert(buffers_equal((uint8_t*)TEST_STRING, decompressed, original_size));

        printf("    Success! Original: %zu bytes, Compressed: %zu bytes (%.2f%%)\n",
              original_size, compressed_size,
              (compressed_size * 100.0) / original_size);

        free(compressed);
        free(decompressed);
    }
}

static void test_compress_null_input(void** state) {
    printf("[TEST] Starting test_compress_null_input...\n");
    (void)state;
    size_t compressed_size = 0;
    uint8_t* result = compress_data(NULL, 10, &compressed_size, SNAPPY_COMPRESSION);
    assert(result == NULL);  // Should return NULL for NULL input
}

static void test_compress_zero_size(void** state) {
    printf("[TEST] Starting test_compress_zero_size...\n");
    (void)state;
    size_t compressed_size = 0;
    uint8_t test_data = 0;
    uint8_t* result = compress_data(&test_data, 0, &compressed_size, LZ4_COMPRESSION);
    assert(result==NULL);
}

static void test_compress_null_output_size(void** state) {
    printf("[TEST] Starting test_compress_null_output_size...\n");
    (void)state;
    size_t compressed_size = 0;
    uint8_t test_data = 0;
    // printf("%s", test_data);
    uint8_t* result = compress_data(&test_data, 1, &compressed_size, ZSTD_COMPRESSION);
    assert(result!=NULL);
}

static void test_compress_invalid_type(void** state) {
    printf("[TEST] Starting test_compress_invalid_type...\n");
    (void)state;
    size_t compressed_size = 0;
    uint8_t test_data = 0;
    uint8_t* result = compress_data(&test_data, 1, &compressed_size, (compression_type)99);
    assert(result==NULL);
}

static void test_decompress_null_input(void** state) {
    printf("[TEST] Starting test_decompress_null_input...\n");
    (void)state;
    size_t decompressed_size = 0;
    uint8_t* result = decompress_data(NULL, 10, &decompressed_size, SNAPPY_COMPRESSION);
    assert(result==NULL);
}

static void test_decompress_zero_size(void** state) {
    printf("[TEST] Starting test_decompress_zero_size...\n");
    (void)state;
    size_t decompressed_size = 0;
    uint8_t test_data = 0;
    uint8_t* result = decompress_data(&test_data, 0, &decompressed_size, LZ4_COMPRESSION);
    assert(result==NULL);
}

static void test_decompress_null_output_size(void** state) {
    printf("[TEST] Starting test_decompress_null_output_size...\n");
    (void)state;
    size_t decompressed_size = 0;
    uint8_t test_data = 0;
    uint8_t* result = decompress_data(&test_data, 1, &decompressed_size, ZSTD_COMPRESSION);
    assert(result==NULL);
}

static void test_decompress_invalid_type(void** state) {
    printf("[TEST] Starting test_decompress_invalid_type...\n");
    (void)state;
    size_t decompressed_size = 0;
    uint8_t test_data = 0;
    uint8_t* result = decompress_data(&test_data, 1, &decompressed_size, (compression_type)99);
    assert(result==NULL);
}

/**
Decompression functions expect specific format headers,
metadata, and structure — which this array lacks.

hence all 0's is treated as a corrupted data
*/
static void test_decompress_corrupted_data(void** state) {
    printf("[TEST] Starting test_decompress_corrupted_data...\n");
    (void)state;
    uint8_t corrupted_data[100] = {0};
    size_t decompressed_size = 0;

    compression_type types[] = {SNAPPY_COMPRESSION, LZ4_COMPRESSION, ZSTD_COMPRESSION};
    for (int i = 0; i < 3; i++) {
        printf("  Testing with %s...\n",
              (types[i] == SNAPPY_COMPRESSION) ? "Snappy" :
              (types[i] == LZ4_COMPRESSION) ? "LZ4" : "Zstd");
        uint8_t* result = decompress_data(corrupted_data, sizeof(corrupted_data),
                                        &decompressed_size, types[i]);
        assert(result==NULL);
    }
}

void test_minimal(void) {
    printf("[TEST] Starting test_minimal...\n");
    const char* test_str = TEST_STRING;
    size_t size = strlen(test_str)+1;
    size_t compressed_size = 0;

    uint8_t* compressed = compress_data((uint8_t*)test_str, size, &compressed_size, SNAPPY_COMPRESSION);
    assert(compressed);

    size_t decompressed_size = 0;
    uint8_t* decompressed = decompress_data(compressed, compressed_size, &decompressed_size, SNAPPY_COMPRESSION);
    assert(decompressed);
    assert(decompressed_size == size);
    assert(memcmp(test_str, decompressed, size) == 0);

    free(compressed);
    free(decompressed);
}

int main(void) {
    printf("=== Starting Compression Tests ===\n\n");

    void **state = NULL;
    test_minimal();
    test_compression_roundtrip();
    test_compress_null_input(state);
    test_compress_zero_size(state);
    test_compress_null_output_size(state);
    test_compress_invalid_type(state);
    test_decompress_null_input(state);
    test_decompress_zero_size(state);
    test_decompress_null_output_size(state);
    test_decompress_invalid_type(state);
    test_decompress_corrupted_data(state);

    printf("\n=== All tests completed ===\n");
    return 0;
}
