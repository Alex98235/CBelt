#ifndef CBELT_H
#define CBELT_H

/* Platform detection and includes */
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE // For strsignal on (very) old glibc
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define CBELT_STRDUP strdup

#elif defined(_WIN32)
#include <io.h> /* _dup, _dup2, _close, _fileno */
#include <windows.h>

/* MSVC uses underscore prefix for these functions */
#ifdef _MSC_VER
#define CBELT_STRDUP _strdup
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#else
#define CBELT_STRDUP strdup
#endif

#else
#error "Unsupported platform, only Linux and Windows/MinGW are supported"
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
 * Terminal control macros
 *--------------------------------------------------------------------------*/
#define CBELT_CLEAR_LINE "\r\x1b[2K"
#define CBELT_SAVE_CURSOR "\x1b[s"
#define CBELT_RESTORE_CURSOR "\x1b[u"

/* Use the platform-appropriate string duplicator */
#define cbelt_strdup CBELT_STRDUP

/* Safe snprintf wrapper.
 * MSVC's _snprintf_s requires an extra count parameter; _TRUNCATE tells it
 * to truncate rather than fail when the buffer is too small.
 */
#if defined(_WIN32)
#define cbelt_snprintf_s(buf, bufsz, ...)                                      \
   _snprintf_s(buf, bufsz, _TRUNCATE, __VA_ARGS__)
#else
#define cbelt_snprintf_s(buf, bufsz, ...) snprintf(buf, bufsz, __VA_ARGS__)
#endif

/*---------------------------------------------------------------------------
 * Diagnostic pragma helpers
 *--------------------------------------------------------------------------*/
#if defined(__GNUC__) || defined(__clang__)
#define CBELT_DIAG_PUSH _Pragma("GCC diagnostic push")
#define CBELT_DIAG_POP _Pragma("GCC diagnostic pop")
#define CBELT_IGNORE_PEDANTIC _Pragma("GCC diagnostic ignored \"-Wpedantic\"")
#else
#define CBELT_DIAG_PUSH
#define CBELT_DIAG_POP
#define CBELT_IGNORE_PEDANTIC
#endif

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
   int timeout_s; // 0 = use default, positive = seconds, negative = no timeout
   struct cbelt_test *next;
} cbelt_test_t;

typedef struct cbelt_group {
   const char *name;
   setup_func_t setup;
   teardown_func_t teardown;
   setup_func_t group_setup;
   teardown_func_t group_teardown;
   struct cbelt_test *tests;
   struct cbelt_test *tests_tail;
   struct cbelt_group *next;
   size_t test_count;
} cbelt_group_t;

/*---------------------------------------------------------------------------
 * Magic values
 *--------------------------------------------------------------------------*/
#define CBELT_SANDBOX_DEFAULT_TIMEOUT_S 30

/*---------------------------------------------------------------------------
 * Resource management helpers (GCC/Clang only)
 * Uses __attribute__((cleanup)). Zero runtime overhead, no setup/teardown
 * required.
 *--------------------------------------------------------------------------*/
#if defined(__GNUC__) || defined(__clang__)

static inline void cbelt_defer_free(void *p) { free(*(void **)p); }

/* Automatically frees a malloc'd pointer when it goes out of scope.
 * Usage:  cbelt_auto ptr = malloc(n);
 * The compiler inserts free(ptr) at every scope exit point (normal return,
 * assertion failure, goto).
 */
#define cbelt_auto __attribute__((cleanup(cbelt_defer_free))) void *

/* Deferred cleanup block, runs an arbitrary function+arg on scope exit.
 * Useful for non-malloc resources: close(fd), unlock(mutex), fclose(file).
 *
 * Usage:
 *     int fd = open("file.txt", O_RDONLY);
 *     cbelt_defer(close, (void *)(intptr_t)fd);
 *
 * The callback is invoked exactly once when the enclosing block exits,
 * whether by normal return, assertion failure, or goto.
 */
typedef struct {
   void (*fn)(void *);
   void *arg;
} cbelt_defer_block_t;

static inline void cbelt_defer_run(cbelt_defer_block_t *b) {
   if (b->fn)
      b->fn(b->arg);
}

#define cbelt_defer(fn, arg)                                                   \
   __attribute__((cleanup(cbelt_defer_run))) cbelt_defer_block_t CBELT_UNIQUE( \
       _cbelt_def_) = {(fn), (arg)}

#endif /* __GNUC__ || __clang__ */

/*---------------------------------------------------------------------------
 * Public API declarations
 *--------------------------------------------------------------------------*/

// Windows does not have vasprintf & asprintf
int cbelt_vasprintf(char **strp, const char *fmt, va_list ap);
int cbelt_asprintf(char **strp, const char *fmt, ...);

// Registration
void cbelt_register_test(const char *group_name, const char *test_name,
                         test_func_t func);
void cbelt_register_test_with_timeout(const char *group_name,
                                      const char *test_name, test_func_t func,
                                      int timeout);

// Setup/Teardown
void cbelt_set_group_setup(const char *group_name, setup_func_t func);
void cbelt_set_group_teardown(const char *group_name, teardown_func_t func);
void cbelt_set_test_setup(const char *group_name, setup_func_t func);
void cbelt_set_test_teardown(const char *group_name, teardown_func_t func);
void cbelt_set_global_setup(setup_func_t func);
void cbelt_set_global_teardown(teardown_func_t func);

// Configuration
void cbelt_set_default_timeout(int seconds);

// Run
void cbelt_parse_args(int argc, char **argv);
int cbelt_run_tests(void);
int cbelt_run_all_tests(void);
int cbelt_run_all_tests_with_args(int argc, char **argv);

// Output redirect (capture stdout for self-tests)
void cbelt_set_output(char *buf, size_t size);
void cbelt_set_output_file(FILE *f);
void cbelt_reset_output(void);

/* Each non-implementation translation unit gets its own file-scope
   cbelt_current_group_name so that constructors in one .c file never
   interfere with constructors in another. The implementation TU uses
   a genuine global (defined below). */
#ifndef CBELT_IMPLEMENTATION
static const char *cbelt_current_group_name = NULL;
extern _Thread_local char cbelt_error_buf[512];
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
   CBELT_DIAG_PUSH CBELT_IGNORE_PEDANTIC static void                           \
       __attribute__((constructor(200 + __COUNTER__)))                         \
       CBELT_UNIQUE(_cbelt_set_group_)(void) {                                 \
      CBELT_DIAG_POP                                                           \
      cbelt_current_group_name = name;                                         \
   }

#define CBELT_TEST(name)                                                       \
   static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void);                   \
   CBELT_DIAG_PUSH CBELT_IGNORE_PEDANTIC static void                           \
       __attribute__((constructor(200 + __COUNTER__)))                         \
       CBELT_UNIQUE(_cbelt_register_##name)(void) {                            \
      CBELT_DIAG_POP                                                           \
      cbelt_register_test(cbelt_current_group_name, #name,                     \
                          CBELT_UNIQUE(_cbelt_test_##name));                   \
   }                                                                           \
   static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void)

#define CBELT_TEST_TIMEOUT(name, timeout_s)                                    \
   static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void);                   \
   CBELT_DIAG_PUSH CBELT_IGNORE_PEDANTIC static void                           \
       __attribute__((constructor(200 + __COUNTER__)))                         \
       CBELT_UNIQUE(_cbelt_register_##name)(void) {                            \
      CBELT_DIAG_POP                                                           \
      cbelt_register_test_with_timeout(cbelt_current_group_name, #name,        \
                                       CBELT_UNIQUE(_cbelt_test_##name),       \
                                       timeout_s);                             \
   }                                                                           \
   static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void)

#define CBELT_TEST_NO_TIMEOUT(name) CBELT_TEST_TIMEOUT(name, -1)

#define CBELT_GROUP_SETUP()                                                    \
   static void CBELT_UNIQUE(_cbelt_group_setup)(void);                         \
   CBELT_DIAG_PUSH CBELT_IGNORE_PEDANTIC static void                           \
       __attribute__((constructor(200 + __COUNTER__)))                         \
       CBELT_UNIQUE(_cbelt_register_group_setup)(void) {                       \
      CBELT_DIAG_POP                                                           \
      cbelt_set_group_setup(cbelt_current_group_name,                          \
                            CBELT_UNIQUE(_cbelt_group_setup));                 \
   }                                                                           \
   static void CBELT_UNIQUE(_cbelt_group_setup)(void)

#define CBELT_GROUP_TEARDOWN()                                                 \
   static void CBELT_UNIQUE(_cbelt_group_teardown)(void);                      \
   CBELT_DIAG_PUSH CBELT_IGNORE_PEDANTIC static void                           \
       __attribute__((constructor(200 + __COUNTER__)))                         \
       CBELT_UNIQUE(_cbelt_register_group_teardown)(void) {                    \
      CBELT_DIAG_POP                                                           \
      cbelt_set_group_teardown(cbelt_current_group_name,                       \
                               CBELT_UNIQUE(_cbelt_group_teardown));           \
   }                                                                           \
   static void CBELT_UNIQUE(_cbelt_group_teardown)(void)

#define CBELT_SETUP()                                                          \
   static void CBELT_UNIQUE(_cbelt_setup)(void);                               \
   CBELT_DIAG_PUSH CBELT_IGNORE_PEDANTIC static void                           \
       __attribute__((constructor(200 + __COUNTER__)))                         \
       CBELT_UNIQUE(_cbelt_register_setup)(void) {                             \
      CBELT_DIAG_POP                                                           \
      cbelt_set_test_setup(cbelt_current_group_name,                           \
                           CBELT_UNIQUE(_cbelt_setup));                        \
   }                                                                           \
   static void CBELT_UNIQUE(_cbelt_setup)(void)

#define CBELT_TEARDOWN()                                                       \
   static void CBELT_UNIQUE(_cbelt_teardown)(void);                            \
   CBELT_DIAG_PUSH CBELT_IGNORE_PEDANTIC static void                           \
       __attribute__((constructor(200 + __COUNTER__)))                         \
       CBELT_UNIQUE(_cbelt_register_teardown)(void) {                          \
      CBELT_DIAG_POP                                                           \
      cbelt_set_test_teardown(cbelt_current_group_name,                        \
                              CBELT_UNIQUE(_cbelt_teardown));                  \
   }                                                                           \
   static void CBELT_UNIQUE(_cbelt_teardown)(void)

#define CBELT_GLOBAL_SETUP()                                                   \
   static void CBELT_UNIQUE(_cbelt_global_setup)(void);                        \
   static void __attribute__((constructor)) CBELT_UNIQUE(                      \
       _cbelt_register_global_setup)(void) {                                   \
      cbelt_set_global_setup(CBELT_UNIQUE(_cbelt_global_setup));               \
   }                                                                           \
   static void CBELT_UNIQUE(_cbelt_global_setup)(void)

#define CBELT_GLOBAL_TEARDOWN()                                                \
   static void CBELT_UNIQUE(_cbelt_global_teardown)(void);                     \
   static void __attribute__((constructor)) CBELT_UNIQUE(                      \
       _cbelt_register_global_teardown)(void) {                                \
      cbelt_set_global_teardown(CBELT_UNIQUE(_cbelt_global_teardown));         \
   }                                                                           \
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
 * Assertion helper macros
 *--------------------------------------------------------------------------*/
#define GENERIC_EQ(a, b) _Generic((a), default: ((a) == (b)))

#define CS_ANY(x)                                                              \
   _Generic((x),                                                               \
       char: "%c",                                                             \
       signed char: "%hhd",                                                    \
       unsigned char: "%hhu",                                                  \
       short: "%hd",                                                           \
       unsigned short: "%hu",                                                  \
       int: "%d",                                                              \
       unsigned int: "%u",                                                     \
       long: "%ld",                                                            \
       unsigned long: "%lu",                                                   \
       long long: "%lld",                                                      \
       unsigned long long: "%llu",                                             \
       float: "%g",                                                            \
       double: "%g",                                                           \
       long double: "%Lg",                                                     \
       _Bool: "%d",                                                            \
       char *: "\"%s\"",                                                       \
       const char *: "\"%s\"",                                                 \
       default: "%p")

#define TYPENAME(x)                                                            \
   _Generic((x),                                                               \
       char: "char",                                                           \
       signed char: "signed char",                                             \
       unsigned char: "unsigned char",                                         \
       short: "short",                                                         \
       unsigned short: "unsigned short",                                       \
       int: "int",                                                             \
       unsigned int: "unsigned int",                                           \
       long: "long",                                                           \
       unsigned long: "unsigned long",                                         \
       long long: "long long",                                                 \
       unsigned long long: "unsigned long long",                               \
       float: "float",                                                         \
       double: "double",                                                       \
       long double: "long double",                                             \
       _Bool: "_Bool",                                                         \
       char *: "char *",                                                       \
       const char *: "const char",                                             \
       void *: "void *",                                                       \
       default: "unhandled type")

#define TYPE_TAG(x)                                                            \
   _Generic((x),                                                               \
       char: 1,                                                                \
       signed char: 2,                                                         \
       unsigned char: 3,                                                       \
       short: 4,                                                               \
       unsigned short: 5,                                                      \
       int: 6,                                                                 \
       unsigned int: 7,                                                        \
       long: 8,                                                                \
       unsigned long: 9,                                                       \
       long long: 10,                                                          \
       unsigned long long: 11,                                                 \
       float: 12,                                                              \
       double: 13,                                                             \
       long double: 14,                                                        \
       _Bool: 15,                                                              \
       char *: 16,                                                             \
       const char *: 17,                                                       \
       void *: 18,                                                             \
       default: 0)

#define CBELT_TYPE_MATCH(a, b) (TYPE_TAG(a) == TYPE_TAG(b))

/*---------------------------------------------------------------------------
 * Assertions
 *--------------------------------------------------------------------------*/
#define cbelt_assert(expr)                                                     \
   do {                                                                        \
      if (!(expr)) {                                                           \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Assertion failed: %s at %s:%d", #expr, __FILE__,    \
                          __LINE__);                                           \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#define cbelt_assert_true(expr) cbelt_assert(expr)
#define cbelt_assert_false(expr) cbelt_assert(!(expr))

#ifdef __cplusplus

/* C++ has no C11 _Generic, so cbelt_assert_equal / cbelt_assert_not_equal are
 * reimplemented with templates. Values are evaluated once (auto&& capture),
 * types are matched at compile time (std::is_same on decayed types), and
 * values are formatted with std::ostringstream — operator<< covers the same
 * categories CS_ANY handled (arithmetic, pointers, const char*, enums via
 * integral promotion).
 *
 * Semantics match the C branch: a type mismatch is a failure; otherwise
 * values are compared with == and both are printed on failure. */
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace cbelt_cpp {

template <typename T, typename U> inline bool types_match() {
   return std::is_same<typename std::decay<T>::type,
                       typename std::decay<U>::type>::value;
}

template <typename T> inline std::string format_value(const T &value) {
   std::ostringstream os;
   os << value;
   return os.str();
}

template <typename T> inline const char *type_name() {
   return typeid(T).name();
}

} // namespace cbelt_cpp

#define cbelt_assert_equal(expected, actual)                                   \
   do {                                                                        \
      auto &&_cbelt_exp_ = (expected);                                         \
      auto &&_cbelt_act_ = (actual);                                           \
      if (!cbelt_cpp::types_match<decltype(_cbelt_exp_),                       \
                                  decltype(_cbelt_act_)>()) {                  \
         cbelt_snprintf_s(                                                     \
             cbelt_error_buf, sizeof(cbelt_error_buf),                         \
             "Type mismatch at %s:%d\n%s (%s) vs %s (%s)", __FILE__, __LINE__, \
             #expected, cbelt_cpp::type_name<decltype(_cbelt_exp_)>(),         \
             #actual, cbelt_cpp::type_name<decltype(_cbelt_act_)>());          \
         return TEST_FAILURE;                                                  \
      }                                                                        \
      if (!(_cbelt_exp_ == _cbelt_act_)) {                                     \
         std::string _cbelt_es = cbelt_cpp::format_value(_cbelt_exp_);         \
         std::string _cbelt_as = cbelt_cpp::format_value(_cbelt_act_);         \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Equality assertion failed at %s:%d\n"               \
                          "  Expected: %s\n"                                   \
                          "  Actual:   %s",                                    \
                          __FILE__, __LINE__, _cbelt_es.c_str(),               \
                          _cbelt_as.c_str());                                  \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#define cbelt_assert_not_equal(expected, actual)                               \
   do {                                                                        \
      auto &&_cbelt_exp_ = (expected);                                         \
      auto &&_cbelt_act_ = (actual);                                           \
      if (cbelt_cpp::types_match<decltype(_cbelt_exp_),                        \
                                 decltype(_cbelt_act_)>() &&                   \
          (_cbelt_exp_ == _cbelt_act_)) {                                      \
         std::string _cbelt_es = cbelt_cpp::format_value(_cbelt_exp_);         \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Inequality assertion failed at %s:%d\n"             \
                          "  Unexpected value: %s",                            \
                          __FILE__, __LINE__, _cbelt_es.c_str());              \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#else /* !__cplusplus */

/*
 * WARNING: cbelt_assert_equal evaluates 'expected' and 'actual' multiple
 * times. Do not pass expressions with side effects (e.g.,
 * func_with_side_effects()).
 */
#define cbelt_assert_equal(expected, actual)                                   \
   do {                                                                        \
      char *_cbelt_exp = NULL;                                                 \
      char *_cbelt_act = NULL;                                                 \
      if (!CBELT_TYPE_MATCH(expected, actual)) {                               \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Type mismatch at %s:%d\n%s (%s) vs %s (%s)",        \
                          __FILE__, __LINE__, #expected, TYPENAME(expected),   \
                          #actual, TYPENAME(actual));                          \
         return TEST_FAILURE;                                                  \
      }                                                                        \
      if (!GENERIC_EQ(expected, actual)) {                                     \
         if (cbelt_asprintf(&_cbelt_exp, CS_ANY(expected), expected) == -1 ||  \
             cbelt_asprintf(&_cbelt_act, CS_ANY(actual), actual) == -1) {      \
            cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),         \
                             "Internal error: failed to format "               \
                             "values");                                        \
            free(_cbelt_exp);                                                  \
            free(_cbelt_act);                                                  \
            return TEST_FAILURE;                                               \
         }                                                                     \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Equality assertion failed at %s:%d\n"               \
                          "  Expected: %s\n"                                   \
                          "  Actual:   %s",                                    \
                          __FILE__, __LINE__, _cbelt_exp, _cbelt_act);         \
      }                                                                        \
      free(_cbelt_exp);                                                        \
      free(_cbelt_act);                                                        \
   }                                                                           \
                                                                               \
   while (0)

#define cbelt_assert_not_equal(expected, actual)                               \
   do {                                                                        \
      char *_cbelt_exp = NULL;                                                 \
      char *_cbelt_act = NULL;                                                 \
      if (CBELT_TYPE_MATCH(expected, actual) &&                                \
          GENERIC_EQ(expected, actual)) {                                      \
         if (cbelt_asprintf(&_cbelt_exp, CS_ANY(expected), expected) == -1 ||  \
             cbelt_asprintf(&_cbelt_act, CS_ANY(actual), actual) == -1) {      \
            cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),         \
                             "Internal error: failed to format values");       \
         }                                                                     \
         free(_cbelt_exp);                                                     \
         free(_cbelt_act);                                                     \
         return TEST_FAILURE;                                                  \
      }                                                                        \
      cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),               \
                       "Inequality assertion failed at %s:%d\n"                \
                       "  Unexpected value: %s",                               \
                       __FILE__, __LINE__, _cbelt_exp);                        \
      free(_cbelt_exp);                                                        \
      free(_cbelt_act);                                                        \
   } while (0)

#endif /* __cplusplus */

#define cbelt_assert_str_not_equal(str1, str2)                                 \
   do {                                                                        \
      if (str1 == NULL && str2 == NULL) {                                      \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "String inequality failed: both NULL at %s:%d",      \
                          __FILE__, __LINE__);                                 \
         return TEST_FAILURE;                                                  \
      }                                                                        \
      if (str1 == NULL || str2 == NULL) {                                      \
         break; /* One NULL, one not - definitely not equal */                 \
      }                                                                        \
      if (strcmp(str1, str2) == 0) {                                           \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "String inequality failed: %s == %s at %s:%d",       \
                          #str1, #str2, __FILE__, __LINE__);                   \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#define cbelt_assert_str_equal(str1, str2)                                     \
   do {                                                                        \
      if (str1 == NULL && str2 == NULL) {                                      \
         break; /* Both NULL - equal */                                        \
      }                                                                        \
      if (str1 == NULL || str2 == NULL) {                                      \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "String equality failed: NULL vs non-NULL at %s:%d", \
                          __FILE__, __LINE__);                                 \
         return TEST_FAILURE;                                                  \
      }                                                                        \
      if (strcmp(str1, str2) != 0) {                                           \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "String equality failed: %s != %s at %s:%d\n"        \
                          "  Expected: \"%s\"\n"                               \
                          "  Actual:   \"%s\"",                                \
                          #str1, #str2, __FILE__, __LINE__, str1, str2);       \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#define cbelt_assert_null(ptr)                                                 \
   do {                                                                        \
      if ((ptr) != NULL) {                                                     \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Expected NULL at %s:%d, got %p", __FILE__,          \
                          __LINE__, (ptr));                                    \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#define cbelt_assert_not_null(ptr)                                             \
   do {                                                                        \
      if ((ptr) == NULL) {                                                     \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Expected non-NULL at %s:%d", __FILE__, __LINE__);   \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#define cbelt_assert_mem_equal(ptr1, ptr2, size)                               \
   do {                                                                        \
      if ((ptr1) == NULL && (ptr2) == NULL) {                                  \
         break; /* Success - both NULL */                                      \
      }                                                                        \
      if ((ptr1) == NULL || (ptr2) == NULL) {                                  \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Memory comparison failed: one pointer is NULL at "  \
                          "%s:%d",                                             \
                          __FILE__, __LINE__);                                 \
         return TEST_FAILURE;                                                  \
      }                                                                        \
      if (memcmp((ptr1), (ptr2), (size)) != 0) {                               \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Memory mismatch at %s:%d (size: %zu bytes)",        \
                          __FILE__, __LINE__, (size_t)(size));                 \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

#define cbelt_assert_in_range(value, min, max)                                 \
   do {                                                                        \
      if ((value) < (min) || (value) > (max)) {                                \
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),            \
                          "Value %s not in range [%s, %s] at %s:%d", #value,   \
                          #min, #max, __FILE__, __LINE__);                     \
         return TEST_FAILURE;                                                  \
      }                                                                        \
   } while (0)

/* Convenience main macro - takes argc, argv for CLI options */
#define CBELT_MAIN                                                             \
   int main(int argc, char **argv) {                                           \
      return cbelt_run_all_tests_with_args(argc, argv);                        \
   }

/*---------------------------------------------------------------------------
 * Implementation (define CBELT_IMPLEMENTATION in exactly one .c file)
 *--------------------------------------------------------------------------*/
#ifdef CBELT_IMPLEMENTATION

/*---------------------------------------------------------------------------
 * Thread abstraction layer
 *--------------------------------------------------------------------------*/
#if defined(__linux__)
#include <pthread.h>
typedef pthread_t cbelt_thread_t;
typedef pthread_mutex_t cbelt_mutex_t;
#define cbelt_mutex_init(m) pthread_mutex_init(m, NULL)
#define cbelt_mutex_destroy(m) pthread_mutex_destroy(m)
#define cbelt_mutex_lock(m) pthread_mutex_lock(m)
#define cbelt_mutex_unlock(m) pthread_mutex_unlock(m)
#define cbelt_thread_create(t, f, a) pthread_create(t, NULL, f, a)
#define cbelt_thread_join(t) pthread_join(t, NULL)
#elif defined(_WIN32)
typedef HANDLE cbelt_thread_t;
typedef CRITICAL_SECTION cbelt_mutex_t;
#define cbelt_mutex_init(m) InitializeCriticalSection(m)
#define cbelt_mutex_destroy(m) DeleteCriticalSection(m)
#define cbelt_mutex_lock(m) EnterCriticalSection(m)
#define cbelt_mutex_unlock(m) LeaveCriticalSection(m)
#define cbelt_thread_create(t, f, a)                                           \
   ((*(t) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(f), (a), 0,         \
                         NULL)) != NULL                                        \
        ? 0                                                                    \
        : -1)
#define cbelt_thread_join(t)                                                   \
   (WaitForSingleObject((t), INFINITE), CloseHandle((t)))
#else
#define CBELT_NO_THREADING
#endif

/*---------------------------------------------------------------------------
 * Runtime configuration struct
 *--------------------------------------------------------------------------*/
typedef struct cbelt_config {
   int quiet;   /* 1 = only show failures and summary */
   int spinner; /* 0 = no spinner, 1 = spinner */
   int color;   /* 0 = never, 1 = auto (isatty), 2 = always */
   int jobs;    /* number of parallel threads (1 = serial) */
   const char
       *filter; /* NULL or prefix/glob pattern matching "group.test_name" */
} cbelt_config_t;

/*---------------------------------------------------------------------------
 * Platform-specific includes for output redirect
 *--------------------------------------------------------------------------*/
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>

#endif

/*---------------------------------------------------------------------------
 * Internal config and stdout redirect state
 *--------------------------------------------------------------------------*/
static struct cbelt_config cbelt_internal_config = {0, 1, 1, 1, NULL};
static int cbelt_saved_stdout_fd = -1;
static int cbelt_capture_pipe_read = -1;
static int cbelt_capture_pipe_write = -1;
static char *cbelt_capture_buf = NULL;
static size_t cbelt_capture_buf_size = 0;
static int cbelt_redirect_active = 0;

/*---------------------------------------------------------------------------
 * Internal globals
 *--------------------------------------------------------------------------*/
cbelt_group_t *cbelt_groups = NULL;
cbelt_group_t *cbelt_groups_tail = NULL;
setup_func_t cbelt_global_setup = NULL;
teardown_func_t cbelt_global_teardown = NULL;
const char *cbelt_current_group_name = NULL;
_Thread_local char cbelt_error_buf[512];
static int cbelt_global_default_timeout_s = CBELT_SANDBOX_DEFAULT_TIMEOUT_S;
#ifndef _WIN32
static pid_t spinner_pid = 0;
static int spinner_pipe[2];
#endif

/*---------------------------------------------------------------------------
 * vasprintf / asprintf helpers
 *--------------------------------------------------------------------------*/
int cbelt_vasprintf(char **strp, const char *fmt, va_list ap) {
   va_list ap_copy;
   va_copy(ap_copy, ap);
   int len = vsnprintf(NULL, 0, fmt, ap_copy);
   va_end(ap_copy);

   if (len < 0)
      return -1;

   char *buf = (char *)malloc(len + 1);
   if (!buf)
      return -1;

   int result = vsnprintf(buf, len + 1, fmt, ap);
   if (result < 0) {
      free(buf);
      return -1;
   }

   *strp = buf;
   return result;
}

int cbelt_asprintf(char **strp, const char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   int result = cbelt_vasprintf(strp, fmt, ap);
   va_end(ap);
   return result;
}

/*---------------------------------------------------------------------------
 * Group / test helpers
 *--------------------------------------------------------------------------*/
static cbelt_group_t *cbelt_find_or_create_group(const char *name) {
   cbelt_group_t *g = cbelt_groups;
   while (g) {
      if (strcmp(g->name, name) == 0)
         return g;
      g = g->next;
   }

   g = (cbelt_group_t *)calloc(1, sizeof(cbelt_group_t));
   g->name = cbelt_strdup(name ? name : "(ungrouped)");
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
                           test_func_t func, int timeout_s) {
   cbelt_test_t *test = (cbelt_test_t *)calloc(1, sizeof(cbelt_test_t));
   test->name = cbelt_strdup(name);
   test->func = func;
   test->timeout_s = timeout_s;
   test->next = NULL;
   if (group->tests_tail) {
      group->tests_tail->next = test;
   } else {
      group->tests = test;
   }
   group->tests_tail = test;
   group->test_count++;
}

static void cbelt_print_indented(const char *msg) {
   const char *p = msg;
   const char *nl;
   while ((nl = strchr(p, '\n')) != NULL) {
      printf("    %.*s\n", (int)(nl - p), p);
      p = nl + 1;
   }
   if (*p)
      printf("    %s\n", p);
}

/*---------------------------------------------------------------------------
 * Color helpers (respect runtime config)
 *--------------------------------------------------------------------------*/
static int cbelt_color_enabled(void) {
   if (cbelt_internal_config.color == 2)
      return 1;
   if (cbelt_internal_config.color == 0)
      return 0;
   /* auto: check if stdout is a terminal */
#ifdef _WIN32
   {
      HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
      if (h != INVALID_HANDLE_VALUE) {
         DWORD mode = 0;
         return GetConsoleMode(h, &mode) != 0;
      }
      return 0;
   }
#else
   return isatty(fileno(stdout));
#endif
}

static const char *cbelt_color(const char *ansi) {
   return cbelt_color_enabled() ? ansi : "";
}

/*---------------------------------------------------------------------------
 * Filter matching
 *--------------------------------------------------------------------------*/
static int cbelt_test_matches(const char *filter, const char *group_name,
                              const char *test_name) {
   if (!filter || filter[0] == '\0')
      return 1;

   /* Try matching "group.test_name" as prefix */
   size_t flen = strlen(filter);
   char full_name[1024];
   cbelt_snprintf_s(full_name, sizeof(full_name), "%s.%s", group_name,
                    test_name);

   /* Simple prefix match */
   if (strncmp(full_name, filter, flen) == 0)
      return 1;

   /* Also try matching group name alone */
   if (strncmp(group_name, filter, flen) == 0)
      return 1;

   /* Also try matching test name alone */
   if (strncmp(test_name, filter, flen) == 0)
      return 1;

   return 0;
}

/*---------------------------------------------------------------------------
 * Public API: registration
 *--------------------------------------------------------------------------*/
void cbelt_register_test(const char *group_name, const char *test_name,
                         test_func_t func) {
   const char *name = group_name ? group_name : "(ungrouped)";
   cbelt_group_t *group = cbelt_find_or_create_group(name);
   cbelt_add_test(group, test_name, func, 0);
}

void cbelt_register_test_with_timeout(const char *group_name,
                                      const char *test_name, test_func_t func,
                                      int timeout_s) {
   const char *name = group_name ? group_name : "(ungrouped)";
   cbelt_group_t *group = cbelt_find_or_create_group(name);
   cbelt_add_test(group, test_name, func, timeout_s);
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

void cbelt_set_default_timeout(int seconds) {
   if (seconds > 0) {
      cbelt_global_default_timeout_s = seconds;
   } else if (seconds < 0) {
      cbelt_global_default_timeout_s = -1;
   }
}

/*---------------------------------------------------------------------------
 * Sandboxed test execution
 *--------------------------------------------------------------------------*/
#ifdef _WIN32
typedef struct {
   test_func_t func;
   char error_buf[512];
   int result;
} cbelt_sandbox_work_t;

static DWORD WINAPI cbelt_sandbox_thread(LPVOID arg) {
   cbelt_sandbox_work_t *work = (cbelt_sandbox_work_t *)arg;
   work->result = work->func();
   strcpy(work->error_buf, cbelt_error_buf);
   return 0;
}
#endif

static TestResult cbelt_run_test_in_sandbox(test_func_t func, int timeout_s) {
#ifdef __linux__
   int pipefd[2];
   if (pipe(pipefd) == -1) {
      perror("pipe");
      return func();
   }

   pid_t pid = fork();
   if (pid < 0) {
      fprintf(stderr,
              CBELT_COLOR_YELLOW
              "Warning: fork() failed (%s) - running test in-process without "
              "isolation\n" CBELT_COLOR_RESET,
              strerror(errno));
      close(pipefd[0]);
      close(pipefd[1]);
      return func();
   }

   if (pid == 0) {
      close(pipefd[0]);

      if (timeout_s > 0) {
         alarm(timeout_s);
      }

      int result = func();

      if (cbelt_error_buf[0] != '\0') {
         write(pipefd[1], cbelt_error_buf, strlen(cbelt_error_buf) + 1);
      }

      close(pipefd[1]);
      exit(result);
   }

   close(pipefd[1]);

   ssize_t bytes_read =
       read(pipefd[0], cbelt_error_buf, sizeof(cbelt_error_buf) - 1);
   if (bytes_read > 0) {
      cbelt_error_buf[bytes_read] = '\0';
   } else {
      cbelt_error_buf[0] = '\0';
   }
   close(pipefd[0]);

   int status;
   pid_t wait_result = waitpid(pid, &status, 0);

   if (wait_result == -1) {
      perror("waitpid");
      return TEST_FAILURE;
   }

   if (WIFEXITED(status)) {
      return WEXITSTATUS(status) == 0 ? TEST_SUCCESS : TEST_FAILURE;
   } else if (WIFSIGNALED(status)) {
      int sig = WTERMSIG(status);
      if (sig == SIGALRM) {
         cbelt_snprintf_s(
             cbelt_error_buf, sizeof(cbelt_error_buf),
             "Test timed out after %d seconds (infinite loop or hang)",
             timeout_s);
      } else {
         cbelt_snprintf_s(cbelt_error_buf, sizeof(cbelt_error_buf),
                          "Test crashed with signal: %d (%s)", sig,
                          strsignal(sig));
      }

      return TEST_CRASH;
   }

   return TEST_FAILURE;
#elif defined(_WIN32)
   cbelt_sandbox_work_t work;
   work.func = func;
   work.error_buf[0] = '\0';
   work.result = TEST_FAILURE;

   HANDLE thread = CreateThread(NULL, 0, cbelt_sandbox_thread, &work, 0, NULL);
   if (!thread) {
      /* Fallback: run in-process */
      return func();
   }

   DWORD timeout_ms = timeout_s > 0 ? (DWORD)timeout_s * 1000 : INFINITE;
   DWORD wait_result = WaitForSingleObject(thread, timeout_ms);

   if (wait_result == WAIT_TIMEOUT) {
      TerminateThread(thread, 1);
      CloseHandle(thread);
      cbelt_snprintf_s(
          cbelt_error_buf, sizeof(cbelt_error_buf),
          "Test timed out after %d seconds (infinite loop or hang)", timeout_s);
      return TEST_CRASH;
   }

   CloseHandle(thread);

   /* Copy error buffer from worker thread back to our thread-local buffer */
   strcpy(cbelt_error_buf, work.error_buf);

   return work.result == 0 ? TEST_SUCCESS : TEST_FAILURE;
#else
   (void)timeout_s;
   return func();
#endif
}

/*---------------------------------------------------------------------------
 * Spinner (Linux fork-based, respects runtime config)
 *--------------------------------------------------------------------------*/
#ifdef __linux__
static void cbelt_spinner_process(const char *test_name) {
   signal(SIGINT, SIG_IGN);

   const char *spinner_frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼",
                                   "⠴", "⠦", "⠧", "⠇", "⠏"};
   int num_frames = sizeof(spinner_frames) / sizeof(spinner_frames[0]);

   srand(time(NULL));
   int frame = rand();

   char c;

   close(spinner_pipe[1]);

   int flags = fcntl(spinner_pipe[0], F_GETFL, 0);
   fcntl(spinner_pipe[0], F_SETFL, flags | O_NONBLOCK);

   while (1) {
      if (read(spinner_pipe[0], &c, 1) > 0) {
         break;
      }

      printf("\r%s %s...", spinner_frames[frame % num_frames], test_name);
      fflush(stdout);
      frame++;
      usleep(80000);
   }

   printf("\r\x1b[2K\r");
   fflush(stdout);

   close(spinner_pipe[0]);
   exit(0);
}

static void cbelt_spinner_stop(void) {
   if (spinner_pid != 0) {
      char stop = 1;
      ssize_t written = write(spinner_pipe[1], &stop, 1);
      if (written != 1)
         kill(spinner_pid, SIGTERM);

      int status;
      waitpid(spinner_pid, &status, 0);

      close(spinner_pipe[1]);
      spinner_pid = 0;

      printf("%s", CBELT_CLEAR_LINE);
      fflush(stdout);
   }
}
#endif

static void cbelt_spinner_start(const char *test_name) {
   if (!cbelt_internal_config.spinner || cbelt_internal_config.quiet) {
      (void)test_name;
      return;
   }
#ifdef __linux__
   if (spinner_pid != 0) {
      cbelt_spinner_stop();
   }

   if (pipe(spinner_pipe) == -1) {
      printf("  %s...", test_name);
      fflush(stdout);
      return;
   }

   spinner_pid = fork();
   if (spinner_pid == 0) {
      cbelt_spinner_process(test_name);
      exit(0);
   } else if (spinner_pid < 0) {
      close(spinner_pipe[0]);
      close(spinner_pipe[1]);
      printf("  %s...", test_name);
      fflush(stdout);
      return;
   }

   close(spinner_pipe[0]);
#else
   printf("  %s...", test_name);
   fflush(stdout);
#endif
}

static void cbelt_spinner_update_result(const char *test_name,
                                        TestResult result) {
   /* In quiet mode, passing tests produce no output at all */
   if (cbelt_internal_config.quiet && result == TEST_SUCCESS) {
      (void)test_name;
      return;
   }

   const char *green = cbelt_color(CBELT_COLOR_GREEN);
   const char *red = cbelt_color(CBELT_COLOR_RED);
   const char *reset = cbelt_color(CBELT_COLOR_RESET);

   /* Clear spinner line if spinner was active */
   if (cbelt_internal_config.spinner) {
#ifdef __linux__
      if (spinner_pid != 0) {
         cbelt_spinner_stop();
      }
#endif
      fflush(stdout);
      printf("%s", CBELT_CLEAR_LINE);
      fflush(stdout);
   }

   if (result == TEST_SUCCESS) {
      if (!cbelt_internal_config.quiet) {
         printf("  %s%s%s\n", green, test_name, reset);
      }
   } else {
      printf("  %s%s%s\n", red, test_name, reset);
      if (cbelt_error_buf[0]) {
         cbelt_print_indented(cbelt_error_buf);
      }
   }
   fflush(stdout);
}

/*---------------------------------------------------------------------------
 * Run a single group's tests
 *--------------------------------------------------------------------------*/
typedef struct {
   cbelt_group_t **groups;
   int group_count;
   int passed;
   int failed;
   int crashed;
   cbelt_mutex_t *print_mutex;
} cbelt_group_work_t;

static void cbelt_run_group(cbelt_group_work_t *work) {
   int total_passed = 0;
   int total_failed = 0;
   int total_crashed = 0;
   const cbelt_config_t *cfg = &cbelt_internal_config;
   const char *bold = cbelt_color(CBELT_COLOR_BOLD);
   const char *cyan = cbelt_color(CBELT_COLOR_CYAN);
   const char *reset = cbelt_color(CBELT_COLOR_RESET);

   int gi;
   for (gi = 0; gi < work->group_count; gi++) {
      cbelt_group_t *group = work->groups[gi];

      /* Skip group entirely if no tests match the filter */
      {
         cbelt_test_t *scan = group->tests;
         int has_matching = 0;
         while (scan) {
            if (cbelt_test_matches(cfg->filter, group->name, scan->name)) {
               has_matching = 1;
               break;
            }
            scan = scan->next;
         }
         if (!has_matching)
            continue;
      }

      /* In non-quiet mode, print header immediately */
      int header_printed = 0;
      if (!cfg->quiet) {
         cbelt_mutex_lock(work->print_mutex);
         printf("\n%s%s=== %s ===%s\n", bold, cyan, group->name, reset);
         cbelt_mutex_unlock(work->print_mutex);
         header_printed = 1;
      }

      if (group->group_setup)
         group->group_setup();

      cbelt_test_t *test = group->tests;
      while (test) {
         /* Apply filter */
         if (!cbelt_test_matches(cfg->filter, group->name, test->name)) {
            test = test->next;
            continue;
         }

         cbelt_error_buf[0] = '\0';

         /* Remember whether redirect was already active before this test */
         int redirect_was_active = cbelt_redirect_active;

         int timeout_s = cbelt_global_default_timeout_s;
         if (test->timeout_s != 0) {
            timeout_s = test->timeout_s;
         }

         cbelt_spinner_start(test->name);

         if (group->setup)
            group->setup();

         TestResult result = cbelt_run_test_in_sandbox(test->func, timeout_s);

         if (group->teardown)
            group->teardown();

         /* Auto-reset stdout only if this test started the redirect */
         if (!redirect_was_active) {
            cbelt_reset_output();
         }

         cbelt_mutex_lock(work->print_mutex);
         /* In quiet mode, defer header until first failure or crash */
         if (!header_printed && result != TEST_SUCCESS) {
            printf("\n%s%s=== %s ===%s\n", bold, cyan, group->name, reset);
            header_printed = 1;
         }
         cbelt_spinner_update_result(test->name, result);
         cbelt_mutex_unlock(work->print_mutex);

         if (result == TEST_SUCCESS)
            total_passed++;
         else if (result == TEST_CRASH)
            total_crashed++;
         else
            total_failed++;

         test = test->next;
      }

      if (group->group_teardown)
         group->group_teardown();
   }

   work->passed = total_passed;
   work->failed = total_failed;
   work->crashed = total_crashed;
}

/* Thread wrapper functions */
#if defined(__linux__)
static void *cbelt_run_group_thread(void *arg) {
   cbelt_run_group((cbelt_group_work_t *)arg);
   return NULL;
}
#elif defined(_WIN32)
static DWORD WINAPI cbelt_run_group_thread(LPVOID arg) {
   cbelt_run_group((cbelt_group_work_t *)arg);
   return 0;
}
#endif

/*---------------------------------------------------------------------------
 * CLI argument parsing
 *--------------------------------------------------------------------------*/
static void cbelt_print_usage(const char *prog_name) {
   printf("Usage: %s [OPTIONS]\n\n", prog_name);
   printf("Options:\n");
   printf("  -q, --quiet           Only show failing tests and summary\n");
   printf("  -f, --filter PATTERN  Only run tests matching PATTERN (prefix "
          "match on\n");
   printf("                        group.test_name, group name, or test "
          "name)\n");
   printf("  --no-spinner          Disable animated spinner\n");
   printf("  --color auto|always|never\n");
   printf("                        Control colored output (default: auto)\n");
   printf("  -j, --jobs N          Run tests in parallel with N threads "
          "(default: 1)\n");
   printf("  -h, --help            Show this help message\n");
}

void cbelt_parse_args(int argc, char **argv) {
   /* Set defaults */
   cbelt_internal_config.quiet = 0;
   cbelt_internal_config.spinner = 1;
   cbelt_internal_config.color = 1; /* auto */
   cbelt_internal_config.jobs = 1;
   cbelt_internal_config.filter = NULL;

   if (!argv)
      return;

   int i;
   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
         cbelt_internal_config.quiet = 1;
      } else if (strcmp(argv[i], "-f") == 0 ||
                 strcmp(argv[i], "--filter") == 0) {
         if (i + 1 < argc) {
            cbelt_internal_config.filter = argv[++i];
         } else {
            fprintf(stderr, "Error: --filter requires a PATTERN argument\n");
         }
      } else if (strcmp(argv[i], "--no-spinner") == 0) {
         cbelt_internal_config.spinner = 0;
      } else if (strncmp(argv[i], "--color=", 8) == 0) {
         const char *val = argv[i] + 8;
         if (strcmp(val, "auto") == 0)
            cbelt_internal_config.color = 1;
         else if (strcmp(val, "always") == 0)
            cbelt_internal_config.color = 2;
         else if (strcmp(val, "never") == 0)
            cbelt_internal_config.color = 0;
         else
            fprintf(stderr, "Error: --color expects auto|always|never\n");
      } else if (strcmp(argv[i], "--color") == 0) {
         if (i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "auto") == 0)
               cbelt_internal_config.color = 1;
            else if (strcmp(argv[i], "always") == 0)
               cbelt_internal_config.color = 2;
            else if (strcmp(argv[i], "never") == 0)
               cbelt_internal_config.color = 0;
            else
               fprintf(stderr, "Error: --color expects auto|always|never\n");
         } else {
            fprintf(stderr, "Error: --color requires auto|always|never\n");
         }
      } else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--jobs") == 0) {
         if (i + 1 < argc) {
            i++;
            int n = atoi(argv[i]);
            cbelt_internal_config.jobs = n > 0 ? n : 1;
         } else {
            fprintf(stderr, "Error: --jobs requires a number\n");
         }
      } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
         cbelt_print_usage(argv[0]);
         exit(0);
      } else {
         fprintf(stderr, "Unknown option: %s\n", argv[i]);
         fprintf(stderr, "Use -h or --help for usage.\n");
         exit(1);
      }
   }
}

/*---------------------------------------------------------------------------
 * Windows ANSI enablement (respects runtime config)
 *--------------------------------------------------------------------------*/
#ifdef _WIN32
static void cbelt_enable_ansi(void) {
   HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
   if (stdout_handle != INVALID_HANDLE_VALUE) {
      DWORD mode = 0;
      if (GetConsoleMode(stdout_handle, &mode)) {
         SetConsoleMode(stdout_handle,
                        mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
      }
   }
}
#endif

/*---------------------------------------------------------------------------
 * Main entry points
 *--------------------------------------------------------------------------*/
int cbelt_run_all_tests_with_args(int argc, char **argv) {
   cbelt_parse_args(argc, argv);

   /* Enable ANSI on Windows (needed for any color output at all) */
#ifdef _WIN32
   cbelt_enable_ansi();
#endif

   return cbelt_run_tests();
}

/* Run tests with the internal config */
int cbelt_run_tests(void) {
   int total_passed = 0;
   int total_failed = 0;
   int total_crashed = 0;

   if (cbelt_global_setup)
      cbelt_global_setup();

   if (cbelt_internal_config.jobs > 1) {
/* --- Parallel execution --- */
#if defined(CBELT_NO_THREADING)
      fprintf(stderr, "Warning: threading not supported on this platform, "
                      "falling back to serial execution\n");
      cbelt_internal_config.jobs = 1;
#else
      /* Count groups and collect them into an array */
      int group_count = 0;
      cbelt_group_t *g = cbelt_groups;
      while (g) {
         group_count++;
         g = g->next;
      }

      if (group_count == 0) {
         if (cbelt_global_teardown)
            cbelt_global_teardown();
         const char *gbold = cbelt_color(CBELT_COLOR_BOLD);
         const char *greset = cbelt_color(CBELT_COLOR_RESET);
         printf("\n%s================================%s\n", gbold, greset);
         printf("0 passed, 0 failed\n");
         return 0;
      }

      cbelt_group_t **group_array = (cbelt_group_t **)calloc(
          (size_t)group_count, sizeof(cbelt_group_t *));
      g = cbelt_groups;
      int gi = 0;
      while (g) {
         group_array[gi++] = g;
         g = g->next;
      }

      cbelt_mutex_t print_mutex;
      cbelt_mutex_init(&print_mutex);

      int num_threads = group_count < cbelt_internal_config.jobs
                            ? group_count
                            : cbelt_internal_config.jobs;
      cbelt_thread_t *threads =
          (cbelt_thread_t *)calloc((size_t)num_threads, sizeof(cbelt_thread_t));
      cbelt_group_work_t *works = (cbelt_group_work_t *)calloc(
          (size_t)num_threads, sizeof(cbelt_group_work_t));

      /* Chunk groups across threads */
      int base = group_count / num_threads;
      int rem = group_count % num_threads;
      int pos = 0;
      for (int ti = 0; ti < num_threads; ti++) {
         works[ti].group_count = base + (ti < rem ? 1 : 0);
         works[ti].groups = &group_array[pos];
         works[ti].print_mutex = &print_mutex;
         pos += works[ti].group_count;
      }

      /* Create threads */
      for (int ti = 0; ti < num_threads; ti++) {
         cbelt_thread_create(&threads[ti], cbelt_run_group_thread, &works[ti]);
      }

      /* Join all threads and accumulate results */
      for (int ti = 0; ti < num_threads; ti++) {
         cbelt_thread_join(threads[ti]);
         total_passed += works[ti].passed;
         total_failed += works[ti].failed;
         total_crashed += works[ti].crashed;
      }

      free(group_array);
      free(threads);
      free(works);
      cbelt_mutex_destroy(&print_mutex);
#endif
   } else {
      /* --- Serial execution --- */
      /* Build group array for the work struct */
      int group_count = 0;
      cbelt_group_t *gc = cbelt_groups;
      while (gc) {
         group_count++;
         gc = gc->next;
      }
      cbelt_group_t **group_array = (cbelt_group_t **)calloc(
          (size_t)group_count, sizeof(cbelt_group_t *));
      gc = cbelt_groups;
      for (int gi = 0; gi < group_count; gi++) {
         group_array[gi] = gc;
         gc = gc->next;
      }

      cbelt_group_work_t work;
      cbelt_mutex_t serial_mutex;
      cbelt_mutex_init(&serial_mutex);
      work.groups = group_array;
      work.group_count = group_count;
      work.print_mutex = &serial_mutex;
      cbelt_run_group(&work);
      total_passed += work.passed;
      total_failed += work.failed;
      total_crashed += work.crashed;
      free(group_array);
      cbelt_mutex_destroy(&serial_mutex);
   }

   if (cbelt_global_teardown)
      cbelt_global_teardown();

   const char *green = cbelt_color(CBELT_COLOR_GREEN);
   const char *red = cbelt_color(CBELT_COLOR_RED);
   const char *bold = cbelt_color(CBELT_COLOR_BOLD);
   const char *reset = cbelt_color(CBELT_COLOR_RESET);

   printf("\n%s================================%s\n", bold, reset);
   if (total_crashed > 0)
      printf("%s%d passed%s, %s%d failed%s, %s%d crashed%s\n",
             green, total_passed, reset, red, total_failed, reset,
             red, total_crashed, reset);
   else
      printf("%s%d passed%s, %s%d failed%s\n", green, total_passed, reset, red,
             total_failed, reset);

   return total_failed > 0 || total_crashed > 0 ? 1 : 0;
}

/* Backward-compatible wrapper */
int cbelt_run_all_tests(void) { return cbelt_run_all_tests_with_args(0, NULL); }

/*---------------------------------------------------------------------------
 * Output redirect implementation
 *--------------------------------------------------------------------------*/
void cbelt_set_output(char *buf, size_t size) {
   if (!buf || size == 0)
      return;

   /* Don't re-enter if already redirecting */
   if (cbelt_redirect_active)
      return;

#ifdef _WIN32
   int fds[2];
   if (_pipe(fds, size, _O_BINARY) != 0)
      return;
#else
   int fds[2];
   if (pipe(fds) != 0)
      return;
#endif

   cbelt_saved_stdout_fd = dup(fileno(stdout));
   if (cbelt_saved_stdout_fd == -1) {
      close(fds[0]);
      close(fds[1]);
      return;
   }

   cbelt_capture_pipe_read = fds[0];
   cbelt_capture_pipe_write = fds[1];

   fflush(stdout);
   dup2(cbelt_capture_pipe_write, fileno(stdout));
   close(cbelt_capture_pipe_write); /* only the dup2'd fd stays open */

   cbelt_capture_buf = buf;
   cbelt_capture_buf_size = size;
   cbelt_redirect_active = 1;
}

void cbelt_set_output_file(FILE *f) {
   if (!f)
      return;
   if (cbelt_redirect_active)
      return;

   cbelt_saved_stdout_fd = dup(fileno(stdout));
   if (cbelt_saved_stdout_fd == -1)
      return;

   fflush(stdout);
   dup2(fileno(f), fileno(stdout));

   cbelt_capture_buf = NULL;
   cbelt_capture_buf_size = 0;
   cbelt_redirect_active = 1;
}

void cbelt_reset_output(void) {
   if (!cbelt_redirect_active)
      return;

   fflush(stdout);

   /* Restore original stdout */
   if (cbelt_saved_stdout_fd != -1) {
      dup2(cbelt_saved_stdout_fd, fileno(stdout));
      close(cbelt_saved_stdout_fd);
      cbelt_saved_stdout_fd = -1;
   }

   /* Read captured output into buffer */
   if (cbelt_capture_buf && cbelt_capture_buf_size > 0) {
      size_t total = 0;
      int n;
      while (total < cbelt_capture_buf_size - 1 &&
             (n = (int)read(cbelt_capture_pipe_read, cbelt_capture_buf + total,
                            cbelt_capture_buf_size - 1 - total)) > 0) {
         total += (size_t)n;
      }
      cbelt_capture_buf[total] = '\0';
   }

   if (cbelt_capture_pipe_read != -1) {
      close(cbelt_capture_pipe_read);
      cbelt_capture_pipe_read = -1;
   }

   cbelt_capture_buf = NULL;
   cbelt_capture_buf_size = 0;
   cbelt_redirect_active = 0;
}

#endif /* CBELT_IMPLEMENTATION */

#endif /* CBELT_H */
