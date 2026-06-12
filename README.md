# CBelt: A Header-Only C Unit Testing Framework

> The name is a stupid riff on _seatbelt_ -> _CeatBelt_ -> _CBelt_

CBelt is a minimal, single-header C unit testing framework that uses GCC/Clang constructor attributes to automatically register tests at load time, meaning no manual test registration, no test lists to maintain. Just write your tests and compile.

<!-- toc:start -->

- [CBelt: A Header-Only C Unit Testing Framework](#cbelt-a-header-only-c-unit-testing-framework)
  - [Features](#features)
  - [Installation](#installation)
  - [Quick Start](#quick-start)
    - [Directory Structure](#directory-structure)
    - [1. Create the runner file](#1-create-the-runner-file)
    - [2. Write your tests](#2-write-your-tests)
    - [3. Compile and run](#3-compile-and-run)
  - [Defining Your Own Tests](#defining-your-own-tests)
    - [Basic Assertions](#basic-assertions)
    - [Test Return Value](#test-return-value)
  - [Sandboxing and Timeouts](#sandboxing-and-timeouts)
    - [How Sandboxing Works](#how-sandboxing-works)
    - [Setting the Default Timeout](#setting-the-default-timeout)
    - [Per-Test Timeouts](#per-test-timeouts)
    - [Disabling Timeouts](#disabling-timeouts)
  - [Test Groups](#test-groups)
  - [Advanced Usage](#advanced-usage)
    - [Test-Level Setup and Teardown](#test-level-setup-and-teardown)
    - [Group-Level Setup and Teardown](#group-level-setup-and-teardown)
    - [Global Setup and Teardown](#global-setup-and-teardown)
    - [Full Lifecycle Order](#full-lifecycle-order)
    - [Multiple Test Files](#multiple-test-files)
    - [What Happens on Failure](#what-happens-on-failure)
  - [Resource Management](#resource-management)
    - [CBelt Auto](#cbelt_auto)
    - [CBelt Defer](#cbelt_defer)
  - [Configuration](#configuration)
    - [Available Options](#available-options)
    - [Usage](#usage)
  - [CBelt Compiler Support](#cbelt-compiler-support)
  - [API Reference](#api-reference)
    - [Macros](#macros)
    - [Functions](#functions)
    - [Types](#types)
  - [Bug Reports and Feature Requests](#bug-reports-and-feature-requests)
  - [License](#license)

<!-- toc:end -->

## Features

- **Single header**: drop `cbelt.h` into your project and start testing!
- **Automatic test registration**: `CBELT_TEST(name)` writes a constructor that registers itself before `main()` runs.
- **Test groups**: organize tests into named groups with `CBELT_GROUP("name")`.
- **Assertions**: `cbelt_assert(expr)` and `cbelt_assert_equal(expected, actual)` with automatic failure messages.
- **_Linux (GCC/Clang) features_**:
  - **Sandboxed execution**: tests run in isolated child processes via `fork()` so that crashes and signals don't bring down the whole suite.
  - **Timeouts**: configurable per-test and global timeouts; hung tests are detected and reported instead of hanging forever.
  - **Crash / signal reporting**: tests that segfault or crash are reported with the signal name and number.
  - **Error propagation across fork**: error messages from failed assertions in the child process are communicated back to the parent via a pipe.
  - `cbelt_auto` & `cbelt_defer` for memory management
- **Setup / teardown hooks**: per-test, per-group, and global lifecycle functions.
- **Colored output**: pass/fail results printed with ANSI colors in the terminal.
- **`CBELT_MAIN` macro**: provides a ready-to-use `main()` so you don't have to write one.

## Installation

CBelt uses **zero external dependencies** beyond the C standard library. To add it to your project:

1. **Copy `cbelt.h`** into your source tree (or clone the repo):

   ```bash
   git clone https://codeberg.org/Spiffy2903/CBelt.git
   ```

2. **Include the header** in your test files:

   ```c
   #include "cbelt.h"
   ```

3. **Define the implementation** in **exactly one** `.c` file in your project:

   ```c
   #define CBELT_IMPLEMENTATION
   #include "cbelt.h"
   ```

That's it. No linking, no configuration, no build system changes.

## Quick Start

### Directory Structure

A typical CBelt test setup looks like this:

```text
project/
├── src/
│   └── my_library.c
├── tests/
│   ├── test_suite.c     ← your tests
│   └── test_main.c      ← the implementation (CBELT_IMPLEMENTATION + CBELT_MAIN)
└── cbelt.h              ← the framework header
```

### 1. Create the runner file

Create a single `.c` file that provides the implementation and the `main()` function, or use the included `example_test_main.c` file:

```c
// tests/test_main.c
#define CBELT_IMPLEMENTATION
#include "../cbelt.h"

CBELT_MAIN
```

- `#define CBELT_IMPLEMENTATION` must be defined **before** including `cbelt.h` in exactly one translation unit.
- `CBELT_MAIN` expands to `int main(void) { return cbelt_run_all_tests(); }`.

### 2. Write your tests

```c
// tests/test_suite.c
#include "../cbelt.h"

CBELT_TEST(addition_works) {
    cbelt_assert(2 + 2 == 4);
    return TEST_SUCCESS;
}

CBELT_TEST(subtraction_works) {
    cbelt_assert(5 - 3 == 2);
    return TEST_SUCCESS;
}
```

Every test function must:

- Have the signature `TestResult func(void)` (handled by the macro).
- Return `TEST_SUCCESS` on success, `TEST_FAILURE` on failure, or `TEST_CRASH` if the test crashes.
- Use `cbelt_assert()` to validate conditions.

### 3. Compile and run

**Minimum C standard**: C11

Compile all test `.c` files together with any source files you want to test, link nothing else:

```bash
gcc -o run_tests tests/test_main.c tests/test_suite.c
./run_tests
```

You should see output like:

```text
=== (ungrouped) ===
  addition_works
  subtraction_works

================================
2 passed, 0 failed
```

Tests that are not placed into a group are collected under the default `(ungrouped)` group.

## Defining Your Own Tests

### Basic Assertions

```c
CBELT_TEST(my_test) {
    int value = 42;

    cbelt_assert(value == 42);           // any boolean expression
    cbelt_assert(value > 0);

    cbelt_assert_equal(42, value);       // compares two integers
    cbelt_assert_equal(100L, 100L);      // works with long integers too

    cbelt_auto str = cbelt_strdup("hello");
    cbelt_assert_not_null(str);
    cbelt_assert_str_equal("hello", str);

    int *ptr = NULL;
    cbelt_assert_null(ptr);

    int data[4] = {1, 2, 3, 4};
    int expected[4] = {1, 2, 3, 4};
    cbelt_assert_mem_equal(expected, data, sizeof(data));

    cbelt_assert_in_range(50, 0, 100);

    return TEST_SUCCESS;
}
```

All assertion macros store the error message into an internal buffer and return `TEST_FAILURE` (1) from the test function, which causes the framework to print a failure. See the [Assertions table](#assertions) in the API Reference for a complete list.

### Test Return Value

A test **must return `TEST_SUCCESS`** to indicate success. If any `cbelt_assert` fails, execution continues but the test ultimately returns `TEST_FAILURE` (via the assertion macros). You can also return early manually:

```c
CBELT_TEST(early_exit_on_failure) {
    cbelt_assert(some_condition());
    if (!some_other_condition()) {
        return TEST_FAILURE;  // manual failure
    }
    return TEST_SUCCESS;
}
```

## Sandboxing and Timeouts

On Linux, CBelt runs each test in an isolated child process via `fork()`. This means:

- A test that **segfaults** or triggers any other fatal signal will be caught and reported as a crash, without taking down the entire test runner.
- A test that **hangs** (e.g., infinite loop) will be killed after a configurable timeout.
- Error messages from failed assertions in the child process are communicated back to the parent via a pipe and printed as normal.

### How Sandboxing Works

When `cbelt_run_test_in_sandbox()` is called:

1. A pipe is created for error message communication.
2. `fork()` creates a child process.
3. The child runs the test function. If an assertion fails, the error message is written to the pipe and the child exits with the test result.
4. The parent reads any error message from the pipe, then waits for the child to finish.
5. If the child was killed by a signal (e.g., SIGSEGV, SIGABRT, SIGALRM), the signal is reported. If the signal was SIGALRM, the test is reported as a timeout.

On non-Linux platforms, sandboxing is not available and tests run directly in-process.

### Setting the Default Timeout

The default timeout for all tests is **30 seconds**. You can change this in `CBELT_GLOBAL_SETUP()`:

```c
CBELT_GLOBAL_SETUP() {
    cbelt_set_default_timeout(10);   // 10 second default timeout
}
```

Pass a **positive number** to set the timeout in seconds.
Pass a **negative number** to disable timeouts globally:

```c
cbelt_set_default_timeout(-1);   // no timeout by default
```

### Per-Test Timeouts

Use `CBELT_TEST_TIMEOUT(name, timeout_s)` to set a specific timeout for an individual test:

```c
CBELT_TEST_TIMEOUT(slow_database_query, 60) {
    // This test can take up to 60 seconds before being killed
    cbelt_assert(query_database() == OK);
    return TEST_SUCCESS;
}
```

A timeout of **0** means "use the global default". A timeout of **-1** means "no timeout" for that specific test.

### Disabling Timeouts

Use `CBELT_TEST_NO_TIMEOUT(name)` for a test that should never be killed:

```c
CBELT_TEST_NO_TIMEOUT(hangs_until_input) {
    // This test won't be timed out
    return TEST_SUCCESS;
}
```

This is equivalent to `CBELT_TEST_TIMEOUT(name, -1)`.

## Test Groups

Use `CBELT_GROUP("name")` to organize related tests. The group name applies to all `CBELT_TEST` blocks that follow it in source order.

```c
CBELT_GROUP("math");

CBELT_TEST(addition) {
    cbelt_assert(2 + 2 == 4);
    return TEST_SUCCESS;
}

CBELT_TEST(multiplication) {
    cbelt_assert(3 * 4 == 12);
    return TEST_SUCCESS;
}

CBELT_GROUP("strings");

CBELT_TEST(length) {
    cbelt_assert(strlen("hello") == 5);
    return TEST_SUCCESS;
}
```

Output:

```text
=== math ===
  addition
  multiplication

=== strings ===
  length

================================
3 passed, 0 failed
```

> **Important:** Groups are scoped by their position in the file. Every `CBELT_TEST` after a `CBELT_GROUP("name")` belongs to that group until another `CBELT_GROUP(...)` is encountered.

## Advanced Usage

### Test-Level Setup and Teardown

`CBELT_SETUP()` and `CBELT_TEARDOWN()` run before/after **each test** in the current group.

```c
static int counter = 0;

CBELT_GROUP("counter_tests");

CBELT_SETUP() {
    counter = 0;   // reset before each test
}

CBELT_TEARDOWN() {
    // cleanup after each test (e.g. free allocated memory)
}

CBELT_TEST(increment) {
    counter++;
    cbelt_assert(counter == 1);
    return TEST_SUCCESS;
}

CBELT_TEST(double_increment) {
    counter += 2;
    cbelt_assert(counter == 2);
    return TEST_SUCCESS;
}
```

### Group-Level Setup and Teardown

`CBELT_GROUP_SETUP()` runs once before any test in the group, and `CBELT_GROUP_TEARDOWN()` runs once after all tests in the group have completed.

```c
static int db_connected = 0;

CBELT_GROUP("database_tests");

CBELT_GROUP_SETUP() {
    db_connected = 1;   // e.g. open a database connection
}

CBELT_GROUP_TEARDOWN() {
    db_connected = 0;   // close the connection
}

CBELT_TEST(query) {
    cbelt_assert(db_connected);   // connection is already open
    return TEST_SUCCESS;
}
```

### Global Setup and Teardown

`CBELT_GLOBAL_SETUP()` runs once before **all** tests, and `CBELT_GLOBAL_TEARDOWN()` runs once after **all** tests have finished.

```c
CBELT_GLOBAL_SETUP() {
    // allocate global resources, seed random numbers, set default timeout, etc.
    srand(42);
    cbelt_set_default_timeout(10);
}

CBELT_GLOBAL_TEARDOWN() {
    // clean up global resources
}
```

### Full Lifecycle Order

For each group, hooks execute in this order:

1. `CBELT_GLOBAL_SETUP()` once before all groups.
2. For each group:
   1. `CBELT_GROUP_SETUP()` once per group.
   2. For each test in the group:
      - `CBELT_SETUP()` before the test.
      - The test function itself (sandboxed on Linux).
      - `CBELT_TEARDOWN()` after the test.
   3. `CBELT_GROUP_TEARDOWN()` once per group.
3. `CBELT_GLOBAL_TEARDOWN()` once after all groups.

### Multiple Test Files

You can split your tests across multiple `.c` files. All test files share the same registration system, as long as they all `#include "cbelt.h"`. Only one file should define `CBELT_IMPLEMENTATION`.

```text
tests/
├── test_math.c
├── test_strings.c
├── test_main.c          ← CBELT_IMPLEMENTATION + CBELT_MAIN here
└── cbelt.h
```

Compile them all together:

```bash
gcc -o run_tests tests/test_main.c tests/test_math.c tests/test_strings.c
```

You can set a global default timeout for **all** your test files via `CBELT_GLOBAL_SETUP()` in **any** test file:

```c
// tests/test_math.c
#include "../cbelt.h"

CBELT_GLOBAL_SETUP() {
    cbelt_set_default_timeout(5);  // 5 second timeout for all tests
}
```

> **Note:** If multiple files define `CBELT_GLOBAL_SETUP()`, all of them will run. The order across translation units is not guaranteed.

### What Happens on Failure

When an assertion fails, the framework:

1. Stores a descriptive error message (expression + file + line) in an internal buffer.
2. Prints the test name in **red**.
3. Prints an indented error message below it.

When a test **crashes** (e.g., segfault, illegal instruction), the framework:

1. Prints the test name in **red**.
2. Prints the signal number and name (e.g., `Test crashed with signal: 11 (Segmentation fault)`).

When a test **times out**, the framework:

1. Prints the test name in **red**.
2. Prints a message like `Test timed out after 10 seconds (infinite loop or hang)`.

Example output with failures:

```text
=== (ungrouped) ===
  passing_test
  failing_test
    Assertion failed: 1 == 0 at tests/test_suite.c:17

=== crash_tests ===
  will_segfault
    Test crashed with signal: 11 (Segmentation fault)

=== timeout_tests ===
  infinite_loop
    Test timed out after 5 seconds (infinite loop or hang)

================================
1 passed, 2 failed
```

The program exits with exit code **1** if **any** test fails for **any reason**, and **0** if all pass.

### Configuration

CBelt's output behavior can be customized using configuration macros. Define these **before** including `cbelt.h` or pass them as compiler flags with `-D`.

#### Available Options

| Macro                   | Effect                                                |
| ----------------------- | ----------------------------------------------------- |
| `CBELT_DISABLE_SPINNER` | Disables animated spinner entirely                    |
| `CBELT_DISABLE_COLOR`   | Disables ANSI colors, adds `PASSED`/`FAILED` suffixes |

#### Usage

**In your source file:**

```c
#define CBELT_DISABLE_SPINNER
#define CBELT_DISABLE_COLOR
#include "cbelt.h"
```

Or via compiler flags:

```bash
gcc -DCBELT_DISABLE_SPINNER -DCBELT_DISABLE_COLOR test_main.c tests.c -o tests
```

Or project-wide: Uncomment the corresponding lines at the top of `cbelt.h`:

```c
// #define CBELT_DISABLE_SPINNER
// #define CBELT_DISABLE_COLOR
```

Both options work independently and can be combined. By default, both spinners and colors are enabled.

## Resource Management

When an assertion fails, the framework calls `return TEST_FAILURE;` from inside the assertion macro, which means any cleanup code written _after_ the assertion in the test is skipped. This can leak memory, file handles, or other resources.

CBelt provides two zero-overhead tools to solve this, using GCC/Clang's `__attribute__((cleanup))` extension:

### `cbelt_auto`

Declare a heap-allocated pointer with `cbelt_auto` instead of `void *`, and it is automatically freed when the enclosing scope exits, whether by normal return, assertion failure, `goto`, or any other path.

```c
CBELT_TEST(my_test) {
   cbelt_auto buffer = malloc(4096);      // ← auto-freed on scope exit
   cbelt_auto items  = calloc(10, sizeof(int)); // ← also auto-freed

   cbelt_assert_not_null(buffer);
   cbelt_assert_not_null(items);

   cbelt_assert(buffer[0] == 0);          // If this fails → return → both freed automatically

   // No manual free() needed
   return TEST_SUCCESS;
}
```

`cbelt_auto` is just a type annotation. It expands to `__attribute__((cleanup(cbelt_defer_free))) void *`. The compiler inserts the `free()` call at every scope exit point. There is **no runtime overhead** on the success path beyond the `free()` call itself.

### `cbelt_defer`

For non-malloc resources (file descriptors, mutexes, custom free functions), use `cbelt_defer(fn, arg)` to schedule an arbitrary cleanup callback:

```c
#include <stdio.h>

CBELT_TEST(file_read) {
   FILE *fp = fopen("data.txt", "r");
   cbelt_assert_not_null(fp);
   cbelt_defer(fclose, fp);   // ← fclose(fp) called on scope exit

   char buf[256];
   cbelt_assert(fgets(buf, sizeof(buf), fp) != NULL);
   cbelt_assert(buf[0] != '\0');

   // If either assertion above fails → return → fclose(fp) runs automatically
   return TEST_SUCCESS;  // Normal exit → fclose(fp) also runs
}
```

The callback is guaranteed to run exactly once when the enclosing block exits, regardless of how it exits.

> **Note:** On Linux, the sandboxing (`fork()`) already reclaims all test process memory on exit, so `cbelt_auto` is technically redundant there. However, it is still recommended for:
>
> - Safety when `fork()` fails (the fallback path runs in-process)
> - Valgrind/ASAN-clean test suites (tools trace allocations inside the child)
> - Portable tests that may run on non-Linux targets
> - Catching logic errors (e.g., double-free from manual `free()` + auto-cleanup)

`cbelt_auto` and `cbelt_defer` use `__attribute__((cleanup))` and are only available on **GCC and Clang**. On MSVC and other compilers without this attribute, they are no-ops. The macros expand to a plain `void *` declaration and a no-op statement respectively, so your code will compile but will **not** automatically free resources on scope exit.

## CBelt Compiler Support

| Compiler | Auto-registration | Sandboxing | Resource Management (`cbelt_auto` / `cbelt_defer`) | Notes                                                                                                                                           |
| -------- | ----------------- | ---------- | -------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| GCC      | ✅                | ✅ (Linux) | ✅                                                 | Uses `__attribute__((constructor))` and `__attribute__((cleanup))`                                                                              |
| Clang    | ✅                | ✅ (Linux) | ✅                                                 | Same mechanisms                                                                                                                                 |
| MSVC     | ❌                | ❌         | ❌                                                 | `CBELT_GROUP`, `CBELT_SETUP`, `CBELT_TEARDOWN` etc. become no-ops; tests must be manually registered; `cbelt_auto` and `cbelt_defer` are no-ops |

On compilers without constructor attribute support, the `CBELT_TEST` macro still works but you must manually call `cbelt_register_test()` to register each test. The `CBELT_GROUP` and lifecycle macros (`CBELT_SETUP`, `CBELT_TEARDOWN`, etc.) become no-ops.

## API Reference

### Macros

| Macro                                 | Purpose                                                                                                  |
| ------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| `CBELT_MAIN`                          | Defines `main()` that calls `cbelt_run_all_tests()`.                                                     |
| `CBELT_GROUP(name)`                   | Sets the active group name for subsequent tests.                                                         |
| `CBELT_TEST(name)`                    | Defines a test function and automatically registers it.                                                  |
| `CBELT_TEST_TIMEOUT(name, timeout_s)` | Defines a test with a specific timeout in seconds. 0 = use default, negative = no timeout.               |
| `CBELT_TEST_NO_TIMEOUT(name)`         | Defines a test with no timeout (runs until completion).                                                  |
| `CBELT_SETUP()`                       | Defines a per-test setup function for the current group.                                                 |
| `CBELT_TEARDOWN()`                    | Defines a per-test teardown function for the current group.                                              |
| `CBELT_GROUP_SETUP()`                 | Defines a group-level setup function.                                                                    |
| `CBELT_GROUP_TEARDOWN()`              | Defines a group-level teardown function.                                                                 |
| `CBELT_GLOBAL_SETUP()`                | Defines a global setup function (runs once before all tests).                                            |
| `CBELT_GLOBAL_TEARDOWN()`             | Defines a global teardown function (runs once after all tests).                                          |
| `cbelt_auto`                          | Type annotation that auto-frees a heap-allocated pointer on scope exit using `__attribute__((cleanup))`. |
| `cbelt_defer(fn, arg)`                | Schedules a cleanup callback to run on scope exit.                                                       |

### Assertions

| Macro                                      | Purpose                                                                                                  |
| ------------------------------------------ | -------------------------------------------------------------------------------------------------------- |
| `cbelt_assert(expr)`                       | Assert that `expr` is truthy. Prints the expression, file, and line on failure.                          |
| `cbelt_assert_true(expr)`                  | Alias for `cbelt_assert(expr)`.                                                                          |
| `cbelt_assert_false(expr)`                 | Assert that `expr` is falsy.                                                                             |
| `cbelt_assert_equal(expected, actual)`     | Type-aware equality assertion. Compares values of any comparable type and prints both values on failure. |
| `cbelt_assert_not_equal(expected, actual)` | Type-aware inequality assertion. Fails if `expected == actual`.                                          |
| `cbelt_assert_str_equal(str1, str2)`       | String equality assertion using `strcmp()`. Handles NULL pointers.                                       |
| `cbelt_assert_str_not_equal(str1, str2)`   | String inequality assertion using `strcmp()`. Handles NULL pointers.                                     |
| `cbelt_assert_null(ptr)`                   | Assert that a pointer is NULL.                                                                           |
| `cbelt_assert_not_null(ptr)`               | Assert that a pointer is non-NULL.                                                                       |
| `cbelt_assert_mem_equal(ptr1, ptr2, size)` | Memory equality assertion using `memcmp()`. Handles NULL pointers.                                       |
| `cbelt_assert_in_range(value, min, max)`   | Assert that `value` is within the inclusive range `[min, max]`.                                          |

### Functions

| Function                                      | Purpose                                                                                                  |
| --------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| `int cbelt_run_all_tests(void)`               | Runs all registered tests and returns 0 on success, 1 on failure. Called automatically by `CBELT_MAIN`.  |
| `void cbelt_register_test(...)`               | Manually register a test (useful on compilers without constructor support).                              |
| `void cbelt_register_test_with_timeout(...)`  | Manually register a test with a specific timeout.                                                        |
| `void cbelt_set_default_timeout(int seconds)` | Set the default timeout for all tests. Positive = seconds, negative = no timeout. Default is 30 seconds. |

### Types

| Type              | Values                            | Description                                         |
| ----------------- | --------------------------------- | --------------------------------------------------- |
| `TestResult`      | `TEST_SUCCESS` (0)                | Test passed.                                        |
|                   | `TEST_FAILURE` (1)                | Test failed (assertion failed or returned failure). |
|                   | `TEST_CRASH` (2)                  | Test crashed or timed out.                          |
| `test_func_t`     | `TestResult (*test_func_t)(void)` | Signature for test functions.                       |
| `setup_func_t`    | `void (*setup_func_t)(void)`      | Signature for setup functions.                      |
| `teardown_func_t` | `void (*teardown_func_t)(void)`   | Signature for teardown functions.                   |

## Bug Reports and Feature Requests

If you find an issue or have an idea for an improvement, please open an [issue](https://codeberg.org/Spiffy2903/CBelt/issues).

## License

MIT License.

See the [LICENSE](LICENSE) file for details.
