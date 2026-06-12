#include "../cbelt.h"

CBELT_GROUP("Additional Tests")

CBELT_TEST(additional_test) { return TEST_SUCCESS; }

/*
CBELT_TEST_TIMEOUT(infinite_loop, 2) {
   for (;;)
      ;
   return TEST_SUCCESS;
}
*/
