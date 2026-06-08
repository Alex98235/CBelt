#ifndef CBELT_H
#define CBELT_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

/*---------------------------------------------------------------------------
 * Spinner configuration
 * Uncomment the line below to disable the animated spinner (useful for CI/CD)
 *--------------------------------------------------------------------------*/
// #define CBELT_DISABLE_SPINNER

/*---------------------------------------------------------------------------
 * Color configuration
 * Uncomment the line below to disable colored output
 *--------------------------------------------------------------------------*/
// #define CBELT_DISABLE_COLOR

/*---------------------------------------------------------------------------
 * ANSI color codes for terminal output
 *--------------------------------------------------------------------------*/
#ifndef CBELT_DISABLE_COLOR
#define CBELT_COLOR_RED "\x1b[31m"
#define CBELT_COLOR_GREEN "\x1b[32m"
#define CBELT_COLOR_YELLOW "\x1b[33m"
#define CBELT_COLOR_CYAN "\x1b[36m"
#define CBELT_COLOR_BOLD "\x1b[1m"
#define CBELT_COLOR_RESET "\x1b[0m"
#else
#define CBELT_COLOR_RED ""
#define CBELT_COLOR_GREEN ""
#define CBELT_COLOR_YELLOW ""
#define CBELT_COLOR_CYAN ""
#define CBELT_COLOR_BOLD ""
#define CBELT_COLOR_RESET ""
#endif

/*---------------------------------------------------------------------------
 * Terminal control macros
 *--------------------------------------------------------------------------*/
#define CBELT_CLEAR_LINE "\r\x1b[2K"
#define CBELT_SAVE_CURSOR "\x1b[s"
#define CBELT_RESTORE_CURSOR "\x1b[u"

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
  struct cbelt_group *next;
  size_t test_count;
} cbelt_group_t;

/*---------------------------------------------------------------------------
 * Magic values
 *--------------------------------------------------------------------------*/
#define CBELT_SANDBOX_DEFAULT_TIMEOUT_S 30

/*---------------------------------------------------------------------------
 * Public API declarations
 *--------------------------------------------------------------------------*/
void cbelt_register_test(const char *group_name, const char *test_name,
                         test_func_t func);
void cbelt_register_test_with_timeout(const char *group_name,
                                      const char *test_name, test_func_t func,
                                      int timeout);
void cbelt_set_group_setup(const char *group_name, setup_func_t func);
void cbelt_set_group_teardown(const char *group_name, teardown_func_t func);
void cbelt_set_test_setup(const char *group_name, setup_func_t func);
void cbelt_set_test_teardown(const char *group_name, teardown_func_t func);
void cbelt_set_global_setup(setup_func_t func);
void cbelt_set_global_teardown(teardown_func_t func);

void cbelt_set_default_timeout(int seconds);

TestResult cbelt_run_test_in_sandbox(test_func_t func, int timeout_s);
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

#define CBELT_TEST_TIMEOUT(name, timeout_s)                                    \
  static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void);                    \
  static void __attribute__((constructor(200 + __COUNTER__))) CBELT_UNIQUE(    \
      _cbelt_register_##name)(void) {                                          \
    cbelt_register_test_with_timeout(cbelt_current_group_name, #name,          \
                                     CBELT_UNIQUE(_cbelt_test_##name),         \
                                     timeout_s);                               \
  }                                                                            \
  static TestResult CBELT_UNIQUE(_cbelt_test_##name)(void)

#define CBELT_TEST_NO_TIMEOUT(name) CBELT_TEST_TIMEOUT(name, -1)

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
 * Assertions
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
static int cbelt_global_default_timeout_s = CBELT_SANDBOX_DEFAULT_TIMEOUT_S;
static pid_t spinner_pid = 0;
static int spinner_pipe[2]; // For communication with spinner process

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
                           test_func_t func, int timeout_s) {
  cbelt_test_t *test = (cbelt_test_t *)calloc(1, sizeof(cbelt_test_t));
  test->name = strdup(name);
  test->func = func;
  test->next = group->tests;
  test->timeout_s = timeout_s;
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
    cbelt_global_default_timeout_s = -1; // No timeout
  }
}

TestResult cbelt_run_test_in_sandbox(test_func_t func, int timeout_s) {
#ifdef __linux__
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    // Pipe creation failed - fall back to no error reporting
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

    // Set timeout unless its a negative value
    if (timeout_s > 0) {
      alarm(timeout_s);
    }

    int result = func();

    // Send error message to parent if there's one
    if (cbelt_error_buf[0] != '\0') {
      write(pipefd[1], cbelt_error_buf, strlen(cbelt_error_buf) + 1);
    }

    close(pipefd[1]);
    exit(result);
  }

  close(pipefd[1]);

  // Read error message from child (non-blocking or with timeout)
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
      snprintf(cbelt_error_buf, sizeof(cbelt_error_buf),
               "Test timed out after %d seconds (infinite loop or hang)\n",
               timeout_s);
    } else {
      snprintf(cbelt_error_buf, sizeof(cbelt_error_buf),
               "Test crashed with signal: %d (%s)\n", sig, strsignal(sig));
    }

    return TEST_CRASH;
  }

  return TEST_FAILURE;
#else
  return func();
#endif
}

static void cbelt_spinner_process(const char *test_name) {
  const char *spinner_frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼",
                                  "⠴", "⠦", "⠧", "⠇", "⠏"};
  int num_frames = sizeof(spinner_frames) / sizeof(spinner_frames[0]);

  srand(time(NULL));
  int frame = rand();

  char c;

  close(spinner_pipe[1]);

  // Make read end non-blocking
  int flags = fcntl(spinner_pipe[0], F_GETFL, 0);
  fcntl(spinner_pipe[0], F_SETFL, flags | O_NONBLOCK);

  while (1) {
    // Check if parent wants spinner to stop
    if (read(spinner_pipe[0], &c, 1) > 0) {
      break;
    }

    printf("\r%s %s...", spinner_frames[frame % num_frames], test_name);
    fflush(stdout);
    frame++;
    usleep(80000);
  }

  // Clear the line completely before exiting
  printf("\r\x1b[2K\r");
  fflush(stdout);

  close(spinner_pipe[0]);
  exit(0);
}

static void cbelt_spinner_stop(void) {
  if (spinner_pid != 0) {
    char stop = 1;
    write(spinner_pipe[1], &stop, 1);

    // Wait for child to clean up and exit
    int status;
    waitpid(spinner_pid, &status, 0);

    close(spinner_pipe[1]);
    spinner_pid = 0;

    printf(CBELT_CLEAR_LINE);
    fflush(stdout);
  }
}

// Start spinner in background process
static void cbelt_spinner_start(const char *test_name) {
#ifdef CBELT_DISABLE_SPINNER
  return;
#else
#ifdef __linux__
  // Linux implementation with fork/pipe
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
  // Non-Linux fallback (Windows, etc.)
  printf("  %s...", test_name);
  fflush(stdout);
#endif
#endif
}

static void cbelt_spinner_update_result(const char *test_name,

                                        TestResult result) {
#ifdef CBELT_DISABLE_SPINNER
#else
#ifdef __linux__
  if (spinner_pid != 0) {
    cbelt_spinner_stop();
  }
#endif
#endif

  if (result == TEST_SUCCESS) {
    printf("  " CBELT_COLOR_GREEN "%s"
#ifdef CBELT_DISABLE_COLOR
           ": PASSED"
#endif
           CBELT_COLOR_RESET "\n",
           test_name);
  } else {
    printf("  " CBELT_COLOR_RED "%s"
#ifdef CBELT_DISABLE_COLOR
           ": FAILED"
#endif
           CBELT_COLOR_RESET "\n",
           test_name);
    if (cbelt_error_buf[0]) {
      cbelt_print_indented(cbelt_error_buf);
    }
  }
  fflush(stdout);
}

#ifdef _WIN32
static void cbelt_enable_ansi(void) {
  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (stdout_handle != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(stdout_handle, &mode)) {
      SetConsoleMode(stdout_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
  }
}
#endif

int cbelt_run_all_tests(void) {
#ifndef CBELT_DISABLE_SPINNER
#ifdef _WIN32
  cbelt_enable_ansi();
#endif
#endif

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
      // Reset error buffer and timeout for this test
      cbelt_error_buf[0] = '\0';

      // Determine timeout
      int timeout_s = cbelt_global_default_timeout_s;
      if (test->timeout_s != 0) {
        timeout_s = test->timeout_s;
      }

      cbelt_spinner_start(test->name);

      if (group->setup)
        group->setup();

      int result = cbelt_run_test_in_sandbox(test->func, timeout_s);

      if (group->teardown)
        group->teardown();

      cbelt_spinner_update_result(test->name, result);

      result == TEST_SUCCESS ? total_passed++ : total_failed++;

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
