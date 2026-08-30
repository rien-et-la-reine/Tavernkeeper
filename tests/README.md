# Host tests

The host test target compiles the production `src/storage/sd_spi.c` against
small Pico SDK fakes. No Pico SDK installation or physical SD card is needed.

Configure, build, and run from the repository root:

```sh
cmake -S tests -B build-host-tests
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
```

On Windows with MSYS2/MinGW installed but not already on `PATH`, use:

```powershell
$env:Path = 'C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:Path
cmake -S tests -B build-host-tests -G "Unix Makefiles" `
    -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe `
    -DCMAKE_MAKE_PROGRAM=C:/msys64/usr/bin/make.exe
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
```

The fake SD card records commands and supplies configurable R1/payload bytes.
The current suite covers configuration, initialization for SDHC and legacy
cards, initialization failures and timeouts, public precondition checks, and
deinitialization behavior. As block reads, writes, and CSD parsing are
implemented, use `pico_mock_sd_set_command()` to attach data tokens and block
payloads to CMD9/CMD17/CMD18 responses. Write-path tests can inspect the TX log
and extend the fake with data-response tokens when that protocol is added.
