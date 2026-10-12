// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <getopt.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libintl.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-varlink.h>

#include "basics.h"
#include "log_msg.h"
#include "varlink-io.systemd.Metrics.h"
#include "metrics-varlink.h"

static int verbose_flag = 0;

static int
run_varlink(void)
{
  _cleanup_(sd_varlink_server_unrefp) sd_varlink_server *varlink_server = NULL;
  int r;

  /* XXX difference to systemd varlink_server_new()? */
  r = sd_varlink_server_new(&varlink_server, /* flags= */ 0);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to allocate varlink server: %s",
	      strerror(-r));
      return r;
    }

  r = sd_varlink_server_add_interface(varlink_server, &vl_interface_io_systemd_Metrics);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to add Varlink interface: %s",
	      strerror(-r));
      return r;
    }

  r = sd_varlink_server_bind_method_many(varlink_server,
					 "io.systemd.Metrics.List",     vl_method_metrics_list,
					 "io.systemd.Metrics.Describe", vl_method_metrics_describe);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to bind Varlink methods: %s",
	      strerror(-r));
      return r;
    }

  r = sd_varlink_server_loop_auto(varlink_server);
  if (r < 0)
    {
      log_msg(LOG_ERR, "Failed to run Varlink event loop: %s",
	      strerror(-r));
      return r;
    }

  return 0;
}

static void
print_help(void)
{
  log_msg(LOG_INFO, "metrics-exporter - Export additional metrics for systemd-report");

  log_msg(LOG_INFO, "  -d,--debug     Debug mode, no reboot done");
  log_msg(LOG_INFO, "  -v,--verbose   Verbose logging");
  log_msg(LOG_INFO, "  -?, --help     Give this help list");
  log_msg(LOG_INFO, "      --version  Print program version");
}

int
main(int argc, char **argv)
{
  int r;

  log_init();

  while (1)
    {
      int c;
      int option_index = 0;
      static struct option long_options[] =
        {
          {"debug", no_argument, NULL, 'd'},
          {"verbose", no_argument, NULL, 'v'},
          {"version", no_argument, NULL, '\255'},
          {"usage", no_argument, NULL, '?'},
          {"help", no_argument, NULL, 'h'},
          {NULL, 0, NULL, '\0'}
        };


      c = getopt_long(argc, argv, "dvh?", long_options, &option_index);
      if (c == (-1))
        break;
      switch (c)
        {
        case 'd':
          debug_flag = 1;
	  verbose_flag = 1;
          break;
        case '?':
        case 'h':
          print_help();
          return 0;
        case 'v':
          verbose_flag = 1;
          break;
        case '\255':
          fprintf(stdout, "metrics-exporter (%s) %s\n", PACKAGE, VERSION);
          return 0;
        default:
          print_help();
          return 1;
        }
    }

  argc -= optind;
  argv += optind;

  if (argc > 1)
    {
      fprintf(stderr, "Try `metrics-exporter --help' for more information.\n");
      return 1;
    }

  if (verbose_flag)
    log_msg(LOG_INFO, "Starting metrics-exporter (%s) %s...", PACKAGE, VERSION);

  r = run_varlink();
  if (r < 0)
    log_msg(LOG_ERR, "ERROR: running varlink failed: %s", strerror(-r));

  return -r;
}
