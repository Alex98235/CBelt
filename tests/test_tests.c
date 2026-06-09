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
   return 0;
}

CBELT_TEST(subtraction) {
   cbelt_assert(5 - 3 == 2);
   cbelt_assert(10 - 20 == -10);
   cbelt_assert(0 - 0 == 0);
   return 0;
}

CBELT_TEST(multiplication) {
   cbelt_assert(3 * 4 == 12);
   cbelt_assert(-2 * 6 == -12);
   cbelt_assert(7 * 0 == 0);
   return 0;
}

CBELT_TEST(division) {
   cbelt_assert(10 / 2 == 5);
   cbelt_assert(7 / 3 == 2); /* integer division */
   cbelt_assert(0 / 5 == 0);
   return 0;
}

/*---------------------------------------------------------------------------
 * Group: equality_tests
 *--------------------------------------------------------------------------*/
CBELT_GROUP("equality_tests");

CBELT_TEST(cbelt_assert_equal_int) {
   cbelt_assert_equal(42, 42);
   cbelt_assert_equal(-1, -1);
   cbelt_assert_equal(0, 0);
   return 0;
}

CBELT_TEST(cbelt_assert_equal_long) {
   cbelt_assert_equal(1000000L, 1000000L);
   cbelt_assert_equal(-50000L, -50000L);
   return 0;
}

/*---------------------------------------------------------------------------
 * Group: bool_tests
 *--------------------------------------------------------------------------*/
CBELT_GROUP("bool_tests");

CBELT_TEST(true_expressions) {
   cbelt_assert(1);
   cbelt_assert(100);
   cbelt_assert(-1);
   return 0;
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
   return 0;
}

CBELT_TEST(logical_ops) {
   cbelt_assert(1 && 1);
   cbelt_assert(1 || 0);
   cbelt_assert(!0);
   return 0;
}

/*---------------------------------------------------------------------------
 * Group: pointer_tests
 *--------------------------------------------------------------------------*/
CBELT_GROUP("pointer_tests");

CBELT_TEST(null_check) {
   void *p = NULL;
   cbelt_assert(p == NULL);
   p = malloc(1);
   cbelt_assert(p != NULL);
   free(p);
   return 0;
}

CBELT_TEST(string_comparison) {
   const char *s1 = "hello";
   const char *s2 = "hello";
   const char *s3 = "world";
   cbelt_assert(s1 == s2); /* string interning may make this true */
   cbelt_assert(s1 != s3);
   cbelt_assert(strlen(s1) == 5);
   cbelt_assert(strcmp(s1, s2) == 0);
   cbelt_assert(strcmp(s1, s3) != 0);
   return 0;
}

/*---------------------------------------------------------------------------
 * Group: setup_teardown_tests
 * Tests that our setup/teardown hooks actually ran.
 * This group MUST be registered last so the counters are set.
 *--------------------------------------------------------------------------*/
CBELT_GROUP("setup_teardown_tests");

CBELT_TEST(global_setup_was_called) {
   cbelt_assert(global_setup_called == 1);
   return 0;
}

CBELT_TEST(global_teardown_will_be_called) {
   /* global_teardown hasn't run yet - it runs after all tests */
   cbelt_assert(global_teardown_called == 0);
   return 0;
}

CBELT_TEST(group_setup_was_called) {
   cbelt_assert(group_setup_called == 1);
   return 0;
}

CBELT_TEST(group_teardown_was_called) {
   /* group_teardown already ran after math_tests group completed */
   cbelt_assert(group_teardown_called == 1);
   return 0;
}

CBELT_TEST(test_setup_was_called) {
   /* test_setup runs before each test in math_tests */
   cbelt_assert(test_setup_called == 1);
   return 0;
}

CBELT_TEST(test_teardown_was_called) {
   /* test_teardown runs after each test in math_tests */
   cbelt_assert(test_teardown_called == 1);
   return 0;
}

CBELT_GROUP("uncategorized")

CBELT_TEST(test_intentional_failure) {
   cbelt_assert(1 == 0);
   return 0;
}
