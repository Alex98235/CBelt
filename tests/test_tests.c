#include "../cbelt.h"

CBELT_GROUP("math_tests");

CBELT_TEST(addition) {
  cbelt_assert(2 + 2 == 4);
  return 0;
}

CBELT_TEST(subtraction) {
  cbelt_assert(5 - 3 == 2);
  return 0;
}

CBELT_TEST(standalone) {
  cbelt_assert(1 == 1);
  return 0;
}

CBELT_GROUP("string_tests");

CBELT_TEST(length) {
  cbelt_assert(strlen("hi") == 2);
  return 0;
}

CBELT_MAIN
