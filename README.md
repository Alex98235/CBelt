# CBelt: A Header-Only C Unit Testing Framework

> The name is a stupid riff on *seatbelt* -> *CeatBelt* -> *CBelt*

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
  - [Test Groups](#test-groups)
  - [Advanced Usage](#advanced-usage)
    - [Test-Level Setup and Teardown](#test-level-setup-and-teardown)
    - [Group-Level Setup and Teardown](#group-level-setup-and-teardown)
    - [Global Setup and Teardown](#global-setup-and-teardown)
    - [Full Lifecycle Order](#full-lifecycle-order)
    - [Multiple Test Files](#multiple-test-files)
    - [What Happens on Failure](#what-happens-on-failure)
  - [Compiler Support](#compiler-support)
  - [API Reference](#api-reference)
    - [Macros](#macros)
    - [Functions](#functions)
  - [Bug Reports and Feature Requests](#bug-reports-and-feature-requests)
  - [License](#license)

<!-- toc:end -->

## Features

- **Single header**: drop `cbelt.h` into your project, no build system changes needed.
- **Automatic test registration**: `CBELT_TEST(name)` writes a constructor that registers itself before `main()` runs.
- **Test groups**: organize tests into named groups with `CBELT_GROUP("name")`.
- **Assertions**: `cbelt_assert(expr)` and `cbelt_assert_equal(expected, actual)` with automatic failure messages.
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
    return 0;
}

CBELT_TEST(subtraction_works) {
    cbelt_assert(5 - 3 == 2);
    return 0;
}
```

Every test function must:

- Have the signature `int func(void)` (handled by the macro).
- Return `0` on success or `1` on failure.
- Use `cbelt_assert()` to validate conditions.

### 3. Compile and run

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

    return 0;
}
```

- `cbelt_assert(expr)` fails if `expr` is false. Prints the expression, file, and line number on failure.
- `cbelt_assert_equal(expected, actual)` fails if `expected != actual`. Prints both values, the file, and the line number on failure.

Both macros store the error message into an internal buffer and return `1` from the test function, which causes the framework to print a failure.

### Test Return Value

A test **must return 0** to indicate success. If any `cbelt_assert` fails, execution continues but the test ultimately returns 1 (via the assertion macros). You can also return early manually:

```c
CBELT_TEST(early_exit_on_failure) {
    cbelt_assert(some_condition());
    if (!some_other_condition()) {
        return 1;  // manual failure
    }
    return 0;
}
```

## Test Groups

Use `CBELT_GROUP("name")` to organize related tests. The group name applies to all `CBELT_TEST` blocks that follow it in source order.

```c
CBELT_GROUP("math");

CBELT_TEST(addition) {
    cbelt_assert(2 + 2 == 4);
    return 0;
}

CBELT_TEST(multiplication) {
    cbelt_assert(3 * 4 == 12);
    return 0;
}

CBELT_GROUP("strings");

CBELT_TEST(length) {
    cbelt_assert(strlen("hello") == 5);
    return 0;
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
    return 0;
}

CBELT_TEST(double_increment) {
    counter += 2;
    cbelt_assert(counter == 2);
    return 0;
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
    return 0;
}
```

### Global Setup and Teardown

`CBELT_GLOBAL_SETUP()` runs once before **all** tests, and `CBELT_GLOBAL_TEARDOWN()` runs once after **all** tests have finished.

```c
CBELT_GLOBAL_SETUP() {
    // allocate global resources, seed random numbers, etc.
    srand(42);
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
      - The test function itself.
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

### What Happens on Failure

When an assertion fails, the framework:

1. Stores a descriptive error message (expression + file + line) in an internal buffer.
2. Prints the test name in **red**.
3. Prints an indented error message below it.

Example output with a failure:

```text
=== (ungrouped) ===
  passing_test
  failing_test
    Assertion failed: 1 == 0 at tests/test_suite.c:17

================================
1 passed, 1 failed
```

The program exits with exit code **1** if any test fails, and **0** if all pass.

## Compiler Support

| Compiler | Auto-registration | Notes |
| --- | --- | --- |
| GCC | ✅ | Uses `__attribute__((constructor))` |
| Clang | ✅ | Same mechanism |
| MSVC | ❌ | `CBELT_GROUP`, `CBELT_SETUP`, `CBELT_TEARDOWN` etc. become no-ops; tests must be manually registered |

On compilers without constructor attribute support, the `CBELT_TEST` macro still works but you must manually call `cbelt_register_test()` to register each test. The `CBELT_GROUP` and lifecycle macros (`CBELT_SETUP`, `CBELT_TEARDOWN`, etc.) become no-ops.

## API Reference

### Macros

| Macro | Purpose |
| --- | --- |
| `CBELT_MAIN` | Defines `main()` that calls `cbelt_run_all_tests()`. |
| `CBELT_GROUP(name)` | Sets the active group name for subsequent tests. |
| `CBELT_TEST(name)` | Defines a test function and automatically registers it. |
| `CBELT_SETUP()` | Defines a per-test setup function for the current group. |
| `CBELT_TEARDOWN()` | Defines a per-test teardown function for the current group. |
| `CBELT_GROUP_SETUP()` | Defines a group-level setup function. |
| `CBELT_GROUP_TEARDOWN()` | Defines a group-level teardown function. |
| `CBELT_GLOBAL_SETUP()` | Defines a global setup function (runs once before all tests). |
| `CBELT_GLOBAL_TEARDOWN()` | Defines a global teardown function (runs once after all tests). |
| `cbelt_assert(expr)` | Assert that `expr` is truthy. |
| `cbelt_assert_equal(expected, actual)` | Assert that two integers are equal. |

### Functions

| Function | Purpose |
| --- | --- |
| `int cbelt_run_all_tests(void)` | Runs all registered tests and returns 0 on success, 1 on failure. Called automatically by `CBELT_MAIN`. |
| `void cbelt_register_test(...)` | Manually register a test (useful on compilers without constructor support). |

## Bug Reports and Feature Requests

If you find an issue or have an idea for an improvement, please open an issue on the [repository](https://codeberg.org/Spiffy2903/CBelt/issues).

## License

MIT License.

See the [LICENSE](LICENSE) file for details.
