#ifndef CBELT_H
#define CBELT_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * ANSI color codes for terminal output
 *--------------------------------------------------------------------------*/
#define CBELT_COLOR_RED "\x1b[31m"
#define CBELT_COLOR_GREEN "\x1b[32m"
#define CBELT_COLOR_YELLOW "\x1b[33m"
#define CBELT_COLOR_CYAN "\x1b[36m"
#define CBELT_COLOR_BOLD "\x1b[1m"
#define CBELT_COLOR_RESET "\x1b[0m"

/*---------------------------------------------------------------------------
 * Helper macros for unique names
 *--------------------------------------------------------------------------*/
#define CBELT_CONCAT(a, b) a##b
#define CBELT_CONCAT2(a, b) CBELT_CONCAT(a, b)
#define CBELT_UNIQUE(prefix) CBELT_CONCAT2(prefix, __LINE__)

/*---------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

typedef enum { TEST_SUCCESS, TEST_FAILURE, TEST_CRASH } TestResult;

typedef void (*setup_func_t)(void);
typedef void (*teardown_func_t)(void);
typedef TestResult (*test_func_t)(void);

typedef struct cbelt_test {
  const char *name;
  test_func_t func;
  struct cbelt_test *next;
} cbelt_test_t;

typedef struct cbelt_group {
  const char *name;
  setup_func_t setup;
  teardown_func_t teardown;
  setup_func_t group_setup;
  teardown_func_t group_teardown;
  struct cbelt_test *tests;
  struct cbelt_group *next;
  size_t test_count;
} cbelt_group_t;

/*---------------------------------------------------------------------------
 * Public API declarations
 *--------------------------------------------------------------------------*/
void cbelt_register_test(const char *group_name, const char *test_name,
                         test_func_t func);
void cbelt_set_group_setup(const char *group_name, setup_func_t func);
void cbelt_set_group_teardown(const char *group_name, teardown_func_t func);
void cbelt_set_test_setup(const char *group_name, setup_func_t func);
void cbelt_set_test_teardown(const char *group_name, teardown_func_t func);
void cbelt_set_global_setup(setup_func_t func);
void cbelt_set_global_teardown(teardown_func_t func);
int cbelt_run_all_tests(void);

/* Each non-implementation translation unit gets its own file-scope
   cbelt_current_group_name so that constructors in one .c file never
   interfere with constructors in another. The implementation TU uses
   a genuine global (defined below). */
#ifndef CBELT_IMPLEMENTATION
static const char *cbelt_current_group_name = NULL;
extern char cbelt_error_buf[512];
#endif

/*---------------------------------------------------------------------------
 * Macros for GCC/Clang
 *--------------------------------------------------------------------------*/
#if defined(__GNUC__) || defined(__clang__)

/* Each CBELT_GROUP sets the per-TU current group name via a constructor.
   __COUNTER__ gives monotonically increasing values in source order, so using
   it as the constructor priority guarantees all constructors execute in source
   order within a translation unit.
   Base 200 avoids the reserved 0-100 range. */
#define CBELT_GROUP(name)                                                      \
  static void __attribute__((constructor(200 + __COUNTER__))) CBELT_UNIQUE(    \
      _cbelt_set_group_)(void) {                                               \
    cbelt_current_group_name = name;                                           \
  }

#define CBELT_TEST(name)                                                       \
  static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void);                    \
  static void __attribute__((constructor(200 + __COUNTER__))) CBELT_UNIQUE(    \
      _cbelt_register_##name)(void) {                                          \
    cbelt_register_test(cbelt_current_group_name, #name,                       \
                        CBELT_UNIQUE(_cbelt_test_##name));                     \
  }                                                                            \
  static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void)

#define CBELT_GROUP_SETUP()                                                    \
  static void CBELT_UNIQUE(_cbelt_group_setup)(void);                          \
  static void __attribute__((constructor(200 + __COUNTER__))) CBELT_UNIQUE(    \
      _cbelt_register_group_setup)(void) {                                     \
    cbelt_set_group_setup(cbelt_current_group_name,                            \
                          CBELT_UNIQUE(_cbelt_group_setup));                   \
  }                                                                            \
  static void CBELT_UNIQUE(_cbelt_group_setup)(void)

#define CBELT_GROUP_TEARDOWN()                                                 \
  static void CBELT_UNIQUE(_cbelt_group_teardown)(void);                       \
  static void __attribute__((constructor(200 + __COUNTER__))) CBELT_UNIQUE(    \
      _cbelt_register_group_teardown)(void) {                                  \
    cbelt_set_group_teardown(cbelt_current_group_name,                         \
                             CBELT_UNIQUE(_cbelt_group_teardown));             \
  }                                                                            \
  static void CBELT_UNIQUE(_cbelt_group_teardown)(void)

#define CBELT_SETUP()                                                          \
  static void CBELT_UNIQUE(_cbelt_setup)(void);                                \
  static void __attribute__((constructor(200 + __COUNTER__))) CBELT_UNIQUE(    \
      _cbelt_register_setup)(void) {                                           \
    cbelt_set_test_setup(cbelt_current_group_name,                             \
                         CBELT_UNIQUE(_cbelt_setup));                          \
  }                                                                            \
  static void CBELT_UNIQUE(_cbelt_setup)(void)

#define CBELT_TEARDOWN()                                                       \
  static void CBELT_UNIQUE(_cbelt_teardown)(void);                             \
  static void __attribute__((constructor(200 + __COUNTER__))) CBELT_UNIQUE(    \
      _cbelt_register_teardown)(void) {                                        \
    cbelt_set_test_teardown(cbelt_current_group_name,                          \
                            CBELT_UNIQUE(_cbelt_teardown));                    \
  }                                                                            \
  static void CBELT_UNIQUE(_cbelt_teardown)(void)

#define CBELT_GLOBAL_SETUP()                                                   \
  static void CBELT_UNIQUE(_cbelt_global_setup)(void);                         \
  static void __attribute__((constructor)) CBELT_UNIQUE(                       \
      _cbelt_register_global_setup)(void) {                                    \
    cbelt_set_global_setup(CBELT_UNIQUE(_cbelt_global_setup));                 \
  }                                                                            \
  static void CBELT_UNIQUE(_cbelt_global_setup)(void)

#define CBELT_GLOBAL_TEARDOWN()                                                \
  static void CBELT_UNIQUE(_cbelt_global_teardown)(void);                      \
  static void __attribute__((constructor)) CBELT_UNIQUE(                       \
      _cbelt_register_global_teardown)(void) {                                 \
    cbelt_set_global_teardown(CBELT_UNIQUE(_cbelt_global_teardown));           \
  }                                                                            \
  static void CBELT_UNIQUE(_cbelt_global_teardown)(void)

/*---------------------------------------------------------------------------
 * Fallback for other compilers
 *--------------------------------------------------------------------------*/
#else

#define CBELT_GROUP(name)
#define CBELT_TEST(name)                                                       \
  static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void)
#define CBELT_GROUP_SETUP()
#define CBELT_GROUP_TEARDOWN()
#define CBELT_SETUP()
#define CBELT_TEARDOWN()
#define CBELT_GLOBAL_SETUP()
#define CBELT_GLOBAL_TEARDOWN()

#endif

/*---------------------------------------------------------------------------
 * Assertions (defer messages to cbelt_error_buf instead of printing)
 *--------------------------------------------------------------------------*/
#define cbelt_assert(expr)                                                     \
  do {                                                                         \
    if (!(expr)) {                                                             \
      snprintf(cbelt_error_buf, sizeof(cbelt_error_buf),                       \
               "Assertion failed: %s at %s:%d", #expr, __FILE__, __LINE__);    \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#define cbelt_assert_equal(expected, actual)                                   \
  do {                                                                         \
    if ((expected) != (actual)) {                                              \
      snprintf(cbelt_error_buf, sizeof(cbelt_error_buf),                       \
               "Assertion failed: expected %lld, got %lld at %s:%d",           \
               (long long)(expected), (long long)(actual), __FILE__,           \
               __LINE__);                                                      \
      return 1;                                                                \
    }                                                                          \
  } while (0)

/* Convenience main macro */
#define CBELT_MAIN                                                             \
  int main(void) { return cbelt_run_all_tests(); }

/*---------------------------------------------------------------------------
 * Implementation (define CBELT_IMPLEMENTATION in exactly one .c file)
 *--------------------------------------------------------------------------*/
#ifdef CBELT_IMPLEMENTATION

/* Internal globals */
cbelt_group_t *cbelt_groups = NULL;
cbelt_group_t *cbelt_groups_tail = NULL;
setup_func_t cbelt_global_setup = NULL;
teardown_func_t cbelt_global_teardown = NULL;
const char *cbelt_current_group_name = NULL;
char cbelt_error_buf[512];

/* Internal helpers */
static cbelt_group_t *cbelt_find_or_create_group(const char *name) {
  cbelt_group_t *g = cbelt_groups;
  while (g) {
    if (strcmp(g->name, name) == 0)
      return g;
    g = g->next;
  }

  g = (cbelt_group_t *)calloc(1, sizeof(cbelt_group_t));
  g->name = strdup(name ? name : "(ungrouped)");
  g->next = NULL;
  if (cbelt_groups_tail) {
    cbelt_groups_tail->next = g;
  } else {
    cbelt_groups = g;
  }
  cbelt_groups_tail = g;
  return g;
}

static void cbelt_add_test(cbelt_group_t *group, const char *name,
                           test_func_t func) {
  cbelt_test_t *test = (cbelt_test_t *)calloc(1, sizeof(cbelt_test_t));
  test->name = strdup(name);
  test->func = func;
  test->next = group->tests;
  group->tests = test;
  group->test_count++;
}

static void cbelt_print_indented(const char *msg) {
  const char *p = msg;
  const char *nl;
  while ((nl = strchr(p, '\n')) != NULL) {
    fprintf(stderr, "    %.*s\n", (int)(nl - p), p);
    p = nl + 1;
  }
  if (*p)
    fprintf(stderr, "    %s\n", p);
}

/* Public API implementation */
void cbelt_register_test(const char *group_name, const char *test_name,
                         test_func_t func) {
  const char *name = group_name ? group_name : "(ungrouped)";
  cbelt_group_t *group = cbelt_find_or_create_group(name);
  cbelt_add_test(group, test_name, func);
}

void cbelt_set_group_setup(const char *group_name, setup_func_t func) {
  if (!group_name)
    return;
  cbelt_group_t *group = cbelt_find_or_create_group(group_name);
  group->group_setup = func;
}

void cbelt_set_group_teardown(const char *group_name, teardown_func_t func) {
  if (!group_name)
    return;
  cbelt_group_t *group = cbelt_find_or_create_group(group_name);
  group->group_teardown = func;
}

void cbelt_set_test_setup(const char *group_name, setup_func_t func) {
  if (!group_name)
    return;
  cbelt_group_t *group = cbelt_find_or_create_group(group_name);
  group->setup = func;
}

void cbelt_set_test_teardown(const char *group_name, teardown_func_t func) {
  if (!group_name)
    return;
  cbelt_group_t *group = cbelt_find_or_create_group(group_name);
  group->teardown = func;
}

void cbelt_set_global_setup(setup_func_t func) { cbelt_global_setup = func; }

void cbelt_set_global_teardown(teardown_func_t func) {
  cbelt_global_teardown = func;
}

int cbelt_run_all_tests(void) {
  int total_passed = 0;
  int total_failed = 0;
  cbelt_group_t *group = cbelt_groups;

  if (cbelt_global_setup)
    cbelt_global_setup();

  while (group) {
    printf("\n" CBELT_COLOR_BOLD CBELT_COLOR_CYAN "=== %s ===" CBELT_COLOR_RESET
           "\n",
           group->name);

    if (group->group_setup)
      group->group_setup();

    cbelt_test_t *test = group->tests;
    while (test) {
      cbelt_error_buf[0] = '\0';

      if (group->setup)
        group->setup();

      int result = test->func();

      if (group->teardown)
        group->teardown();

      if (result == 0) {
        printf("  " CBELT_COLOR_GREEN "%s" CBELT_COLOR_RESET "\n", test->name);
        total_passed++;
      } else {
        printf("  " CBELT_COLOR_RED "%s" CBELT_COLOR_RESET "\n", test->name);
        if (cbelt_error_buf[0])
          cbelt_print_indented(cbelt_error_buf);
        total_failed++;
      }

      test = test->next;
    }

    if (group->group_teardown)
      group->group_teardown();

    group = group->next;
  }

  if (cbelt_global_teardown)
    cbelt_global_teardown();

  printf("\n" CBELT_COLOR_BOLD
         "================================\n" CBELT_COLOR_RESET);
  printf(CBELT_COLOR_GREEN "%d passed" CBELT_COLOR_RESET ", " CBELT_COLOR_RED
                           "%d failed" CBELT_COLOR_RESET "\n",
         total_passed, total_failed);
  return total_failed > 0 ? 1 : 0;
}

#endif /* CBELT_IMPLEMENTATION */

#endif /* CBELT_H */
