#include "../cbelt.h"
#include <stdlib.h>

/*---------------------------------------------------------------------------
 * Tracking counters for setup/teardown verification
 *--------------------------------------------------------------------------*/
static int global_setup_called = 0;
static int global_teardown_called = 0;
static int group_setup_called = 0;
static int group_teardown_called = 0;
static int test_setup_called = 0;
static int test_teardown_called = 0;

/*---------------------------------------------------------------------------
 * Global setup / teardown
 *--------------------------------------------------------------------------*/
CBELT_GLOBAL_SETUP() {
   global_setup_called = 1;
   cbelt_set_default_timeout(10);
}

CBELT_GLOBAL_TEARDOWN() { global_teardown_called = 1; }

/*---------------------------------------------------------------------------
 * Group: math_tests
 *--------------------------------------------------------------------------*/
CBELT_GROUP("math_tests");

CBELT_GROUP_SETUP() { group_setup_called = 1; }

CBELT_GROUP_TEARDOWN() { group_teardown_called = 1; }

CBELT_SETUP() { test_setup_called = 1; }

CBELT_TEARDOWN() { test_teardown_called = 1; }

CBELT_TEST(addition) {
   cbelt_assert(2 + 2 == 4);
   cbelt_assert(100 + 200 == 300);
   cbelt_assert(-5 + 5 == 0);

   return TEST_SUCCESS;
}

CBELT_TEST(subtraction) {
   cbelt_assert(5 - 3 == 2);
   cbelt_assert(10 - 20 == -10);
   cbelt_assert(0 - 0 == 0);

   return TEST_SUCCESS;
}

CBELT_TEST(multiplication) {
   cbelt_assert(3 * 4 == 12);
   cbelt_assert(-2 * 6 == -12);
   cbelt_assert(7 * 0 == 0);

   return TEST_SUCCESS;
}

CBELT_TEST(division) {
   cbelt_assert(10 / 2 == 5);
   cbelt_assert(7 / 3 == 2); /* integer division */
   cbelt_assert(0 / 5 == 0);

   return TEST_SUCCESS;
}

CBELT_TEST(in_range) {
   cbelt_assert_in_range(4, 4, 4);
   cbelt_assert_in_range(4, 3, 4);
   cbelt_assert_in_range(101, 0, 500);

   return TEST_SUCCESS;
}

/*---------------------------------------------------------------------------
 * Group: equality_tests
 *--------------------------------------------------------------------------*/
CBELT_GROUP("equality_tests");

CBELT_TEST(cbelt_assert_equal_int) {
   cbelt_assert_equal(42, 42);
   cbelt_assert_equal(-1, -1);
   cbelt_assert_not_equal(-1, 42);
   cbelt_assert_equal(0, 0);

   return TEST_SUCCESS;
}

CBELT_TEST(cbelt_assert_equal_long) {
   cbelt_assert_equal(1000000L, 1000000L);
   cbelt_assert_not_equal(1000000L, -50000L);
   cbelt_assert_equal(-50000L, -50000L);

   return TEST_SUCCESS;
}

/*---------------------------------------------------------------------------
 * Group: bool_tests
 *--------------------------------------------------------------------------*/
CBELT_GROUP("bool_tests");

CBELT_TEST(true_expressions) {
   cbelt_assert(1);
   cbelt_assert(100);
   cbelt_assert(-1);

   return TEST_SUCCESS;
}

CBELT_TEST(comparisons) {
   int a = 5;
   int b = 5;
   int c = 10;
   cbelt_assert(a == b);
   cbelt_assert(b != c);
   cbelt_assert(a < c);
   cbelt_assert(c > a);
   cbelt_assert(a <= b);
   cbelt_assert(c >= b);

   return TEST_SUCCESS;
}

CBELT_TEST(logical_ops) {
   cbelt_assert(1 && 1);
   cbelt_assert(1 || 0);
   cbelt_assert(!0);

   return TEST_SUCCESS;
}

/*---------------------------------------------------------------------------
 * Group: pointer_tests
 *--------------------------------------------------------------------------*/
CBELT_GROUP("pointer_tests");

CBELT_TEST(null_check) {
   cbelt_auto p = NULL;
   cbelt_assert_null(p);
   p = malloc(1);
   cbelt_assert_not_null(p);

   return TEST_SUCCESS;
}

CBELT_TEST(string_comparison) {
   const char *s1 = "hello";
   const char *s2 = "hello";
   const char *s3 = "world";
   cbelt_assert_str_equal(s1, s2);
   cbelt_assert_str_not_equal(s1, s3);
   cbelt_assert_equal((int)strlen(s1), 5);
   cbelt_assert_equal(strcmp(s1, s2), 0);
   cbelt_assert_not_equal(strcmp(s1, s3), 0);

   return TEST_SUCCESS;
}

CBELT_TEST(mem_equality) {
   int data[4] = {1, 2, 3, 4};
   int expected[4] = {1, 2, 3, 4};
   cbelt_assert_mem_equal(expected, data, sizeof(data));

   return TEST_SUCCESS;
}

CBELT_TEST(mem_equality_intentional_failure) {
   cbelt_auto a = (char *)malloc(64 * sizeof(char));
   cbelt_auto b = (int *)malloc(64 * sizeof(int));

   cbelt_assert_mem_equal(a, b, 64 * sizeof(char));

   return TEST_SUCCESS;
}

/*---------------------------------------------------------------------------
 * Group: setup_teardown_tests
 * Tests that our setup/teardown hooks actually ran.
 * This group MUST be registered last so the counters are set.
 *--------------------------------------------------------------------------*/
CBELT_GROUP("setup_teardown_tests");

CBELT_TEST(global_setup_was_called) {
   cbelt_assert(global_setup_called == 1);

   return TEST_SUCCESS;
}

CBELT_TEST(global_teardown_will_be_called) {
   /* global_teardown hasn't run yet - it runs after all tests */
   cbelt_assert(global_teardown_called == 0);

   return TEST_SUCCESS;
}

CBELT_TEST(group_setup_was_called) {
   cbelt_assert(group_setup_called == 1);

   return TEST_SUCCESS;
}

CBELT_TEST(group_teardown_was_called) {
   /* group_teardown already ran after math_tests group completed */
   cbelt_assert(group_teardown_called == 1);

   return TEST_SUCCESS;
}

CBELT_TEST(test_setup_was_called) {
   /* test_setup runs before each test in math_tests */
   cbelt_assert(test_setup_called == 1);

   return TEST_SUCCESS;
}

CBELT_TEST(test_teardown_was_called) {
   /* test_teardown runs after each test in math_tests */
   cbelt_assert(test_teardown_called == 1);

   return TEST_SUCCESS;
}

CBELT_GROUP("null checks")

CBELT_TEST(is_null) {
   char *a = NULL;
   cbelt_assert_null(a);
   a = "Hello World!\n";
   cbelt_assert_not_null(a);

   return TEST_SUCCESS;
}

CBELT_GROUP("resource_cleanup");

static int defer_ran = 0;

static void cbelt_test_defer_fn(void *arg) {
   (void)arg;
   defer_ran = 1;
}

CBELT_TEST(cbelt_auto_frees_on_normal_return) {
   cbelt_auto p = malloc(64);
   cbelt_assert_not_null(p);
   /* If p is not freed on return, Valgrind/ASAN would report a leak.
      On normal return the cleanup runs automatically. */
   return TEST_SUCCESS;
}

CBELT_TEST(cbelt_auto_frees_on_assertion_failure) {
   cbelt_auto a = malloc(64);
   cbelt_assert_not_null(a);

   cbelt_auto b = malloc(64);
   cbelt_assert_not_null(b);

   /* Now force a failure. Both a and b should be freed on the early return. */
   int x = 1, y = 2;
   cbelt_assert(x == y);

   (void)a;
   (void)b;
   return TEST_SUCCESS;
}

CBELT_TEST(cbelt_auto_null_is_safe_to_free) {
   cbelt_auto p = NULL;

   return TEST_SUCCESS;
}

CBELT_TEST(cbelt_defer_runs_on_normal_return) {
   defer_ran = 0;
   cbelt_defer(cbelt_test_defer_fn, NULL);
   cbelt_assert(defer_ran == 0);
   return TEST_SUCCESS;
}

CBELT_TEST(cbelt_defer_runs_on_assertion_failure) {
   defer_ran = 0;
   cbelt_defer(cbelt_test_defer_fn, NULL);

   int x = 1, y = 2;
   cbelt_assert(x == y);

   (void)defer_ran;
   return TEST_SUCCESS;
}

CBELT_GROUP("uncategorized")

CBELT_TEST(test_intentional_failure) {
   cbelt_assert(1 == 0);

   return TEST_SUCCESS;
}
