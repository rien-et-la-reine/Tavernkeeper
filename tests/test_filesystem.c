#include "storage/filesystem.h"
#include "test_check.h"

static block_device_result_t unexpected_init(void *context)
{
    (void)context;
    REQUIRE(!"filesystem scaffold must not access storage");
    return BLOCK_DEVICE_RESULT_IO_ERROR;
}
static block_device_result_t unexpected_read(void *context, uint64_t lba, void *buffer, size_t count)
{
    (void)lba; (void)buffer; (void)count;
    return unexpected_init(context);
}
static block_device_result_t unexpected_write(void *context, uint64_t lba, const void *buffer, size_t count)
{
    (void)lba; (void)buffer; (void)count;
    return unexpected_init(context);
}
static block_device_result_t unexpected_info(void *context, block_device_info_t *info)
{
    (void)info;
    return unexpected_init(context);
}
int main(void)
{
    int context = 0;
    const block_device_operations_t ops = {
        unexpected_init, unexpected_init, unexpected_read, unexpected_write, unexpected_info
    };
    block_device_t device = { &context, &ops };
    block_device_t invalid[] = { {NULL, &ops}, {&context, NULL} };
    filesystem_t fs = { &device, true };
    REQUIRE(filesystem_is_mounted(&fs));
    REQUIRE(filesystem_prepare(NULL, &device) == FILESYSTEM_RESULT_INVALID_ARGUMENT);
    REQUIRE(filesystem_prepare(&fs, NULL) == FILESYSTEM_RESULT_INVALID_ARGUMENT);
    REQUIRE(fs.block_device == &device && fs.mounted);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        REQUIRE(filesystem_prepare(&fs, &invalid[i]) == FILESYSTEM_RESULT_INVALID_ARGUMENT);
        REQUIRE(fs.block_device == &device && fs.mounted);
        filesystem_t bad = { &invalid[i], false };
        REQUIRE(filesystem_mount(&bad) == FILESYSTEM_RESULT_INVALID_ARGUMENT);
    }
    REQUIRE(!filesystem_is_mounted(NULL));
    REQUIRE(filesystem_mount(NULL) == FILESYSTEM_RESULT_INVALID_ARGUMENT);
    REQUIRE(filesystem_unmount(NULL) == FILESYSTEM_RESULT_INVALID_ARGUMENT);
    filesystem_t unprepared = {0};
    REQUIRE(filesystem_mount(&unprepared) == FILESYSTEM_RESULT_INVALID_ARGUMENT);
    REQUIRE(filesystem_prepare(&fs, &device) == FILESYSTEM_RESULT_OK);
    REQUIRE(fs.block_device == &device && !filesystem_is_mounted(&fs));
    for (unsigned int i = 0; i < 2; ++i) {
        REQUIRE(filesystem_mount(&fs) == FILESYSTEM_RESULT_NOT_IMPLEMENTED);
        REQUIRE(!filesystem_is_mounted(&fs));
        REQUIRE(filesystem_unmount(&fs) == FILESYSTEM_RESULT_NOT_IMPLEMENTED);
        REQUIRE(!filesystem_is_mounted(&fs) && fs.block_device == &device);
    }
    block_device_t replacement = { &context, &ops };
    REQUIRE(filesystem_prepare(&fs, &replacement) == FILESYSTEM_RESULT_OK);
    REQUIRE(fs.block_device == &replacement && !filesystem_is_mounted(&fs));
    puts("PASS filesystem validation and explicit scaffold contracts (no FatFs)");
    return 0;
}
