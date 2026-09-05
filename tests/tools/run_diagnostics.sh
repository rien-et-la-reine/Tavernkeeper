#!/bin/sh
# Run the host diagnostics that are not part of the default suite.
#
#   sh tests/tools/run_diagnostics.sh          # everything
#   sh tests/tools/run_diagnostics.sh strict sanitize
#
# Stages: strict, sanitize, gaps, coverage, mutation.
# Run from the repository root. Every stage uses its own build directory so
# they do not disturb each other or the default tests/build.
set -e

stages=${*:-"strict sanitize gaps coverage mutation"}
jobs=${JOBS:-4}

run_stage() {
    printf '\n=== %s ===\n' "$1"
}

for stage in $stages; do
case "$stage" in
strict)
    run_stage "strict warnings as errors"
    cmake -S tests -B tests/build-strict -DCMAKE_BUILD_TYPE=Release \
        -DTAVERNKEEP_TEST_STRICT_WARNINGS=ON
    cmake --build tests/build-strict -j "$jobs"
    ctest --test-dir tests/build-strict --output-on-failure
    ;;
sanitize)
    run_stage "AddressSanitizer and UndefinedBehaviorSanitizer"
    # Needs GCC, or clang with its compiler-rt runtime installed
    # (libclang-rt-<version>-dev on Debian and Ubuntu).
    cmake -S tests -B tests/build-sanitize -DCMAKE_BUILD_TYPE=Debug \
        -DTAVERNKEEP_TEST_SANITIZE=ON -DTAVERNKEEP_TEST_STRICT_WARNINGS=ON
    cmake --build tests/build-sanitize -j "$jobs"
    ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1 \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
        ctest --test-dir tests/build-sanitize --output-on-failure
    ;;
gaps)
    run_stage "regressions for gaps that are still open (these must fail)"
    cmake -S tests -B tests/build-gaps -DCMAKE_BUILD_TYPE=Release \
        -DTAVERNKEEP_TEST_KNOWN_GAPS=ON
    cmake --build tests/build-gaps -j "$jobs"
    # Failure here is the expected outcome; see KNOWN_GAPS.md.
    ctest --test-dir tests/build-gaps -L known-gap --output-on-failure || true
    ;;
coverage)
    run_stage "GCC coverage"
    cmake -S tests -B tests/build-coverage -DCMAKE_BUILD_TYPE=Debug \
        -DTAVERNKEEP_TEST_COVERAGE=ON
    cmake --build tests/build-coverage -j "$jobs"
    ctest --test-dir tests/build-coverage -L host --output-on-failure
    printf '\nRun gcov per target from tests/build-coverage; reports that share\n'
    printf 'a basename overwrite each other, so do not sum them.\n'
    ;;
mutation)
    run_stage "mutation testing"
    python3 tests/tools/mutate.py --jobs "$jobs" \
        --build-dir tests/build-mutation
    ;;
*)
    printf 'unknown stage: %s\n' "$stage" >&2
    exit 2
    ;;
esac
done

printf '\nAll requested diagnostics finished.\n'
