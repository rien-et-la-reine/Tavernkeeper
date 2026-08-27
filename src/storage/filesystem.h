#ifndef TAVERNKEEP_STORAGE_FILESYSTEM_H
#define TAVERNKEEP_STORAGE_FILESYSTEM_H

#include <stdbool.h>

#include "storage/block_device.h"

typedef enum {
    FILESYSTEM_RESULT_OK = 0,
    FILESYSTEM_RESULT_INVALID_ARGUMENT,
    FILESYSTEM_RESULT_STORAGE_ERROR,
    FILESYSTEM_RESULT_NOT_IMPLEMENTED,
} filesystem_result_t;

typedef struct {
    block_device_t *block_device;
    bool mounted;
} filesystem_t;

filesystem_result_t filesystem_prepare(
    filesystem_t *filesystem,
    block_device_t *block_device);

filesystem_result_t filesystem_mount(filesystem_t *filesystem);
filesystem_result_t filesystem_unmount(filesystem_t *filesystem);
bool filesystem_is_mounted(const filesystem_t *filesystem);

#endif

