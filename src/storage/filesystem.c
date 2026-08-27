#include "storage/filesystem.h"

#include <stddef.h>

filesystem_result_t filesystem_prepare(
    filesystem_t *filesystem,
    block_device_t *block_device)
{
    if (filesystem == NULL || !block_device_is_valid(block_device)) {
        return FILESYSTEM_RESULT_INVALID_ARGUMENT;
    }

    filesystem->block_device = block_device;
    filesystem->mounted = false;
    return FILESYSTEM_RESULT_OK;
}

filesystem_result_t filesystem_mount(filesystem_t *filesystem)
{
    if (filesystem == NULL
            || !block_device_is_valid(filesystem->block_device)) {
        return FILESYSTEM_RESULT_INVALID_ARGUMENT;
    }

    /*
     * TODO(owner): Call FatFs here after adding third_party/fatfs and a diskio
     * adapter that translates FatFs sector operations to block_device_t.
     */
    return FILESYSTEM_RESULT_NOT_IMPLEMENTED;
}

filesystem_result_t filesystem_unmount(filesystem_t *filesystem)
{
    if (filesystem == NULL) {
        return FILESYSTEM_RESULT_INVALID_ARGUMENT;
    }

    /* TODO(owner): Unmount through FatFs once integration exists. */
    return FILESYSTEM_RESULT_NOT_IMPLEMENTED;
}

bool filesystem_is_mounted(const filesystem_t *filesystem)
{
    return filesystem != NULL && filesystem->mounted;
}

