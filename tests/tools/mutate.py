#!/usr/bin/env python3
"""Apply representative incorrect implementations and report which the suite catches.

A test suite that executes a line proves nothing about whether it would notice
that line being wrong. This script substitutes deliberate mistakes into the
production sources one at a time, rebuilds, runs the enabled host suite, and
records whether anything failed. A mutation that survives is a hole: either the
tests need strengthening or the behaviour is genuinely outside the contract, in
which case say so in MUTATION.md rather than leaving it unexplained.

Usage, from the repository root:

    python3 tests/tools/mutate.py                 # every mutation
    python3 tests/tools/mutate.py --only cmd0-crc-constant range-check-off-by-one
    python3 tests/tools/mutate.py --list
    python3 tests/tools/mutate.py --jobs 8 --build-dir tests/build-mutation

The working tree is restored after each mutation and again on exit, including
after Ctrl-C or a build failure.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MUTATIONS_FILE = os.path.join(os.path.dirname(__file__), "mutations.txt")


class Mutation:
    def __init__(self, identifier, path, why, find, replace, equivalent,
                 unkillable):
        self.identifier = identifier
        self.path = path
        self.why = why
        self.find = find
        self.replace = replace
        # A written argument that no reachable input distinguishes this
        # mutation from the original. Such a mutation cannot be killed by any
        # test, so surviving it is expected and is not a hole in the suite.
        self.equivalent = equivalent
        # A written reason why no test *in this harness* can reach the
        # behaviour, as distinct from an equivalent mutant, which no program
        # could distinguish. Both survive; only one of them is fine forever.
        self.unkillable = unkillable


def parse_mutations(text):
    mutations = []
    for block in re.split(r"\n\s*\n(?=id:)", text):
        block = block.strip("\n")
        if not block.strip() or block.lstrip().startswith("#"):
            continue
        lines = block.split("\n")
        header = {}
        index = 0
        why_lines = []
        while index < len(lines) and lines[index].strip() != "---":
            line = lines[index]
            if line.startswith("#"):
                index += 1
                continue
            match = re.match(r"^(id|file|why|equivalent|unkillable):\s*(.*)$",
                             line)
            if match:
                key = match.group(1)
                header[key] = match.group(2)
                continuation = [match.group(2)]
                if key == "why":
                    why_lines = continuation
                elif key in ("equivalent", "unkillable"):
                    header["_note_lines"] = continuation
                    header["_note_kind"] = key
            elif line.startswith("     "):
                if "_note_lines" in header and header.get("_last", "") in (
                        "equivalent", "unkillable"):
                    header["_note_lines"].append(line.strip())
                elif why_lines:
                    why_lines.append(line.strip())
            if match:
                header["_last"] = match.group(1)
            index += 1
        if index >= len(lines):
            continue
        index += 1
        find_lines = []
        while index < len(lines) and lines[index] != "===":
            find_lines.append(lines[index])
            index += 1
        index += 1
        replace_lines = lines[index:]
        equivalent = None
        unkillable = None
        if "_note_lines" in header:
            note = " ".join(header["_note_lines"])
            if header.get("_note_kind") == "equivalent":
                equivalent = note
            else:
                unkillable = note
        mutations.append(Mutation(
            header.get("id", "?"),
            header.get("file", ""),
            " ".join(why_lines),
            "\n".join(find_lines),
            "\n".join(replace_lines),
            equivalent,
            unkillable))
    return mutations


def run(command, cwd=None, timeout=900):
    return subprocess.run(
        command, cwd=cwd, capture_output=True, text=True, timeout=timeout)


def configure(build_dir, extra):
    args = ["cmake", "-S", "tests", "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release"]
    args.extend(extra)
    return run(args, cwd=REPO_ROOT)


def build(build_dir, jobs):
    return run(["cmake", "--build", build_dir, "-j", str(jobs)], cwd=REPO_ROOT)


def ctest(build_dir):
    return run(
        ["ctest", "--test-dir", build_dir, "-L", "host", "--output-on-failure"],
        cwd=REPO_ROOT)


def failing_tests(output):
    names = []
    capture = False
    for line in output.splitlines():
        if line.startswith("The following tests FAILED:"):
            capture = True
            continue
        if capture:
            match = re.match(r"\s*\d+\s*-\s*(\S+)", line)
            if match:
                names.append(match.group(1))
            elif line.strip() == "":
                break
    return names


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--only", nargs="*", default=None,
                        help="run only these mutation ids")
    parser.add_argument("--list", action="store_true",
                        help="list mutation ids and exit")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    parser.add_argument("--build-dir", default="tests/build-mutation")
    parser.add_argument("--cmake-arg", action="append", default=[],
                        help="extra -D argument passed to cmake")
    arguments = parser.parse_args()

    with open(MUTATIONS_FILE, "r", encoding="utf-8") as handle:
        mutations = parse_mutations(handle.read())
    if arguments.list:
        for mutation in mutations:
            print("{:34s} {}".format(mutation.identifier, mutation.why))
        return 0
    if arguments.only:
        wanted = set(arguments.only)
        unknown = wanted - {m.identifier for m in mutations}
        if unknown:
            print("unknown mutation id(s): {}".format(", ".join(sorted(unknown))),
                  file=sys.stderr)
            return 2
        mutations = [m for m in mutations if m.identifier in wanted]

    build_dir = arguments.build_dir
    print("configuring {} ...".format(build_dir))
    result = configure(build_dir, arguments.cmake_arg)
    if result.returncode != 0:
        print(result.stdout + result.stderr, file=sys.stderr)
        return 1

    print("baseline build and run ...")
    result = build(build_dir, arguments.jobs)
    if result.returncode != 0:
        print(result.stdout + result.stderr, file=sys.stderr)
        return 1
    result = ctest(build_dir)
    if result.returncode != 0:
        print("The unmutated suite must pass before mutations mean anything.",
              file=sys.stderr)
        print(result.stdout[-4000:], file=sys.stderr)
        return 1
    print("baseline: suite passes\n")

    backups = {}
    survivors = []
    equivalents = []
    unkillables = []
    killed = []
    uncompilable = []

    def restore():
        for path, original in backups.items():
            with open(path, "w", encoding="utf-8", newline="") as handle:
                handle.write(original)

    try:
        for index, mutation in enumerate(mutations, start=1):
            absolute = os.path.join(REPO_ROOT, mutation.path)
            if absolute not in backups:
                with open(absolute, "r", encoding="utf-8", newline="") as handle:
                    backups[absolute] = handle.read()
            source = backups[absolute]
            # The repository mixes LF and CRLF sources, so match on normalised
            # text and write the file back in its original style. Otherwise a
            # mutation would silently fail to apply and be reported as a
            # surviving one - a false clean bill of health.
            crlf = source.count("\r\n") > source.count("\n") // 2
            normalised = source.replace("\r\n", "\n")
            occurrences = normalised.count(mutation.find)
            if occurrences != 1:
                print("[{:2d}/{:2d}] {:34s} SKIPPED: pattern found {} times"
                      .format(index, len(mutations), mutation.identifier,
                              occurrences))
                survivors.append((mutation, "pattern did not match uniquely"))
                continue
            mutated = normalised.replace(mutation.find, mutation.replace)
            if crlf:
                mutated = mutated.replace("\n", "\r\n")
            with open(absolute, "w", encoding="utf-8", newline="") as handle:
                handle.write(mutated)

            started = time.time()
            build_result = build(build_dir, arguments.jobs)
            if build_result.returncode != 0:
                print("[{:2d}/{:2d}] {:34s} DID NOT COMPILE"
                      .format(index, len(mutations), mutation.identifier))
                uncompilable.append(mutation)
                restore()
                continue
            test_result = ctest(build_dir)
            elapsed = time.time() - started
            restore()

            if test_result.returncode != 0:
                names = failing_tests(test_result.stdout)
                killed.append((mutation, names))
                print("[{:2d}/{:2d}] {:34s} KILLED by {} ({:.1f}s)"
                      .format(index, len(mutations), mutation.identifier,
                              ", ".join(names) if names else "the suite",
                              elapsed))
            elif mutation.equivalent:
                equivalents.append(mutation)
                print("[{:2d}/{:2d}] {:34s} survived, documented equivalent"
                      " ({:.1f}s)".format(index, len(mutations),
                                          mutation.identifier, elapsed))
            elif mutation.unkillable:
                unkillables.append(mutation)
                print("[{:2d}/{:2d}] {:34s} survived, out of this harness'"
                      " reach ({:.1f}s)".format(index, len(mutations),
                                                mutation.identifier, elapsed))
            else:
                survivors.append((mutation, "suite still passes"))
                print("[{:2d}/{:2d}] {:34s} SURVIVED ({:.1f}s)"
                      .format(index, len(mutations), mutation.identifier,
                              elapsed))
    finally:
        restore()

    total = (len(killed) + len(survivors) + len(equivalents)
             + len(unkillables))
    print("\n{} of {} mutations detected; {} documented equivalent;"
          " {} out of this harness' reach"
          .format(len(killed), total, len(equivalents), len(unkillables)))
    for mutation in equivalents:
        print("  equivalent:  {:24s} {}".format(
            mutation.identifier, mutation.equivalent))
    for mutation in unkillables:
        print("  unreachable: {:24s} {}".format(
            mutation.identifier, mutation.unkillable))
    if uncompilable:
        print("{} did not compile (not counted)".format(len(uncompilable)))
    if survivors:
        print("\nSURVIVING MUTATIONS - each needs a stronger test or a written"
              " reason it is outside the contract:")
        for mutation, reason in survivors:
            print("  {:34s} {} [{}]".format(
                mutation.identifier, mutation.why, reason))
    return 1 if survivors else 0


if __name__ == "__main__":
    sys.exit(main())
