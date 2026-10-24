/* Self-tests for the cbelt framework itself.
 * Compile as a standalone executable.
 * Uses cbelt_set_output/cbelt_reset_output (pipe + dup2) to capture
 * and verify cbelt's own output. */

#define CBELT_IMPLEMENTATION
#include "../cbelt.h"
#include <string.h>

/* Capture buffer */
static char capture_buf[65536];

static int captured_contains(const char *s) {
    return strstr(capture_buf, s) != NULL;
}

/* Test harness state */
static int total_tests = 0;
static int total_failures = 0;

#define TEST(name)                                                             \
   do {                                                                        \
      total_tests++;                                                           \
      fprintf(stderr, "  %s...", name);                                        \
   } while (0)

#define PASS()                                                                 \
   fprintf(stderr, "PASS\n")

#define FAIL(msg)                                                              \
   do {                                                                        \
      total_failures++;                                                        \
      fprintf(stderr, "FAIL: %s\n", msg);                                      \
   } while (0)

#define ASSERT(cond, msg)                                                      \
   do {                                                                        \
      if (!(cond)) {                                                           \
         FAIL(msg);                                                            \
         return;                                                               \
      }                                                                        \
   } while (0)

/*=============================================================================
 * Test groups (registered for cbelt_run_tests)
 *============================================================================*/

CBELT_GROUP("alpha_group")

CBELT_TEST(alpha_one) {
   cbelt_assert(1);
   return TEST_SUCCESS;
}
CBELT_TEST(alpha_two) {
   cbelt_assert(1);
   return TEST_SUCCESS;
}

CBELT_GROUP("beta_group")

CBELT_TEST(beta_one) {
   cbelt_assert(1);
   return TEST_SUCCESS;
}

CBELT_GROUP("fail_group")

CBELT_TEST(fail_one) {
   cbelt_assert(0);
   return TEST_SUCCESS;
}

/* Group with multiple tests, only some match certain filters */
CBELT_GROUP("mixed_group")

CBELT_TEST(mixed_apple) {
   cbelt_assert(1);
   return TEST_SUCCESS;
}
CBELT_TEST(mixed_banana) {
   cbelt_assert(1);
   return TEST_SUCCESS;
}
CBELT_TEST(mixed_cherry) {
   cbelt_assert(1);
   return TEST_SUCCESS;
}

/*=============================================================================
 * Wrapper functions for assertion macro testing
 * cbelt_assert_* macros return TEST_FAILURE on failure, so wrap them
 * in sub-functions and check the return value.
 *============================================================================*/

static TestResult check_assert_true_false(void) {
   cbelt_assert_true(1);
   cbelt_assert_false(0);
   return TEST_SUCCESS;
}

static TestResult check_assert_not_equal_pass(void) {
   cbelt_assert_not_equal(1, 2);
   return TEST_SUCCESS;
}

static TestResult check_assert_in_range_bounds(void) {
   cbelt_assert_in_range(5, 5, 10);  /* at min */
   cbelt_assert_in_range(10, 5, 10); /* at max */
   cbelt_assert_in_range(7, 5, 10);  /* in middle */
   return TEST_SUCCESS;
}

static TestResult check_assert_str_equal_empty(void) {
   cbelt_assert_str_equal("", "");
   return TEST_SUCCESS;
}

static TestResult check_assert_mem_equal_zero(void) {
   int a = 42;
   cbelt_assert_mem_equal(&a, &a, 0);
   return TEST_SUCCESS;
}

static TestResult check_assert_str_equal_same_ptr(void) {
   const char *s = "hello";
   cbelt_assert_str_equal(s, s);
   return TEST_SUCCESS;
}

/*=============================================================================
 * Sandbox test groups: Linux only (fork-based)
 *============================================================================*/
#ifdef __linux__

CBELT_GROUP("sandbox_self_test")

CBELT_TEST_TIMEOUT(should_timeout, 1) {
   for (;;)
      ;
   return TEST_SUCCESS;
}

CBELT_TEST(should_crash) {
   /* Force a segfault to test crash detection */
   *(volatile int *)0 = 0;
   return TEST_SUCCESS;
}

#endif /* __linux__ */

/*=============================================================================
 * Self-tests
 *============================================================================*/

/* --- Filter tests --- */

static void test_filter_selects_group(void) {
   TEST("filter selects only matching group");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "alpha_group", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("=== alpha_group ==="),
          "should contain alpha_group header");
   ASSERT(captured_contains("alpha_one"), "should contain alpha_one");
   ASSERT(captured_contains("alpha_two"), "should contain alpha_two");
   ASSERT(!captured_contains("beta_group"), "should NOT contain beta_group");
   ASSERT(!captured_contains("fail_group"), "should NOT contain fail_group");
   PASS();
}

static void test_filter_test_name(void) {
   TEST("filter matches individual test name");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "alpha_one", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("alpha_one"), "should contain alpha_one");
   ASSERT(!captured_contains("alpha_two"), "should NOT contain alpha_two");
   PASS();
}

static void test_filter_group_dot_test(void) {
   TEST("filter matches group.test_name pattern");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "alpha_group.alpha_one", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("alpha_one"), "should contain alpha_one");
   ASSERT(!captured_contains("alpha_two"), "should NOT contain alpha_two");
   PASS();
}

static void test_filter_nonexistent(void) {
   TEST("filter with no matches shows empty summary");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "nonexistent", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success (0 tests ran)");
   ASSERT(captured_contains("0 passed"), "should show 0 passed");
   ASSERT(!captured_contains("alpha_group"), "should not show any group");
   PASS();
}

static void test_filter_empty_string(void) {
   TEST("empty filter runs all groups");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (fail_group present)");
   ASSERT(captured_contains("=== alpha_group ==="), "should run alpha_group");
   ASSERT(captured_contains("=== beta_group ==="), "should run beta_group");
   ASSERT(captured_contains("=== fail_group ==="), "should run fail_group");
   PASS();
}

static void test_filter_partial_group(void) {
   TEST("filter matches partial group name");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "alpha", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("=== alpha_group ==="), "should match alpha_group");
   ASSERT(captured_contains("alpha_one"), "should contain alpha_one");
   ASSERT(!captured_contains("beta_group"), "should NOT match beta_group");
   PASS();
}

static void test_filter_partial_test(void) {
   TEST("filter matches partial test name");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "alpha_on", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("alpha_one"), "should match alpha_one");
   ASSERT(!captured_contains("alpha_two"), "should NOT match alpha_two");
   PASS();
}

/* --- Quiet mode tests --- */

static void test_quiet_mode(void) {
   TEST("quiet mode hides passing tests, shows failures");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(5, (char*[]){"test", "-q", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (fail_group has a failing test)");
   ASSERT(captured_contains("fail_group"),
          "should contain failing group header");
   ASSERT(captured_contains("fail_one"), "should contain failing test name");
   ASSERT(!captured_contains("alpha_one"), "should NOT contain passing test");
   ASSERT(!captured_contains("beta_one"), "should NOT contain passing test");
   PASS();
}

static void test_quiet_no_failures(void) {
   TEST("quiet + all-pass filter shows only summary");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(7, (char*[]){"test", "-q", "-f", "alpha_group", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(!captured_contains("=== alpha_group ==="),
          "no group header in quiet all-pass");
   ASSERT(!captured_contains("alpha_one"), "no test name in quiet all-pass");
   ASSERT(captured_contains("2 passed"), "summary shows 2 passed");
   ASSERT(captured_contains("0 failed"), "summary shows 0 failed");
   PASS();
}

static void test_quiet_with_filter(void) {
   TEST("quiet + filter shows only matching failures");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(7, (char*[]){"test", "-q", "-f", "fail_group", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure");
   ASSERT(captured_contains("=== fail_group ==="),
          "failing group header shown");
   ASSERT(captured_contains("fail_one"), "failing test shown");
   ASSERT(!captured_contains("alpha_group"), "passing group not shown");
   PASS();
}

/* --- Spinner tests --- */

static void test_spinner_disabled(void) {
   TEST("spinner=0 skips progress output");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "--no-spinner", "--color", "never", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   /* With spinner disabled, no clear-line escape sequences should appear. */
   ASSERT(!captured_contains("\x1b[2K"), "no clear line with spinner off");
   ASSERT(captured_contains("alpha_one"), "test name still shown in result");
   PASS();
}

static void test_spinner_quiet(void) {
   TEST("spinner suppressed in quiet mode");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   /* Omit --no-spinner so spinner=1 default; quiet should suppress it */
   cbelt_parse_args(6, (char*[]){"test", "-q", "--color", "never", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   /* No test output at all in quiet + all-pass */
   ASSERT(!captured_contains("alpha_one"), "no test name in quiet all-pass");
   ASSERT(captured_contains("2 passed"), "summary shows 2 passed");
   PASS();
}

/* --- Color tests --- */

static void test_color_never(void) {
   TEST("color=never produces no ANSI codes");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "--no-spinner", "--color", "never", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(!captured_contains("\x1b["), "should not contain ANSI escape codes");
   PASS();
}

static void test_color_always(void) {
   TEST("color=always produces ANSI codes");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "--no-spinner", "--color", "always", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("\x1b["), "should contain ANSI escape codes");
   PASS();
}

/* --- Return value tests --- */

static void test_return_value_all_pass(void) {
   TEST("return value 0 when all pass");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "alpha_group", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected 0");
   PASS();
}

static void test_return_value_with_failures(void) {
   TEST("return value 1 when some fail");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "fail_group", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected 1");
   PASS();
}

/* --- Summary counts --- */

static void test_summary_counts(void) {
   TEST("summary shows correct pass/fail counts");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(4, (char*[]){"test", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure");
   /* alpha_group: 2 passed, beta_group: 1 passed, fail_group: 1 failed
      mixed_group: 3 passed */
   ASSERT(captured_contains("6 passed"), "expected 6 passed");
   ASSERT(captured_contains("1 failed"), "expected 1 failed");
   PASS();
}

/* --- Error message format --- */

static void test_error_message_has_file_line(void) {
   TEST("error message contains source file and line");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "fail_group", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure");
   ASSERT(captured_contains("Assertion failed"),
          "should contain assertion message");
   ASSERT(captured_contains("self_test.c"), "should contain source file name");
   PASS();
}

/* --- Assertion macro edge cases --- */

static void test_assert_true_false(void) {
   TEST("cbelt_assert_true and cbelt_assert_false pass");
   ASSERT(check_assert_true_false() == TEST_SUCCESS,
          "assert_true/false should pass");
   PASS();
}

static void test_assert_not_equal_pass(void) {
   TEST("cbelt_assert_not_equal with different values");
   ASSERT(check_assert_not_equal_pass() == TEST_SUCCESS,
          "not_equal should pass");
   PASS();
}

static void test_assert_in_range_bounds(void) {
   TEST("cbelt_assert_in_range at boundary values");
   ASSERT(check_assert_in_range_bounds() == TEST_SUCCESS,
          "in_range bounds should pass");
   PASS();
}

static void test_assert_str_equal_empty(void) {
   TEST("cbelt_assert_str_equal with empty strings");
   ASSERT(check_assert_str_equal_empty() == TEST_SUCCESS,
          "empty strings should pass");
   PASS();
}

static void test_assert_str_equal_same_ptr(void) {
   TEST("cbelt_assert_str_equal with same pointer");
   ASSERT(check_assert_str_equal_same_ptr() == TEST_SUCCESS,
          "same pointer should pass");
   PASS();
}

static void test_assert_mem_equal_zero(void) {
   TEST("cbelt_assert_mem_equal with zero size");
   ASSERT(check_assert_mem_equal_zero() == TEST_SUCCESS,
          "zero size should pass");
   PASS();
}

/* --- parse_args behavior tests --- */

static void test_parse_args_defaults(void) {
   TEST("cbelt_parse_args sets correct defaults (all groups run)");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(1, (char*[]){"test", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (fail_group has failure)");
   ASSERT(captured_contains("alpha_group"), "all groups should run with defaults");
   ASSERT(captured_contains("beta_group"), "all groups should run");
   ASSERT(captured_contains("fail_group"), "all groups should run");
   PASS();
}

static void test_parse_args_quiet_flag(void) {
   TEST("cbelt_parse_args handles -q flag (hides passing tests)");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(5, (char*[]){"test", "-q", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure");
   ASSERT(captured_contains("fail_one"), "failing test should appear");
   ASSERT(!captured_contains("alpha_one"), "passing test should be hidden");
   PASS();
}

static void test_parse_args_filter(void) {
   TEST("cbelt_parse_args handles -f flag");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "alpha_group", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("=== alpha_group ==="), "should contain alpha_group");
   ASSERT(!captured_contains("beta_group"), "should NOT contain beta_group");
   PASS();
}

static void test_parse_args_color(void) {
   TEST("cbelt_parse_args handles --color flag (never)");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "--color", "never", "--no-spinner", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(!captured_contains("\x1b["), "should not contain ANSI codes");
   PASS();
}

static void test_parse_args_color_auto(void) {
   TEST("cbelt_parse_args handles --color auto (no ANSI on pipe)");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "--color", "auto", "--no-spinner", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   /* Pipe is not a TTY, so auto behaves like never */
   ASSERT(!captured_contains("\x1b["), "should not contain ANSI codes on pipe");
   PASS();
}

static void test_parse_args_color_equals(void) {
   TEST("cbelt_parse_args handles --color=always");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(5, (char*[]){"test", "--color=always", "--no-spinner", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("\x1b["), "should contain ANSI codes");
   PASS();
}

static void test_parse_args_no_spinner(void) {
   TEST("cbelt_parse_args handles --no-spinner");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "--no-spinner", "--color", "never", "-f", "alpha_group", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 0, "expected success");
   ASSERT(captured_contains("alpha_one"), "test should run");
   ASSERT(captured_contains("alpha_two"), "test should run");
   PASS();
}

static void test_parse_args_jobs(void) {
   TEST("cbelt_parse_args handles -j flag");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-j", "4", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (fail_group present)");
   ASSERT(captured_contains("6 passed"), "all tests ran");
   PASS();
}

static void test_parse_args_jobs_invalid(void) {
   TEST("cbelt_parse_args handles -j 0 (defaults to 1)");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-j", "0", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (fail_group present)");
   ASSERT(captured_contains("6 passed"), "all tests ran");
   PASS();
}

static void test_parse_args_jobs_long(void) {
   TEST("cbelt_parse_args handles --jobs flag");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "--jobs", "8", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (fail_group present)");
   ASSERT(captured_contains("6 passed"), "all tests ran");
   PASS();
}

/* --- Config defaults --- */

static void test_set_default_timeout(void) {
   TEST("cbelt_set_default_timeout accepts valid values");
   /* Just verify the function doesn't crash with various inputs */
   cbelt_set_default_timeout(5);
   cbelt_set_default_timeout(-1); /* no timeout */
   cbelt_set_default_timeout(0);  /* zero = no change (keeps -1) */
   cbelt_set_default_timeout(30); /* restore default */
   PASS();
}

/* --- Sandbox tests (Linux only) --- */
#ifdef __linux__

static void test_sandbox_timeout(void) {
   TEST("sandbox detects test timeout");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "sandbox_self_test.should_timeout", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (timeout)");
   ASSERT(captured_contains("timed out"), "should mention timeout");
   PASS();
}

static void test_sandbox_crash(void) {
   TEST("sandbox detects test crash");
   cbelt_set_output(capture_buf, sizeof(capture_buf));
   cbelt_parse_args(6, (char*[]){"test", "-f", "sandbox_self_test.should_crash", "--no-spinner", "--color", "never", NULL});
   int ret = cbelt_run_tests();
   cbelt_reset_output();
   ASSERT(ret == 1, "expected failure (crash)");
   ASSERT(captured_contains("crashed with signal"),
          "should mention signal crash");
   PASS();
}

#endif /* __linux__ */

/*=============================================================================
 * Main
 *============================================================================*/
int main(void) {
   printf("=== cbelt self-tests ===\n\n");

   /* Filter tests */
   test_filter_selects_group();
   test_filter_test_name();
   test_filter_group_dot_test();
   test_filter_nonexistent();
   test_filter_empty_string();
   test_filter_partial_group();
   test_filter_partial_test();

   /* Quiet mode tests */
   test_quiet_mode();
   test_quiet_no_failures();
   test_quiet_with_filter();

   /* Spinner tests */
   test_spinner_disabled();
   test_spinner_quiet();

   /* Color tests */
   test_color_never();
   test_color_always();

   /* Return value tests */
   test_return_value_all_pass();
   test_return_value_with_failures();

   /* Summary counts */
   test_summary_counts();

   /* Error message format */
   test_error_message_has_file_line();

   /* Assertion macro edge cases */
   test_assert_true_false();
   test_assert_not_equal_pass();
   test_assert_in_range_bounds();
   test_assert_str_equal_empty();
   test_assert_str_equal_same_ptr();
   test_assert_mem_equal_zero();

   /* parse_args behavior tests */
   test_parse_args_defaults();
   test_parse_args_quiet_flag();
   test_parse_args_filter();
   test_parse_args_color();
   test_parse_args_color_auto();
   test_parse_args_color_equals();
   test_parse_args_no_spinner();
   test_parse_args_jobs();
   test_parse_args_jobs_invalid();
   test_parse_args_jobs_long();

   /* Config defaults */
   test_set_default_timeout();

   /* Sandbox tests (Linux only) */
#ifdef __linux__
   test_sandbox_timeout();
   test_sandbox_crash();
#endif

   printf("\n================================\n");
   printf("%d passed, %d failed\n", total_tests - total_failures,
          total_failures);

   return total_failures > 0 ? 1 : 0;
}
