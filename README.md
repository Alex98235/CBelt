# CBelt

> The name is a stupid riff on _seatbelt_ → _CeatBelt_ → _CBelt_

Single-header C testing framework with automatic test registration (GCC/Clang constructor attributes), sandboxed execution, timeouts, parallelism, and zero external dependencies.

```c
CBELT_TEST(addition) {
    cbelt_assert(2 + 2 == 4);
    return TEST_SUCCESS;
}
CBELT_MAIN  // provides main(argc, argv)
```

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Writing Tests](#writing-tests)
  - [Assert](#assert)
  - [Groups](#groups)
- [Running Tests](#running-tests)
  - [Execution Model](#execution-model)
- [Sandboxing & Timeouts](#sandboxing--timeouts)
  - [Configuration](#configuration)
- [Setup & Teardown](#setup--teardown)
  - [Lifecycle Order](#lifecycle-order)
- [Resource Management](#resource-management)
  - [`cbelt_auto`](#cbelt_auto)
  - [`cbelt_defer`](#cbelt_defer)
- [Output Capture](#output-capture)
- [Platform Support](#platform-support)
- [API Reference](#api-reference)
  - [Macros](#macros)
  - [Assertions](#assertions)
  - [Functions](#functions)
  - [Types](#types)

## Features

- **Single header**: drop `cbelt.h` into your project, no linking or build system changes
- **Auto-registration**: `CBELT_TEST(name)` registers itself before `main()` via constructor attribute
- **Sandboxed execution**: Linux: `fork()`-based isolation for crash/timeout detection; Windows: thread-based timeout detection
- **Timeouts**: global and per-test configurable timeouts; hung tests are killed and reported
- **Parallelism**: `--jobs N` runs groups across N threads (pthreads on Linux, Win32 threads on Windows)
- **Test groups**: organize tests with `CBELT_GROUP("name")`, hooks at test/group/global level
- **Rich assertions**: `cbelt_assert_equal`, `cbelt_assert_str_equal`, `cbelt_assert_mem_equal`, `cbelt_assert_in_range`, and more
- **CLI flags**: quiet mode, filter, spinner toggle, color control, parallelism
- **Resource management**: `cbelt_auto` and `cbelt_defer` for leak-safe cleanup on assertion failure
- **Output capture**: redirect stdout to a buffer or file for testing code that prints to the console

## Quick Start

Suggested project structure:

```text
project/
├── tests/
│   ├── cbelt.h
│   ├── test_main.c      ← CBELT_IMPLEMENTATION + CBELT_MAIN
│   ├── test_math.c      ← your tests
│   └── test_something.c ← more tests
└── src/
```

`test_main.c`: **exactly one** file needs the implementation:

```c
#define CBELT_IMPLEMENTATION
#include "../cbelt.h"
CBELT_MAIN
```

Write tests:

```c
// test_math.c
#include "../cbelt.h"

CBELT_GROUP("math")

CBELT_TEST(addition) {
    cbelt_assert(2 + 2 == 4);
    return TEST_SUCCESS;
}

CBELT_TEST(subtraction) {
    cbelt_assert(5 - 3 == 2);
    return TEST_SUCCESS;
}
```

**Compile and run** (C11 or later required):

```bash
gcc -std=c11 -o run_tests tests/test_main.c tests/test_math.c
./run_tests
```

Output:

```text
=== math ===
  addition
  subtraction

================================
2 passed, 0 failed
```

Tests without a group appear under `(ungrouped)`.

## Writing Tests

### Assert

```c
cbelt_assert(expr);                          // any boolean expression
cbelt_assert_equal(42, value);               // type-aware equality
cbelt_assert_not_equal(1, 2);                // type-aware inequality
cbelt_assert_str_equal("hello", str);        // strcmp-based
cbelt_assert_str_not_equal("a", "b");
cbelt_assert_null(ptr);
cbelt_assert_not_null(ptr);
cbelt_assert_mem_equal(expected, data, sz);  // memcmp-based
cbelt_assert_in_range(50, 0, 100);           // inclusive [min, max]
```

All assertion macros print the expression, file, and line on failure, then return `TEST_FAILURE`.

### Groups

```c
CBELT_GROUP("math");

CBELT_TEST(addition) { ... }
CBELT_TEST(multiplication) { ... }

CBELT_GROUP("strings");

CBELT_TEST(length) { ... }
```

Groups apply to all tests that follow them in source order. Tests within a group execute **top-to-bottom** as written.

## Running Tests

When using `CBELT_MAIN` or calling `cbelt_run_all_tests_with_args(argc, argv)`:

| Flag | Effect |
| --- | --- |
| `-q`, `--quiet` | Show only failing tests and the summary |
| `-f`, `--filter PATTERN` | Run tests matching PATTERN (prefix match on `group.test_name`, group name, or test name) |
| `--no-spinner` | Disable the animated spinner |
| `--color auto` | Colors when stdout is a terminal (default) |
| `--color always` | Always use ANSI colors |
| `--color never` | Disable colors |
| `-j N`, `--jobs N` | Run groups in parallel across N threads (default: 1) |
| `-h`, `--help` | Print usage |

```bash
./run_tests --quiet                          # only failures
./run_tests --filter math                    # tests matching "math"
./run_tests --jobs 4 --no-spinner            # parallel, no animation
./run_tests --color always                   # colors even when piping
```

### Execution Model

- **Serial (default)**: groups execute in registration order. Tests within a group run sequentially top-to-bottom.
- **Parallel** (`--jobs N`, when N > 1): groups are distributed across N threads (chunked round-robin). Each group runs entirely within one thread. _**Tests within a group are still sequential**_.
- **Filtering**: Groups with no matching tests are skipped entirely.
- **Exit code**: 0 if all pass, 1 if any fail.

## Sandboxing & Timeouts

**Linux**: each test runs in a forked child process. Crashes (segfault, SIGABRT, etc.) are caught and reported with the signal name. Timeouts use `alarm()`. Error messages from failed assertions are propagated back via a pipe.

**Windows**: each test runs in a `CreateThread` with `WaitForSingleObject` timeout. Tests that exceed the timeout are terminated. Crash detection (access violation) is **not** available on Windows — a crashing test will bring down the whole process.

**Other platforms**: tests run in-process without isolation or timeout support.

### Configuration

Set the default timeout in `CBELT_GLOBAL_SETUP()`:

```c
CBELT_GLOBAL_SETUP() {
    cbelt_set_default_timeout(10);   // 10 seconds (default: 30)
    cbelt_set_default_timeout(-1);   // disable globally
}
```

Per-test timeout:

```c
CBELT_TEST_TIMEOUT(slow_query, 60) { ... }      // 60 second timeout
CBELT_TEST_NO_TIMEOUT(wait_for_input) { ... }    // no timeout
```

A timeout of `0` means "use global default". A negative value means "no timeout".

## Setup & Teardown

Three levels of lifecycle hooks, each scoped to the current group:

```c
CBELT_GROUP("database_tests");

CBELT_GROUP_SETUP()   { /* once before any test in this group */ }
CBELT_GROUP_TEARDOWN(){ /* once after all tests in this group */ }

CBELT_SETUP()         { /* before each test in this group */ }
CBELT_TEARDOWN()      { /* after each test in this group */ }

CBELT_TEST(query) { ... }
```

**Global hooks** (run once before/after all groups):

```c
CBELT_GLOBAL_SETUP()    { srand(42); cbelt_set_default_timeout(10); }
CBELT_GLOBAL_TEARDOWN() { /* cleanup global resources */ }
```

### Lifecycle Order

```text
CBELT_GLOBAL_SETUP()
  For each group:
    CBELT_GROUP_SETUP()
      For each test:
        CBELT_SETUP()
        test function (sandboxed on Linux)
        CBELT_TEARDOWN()
    CBELT_GROUP_TEARDOWN()
CBELT_GLOBAL_TEARDOWN()
```

## Resource Management

Assertion failures call `return TEST_FAILURE` inside the macro, skipping any cleanup code after the assertion. CBelt provides GCC/Clang `__attribute__((cleanup))`-based tools to handle this:

### `cbelt_auto`

Auto-frees heap allocations on scope exit (normal return or assertion failure):

```c
CBELT_TEST(my_test) {
    cbelt_auto buffer = malloc(4096);
    cbelt_auto items  = calloc(10, sizeof(int));
    // freed automatically on return
    return TEST_SUCCESS;
}
```

### `cbelt_defer`

Schedules an arbitrary cleanup callback:

```c
CBELT_TEST(file_read) {
    FILE *fp = fopen("data.txt", "r");
    cbelt_assert_not_null(fp);
    cbelt_defer(fclose, fp);
    // fclose(fp) runs on scope exit regardless of how it exits
    return TEST_SUCCESS;
}
```

> **MSVC note**: `cbelt_auto` and `cbelt_defer` are no-ops on MSVC (no `__attribute__((cleanup))`). They compile without error but won't auto-cleanup.

## Output Capture

Redirect stdout to a buffer or file for programmatic capture. Auto-resets after each test.

```c
CBELT_TEST(prints_correctly) {
    char buf[1024];
    cbelt_set_output(buf, sizeof(buf));       // redirect stdout → buf
    print_greeting();                          // printf("Hello!") → buf
    cbelt_assert_str_equal(buf, "Hello!");     // assert on captured output
    // stdout is auto-restored on test return
    return TEST_SUCCESS;
}
```

For file-based capture:

```c
cbelt_set_output_file(stderr);    // send all stdout to stderr
cbelt_set_output_file(logfile);   // capture to a file
cbelt_reset_output();             // manual restore (for special cases)
```

## Platform Support

| Feature | GCC | Clang | MSVC |
| --- | --- | --- | --- |
| Auto-registration | ✅ | ✅ | ❌ (manual registration) |
| Sandbox (Linux) | ✅ | ✅ | N/A |
| Sandbox (Windows timeout) | ✅ | ✅ | ✅ |
| `cbelt_auto` / `cbelt_defer` | ✅ | ✅ | ❌ (no-op) |

## API Reference

### Macros

| Macro | Purpose |
| --- | --- |
| `CBELT_MAIN` | Defines `main(argc, argv)` calling `cbelt_run_all_tests_with_args()` |
| `CBELT_GROUP(name)` | Sets active group for subsequent tests |
| `CBELT_TEST(name)` | Defines a test function and auto-registers it |
| `CBELT_TEST_TIMEOUT(name, s)` | Test with specific timeout (0=default, negative=no timeout) |
| `CBELT_TEST_NO_TIMEOUT(name)` | Test with no timeout |
| `CBELT_SETUP()` / `CBELT_TEARDOWN()` | Per-test setup/teardown for current group |
| `CBELT_GROUP_SETUP()` / `CBELT_GROUP_TEARDOWN()` | Group-level setup/teardown |
| `CBELT_GLOBAL_SETUP()` / `CBELT_GLOBAL_TEARDOWN()` | Global setup/teardown (once before/after all) |
| `cbelt_auto` | Auto-free annotation for heap pointers |
| `cbelt_defer(fn, arg)` | Schedule cleanup on scope exit |

### Assertions

| Macro | Fails if... |
| --- | --- |
| `cbelt_assert(expr)` | `expr` is falsy |
| `cbelt_assert_true(expr)` | Alias for `cbelt_assert(expr)` |
| `cbelt_assert_false(expr)` | `expr` is truthy |
| `cbelt_assert_equal(a, b)` | `a != b` (type-aware) |
| `cbelt_assert_not_equal(a, b)` | `a == b` (type-aware) |
| `cbelt_assert_str_equal(a, b)` | `strcmp(a, b) != 0` |
| `cbelt_assert_str_not_equal(a, b)` | `strcmp(a, b) == 0` |
| `cbelt_assert_null(ptr)` | `ptr != NULL` |
| `cbelt_assert_not_null(ptr)` | `ptr == NULL` |
| `cbelt_assert_mem_equal(a, b, sz)` | `memcmp(a, b, sz) != 0` (zero-size passes) |
| `cbelt_assert_in_range(v, min, max)` | `v < min` or `v > max` (inclusive) |

### Functions

| Function | Purpose |
| --- | --- |
| `cbelt_run_tests()` | Run all registered tests |
| `cbelt_run_all_tests()` | Run with default config (no CLI) |
| `cbelt_run_all_tests_with_args(argc, argv)` | Run with CLI flag parsing |
| `cbelt_set_default_timeout(seconds)` | Set global timeout (negative = no timeout) |
| `cbelt_set_output(buf, size)` | Redirect stdout to a char buffer |
| `cbelt_set_output_file(f)` | Redirect stdout to a `FILE*` |
| `cbelt_reset_output()` | Restore original stdout |
| `cbelt_register_test(...)` | Manual test registration (for MSVC) |

### Types

| Type | Values | Description |
| --- | --- | --- |
| `TestResult` | `TEST_SUCCESS` (0), `TEST_FAILURE` (1), `TEST_CRASH` (2) | Test outcome |

---

[Issues](https://github.com/Alex98235/CBelt/issues) · MIT License
