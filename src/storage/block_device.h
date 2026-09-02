#ifndef TAVERNKEEP_STORAGE_BLOCK_DEVICE_H
#define TAVERNKEEP_STORAGE_BLOCK_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    BLOCK_DEVICE_RESULT_OK = 0,
    BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
    BLOCK_DEVICE_RESULT_NOT_INITIALIZED,
    BLOCK_DEVICE_RESULT_OUT_OF_RANGE,
    BLOCK_DEVICE_RESULT_IO_ERROR,
    BLOCK_DEVICE_RESULT_BUSY_TIMEOUT,
    BLOCK_DEVICE_RESULT_INVALID_DEVICE, //either write protected or no card detected
    BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED,
} block_device_result_t;

typedef struct {
    uint32_t block_size_bytes;
    uint64_t block_count;
    bool writable;
} block_device_info_t;

typedef struct {
    block_device_result_t (*init)(void *context);
    block_device_result_t (*deinit)(void *context);
    block_device_result_t (*read_blocks)(
        void *context,
        uint64_t first_lba,
        void *buffer,
        size_t block_count);
    block_device_result_t (*write_blocks)(
        void *context,
        uint64_t first_lba,
        const void *buffer,
        size_t block_count);
    block_device_result_t (*get_info)(
        void *context,
        block_device_info_t *info);
} block_device_operations_t;

typedef struct {
    void *context;
    const block_device_operations_t *operations;
} block_device_t;

static inline bool block_device_is_valid(const block_device_t *device)
{
    return device != NULL && device->context != NULL
        && device->operations != NULL;
}

static inline block_device_result_t block_device_init(
    const block_device_t *device)
{
    if (!block_device_is_valid(device) || device->operations->init == NULL) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    return device->operations->init(device->context);
}

static inline block_device_result_t block_device_deinit(
    const block_device_t *device)
{
    if (!block_device_is_valid(device) || device->operations->deinit == NULL) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    return device->operations->deinit(device->context);
}

static inline block_device_result_t block_device_read_blocks(
    const block_device_t *device,
    uint64_t first_lba,
    void *buffer,
    size_t block_count)
{
    if (!block_device_is_valid(device)
            || device->operations->read_blocks == NULL || buffer == NULL
            || block_count == 0U) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    return device->operations->read_blocks(
        device->context, first_lba, buffer, block_count);
}

static inline block_device_result_t block_device_write_blocks(
    const block_device_t *device,
    uint64_t first_lba,
    const void *buffer,
    size_t block_count)
{
    if (!block_device_is_valid(device)
            || device->operations->write_blocks == NULL || buffer == NULL
            || block_count == 0U) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    return device->operations->write_blocks(
        device->context, first_lba, buffer, block_count);
}

static inline block_device_result_t block_device_get_info(
    const block_device_t *device,
    block_device_info_t *info)
{
    if (!block_device_is_valid(device) || device->operations->get_info == NULL
            || info == NULL) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    return device->operations->get_info(device->context, info);
}

#endif
