// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdlib.h>

#include "basics.h"
#include "exec_cmd.h"

static int failures = 0;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond))                                                       \
      {                                                                \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++;                                                    \
      }                                                                \
  } while (0)

/* Basic capturing of stdout. */
static void
test_capture_output(void)
{
  _cleanup_free_ char *out = NULL;
  int r = exec_cmd(&out, "echo", "echo", "hello", "world", NULL);

  CHECK(r == 0);
  CHECK(out != NULL);
  CHECK(out && streq(out, "hello world\n"));
}

/* Non-zero exit codes must be propagated and no output captured. */
static void
test_exit_code_propagation(void)
{
  _cleanup_free_ char *out = NULL;
  int r = exec_cmd(&out, "sh", "sh", "-c", "echo should-not-be-returned; exit 3", NULL);

  CHECK(r == 3);
  CHECK(out == NULL);
}

/* Passing NULL as output must not try to capture anything. */
static void
test_no_capture_requested(void)
{
  int r = exec_cmd(NULL, "true", "true", NULL);

  CHECK(r == 0);
}

/* A command that does not exist must return a negative errno value. */
static void
test_command_not_found(void)
{
  _cleanup_free_ char *out = NULL;
  int r = exec_cmd(&out, "/no/such/command/exists", "x", NULL);

  CHECK(r < 0);
  CHECK(out == NULL);
}

/* Output larger than a single internal read chunk must be captured
   completely, to exercise the buffer growth logic. */
static void
test_large_output(void)
{
  _cleanup_free_ char *out = NULL;
  int r = exec_cmd(&out, "sh", "sh", "-c",
		    "i=0; while [ $i -lt 2000 ]; do echo \"line $i\"; i=$((i + 1)); done",
		    NULL);

  CHECK(r == 0);
  CHECK(out != NULL);
  if (out)
    {
      size_t len = strlen(out);
      CHECK(strncmp(out, "line 0\n", 7) == 0);
      CHECK(len >= 10 && streq(out + len - 10, "line 1999\n"));
    }
}

int
main(void)
{
  test_capture_output();
  test_exit_code_propagation();
  test_no_capture_requested();
  test_command_not_found();
  test_large_output();

  if (failures > 0)
    {
      fprintf(stderr, "%d check(s) failed\n", failures);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
