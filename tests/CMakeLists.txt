cmake_minimum_required(VERSION 3.16)

project(tavernkeep_host_tests LANGUAGES C)

enable_testing()

add_executable(sd_spi_host_tests
    test_sd_spi.c
    support/pico_mock.c
    ../src/storage/sd_spi.c
)

target_include_directories(sd_spi_host_tests PRIVATE
    fakes
    support
    ../src
)

set_target_properties(sd_spi_host_tests PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(sd_spi_host_tests PRIVATE /W4)
else()
    target_compile_options(sd_spi_host_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

add_test(NAME sd_spi_host_tests COMMAND sd_spi_host_tests)

